
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

    // Changes the active data directory and saves its location.
    bool setDataRoot(const juce::File& directory);

    // Copies all current user data to a new, empty directory.
    // The original data is deliberately preserved.
    bool moveDataTo(const juce::File& directory);

private:
    juce::File configDirectory;
    juce::File configFile;
    juce::File dataRoot;

    juce::File getDefaultDataRoot() const;
    bool saveLocation(const juce::File& directory);
    void initialise();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StorageManager)
};
