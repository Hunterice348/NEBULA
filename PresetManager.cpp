#include "PresetManager.h"

PresetManager::PresetManager(juce::AudioProcessorValueTreeState& a, Rack& r)
    : apvts(a), rack(r)
{
    presetDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                    .getChildFile("LoFiLabs").getChildFile("NEBULA").getChildFile("Presets");
    if (!presetDir.exists()) presetDir.createDirectory();

    buildFactoryPresets();
    for (auto& fp : factoryPresets) factoryNames.add(fp.name);
    scanUserPresets();
}

void PresetManager::buildFactoryPresets()
{
    factoryPresets = {
        { "Init", {}, {} },
        { "Deep Space Pad", {
            {"REVERB_SIZE", 0.9f}, {"REVERB_DECAY", 0.8f}, {"REVERB_MIX", 0.6f},
            {"CHORUS_RATE", 0.2f}, {"CHORUS_DEPTH", 0.6f}, {"CHORUS_MIX", 0.5f},
            {"DELAY_TIME", 0.4f}, {"DELAY_FDBK", 0.55f}, {"DELAY_MIX", 0.4f}
        }, { "reverb", "chorus", "delay" } },
        { "Dub Echo", {
            {"DELAY_TIME", 0.6f}, {"DELAY_FDBK", 0.75f}, {"DELAY_MIX", 0.7f},
            {"FILTER_CUTOFF", 0.4f}, {"FILTER_MIX", 0.8f}
        }, { "delay", "filter" } },
        // ... другие пресеты
    };
}

void PresetManager::scanUserPresets()
{
    userPresets.clear();
    for (auto& f : presetDir.findChildFiles(juce::File::findFiles, false, "*.xml"))
        userPresets.add(f.getFileNameWithoutExtension());
    userPresets.sort(true);
}

juce::File PresetManager::presetFile(const juce::String& n) const { return presetDir.getChildFile(n + ".xml"); }
juce::StringArray PresetManager::getFactoryPresetNames() const { return factoryNames; }
juce::StringArray PresetManager::getUserPresetNames() const { return userPresets; }
juce::StringArray PresetManager::getAllPresetNames() const { auto r = factoryNames; r.addArray(userPresets); return r; }
bool PresetManager::isFactoryPreset(const juce::String& n) const { return factoryNames.contains(n); }

bool PresetManager::loadPreset(const juce::String& name)
{
    if (name.isEmpty()) return false;

    // Фабричный пресет
    for (auto& fp : factoryPresets) {
        if (fp.name == name) {
            rack.clearAll();
            for (auto& type : fp.chain) rack.addEffect(type);
            for (auto& [id, val] : fp.params)
                if (auto* p = apvts.getParameter(id)) p->setValueNotifyingHost(p->convertTo0to1(val));
            currentPresetName = name;
            if (onPresetChanged) onPresetChanged();
            return true;
        }
    }

    // Пользовательский пресет
    auto file = presetFile(name);
    if (file.existsAsFile()) {
        if (auto xml = juce::XmlDocument::parse(file)) {
            auto tree = juce::ValueTree::fromXml(*xml);
            if (tree.isValid()) {
                apvts.replaceState(tree);
                if (auto rackTree = tree.getChildWithName("RACK"); rackTree.isValid())
                    rack.loadState(rackTree);
                currentPresetName = name;
                if (onPresetChanged) onPresetChanged();
                return true;
            }
        }
    }
    return false;
}

bool PresetManager::loadPresetByIndex(int index)
{
    auto names = getAllPresetNames();
    if (index >= 0 && index < names.size()) return loadPreset(names[index]);
    return false;
}

bool PresetManager::savePreset(const juce::String& name)
{
    auto n = name.trim();
    if (n.isEmpty() || n.containsAnyOf("\\/:*?\"<>|") || isFactoryPreset(n)) return false;
    auto state = apvts.copyState();
    state.appendChild(rack.saveState(), nullptr);
    auto xml = state.createXml();
    if (!xml) return false;
    if (!presetFile(n).replaceWithText(xml->toString())) return false;
    if (!userPresets.contains(n)) { userPresets.add(n); userPresets.sort(true); }
    currentPresetName = n;
    if (onPresetListChanged) onPresetListChanged();
    if (onPresetChanged) onPresetChanged();
    return true;
}

bool PresetManager::deletePreset(const juce::String& name)
{
    if (isFactoryPreset(name)) return false;
    auto f = presetFile(name);
    if (!f.existsAsFile() || !f.deleteFile()) return false;
    userPresets.removeString(name);
    if (currentPresetName == name) { currentPresetName.clear(); if (onPresetChanged) onPresetChanged(); }
    if (onPresetListChanged) onPresetListChanged();
    return true;
}

bool PresetManager::loadNextPreset() {
    auto all = getAllPresetNames(); if (all.isEmpty()) return false;
    int i = all.indexOf(currentPresetName); i = (i < 0 ? 0 : (i + 1) % all.size());
    return loadPreset(all[i]);
}
bool PresetManager::loadPreviousPreset() {
    auto all = getAllPresetNames(); if (all.isEmpty()) return false;
    int i = all.indexOf(currentPresetName); i = (i < 0 ? all.size() - 1 : (i - 1 + all.size()) % all.size());
    return loadPreset(all[i]);
}
