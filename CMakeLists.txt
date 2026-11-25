cmake_minimum_required(VERSION 3.22)
project(NEBULA_SUITE VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

# --- JUCE Dependency (Pinned 8.0.6) ---
set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

# --- SHARED CORE LIBRARY ---
add_library(NEBULA_Core STATIC
    Source/Core/StateManager.cpp
    Source/Core/DSP/Compressor.cpp
)
target_include_directories(NEBULA_Core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Source)
target_link_libraries(NEBULA_Core PUBLIC
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_extra
)
target_compile_definitions(NEBULA_Core PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)

# --- ФУНКЦИЯ ДЛЯ СОЗДАНИЯ ПЛАГИНОВ ---
function(add_nebula_plugin TARGET_NAME PLUGIN_CODE PRODUCT_NAME DESCRIPTION IS_SYNTH NEEDS_MIDI)
    juce_add_plugin(${TARGET_NAME}
        COMPANY_NAME "Hunterice Records"
        BUNDLE_ID "com.hunterice.nebula.${TARGET_NAME}"
        PLUGIN_MANUFACTURER_CODE Hntr
        PLUGIN_CODE ${PLUGIN_CODE}
        PRODUCT_NAME "${PRODUCT_NAME}"
        DESCRIPTION "${DESCRIPTION}"
        FORMATS VST3 Standalone
        IS_SYNTH ${IS_SYNTH}
        NEEDS_MIDI_INPUT ${NEEDS_MIDI}
        NEEDS_MIDI_OUTPUT ${NEEDS_MIDI}
        EDITOR_WANTS_KEYBOARD_FOCUS TRUE
        COPY_PLUGIN_AFTER_BUILD TRUE
    )

    target_sources(${TARGET_NAME} PRIVATE
        Source/${TARGET_NAME}/PluginProcessor.cpp
        Source/${TARGET_NAME}/PluginProcessor.h
        Source/${TARGET_NAME}/PluginEditor.cpp
        Source/${TARGET_NAME}/PluginEditor.h
    )

    target_link_libraries(${TARGET_NAME} PRIVATE
        NEBULA_Core
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
    )
endfunction()

# --- 1. NEBULA FX ---
add_nebula_plugin(NEBULA_FX "Nfx1" "NEBULA FX" "Modular Industrial Audio Effects" FALSE FALSE)

# --- 2. NEBULA DRUM (Заглушка для полной структуры, добавляется аналогично) ---
# add_nebula_plugin(NEBULA_DRUM "Ndr1" "NEBULA DRUM" "Industrial Drum Machine" TRUE TRUE)

# --- 3. NEBULA SYNTH (Заглушка для полной структуры, добавляется аналогично) ---
# add_nebula_plugin(NEBULA_SYNTH "Nsy1" "NEBULA SYNTH" "Advanced Wavetable Synthesizer" TRUE TRUE)
