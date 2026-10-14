cmake_minimum_required(VERSION 3.22)
project(Freeze VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")

option(GF_BUILD_PLUGIN "Build the JUCE plugin and tools" ON)

# JUCE-free DSP core
file(GLOB GF_DSP_SOURCES CONFIGURE_DEPENDS source/dsp/*.cpp)
if(GF_DSP_SOURCES)
    add_library(freeze_dsp STATIC ${GF_DSP_SOURCES})
    target_include_directories(freeze_dsp PUBLIC source)
    set_target_properties(freeze_dsp PROPERTIES POSITION_INDEPENDENT_CODE ON)
else()
    add_library(freeze_dsp INTERFACE)
    target_include_directories(freeze_dsp INTERFACE source)
endif()

# JUCE-free unit tests
enable_testing()
file(GLOB GF_TEST_SOURCES CONFIGURE_DEPENDS tests/*.cpp)
add_executable(freeze_tests ${GF_TEST_SOURCES})
target_link_libraries(freeze_tests PRIVATE freeze_dsp)
add_test(NAME freeze_tests COMMAND freeze_tests)

if(GF_BUILD_PLUGIN)
    add_subdirectory(external/JUCE)
endif()

if(GF_BUILD_PLUGIN)
    # On macOS the user VST3 folder needs no admin rights; on Windows it does, so don't copy there.
    if(APPLE)
        set(GF_COPY_AFTER_BUILD TRUE)
    else()
        set(GF_COPY_AFTER_BUILD FALSE)
    endif()

    set(GF_PLUGIN_SOURCES source/PluginProcessor.cpp source/PluginEditor.cpp)
    set(GF_JUCE_DEFINES JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)

    juce_add_plugin(Freeze
        COMPANY_NAME "fouren"
        PRODUCT_NAME "Freeze"
        BUNDLE_ID com.fouren.freeze
        PLUGIN_MANUFACTURER_CODE Fren
        PLUGIN_CODE Frz1
        FORMATS VST3 Standalone
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        VST3_CATEGORIES Fx
        COPY_PLUGIN_AFTER_BUILD ${GF_COPY_AFTER_BUILD})
    target_sources(Freeze PRIVATE ${GF_PLUGIN_SOURCES})
    target_compile_definitions(Freeze PUBLIC ${GF_JUCE_DEFINES})
    target_link_libraries(Freeze
        PRIVATE freeze_dsp juce::juce_audio_utils
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

    # Processor and editor tests (need JUCE, so they live in their own binary)
    juce_add_console_app(freeze_plugin_tests PRODUCT_NAME "freeze_plugin_tests")
    target_sources(freeze_plugin_tests PRIVATE tests/TestMain.cpp tests/plugin/test_processor.cpp ${GF_PLUGIN_SOURCES})
    target_include_directories(freeze_plugin_tests PRIVATE source)
    target_compile_definitions(freeze_plugin_tests PRIVATE ${GF_JUCE_DEFINES} JucePlugin_Name="Freeze")
    target_link_libraries(freeze_plugin_tests
        PRIVATE freeze_dsp juce::juce_audio_utils
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    add_test(NAME freeze_plugin_tests COMMAND freeze_plugin_tests)
endif()

if(GF_BUILD_PLUGIN)
    # Offline renderer: hear the effect without a DAW
    juce_add_console_app(render PRODUCT_NAME "render")
    target_sources(render PRIVATE tools/render/main.cpp)
    target_compile_definitions(render PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(render
        PRIVATE freeze_dsp juce::juce_audio_formats juce::juce_audio_basics juce::juce_core
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)

    # UI snapshot: renders the editor to PNG
    juce_add_console_app(snapshot PRODUCT_NAME "snapshot")
    target_sources(snapshot PRIVATE tools/snapshot/main.cpp ${GF_PLUGIN_SOURCES})
    target_include_directories(snapshot PRIVATE source)
    target_compile_definitions(snapshot PRIVATE ${GF_JUCE_DEFINES} JucePlugin_Name="Freeze")
    target_link_libraries(snapshot
        PRIVATE freeze_dsp juce::juce_audio_utils
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
endif()
