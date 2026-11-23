
#include "PresetManager.h"

namespace
{
    constexpr const char* rackStateName = "RACK";
    constexpr const char* presetExtension = ".xml";
}

// ============================================================
// CONSTRUCTOR
// ============================================================

PresetManager::PresetManager(
    juce::AudioProcessorValueTreeState& a,
    Rack& r)
    : apvts(a),
      rack(r)
{
    presetDir = storageManager.getPresetsDirectory();

    if (presetDir.createDirectory().failed())
        return;

    buildFactoryPresets();

    for (const auto& preset : factoryPresets)
        factoryNames.add(preset.name);

    scanUserPresets();
}

// ============================================================
// FACTORY PRESETS
// ============================================================

void PresetManager::buildFactoryPresets()
{
    factoryPresets.clear();

    factoryPresets.push_back({
        "Init",
        {
            { "INPUT", 0.0f },
            { "MIX", 100.0f },
            { "OUTPUT", 0.0f }
        },
        {}
    });

    factoryPresets.push_back({
        "Deep Space",
        {
            { "INPUT", 0.0f },
            { "MIX", 100.0f },
            { "OUTPUT", -1.0f }
        },
        {
            { "reverb", true, 0.65f },
            { "chorus", true, 0.45f },
            { "delay", true, 0.35f }
        }
    });

    factoryPresets.push_back({
        "Dub Echo",
        {
            { "INPUT", 0.0f },
            { "MIX", 100.0f },
            { "OUTPUT", 0.0f }
        },
        {
            { "delay", true, 0.75f },
            { "filter", true, 0.65f }
        }
    });

    factoryPresets.push_back({
        "Industrial Drive",
        {
            { "INPUT", 3.0f },
            { "MIX", 100.0f },
            { "OUTPUT", -2.0f }
        },
        {
            { "distortion", true, 0.8f },
            { "filter", true, 0.7f },
            { "compressor", true, 0.8f }
        }
    });

    factoryPresets.push_back({
        "Wide Motion",
        {
            { "INPUT", 0.0f },
            { "MIX", 100.0f },
            { "OUTPUT", 0.0f }
        },
        {
            { "chorus", true, 0.75f },
            { "phaser", true, 0.55f },
            { "reverb", true, 0.4f }
        }
    });

    factoryPresets.push_back({
        "Tight Punch",
        {
            { "INPUT", 2.0f },
            { "MIX", 100.0f },
            { "OUTPUT", 0.0f }
        },
        {
            { "compressor", true, 1.0f },
            { "filter", true, 0.35f }
        }
    });

    factoryPresets.push_back({
        "Lo-Fi Machine",
        {
            { "INPUT", 0.0f },
            { "MIX", 100.0f },
            { "OUTPUT", -1.0f }
        },
        {
            { "bitcrusher", true, 0.8f },
            { "distortion", true, 0.35f },
            { "filter", true, 0.65f }
        }
    });

    factoryPresets.push_back({
        "Dark Atmosphere",
        {
            { "INPUT", 0.0f },
            { "MIX", 100.0f },
            { "OUTPUT", -2.0f }
        },
        {
            { "reverb", true, 0.85f },
            { "phaser", true, 0.45f },
            { "filter", true, 0.7f }
        }
    });

    factoryPresets.push_back({
        "Stereo Echo",
        {
            { "INPUT", 0.0f },
            { "MIX", 100.0f },
            { "OUTPUT", 0.0f }
        },
        {
            { "delay", true, 0.7f },
            { "chorus", true, 0.6f },
            { "reverb", true, 0.3f }
        }
    });

    factoryPresets.push_back({
        "Phaser Storm",
        {
            { "INPUT", 1.0f },
            { "MIX", 100.0f },
            { "OUTPUT", -1.0f }
        },
        {
            { "phaser", true, 0.85f },
            { "distortion", true, 0.4f },
            { "delay", true, 0.35f }
        }
    });

    factoryPresets.push_back({
        "Broken Signal",
        {
            { "INPUT", 2.0f },
            { "MIX", 100.0f },
            { "OUTPUT", -3.0f }
        },
        {
            { "bitcrusher", true, 0.9f },
            { "distortion", true, 0.7f },
            { "phaser", true, 0.4f }
        }
    });

    factoryPresets.push_back({
        "Cinematic Impact",
        {
            { "INPUT", 0.0f },
            { "MIX", 100.0f },
            { "OUTPUT", 0.0f }
        },
        {
            { "compressor", true, 0.9f },
            { "distortion", true, 0.35f },
            { "reverb", true, 0.75f }
        }
    });
}

// ============================================================
// NAME VALIDATION
// ============================================================

bool PresetManager::isValidPresetName(
    const juce::String& name)
{
    const auto safeName = name.trim();

    if (safeName.isEmpty() || safeName.length() > 80)
        return false;

    if (safeName.containsAnyOf("\\/:*?\"<>|"))
        return false;

    if (safeName == "." || safeName == "..")
        return false;

    return true;
}

// ============================================================
// PRESET FILE
// ============================================================

juce::File PresetManager::presetFile(
    const juce::String& name) const
{
    if (!isValidPresetName(name))
        return {};

    return presetDir.getChildFile(
        name.trim() + presetExtension);
}

// ============================================================
// SCAN USER PRESETS
// ============================================================

void PresetManager::scanUserPresets()
{
    userPresets.clear();

    if (!presetDir.isDirectory())
        return;

    const auto files = presetDir.findChildFiles(
        juce::File::findFiles,
        false,
        "*.xml");

    for (const auto& file : files)
    {
        const auto name = file.getFileNameWithoutExtension();

        if (isValidPresetName(name)
            && !factoryNames.contains(name))
        {
            userPresets.add(name);
        }
    }

    userPresets.sort(true);
}

// ============================================================
// PRESET LISTS
// ============================================================

juce::StringArray PresetManager::getFactoryPresetNames() const
{
    return factoryNames;
}

juce::StringArray PresetManager::getUserPresetNames() const
{
    return userPresets;
}

juce::StringArray PresetManager::getAllPresetNames() const
{
    auto result = factoryNames;
    result.addArray(userPresets);
    return result;
}

bool PresetManager::isFactoryPreset(
    const juce::String& name) const
{
    return factoryNames.contains(name);
}

// ============================================================
// LOAD FACTORY PRESET
// ============================================================

bool PresetManager::loadFactoryPreset(
    const FactoryPreset& preset)
{
    const juce::StringArray parameterIDs {
        "INPUT",
        "MIX",
        "OUTPUT"
    };

    // Restore global parameter defaults.
    for (const auto& id : parameterIDs)
    {
        if (auto* parameter = apvts.getParameter(id))
        {
            parameter->setValueNotifyingHost(
                parameter->getDefaultValue());
        }
    }

    // Apply preset parameter values.
    for (const auto& item : preset.parameters)
    {
        if (auto* parameter = apvts.getParameter(item.first))
        {
            const float normalized =
                parameter->convertTo0to1(item.second);

            parameter->setValueNotifyingHost(
                juce::jlimit(0.0f, 1.0f, normalized));
        }
    }

    // Construct Rack state.
    juce::ValueTree rackState(rackStateName);

    for (const auto& spec : preset.effects)
    {
        juce::ValueTree effect(spec.typeID);

        effect.setProperty("on", spec.enabled, nullptr);
        effect.setProperty("mix", spec.mix, nullptr);

        rackState.appendChild(effect, nullptr);
    }

    rack.loadState(rackState);

    currentPresetName = preset.name;

    if (onPresetChanged)
        onPresetChanged();

    return true;
}

// ============================================================
// LOAD PRESET
// ============================================================

bool PresetManager::loadPreset(
    const juce::String& name)
{
    if (name.isEmpty())
        return false;

    // Search factory presets.
    for (const auto& preset : factoryPresets)
    {
        if (preset.name == name)
            return loadFactoryPreset(preset);
    }

    // Load user preset.
    const auto file = presetFile(name);

    if (!file.existsAsFile())
        return false;

    const auto xml = juce::XmlDocument::parse(file);

    if (xml == nullptr)
        return false;

    auto loadedState = juce::ValueTree::fromXml(*xml);

    if (!loadedState.isValid())
        return false;

    if (!loadedState.hasType(apvts.state.getType()))
        return false;

    const auto rackState =
        loadedState.getChildWithName(rackStateName);

    auto parameterState = loadedState.createCopy();

    const auto rackChild =
        parameterState.getChildWithName(rackStateName);

    if (rackChild.isValid())
        parameterState.removeChild(rackChild, nullptr);

    apvts.replaceState(parameterState);

    if (rackState.isValid())
        rack.loadState(rackState);
    else
        rack.clearAll();

    currentPresetName = name;

    if (onPresetChanged)
        onPresetChanged();

    return true;
}

// ============================================================
// LOAD BY INDEX
// ============================================================

bool PresetManager::loadPresetByIndex(int index)
{
    const auto names = getAllPresetNames();

    if (index < 0 || index >= names.size())
        return false;

    return loadPreset(names[index]);
}

// ============================================================
// SAVE PRESET
// ============================================================

bool PresetManager::savePreset(
    const juce::String& name)
{
    const auto safeName = name.trim();

    if (!isValidPresetName(safeName))
        return false;

    if (isFactoryPreset(safeName))
        return false;

    if (presetDir.createDirectory().failed())
        return false;

    auto state = apvts.copyState();

    const auto previousRack =
        state.getChildWithName(rackStateName);

    if (previousRack.isValid())
        state.removeChild(previousRack, nullptr);

    state.appendChild(rack.saveState(), nullptr);

    state.setProperty(
        "presetName",
        safeName,
        nullptr);

    auto xml = state.createXml();

    if (xml == nullptr)
        return false;

    const auto file = presetFile(safeName);

    if (file == juce::File())
        return false;

    if (!file.replaceWithText(xml->toString()))
        return false;

    scanUserPresets();

    currentPresetName = safeName;

    if (onPresetListChanged)
        onPresetListChanged();

    if (onPresetChanged)
        onPresetChanged();

    return true;
}

// ============================================================
// DELETE PRESET
// ============================================================

bool PresetManager::deletePreset(
    const juce::String& name)
{
    if (name.isEmpty() || isFactoryPreset(name))
        return false;

    const auto file = presetFile(name);

    if (!file.existsAsFile())
        return false;

    if (!file.deleteFile())
        return false;

    scanUserPresets();

    if (currentPresetName == name)
    {
        currentPresetName.clear();

        if (onPresetChanged)
            onPresetChanged();
    }

    if (onPresetListChanged)
        onPresetListChanged();

    return true;
}

// ============================================================
// NEXT PRESET
// ============================================================

bool PresetManager::loadNextPreset()
{
    const auto names = getAllPresetNames();

    if (names.isEmpty())
        return false;

    const int currentIndex =
        names.indexOf(currentPresetName);

    const int nextIndex =
        currentIndex < 0
            ? 0
            : (currentIndex + 1) % names.size();

    return loadPreset(names[nextIndex]);
}

// ============================================================
// PREVIOUS PRESET
// ============================================================

bool PresetManager::loadPreviousPreset()
{
    const auto names = getAllPresetNames();

    if (names.isEmpty())
        return false;

    const int currentIndex =
        names.indexOf(currentPresetName);

    const int previousIndex =
        currentIndex < 0
            ? names.size() - 1
            : (currentIndex - 1 + names.size()) % names.size();

    return loadPreset(names[previousIndex]);
}

// ============================================================
// CURRENT PRESET
// ============================================================

juce::String PresetManager::getCurrentPresetName() const
{
    return currentPresetName;
}

void PresetManager::setCurrentPresetName(
    const juce::String& name)
{
    currentPresetName = name;

    if (onPresetChanged)
        onPresetChanged();
}

// ============================================================
// STORAGE
// ============================================================

juce::File PresetManager::getStorageRoot() const
{
    return storageManager.getDataRoot();
}

bool PresetManager::moveStorageTo(
    const juce::File& directory)
{
    if (!storageManager.moveDataTo(directory))
        return false;

    presetDir = storageManager.getPresetsDirectory();

    scanUserPresets();

    if (onPresetListChanged)
        onPresetListChanged();

    return true;
}
