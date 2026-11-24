
cmake_minimum_required(VERSION 3.22)

project(NEBULA VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)
include(FetchContent)

set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(JUCE)

juce_add_plugin(NEBULA_FX
    COMPANY_NAME "Hunterice Records"
    BUNDLE_ID "com.hunterice.nebula.fx"
    PLUGIN_MANUFACTURER_CODE Hntr
    PLUGIN_CODE Nfx1
    PRODUCT_NAME "NEBULA FX"
    DESCRIPTION "Modular Industrial Audio Effects"

    FORMATS VST3 Standalone

    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE

    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

target_sources(NEBULA_FX PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h

    Source/Core/ParameterSmoothing.cpp
    Source/Core/ParameterSmoothing.h

    Source/Core/AudioMetering.cpp
    Source/Core/AudioMetering.h
)

target_compile_definitions(NEBULA_FX PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(NEBULA_FX PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

if(BUILD_TESTING)
    add_executable(NEBULA_CoreTests
        Tests/DSPTests.cpp
    )

    target_include_directories(NEBULA_CoreTests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Source
    )

    add_test(
        NAME NEBULA_CoreTests
        COMMAND NEBULA_CoreTests
    )
endif()
