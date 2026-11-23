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