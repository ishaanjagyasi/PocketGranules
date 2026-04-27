#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <cmath>

// Standard one-pole envelope follower with separate rise (attack) and fall (release) times.
// Reads the plugin's input audio and produces a unipolar [0, 1] envelope each block.
class EnvelopeFollower
{
public:
    void prepare (double sr)
    {
        sampleRate = sr > 0.0 ? sr : 44100.0;
        envelope = 0.0f;
        recomputeCoefs();
        // Reset history ring used for the central waveform display
        for (auto& h : history) h.store (0.0f);
        historyWriteIdx = 0;
    }

    void setSensitivity (float s) { sensitivity = juce::jlimit (0.0f, 4.0f, s); }
    void setRiseMs      (float ms) { riseMs = juce::jmax (0.05f, ms); recomputeCoefs(); }
    void setFallMs      (float ms) { fallMs = juce::jmax (0.5f,  ms); recomputeCoefs(); }

    // Called from processBlock with the plugin's INPUT audio buffer (pre grain processing)
    void process (const float* leftCh, const float* rightCh, int numSamples)
    {
        if (numSamples <= 0) return;

        for (int i = 0; i < numSamples; ++i)
        {
            float l = leftCh ? leftCh[i] : 0.0f;
            float r = rightCh ? rightCh[i] : l;
            float in = std::abs ((l + r) * 0.5f) * sensitivity;
            float coef = (in > envelope) ? riseCoef : fallCoef;
            envelope += coef * (in - envelope);
        }

        envelope = juce::jlimit (0.0f, 1.0f, envelope);
        output.store (envelope);

        // Feed history ring (one sample per block — keeps it cheap)
        history[historyWriteIdx].store (envelope);
        historyWriteIdx = (historyWriteIdx + 1) % kHistorySize;
    }

    float getOutput() const { return output.load (std::memory_order_relaxed); }

    // Last N envelope values for the central display (oldest -> newest)
    static constexpr int kHistorySize = 192;
    float getHistoryAt (int idx) const
    {
        // idx in [0, kHistorySize): 0 = oldest, kHistorySize-1 = newest
        int read = (historyWriteIdx + idx) % kHistorySize;
        return history[read].load (std::memory_order_relaxed);
    }

private:
    void recomputeCoefs()
    {
        if (sampleRate <= 0.0) return;
        // One-pole coefficient: coef = 1 - exp(-1 / (timeMs/1000 * sampleRate))
        riseCoef = 1.0f - std::exp (-1.0f / (riseMs * 0.001f * static_cast<float> (sampleRate)));
        fallCoef = 1.0f - std::exp (-1.0f / (fallMs * 0.001f * static_cast<float> (sampleRate)));
    }

    double sampleRate = 44100.0;
    float sensitivity = 1.0f;
    float riseMs = 10.0f;
    float fallMs = 100.0f;
    float riseCoef = 0.1f;
    float fallCoef = 0.01f;
    float envelope = 0.0f;
    std::atomic<float> output { 0.0f };

    std::array<std::atomic<float>, kHistorySize> history;
    int historyWriteIdx = 0;
};
