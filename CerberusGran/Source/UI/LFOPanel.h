#pragma once
#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "../RotaryKnob.h"

class LFOPanel : public juce::Component
{
public:
    LFOPanel (CerberusGranAudioProcessor& p)
        : processor (p),
          rateKnob ("Rate", " Hz"),
          syncDivKnob ("Div", ""),
          depthKnob ("Depth", ""),
          phaseKnob ("Phase", "")
    {
        accent     = juce::Colour (0xff33333a);
        knobBg     = juce::Colour (0xffc8c8cd);
        labelCol   = juce::Colour (0xff222226);
        valueCol   = juce::Colour (0xff555560);

        styleKnob (rateKnob);
        addAndMakeVisible (rateKnob);

        // Sync div knob — 8 bars down to 1/64
        styleKnob (syncDivKnob);
        syncDivKnob.getSlider().setRange (0, 9, 1);
        syncDivKnob.getSlider().textFromValueFunction = [] (double v) {
            static const char* l[] = {"8/1","4/1","2/1","1/1","1/2","1/4","1/8","1/16","1/32","1/64"};
            return juce::String (l[juce::jlimit (0, 9, (int)v)]);
        };
        syncDivKnob.getSlider().updateText();
        syncDivKnob.setVisible (false);
        addAndMakeVisible (syncDivKnob);

        styleKnob (depthKnob);
        addAndMakeVisible (depthKnob);

        styleKnob (phaseKnob);
        addAndMakeVisible (phaseKnob);

        shapeBox.addItemList ({ "Sine", "Triangle", "Saw Up", "Saw Down", "Square", "S&H" }, 1);
        shapeBox.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xffd8d8dc));
        shapeBox.setColour (juce::ComboBox::textColourId, juce::Colour (0xff1a1a1e));
        addAndMakeVisible (shapeBox);

        // Sync toggle button
        syncBtn.setButtonText ("Sync");
        syncBtn.setClickingTogglesState (true);
        stylePillButton (syncBtn);
        syncBtn.onClick = [this] {
            auto& apvts = processor.apvts;
            auto pid = "lfo" + juce::String (currentLfo) + "_rateMode";
            if (auto* p = apvts.getParameter (pid))
                p->setValueNotifyingHost (syncBtn.getToggleState() ? 1.0f : 0.0f);
            updateSyncVisibility();
        };
        addAndMakeVisible (syncBtn);

        // Sync type combo — only visible in sync mode
        syncTypeBox.addItemList ({ "Norm", "Trip", "Dot" }, 1);
        syncTypeBox.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xffd8d8dc));
        syncTypeBox.setColour (juce::ComboBox::textColourId, juce::Colour (0xff1a1a1e));
        syncTypeBox.setVisible (false);
        addAndMakeVisible (syncTypeBox);

        // Bipolar/unipolar is now per-connection (set via Option-click on a knob in
        // assign mode). The LFO source itself always outputs raw bipolar [-1, 1].
        // bipolarBtn is intentionally not added — APVTS lfoN_bipolar is kept for
        // backward compatibility but no longer affects DSP or visualisation.
        juce::ignoreUnused (bipolarBtn);

        // 5 LFO selector tabs (1..5)
        for (int i = 0; i < 5; ++i)
        {
            auto* b = new juce::TextButton (juce::String (i + 1));
            b->setClickingTogglesState (true);
            b->setRadioGroupId (303);
            stylePillButton (*b);
            b->onClick = [this, i] { selectLfo (i); };
            lfoTabBtns.add (b);
            addAndMakeVisible (b);
        }
        lfoTabBtns[0]->setToggleState (true, juce::dontSendNotification);

        selectLfo (0);
    }

    int getCurrentLfoIndex() const { return currentLfo; }

    void paint (juce::Graphics& g) override
    {
        // Preview area background
        auto preview = previewBounds.toFloat();
        g.setColour (juce::Colour (0xfff4f4f7));
        g.fillRoundedRectangle (preview, 8.0f);
        g.setColour (juce::Colour (0xffc0c0c5));
        g.drawRoundedRectangle (preview.reduced (0.5f), 8.0f, 0.5f);

        // Centre line
        float midY = preview.getCentreY();
        g.setColour (juce::Colour (0xffbbbbc0));
        g.drawHorizontalLine ((int) midY, preview.getX(), preview.getRight());

        // Preview always shows raw bipolar shape — per-connection bipolar/unipolar
        // interpretation is shown on the knob rings, not on the preview itself.
        auto pid = "lfo" + juce::String (currentLfo) + "_";
        float uiDepth    = processor.apvts.getRawParameterValue (pid + "depth")->load();
        int   uiShape    = static_cast<int> (processor.apvts.getRawParameterValue (pid + "shape")->load());
        float uiPhaseOff = processor.apvts.getRawParameterValue (pid + "phase")->load();

        juce::Path path;
        int numPts = juce::jmax (32, (int) preview.getWidth());
        float halfH = preview.getHeight() * 0.45f;

        for (int i = 0; i <= numPts; ++i)
        {
            float phase = (float) i / (float) numPts;
            float p = phase + uiPhaseOff;
            p -= std::floor (p);
            float raw = LFO::computeShape (uiShape, p);          // [-1, 1]
            float v   = raw * uiDepth;                           // [-depth, depth]
            float x = preview.getX() + phase * preview.getWidth();
            float y = midY - v * halfH;
            if (i == 0) path.startNewSubPath (x, y);
            else        path.lineTo (x, y);
        }

        g.setColour (juce::Colour (0xff33333a));
        g.strokePath (path, juce::PathStrokeType (1.8f));

        // Phase cursor still reads live phase from the running LFO instance
        float ph = processor.modEngine.lfos[currentLfo].getPhase();
        float px = preview.getX() + ph * preview.getWidth();
        g.setColour (juce::Colour (0xff33333a).withAlpha (0.6f));
        g.drawVerticalLine ((int) px, preview.getY(), preview.getBottom());
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (4);

        // LFO selector tabs row at top
        auto tabRow = area.removeFromTop (22);
        int tabW = 32;
        for (auto* b : lfoTabBtns)
        {
            b->setBounds (tabRow.removeFromLeft (tabW).withSizeKeepingCentre (tabW, 22));
            tabRow.removeFromLeft (4);
        }
        area.removeFromTop (4);

        // Preview area (~50% remaining height)
        int previewH = (int) (area.getHeight() * 0.50f);
        previewBounds = area.removeFromTop (previewH);
        area.removeFromTop (8);

        // Bottom controls row
        int knobW = 70;
        // Rate or Sync Div knob (one or the other based on mode)
        rateKnob.setBounds    (area.removeFromLeft (knobW));
        syncDivKnob.setBounds (rateKnob.getBounds());
        area.removeFromLeft (4);

        // Sync toggle + type combo stacked vertically next to rate
        {
            auto stack = area.removeFromLeft (60);
            int rowH = stack.getHeight() / 2;
            syncBtn.setBounds (stack.removeFromTop (rowH).withSizeKeepingCentre (60, 22));
            syncTypeBox.setBounds (stack.withSizeKeepingCentre (60, 22));
        }
        area.removeFromLeft (8);

        depthKnob.setBounds (area.removeFromLeft (knobW));
        area.removeFromLeft (4);
        phaseKnob.setBounds (area.removeFromLeft (knobW));
        area.removeFromLeft (12);

        int boxH = 24;
        shapeBox.setBounds (area.removeFromLeft (90).withSizeKeepingCentre (90, boxH));
        // Bipolar button removed — polarity is now per-connection (Option-click on a knob in assign mode)
    }

private:
    using SliderAttach = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttach  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttach = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void styleKnob (RotaryKnob& k)
    {
        k.setAccentColour (accent);
        k.setKnobBgColour (knobBg);
        k.setKnobOutline  (false);
        k.setTextColours  (labelCol, valueCol);
    }

    void stylePillButton (juce::TextButton& b)
    {
        b.setColour (juce::TextButton::buttonColourId,   juce::Colour (0xffd8d8dc));
        b.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff33333a));
        b.setColour (juce::TextButton::textColourOffId,  juce::Colour (0xff555560));
        b.setColour (juce::TextButton::textColourOnId,   juce::Colour (0xffeeeeee));
    }

    void selectLfo (int idx)
    {
        currentLfo = juce::jlimit (0, 4, idx);

        // Update tab toggle visuals (in case called programmatically)
        for (int i = 0; i < (int) lfoTabBtns.size(); ++i)
            lfoTabBtns[i]->setToggleState (i == currentLfo, juce::dontSendNotification);

        // Re-bind attachments to current LFO's params
        auto& apvts = processor.apvts;
        auto pid = [this] (const juce::String& n) {
            return "lfo" + juce::String (currentLfo) + "_" + n;
        };

        rateAttach     .reset();
        syncDivAttach  .reset();
        depthAttach    .reset();
        phaseAttach    .reset();
        shapeAttach    .reset();
        bipolarAttach  .reset();
        syncTypeAttach .reset();

        rateAttach     = std::make_unique<SliderAttach> (apvts, pid ("rate"),         rateKnob.getSlider());
        syncDivAttach  = std::make_unique<SliderAttach> (apvts, pid ("rateSyncDiv"),  syncDivKnob.getSlider());
        depthAttach    = std::make_unique<SliderAttach> (apvts, pid ("depth"),        depthKnob.getSlider());
        phaseAttach    = std::make_unique<SliderAttach> (apvts, pid ("phase"),        phaseKnob.getSlider());
        shapeAttach    = std::make_unique<ComboAttach>  (apvts, pid ("shape"),        shapeBox);
        bipolarAttach  = std::make_unique<ButtonAttach> (apvts, pid ("bipolar"),      bipolarBtn);
        syncTypeAttach = std::make_unique<ComboAttach>  (apvts, pid ("rateSyncType"), syncTypeBox);

        // Sync button is mode-controlled directly, not via attachment
        if (auto* p = apvts.getRawParameterValue (pid ("rateMode")))
            syncBtn.setToggleState (p->load() >= 0.5f, juce::dontSendNotification);

        updateSyncVisibility();
        repaint();
    }

    void updateSyncVisibility()
    {
        bool isSync = syncBtn.getToggleState();
        rateKnob.setVisible    (! isSync);
        syncDivKnob.setVisible (isSync);
        syncTypeBox.setVisible (isSync);
    }

    CerberusGranAudioProcessor& processor;
    juce::Rectangle<int> previewBounds;

    juce::Colour accent, knobBg, labelCol, valueCol;
    int currentLfo = 0;

    RotaryKnob rateKnob, syncDivKnob, depthKnob, phaseKnob;
    juce::ComboBox shapeBox, syncTypeBox;
    juce::TextButton bipolarBtn, syncBtn;
    juce::OwnedArray<juce::TextButton> lfoTabBtns;

    std::unique_ptr<SliderAttach> rateAttach, syncDivAttach, depthAttach, phaseAttach;
    std::unique_ptr<ComboAttach>  shapeAttach, syncTypeAttach;
    std::unique_ptr<ButtonAttach> bipolarAttach;
};
