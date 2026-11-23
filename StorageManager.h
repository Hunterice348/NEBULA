
#pragma once

#include <JuceHeader.h>

class StorageManager
{
public:
    StorageManager();

    // Main user data directory.
    juce::File getDataRoot() const;

    // Data subdirectories.
    juce::File getPresetsDirectory() const;
    juce::File getSamplesDirectory() const;
    juce::File getDrumKitsDirectory() const;
    juce::File getPatternsDirectory() const;
    juce::File getBanksDirectory() const;
    juce::File getSettingsDirectory() const;

    // Select an existing data directory.
    // This does not move or delete any files.
    bool setDataRoot(const juce::File& directory);

    // Copy the complete NEBULA data library
    // to a new, empty destination directory.
    // The original library is preserved.
    bool moveDataTo(const juce::File& directory);

private:
    juce::File configDirectory;
    juce::File configFile;
    juce::File dataRoot;

    juce::File getDefaultDataRoot() const;

    bool saveLocation(const juce::File& directory);
    bool initialise();

    static bool copyDirectoryContents(
        const juce::File& source,
        const juce::File& destination);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StorageManager)
};
