
cmake_minimum_required(VERSION 3.22)

project(NEBULA VERSION 0.2.0 LANGUAGES C CXX)

include(CTest)

add_subdirectory(JUCE)

# ============================================================
# NEBULA FX
# ============================================================

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
    Effects.cpp
    Effects.h
    Rack.cpp
    Rack.h
    PresetManager.cpp
    PresetManager.h
    PresetBar.cpp
    PresetBar.h
    PluginEditor.cpp
    PluginEditor.h
    NebulaLookAndFeel.cpp
    NebulaLookAndFeel.h
    ReelComponent.cpp
    ReelComponent.h
    SpectrumDisplay.cpp
    SpectrumDisplay.h
    SpectrumFrameQueue.h
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

# ============================================================
# NEBULA INSTRUMENT
# ============================================================

juce_add_plugin(NebulaInstrument
    COMPANY_NAME "LoFiLabs"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE

    PLUGIN_MANUFACTURER_CODE LfLb
    PLUGIN_CODE Nbli

    FORMATS VST3 Standalone

    PRODUCT_NAME "NEBULA INSTRUMENT"
    PLUGIN_CATEGORY "Instrument"
    VST3_CATEGORIES "Instrument" "Synth"

    VST3_AUTO_MANIFEST FALSE
)

target_sources(NebulaInstrument PRIVATE
    Instrument/InstrumentProcessor.cpp
    Instrument/InstrumentProcessor.h
    Instrument/InstrumentVoice.cpp
    Instrument/InstrumentVoice.h
)

target_compile_features(NebulaInstrument PRIVATE cxx_std_17)

juce_generate_juce_header(NebulaInstrument)

target_link_libraries(NebulaInstrument
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# ============================================================
# TESTS
# ============================================================

if (BUILD_TESTING)
    find_package(Threads REQUIRED)

    add_executable(
        SpectrumFrameQueueTests
        tests/SpectrumFrameQueueTests.cpp
    )

    target_include_directories(
        SpectrumFrameQueueTests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
    )

    target_compile_features(
        SpectrumFrameQueueTests
        PRIVATE
            cxx_std_17
    )

    target_link_libraries(
        SpectrumFrameQueueTests
        PRIVATE
            Threads::Threads
    )

    add_test(
        NAME SpectrumFrameQueueTests
        COMMAND SpectrumFrameQueueTests
    )
endif()
