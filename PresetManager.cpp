
// ============================================================
// STORAGE MANAGEMENT
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

PresetManager::PresetManager(
    juce::AudioProcessorValueTreeState& a,
    Rack& r)
    : apvts(a),
      rack(r)
{
    presetDir = storageManager.getPresetsDirectory();

    if (!presetDir.exists())
        presetDir.createDirectory();

    buildFactoryPresets();

    for (const auto& preset : factoryPresets)
        factoryNames.add(preset.name);

    scanUserPresets();
}
