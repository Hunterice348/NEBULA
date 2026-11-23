
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
