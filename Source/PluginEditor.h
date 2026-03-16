#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

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

private:
    DrawnCurveProcessor& proc;
    double captureStartTime { 0.0 };
    bool   isCapturing      { false };
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

    // Parameter sliders + labels
    juce::Slider ccSlider, channelSlider, smoothingSlider, minOutSlider, maxOutSlider;
    juce::Label  ccLabel, channelLabel, smoothingLabel, minOutLabel, maxOutLabel;

    using Attach = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<Attach> ccAttach, channelAttach, smoothingAttach,
                             minAttach, maxAttach;

    // Message-type radio buttons: [0]=CC  [1]=Channel Pressure  [2]=Pitch Bend
    std::array<juce::TextButton, 3> msgTypeBtns;

    void setupSlider (juce::Slider& s, juce::Label& l,
                      const juce::String& labelText,
                      juce::Slider::SliderStyle style = juce::Slider::LinearHorizontal);

    // Called on UI thread to highlight the active button and dim CC# if needed.
    void updateMsgTypeButtons();
    void updateCCVisibility();

    // AudioProcessorValueTreeState::Listener — keeps buttons in sync with
    // parameter changes that arrive from outside (automation, state restore).
    void parameterChanged (const juce::String& paramID, float newValue) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrawnCurveEditor)
};
