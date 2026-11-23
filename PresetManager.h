
#pragma once

#include <JuceHeader.h>

#include <functional>
#include <map>
#include <vector>

#include "Rack.h"
#include "StorageManager.h"

class PresetManager
{
public:
    PresetManager(
        juce::AudioProcessorValueTreeState& apvts,
        Rack& rack);

    // Preset lists
    juce::StringArray getFactoryPresetNames() const;
    juce::StringArray getUserPresetNames() const;
    juce::StringArray getAllPresetNames() const;

    // Preset operations
    bool loadPreset(const juce::String& name);
    bool loadPresetByIndex(int index);

    bool savePreset(const juce::String& name);
    bool deletePreset(const juce::String& name);

    bool loadNextPreset();
    bool loadPreviousPreset();

    // Current preset
    juce::String getCurrentPresetName() const
    {
        return currentPresetName;
    }

    void setCurrentPresetName(const juce::String& name)
    {
        currentPresetName = name;
    }

    bool isFactoryPreset(const juce::String& name) const;

    // Storage management
    juce::File getStorageRoot() const;

    bool moveStorageTo(
        const juce::File& directory);

    // Callbacks
    std::function<void()> onPresetChanged;
    std::function<void()> onPresetListChanged;

private:
    juce::AudioProcessorValueTreeState& apvts;
    Rack& rack;

    StorageManager storageManager;

    juce::File presetDir;

    juce::StringArray factoryNames;
    juce::StringArray userPresets;

    juce::String currentPresetName;

    void scanUserPresets();

    juce::File presetFile(
        const juce::String& name) const;

    void buildFactoryPresets();

    struct FactoryPreset
    {
        juce::String name;

        std::map<juce::String, float> params;

        std::vector<juce::String> chain;
    };

    std::vector<FactoryPreset> factoryPresets;
};
