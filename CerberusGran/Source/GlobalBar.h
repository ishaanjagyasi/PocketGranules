#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

// Custom button for the Presets menu — rectangular background, text + small dropdown arrow,
// same font size/weight as the "Live mode" combobox.
class PresetMenuButton : public juce::TextButton
{
public:
    PresetMenuButton() : juce::TextButton ("Presets") {}

    void paintButton (juce::Graphics& g, bool, bool) override
    {
        auto bounds = getLocalBounds().toFloat();

        // Rectangular background (not pill)
        g.setColour (juce::Colour (0xff2E2E34));
        g.fillRect (bounds);

        // Text
        juce::Font font ("Avenir", 16.0f, juce::Font::bold);
        g.setFont (font);
        g.setColour (juce::Colour (0xffcccccc));

        auto text = getButtonText();
        int textW = font.getStringWidth (text);
        int gap   = 8;
        int arrowW = 8;
        int totalW = textW + gap + arrowW;
        int startX = static_cast<int> (bounds.getCentreX() - totalW * 0.5f);

        g.drawText (text, startX, 0, textW, static_cast<int> (bounds.getHeight()),
                    juce::Justification::centredLeft);

        // Down arrow triangle
        float arrowCx = static_cast<float> (startX + textW + gap + arrowW * 0.5f);
        float arrowCy = bounds.getCentreY();
        juce::Path arrow;
        arrow.addTriangle (arrowCx - 4.0f, arrowCy - 2.5f,
                           arrowCx + 4.0f, arrowCy - 2.5f,
                           arrowCx,        arrowCy + 3.5f);
        g.setColour (juce::Colour (0xff888888));
        g.fillPath (arrow);
    }
};

class GlobalBar : public juce::Component
{
public:
    GlobalBar (juce::AudioProcessorValueTreeState& apvts,
               const std::array<juce::Colour, 5>& colours)
        : apvtsRef (apvts), headColours (colours)
    {
        // Presets menu — rectangular background, text + arrow style
        presetBtn.onClick = [this] { openPresetMenu(); };
        addAndMakeVisible (presetBtn);
        // Head navigation — invisible buttons, triangles painted in paint()
        prevBtn.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        prevBtn.setColour (juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
        prevBtn.onClick = [this] { switchHead (currentHead - 1); };
        addAndMakeVisible (prevBtn);

        nextBtn.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        nextBtn.setColour (juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
        nextBtn.onClick = [this] { switchHead (currentHead + 1); };
        addAndMakeVisible (nextBtn);

        // Plugin name
        nameLabel.setText ("Pocket Granules", juce::dontSendNotification);
        nameLabel.setFont (juce::Font ("Avenir", 30.0f, juce::Font::bold));
        nameLabel.setColour (juce::Label::textColourId, juce::Colour (0xffeeeeee));
        nameLabel.setJustificationType (juce::Justification::centredLeft);
        nameLabel.setBorderSize (juce::BorderSize<int> (0));
        addAndMakeVisible (nameLabel);

        // Source mode selector — transparent background, just text + arrow, big font
        sourceModeBox.addItemList ({ "Live mode", "File mode" }, 1);
        sourceModeBox.setColour (juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
        sourceModeBox.setColour (juce::ComboBox::textColourId, juce::Colour (0xffcccccc));
        sourceModeBox.setComponentID ("sourceModeBigCombo");
        sourceModeBox.onChange = [this] { onModeChanged(); };
        addAndMakeVisible (sourceModeBox);
        sourceModeAttach = std::make_unique<ComboAttach> (apvts, "sourceMode", sourceModeBox);

        // Gain horizontal bar slider
        gainSlider.setSliderStyle (juce::Slider::LinearBar);
        gainSlider.setTextBoxIsEditable (false);
        gainSlider.setTextValueSuffix (" dB");
        gainSlider.setScrollWheelEnabled (false);
        gainSlider.setSliderSnapsToMousePosition (false);
        addAndMakeVisible (gainSlider);
        gainAttach = std::make_unique<SliderAttach> (apvts, "masterGain", gainSlider);

        // Dry/Wet horizontal bar slider
        mixSlider.setSliderStyle (juce::Slider::LinearBar);
        mixSlider.setTextBoxIsEditable (false);
        mixSlider.setTextValueSuffix ("%");
        mixSlider.setScrollWheelEnabled (false);
        mixSlider.setSliderSnapsToMousePosition (true);
        addAndMakeVisible (mixSlider);
        mixAttach = std::make_unique<SliderAttach> (apvts, "mix", mixSlider);
    }

    void switchHead (int newHead)
    {
        newHead = juce::jlimit (0, 4, newHead);
        if (newHead == currentHead) return;
        currentHead = newHead;
        repaint();
        if (onHeadChanged) onHeadChanged (currentHead);
    }

    int getCurrentHead() const { return currentHead; }

    std::function<void(int)> onHeadChanged;

    void paint (juce::Graphics& g) override
    {
        g.setColour (juce::Colour (0xff1E1E22));
        g.fillRect (getLocalBounds());

        // Bottom border
        g.setColour (juce::Colour (0xff3A3A40));
        g.drawLine (0.0f, static_cast<float> (getHeight()),
                    static_cast<float> (getWidth()), static_cast<float> (getHeight()), 0.5f);

        // Nav triangles
        {
            auto pb = prevBtn.getBounds().toFloat();
            float cx = pb.getCentreX(), cy = pb.getCentreY(), sz = 7.0f;
            juce::Path tri;
            tri.addTriangle (cx + sz, cy - sz, cx + sz, cy + sz, cx - sz, cy);
            g.setColour (juce::Colour (0xffdddddd));
            g.fillPath (tri);
        }
        {
            auto nb = nextBtn.getBounds().toFloat();
            float cx = nb.getCentreX(), cy = nb.getCentreY(), sz = 7.0f;
            juce::Path tri;
            tri.addTriangle (cx - sz, cy - sz, cx - sz, cy + sz, cx + sz, cy);
            g.setColour (juce::Colour (0xffdddddd));
            g.fillPath (tri);
        }

        // Head badge
        auto badgeColour = headColours[currentHead];
        float badgeSize = 26.0f;
        float badgeX = prevBtn.getRight() + 3.0f;
        float badgeY = getHeight() * 0.5f - badgeSize * 0.5f;
        g.setColour (badgeColour);
        g.fillRoundedRectangle (badgeX, badgeY, badgeSize, badgeSize, 7.0f);

        // Labels above sliders — Avenir Bold, larger
        g.setColour (juce::Colour (0xffcccccc));
        g.setFont (juce::Font ("Avenir", 14.0f, juce::Font::bold));
        g.drawText ("Gain", gainSlider.getBounds().translated (0, -16).withHeight (14),
                    juce::Justification::centredLeft);
        g.drawText ("Dry/Wet", mixSlider.getBounds().translated (0, -16).withHeight (14),
                    juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (8, 4);
        int h = area.getHeight();
        int btnH = 24;
        int cy = area.getY() + (h - btnH) / 2;

        // [◀] [badge] [▶]
        prevBtn.setBounds (area.removeFromLeft (24).withHeight (btnH).withY (cy));
        area.removeFromLeft (34); // badge space
        nextBtn.setBounds (area.removeFromLeft (24).withHeight (btnH).withY (cy));
        area.removeFromLeft (2);

        // Plugin name — measure text width so combo can sit tightly next to it
        int nameW = juce::Font ("Avenir", 30.0f, juce::Font::bold)
                        .getStringWidth (nameLabel.getText()) + 4;
        nameLabel.setBounds (area.removeFromLeft (nameW).withHeight (h));
        area.removeFromLeft (8);

        // Presets menu — rectangular box with text + arrow, just right of plugin name
        presetBtn.setBounds (area.removeFromLeft (90).withHeight (28).withY (cy - 2));

        // Right side — layout from right: [Gain] [gap] [D/W], then [Live mode] just left of Gain
        int barW = 56;
        int barH = 13;
        int barGap = 8;
        int barY = cy + (btnH - barH) / 2 + 5;

        mixSlider.setBounds (area.getRight() - barW, barY, barW, barH);
        gainSlider.setBounds (area.getRight() - barW * 2 - barGap, barY, barW, barH);

        // Force Avenir Bold on the slider's internal value-box label, since the
        // LookAndFeel's createSliderTextBox may have been called before our L&F was set
        forceAvenirOnSliderText (gainSlider);
        forceAvenirOnSliderText (mixSlider);

        // Mode selector placed just left of the Gain slider
        juce::Font modeFont ("Avenir", 16.0f, juce::Font::bold);
        int modeTextW = modeFont.getStringWidth (sourceModeBox.getText());
        if (modeTextW == 0) modeTextW = modeFont.getStringWidth ("Live mode");
        int modeBoxW = modeTextW + 32;
        int modeBoxX = gainSlider.getX() - modeBoxW - 10;
        sourceModeBox.setBounds (modeBoxX, cy - 2, modeBoxW, 28);
    }

private:
    juce::AudioProcessorValueTreeState& apvtsRef;
    const std::array<juce::Colour, 5>& headColours;
    int currentHead = 0;

    juce::TextButton prevBtn, nextBtn;
    juce::Label nameLabel;
    juce::ComboBox sourceModeBox;
    PresetMenuButton presetBtn;
    std::unique_ptr<juce::FileChooser> folderChooser;
    juce::Slider gainSlider, mixSlider;

    using SliderAttach = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttach = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAttach  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    std::unique_ptr<SliderAttach> gainAttach, mixAttach;
    std::unique_ptr<ComboAttach> sourceModeAttach;

    // Walks the slider's children to find its internal value-box Label and
    // forces Avenir Bold on it. Necessary because the slider's value box may have
    // been created with the default LookAndFeel before ours was applied.
    static void forceAvenirOnSliderText (juce::Slider& slider)
    {
        for (auto* child : slider.getChildren())
            if (auto* label = dynamic_cast<juce::Label*> (child))
                label->setFont (juce::Font ("Avenir", 11.0f, juce::Font::bold));
    }

    // ========== Preset menu helpers ==========

    static juce::PropertiesFile& propsFile()
    {
        static juce::PropertiesFile* instance = nullptr;
        if (instance == nullptr)
        {
            juce::PropertiesFile::Options opts;
            opts.applicationName  = "PocketGranules";
            opts.filenameSuffix   = ".settings";
            opts.osxLibrarySubFolder = "Application Support";
            opts.folderName       = "PocketGranules";
            instance = new juce::PropertiesFile (opts);
        }
        return *instance;
    }

    static juce::File getPresetFolder()
    {
        auto path = propsFile().getValue ("presetFolder", "");
        return path.isEmpty() ? juce::File() : juce::File (path);
    }

    static void setPresetFolder (const juce::File& folder)
    {
        propsFile().setValue ("presetFolder", folder.getFullPathName());
        propsFile().saveIfNeeded();
    }

    void openPresetMenu()
    {
        juce::PopupMenu menu;
        menu.setLookAndFeel (&getLookAndFeel());

        menu.addItem (1, "Save Preset");

        // Build Load Preset submenu from files in the saved folder
        juce::PopupMenu loadMenu;
        auto folder = getPresetFolder();
        if (folder.isDirectory())
        {
            auto files = folder.findChildFiles (juce::File::findFiles, false, "*.pgpreset");
            // Stable alphabetical order
            std::sort (files.begin(), files.end(),
                       [] (const juce::File& a, const juce::File& b)
                       { return a.getFileName().compareIgnoreCase (b.getFileName()) < 0; });

            if (files.isEmpty())
                loadMenu.addItem (0, "(no presets in folder)", false, false);
            else
            {
                int id = 1000;
                for (auto& f : files)
                    loadMenu.addItem (id++, f.getFileNameWithoutExtension());
            }
        }
        else
        {
            loadMenu.addItem (0, "(save a preset to set the folder)", false, false);
        }
        menu.addSubMenu ("Load Preset", loadMenu);

        menu.addSeparator();
        menu.addItem (2, "Init Preset");

        auto opts = juce::PopupMenu::Options()
                        .withTargetComponent (&presetBtn)
                        .withMinimumWidth (160);

        menu.showMenuAsync (opts, [this] (int result) { handlePresetResult (result); });
    }

    void handlePresetResult (int result)
    {
        if (result == 1) { savePreset(); return; }
        if (result == 2) { initPreset(); return; }
        if (result >= 1000)
        {
            auto folder = getPresetFolder();
            if (! folder.isDirectory()) return;
            auto files = folder.findChildFiles (juce::File::findFiles, false, "*.pgpreset");
            std::sort (files.begin(), files.end(),
                       [] (const juce::File& a, const juce::File& b)
                       { return a.getFileName().compareIgnoreCase (b.getFileName()) < 0; });
            int idx = result - 1000;
            if (idx >= 0 && idx < files.size()) loadPreset (files[idx]);
        }
    }

    void savePreset()
    {
        auto folder = getPresetFolder();
        if (! folder.isDirectory())
        {
            // First time: prompt for folder
            folderChooser = std::make_unique<juce::FileChooser> (
                "Choose folder to store presets",
                juce::File::getSpecialLocation (juce::File::userDocumentsDirectory));

            folderChooser->launchAsync (
                juce::FileBrowserComponent::canSelectDirectories
                    | juce::FileBrowserComponent::openMode,
                [this] (const juce::FileChooser& fc)
                {
                    auto chosen = fc.getResult();
                    if (chosen.isDirectory())
                    {
                        setPresetFolder (chosen);
                        promptAndSavePreset (chosen);
                    }
                });
        }
        else
        {
            promptAndSavePreset (folder);
        }
    }

    void promptAndSavePreset (const juce::File& folder)
    {
        auto* aw = new juce::AlertWindow ("Save Preset",
                                         "Enter preset name:",
                                         juce::AlertWindow::QuestionIcon);
        aw->addTextEditor ("name", "MyPreset");
        aw->addButton ("Save",   1, juce::KeyPress (juce::KeyPress::returnKey));
        aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

        aw->enterModalState (true,
            juce::ModalCallbackFunction::create (
                [aw, folder, this] (int result)
                {
                    if (result == 1)
                    {
                        auto name = aw->getTextEditorContents ("name").trim();
                        if (name.isNotEmpty())
                        {
                            auto file = folder.getChildFile (name + ".pgpreset");
                            saveStateToFile (file);
                        }
                    }
                    delete aw;
                }),
            true);
    }

    void saveStateToFile (const juce::File& file)
    {
        juce::MemoryBlock data;
        apvtsRef.processor.getStateInformation (data);
        file.replaceWithData (data.getData(), data.getSize());
    }

    void loadPreset (const juce::File& file)
    {
        juce::MemoryBlock data;
        if (file.loadFileAsData (data))
            apvtsRef.processor.setStateInformation (data.getData(), (int) data.getSize());
    }

    void initPreset()
    {
        if (auto* p = dynamic_cast<CerberusGranAudioProcessor*> (&apvtsRef.processor))
            p->resetToDefaults();
    }

    void onModeChanged()
    {
        resized(); // re-measure text width for any selection
        bool isFile = (sourceModeBox.getSelectedId() == 2);
        if (isFile)
        {
            if (auto* p = apvtsRef.getParameter ("mix"))
                p->setValueNotifyingHost (1.0f);
            mixSlider.setEnabled (false);
            mixSlider.setAlpha (0.4f);
        }
        else
        {
            mixSlider.setEnabled (true);
            mixSlider.setAlpha (1.0f);
        }
    }
};
