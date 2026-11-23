
#include "PresetManager.h"

#include <map>
#include <vector>

// ============================================================
// CONSTRUCTOR
// ============================================================

PresetManager::PresetManager(
    juce::AudioProcessorValueTreeState& a,
    Rack& r)
    : apvts(a),
      rack(r)
{
    presetDir =
        juce::File::getSpecialLocation(
            juce::File::userApplicationDataDirectory)
            .getChildFile("LoFiLabs")
            .getChildFile("NEBULA")
            .getChildFile("Presets");

    if (!presetDir.exists())
        presetDir.createDirectory();

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
    factoryPresets =
    {
        {
            "Init",
            {},
            {}
        },

        {
            "Deep Space Pad",
            {
                { "reverb.p0", 0.90f },
                { "reverb.p1", 0.80f },
                { "reverb.p2", 0.60f },

                { "chorus.p0", 0.20f },
                { "chorus.p1", 0.60f },

                { "delay.p0", 0.40f },
                { "delay.p1", 0.55f }
            },
            {
                "reverb",
                "chorus",
                "delay"
            }
        },

        {
            "Dub Echo",
            {
                { "delay.p0", 0.60f },
                { "delay.p1", 0.75f },

                { "filter.p0", 0.40f },
                { "filter.p1", 0.80f }
            },
            {
                "delay",
                "filter"
            }
        },

        {
            "Industrial Drive",
            {
                { "distortion.p0", 0.72f },
                { "distortion.p1", 0.35f },

                { "compressor.p0", 0.60f },
                { "compressor.p1", 0.72f },
                { "compressor.p2", 0.25f }
            },
            {
                "distortion",
                "compressor"
            }
        },

        {
            "Lo-Fi Machine",
            {
                { "bitcrusher.p0", 0.35f },
                { "bitcrusher.p1", 0.70f },

                { "filter.p0", 0.45f },
                { "filter.p1", 0.35f },

                { "distortion.p0", 0.25f },
                { "distortion.p1", 0.60f }
            },
            {
                "bitcrusher",
                "filter",
                "distortion"
            }
        },

        {
            "Phased Atmosphere",
            {
                { "phaser.p0", 0.25f },
                { "phaser.p1", 0.75f },
                { "phaser.p2", 0.40f },

                { "reverb.p0", 0.70f },
                { "reverb.p1", 0.65f },
                { "reverb.p2", 0.45f }
            },
            {
                "phaser",
                "reverb"
            }
        }
    };
}


// ============================================================
// USER PRESET SCANNING
// ============================================================

void PresetManager::scanUserPresets()
{
    userPresets.clear();

    if (!presetDir.exists())
        presetDir.createDirectory();

    const auto files = presetDir.findChildFiles(
        juce::File::findFiles,
        false,
        "*.xml");

    for (const auto& file : files)
        userPresets.add(file.getFileNameWithoutExtension());

    userPresets.sort(true);
}


// ============================================================
// PRESET HELPERS
// ============================================================

juce::File PresetManager::presetFile(
    const juce::String& name) const
{
    return presetDir.getChildFile(name + ".xml");
}

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
// LOAD PRESET
// ============================================================

bool PresetManager::loadPreset(
    const juce::String& name)
{
    if (name.isEmpty())
        return false;

    // --------------------------------------------------------
    // FACTORY PRESET
    // --------------------------------------------------------

    for (const auto& preset : factoryPresets)
    {
        if (preset.name != name)
            continue;

        rack.clearAll();

        for (const auto& type : preset.chain)
        {
            if (!rack.addEffect(type))
                continue;
        }

        // Apply effect parameters using the current Rack API.
        for (int effectIndex = 0;
             effectIndex < static_cast<int>(preset.chain.size());
             ++effectIndex)
        {
            const auto& type =
                preset.chain[static_cast<size_t>(effectIndex)];

            const juce::String prefix = type + ".p";

            for (const auto& parameter : preset.params)
            {
                if (!parameter.first.startsWith(prefix))
                    continue;

                const auto parameterIndexText =
                    parameter.first.fromFirstOccurrenceOf(
                        prefix,
                        false,
                        false);

                const int parameterIndex =
                    parameterIndexText.getIntValue();

                if (parameterIndex < 0)
                    continue;

                rack.setEffectParameter(
                    effectIndex,
                    parameterIndex,
                    parameter.second);
            }
        }

        currentPresetName = name;

        if (onPresetChanged)
            onPresetChanged();

        return true;
    }

    // --------------------------------------------------------
    // USER PRESET
    // --------------------------------------------------------

    const auto file = presetFile(name);

    if (!file.existsAsFile())
        return false;

    const auto xml = juce::XmlDocument::parse(file);

    if (xml == nullptr)
        return false;

    const auto tree = juce::ValueTree::fromXml(*xml);

    if (!tree.isValid())
        return false;

    if (tree.getType() != apvts.state.getType())
        return false;

    apvts.replaceState(tree);

    const auto rackTree =
        tree.getChildWithName("RACK");

    if (rackTree.isValid())
        rack.loadState(rackTree);

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
    const auto cleanName = name.trim();

    if (cleanName.isEmpty())
        return false;

    if (cleanName.containsAnyOf("\\/:*?\"<>|"))
        return false;

    if (isFactoryPreset(cleanName))
        return false;

    auto state = apvts.copyState();

    const auto rackState = rack.saveState();

    state.appendChild(rackState, nullptr);

    const auto xml = state.createXml();

    if (xml == nullptr)
        return false;

    if (!presetFile(cleanName).replaceWithText(
            xml->toString()))
    {
        return false;
    }

    if (!userPresets.contains(cleanName))
    {
        userPresets.add(cleanName);
        userPresets.sort(true);
    }

    currentPresetName = cleanName;

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
    if (isFactoryPreset(name))
        return false;

    const auto file = presetFile(name);

    if (!file.existsAsFile())
        return false;

    if (!file.deleteFile())
        return false;

    userPresets.removeString(name);

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
// NEXT / PREVIOUS
// ============================================================

bool PresetManager::loadNextPreset()
{
    const auto names = getAllPresetNames();

    if (names.isEmpty())
        return false;

    int index = names.indexOf(currentPresetName);

    index = (index < 0)
        ? 0
        : (index + 1) % names.size();

    return loadPreset(names[index]);
}

bool PresetManager::loadPreviousPreset()
{
    const auto names = getAllPresetNames();

    if (names.isEmpty())
        return false;

    int index = names.indexOf(currentPresetName);

    index = (index < 0)
        ? names.size() - 1
        : (index - 1 + names.size()) % names.size();

    return loadPreset(names[index]);
}
