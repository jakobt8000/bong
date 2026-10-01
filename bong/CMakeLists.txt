cmake_minimum_required(VERSION 3.22)

project(bong VERSION 2.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
endif()

# JUCE: use a local checkout if JUCE_DIR is given, otherwise download it
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.9
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

if(APPLE)
    set(BONG_FORMATS AU VST3)
else()
    set(BONG_FORMATS VST3)
endif()

juce_add_plugin(Bong
    COMPANY_NAME "Jakob2ft"
    PLUGIN_MANUFACTURER_CODE Jk2f
    PLUGIN_CODE Bong
    FORMATS ${BONG_FORMATS}
    PRODUCT_NAME "bong"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Delay Reverb
    AU_MAIN_TYPE kAudioUnitType_Effect
    BUNDLE_ID dk.jakob2ft.bong)

juce_add_binary_data(BongFonts SOURCES
    Fonts/Silkscreen-Regular.ttf
    Fonts/Silkscreen-Bold.ttf)

target_sources(Bong PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(Bong PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Bong
    PRIVATE
        BongFonts
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Optional: headless GUI snapshot tool used during development (cmake -DBONG_TESTS=ON)
if(BONG_TESTS)
    juce_add_console_app(BongSnapshot PRODUCT_NAME "BongSnapshot")
    target_sources(BongSnapshot PRIVATE tests/snapshot.cpp)
    target_compile_definitions(BongSnapshot PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(BongSnapshot PRIVATE Bong BongFonts juce::juce_gui_extra)
endif()
