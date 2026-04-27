#pragma once
#include <JuceHeader.h>
#include "FXSlotCard.h"

class FXChainPanel : public juce::Component
{
public:
    FXChainPanel (juce::AudioProcessorValueTreeState& apvts, int headIndex, juce::Colour accent)
    {
        // Don't catch clicks on the panel itself; let children/parent handle them.
        // This lets EngineColumn intercept clicks on FX knobs in assign mode.
        setInterceptsMouseClicks (false, true);

        for (int i = 0; i < 4; ++i)
        {
            auto* card = new FXSlotCard (apvts, headIndex, accent);
            card->setInterceptsMouseClicks (false, true);
            card->onSelectionChanged = [this] {
                updateAvailableEffects();
                if (onModTargetsChanged) onModTargetsChanged();
            };
            cards.add (card);
            addAndMakeVisible (card);
        }
    }

    // Notification fired when any FX slot's selected type changes (so visible mod targets shift)
    std::function<void()> onModTargetsChanged;

    // Aggregate all currently-visible mod targets across all 4 slots
    std::vector<FXSlotCard::ModTarget> getAllVisibleModTargets() const
    {
        std::vector<FXSlotCard::ModTarget> result;
        for (auto* card : cards)
        {
            auto t = card->getVisibleModTargets();
            result.insert (result.end(), t.begin(), t.end());
        }
        return result;
    }

    // Apply assign-mode click pass-through to all FX slot knobs
    void setKnobsClickThrough (bool clickThrough)
    {
        for (auto* card : cards)
            card->setKnobsClickThrough (clickThrough);
    }

    void resized() override
    {
        auto area = getLocalBounds();
        int gap = 4;
        int cols = 2, rows = 2;
        int cardW = (area.getWidth() - gap * (cols - 1)) / cols;
        int cardH = (area.getHeight() - gap * (rows - 1)) / rows;

        for (int r = 0; r < rows; ++r)
            for (int c = 0; c < cols; ++c)
            {
                int idx = r * cols + c;
                int x = area.getX() + c * (cardW + gap);
                int y = area.getY() + r * (cardH + gap);
                cards[idx]->setBounds (x, y, cardW, cardH);
            }
    }

    int getIdealHeight() const
    {
        return getHeight();
    }

private:
    juce::OwnedArray<FXSlotCard> cards;

    void updateAvailableEffects()
    {
        for (int i = 0; i < cards.size(); ++i)
        {
            std::set<FXType> usedByOthers;
            for (int j = 0; j < cards.size(); ++j)
            {
                if (j == i) continue;
                auto t = cards[j]->getSelectedType();
                if (t != FXType::None)
                    usedByOthers.insert (t);
            }
            cards[i]->setDisabledEffects (usedByOthers);
        }
    }
};
