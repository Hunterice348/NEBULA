cmake_minimum_required(VERSION 3.22)
project(NebulaMultiFX VERSION 0.1.0 LANGUAGES C CXX)

add_subdirectory(JUCE)

juce_add_plugin(NebulaMultiFX
    COMPANY_NAME "LoFiLabs"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_MANUFACTURER_CODE LfLb
    PLUGIN_CODE NblA
    FORMATS VST3 Standalone
    PRODUCT_NAME "NEBULA"
    PLUGIN_CATEGORY "Fx"
    VST3_CATEGORIES "Fx"
    VST3_AUTO_MANIFEST FALSE
)

target_sources(NebulaMultiFX PRIVATE
    PluginProcessor.cpp
    PluginProcessor.h
    PluginEditor.cpp
    PluginEditor.h
    Effects.cpp
    Effects.h
    Rack.cpp
    Rack.h
    PresetManager.cpp
    PresetManager.h
    PresetBar.cpp
    PresetBar.h
    NebulaLookAndFeel.cpp
    NebulaLookAndFeel.h
    ReelComponent.cpp
    ReelComponent.h
    SpectrumDisplay.cpp
    SpectrumDisplay.h
)

target_compile_features(NebulaMultiFX PRIVATE cxx_std_17)
juce_generate_juce_header(NebulaMultiFX)

target_link_libraries(NebulaMultiFX
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
