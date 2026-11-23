
#include "StorageManager.h"

// ============================================================
// CONSTRUCTOR
// ============================================================

StorageManager::StorageManager()
{
    const auto appData =
        juce::File::getSpecialLocation(
            juce::File::userApplicationDataDirectory);

    configDirectory =
        appData.getChildFile("LoFiLabs")
               .getChildFile("NEBULA_Config");

    configFile =
        configDirectory.getChildFile("storage_path.txt");

    initialise();
}

// ============================================================
// DEFAULT LOCATION
// ============================================================

juce::File StorageManager::getDefaultDataRoot() const
{
    const auto appData =
        juce::File::getSpecialLocation(
            juce::File::userApplicationDataDirectory);

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

    // Persist the default path on first launch.
    if (!configFile.existsAsFile())
        return saveLocation(dataRoot);

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
// SAVE LOCATION
// ============================================================

bool StorageManager::saveLocation(
    const juce::File& directory)
{
    if (!directory.isDirectory())
        return false;

    if (configDirectory.createDirectory().failed())
        return false;

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

    if (directory == dataRoot)
        return true;

    if (directory.isAChildOf(dataRoot)
        || dataRoot.isAChildOf(directory))
    {
        return false;
    }

    if (directory.getChildFile("Presets").createDirectory().failed())
        return false;

    if (directory.getChildFile("Samples").createDirectory().failed())
        return false;

    if (directory.getChildFile("DrumKits").createDirectory().failed())
        return false;

    if (directory.getChildFile("Patterns").createDirectory().failed())
        return false;

    if (directory.getChildFile("Banks").createDirectory().failed())
        return false;

    if (directory.getChildFile("Settings").createDirectory().failed())
        return false;

    return saveLocation(directory);
}

// ============================================================
// RECURSIVE COPY
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

    // Do not copy into the source directory
    // or into one of its child directories.
    if (source.isAChildOf(target)
        || target.isAChildOf(source))
    {
        return false;
    }

    // Never overwrite an existing destination.
    if (target.exists())
        return false;

    const auto parent = target.getParentDirectory();

    if (!parent.isDirectory())
        return false;

    // Copy the library.
    if (!copyDirectoryContents(source, target))
    {
        target.deleteRecursively();
        return false;
    }

    // Change the active path only after successful copy.
    if (!saveLocation(target))
    {
        target.deleteRecursively();
        return false;
    }

    // Source data intentionally remains untouched.
    return true;
}
