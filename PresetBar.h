#pragma once
#include <JuceHeader.h>
#include "PresetManager.h"

class PresetBar : public juce::Component, private juce::ComboBox::Listener, private juce::Button::Listener
{
public:
    explicit PresetBar(PresetManager& pm);
    ~PresetBar() override = default;
    void paint(juce::Graphics&) override;
    void resized() override;
    void refreshList();

private:
    PresetManager& presetManager;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, saveBtn { "SAVE" }, delBtn { "DEL" };
    juce::ComboBox presetBox;
    bool updatingBox = false;

    void comboBoxChanged(juce::ComboBox*) override;
    void buttonClicked(juce::Button*) override;
    void updatePresetName();
    void showSaveDialog();
    void showDeleteConfirm();
};
