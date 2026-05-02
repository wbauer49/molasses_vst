cmake_minimum_required(VERSION 3.21)

project(Chunker LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc")
endif()

# Paths
set(JUCE_PATH "C:/JUCE" CACHE PATH "Path to JUCE")
set(VST3_SDK_PATH "C:/JUCE/VST_SDK/vst3sdk" CACHE PATH "Path to VST3 SDK")

# Convert to forward slashes (normalize paths)
file(TO_CMAKE_PATH "${JUCE_PATH}" JUCE_PATH)
file(TO_CMAKE_PATH "${VST3_SDK_PATH}" VST3_SDK_PATH)

# Add JUCE as a subdirectory
add_subdirectory("${JUCE_PATH}" JUCE)

# Create plugin target
juce_add_plugin(Chunker
    PRODUCT_NAME "Chunker"
    VERSION "1.0.0"
    COMPANY_NAME "yourcompany"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE Manu
    PLUGIN_CODE Momb
    FORMATS Standalone
    AU_MAIN_TYPE kAudioUnitType_Effect
    AU_EXPORT_PREFIX ChunkerAU
    AU_EXPORT_PREFIX_QUOTED "ChunkerAU"
    AAX_IDENTIFIER com.yourcompany.chunker
)

# Link JUCE modules
target_link_libraries(Chunker
    PRIVATE
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_plugin_client
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_data_structures
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
)

# Source files
target_sources(Chunker
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
)

# Include directories
target_include_directories(Chunker
    PRIVATE
        Source
        JuceLibraryCode
)

# Compiler options
if(MSVC)
    target_compile_options(Chunker PRIVATE /fp:fast /W4)
else()
    target_compile_options(Chunker PRIVATE -ffast-math -Wall -Wextra)
endif()
