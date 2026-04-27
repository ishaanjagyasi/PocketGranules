#pragma once
#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "../RotaryKnob.h"
#include "../Modulation/StepSequencer.h"

// Pill-shaped button with a vector icon for play modes
class IconPillButton : public juce::Button
{
public:
    enum Icon { ArrowRight, ArrowLeft, PingPong, Dice, PlusMinus };

    IconPillButton (const juce::String& name, Icon i)
        : juce::Button (name), icon (i) {}

    juce::Colour onBg   { 0xff33333a };
    juce::Colour offBg  { 0xffd8d8dc };
    juce::Colour onFg   { 0xffeeeeee };
    juce::Colour offFg  { 0xff555560 };

    void paintButton (juce::Graphics& g, bool, bool) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (0.5f);
        float corner = bounds.getHeight() * 0.45f;
        bool on = getToggleState();

        g.setColour (on ? onBg : offBg);
        g.fillRoundedRectangle (bounds, corner);

        g.setColour (on ? onFg : offFg);

        // Text icons use the full button bounds for legibility; vector icons use a reduced area
        if (icon == PlusMinus)
        {
            g.setFont (juce::Font ("Avenir", bounds.getHeight() * 0.7f, juce::Font::bold));
            g.drawText ("+/-", bounds, juce::Justification::centred);
            return;
        }

        auto iconArea = bounds.reduced (bounds.getWidth() * 0.28f, bounds.getHeight() * 0.28f);

        switch (icon)
        {
            case ArrowRight: drawTriangle (g, iconArea, true);  break;
            case ArrowLeft:  drawTriangle (g, iconArea, false); break;
            case PingPong:   drawPingPong (g, iconArea);        break;
            case Dice:       drawDice     (g, iconArea);        break;
            default: break;
        }
    }

private:
    Icon icon;

    // Single arrow sized to roughly match a single triangle of the PingPong icon
    static void drawTriangle (juce::Graphics& g, juce::Rectangle<float> r, bool right)
    {
        float halfH  = r.getHeight() * 0.45f;
        float arrowW = r.getHeight() * 0.7f;  // proportional to height for balanced look
        float midX = r.getCentreX();
        float midY = r.getCentreY();

        juce::Path p;
        if (right)
            p.addTriangle (midX - arrowW * 0.5f, midY - halfH,
                           midX - arrowW * 0.5f, midY + halfH,
                           midX + arrowW * 0.5f, midY);
        else
            p.addTriangle (midX + arrowW * 0.5f, midY - halfH,
                           midX + arrowW * 0.5f, midY + halfH,
                           midX - arrowW * 0.5f, midY);
        g.fillPath (p);
    }

    static void drawPingPong (juce::Graphics& g, juce::Rectangle<float> r)
    {
        float midX  = r.getCentreX();
        float midY  = r.getCentreY();
        float halfH = r.getHeight() * 0.45f;
        float halfW = r.getWidth() * 0.45f;
        float gap   = r.getWidth() * 0.06f;

        juce::Path left, right;
        left.addTriangle  (midX - gap, midY - halfH, midX - gap, midY + halfH, midX - halfW, midY);
        right.addTriangle (midX + gap, midY - halfH, midX + gap, midY + halfH, midX + halfW, midY);
        g.fillPath (left);
        g.fillPath (right);
    }

    static void drawDice (juce::Graphics& g, juce::Rectangle<float> r)
    {
        float side = juce::jmin (r.getWidth(), r.getHeight());
        auto sq = juce::Rectangle<float> (r.getCentreX() - side * 0.5f,
                                          r.getCentreY() - side * 0.5f,
                                          side, side);
        g.drawRoundedRectangle (sq, side * 0.18f, 1.4f);

        float dotR = side * 0.09f;
        auto dot = [&] (float fx, float fy)
        {
            float cx = sq.getX() + sq.getWidth() * fx;
            float cy = sq.getY() + sq.getHeight() * fy;
            g.fillEllipse (cx - dotR, cy - dotR, dotR * 2.0f, dotR * 2.0f);
        };
        dot (0.28f, 0.28f);
        dot (0.72f, 0.28f);
        dot (0.5f,  0.5f);
        dot (0.28f, 0.72f);
        dot (0.72f, 0.72f);
    }
};

class StepSequencerPanel : public juce::Component
{
public:
    StepSequencerPanel (CerberusGranAudioProcessor& p)
        : processor (p),
          rateKnob ("Rate", " Hz"),
          syncDivKnob ("Div", ""),
          smoothKnob ("Smooth", "")
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

        styleKnob (rateKnob);
        addAndMakeVisible (rateKnob);
        rateAttach = std::make_unique<SliderAttach> (p.apvts, "seq_rate", rateKnob.getSlider());

        // Sync div knob — stepped 1/N
        styleKnob (syncDivKnob);
        syncDivKnob.getSlider().setRange (0, 8, 1);
        syncDivKnob.getSlider().textFromValueFunction = [] (double v) {
            static const char* l[] = {"1/1","1/2","1/4","1/8","1/16","1/32","1/64","1/128","1/256"};
            return juce::String (l[juce::jlimit (0, 8, (int)v)]);
        };
        syncDivKnob.getSlider().updateText();
        syncDivKnob.setVisible (false);
        addAndMakeVisible (syncDivKnob);
        syncDivAttach = std::make_unique<SliderAttach> (p.apvts, "seq_rateSyncDiv", syncDivKnob.getSlider());

        styleKnob (smoothKnob);
        addAndMakeVisible (smoothKnob);
        smoothAttach = std::make_unique<SliderAttach> (p.apvts, "seq_smooth", smoothKnob.getSlider());

        // Sync toggle
        syncBtn.setButtonText ("Sync");
        syncBtn.setClickingTogglesState (true);
        stylePillButton (syncBtn);
        syncBtn.onClick = [this] {
            if (auto* p = processor.apvts.getParameter ("seq_rateMode"))
                p->setValueNotifyingHost (syncBtn.getToggleState() ? 1.0f : 0.0f);
            updateSyncVisibility();
        };
        addAndMakeVisible (syncBtn);
        if (auto* p = processor.apvts.getRawParameterValue ("seq_rateMode"))
            syncBtn.setToggleState (p->load() >= 0.5f, juce::dontSendNotification);

        // Sync type combo
        syncTypeBox.addItemList ({ "Norm", "Trip", "Dot" }, 1);
        syncTypeBox.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xffd8d8dc));
        syncTypeBox.setColour (juce::ComboBox::textColourId, juce::Colour (0xff1a1a1e));
        syncTypeBox.setVisible (false);
        addAndMakeVisible (syncTypeBox);
        syncTypeAttach = std::make_unique<ComboAttach> (p.apvts, "seq_rateSyncType", syncTypeBox);

        // Bipolar +/- toggle for the Step Sequencer (controls step display + edit + output mode)
        bipolarBtn.setClickingTogglesState (true);
        addAndMakeVisible (bipolarBtn);
        bipolarAttach = std::make_unique<ButtonAttach> (p.apvts, "seq_bipolar", bipolarBtn);

        // Clear button
        clearBtn.setButtonText ("Clear");
        clearBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xffd8d8dc));
        clearBtn.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff555560));
        clearBtn.onClick = [this] {
            processor.modEngine.stepSeq.clearSteps();
            repaint();
        };
        addAndMakeVisible (clearBtn);

        // Play mode icon buttons (Fwd, Rev, PingPong, Random)
        const IconPillButton::Icon icons[] = {
            IconPillButton::ArrowRight,
            IconPillButton::ArrowLeft,
            IconPillButton::PingPong,
            IconPillButton::Dice
        };
        for (int i = 0; i < 4; ++i)
        {
            auto* b = new IconPillButton ("mode" + juce::String (i), icons[i]);
            b->setClickingTogglesState (true);
            b->setRadioGroupId (202);
            b->onClick = [this, i] {
                if (auto* p = processor.apvts.getParameter ("seq_playmode"))
                    p->setValueNotifyingHost (static_cast<float> (i) / 3.0f);
            };
            playModeBtns.add (b);
            addAndMakeVisible (b);
        }
        int currentMode = static_cast<int> (processor.apvts.getRawParameterValue ("seq_playmode")->load());
        playModeBtns[juce::jlimit (0, 3, currentMode)]->setToggleState (true, juce::dontSendNotification);

        updateSyncVisibility();
    }

    void paint (juce::Graphics& g) override
    {
        auto& seq = processor.modEngine.stepSeq;
        int length = seq.getLength();

        // Step display background
        g.setColour (juce::Colour (0xfff4f4f7));
        g.fillRoundedRectangle (stepAreaBounds.toFloat(), 8.0f);
        g.setColour (juce::Colour (0xffc0c0c5));
        g.drawRoundedRectangle (stepAreaBounds.toFloat().reduced (0.5f), 8.0f, 0.5f);

        // Centre line
        float midY = stepAreaBounds.getCentreY();
        g.setColour (juce::Colour (0xffbbbbc0));
        g.drawHorizontalLine ((int) midY, static_cast<float> (stepAreaBounds.getX()),
                                           static_cast<float> (stepAreaBounds.getRight()));

        // Step columns — bipolar (centered ±) or unipolar (bottom-up) based on +/- toggle
        bool bipolar = processor.apvts.getRawParameterValue ("seq_bipolar")->load() >= 0.5f;
        int currentStep = seq.getCurrentStep();
        float colW = (float) stepAreaBounds.getWidth() / (float) StepSequencer::kMaxSteps;

        for (int i = 0; i < StepSequencer::kMaxSteps; ++i)
        {
            float x = stepAreaBounds.getX() + i * colW;
            bool active = (i < length);
            bool isCurrent = (i == currentStep) && active;

            float val = seq.getStepValue (i);
            float barX = x + 1.5f;
            float barW = colW - 3.0f;

            juce::Colour barCol = active ? juce::Colour (0xff33333a) : juce::Colour (0xffaaaaaf);
            if (isCurrent) barCol = barCol.brighter (0.4f);
            g.setColour (barCol.withAlpha (active ? 0.85f : 0.35f));

            if (bipolar)
            {
                // Centered around midline; positive grows up, negative grows down
                float halfH = stepAreaBounds.getHeight() * 0.45f;
                float h = std::abs (val) * halfH;
                if (val >= 0) g.fillRect (barX, midY - h, barW, h);
                else          g.fillRect (barX, midY,     barW, h);
            }
            else
            {
                // Bars from bottom up
                float h = juce::jmax (0.0f, val) * stepAreaBounds.getHeight() * 0.9f;
                float bottom = stepAreaBounds.getBottom() - 4.0f;
                g.fillRect (barX, bottom - h, barW, h);
            }

            if (isCurrent)
            {
                g.setColour (juce::Colour (0xff33333a).withAlpha (0.12f));
                g.fillRect (x, (float) stepAreaBounds.getY(), colW, (float) stepAreaBounds.getHeight());
            }

            if (i > 0)
            {
                g.setColour (juce::Colour (0xffe0e0e5));
                g.drawVerticalLine ((int) x, (float) stepAreaBounds.getY(), (float) stepAreaBounds.getBottom());
            }
        }
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (4);

        // Top 55%: step display
        int displayH = (int) (area.getHeight() * 0.55f);
        stepAreaBounds = area.removeFromTop (displayH);
        area.removeFromTop (8);

        // Bottom controls row
        int knobW = 70;
        rateKnob.setBounds    (area.removeFromLeft (knobW));
        syncDivKnob.setBounds (rateKnob.getBounds());
        area.removeFromLeft (4);

        // Sync btn + type combo stacked vertically next to rate knob
        {
            auto stack = area.removeFromLeft (54);
            int rowH = stack.getHeight() / 2;
            syncBtn.setBounds     (stack.removeFromTop (rowH).withSizeKeepingCentre (54, 22));
            syncTypeBox.setBounds (stack.withSizeKeepingCentre (54, 22));
        }
        area.removeFromLeft (8);

        smoothKnob.setBounds (area.removeFromLeft (knobW));
        area.removeFromLeft (12);

        int btnH  = 22;
        int btnW  = 70;
        int btnY  = area.getY() + (area.getHeight() - btnH) / 2;

        // Pin Clear to far right
        clearBtn.setBounds (area.getRight() - btnW, btnY, btnW, btnH);
        area.removeFromRight (btnW + 12);

        // 4 icon play-mode buttons clustered tightly on the left,
        // +/- bipolar toggle pinned to the right (next to Clear)
        int btnUniformW = 36;
        int tightGap = 4;
        int x = area.getX();
        for (int i = 0; i < 4; ++i)
        {
            playModeBtns[i]->setBounds (x, btnY, btnUniformW, btnH);
            x += btnUniformW + tightGap;
        }
        bipolarBtn.setBounds (area.getRight() - btnUniformW, btnY, btnUniformW, btnH);
    }

    void mouseDown (const juce::MouseEvent& e) override  { editStep (e); }
    void mouseDrag (const juce::MouseEvent& e) override  { editStep (e); }

private:
    void editStep (const juce::MouseEvent& e)
    {
        if (! stepAreaBounds.contains (e.getPosition())) return;

        float colW = (float) stepAreaBounds.getWidth() / (float) StepSequencer::kMaxSteps;
        int col = juce::jlimit (0, StepSequencer::kMaxSteps - 1,
                                (int) ((e.x - stepAreaBounds.getX()) / colW));

        bool bipolar = processor.apvts.getRawParameterValue ("seq_bipolar")->load() >= 0.5f;
        float val;
        if (bipolar)
        {
            float midY = stepAreaBounds.getCentreY();
            float halfH = stepAreaBounds.getHeight() * 0.45f;
            val = juce::jlimit (-1.0f, 1.0f, (midY - e.y) / halfH);
        }
        else
        {
            float bottom = stepAreaBounds.getBottom() - 4.0f;
            float h = bottom - e.y;
            val = juce::jlimit (0.0f, 1.0f, h / (stepAreaBounds.getHeight() * 0.9f));
        }

        processor.modEngine.stepSeq.setStepValue (col, val);
        repaint();
    }

    void stylePillButton (juce::TextButton& b)
    {
        b.setColour (juce::TextButton::buttonColourId,   juce::Colour (0xffd8d8dc));
        b.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff33333a));
        b.setColour (juce::TextButton::textColourOffId,  juce::Colour (0xff555560));
        b.setColour (juce::TextButton::textColourOnId,   juce::Colour (0xffeeeeee));
    }

    void updateSyncVisibility()
    {
        bool isSync = syncBtn.getToggleState();
        rateKnob.setVisible    (! isSync);
        syncDivKnob.setVisible (isSync);
        syncTypeBox.setVisible (isSync);
    }

    using SliderAttach = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttach = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAttach  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    CerberusGranAudioProcessor& processor;
    juce::Rectangle<int> stepAreaBounds;

    RotaryKnob rateKnob, syncDivKnob, smoothKnob;
    IconPillButton bipolarBtn { "bipolar", IconPillButton::PlusMinus };
    juce::TextButton clearBtn, syncBtn;
    juce::ComboBox syncTypeBox;
    juce::OwnedArray<IconPillButton> playModeBtns;

    std::unique_ptr<SliderAttach> rateAttach, syncDivAttach, smoothAttach;
    std::unique_ptr<ButtonAttach> bipolarAttach;
    std::unique_ptr<ComboAttach>  syncTypeAttach;
};
