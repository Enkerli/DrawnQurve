#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

// Colour palette struct — defined in PluginEditor.cpp.
// Forward-declared here so CurveDisplay can hold a pointer without
// pulling in the full definition.
struct Theme;

//==============================================================================
/// Displays the recorded curve and handles gesture capture via touch / mouse.
class CurveDisplay : public juce::Component,
                     public juce::Timer
{
public:
    explicit CurveDisplay (DrawnCurveProcessor& p);
    ~CurveDisplay() override;

    void paint    (juce::Graphics&)           override;
    void resized  ()                          override;
    void mouseDown (const juce::MouseEvent&)  override;
    void mouseDrag (const juce::MouseEvent&)  override;
    void mouseUp   (const juce::MouseEvent&)  override;
    void timerCallback()                      override;

    void setLightMode (bool light);

private:
    DrawnCurveProcessor& proc;
    double captureStartTime { 0.0 };
    bool   isCapturing      { false };
    bool   _lightMode       { false };
    juce::Path capturePath;   // live visual trail while drawing

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CurveDisplay)
};

//==============================================================================
// ComboBox popups don't work inside an AUv3 XPC process on iOS (no TopLevelWindow).
// Instead we use three TextButtons as a radio group for the message type.
class DrawnCurveEditor : public juce::AudioProcessorEditor,
                         private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit DrawnCurveEditor (DrawnCurveProcessor&);
    ~DrawnCurveEditor() override;

    void paint   (juce::Graphics&) override;
    void resized ()                override;

private:
    DrawnCurveProcessor& proc;   // named 'proc' to avoid shadowing AudioProcessorEditor::processor

    CurveDisplay curveDisplay;

    juce::TextButton playButton  { "Play"  };
    juce::TextButton clearButton { "Clear" };
    juce::TextButton themeButton { "Light" };   // toggles dark ↔ light
    juce::TextButton syncButton  { "Sync"  };   // toggles host transport + tempo sync

    // Playback-direction radio buttons: [0]=Forward  [1]=Reverse  [2]=Ping-Pong
    std::array<juce::TextButton, 3> dirBtns;

    // Parameter sliders + labels
    juce::Slider ccSlider, channelSlider, smoothingSlider,
                 minOutSlider, maxOutSlider, speedSlider;
    juce::Label  ccLabel, channelLabel, smoothingLabel,
                 minOutLabel, maxOutLabel, speedLabel;

    using Attach = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<Attach> ccAttach, channelAttach, smoothingAttach,
                             minAttach, maxAttach, speedAttach;

    // Message-type radio buttons: [0]=CC  [1]=Channel Pressure  [2]=Pitch Bend
    std::array<juce::TextButton, 3> msgTypeBtns;

    bool _lightMode { false };

    void setupSlider (juce::Slider& s, juce::Label& l,
                      const juce::String& labelText,
                      juce::Slider::SliderStyle style = juce::Slider::LinearHorizontal);

    // Re-colours all child components to match _lightMode.
    void applyTheme();

    // Swaps speed slider attachment and dims Play button based on sync state.
    void onSyncToggled (bool isSync);

    // Called on UI thread to highlight the active button and dim CC# if needed.
    void updateMsgTypeButtons();
    void updateCCVisibility();
    void updateDirButtons();

    // AudioProcessorValueTreeState::Listener — keeps buttons in sync with
    // parameter changes that arrive from outside (automation, state restore).
    void parameterChanged (const juce::String& paramID, float newValue) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrawnCurveEditor)
};
