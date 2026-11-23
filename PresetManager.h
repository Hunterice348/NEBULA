
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

    juce::StringArray getFactoryPresetNames() const;
    juce::StringArray getUserPresetNames() const;
    juce::StringArray getAllPresetNames() const;

    bool loadPreset(const juce::String& name);
    bool loadPresetByIndex(int index);

    bool savePreset(const juce::String& name);
    bool deletePreset(const juce::String& name);

    bool loadNextPreset();
    bool loadPreviousPreset();

    juce::String getCurrentPresetName() const;
    void setCurrentPresetName(const juce::String& name);

    bool isFactoryPreset(const juce::String& name) const;

    juce::File getStorageRoot() const;
    bool moveStorageTo(const juce::File& directory);

    std::function<void()> onPresetChanged;
    std::function<void()> onPresetListChanged;

private:
    struct FactoryEffect
    {
        juce::String typeID;
        bool enabled = true;
        float mix = 1.0f;
    };

    struct FactoryPreset
    {
        juce::String name;
        std::map<juce::String, float> parameters;
        std::vector<FactoryEffect> effects;
    };

    juce::AudioProcessorValueTreeState& apvts;
    Rack& rack;

    StorageManager storageManager;
    juce::File presetDir;

    juce::StringArray factoryNames;
    juce::StringArray userPresets;

    juce::String currentPresetName;

    std::vector<FactoryPreset> factoryPresets;

    void buildFactoryPresets();
    void scanUserPresets();

    juce::File presetFile(
        const juce::String& name) const;

    bool loadFactoryPreset(
        const FactoryPreset& preset);

    static bool isValidPresetName(
        const juce::String& name);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetManager)
};
