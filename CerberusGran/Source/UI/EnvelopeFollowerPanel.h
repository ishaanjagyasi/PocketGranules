#pragma once
#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "../RotaryKnob.h"

class EnvelopeFollowerPanel : public juce::Component
{
public:
    EnvelopeFollowerPanel (CerberusGranAudioProcessor& p)
        : processor (p),
          sensKnob ("Sensitivity", ""),
          riseKnob ("Rise", " ms"),
          fallKnob ("Fall", " ms")
    {
        auto accent   = juce::Colour (0xff33333a);
        auto knobBg   = juce::Colour (0xffc8c8cd);
        auto labelCol = juce::Colour (0xff222226);
        auto valueCol = juce::Colour (0xff555560);

        auto styleKnob = [&] (RotaryKnob& k)
        {
            k.setAccentColour (accent);
            k.setKnobBgColour (knobBg);
            k.setKnobOutline  (false);
            k.setTextColours  (labelCol, valueCol);
        };

        styleKnob (sensKnob);
        addAndMakeVisible (sensKnob);
        sensAttach = std::make_unique<SliderAttach> (p.apvts, "env_sens", sensKnob.getSlider());

        styleKnob (riseKnob);
        addAndMakeVisible (riseKnob);
        riseAttach = std::make_unique<SliderAttach> (p.apvts, "env_rise", riseKnob.getSlider());

        styleKnob (fallKnob);
        addAndMakeVisible (fallKnob);
        fallAttach = std::make_unique<SliderAttach> (p.apvts, "env_fall", fallKnob.getSlider());
    }

    void paint (juce::Graphics& g) override
    {
        // Display panel background
        auto display = displayBounds.toFloat();
        g.setColour (juce::Colour (0xfff4f4f7));
        g.fillRoundedRectangle (display, 8.0f);
        g.setColour (juce::Colour (0xffc0c0c5));
        g.drawRoundedRectangle (display.reduced (0.5f), 8.0f, 0.5f);

        // Draw envelope history as a filled "crown" curve from baseline up
        const auto& env = processor.modEngine.envFollower;
        const int N = EnvelopeFollower::kHistorySize;
        float baseY = display.getBottom() - 4.0f;
        float topY  = display.getY() + 4.0f;
        float amplH = baseY - topY;

        juce::Path crown;
        crown.startNewSubPath (display.getX(), baseY);
        for (int i = 0; i < N; ++i)
        {
            float t = (float) i / (float) (N - 1);
            float x = display.getX() + t * display.getWidth();
            float v = juce::jlimit (0.0f, 1.0f, env.getHistoryAt (i));
            float y = baseY - v * amplH;
            crown.lineTo (x, y);
        }
        crown.lineTo (display.getRight(), baseY);
        crown.closeSubPath();

        // Dark grey to match the rest of the modulation panel theme
        g.setColour (juce::Colour (0xff33333a).withAlpha (0.20f));
        g.fillPath (crown);
        g.setColour (juce::Colour (0xff33333a));
        g.strokePath (crown, juce::PathStrokeType (1.4f));
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (4);

        // Top ~55%: envelope display
        int displayH = (int) (area.getHeight() * 0.55f);
        displayBounds = area.removeFromTop (displayH);
        area.removeFromTop (8);

        // Bottom controls row — 3 knobs centred horizontally
        int knobW = 80;
        int gap   = 16;
        int totalW = knobW * 3 + gap * 2;
        int startX = area.getX() + (area.getWidth() - totalW) / 2;
        int y = area.getY();
        int h = area.getHeight();

        sensKnob.setBounds (startX,                      y, knobW, h);
        riseKnob.setBounds (startX + knobW + gap,        y, knobW, h);
        fallKnob.setBounds (startX + (knobW + gap) * 2,  y, knobW, h);
    }

private:
    using SliderAttach = juce::AudioProcessorValueTreeState::SliderAttachment;

    CerberusGranAudioProcessor& processor;
    juce::Rectangle<int> displayBounds;

    RotaryKnob sensKnob, riseKnob, fallKnob;
    std::unique_ptr<SliderAttach> sensAttach, riseAttach, fallAttach;
};
