#pragma once
#include <JuceHeader.h>
#include "RotaryKnob.h"
#include "GrainShapeKnob.h"
#include "FXChainPanel.h"
#include "Modulation/ModulationEngine.h"

class EngineColumn : public juce::Component
{
public:
    static inline juce::Colour sectionPurple { 0xff8B5CF6 };

    EngineColumn (juce::AudioProcessorValueTreeState& apvts, int headIndex, juce::Colour accent,
                  ModulationEngine* modEngineIn = nullptr)
        : index (headIndex), accentColour (accent),
          fxPanel (apvts, headIndex, accent),
          modEngine (modEngineIn),
          apvtsRef (apvts)
    {
        auto id = [headIndex] (const juce::String& name)
        { return "head" + juce::String (headIndex) + "_" + name; };

        // Cache modulatable param IDs keyed by card slot 0..7
        cardParamIds[0] = id ("position");
        cardParamIds[1] = id ("spread");
        cardParamIds[2] = id ("rate");
        cardParamIds[3] = id ("length");
        cardParamIds[4] = id ("pitch");
        cardParamIds[5] = id ("shape");
        cardParamIds[6] = id ("gain");
        cardParamIds[7] = id ("reverse");

        enableBtn.getProperties().set ("accentColour", (juce::int64) accent.getARGB());
        addAndMakeVisible (enableBtn);
        enableAttach = std::make_unique<ButtonAttach> (apvts, id ("enable"), enableBtn);

        freezeBtn.setButtonText ("Freeze");
        freezeBtn.setClickingTogglesState (true);
        freezeBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2E2E34));
        freezeBtn.setColour (juce::TextButton::buttonOnColourId, accent);
        freezeBtn.setColour (juce::TextButton::textColourOffId, juce::Colour (0xffaaaaaa));
        freezeBtn.setColour (juce::TextButton::textColourOnId, juce::Colour (0xff1A1A1E));
        addAndMakeVisible (freezeBtn);
        freezeAttach = std::make_unique<ButtonAttach> (apvts, id ("freeze"), freezeBtn);

        // Advanced button — sits in FX Chain label row
        advancedBtn.setButtonText ("Mod");
        advancedBtn.setClickingTogglesState (true);
        advancedBtn.setColour (juce::TextButton::buttonColourId,   juce::Colour (0xff2E2E34));
        advancedBtn.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffd8d8dc));
        advancedBtn.setColour (juce::TextButton::textColourOffId,  juce::Colour (0xffaaaaaa));
        advancedBtn.setColour (juce::TextButton::textColourOnId,   juce::Colour (0xff1A1A1E));
        advancedBtn.onClick = [this] {
            if (onAdvancedToggled)
                onAdvancedToggled (advancedBtn.getToggleState());
        };
        addAndMakeVisible (advancedBtn);

        auto makeKnob = [&] (const juce::String& label, const juce::String& suffix,
                             const juce::String& paramName) -> RotaryKnob*
        {
            auto* k = new RotaryKnob (label, suffix);
            k->setAccentColour (accent);
            addAndMakeVisible (k);
            knobAttachments.add (new SliderAttach (apvts, id (paramName), k->getSlider()));
            return k;
        };

        posKnob     = makeKnob ("Pos",     "%",  "position");
        spreadKnob  = makeKnob ("Spread",  "",   "spread");
        rateKnob    = makeKnob ("Rate",    "ms", "rate");
        lenKnob     = makeKnob ("Len",     "ms", "length");
        pitchKnob   = makeKnob ("Pitch",   "st", "pitch");
        gainKnob    = makeKnob ("Gain",    "dB", "gain");
        reverseKnob = makeKnob ("Reverse", "%",  "reverse");

        syncDivKnob = new RotaryKnob ("Div", "");
        syncDivKnob->setAccentColour (accent);
        syncDivKnob->getSlider().setRange (0, 8, 1);
        syncDivKnob->getSlider().textFromValueFunction = [] (double v) {
            static const char* l[] = {"1/1","1/2","1/4","1/8","1/16","1/32","1/64","1/128","1/256"};
            return juce::String (l[juce::jlimit (0, 8, (int)v)]);
        };
        syncDivKnob->getSlider().updateText();
        syncDivKnob->setVisible (false);
        addAndMakeVisible (syncDivKnob);
        syncDivAttach = std::make_unique<SliderAttach> (apvts, id ("rateSyncDiv"), syncDivKnob->getSlider());

        syncBtn.setButtonText ("Sync");
        syncBtn.setClickingTogglesState (true);
        syncBtn.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        syncBtn.setColour (juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
        syncBtn.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff999999));
        syncBtn.setColour (juce::TextButton::textColourOnId, juce::Colour (0xffeeeeee));
        syncBtn.onClick = [this, &apvts, id] {
            if (auto* p = apvts.getParameter (id ("rateMode")))
                p->setValueNotifyingHost (syncBtn.getToggleState() ? 1.0f : 0.0f);
            updateRateModeVisibility();
        };
        addAndMakeVisible (syncBtn);
        if (auto* p = apvts.getRawParameterValue (id ("rateMode")))
            syncBtn.setToggleState (p->load() >= 0.5f, juce::dontSendNotification);

        syncTypeBox.addItemList ({ "Norm", "Trip", "Dot" }, 1);
        syncTypeBox.setColour (juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
        syncTypeBox.setEnabled (false);
        syncTypeBox.setAlpha (0.3f);
        addAndMakeVisible (syncTypeBox);
        syncTypeAttach = std::make_unique<ComboAttach> (apvts, id ("rateSyncType"), syncTypeBox);

        sizeLinkBtn.setButtonText ("Link");
        sizeLinkBtn.setClickingTogglesState (true);
        sizeLinkBtn.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        sizeLinkBtn.setColour (juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
        sizeLinkBtn.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff999999));
        sizeLinkBtn.setColour (juce::TextButton::textColourOnId, juce::Colour (0xffeeeeee));
        sizeLinkBtn.onClick = [this] { updateSizeLinkVisibility(); };
        addAndMakeVisible (sizeLinkBtn);
        sizeLinkAttach = std::make_unique<ButtonAttach> (apvts, id ("sizeLink"), sizeLinkBtn);

        sizeRatioKnob = new RotaryKnob ("Ratio", "");
        sizeRatioKnob->setAccentColour (accent);
        sizeRatioKnob->getSlider().setRange (0, 6, 1);
        sizeRatioKnob->getSlider().textFromValueFunction = [] (double v) {
            static const char* l[] = {"1:4","1:2","1:1","2:1","4:1","8:1","16:1"};
            return juce::String (l[juce::jlimit (0, 6, (int)v)]);
        };
        sizeRatioKnob->getSlider().updateText();
        sizeRatioKnob->setVisible (false);
        addAndMakeVisible (sizeRatioKnob);
        sizeRatioAttach = std::make_unique<SliderAttach> (apvts, id ("sizeRatio"), sizeRatioKnob->getSlider());

        shapeKnob.setAccentColour (accent);
        addAndMakeVisible (shapeKnob);
        shapeAttach = std::make_unique<SliderAttach> (apvts, id ("shape"), shapeKnob.getSlider());

        addAndMakeVisible (fxPanel);

        // When user changes an FX slot's type during assign mode, the newly visible
        // knobs need their click-through state re-applied + a repaint to update rings
        fxPanel.onModTargetsChanged = [this]
        {
            if (assignMode) fxPanel.setKnobsClickThrough (true);
            repaint();
        };

        // Dim only the panel areas (not the top row of labels/buttons) when disabled
        enableAttachmentCb = std::make_unique<juce::ParameterAttachment> (
            *apvts.getParameter (id ("enable")),
            [this] (float v) { grainEnabled = v >= 0.5f; repaint(); },
            nullptr);
        enableAttachmentCb->sendInitialUpdate();

        updateRateModeVisibility();
        updateSizeLinkVisibility();
    }

    // Set which modulation source the EngineColumn is currently "focused on" — only
    // connections from this source will draw their rings on the grain knobs.
    // When in assign mode, this also updates which source the next drag will write to,
    // so switching LFO tabs mid-assign retargets new assignments to the new source.
    void setCurrentViewSource (int sourceIndex)
    {
        if (sourceIndex == currentViewSource) return;
        currentViewSource = sourceIndex;
        if (assignMode) assignSource = sourceIndex;
        repaint();
    }

    // Enable/disable assignment mode — disables knob interaction so EngineColumn gets the clicks
    void setAssignMode (int sourceIndex, bool active)
    {
        assignMode = active;
        assignSource = sourceIndex;

        // Disable knob click capture when in assign mode so we intercept drags
        auto toggle = [active] (juce::Component* c)
        { if (c) c->setInterceptsMouseClicks (!active, !active); };

        toggle (posKnob); toggle (spreadKnob);
        toggle (rateKnob); toggle (syncDivKnob);
        toggle (lenKnob);  toggle (sizeRatioKnob);
        toggle (pitchKnob); toggle (&shapeKnob);
        toggle (gainKnob);  toggle (reverseKnob);

        // Same treatment for FX knobs (currently visible ones)
        fxPanel.setKnobsClickThrough (active);

        repaint();
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! assignMode || modEngine == nullptr) return;

        auto slots = buildAllModSlots();
        int slotIdx = findSlotAt (e.getPosition(), slots);
        if (slotIdx < 0) return;
        const auto& paramId = slots[slotIdx].paramId;

        // Right-click in assign mode → context menu for managing modulators on this knob
        if (e.mods.isRightButtonDown() || e.mods.isPopupMenu())
        {
            showModContextMenuForParam (paramId, e.getScreenPosition());
            return;
        }

        // Option/Alt-click in assign mode → toggle bipolar/unipolar on this connection.
        // Step Sequencer and Envelope Follower are always unipolar, so option-click is a no-op.
        if (e.mods.isAltDown())
        {
            if (assignSource == ModulationEngine::kStepSeq
             || assignSource == ModulationEngine::kEnvFollower) return;

            if (! modEngine->hasConnection (assignSource, paramId))
                modEngine->addOrUpdateConnection (assignSource, paramId, 0.0f, true);
            else
                modEngine->toggleConnectionBipolar (assignSource, paramId);
            repaint();
            return;
        }

        draggingParamId  = paramId;
        dragStartAmount  = modEngine->getConnectionAmount (assignSource, paramId);
        dragStartY       = e.y;
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (! assignMode || modEngine == nullptr) return;
        auto slots = buildAllModSlots();
        int slotIdx = findSlotAt (e.getPosition(), slots);
        if (slotIdx < 0) return;
        modEngine->removeConnection (assignSource, slots[slotIdx].paramId);
        repaint();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! assignMode || modEngine == nullptr || draggingParamId.isEmpty()) return;
        float delta = (dragStartY - e.y) / 100.0f;
        float newAmount = juce::jlimit (-1.0f, 1.0f, dragStartAmount + delta);
        modEngine->addOrUpdateConnection (assignSource, draggingParamId, newAmount);
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (assignMode && ! draggingParamId.isEmpty())
        {
            float a = modEngine->getConnectionAmount (assignSource, draggingParamId);
            if (std::abs (a) < 0.01f)
                modEngine->removeConnection (assignSource, draggingParamId);
            draggingParamId.clear();
            repaint();
        }
    }

    void paint (juce::Graphics& g) override
    {
        // Section labels
        g.setColour (juce::Colour (0xffcccccc));
        g.setFont (juce::Font ("Avenir", 13.0f, juce::Font::bold));
        g.drawText ("Grain Control", grainLabelBounds, juce::Justification::centredLeft);
        g.drawText ("FX Chain", fxLabelBounds, juce::Justification::centredLeft);

        // Main section panels — per-head accent colour
        g.setColour (accentColour.withAlpha (0.30f));
        g.fillRoundedRectangle (grainPanelBounds.toFloat(), 16.0f);
        g.fillRoundedRectangle (fxPanelBounds.toFloat(), 16.0f);

        // Knob card backgrounds
        g.setColour (accentColour.withAlpha (0.22f));
        for (auto& r : knobCardBounds)
            g.fillRoundedRectangle (r.toFloat(), 10.0f);
    }

    void paintOverChildren (juce::Graphics& g) override
    {
        if (modEngine == nullptr) return;

        auto slots = buildAllModSlots();

        for (const auto& slot : slots)
        {
            juce::Slider* slider = slot.slider;
            if (slider == nullptr) continue;

            // Translate slider's local bounds into EngineColumn coords;
            // mirror the .reduced(4) inside drawRotarySlider for pixel alignment
            auto sliderInCol = getLocalArea (slider, slider->getLocalBounds()).toFloat();
            auto knobBounds  = sliderInCol.reduced (4.0f);
            float radius = juce::jmin (knobBounds.getWidth(), knobBounds.getHeight()) * 0.5f;
            float cx = knobBounds.getCentreX();
            float cy = knobBounds.getCentreY();

            // Knob's angular range and current position
            auto rp = slider->getRotaryParameters();
            float startA = rp.startAngleRadians;
            float endA   = rp.endAngleRadians;
            float angularRange = endA - startA;
            float normPos = static_cast<float> (slider->valueToProportionOfLength (slider->getValue()));
            float currentA = startA + normPos * angularRange;

            auto conns = modEngine->getConnectionsForParam (slot.paramId);

            if (assignMode)
            {
                float r = radius + 2.0f;
                juce::Colour outlineCol;
                if      (currentViewSource == ModulationEngine::kStepSeq)     outlineCol = juce::Colour (0xff7DD3FC); // sky blue
                else if (currentViewSource == ModulationEngine::kEnvFollower) outlineCol = juce::Colour (0xffF5C84A); // yellow
                else                                                          outlineCol = juce::Colours::white;
                g.setColour (outlineCol.withAlpha (0.25f));
                g.drawEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f, 0.8f);
            }

            // Modulation arcs — only show connections from the currently-viewed source
            float ringRadius = radius + 4.0f;
            for (const auto& c : conns)
            {
                if (c.sourceIndex != currentViewSource) continue;

                float amt = juce::jlimit (-1.0f, 1.0f, c.amount);
                bool bipolar = c.bipolar; // per-connection — toggle via Option-click in assign mode

                float arcA, arcB;
                if (bipolar)
                {
                    // Bipolar source [-1..1]: arc spans both sides of current position
                    float halfSwing = std::abs (amt) * angularRange;
                    arcA = currentA - halfSwing;
                    arcB = currentA + halfSwing;
                }
                else
                {
                    // Unipolar source [0..1]: arc only extends in the direction of amount sign
                    float swing = amt * angularRange;
                    arcA = currentA;
                    arcB = currentA + swing;
                }

                // Clip to the knob's start/end so the arc never crosses past the dial limits
                arcA = juce::jlimit (startA, endA, arcA);
                arcB = juce::jlimit (startA, endA, arcB);
                if (arcA > arcB) std::swap (arcA, arcB);

                if (std::abs (arcB - arcA) > 0.001f)
                {
                    juce::Path arc;
                    arc.addCentredArc (cx, cy, ringRadius, ringRadius, 0.0f,
                                       arcA, arcB, true);
                    // White for LFO, sky blue for Step Sequencer, yellow for Envelope Follower
                    juce::Colour ringCol;
                    if      (c.sourceIndex == ModulationEngine::kStepSeq)     ringCol = juce::Colour (0xff7DD3FC);
                    else if (c.sourceIndex == ModulationEngine::kEnvFollower) ringCol = juce::Colour (0xffF5C84A);
                    else                                                      ringCol = juce::Colours::white;
                    float alpha = c.bypassed ? 0.20f : 0.85f;
                    g.setColour (ringCol.withAlpha (alpha));
                    g.strokePath (arc, juce::PathStrokeType (1.4f));
                }
                ringRadius += 3.0f;
            }
        }

        // Disabled-state dim: cover only the panel areas, leaving the top label/button row visible
        if (! grainEnabled)
        {
            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.fillRoundedRectangle (grainPanelBounds.toFloat(), 16.0f);
            g.fillRoundedRectangle (fxPanelBounds.toFloat(),    16.0f);
        }
    }

    // Returns the actual juce::Slider child for a given card slot 0..7,
    // accounting for the rate/syncDiv and len/sizeRatio swap.
    juce::Slider* getCardSliderForSlot (int slot)
    {
        bool isSync   = syncBtn.getToggleState();
        bool isLinked = sizeLinkBtn.getToggleState();
        switch (slot)
        {
            case 0: return &posKnob->getSlider();
            case 1: return &spreadKnob->getSlider();
            case 2: return isSync   ? &syncDivKnob->getSlider()   : &rateKnob->getSlider();
            case 3: return isLinked ? &sizeRatioKnob->getSlider() : &lenKnob->getSlider();
            case 4: return &pitchKnob->getSlider();
            case 5: return &shapeKnob.getSlider();
            case 6: return &gainKnob->getSlider();
            case 7: return &reverseKnob->getSlider();
        }
        return nullptr;
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (6, 4);

        int gap = 8;
        int grainW = static_cast<int> (area.getWidth() * 0.50f);
        auto grainHalf = area.removeFromLeft (grainW);
        area.removeFromLeft (gap);
        auto fxHalf = area;

        // Label row
        auto grainLabelRow = grainHalf.removeFromTop (22);
        auto fxLabelRow = fxHalf.removeFromTop (22);
        grainHalf.removeFromTop (4);
        fxHalf.removeFromTop (4);

        // FX label area: "FX Chain" on left + Advanced button on right
        fxLabelBounds = fxLabelRow;
        advancedBtn.setBounds (fxLabelRow.removeFromRight (80).withHeight (20).withY (fxLabelRow.getY()));

        grainLabelBounds = grainLabelRow.removeFromLeft (100);
        enableBtn.setBounds (grainLabelRow.removeFromRight (44).withHeight (20).withY (grainLabelRow.getY()));
        freezeBtn.setBounds (grainLabelRow.removeFromRight (55).withHeight (20).withY (grainLabelRow.getY()));

        grainPanelBounds = grainHalf;
        fxPanelBounds = fxHalf;

        // === Grain Control: 4x2 grid of knob cards ===
        auto gp = grainPanelBounds.reduced (6, 4);
        bool isSync = syncBtn.getToggleState();
        bool isLinked = sizeLinkBtn.getToggleState();

        int cols = 4, rows = 2;
        int ctrlH = 18; // Sync/Note/Link row height
        int cardGap = 4; // gap between cards

        // Available for cards after ctrl row
        int availW = gp.getWidth();
        int availH = gp.getHeight() - ctrlH - 2; // 2px gap below ctrl row
        int cardW = (availW - cardGap * (cols - 1)) / cols;
        int cardH = (availH - cardGap * (rows - 1)) / rows;

        knobCardBounds.clear();

        auto cardRect = [&] (int col, int row) -> juce::Rectangle<int>
        {
            int x = gp.getX() + col * (cardW + cardGap);
            int y = gp.getY() + ctrlH + 2 + row * (cardH + cardGap);
            return { x, y, cardW, cardH };
        };

        // Sync/Note/Link row — above the grid, spanning cols 2-3
        {
            // Use the full width of cols 2+3 for Sync + NoteType + Link
            int ctrlX = gp.getX() + 2 * (cardW + cardGap);
            int ctrlY = gp.getY();
            int totalW = cardW * 2 + cardGap; // both columns combined
            int syncW = totalW * 25 / 100;  // "Sync"
            int noteW = totalW * 35 / 100;  // "Norm ▼"
            int linkW = totalW - syncW - noteW; // "Link"
            syncBtn.setBounds (ctrlX, ctrlY, syncW, ctrlH);
            syncTypeBox.setBounds (ctrlX + syncW, ctrlY, noteW, ctrlH);
            sizeLinkBtn.setBounds (ctrlX + syncW + noteW, ctrlY, linkW, ctrlH);
        }

        // Row 0 cards
        auto c00 = cardRect (0, 0);  knobCardBounds.push_back (c00);
        auto c01 = cardRect (1, 0);  knobCardBounds.push_back (c01);
        auto c02 = cardRect (2, 0);  knobCardBounds.push_back (c02);
        auto c03 = cardRect (3, 0);  knobCardBounds.push_back (c03);

        posKnob->setBounds (c00);
        spreadKnob->setBounds (c01);
        (isSync ? syncDivKnob : rateKnob)->setBounds (c02);
        (isLinked ? sizeRatioKnob : lenKnob)->setBounds (c03);

        // Row 1 cards
        auto c10 = cardRect (0, 1);  knobCardBounds.push_back (c10);
        auto c11 = cardRect (1, 1);  knobCardBounds.push_back (c11);
        auto c12 = cardRect (2, 1);  knobCardBounds.push_back (c12);
        auto c13 = cardRect (3, 1);  knobCardBounds.push_back (c13);

        pitchKnob->setBounds (c10);
        shapeKnob.setBounds (c11);
        gainKnob->setBounds (c12);
        reverseKnob->setBounds (c13);

        // === FX Chain Panel ===
        fxPanel.setBounds (fxPanelBounds.reduced (8));
    }

private:
    int index;
    juce::Colour accentColour;
    juce::Rectangle<int> grainLabelBounds, fxLabelBounds;
    juce::Rectangle<int> grainPanelBounds, fxPanelBounds;
    std::vector<juce::Rectangle<int>> knobCardBounds;
    std::array<juce::String, 8> cardParamIds;

    ModulationEngine* modEngine = nullptr;
    juce::AudioProcessorValueTreeState& apvtsRef;
    bool grainEnabled = true;
    bool assignMode = false;
    int  assignSource = 0;
    int  currentViewSource = ModulationEngine::kLFO0; // default to LFO 1
    juce::String draggingParamId;
    float dragStartAmount = 0.0f;
    int  dragStartY = 0;

    // Read bipolar state directly from APVTS so the UI stays in sync without
    // depending on audio-thread timing.
    bool isSourceBipolarFromApvts (int sourceIndex) const
    {
        if (sourceIndex == ModulationEngine::kStepSeq)
        {
            if (auto* p = apvtsRef.getRawParameterValue ("seq_bipolar"))
                return p->load() >= 0.5f;
            return false;
        }
        if (ModulationEngine::isLFOSource (sourceIndex))
        {
            auto pid = "lfo" + juce::String (sourceIndex) + "_bipolar";
            if (auto* p = apvtsRef.getRawParameterValue (pid))
                return p->load() >= 0.5f;
        }
        return true;
    }

    int findCardAt (juce::Point<int> p) const
    {
        for (size_t i = 0; i < knobCardBounds.size(); ++i)
            if (knobCardBounds[i].contains (p))
                return static_cast<int> (i);
        return -1;
    }

    // Unified mod-slot list — all currently-modulatable knobs (8 grain + visible FX)
    struct ModSlot { juce::Slider* slider; juce::String paramId; };

    std::vector<ModSlot> buildAllModSlots()
    {
        std::vector<ModSlot> slots;
        for (int i = 0; i < (int) cardParamIds.size(); ++i)
            if (auto* s = getCardSliderForSlot (i))
                slots.push_back ({ s, cardParamIds[i] });

        for (auto& t : fxPanel.getAllVisibleModTargets())
            slots.push_back ({ t.slider, t.paramId });

        return slots;
    }

    int findSlotAt (juce::Point<int> p, const std::vector<ModSlot>& slots) const
    {
        for (size_t i = 0; i < slots.size(); ++i)
        {
            auto bounds = getLocalArea (slots[i].slider, slots[i].slider->getLocalBounds());
            if (bounds.contains (p)) return static_cast<int> (i);
        }
        return -1;
    }

    juce::String sourceDisplayName (int srcIdx) const
    {
        if (srcIdx == ModulationEngine::kStepSeq) return "Step Seq";
        if (ModulationEngine::isLFOSource (srcIdx))
            return "LFO " + juce::String (srcIdx + 1);
        return "Source " + juce::String (srcIdx);
    }

    void showModContextMenuForParam (const juce::String& paramId, juce::Point<int> screenPos)
    {
        if (modEngine == nullptr) return;
        auto conns = modEngine->getConnectionsForParam (paramId);

        juce::PopupMenu menu;

        if (conns.empty())
        {
            menu.addSectionHeader ("No modulators on this control");
        }
        else
        {
            menu.addSectionHeader ("Modulators");
            for (const auto& c : conns)
            {
                const juce::String name = sourceDisplayName (c.sourceIndex);
                menu.addItem (1000 + c.sourceIndex,
                              "Bypass " + name,
                              true,
                              c.bypassed);
            }
            for (const auto& c : conns)
            {
                const juce::String name = sourceDisplayName (c.sourceIndex);
                menu.addItem (2000 + c.sourceIndex, "Remove " + name);
            }
            menu.addSeparator();
            menu.addItem (9999, "Remove All Modulators");
        }

        // Apply CerberusLookAndFeel directly (Avenir Bold, dark theme — same as FX dropdowns)
        menu.setLookAndFeel (&getLookAndFeel());

        // Anchor the menu at the actual mouse cursor location
        auto opts = juce::PopupMenu::Options()
                        .withTargetScreenArea (juce::Rectangle<int> (screenPos.x, screenPos.y, 1, 1))
                        .withMinimumWidth (180);

        menu.showMenuAsync (opts,
            [this, paramId] (int result)
            {
                if (result == 0 || modEngine == nullptr) return;
                if (result == 9999)
                {
                    modEngine->removeAllConnectionsForParam (paramId);
                }
                else if (result >= 1000 && result < 2000)
                {
                    int src = result - 1000;
                    bool current = modEngine->isConnectionBypassed (src, paramId);
                    modEngine->setConnectionBypassed (src, paramId, ! current);
                }
                else if (result >= 2000 && result < 3000)
                {
                    int src = result - 2000;
                    modEngine->removeConnection (src, paramId);
                }
                repaint();
            });
    }

    juce::ToggleButton enableBtn;
    juce::TextButton freezeBtn;

public:
    juce::TextButton advancedBtn;
    std::function<void (bool)> onAdvancedToggled;

private:

    RotaryKnob* posKnob = nullptr;
    RotaryKnob* spreadKnob = nullptr;
    RotaryKnob* rateKnob = nullptr;
    RotaryKnob* lenKnob = nullptr;
    RotaryKnob* pitchKnob = nullptr;
    RotaryKnob* gainKnob = nullptr;
    RotaryKnob* reverseKnob = nullptr;
    RotaryKnob* syncDivKnob = nullptr;
    RotaryKnob* sizeRatioKnob = nullptr;
    GrainShapeKnob shapeKnob;
    FXChainPanel fxPanel;
    juce::TextButton syncBtn;
    juce::ComboBox syncTypeBox;
    juce::TextButton sizeLinkBtn;

    using SliderAttach = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttach = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAttach  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<ButtonAttach> enableAttach, sizeLinkAttach, freezeAttach;
    std::unique_ptr<SliderAttach> shapeAttach, syncDivAttach, sizeRatioAttach;
    std::unique_ptr<ComboAttach> syncTypeAttach;
    juce::OwnedArray<SliderAttach> knobAttachments;
    std::unique_ptr<juce::ParameterAttachment> enableAttachmentCb;

    void updateRateModeVisibility()
    {
        bool isSync = syncBtn.getToggleState();
        rateKnob->setVisible (!isSync);
        syncDivKnob->setVisible (isSync);
        syncTypeBox.setEnabled (isSync);
        syncTypeBox.setAlpha (isSync ? 1.0f : 0.3f);
        resized();
    }

    void updateSizeLinkVisibility()
    {
        bool linked = sizeLinkBtn.getToggleState();
        lenKnob->setVisible (!linked);
        sizeRatioKnob->setVisible (linked);
        resized();
    }
};
