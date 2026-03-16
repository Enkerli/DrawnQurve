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

    // Separate division counts for each axis (range 2-8, default 4).
    void setXDivisions (int n) { _xDivisions = juce::jlimit (2, 8, n); repaint(); }
    void setYDivisions (int n) { _yDivisions = juce::jlimit (2, 8, n); repaint(); }
    int  getXDivisions() const { return _xDivisions; }
    int  getYDivisions() const { return _yDivisions; }

private:
    DrawnCurveProcessor& proc;
    double captureStartTime { 0.0 };
    bool   isCapturing      { false };
    bool   _lightMode       { false };
    int    _xDivisions      { 4 };    // horizontal grid / X-axis tick count
    int    _yDivisions      { 4 };    // vertical grid   / Y-axis tick count
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
    // Custom LookAndFeel for direction buttons: draws filled-triangle arrows as
    // paths so no Unicode / font-coverage dependency is needed.
    // Button texts "Fwd", "Rev", "P-P" are used as identifiers; the matching
    // buttons get a drawn arrow prepended to their label text.
    struct SymbolLF : public juce::LookAndFeel_V4
    {
        void drawButtonText (juce::Graphics& g, juce::TextButton& btn,
                             bool /*highlighted*/, bool /*down*/) override
        {
            const auto b    = btn.getLocalBounds().toFloat().reduced (3.0f, 2.0f);
            const auto col  = btn.findColour (juce::TextButton::textColourOffId);
            const auto text = btn.getButtonText();

            g.setColour (col);

            const float h  = b.getHeight() * 0.50f;   // arrow height
            const float w  = h * 0.65f;               // arrow width (depth)
            const float cy = b.getCentreY();

            // Helper: filled triangle.  pointRight=true  →  ▶   (tip at tipX)
            //                           pointRight=false →  ◀   (tip at tipX)
            auto fillTri = [&] (float tipX, bool pointRight)
            {
                juce::Path p;
                if (pointRight)
                    p.addTriangle (tipX,     cy,
                                   tipX - w, cy - h / 2.0f,
                                   tipX - w, cy + h / 2.0f);
                else
                    p.addTriangle (tipX,     cy,
                                   tipX + w, cy - h / 2.0f,
                                   tipX + w, cy + h / 2.0f);
                g.fillPath (p);
            };

            g.setFont (juce::Font (12.0f));

            if (text == "Fwd")
            {
                // ▶ Fwd
                fillTri (b.getX() + w + 4.0f, true);
                g.drawFittedText ("Fwd",
                                  b.withLeft (b.getX() + w + 10.0f).toNearestInt(),
                                  juce::Justification::centredLeft, 1);
            }
            else if (text == "Rev")
            {
                // ◀ Rev
                fillTri (b.getX() + 4.0f, false);
                g.drawFittedText ("Rev",
                                  b.withLeft (b.getX() + w + 10.0f).toNearestInt(),
                                  juce::Justification::centredLeft, 1);
            }
            else if (text == "P-P")
            {
                // ◀ P-P ▶  (outward-facing arrows on each end)
                fillTri (b.getX()    + 4.0f, false);   // ◀ left
                fillTri (b.getRight() - 4.0f, true);   // ▶ right
                const auto mid = b.withLeft  (b.getX()    + w + 8.0f)
                                  .withRight (b.getRight() - w - 8.0f);
                g.drawFittedText ("P-P", mid.toNearestInt(),
                                  juce::Justification::centred, 1);
            }
            else
            {
                // All other buttons (CC, Aft, PB, Note, Y-, Y+, …): plain text.
                g.drawFittedText (text, b.toNearestInt(),
                                  juce::Justification::centred, 1);
            }
        }
    };
    SymbolLF _symbolLF;

    DrawnCurveProcessor& proc;   // named 'proc' to avoid shadowing AudioProcessorEditor::processor

    CurveDisplay curveDisplay;

    juce::TextButton playButton  { "Play"  };
    juce::TextButton clearButton { "Clear" };
    juce::TextButton themeButton { "Light" };   // toggles dark ↔ light
    juce::TextButton syncButton  { "Sync"  };   // toggles host transport + tempo sync

    // Playback-direction radio buttons: [0]=Forward  [1]=Reverse  [2]=Ping-Pong
    std::array<juce::TextButton, 3> dirBtns;

    // Separate Y/X tick [-]/[+] buttons on the right of the direction row.
    juce::TextButton tickYMinusBtn { "Y-" };
    juce::TextButton tickYPlusBtn  { "Y+" };
    juce::TextButton tickXMinusBtn { "X-" };
    juce::TextButton tickXPlusBtn  { "X+" };

    // Parameter sliders + labels
    juce::Slider ccSlider, channelSlider, smoothingSlider,
                 rangeSlider, speedSlider;   // rangeSlider = TwoValueHorizontal for min/max out
    juce::Label  ccLabel, channelLabel, smoothingLabel,
                 rangeLabel, speedLabel;

    using Attach = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<Attach> ccAttach, channelAttach, smoothingAttach,
                             speedAttach;
    // rangeSlider has no SliderAttachment — JUCE doesn't support TwoValue sliders
    // in APVTS; instead it uses onValueChange write-back + APVTS listeners.

    // Message-type radio buttons: [0]=CC  [1]=Channel Pressure  [2]=Pitch Bend  [3]=Note
    std::array<juce::TextButton, 4> msgTypeBtns;

    bool _lightMode { false };

    void setupSlider (juce::Slider& s, juce::Label& l,
                      const juce::String& labelText,
                      juce::Slider::SliderStyle style = juce::Slider::LinearHorizontal);

    // Re-colours all child components to match _lightMode.
    void applyTheme();

    // Swaps speed slider attachment and dims Play button based on sync state.
    void onSyncToggled (bool isSync);

    // Called on UI thread to highlight the active button and update the CC#/Vel slot.
    void updateMsgTypeButtons();
    void updateCCSlot();      // swaps CC# ↔ Velocity attachment depending on message type
    void updateDirButtons();
    void updateRangeSlider(); // syncs rangeSlider thumbs from APVTS (external param changes)
    void updateRangeLabel();  // refreshes rangeLabel text with current min/max values

    // AudioProcessorValueTreeState::Listener — keeps buttons in sync with
    // parameter changes that arrive from outside (automation, state restore).
    void parameterChanged (const juce::String& paramID, float newValue) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrawnCurveEditor)
};
