
#include "PresetBar.h"

PresetBar::PresetBar(PresetManager& pm)
    : presetManager(pm)
{
    addAndMakeVisible(prevBtn);
    addAndMakeVisible(nextBtn);
    addAndMakeVisible(saveBtn);
    addAndMakeVisible(delBtn);
    addAndMakeVisible(presetBox);

    prevBtn.addListener(this);
    nextBtn.addListener(this);
    saveBtn.addListener(this);
    delBtn.addListener(this);
    presetBox.addListener(this);

    presetBox.setColour(
        juce::ComboBox::backgroundColourId,
        juce::Colour(0xFF0A0E1A));

    presetBox.setColour(
        juce::ComboBox::textColourId,
        juce::Colour(0xFF00E5FF));

    presetBox.setColour(
        juce::ComboBox::outlineColourId,
        juce::Colour(0x6600E5FF));

    presetManager.onPresetChanged = [this]()
    {
        updatePresetName();
    };

    presetManager.onPresetListChanged = [this]()
    {
        refreshList();
    };

    refreshList();
    updatePresetName();
}

PresetBar::~PresetBar()
{
    // Prevent callbacks from accessing a destroyed editor.
    presetManager.onPresetChanged = {};
    presetManager.onPresetListChanged = {};

    prevBtn.removeListener(this);
    nextBtn.removeListener(this);
    saveBtn.removeListener(this);
    delBtn.removeListener(this);
    presetBox.removeListener(this);
}

void PresetBar::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour(juce::Colour(0xE6161A26));
    g.fillRoundedRectangle(bounds, 4.0f);

    g.setColour(juce::Colour(0x6600E5FF));
    g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
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

    const auto factory = presetManager.getFactoryPresetNames();

    for (int i = 0; i < factory.size(); ++i)
        presetBox.addItem(factory[i], id++);

    const auto user = presetManager.getUserPresetNames();

    if (!user.isEmpty())
    {
        presetBox.addSectionHeading("User");

        for (int i = 0; i < user.size(); ++i)
            presetBox.addItem(user[i], id++);
    }

    updatingBox = false;

    updatePresetName();
}

void PresetBar::updatePresetName()
{
    updatingBox = true;

    const auto name = presetManager.getCurrentPresetName();

    presetBox.setText(
        name.isEmpty() ? "<no preset>" : name,
        juce::dontSendNotification);

    updatingBox = false;
}

void PresetBar::comboBoxChanged(juce::ComboBox* cb)
{
    if (updatingBox || cb != &presetBox)
        return;

    const auto name = presetBox.getText();

    if (name.isNotEmpty()
        && name != presetManager.getCurrentPresetName())
    {
        presetManager.loadPreset(name);
    }
}

void PresetBar::buttonClicked(juce::Button* button)
{
    if (button == &prevBtn)
    {
        presetManager.loadPreviousPreset();
    }
    else if (button == &nextBtn)
    {
        presetManager.loadNextPreset();
    }
    else if (button == &saveBtn)
    {
        showSaveDialog();
    }
    else if (button == &delBtn)
    {
        showDeleteConfirm();
    }
}

void PresetBar::showSaveDialog()
{
    auto* window = new juce::AlertWindow(
        "Save Preset",
        "Enter a preset name:",
        juce::MessageBoxIconType::NoIcon);

    window->addTextEditor(
        "name",
        "My Preset",
        "Name:");

    window->addButton(
        "Save",
        1,
        juce::KeyPress(juce::KeyPress::returnKey));

    window->addButton(
        "Cancel",
        0,
        juce::KeyPress(juce::KeyPress::escapeKey));

    juce::Component::SafePointer<PresetBar> safeThis(this);

    window->enterModalState(
        true,
        juce::ModalCallbackFunction::create(
            [safeThis, window](int result)
            {
                if (result != 1 || safeThis == nullptr)
                    return;

                const auto name =
                    window->getTextEditorContents("name").trim();

                if (name.isNotEmpty())
                    safeThis->presetManager.savePreset(name);
            }),
        true);
}

void PresetBar::showDeleteConfirm()
{
    const auto name = presetManager.getCurrentPresetName();

    if (name.isEmpty() || presetManager.isFactoryPreset(name))
        return;

    juce::Component::SafePointer<PresetBar> safeThis(this);

    juce::AlertWindow::showAsync(
        juce::MessageBoxOptions()
            .withIconType(juce::MessageBoxIconType::QuestionIcon)
            .withTitle("Delete Preset")
            .withMessage("Delete \"" + name + "\"?")
            .withButton("Delete")
            .withButton("Cancel"),

        [safeThis, name](int result)
        {
            if (safeThis == nullptr)
                return;

            if (result == 1)
                safeThis->presetManager.deletePreset(name);
        });
}
