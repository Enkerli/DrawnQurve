cmake_minimum_required(VERSION 3.22)
project(DrawnCurve VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

# ── JUCE ─────────────────────────────────────────────────────────────────────
# Point JUCE_PATH at your local JUCE copy, OR symlink it here:
#   ln -s /path/to/JUCE /Users/alex/Documents/ClaudeCode/DrawnCurveJUCE/JUCE
#
# To configure for iOS from the command line:
#   cmake -B build-ios -G Xcode \
#     -DCMAKE_SYSTEM_NAME=iOS \
#     -DCMAKE_OSX_DEPLOYMENT_TARGET=16.0 \
#     -DJUCE_PATH=/path/to/JUCE
#   cmake --open build-ios
#   (then set signing in Xcode → DrawnCurve_Standalone target)
# ─────────────────────────────────────────────────────────────────────────────

find_package(JUCE CONFIG QUIET)
if(NOT JUCE_FOUND)
    set(JUCE_PATH "${CMAKE_SOURCE_DIR}/JUCE" CACHE PATH "Path to JUCE directory")
    if(NOT EXISTS "${JUCE_PATH}/CMakeLists.txt")
        message(FATAL_ERROR
            "JUCE not found.\n"
            "Either:\n"
            "  (a) Set -DJUCE_PATH=/path/to/JUCE on the cmake command line, or\n"
            "  (b) Symlink JUCE into this directory:\n"
            "      ln -s /path/to/JUCE ${CMAKE_SOURCE_DIR}/JUCE\n")
    endif()
    add_subdirectory("${JUCE_PATH}" JUCE)
endif()

# ── Plugin target ─────────────────────────────────────────────────────────────
#
# IS_MIDI_EFFECT TRUE  →  kAudioUnitType_MIDIProcessor ('aumi')
# No audio buses.  MIDI output only via midiOutputEventBlock.
# ─────────────────────────────────────────────────────────────────────────────
juce_add_plugin(DrawnCurve
    COMPANY_NAME                "Enkerli"
    COMPANY_WEBSITE             "https://enkerli.com"
    BUNDLE_ID                   "com.enkerli.DrawnCurve"
    PLUGIN_MANUFACTURER_CODE    Enke
    PLUGIN_CODE                 Dcau
    FORMATS                     AUv3 Standalone
    PRODUCT_NAME                "DrawnCurve"
    ICON_BIG                    "${CMAKE_SOURCE_DIR}/Assets/icon_1024.png"
    DESCRIPTION                 "Drawn-curve MIDI CC looper"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           TRUE
    IS_MIDI_EFFECT              TRUE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    MICROPHONE_PERMISSION_ENABLED FALSE
)

# ── Sources ───────────────────────────────────────────────────────────────────
target_sources(DrawnCurve PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Engine/GestureEngine.cpp
    Source/Engine/GestureCaptureSession.cpp
)

target_include_directories(DrawnCurve PRIVATE Source/Engine)

target_compile_definitions(DrawnCurve
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        # Note: the Standalone app crashes on iOS because JUCE tries to open
        # RemoteIO with 0 audio channels (JUCE 8 bug for MIDI effects).
        # This is harmless — iOS registers the AUv3 extension from the bundle
        # at install time, before the Standalone ever runs.  Ignore the crash.
)

target_link_libraries(DrawnCurve
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_plugin_client
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
