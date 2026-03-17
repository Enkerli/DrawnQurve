#pragma once

/**
 * @file PluginEditor.h
 *
 * DrawnCurveEditor — JUCE AudioProcessorEditor for DrawnCurve AUv3.
 *
 * UI Layout (640 x 560 px)
 * ────────────────────────
 *   Row 1  (40 px)  : Play · Clear | CC Aft PB Note | Sync · ? · Dark/Light
 *   Row 2  (34 px)  : [Fwd][Rev][P-P]               [Y-][Y+]  [X-][X+]
 *   Param 1 (44 px) : CC# / Vel  |  Channel  |  Smooth
 *   Param 2 (44 px) : Range (min/max out, 2/3 width)  |  Speed / Beats (1/3)
 *   Curve display   : remainder — live grid + recorded curve + playhead
 *
 * Key design decisions
 * ────────────────────
 * - Range slider uses JUCE TwoValueHorizontal with manual APVTS write-back
 *   because JUCE SliderAttachment does not support two-value sliders.
 * - The direction control is a SegmentedControl (3-way switch: ◀ ◆ ▶).
 *   Visual order is Rev / Ping-Pong / Fwd (left to right); Forward is the
 *   default (rightmost, APVTS param value 0).  A SegmentPainter draws filled
 *   triangles as juce::Path objects — no font dependency, AUv3 sandbox safe.
 * - Message-type buttons (CC/Aft/PB/Note) use a minimal SymbolLF subclass
 *   that renders plain centred text at a fixed 12 pt size.
 * - The Speed slider attachment is swapped at runtime between "playbackSpeed"
 *   and "syncBeats" when sync mode is toggled, so no extra slider is needed.
 * - The "?" button opens a full-editor help overlay (HelpOverlay) since
 *   JUCE tooltips do not function inside an AUv3 XPC process on iOS.
 */

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "SegmentedControl.h"

// Colour palette struct — defined in PluginEditor.cpp.
// Forward-declared here so CurveDisplay can hold a pointer without
// pulling in the full definition.
struct Theme;

//==============================================================================
/**
 * Interactive display of the recorded curve, playhead, grid, and axis labels.
 *
 * Also handles touch / mouse gesture capture: mouseDown starts recording,
 * mouseDrag appends points, mouseUp finalises the snapshot.
 *
 * A 30 Hz timer drives continuous repaints so the playhead and axis labels
 * stay in sync with live parameter changes even when the user is not touching
 * the screen.
 */
class CurveDisplay : public juce::Component,
                     public juce::Timer
{
public:
    explicit CurveDisplay (DrawnCurveProcessor& p);
    ~CurveDisplay() override;

    void paint     (juce::Graphics&)          override;
    void resized   ()                         override;
    void mouseDown (const juce::MouseEvent&)  override;
    void mouseDrag (const juce::MouseEvent&)  override;
    void mouseUp   (const juce::MouseEvent&)  override;
    void timerCallback()                      override;

    /// Switch between dark (default) and light colour themes.
    void setLightMode (bool light);

    /// Number of vertical grid divisions (X axis).  Range 2-8, default 4.
    void setXDivisions (int n) { _xDivisions = juce::jlimit (2, 8, n); repaint(); }
    /// Number of horizontal grid divisions (Y axis).  Range 2-8, default 4.
    void setYDivisions (int n) { _yDivisions = juce::jlimit (2, 8, n); repaint(); }
    int  getXDivisions() const { return _xDivisions; }
    int  getYDivisions() const { return _yDivisions; }

private:
    DrawnCurveProcessor& proc;
    double     captureStartTime { 0.0   };   ///< High-res timestamp of mouseDown
    bool       isCapturing      { false };   ///< True while finger/mouse is down
    bool       _lightMode       { false };
    int        _xDivisions      { 4 };       ///< Vertical grid line count
    int        _yDivisions      { 4 };       ///< Horizontal grid line count
    juce::Path capturePath;                  ///< Live visual trail drawn in screen coordinates

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CurveDisplay)
};

//==============================================================================
/**
 * Full-editor semi-transparent help overlay.
 *
 * Shown when the user taps "?".  Draws a quick-reference guide over all
 * other controls.  Tapping anywhere dismisses it.
 *
 * JUCE tooltips do not work inside an AUv3 XPC sandbox process on iOS
 * (they require a TopLevelWindow), so this overlay is the chosen alternative.
 */
class HelpOverlay : public juce::Component
{
public:
    HelpOverlay();

    void paint     (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;   ///< Any tap dismisses the overlay

    /// true = dark background / light text (default), false = lighter tint
    void setLightMode (bool light) { _lightMode = light; repaint(); }

private:
    bool _lightMode { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HelpOverlay)
};

//==============================================================================
/**
 * Main plugin editor window.
 *
 * ComboBox popups don't work inside an AUv3 XPC process on iOS (no
 * TopLevelWindow), so every multi-option parameter uses TextButton radio
 * groups instead.
 */
class DrawnCurveEditor : public juce::AudioProcessorEditor,
                         private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit DrawnCurveEditor (DrawnCurveProcessor&);
    ~DrawnCurveEditor() override;

    void paint   (juce::Graphics&) override;
    void resized ()                override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    // ── Custom LookAndFeel for message-type buttons ───────────────────────────
    //
    // Previously also handled direction buttons ("Fwd"/"Rev"/"P-P"), which have
    // been replaced by a SegmentedControl with a custom SegmentPainter that draws
    // proper stem+arrowhead arrows without any font dependency.
    //
    // Retained here for msgTypeBtns ("CC", "Aft", "PB", "Note").  The override
    // draws plain centred text for all unrecognised labels.  The reduced() call
    // gives a small inset margin so text doesn't touch the button edge.
    struct SymbolLF : public juce::LookAndFeel_V4
    {
        void drawButtonText (juce::Graphics& g, juce::TextButton& btn,
                             bool /*highlighted*/, bool /*down*/) override
        {
            const auto b    = btn.getLocalBounds().toFloat().reduced (3.0f, 2.0f);
            const auto col  = btn.findColour (juce::TextButton::textColourOffId);
            const auto text = btn.getButtonText();

            g.setColour (col);
            g.setFont (juce::Font (12.0f));
            g.drawFittedText (text, b.toNearestInt(),
                              juce::Justification::centred, 1);
        }
    };
    SymbolLF _symbolLF;   ///< Used by msgTypeBtns (CC / Aft / PB / Note)

    // ── Core references ───────────────────────────────────────────────────────
    DrawnCurveProcessor& proc;   ///< Named 'proc' to avoid shadowing AudioProcessorEditor::processor
    CurveDisplay curveDisplay;
    HelpOverlay  helpOverlay;    ///< Sits above all other children; hidden by default

    // ── Row 1 utility buttons ─────────────────────────────────────────────────
    juce::TextButton playButton  { "Play"  };
    juce::TextButton clearButton { "Clear" };
    juce::TextButton themeButton { "Light" };   ///< Toggles dark <-> light label
    juce::TextButton syncButton  { "Sync"  };   ///< Toggles host transport + tempo sync
    juce::TextButton helpButton  { "?"     };   ///< Shows/hides the help overlay

    // ── Row 2: playback direction switch + grid tick controls ─────────────────
    /// Unified 3-way switch — visual order: Reverse ◀  Ping-Pong ◆  Forward ▶.
    /// Triangle glyphs are drawn by a SegmentPainter — no font required.
    ///
    /// APVTS "playbackDirection" uses a different index order than the visual
    /// layout (0=Forward, 1=Reverse, 2=Ping-Pong).  The helpers below handle
    /// the translation so the control and the parameter stay in sync.
    SegmentedControl dirControl;

    // Visual [Rev, P-P, Fwd] ↔ APVTS [Fwd, Rev, P-P] index mapping.
    static constexpr int kDirParamToVis[3] = { 2, 0, 1 };   // APVTS val → visual index
    static constexpr int kDirVisToParam[3] = { 1, 2, 0 };   // visual index → APVTS val

    /// Grid division controls — adjust how many lines appear on each axis.
    juce::TextButton tickYMinusBtn { "Y-" };
    juce::TextButton tickYPlusBtn  { "Y+" };
    juce::TextButton tickXMinusBtn { "X-" };
    juce::TextButton tickXPlusBtn  { "X+" };

    // ── Parameter sliders + labels ────────────────────────────────────────────
    juce::Slider ccSlider;         ///< CC number (0-127) or Note velocity (1-127)
    juce::Slider channelSlider;    ///< MIDI channel (1-16)
    juce::Slider smoothingSlider;  ///< One-pole smoother amount (0-1)
    juce::Slider rangeSlider;      ///< TwoValueHorizontal: left = Min Out, right = Max Out
    juce::Slider speedSlider;      ///< Playback speed (x) or loop beats, depending on sync mode

    juce::Label ccLabel, channelLabel, smoothingLabel, rangeLabel, speedLabel;

    // ── APVTS slider attachments ──────────────────────────────────────────────
    using Attach = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<Attach> ccAttach, channelAttach, smoothingAttach, speedAttach;
    // rangeSlider has no SliderAttachment — JUCE does not support TwoValue sliders
    // in APVTS.  Instead onValueChange writes back directly to the parameter, and
    // addParameterListener catches external changes (automation, state restore).

    // ── Message-type radio buttons ────────────────────────────────────────────
    /// [0]=CC  [1]=Channel Pressure  [2]=Pitch Bend  [3]=Note
    std::array<juce::TextButton, 4> msgTypeBtns;

    // ── Scale quantization rows (visible only in Note mode) ───────────────────
    // Row A — scale preset picker (8 options) + root note (12 pitch classes)
    // Row B — custom mask (12 pitch-class toggles, visible only when Custom selected)

    // Names mirror kScalePresetMasks order in PluginProcessor.cpp.
    static constexpr int kNumScalePresets = 8;
    std::array<juce::TextButton, kNumScalePresets> scalePresetBtns;  ///< Chrom/Maj/Min/…/Custom
    juce::Label scaleLabel { {}, "Scale" };

    static constexpr int kNumPitchClasses = 12;
    std::array<juce::TextButton, kNumPitchClasses> rootNoteBtns;     ///< C C# D D# E … B
    juce::Label rootLabel  { {}, "Key" };

    /// Toggle buttons for custom scale mask — pitch-class buttons styled as circles.
    std::array<juce::TextButton, kNumPitchClasses> customMaskBtns;
    juce::Label notesLabel { {}, "Notes" };

    // ── State ─────────────────────────────────────────────────────────────────
    bool _lightMode { false };

    // ── Private helpers ───────────────────────────────────────────────────────

    /// Configures style, text box, and visibility for a standard horizontal slider.
    void setupSlider (juce::Slider& s, juce::Label& l,
                      const juce::String& labelText,
                      juce::Slider::SliderStyle style = juce::Slider::LinearHorizontal);

    /// Re-colours every child component to match the current _lightMode.
    void applyTheme();

    /// Called when the Sync button is toggled.
    /// Swaps the speed-slider attachment (playbackSpeed <-> syncBeats) and
    /// dims the Play button while host transport is in control.
    void onSyncToggled (bool isSync);

    /// Highlights the active message-type button and updates the CC#/Vel slot.
    void updateMsgTypeButtons();

    /// Swaps the ccSlider attachment and label between CC# and Velocity.
    void updateCCSlot();

    /// Show/hide scale rows and trigger resized().  Call when messageType or scaleMode changes.
    void updateScaleVisibility();

    /// Highlight the active scale preset button.
    void updateScalePresetButtons();

    /// Highlight the active root note button.
    void updateRootNoteButtons();

    /// Sync the 12 custom-mask toggle buttons to the current scaleCustomMask parameter.
    void updateCustomMaskButtons();

    /// Syncs rangeSlider thumbs from APVTS (called after external parameter changes).
    void updateRangeSlider();

    /// Refreshes the rangeLabel text ("0.00 - 1.00" format).
    void updateRangeLabel();

    /// APVTS listener: marshals parameter changes onto the message thread
    /// and calls the appropriate update* helper.
    void parameterChanged (const juce::String& paramID, float newValue) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrawnCurveEditor)
};
