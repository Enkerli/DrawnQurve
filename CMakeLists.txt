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
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           TRUE
    IS_MIDI_EFFECT              TRUE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
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

# Ensure all source files are interpreted as UTF-8 (for Unicode symbols)
add_compile_options($<$<CXX_COMPILER_ID:GNU,Clang>:-finput-charset=UTF-8>)

# For MSVC, ensure /source-charset:utf-8 is used (uncomment if needed):
# add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/source-charset:utf-8>)

# ── Symbol font binary data (currently disabled) ──────────────────────────────
#
# This is the JUCE equivalent of declaring a font in iOS Info.plist (UIAppFonts).
# Uncomment when a TTF/OTF symbol font (e.g. Material Symbols, or an exported
# SF Symbol set) is available at Assets/SymbolFont.ttf.
#
# How it works:
#   1. juce_add_binary_data compiles the font file into a C++ static array.
#   2. BinaryData::SymbolFont_ttf / BinaryData::SymbolFont_ttfSize are the
#      resulting symbols (name follows the filename, with '.' → '_').
#   3. In PluginEditor.cpp (or wherever the font is needed):
#
#        static const juce::Typeface::Ptr symbolTypeface =
#            juce::Typeface::createSystemTypefaceFor (
#                BinaryData::SymbolFont_ttf,
#                BinaryData::SymbolFont_ttfSize);
#        juce::Font symbolFont (juce::FontOptions{}.withTypeface (symbolTypeface));
#        symbolFont.setHeight (20.0f);
#        g.setFont (symbolFont);
#        g.drawText (u8"\U0001F78E", bounds, juce::Justification::centred);
#
# This approach is fully sandboxed (no filesystem access) and therefore safe
# inside an AUv3 XPC extension on iOS.
#
# juce_add_binary_data(DrawnCurveAssets SOURCES
#     Assets/SymbolFont.ttf
# )
# target_link_libraries(DrawnCurve PRIVATE DrawnCurveAssets)
#
# ─────────────────────────────────────────────────────────────────────────────

# ── Sources ───────────────────────────────────────────────────────────────────
target_sources(DrawnCurve PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Engine/GestureEngine.cpp
    Source/Engine/GestureCaptureSession.cpp
    # Header-only — listed so Xcode shows them in the project navigator
    Source/ScaleData.h
    Source/UI/PCSPreview.h
    Source/UI/IconFactory.h
    Source/ScaleLattice.h
)

target_include_directories(DrawnCurve PRIVATE Source/Engine Source Source/UI)

# ── iOS code signing ──────────────────────────────────────────────────────────
# Embed signing identity so cmake regeneration preserves it.
# These settings match the provisioning profiles on the build machine.
set_target_properties (DrawnCurve_AUv3 PROPERTIES
    XCODE_ATTRIBUTE_DEVELOPMENT_TEAM        "P8W7XXJN6C"
    XCODE_ATTRIBUTE_CODE_SIGN_STYLE         "Automatic"
    XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY      "Apple Development")
set_target_properties (DrawnCurve_Standalone PROPERTIES
    XCODE_ATTRIBUTE_DEVELOPMENT_TEAM        "P8W7XXJN6C"
    XCODE_ATTRIBUTE_CODE_SIGN_STYLE         "Automatic"
    XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY      "Apple Development")

target_compile_definitions(DrawnCurve
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        # Enable per-lane playback parameters (speed multiplier, direction, sync group).
        # Remove to compile without per-lane playback support.
        DC_HAVE_PER_LANE_PLAYBACK_PARAMS=1
        # Known JUCE 8.x issue: the Standalone wrapper tries to open RemoteIO
        # with 2 audio channels even for IS_MIDI_EFFECT plugins (0 audio buses).
        # AURemoteIO returns error 561015905 and logs to console; plugin audio
        # is never used, so the error is harmless.  The AUv3 extension loads
        # correctly in real hosts (AUM, Cubasis, etc.) without this error.
        # CA Event launch-measurement failures in the same log are also benign
        # sandbox artefacts and cannot be suppressed from our code.
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

