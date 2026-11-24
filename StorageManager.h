
#pragma once

#include <JuceHeader.h>

class StorageManager
{
public:
    StorageManager();

    juce::File getDataRoot() const;

    juce::File getPresetsDirectory() const;
    juce::File getSamplesDirectory() const;
    juce::File getDrumKitsDirectory() const;
    juce::File getPatternsDirectory() const;
    juce::File getBanksDirectory() const;
    juce::File getSettingsDirectory() const;

    bool setDataRoot(const juce::File& directory);

    // Copies the complete data library to a new directory.
    // The source directory is preserved.
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
