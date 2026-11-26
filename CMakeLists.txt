cmake_minimum_required(VERSION 3.22)
project(Kuzko VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(Kuzko
    COMPANY_NAME "Kuzko"
    PLUGIN_MANUFACTURER_CODE Kuzk
    PLUGIN_CODE Kz01
    FORMATS VST3 Standalone
    PRODUCT_NAME "Kuzko EQ"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx EQ)

target_sources(Kuzko PRIVATE
    PluginProcessor.cpp
    PluginEditor.cpp)
# Fond optionnel : mets une image dans Assets/ nommee background.jpg ou background.png
set(KUZKO_BG "")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Assets/background.jpg")
    set(KUZKO_BG "${CMAKE_CURRENT_SOURCE_DIR}/Assets/background.jpg")
    set(KUZKO_BG_ID "background_jpg")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Assets/background.png")
    set(KUZKO_BG "${CMAKE_CURRENT_SOURCE_DIR}/Assets/background.png")
    set(KUZKO_BG_ID "background_png")
endif()

if(KUZKO_BG)
    juce_add_binary_data(KuzkoAssets SOURCES "${KUZKO_BG}")
    target_link_libraries(Kuzko PRIVATE KuzkoAssets)
    target_compile_definitions(Kuzko PRIVATE
        KUZKO_HAS_BG=1
        KUZKO_BG_DATA=BinaryData::${KUZKO_BG_ID}
        KUZKO_BG_SIZE=BinaryData::${KUZKO_BG_ID}Size)
else()
    target_compile_definitions(Kuzko PRIVATE KUZKO_HAS_BG=0)
endif()

target_compile_definitions(Kuzko PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Kuzko
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
