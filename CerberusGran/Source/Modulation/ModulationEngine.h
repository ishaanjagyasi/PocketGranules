#pragma once
#include <JuceHeader.h>
#include <vector>
#include <mutex>
#include <algorithm>
#include "LFO.h"
#include "StepSequencer.h"
#include "EnvelopeFollower.h"

class ModulationEngine
{
public:
    // Source indices: 0..kNumLFOs-1 are LFOs, then Step Sequencer, then Envelope Follower.
    static constexpr int kNumLFOs = 5;
    static constexpr int kLFO0 = 0;
    static constexpr int kStepSeq     = kNumLFOs;     // = 5
    static constexpr int kEnvFollower = kStepSeq + 1; // = 6
    static constexpr int kNumSources  = kEnvFollower + 1;
    static bool isLFOSource (int idx) { return idx >= 0 && idx < kNumLFOs; }

    struct Connection
    {
        int sourceIndex;
        juce::String destParamId;
        float amount;       // -1..1
        bool bypassed = false;
        bool bipolar  = false; // false = unipolar (default for new), true = bipolar
    };

    ModulationEngine() = default;

    void prepare (double sr)
    {
        for (auto& l : lfos) l.prepare (sr);
        stepSeq.prepare (sr);
        envFollower.prepare (sr);
    }

    // Called once per audio block. Advances LFO/StepSeq sources.
    // The envelope follower is fed by the processor directly (it needs audio input).
    void tick (int numSamples)
    {
        for (auto& l : lfos) l.advance (numSamples);
        stepSeq.advance (numSamples);
    }

    float getSourceOutput (int sourceIndex) const
    {
        if (isLFOSource (sourceIndex)) return lfos[sourceIndex].getOutput();
        if (sourceIndex == kStepSeq)   return stepSeq.getOutput();
        if (sourceIndex == kEnvFollower) return envFollower.getOutput();
        return 0.0f;
    }

    bool isSourceBipolar (int sourceIndex) const noexcept
    {
        if (isLFOSource (sourceIndex)) return lfos[sourceIndex].isBipolar();
        if (sourceIndex == kStepSeq)   return stepSeq.isBipolar();
        return true;
    }

    // Apply modulation on top of a base parameter value.
    // range = paramMax - paramMin. Result clamped to [paramMin, paramMax].
    float applyMod (const juce::String& paramId, float baseValue,
                    float paramMin, float paramMax) const
    {
        std::lock_guard<std::mutex> lock (connMutex);
        if (connections.empty())
            return juce::jlimit (paramMin, paramMax, baseValue);

        float modSum = 0.0f;
        for (const auto& c : connections)
        {
            if (c.bypassed) continue;
            if (c.destParamId != paramId) continue;

            float raw = getSourceOutput (c.sourceIndex);
            float v;
            if (c.sourceIndex == kStepSeq || c.sourceIndex == kEnvFollower)
            {
                // Step Seq applies its own polarity transform; Env Follower is always
                // unipolar [0, 1]. Pass through directly.
                v = raw;
            }
            else
            {
                // LFO outputs raw bipolar [-1, 1]; per-connection bipolar/unipolar transform
                v = c.bipolar ? raw : (raw * 0.5f + 0.5f);
            }
            modSum += v * c.amount;
        }

        if (modSum == 0.0f)
            return juce::jlimit (paramMin, paramMax, baseValue);

        float range = paramMax - paramMin;
        return juce::jlimit (paramMin, paramMax, baseValue + modSum * range);
    }

    // -------- UI/message thread operations --------

    // Update amount for an existing connection or create a new one with the given default bipolar mode
    void addOrUpdateConnection (int sourceIndex, const juce::String& destParamId, float amount,
                                bool defaultBipolarIfNew = false)
    {
        std::lock_guard<std::mutex> lock (connMutex);
        for (auto& c : connections)
        {
            if (c.sourceIndex == sourceIndex && c.destParamId == destParamId)
            {
                c.amount = amount;
                return;
            }
        }
        Connection c;
        c.sourceIndex = sourceIndex;
        c.destParamId = destParamId;
        c.amount = amount;
        c.bipolar = defaultBipolarIfNew;
        connections.push_back (c);
    }

    // Toggle (or create then toggle) the bipolar interpretation of a single connection
    void toggleConnectionBipolar (int sourceIndex, const juce::String& destParamId)
    {
        std::lock_guard<std::mutex> lock (connMutex);
        for (auto& c : connections)
        {
            if (c.sourceIndex == sourceIndex && c.destParamId == destParamId)
            {
                c.bipolar = ! c.bipolar;
                return;
            }
        }
    }

    bool isConnectionBipolar (int sourceIndex, const juce::String& destParamId) const
    {
        std::lock_guard<std::mutex> lock (connMutex);
        for (const auto& c : connections)
            if (c.sourceIndex == sourceIndex && c.destParamId == destParamId)
                return c.bipolar;
        return false;
    }

    void removeConnection (int sourceIndex, const juce::String& destParamId)
    {
        std::lock_guard<std::mutex> lock (connMutex);
        connections.erase (
            std::remove_if (connections.begin(), connections.end(),
                [&] (const Connection& c) {
                    return c.sourceIndex == sourceIndex && c.destParamId == destParamId;
                }),
            connections.end());
    }

    float getConnectionAmount (int sourceIndex, const juce::String& destParamId) const
    {
        std::lock_guard<std::mutex> lock (connMutex);
        for (const auto& c : connections)
            if (c.sourceIndex == sourceIndex && c.destParamId == destParamId)
                return c.amount;
        return 0.0f;
    }

    bool hasConnection (int sourceIndex, const juce::String& destParamId) const
    {
        std::lock_guard<std::mutex> lock (connMutex);
        for (const auto& c : connections)
            if (c.sourceIndex == sourceIndex && c.destParamId == destParamId)
                return true;
        return false;
    }

    void setConnectionBypassed (int sourceIndex, const juce::String& destParamId, bool bypassed)
    {
        std::lock_guard<std::mutex> lock (connMutex);
        for (auto& c : connections)
            if (c.sourceIndex == sourceIndex && c.destParamId == destParamId)
            {
                c.bypassed = bypassed;
                return;
            }
    }

    bool isConnectionBypassed (int sourceIndex, const juce::String& destParamId) const
    {
        std::lock_guard<std::mutex> lock (connMutex);
        for (const auto& c : connections)
            if (c.sourceIndex == sourceIndex && c.destParamId == destParamId)
                return c.bypassed;
        return false;
    }

    void removeAllConnectionsForParam (const juce::String& destParamId)
    {
        std::lock_guard<std::mutex> lock (connMutex);
        connections.erase (
            std::remove_if (connections.begin(), connections.end(),
                [&] (const Connection& c) { return c.destParamId == destParamId; }),
            connections.end());
    }

    // Returns all connections that target paramId (for drawing aggregated rings)
    std::vector<Connection> getConnectionsForParam (const juce::String& paramId) const
    {
        std::vector<Connection> result;
        std::lock_guard<std::mutex> lock (connMutex);
        for (const auto& c : connections)
            if (c.destParamId == paramId)
                result.push_back (c);
        return result;
    }

    std::vector<Connection> getAllConnections() const
    {
        std::lock_guard<std::mutex> lock (connMutex);
        return connections;
    }

    void clearConnections()
    {
        std::lock_guard<std::mutex> lock (connMutex);
        connections.clear();
    }

    // Public for direct access from audio thread setters
    std::array<LFO, kNumLFOs> lfos;
    StepSequencer stepSeq;
    EnvelopeFollower envFollower;

private:
    mutable std::mutex connMutex;
    std::vector<Connection> connections;
};
