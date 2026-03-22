#pragma once

/**
 * @file PluginEditor.h
 *
 * DrawnCurveEditor — JUCE AudioProcessorEditor for DrawnCurve AUv3.
 *
 * UI Layout (640 × 560 px)
 * ────────────────────────
 *   Utility bar (28 px)  : Theme toggle · Help button
 *   ┌──────────────────────────────────┬───────────────────────┐
 *   │  Left column (376 px)            │  Right column (244 px)│
 *   │                                  │  Transport (94 px)    │
 *   │  Canvas + Y/X density steppers   │  Shaping  (132 px)    │
 *   │                                  │    Lane focus selector│
 *   │                                  │    Smooth             │
 *   │                                  │    Range              │
 *   │                                  │  Routing matrix(148px)│
 *   │                                  │    header row         │
 *   │                                  │    lane × 3 rows      │
 *   │                                  │    mapping detail     │
 *   │  [Note editor — Note mode only]  │                       │
 *   └──────────────────────────────────┴───────────────────────┘
 */

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "SegmentedControl.h"
#include "ScaleLattice.h"

// Colour palette struct — defined in PluginEditor.cpp.
struct Theme;

// ---------------------------------------------------------------------------
// Lane colour/stroke identifiers — used by CurveDisplay and routing matrix.
// ---------------------------------------------------------------------------

/// Lane colours (light mode).  Index = lane number.
static const juce::Colour kLaneColourLight[kMaxLanes] =
{
    juce::Colour (0xff28261F),   // Lane 0 — warm near-black
    juce::Colour (0xff7C3AED),   // Lane 1 — violet
    juce::Colour (0xff0B6E4F),   // Lane 2 — forest green
};

/// Lane colours (dark mode).
static const juce::Colour kLaneColourDark[kMaxLanes] =
{
    juce::Colour (0xffE0E0E0),   // Lane 0 — light grey
    juce::Colour (0xffA78BFA),   // Lane 1 — violet light
    juce::Colour (0xff34D399),   // Lane 2 — emerald
};

//==============================================================================
/**
 * Interactive display of all lane curves, playhead, grid, and axis labels.
 *
 * Renders each lane's curve with a distinct colour and stroke texture.
 * The focused lane is drawn at full opacity; other lanes at 40%.
 * Drawing (mouseDown / Drag / Up) always targets the focused lane.
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

    void setLightMode   (bool light);
    void setFocusedLane (int lane)  { _focusedLane = lane; repaint(); }
    int  getFocusedLane()    const  { return _focusedLane; }

    void setXDivisions (int n) { _xDivisions = juce::jlimit (2, 8, n); repaint(); }
    void setYDivisions (int n) { _yDivisions = juce::jlimit (2, 8, n); repaint(); }
    int  getXDivisions() const { return _xDivisions; }
    int  getYDivisions() const { return _yDivisions; }

private:
    DrawnCurveProcessor& proc;
    double     captureStartTime { 0.0   };
    bool       isCapturing      { false };
    bool       _lightMode       { true  };
    int        _focusedLane     { 0     };
    int        _xDivisions      { 4 };
    int        _yDivisions      { 4 };
    bool       _blinkOn         { true  };   ///< Toggled by _blinkCounter to drive pause blink
    int        _blinkCounter    { 0     };   ///< Incremented each timer tick; wraps at kBlinkPeriod
    static constexpr int kBlinkPeriod = 12; ///< Timer ticks per blink half-cycle (~1.25 Hz at 30 Hz)
    juce::Path capturePath;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CurveDisplay)
};

//==============================================================================
class HelpOverlay : public juce::Component
{
public:
    HelpOverlay();
    void paint     (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void setLightMode (bool light) { _lightMode = light; repaint(); }

private:
    bool _lightMode { true  };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HelpOverlay)
};

//==============================================================================
/**
 * Main plugin editor window.
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
    // ── Custom LookAndFeels ───────────────────────────────────────────────────

    struct SymbolLF : public juce::LookAndFeel_V4
    {
        void drawButtonText (juce::Graphics& g, juce::TextButton& btn,
                             bool, bool) override
        {
            const auto b   = btn.getLocalBounds().toFloat().reduced (3.0f, 2.0f);
            g.setColour (btn.findColour (juce::TextButton::textColourOffId));
            g.setFont (juce::Font (juce::FontOptions{}.withHeight (12.0f)));
            g.drawFittedText (btn.getButtonText(), b.toNearestInt(),
                              juce::Justification::centred, 1);
        }
    };
    SymbolLF _symbolLF;

    struct SyncLF : public juce::LookAndFeel_V4
    {
        // Draws two stacked labels: "FREE" and "SYNC".
        // The button's text string is "FREE" or "SYNC"; the active one is
        // drawn at full opacity, the inactive one at 35%.
        void drawButtonText (juce::Graphics& g, juce::TextButton& btn,
                             bool, bool) override
        {
            const juce::String active = btn.getButtonText();   // "FREE" or "SYNC"
            const auto col   = btn.findColour (juce::TextButton::textColourOffId);
            const auto b     = btn.getLocalBounds().toFloat();
            const float half = b.getHeight() * 0.5f;
            g.setFont (juce::Font (juce::FontOptions{}.withHeight (9.5f).withStyle ("Bold")));
            for (int i = 0; i < 2; ++i)
            {
                const juce::String label = (i == 0) ? "FREE" : "SYNC";
                const bool isActive = (label == active);
                g.setColour (col.withAlpha (isActive ? 1.0f : 0.32f));
                g.drawFittedText (label,
                    juce::roundToInt (b.getX()), juce::roundToInt (b.getY() + half * i),
                    juce::roundToInt (b.getWidth()), juce::roundToInt (half),
                    juce::Justification::centred, 1);
            }
        }
    };
    SyncLF _syncLF;

    struct ScaleActionLF : public juce::LookAndFeel_V4
    {
        void drawButtonText (juce::Graphics& g, juce::TextButton& btn,
                             bool, bool) override
        {
            const auto  col  = btn.findColour (juce::TextButton::textColourOffId);
            const auto  b    = btn.getLocalBounds().toFloat().reduced (4.0f, 5.0f);
            const float r    = juce::jmin (b.getWidth(), b.getHeight()) * 0.38f;
            const float cx   = b.getCentreX(), cy = b.getCentreY();
            const auto  text = btn.getButtonText();
            g.setColour (col);
            if (text == "All")
                g.fillEllipse (cx-r, cy-r, r*2.0f, r*2.0f);
            else if (text == "None")
                g.drawEllipse (cx-r, cy-r, r*2.0f, r*2.0f, 1.5f);
            else if (text == "Inv")
            {
                g.drawEllipse (cx-r, cy-r, r*2.0f, r*2.0f, 1.5f);
                juce::Path half;
                half.addPieSegment (cx-r, cy-r, r*2.0f, r*2.0f,
                                    -juce::MathConstants<float>::halfPi,
                                     juce::MathConstants<float>::halfPi, 0.0f);
                g.fillPath (half);
            }
            else if (text == "Root")
            {
                const float d = r * 0.82f;
                juce::Path dia;
                dia.startNewSubPath (cx, cy-d); dia.lineTo (cx+d, cy);
                dia.lineTo (cx, cy+d);          dia.lineTo (cx-d, cy);
                dia.closeSubPath();
                g.fillPath (dia);
            }
        }
    };
    ScaleActionLF _scaleActionLF;

    struct DensityLF : public juce::LookAndFeel_V4
    {
        void drawButtonText (juce::Graphics& g, juce::TextButton& btn,
                             bool, bool) override
        {
            const auto  text    = btn.getButtonText();
            const bool  isX     = text.startsWithChar ('X');
            const bool  isDense = text.endsWithChar   ('+');
            const auto  col     = btn.findColour (juce::TextButton::textColourOffId);
            const auto  b       = btn.getLocalBounds().toFloat().reduced (4.0f, 5.0f);
            const int   n       = isDense ? 5 : 3;
            g.setColour (col);
            for (int i = 0; i < n; ++i)
            {
                const float t = (i + 0.5f) / static_cast<float> (n);
                if (!isX) { const float y = b.getY() + b.getHeight() * t;
                            g.drawLine (b.getX(), y, b.getRight(), y, 1.3f); }
                else       { const float x = b.getX() + b.getWidth() * t;
                            g.drawLine (x, b.getY(), x, b.getBottom(), 1.3f); }
            }
        }
    };
    DensityLF _densityLF;

    // ── Core references ───────────────────────────────────────────────────────
    DrawnCurveProcessor& proc;
    CurveDisplay curveDisplay;
    HelpOverlay  helpOverlay;

    // ── Utility bar ───────────────────────────────────────────────────────────
    juce::TextButton playButton  { "Play"  };
    juce::TextButton clearButton { "Clear" };
    juce::TextButton panicButton { "!"     };
    juce::TextButton themeButton { "Light" };
    juce::TextButton syncButton  { "Sync"  };
    juce::TextButton helpButton  { "?"     };

    // ── Transport: direction + sync ───────────────────────────────────────────
    SegmentedControl dirControl;
    static constexpr int kDirParamToVis[3] = { 2, 0, 1 };
    static constexpr int kDirVisToParam[3] = { 1, 2, 0 };

    // ── Canvas density + Clear ────────────────────────────────────────────────
    juce::TextButton tickYMinusBtn { "Y-" };
    juce::TextButton tickYPlusBtn  { "Y+" };
    juce::TextButton tickXMinusBtn { "X-" };
    juce::TextButton tickXPlusBtn  { "X+" };

    // ── Shared speed slider + attachment ─────────────────────────────────────
    juce::Slider speedSlider;
    juce::Label  speedLabel;
    using Attach = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<Attach> speedAttach;

    // ── Shaping panel — per focused lane ─────────────────────────────────────
    SegmentedControl laneFocusCtrl;   ///< Lane 1 | Lane 2 | Lane 3

    juce::Slider smoothingSlider;
    juce::Slider rangeSlider;
    juce::Label  smoothingLabel, rangeLabel;
    std::unique_ptr<Attach> smoothingAttach;

    // ── Routing matrix — one row per lane ─────────────────────────────────────
    // Each row: type button | detail label | channel label | teach | mute
    // laneTypeBtn cycles through CC/PB/Note/Aft on click; right-click = popup menu.
    std::array<juce::TextButton, kMaxLanes> laneTypeBtn;
    std::array<juce::Label,      kMaxLanes> laneDetailLabel;
    std::array<juce::Label,      kMaxLanes> laneChannelLabel;
    std::array<juce::TextButton, kMaxLanes> laneTeachBtn;
    std::array<juce::TextButton, kMaxLanes> laneMuteBtn;

    /// One-line detail text below the matrix rows: e.g. "CC 74 · Ch 1"
    juce::Label mappingDetailLabel;

    // ── Notes editor (visible in Note mode for focused lane) ──────────────────
    static constexpr int kNumScalePresets = 8;
    std::array<juce::TextButton, kNumScalePresets> scalePresetBtns;
    juce::Label scaleLabel { {}, "Scale" };

    ScaleLattice scaleLattice;

    juce::TextButton scaleAllBtn  { "All"  };
    juce::TextButton scaleNoneBtn { "None" };
    juce::TextButton scaleInvBtn  { "Inv"  };
    juce::TextButton scaleRootBtn { "Root" };

    juce::Label maskLabel;

    static constexpr int kScaleLatticeH = 100;

    // ── Editor state ──────────────────────────────────────────────────────────
    bool _lightMode    { true  };
    int  _focusedLane  { 0     };   ///< Which lane's shaping / notes are shown

    // Section background rects (set in resized, read in paint).
    juce::Rectangle<int> _secTransport;
    juce::Rectangle<int> _secShaping;
    juce::Rectangle<int> _secRouting;
    juce::Rectangle<int> _secNotes;

    // ── Private helpers ───────────────────────────────────────────────────────
    void setupSlider (juce::Slider& s, juce::Label& l,
                      const juce::String& labelText,
                      juce::Slider::SliderStyle style = juce::Slider::LinearHorizontal);

    void applyTheme();
    void onSyncToggled    (bool isSync);

    /// Rebind shaping sliders + range slider to the given lane's APVTS params.
    void bindShapingToLane (int lane);

    /// Set _focusedLane, update canvas, rebind shaping, refresh note editor.
    void setFocusedLane (int lane);

    void updateMsgTypeButtons();      // kept for backward compat, delegates to lane row
    void updateLaneRow      (int lane);   ///< Refresh target/detail/channel display for one lane
    void updateAllLaneRows  ();           ///< Refresh all three lane rows
    void updateScaleVisibility();
    void updateScalePresetButtons();
    void updateRangeSlider();
    void updateRangeLabel();
    void updateMaskLabel();

    void parameterChanged (const juce::String& paramID, float newValue) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrawnCurveEditor)
};
