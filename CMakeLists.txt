cmake_minimum_required(VERSION 3.22)

project(RidgelineDrums VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")

option(RIDGELINE_BUILD_PLUGIN "Build the VST3 plugin (needs JUCE, downloaded automatically)" ON)
option(RIDGELINE_BUILD_TESTS "Build the standalone engine test" ON)

# ---------------------------------------------------------------------------
# Peaks drum engine (MIT licence, (c) Emilie Gillet) - see THIRD_PARTY_NOTICES.md
# ---------------------------------------------------------------------------
set(PEAKS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/peaks)

add_library(peaks_engine STATIC
    ${PEAKS_DIR}/peaks/drums/bass_drum.cc
    ${PEAKS_DIR}/peaks/drums/snare_drum.cc
    ${PEAKS_DIR}/peaks/drums/high_hat.cc
    ${PEAKS_DIR}/peaks/drums/fm_drum.cc
    ${PEAKS_DIR}/peaks/resources.cc
    ${PEAKS_DIR}/stmlib/utils/random.cc)

target_include_directories(peaks_engine PUBLIC ${PEAKS_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/Source)

# TEST switches off an ARM-only memory-section attribute in the original firmware.
target_compile_definitions(peaks_engine PUBLIC TEST=1)

if(MSVC)
    # Drops GCC-style __attribute__ hints that Microsoft's compiler can't parse.
    # (Source files that include the Peaks headers get the same shim via PeaksVoice.h.)
    target_compile_options(peaks_engine PRIVATE /FI${CMAKE_CURRENT_SOURCE_DIR}/Source/msvc_compat.h /W0)
else()
    target_compile_options(peaks_engine PRIVATE -w)
endif()

# ---------------------------------------------------------------------------
# Engine test (no JUCE needed)
# ---------------------------------------------------------------------------
if(RIDGELINE_BUILD_TESTS)
    add_executable(engine_test tests/engine_test.cpp)
    target_link_libraries(engine_test PRIVATE peaks_engine)
    enable_testing()
    add_test(NAME engine_test COMMAND engine_test)
endif()

# ---------------------------------------------------------------------------
# The VST3 plugin
# ---------------------------------------------------------------------------
if(RIDGELINE_BUILD_PLUGIN)
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.16-security
        GIT_SHALLOW    ON)
    FetchContent_MakeAvailable(JUCE)

    juce_add_plugin(RidgelineDrums
        COMPANY_NAME                "Ridgeline"
        PLUGIN_MANUFACTURER_CODE    Rdgl
        PLUGIN_CODE                 Rdrm
        PRODUCT_NAME                "Ridgeline Drums"
        FORMATS                     VST3
        IS_SYNTH                    TRUE
        NEEDS_MIDI_INPUT            TRUE
        NEEDS_MIDI_OUTPUT           FALSE
        IS_MIDI_EFFECT              FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD     FALSE
        VST3_CATEGORIES             Instrument Drum)

    target_sources(RidgelineDrums PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        Source/PeaksVoice.h)

    target_compile_definitions(RidgelineDrums PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

    target_link_libraries(RidgelineDrums
        PRIVATE
            peaks_engine
            juce::juce_audio_utils
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)

    # Developer-only end-to-end check: loads the built .vst3 like a DAW, sends it
    # MIDI, and saves a screenshot of the window. Off by default.
    option(RIDGELINE_BUILD_HOST_CHECK "Build the developer host check" OFF)
    if(RIDGELINE_BUILD_HOST_CHECK)
        juce_add_console_app(HostCheck PRODUCT_NAME "HostCheck")
        target_sources(HostCheck PRIVATE
            tests/host_check.cpp
            Source/PluginProcessor.cpp
            Source/PluginEditor.cpp)
        target_compile_definitions(HostCheck PRIVATE
            JUCE_PLUGINHOST_VST3=1
            JUCE_USE_CURL=0
            JUCE_WEB_BROWSER=0
            JucePlugin_Name="Ridgeline Drums")
        target_link_libraries(HostCheck PRIVATE
            peaks_engine
            juce::juce_audio_utils
            juce::juce_recommended_config_flags)
    endif()
endif()
