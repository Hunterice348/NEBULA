#include "PresetBar.h"

PresetBar::PresetBar(PresetManager& pm) : presetManager(pm)
{
    addAndMakeVisible(prevBtn); addAndMakeVisible(nextBtn);
    addAndMakeVisible(saveBtn); addAndMakeVisible(delBtn);
    addAndMakeVisible(presetBox);

    prevBtn.addListener(this); nextBtn.addListener(this);
    saveBtn.addListener(this); delBtn.addListener(this);
    presetBox.addListener(this);

    presetBox.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xFF0A0E1A));
    presetBox.setColour(juce::ComboBox::textColourId, juce::Colour(0xFF00E5FF));
    presetBox.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xFF00E5FF).withAlpha(0.4f));

    presetManager.onPresetChanged = [this]() { updatePresetName(); };
    presetManager.onPresetListChanged = [this]() { refreshList(); };
    refreshList();
    updatePresetName();
}

void PresetBar::paint(juce::Graphics& g)
{
    g.setColour(juce::Colour(0xFF161A26).withAlpha(0.9f));
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 4.0f);
    g.setColour(juce::Colour(0xFF00E5FF).withAlpha(0.3f));
    g.drawRoundedRectangle(getLocalBounds().toFloat(), 4.0f, 1.0f);
}

void PresetBar::resized()
{
    auto area = getLocalBounds().reduced(3);
    prevBtn.setBounds(area.removeFromLeft(26));
    nextBtn.setBounds(area.removeFromLeft(26));
    area.removeFromLeft(6);
    delBtn.setBounds(area.removeFromRight(50));
    saveBtn.setBounds(area.removeFromRight(50));
    area.removeFromRight(6);
    presetBox.setBounds(area);
}

void PresetBar::refreshList()
{
    updatingBox = true;
    presetBox.clear(juce::dontSendNotification);
    int id = 1;
    presetBox.addSectionHeading("Factory");
    for (auto& n : presetManager.getFactoryPresetNames()) presetBox.addItem(n, id++);
    auto user = presetManager.getUserPresetNames();
    if (!user.isEmpty()) {
        presetBox.addSectionHeading("User");
        for (auto& n : user) presetBox.addItem(n, id++);
    }
    updatingBox = false;
}

void PresetBar::updatePresetName()
{
    updatingBox = true;
    auto n = presetManager.getCurrentPresetName();
    presetBox.setText(n.isEmpty() ? "<no preset>" : n, juce::dontSendNotification);
    updatingBox = false;
}

void PresetBar::comboBoxChanged(juce::ComboBox* cb)
{
    if (updatingBox || cb != &presetBox) return;
    auto n = presetBox.getText();
    if (n.isNotEmpty() && n != presetManager.getCurrentPresetName()) presetManager.loadPreset(n);
}

void PresetBar::buttonClicked(juce::Button* b)
{
    if (b == &prevBtn) presetManager.loadPreviousPreset();
    else if (b == &nextBtn) presetManager.loadNextPreset();
    else if (b == &saveBtn) showSaveDialog();
    else if (b == &delBtn) showDeleteConfirm();
}

void PresetBar::showSaveDialog()
{
    auto* aw = new juce::AlertWindow("Save Preset", "Enter a name:", juce::MessageBoxIconType::NoIcon);
    aw->addTextEditor("name", "My Preset", "Name:");
    aw->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    aw->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    aw->enterModalState(true, juce::ModalCallbackFunction::create([this, aw](int r) {
        if (r == 1) presetManager.savePreset(aw->getTextEditorContents("name").trim());
    }), true);
}

void PresetBar::showDeleteConfirm()
{
    auto n = presetManager.getCurrentPresetName();
    if (n.isEmpty() || presetManager.isFactoryPreset(n)) return;
    juce::AlertWindow::showAsync(juce::MessageBoxOptions()
        .withIconType(juce::MessageBoxIconType::QuestionIcon)
        .withTitle("Delete Preset").withMessage("Delete \"" + n + "\"?")
        .withButton("Delete").withButton("Cancel"),
        [this, n](int r) { if (r == 1) presetManager.deletePreset(n); });
}
