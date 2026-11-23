
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
        { "Init", {}, {} },

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
            { "reverb", "chorus", "delay" }
        },

        {
            "Dub Echo",
            {
                { "delay.p0", 0.60f },
                { "delay.p1", 0.75f },
                { "filter.p0", 0.40f },
                { "filter.p1", 0.80f }
            },
            { "delay", "filter" }
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
            { "distortion", "compressor" }
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
            { "bitcrusher", "filter", "distortion" }
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
            { "phaser", "reverb" }
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
// BUILD FACTORY RACK STATE
// ============================================================

namespace
{
    juce::ValueTree makeFactoryRackState(
        const std::vector<juce::String>& chain,
        const std::map<juce::String, float>& parameters)
    {
        juce::ValueTree rackTree("RACK");

        for (const auto& type : chain)
        {
            juce::ValueTree effectTree(type);

            effectTree.setProperty("on", true, nullptr);
            effectTree.setProperty("mix", 1.0f, nullptr);

            const juce::String prefix = type + ".p";

            for (const auto& parameter : parameters)
            {
                if (!parameter.first.startsWith(prefix))
                    continue;

                const auto indexText =
                    parameter.first.fromFirstOccurrenceOf(
                        prefix,
                        false,
                        false);

                // Reject malformed parameter suffixes.
                if (indexText.isEmpty())
                    continue;

                const int parameterIndex = indexText.getIntValue();

                if (parameterIndex < 0)
                    continue;

                if (indexText != juce::String(parameterIndex))
                    continue;

                effectTree.setProperty(
                    "p" + juce::String(parameterIndex),
                    parameter.second,
                    nullptr);
            }

            rackTree.appendChild(effectTree, nullptr);
        }

        return rackTree;
    }
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

        const auto rackState = makeFactoryRackState(
            preset.chain,
            preset.params);

        // Replace the complete chain in one Rack operation.
        rack.loadState(rackState);

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

    const auto rackTree = tree.getChildWithName("RACK");

    // Validate the rack state before changing APVTS.
    if (!rackTree.isValid())
        return false;

    apvts.replaceState(tree);
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

    if (!presetFile(cleanName).replaceWithText(xml->toString()))
        return false;

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
