
#include "PresetBar.h"
#include <memory>

// ============================================================
// CONSTRUCTOR
// ============================================================

PresetBar::PresetBar(PresetManager& pm)
    : presetManager(pm)
{
    addAndMakeVisible(prevBtn);
    addAndMakeVisible(nextBtn);
    addAndMakeVisible(dataBtn);
    addAndMakeVisible(saveBtn);
    addAndMakeVisible(delBtn);
    addAndMakeVisible(presetBox);

    prevBtn.addListener(this);
    nextBtn.addListener(this);
    dataBtn.addListener(this);
    saveBtn.addListener(this);
    delBtn.addListener(this);
    presetBox.addListener(this);

    prevBtn.setButtonText("<");
    nextBtn.setButtonText(">");
    dataBtn.setButtonText("DATA");
    saveBtn.setButtonText("SAVE");
    delBtn.setButtonText("DEL");

    dataBtn.setTooltip("Choose the NEBULA user data location");
    saveBtn.setTooltip("Save your current sound as a new user preset");
    delBtn.setTooltip("Delete the selected user preset");

    const auto cyan = juce::Colour(0xFF00E5FF);
    const auto background = juce::Colour(0xFF0A0E1A);

    presetBox.setColour(
        juce::ComboBox::backgroundColourId,
        background);

    presetBox.setColour(
        juce::ComboBox::textColourId,
        cyan);

    presetBox.setColour(
        juce::ComboBox::outlineColourId,
        juce::Colour(0x6600E5FF));

    dataBtn.setColour(
        juce::TextButton::buttonColourId,
        juce::Colour(0xFF172B3A));

    dataBtn.setColour(
        juce::TextButton::textColourOffId,
        cyan);

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

// ============================================================
// DESTRUCTOR
// ============================================================

PresetBar::~PresetBar()
{
    presetManager.onPresetChanged = {};
    presetManager.onPresetListChanged = {};

    prevBtn.removeListener(this);
    nextBtn.removeListener(this);
    dataBtn.removeListener(this);
    saveBtn.removeListener(this);
    delBtn.removeListener(this);
    presetBox.removeListener(this);
}

// ============================================================
// PAINT
// ============================================================

void PresetBar::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour(juce::Colour(0xE6161A26));
    g.fillRoundedRectangle(bounds, 4.0f);

    g.setColour(juce::Colour(0x6600E5FF));
    g.drawRoundedRectangle(bounds, 4.0f, 1.0f);
}

// ============================================================
// RESIZED
// ============================================================

void PresetBar::resized()
{
    auto area = getLocalBounds().reduced(3);

    prevBtn.setBounds(area.removeFromLeft(26));
    nextBtn.setBounds(area.removeFromLeft(26));

    area.removeFromLeft(5);

    dataBtn.setBounds(area.removeFromRight(52));
    area.removeFromRight(4);

    delBtn.setBounds(area.removeFromRight(46));
    saveBtn.setBounds(area.removeFromRight(50));

    area.removeFromRight(5);

    presetBox.setBounds(area);
}

// ============================================================
// REFRESH PRESET LIST
// ============================================================

void PresetBar::refreshList()
{
    updatingBox = true;

    presetBox.clear(juce::dontSendNotification);

    int id = 1;

    presetBox.addSectionHeading("Factory");

    const auto factory =
        presetManager.getFactoryPresetNames();

    for (const auto& name : factory)
        presetBox.addItem(name, id++);

    const auto user =
        presetManager.getUserPresetNames();

    if (!user.isEmpty())
    {
        presetBox.addSectionHeading("User");

        for (const auto& name : user)
            presetBox.addItem(name, id++);
    }

    updatingBox = false;

    updatePresetName();
}

// ============================================================
// UPDATE PRESET NAME
// ============================================================

void PresetBar::updatePresetName()
{
    updatingBox = true;

    const auto name =
        presetManager.getCurrentPresetName();

    presetBox.setText(
        name.isEmpty() ? "<no preset>" : name,
        juce::dontSendNotification);

    updatingBox = false;
}

// ============================================================
// COMBO BOX
// ============================================================

void PresetBar::comboBoxChanged(
    juce::ComboBox* combo)
{
    if (updatingBox || combo != &presetBox)
        return;

    const auto name = presetBox.getText();

    if (name.isNotEmpty()
        && name != presetManager.getCurrentPresetName())
    {
        const bool loaded =
            presetManager.loadPreset(name);

        if (!loaded)
        {
            showStorageMessage(
                "Preset Error",
                "NEBULA could not load this preset.");
        }
    }
}

// ============================================================
// BUTTONS
// ============================================================

void PresetBar::buttonClicked(
    juce::Button* button)
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
    else if (button == &dataBtn)
    {
        showStorageChooser();
    }
}

// ============================================================
// SAVE CUSTOM PRESET
// ============================================================

void PresetBar::showSaveDialog()
{
    auto window =
        std::make_shared<juce::AlertWindow>(
            "Create User Preset",
            "Save your current NEBULA sound as a preset.",
            juce::MessageBoxIconType::NoIcon);

    window->addTextEditor(
        "name",
        "My Preset",
        "Preset name:");

    window->addButton(
        "Save Preset",
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

                if (name.isEmpty())
                {
                    safeThis->showStorageMessage(
                        "Invalid Preset Name",
                        "Please enter a name for your preset.");

                    return;
                }

                const bool saved =
                    safeThis->presetManager.savePreset(name);

                if (saved)
                {
                    safeThis->showStorageMessage(
                        "Preset Created",
                        "Your custom preset was saved successfully:\n\n"
                        + name
                        + "\n\nIt is now available in the User preset list.");
                }
                else
                {
                    safeThis->showStorageMessage(
                        "Preset Save Failed",
                        "NEBULA could not save this preset.\n\n"
                        "Check the preset name and data folder permissions.\n"
                        "Factory preset names cannot be used.");
                }
            }),
        false);
}

// ============================================================
// DELETE CONFIRMATION
// ============================================================

void PresetBar::showDeleteConfirm()
{
    const auto name =
        presetManager.getCurrentPresetName();

    if (name.isEmpty())
    {
        showStorageMessage(
            "Delete Preset",
            "No preset is currently selected.");

        return;
    }

    if (presetManager.isFactoryPreset(name))
    {
        showStorageMessage(
            "Delete Preset",
            "Factory presets cannot be deleted.");

        return;
    }

    juce::Component::SafePointer<PresetBar> safeThis(this);

    juce::AlertWindow::showAsync(
        juce::MessageBoxOptions()
            .withIconType(
                juce::MessageBoxIconType::QuestionIcon)
            .withTitle("Delete Preset")
            .withMessage("Delete \"" + name + "\"?")
            .withButton("Delete")
            .withButton("Cancel"),

        [safeThis, name](int result)
        {
            if (safeThis == nullptr)
                return;

            // JUCE 8.0.6:
            // With two buttons, the first button returns 1.
            if (result == 1)
            {
                const bool deleted =
                    safeThis->presetManager.deletePreset(name);

                if (deleted)
                {
                    safeThis->showStorageMessage(
                        "Preset Deleted",
                        "The user preset was deleted.");
                }
                else
                {
                    safeThis->showStorageMessage(
                        "Delete Failed",
                        "NEBULA could not delete this preset.");
                }
            }
        });
}

// ============================================================
// GENERAL MESSAGE
// ============================================================

void PresetBar::showStorageMessage(
    const juce::String& title,
    const juce::String& message)
{
    juce::AlertWindow::showAsync(
        juce::MessageBoxOptions()
            .withIconType(
                juce::MessageBoxIconType::InfoIcon)
            .withTitle(title)
            .withMessage(message)
            .withButton("OK"));
}

// ============================================================
// STORAGE FOLDER CHOOSER
// ============================================================

void PresetBar::showStorageChooser()
{
    const auto currentRoot =
        presetManager.getStorageRoot();

    const auto initialLocation =
        currentRoot.getParentDirectory();

    auto chooser = std::make_shared<juce::FileChooser>(
        "Select a parent folder for NEBULA data",
        initialLocation,
        "*",
        true);

    juce::Component::SafePointer<PresetBar> safeThis(this);

    chooser->launchAsync(
        juce::FileBrowserComponent::openMode
            | juce::FileBrowserComponent::canSelectDirectories,

        [safeThis, chooser](const juce::FileChooser& fileChooser)
        {
            if (safeThis == nullptr)
                return;

            const auto selectedParent =
                fileChooser.getResult();

            if (!selectedParent.isDirectory())
                return;

            const auto destination =
                selectedParent.getChildFile("NEBULA");

            const auto oldLocation =
                safeThis->presetManager.getStorageRoot();

            if (destination == oldLocation)
            {
                safeThis->showStorageMessage(
                    "NEBULA Storage",
                    "This is already the active data location.");

                return;
            }

            if (destination.exists())
            {
                safeThis->showStorageMessage(
                    "Folder Already Exists",
                    "The selected parent folder already contains "
                    "a NEBULA directory.\n\n"
                    "Choose another parent folder. "
                    "Existing data will not be overwritten.");

                return;
            }

            const bool moved =
                safeThis->presetManager.moveStorageTo(destination);

            if (moved)
            {
                safeThis->showStorageMessage(
                    "Storage Updated",
                    "NEBULA user data has been copied to:\n\n"
                    + destination.getFullPathName()
                    + "\n\nThe previous data folder has been preserved.");
            }
            else
            {
                safeThis->showStorageMessage(
                    "Storage Error",
                    "NEBULA could not move the data library.\n\n"
                    "The original data folder has not been deleted. "
                    "Check folder permissions and available disk space.");
            }
        });
}
