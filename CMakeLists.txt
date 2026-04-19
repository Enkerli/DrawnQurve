cmake_minimum_required(VERSION 3.22)
project(DrawnCurve VERSION 1.0.0)
# User-facing name is "DrawnQurve" — the cmake target remains "DrawnCurve"
# to avoid breaking all target_* references.  PRODUCT_NAME and BUNDLE_ID
# control the name visible to users and the App Store.

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

# ── JUCE ─────────────────────────────────────────────────────────────────────
# Point JUCE_PATH at your local JUCE copy, OR symlink it here:
#   ln -s /path/to/JUCE /Users/alex/Documents/ClaudeCode/DrawnQurve/JUCE
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

# ── Detect platform ──────────────────────────────────────────────────────────
set(IS_IOS FALSE)
set(IS_MACOS FALSE)

if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(IS_IOS TRUE)
    message(STATUS "Configuring for iOS")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin" OR APPLE AND NOT IOS)
    set(IS_MACOS TRUE)
    message(STATUS "Configuring for macOS")
endif()

# ── Platform-specific deployment targets ──────────────────────────────────────
if(IS_IOS)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "16.0" CACHE STRING "Minimum iOS deployment version")
elseif(IS_MACOS)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS deployment version")
endif()

# ── Plugin target ─────────────────────────────────────────────────────────────
#
# IS_MIDI_EFFECT TRUE  →  kAudioUnitType_MIDIProcessor ('aumi')
# No audio buses.  MIDI output only via midiOutputEventBlock.
# ─────────────────────────────────────────────────────────────────────────────

# Build base plugin properties (common to all platforms)
set(PLUGIN_BASE_PROPS
    COMPANY_NAME                "Enkerli"
    COMPANY_WEBSITE             "https://enkerli.com"
    BUNDLE_ID                   "com.enkerli.DrawnQurve"
    PLUGIN_MANUFACTURER_CODE    Enke
    PLUGIN_CODE                 Dqau
    PRODUCT_NAME                "DrawnQurve"
    ICON_BIG                    "${CMAKE_SOURCE_DIR}/Assets/icon_1024.png"
    DESCRIPTION                 "Drawn-qurve MIDI looper"
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           TRUE
    IS_MIDI_EFFECT              TRUE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
)

# Add platform-specific properties
if(IS_IOS)
    juce_add_plugin(DrawnCurve
        ${PLUGIN_BASE_PROPS}
        FORMATS                     AUv3 Standalone
        MICROPHONE_PERMISSION_ENABLED FALSE
        # JUCE generates UISupportedInterfaceOrientations from these properties;
        # using IPHONE_SCREEN_ORIENTATIONS / IPAD_SCREEN_ORIENTATIONS here takes
        # precedence over the JUCE default (Portrait + Landscape only) and ensures
        # PortraitUpsideDown is included.  PLIST_TO_MERGE for the same key was
        # silently ignored because JUCE writes the key after the merge step.
        IPHONE_SCREEN_ORIENTATIONS
            UIInterfaceOrientationPortrait
            UIInterfaceOrientationPortraitUpsideDown
            UIInterfaceOrientationLandscapeLeft
            UIInterfaceOrientationLandscapeRight
        IPAD_SCREEN_ORIENTATIONS
            UIInterfaceOrientationPortrait
            UIInterfaceOrientationPortraitUpsideDown
            UIInterfaceOrientationLandscapeLeft
            UIInterfaceOrientationLandscapeRight
        BACKGROUND_AUDIO_ENABLED TRUE
        PLIST_TO_MERGE "${CMAKE_SOURCE_DIR}/Assets/Info-Standalone.plist"
    )
elseif(IS_MACOS)
    juce_add_plugin(DrawnCurve
        ${PLUGIN_BASE_PROPS}
        FORMATS                     AU AUv3 Standalone
        # macOS does not use BACKGROUND_AUDIO_ENABLED or orientation properties
    )
else()
    # Fallback for other platforms (Windows, Linux, etc.)
    juce_add_plugin(DrawnCurve
        ${PLUGIN_BASE_PROPS}
        FORMATS                     Standalone
    )
endif()

# Ensure all source files are interpreted as UTF-8 (for Unicode symbols)
add_compile_options($<$<CXX_COMPILER_ID:GNU,Clang>:-finput-charset=UTF-8>)

# For MSVC, ensure /source-charset:utf-8 is used (uncomment if needed):
# add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/source-charset:utf-8>)

# ── Bundled fonts (v2 Studio typography) ──────────────────────────────────────
#
# JUCE compiles each .ttf into a C++ static array via juce_add_binary_data.
# Generated symbols: '.' is replaced with '_' but '-' is *stripped* outright.
# So the filenames below resolve to:
#   BinaryData::Domine_ttf           BinaryData::Domine_ttfSize
#   BinaryData::InterTight_ttf       BinaryData::InterTight_ttfSize
#   BinaryData::InterTightItalic_ttf BinaryData::InterTightItalic_ttfSize  (no underscore!)
#   BinaryData::Caveat_ttf           BinaryData::Caveat_ttfSize
#
# All four are variable fonts (wght axis). Domine has no italic master; the
# editor synthesises italic via juce::Font::italic where needed (matches the
# webapp, which also lacks a true Domine italic).
#
# Sandboxed access — safe inside the AUv3 XPC extension on iOS.
# ─────────────────────────────────────────────────────────────────────────────
juce_add_binary_data(DrawnCurveFonts SOURCES
    Assets/Fonts/Domine.ttf
    Assets/Fonts/InterTight.ttf
    Assets/Fonts/InterTight-Italic.ttf
    Assets/Fonts/Caveat.ttf
)
target_link_libraries(DrawnCurve PRIVATE DrawnCurveFonts)

# ── Sources ───────────────────────────────────────────────────────────────────
target_sources(DrawnCurve PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Engine/GestureEngine.cpp
    Source/Engine/GestureCaptureSession.cpp
    Source/StandaloneApp.cpp
    # Header-only — listed so Xcode shows them in the project navigator
    Source/ScaleData.h
    Source/UI/PCSPreview.h
    Source/UI/IconFactory.h
    Source/UI/ChromaticWheel.h
    Source/ScaleLattice.h
)

target_include_directories(DrawnCurve PRIVATE Source/Engine Source Source/UI)

# ── iOS code signing ──────────────────────────────────────────────────────────
# Embed signing identity so cmake regeneration preserves it.
# These settings match the provisioning profiles on the build machine.
#
# ⚠ MANUAL STEP REQUIRED AFTER EVERY `cmake -B build-ios -G Xcode` ⚠
# -----------------------------------------------------------------------------
# Even with the XCODE_ATTRIBUTE_* properties below, Xcode still needs the
# provisioning profile selected by hand after every regeneration of the
# .xcodeproj.  After every iOS cmake run you MUST open Xcode and, for BOTH
# targets (DrawnCurve_Standalone and DrawnCurve_AUv3):
#
#   1. Open build-ios/DrawnCurve.xcodeproj
#   2. Select the target → Signing & Capabilities tab
#   3. Confirm Team = "Alexandre Enkerli (P8W7XXJN6C)"
#   4. Confirm "Automatically manage signing" is checked
#   5. If Xcode shows "Failed to register bundle identifier" or a stale
#      provisioning-profile warning, click "Try Again" / let Xcode refresh
#   6. Repeat for the *other* target (Standalone embeds the AUv3 .appex,
#      so both must sign cleanly before installation works on a device)
#
# This is a known Xcode + CMake interaction — the generator writes signing
# attributes into the pbxproj but the IDE re-evaluates provisioning lazily
# the first time a target is selected after generation.  Building from the
# command line with CODE_SIGNING_ALLOWED=YES will also fail until step 1–5
# is performed once per regeneration.  Running with CODE_SIGNING_ALLOWED=NO
# (used in CI/build-only checks) sidesteps it but produces an unsigned
# binary that cannot be installed.
# -----------------------------------------------------------------------------
if(IS_IOS)
    if(TARGET DrawnCurve_AUv3)
        set_target_properties (DrawnCurve_AUv3 PROPERTIES
            XCODE_ATTRIBUTE_DEVELOPMENT_TEAM        "P8W7XXJN6C"
            XCODE_ATTRIBUTE_CODE_SIGN_STYLE         "Automatic"
            XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY      "Apple Development")
    endif()
    if(TARGET DrawnCurve_Standalone)
        set_target_properties (DrawnCurve_Standalone PROPERTIES
            XCODE_ATTRIBUTE_DEVELOPMENT_TEAM        "P8W7XXJN6C"
            XCODE_ATTRIBUTE_CODE_SIGN_STYLE         "Automatic"
            XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY      "Apple Development")
    endif()
endif()

target_compile_definitions(DrawnCurve
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        # Enable per-lane playback parameters (speed multiplier, direction, sync group).
        # Remove to compile without per-lane playback support.
        DC_HAVE_PER_LANE_PLAYBACK_PARAMS=1
        # Tell JUCE standalone not to open audio devices for MIDI-only plugins
        # This prevents the RemoteIO/CoreAudio errors on macOS
        JUCE_STANDALONE_FILTER_WINDOW_USE_KIOSK_MODE=0
)

# Platform-specific definitions
if(IS_IOS)
    target_compile_definitions(DrawnCurve PUBLIC
        # Known JUCE 8.x issue: the Standalone wrapper tries to open RemoteIO
        # with 2 audio channels even for IS_MIDI_EFFECT plugins (0 audio buses).
        # AURemoteIO returns error 561015905 and logs to console; plugin audio
        # is never used, so the error is harmless.  The AUv3 extension loads
        # correctly in real hosts (AUM, Cubasis, etc.) without this error.
        # CA Event launch-measurement failures in the same log are also benign
        # sandbox artefacts and cannot be suppressed from our code.
    )
elseif(IS_MACOS)
    target_compile_definitions(DrawnCurve PUBLIC
        # Use a custom standalone app (StandaloneApp.cpp) instead of JUCE's default
        # StandaloneFilterApp.  The custom app closes the dummy CoreAudio output that
        # JUCE opens for MIDI-effect plugins, preventing HALC_ProxyIOContext overload
        # messages and the mutex crash on quit.
        JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1
    )
endif()

target_link_libraries(DrawnCurve
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_plugin_client
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

