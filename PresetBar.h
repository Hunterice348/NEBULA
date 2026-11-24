
#pragma once

#include <JuceHeader.h>
#include <memory>

#include "PresetManager.h"

class PresetBar : public juce::Component,
                  private juce::ComboBox::Listener,
                  private juce::Button::Listener
{
public:
    explicit PresetBar(PresetManager& pm);
    ~PresetBar() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    void refreshList();

private:
    PresetManager& presetManager;

    juce::TextButton prevBtn { "<" };
    juce::TextButton nextBtn { ">" };
    juce::TextButton dataBtn { "DATA" };
    juce::TextButton saveBtn { "SAVE" };
    juce::TextButton delBtn { "DEL" };

    juce::ComboBox presetBox;

    bool updatingBox = false;

    void comboBoxChanged(juce::ComboBox*) override;
    void buttonClicked(juce::Button*) override;

    void updatePresetName();
    void showSaveDialog();
    void showDeleteConfirm();
    void showStorageChooser();

    void showStorageMessage(
        const juce::String& title,
        const juce::String& message);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetBar)
};
