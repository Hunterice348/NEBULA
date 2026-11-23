
#include "StorageManager.h"

// ============================================================
// CONSTRUCTOR
// ============================================================

StorageManager::StorageManager()
{
    const auto appData =
        juce::File::getSpecialLocation(
            juce::File::userApplicationDataDirectory);

    // Configuration remains independent from plugin location.
    configDirectory =
        appData.getChildFile("LoFiLabs")
               .getChildFile("NEBULA_Config");

    configFile =
        configDirectory.getChildFile("storage_path.txt");

    initialise();
}

// ============================================================
// DEFAULT DATA LOCATION
// ============================================================

juce::File StorageManager::getDefaultDataRoot() const
{
    const auto appData =
        juce::File::getSpecialLocation(
            juce::File::userApplicationDataDirectory);

    // Preserve the existing NEBULA user data location.
    return appData.getChildFile("LoFiLabs")
                  .getChildFile("NEBULA");
}

// ============================================================
// INITIALISE
// ============================================================

bool StorageManager::initialise()
{
    if (configDirectory.createDirectory().failed())
        return false;

    if (configFile.existsAsFile())
    {
        const auto savedPath =
            configFile.loadFileAsString().trim();

        if (savedPath.isNotEmpty())
        {
            const juce::File savedDirectory(savedPath);

            if (savedDirectory.isDirectory())
                dataRoot = savedDirectory;
        }
    }

    if (!dataRoot.isDirectory())
        dataRoot = getDefaultDataRoot();

    if (dataRoot.createDirectory().failed())
        return false;

    // Create the standard NEBULA library structure.
    if (getPresetsDirectory().createDirectory().failed())
        return false;

    if (getSamplesDirectory().createDirectory().failed())
        return false;

    if (getDrumKitsDirectory().createDirectory().failed())
        return false;

    if (getPatternsDirectory().createDirectory().failed())
        return false;

    if (getBanksDirectory().createDirectory().failed())
        return false;

    if (getSettingsDirectory().createDirectory().failed())
        return false;

    return true;
}

// ============================================================
// DIRECTORY ACCESS
// ============================================================

juce::File StorageManager::getDataRoot() const
{
    return dataRoot;
}

juce::File StorageManager::getPresetsDirectory() const
{
    return dataRoot.getChildFile("Presets");
}

juce::File StorageManager::getSamplesDirectory() const
{
    return dataRoot.getChildFile("Samples");
}

juce::File StorageManager::getDrumKitsDirectory() const
{
    return dataRoot.getChildFile("DrumKits");
}

juce::File StorageManager::getPatternsDirectory() const
{
    return dataRoot.getChildFile("Patterns");
}

juce::File StorageManager::getBanksDirectory() const
{
    return dataRoot.getChildFile("Banks");
}

juce::File StorageManager::getSettingsDirectory() const
{
    return dataRoot.getChildFile("Settings");
}

// ============================================================
// SAVE STORAGE LOCATION
// ============================================================

bool StorageManager::saveLocation(
    const juce::File& directory)
{
    if (!directory.isDirectory())
        return false;

    if (configDirectory.createDirectory().failed())
        return false;

    // JUCE writes the file using its replacement mechanism.
    if (!configFile.replaceWithText(
            directory.getFullPathName()))
    {
        return false;
    }

    dataRoot = directory;

    return true;
}

// ============================================================
// SET DATA ROOT
// ============================================================

bool StorageManager::setDataRoot(
    const juce::File& directory)
{
    if (directory == juce::File())
        return false;

    if (!directory.isDirectory())
        return false;

    return saveLocation(directory);
}

// ============================================================
// RECURSIVE DIRECTORY COPY
// ============================================================

bool StorageManager::copyDirectoryContents(
    const juce::File& source,
    const juce::File& destination)
{
    if (!source.isDirectory())
        return false;

    if (destination.createDirectory().failed())
        return false;

    const auto children =
        source.findChildFiles(
            juce::File::findFilesAndDirectories,
            false);

    for (const auto& child : children)
    {
        const auto target =
            destination.getChildFile(child.getFileName());

        if (child.isDirectory())
        {
            if (!copyDirectoryContents(child, target))
                return false;
        }
        else
        {
            if (!child.copyFileTo(target))
                return false;
        }
    }

    return true;
}

// ============================================================
// COPY DATA TO NEW LOCATION
// ============================================================

bool StorageManager::moveDataTo(
    const juce::File& directory)
{
    if (directory == juce::File())
        return false;

    if (!dataRoot.isDirectory())
        return false;

    const auto source = dataRoot;
    const auto target = directory;

    if (source == target)
        return true;

    // Prevent copying a directory into itself
    // or into one of its descendants.
    if (source.isAChildOf(target)
        || target.isAChildOf(source))
    {
        return false;
    }

    // Do not overwrite an existing directory.
    if (target.exists())
        return false;

    const auto parent = target.getParentDirectory();

    if (!parent.isDirectory())
        return false;

    // Copy everything before changing the active location.
    if (!copyDirectoryContents(source, target))
    {
        // Remove incomplete copy only.
        target.deleteRecursively();
        return false;
    }

    // Switch to the new location only after successful copy.
    if (!saveLocation(target))
    {
        // Original data remains untouched.
        // Remove the incomplete destination copy.
        target.deleteRecursively();
        return false;
    }

    // IMPORTANT:
    // The original directory is deliberately preserved.
    return true;
}
