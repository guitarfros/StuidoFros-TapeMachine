cmake_minimum_required(VERSION 3.22)

project(StudioFrosTapeMachine VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ------------------------------------------------------------------------------------------------
# Options
# ------------------------------------------------------------------------------------------------
# Copy the freshly built VST3 into the user's VST3 folder after every build
# (Linux: ~/.vst3, macOS: ~/Library/Audio/Plug-Ins/VST3). Off by default on Windows,
# where the system VST3 folder needs administrator rights.
if(WIN32)
    set(_install_default OFF)
else()
    set(_install_default ON)
endif()
option(TAPE_INSTALL_AFTER_BUILD "Copy the VST3 to the user's VST3 folder after building" ${_install_default})
option(TAPE_INSTALL_STANDALONE  "Linux: copy the Standalone to ~/.local/bin after building" ON)
option(TAPE_BUILD_TESTS         "Build the headless DSP test" OFF)

# ------------------------------------------------------------------------------------------------
# JUCE: use ./JUCE if present (git submodule / manual copy), otherwise download it.
# Offline? Point FETCHCONTENT_SOURCE_DIR_JUCE at an existing JUCE checkout.
# ------------------------------------------------------------------------------------------------
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.4
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

if(TAPE_BUILD_TESTS)
    # Headless DSP test: only needs juce_dsp, no GUI / audio device libraries.
    juce_add_console_app(TapeEngineTest PRODUCT_NAME "TapeEngineTest")
    target_sources(TapeEngineTest PRIVATE tests/EngineTest.cpp)
    target_include_directories(TapeEngineTest PRIVATE Source)
    target_compile_definitions(TapeEngineTest PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1)
    target_link_libraries(TapeEngineTest PRIVATE
        juce::juce_dsp
        juce::juce_recommended_config_flags)
    return()
endif()

# ------------------------------------------------------------------------------------------------
# Plugin
# ------------------------------------------------------------------------------------------------
if(TAPE_INSTALL_AFTER_BUILD)
    set(_copy TRUE)
else()
    set(_copy FALSE)
endif()

juce_add_plugin(StudioFrosTapeMachine
    COMPANY_NAME                "GuitarFros"
    PRODUCT_NAME                "StudioFros - Tape Machine"
    BUNDLE_ID                   "com.guitarfros.studiofros.tapemachine"
    PLUGIN_MANUFACTURER_CODE    Gfro
    PLUGIN_CODE                 Sftm
    FORMATS                     VST3 Standalone
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES             Fx Distortion
    COPY_PLUGIN_AFTER_BUILD     ${_copy})

if(TAPE_INSTALL_AFTER_BUILD)
    if(APPLE)
        set_target_properties(StudioFrosTapeMachine PROPERTIES
            JUCE_VST3_COPY_DIR "$ENV{HOME}/Library/Audio/Plug-Ins/VST3")
    elseif(UNIX)
        set_target_properties(StudioFrosTapeMachine PROPERTIES
            JUCE_VST3_COPY_DIR "$ENV{HOME}/.vst3")
    endif()
endif()

target_sources(StudioFrosTapeMachine PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/UIComponents.h
    Source/Presets.h
    Source/Layout.h
    Source/Parameters.h
    Source/TapeEngine.h)

# Panel artwork: the background plus the sprites the plug-in animates (reels, lit buttons).
# They are generated from assets/source/panel-original.webp by tools/prepare_assets.py and embedded
# in the binary (BinaryData::background_png, ...). Positions on the artwork live in Source/Layout.h.
set(TAPE_ART_FILES
    background.png reel_left.png reel_right.png button_bypass_on.png button_power_on.png)
set(TAPE_ART "")
foreach(f ${TAPE_ART_FILES})
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/${f}")
        message(FATAL_ERROR "StudioFros: assets/${f} is missing. Run tools/prepare_assets.py (see assets/README.md).")
    endif()
    list(APPEND TAPE_ART "${CMAKE_CURRENT_SOURCE_DIR}/assets/${f}")
endforeach()
juce_add_binary_data(StudioFrosAssets SOURCES ${TAPE_ART})
set_target_properties(StudioFrosAssets PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
target_link_libraries(StudioFrosTapeMachine PRIVATE StudioFrosAssets)

target_compile_definitions(StudioFrosTapeMachine PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0      # allowed because the project is GPLv3
    JUCE_REPORT_APP_USAGE=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1)

target_link_libraries(StudioFrosTapeMachine
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_basics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Linux: also install the Standalone where a normal user can launch it from a terminal / launcher.
if(UNIX AND NOT APPLE AND TAPE_INSTALL_STANDALONE AND TAPE_INSTALL_AFTER_BUILD)
    add_custom_command(TARGET StudioFrosTapeMachine_Standalone POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$ENV{HOME}/.local/bin"
        COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:StudioFrosTapeMachine_Standalone>"
                "$ENV{HOME}/.local/bin/StudioFros-TapeMachine"
        COMMENT "Installing Standalone to ~/.local/bin/StudioFros-TapeMachine")
endif()
