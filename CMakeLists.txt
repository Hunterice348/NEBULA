cmake_minimum_required(VERSION 3.22)
project(NEBULA_SUITE VERSION 1.0.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.6 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

function(add_nebula TARGET CODE NAME DESC IS_SYNTH MIDI)
    juce_add_plugin(${TARGET} 
        COMPANY_NAME "Hunterice Records" 
        BUNDLE_ID "com.hunterice.nebula.${TARGET}"
        PLUGIN_MANUFACTURER_CODE Hntr 
        PLUGIN_CODE ${CODE} 
        PRODUCT_NAME "${NAME}" 
        DESCRIPTION "${DESC}"
        FORMATS VST3 Standalone 
        IS_SYNTH ${IS_SYNTH} 
        NEEDS_MIDI_INPUT ${MIDI} 
        NEEDS_MIDI_OUTPUT ${MIDI}
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE 
        COPY_PLUGIN_AFTER_BUILD TRUE)
    target_sources(${TARGET} PRIVATE Source/${TARGET}.cpp)
    target_link_libraries(${TARGET} PRIVATE 
        juce::juce_audio_utils 
        juce::juce_dsp 
        juce::juce_gui_extra
        juce::juce_recommended_config_flags 
        juce::juce_recommended_lto_flags 
        juce::juce_recommended_warning_flags)
    target_compile_definitions(${TARGET} PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
endfunction()

add_nebula(NEBULA_FX "Nfx1" "NEBULA FX" "Modular Industrial Audio Effects" FALSE FALSE)
add_nebula(NEBULA_DRUM "Ndr1" "NEBULA DRUM" "Industrial Drum Machine & Sequencer" TRUE TRUE)
add_nebula(NEBULA_SYNTH "Nsy1" "NEBULA SYNTH" "Advanced Wavetable Synthesizer" TRUE TRUE)