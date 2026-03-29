#pragma once

/**
 * @file PluginEditor.h
 *
 * DrawnCurveEditor — JUCE AudioProcessorEditor for DrawnCurve AUv3.
 *
 * UI Layout (800 × 760 px)
 * ────────────────────────
 *   Utility bar (36 px)  : Sync · ≡LaneSync · FREE/speed · | ♯/♭ · Clear · Panic · Theme · ?
 *   ┌─────────────────────────────────────┬──────────────────────────┐
 *   │  Stage panel (476 px)               │  Right rail (296 px)     │
 *   │  Canvas + Y/X density steppers      │  FOCUSED LANE (full)     │
 *   │                                     │    Lane selector 1/2/3   │
 *   │                                     │    Direction             │
 *   │                                     │    Speed (per-lane) ×    │
 *   │                                     │    Range                 │
 *   │                                     │    Smooth / Phase        │
 *   │                                     │    ── LANES ──           │
 *   │                                     │    ∞/1  Matrix rows      │
 *   └─────────────────────────────────────┴──────────────────────────┘
 *   Musical zone — full width (44 px collapsed / 268 px expanded)
 */

#include <array>
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "SegmentedControl.h"
#include "ScaleLattice.h"
#include "UI/IconFactory.h"
#include "ScaleData.h"

// Colour palette struct — defined in PluginEditor.cpp.
struct Theme;

//==============================================================================
/**
 * App-level LookAndFeel — delivers SF Pro for every font request via
 * getTypefaceForFont(), bypassing JUCE's internal name→CTFontDescriptor lookup
 * which fails for SF Pro because its internal PostScript names (e.g.
 * ".SFUI-Regular") are not registered as CTFont family names.
 *
 * Root fix: JUCE exposes Typeface::findSystemTypeface() which calls
 * CTFontCreateUIFontForLanguage(kCTFontUIFontSystem, …) — the only CoreText
 * API that reliably yields the real SF Pro CTFontRef on every iOS version.
 * Overriding getTypefaceForFont() to return this typeface ensures HarfBuzz
 * is backed by the actual SF Pro font object with full Unicode coverage,
 * including ♭ (U+266D), ♯ (U+266F), ♮ (U+266E) and every other glyph Apple
 * ships in the system typeface.
 *
 * All widget-specific LookAndFeel subclasses inside DrawnCurveEditor inherit
 * from this class so they all get the same font.
 */
class DrawnCurveLookAndFeel : public juce::LookAndFeel_V4
{
public:
    DrawnCurveLookAndFeel()
    {
        // Expose the system typeface family name so any LookAndFeel-wide
        // query for the default sans-serif also resolves to SF Pro.
        if (auto tp = sfProTypeface())
            setDefaultSansSerifTypefaceName (tp->getName());
    }

    /** Override: return the real SF Pro typeface for every font request.
     *  Size / style adjustments are applied by JUCE on top of the typeface,
     *  so this does not affect layout metrics or weight handling. */
    juce::Typeface::Ptr getTypefaceForFont (const juce::Font& f) override
    {
        if (auto tp = sfProTypeface())
            return tp;
        return LookAndFeel_V4::getTypefaceForFont (f);
    }

    juce::Font getLabelFont (juce::Label&) override
    {
        return makeFont (12.0f);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override
    {
        return makeFont (juce::jmin (14.0f, (float) buttonHeight * 0.55f));
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override
    {
        return makeFont (13.0f);
    }

    /** Shared font factory — use this everywhere a juce::Font is needed.
     *
     *  Relies on DrawnCurveLookAndFeel being installed as the GLOBAL default
     *  LookAndFeel (via LookAndFeel::setDefaultLookAndFeel in the editor
     *  constructor).  That causes juce_getTypefaceForFont() to call our
     *  getTypefaceForFont() override, which always returns sfProTypeface()
     *  regardless of the name string here.
     *
     *  We deliberately do NOT call FontOptions::withTypeface() here:
     *    • withTypeface embeds a size-0 CTFontRef (from findSystemTypeface())
     *      directly into the Font object, bypassing JUCE's per-render sizing.
     *      HarfBuzz then uses the size-0 metrics and mishandles multi-byte
     *      Unicode glyphs — ♭ ♯ ♮ render as raw UTF-8 bytes ("â[?]®6" etc.).
     *    • When JUCE's fallback shaper (findSuitableFontForText) later calls
     *      withTypeface() on the same FontOptions — which now has non-empty
     *      name and style set by the first withTypeface() call — it fires
     *      jassert(x == nullptr || style.isEmpty()) in juce_FontOptions.h:126.
     *
     *  withFallbackEnabled is still set so CoreText substitutes any code points
     *  SF Pro delegates to a secondary typeface (emoji, rare scripts, etc.). */
    static juce::Font makeFont (float height)
    {
        return juce::Font (juce::FontOptions{}
                               .withName (juce::Font::getDefaultSansSerifFontName())
                               .withHeight (height)
                               .withFallbackEnabled (true));
    }

private:
    /** Process-lifetime SF Pro typeface used by getTypefaceForFont() to cover
     *  fonts NOT created via makeFont() — JUCE's own widget internals, fonts
     *  created by third-party code, SegmentedControl, ScaleLattice, etc. */
    static const juce::Typeface::Ptr& sfProTypeface()
    {
        static const juce::Typeface::Ptr instance = juce::Typeface::findSystemTypeface();
        return instance;
    }
};

// ---------------------------------------------------------------------------
// Lane colour/stroke identifiers — used by CurveDisplay and routing matrix.
// ---------------------------------------------------------------------------

/// Lane colours (light mode).  Index = lane number.
static const juce::Colour kLaneColourLight[kMaxLanes] =
{
    juce::Colour (0xff7A4CFF),   // Lane 0 — purple (primary note lane)
    juce::Colour (0xff2D9D74),   // Lane 1 — teal green (expression / CC)
    juce::Colour (0xffD9822B),   // Lane 2 — warm orange (counterline / note)
    juce::Colour (0xff0891B2),   // Lane 3 — cyan blue (extra modulation)
};

/// Lane colours (dark mode).
static const juce::Colour kLaneColourDark[kMaxLanes] =
{
    juce::Colour (0xffA78BFA),   // Lane 0 — violet light
    juce::Colour (0xff34D399),   // Lane 1 — emerald
    juce::Colour (0xffFB923C),   // Lane 2 — orange light
    juce::Colour (0xff22D3EE),   // Lane 3 — cyan light
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
    void setUseFlats    (bool b)    { _useFlats = b; repaint(); }

    void setXDivisions (int n) { _xDivisions = juce::jlimit (2, 32, n); repaint(); }
    void setYDivisions (int n) { _yDivisions = juce::jlimit (2, 24, n); repaint(); }
    int  getXDivisions() const { return _xDivisions; }
    int  getYDivisions() const { return _yDivisions; }

    std::function<void()> onBadgeTap;    ///< Called when the scale badge is tapped
    std::function<void()> onCurveDrawn; ///< Called when a gesture is finalized (mouseUp)

private:
    juce::Rectangle<int> _badgeRect;    ///< Updated each paint(); used for tap detection
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
    bool       _useFlats        { false };   ///< Mirror of editor flag; controls note name rendering
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

/**
 * TwoValueHorizontal slider that supports dragging the track (not the thumbs)
 * to shift the entire range. Tapping/dragging near a thumb still moves that
 * thumb individually (standard behaviour via the base class).
 */
/**
 * Two-thumb horizontal range slider with whole-range drag support.
 *
 * Standard TwoValueHorizontal behaviour:
 *   Tap/drag within kThumbHitRadius px of the min or max thumb → moves that thumb alone.
 *
 * Range-drag behaviour (replaces JUCE's default "snap nearest thumb" on track tap):
 *   Tap/drag anywhere on the track that is NOT near either thumb → shifts both thumbs by
 *   the same delta, preserving the span.  Both ends are clamped to [minimum, maximum]
 *   so the span shrinks at the extremes.
 *
 * Practical use (DrawnCurve):
 *   The range slider maps [minOutput, maxOutput] → MIDI output range.
 *   Dragging the track transposes a Note-mode lane or shifts a CC/PB band.
 *   Grabbing a thumb adjusts only that boundary.
 */
class DualRangeSlider : public juce::Slider
{
public:
    DualRangeSlider() { setSliderStyle (juce::Slider::TwoValueHorizontal); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        _rangeDragging = false;
        if (isThumbHit (e.x))
        {
            juce::Slider::mouseDown (e);
            return;
        }
        _dragStartMin = getMinValue();
        _dragStartMax = getMaxValue();
        _dragStartX   = e.x;
        _rangeDragging = true;
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! _rangeDragging) { juce::Slider::mouseDrag (e); return; }
        const double delta = (e.x - _dragStartX) / (double) getWidth()
                             * (getMaximum() - getMinimum());
        // Each thumb clamps independently — span shrinks at walls (sticky-wall behaviour).
        setMinValue (juce::jlimit (getMinimum(), getMaximum(), _dragStartMin + delta),
                     juce::sendNotificationSync);
        setMaxValue (juce::jlimit (getMinimum(), getMaximum(), _dragStartMax + delta),
                     juce::sendNotificationSync);
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! _rangeDragging) { juce::Slider::mouseUp (e); return; }
        _rangeDragging = false;
    }

private:
    bool   _rangeDragging { false };
    double _dragStartMin  { 0.0 };
    double _dragStartMax  { 1.0 };
    int    _dragStartX    { 0 };
    static constexpr int kThumbHitRadius = 18;

    bool isThumbHit (int px) const
    {
        const auto range = getMaximum() - getMinimum();
        if (range <= 0.0) return false;
        const float minPx = static_cast<float> (
            (getMinValue() - getMinimum()) / range * getWidth());
        const float maxPx = static_cast<float> (
            (getMaxValue() - getMinimum()) / range * getWidth());
        return std::abs (px - (int)minPx) <= kThumbHitRadius
            || std::abs (px - (int)maxPx) <= kThumbHitRadius;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DualRangeSlider)
};

// Forward declaration — RoutingOverlay is defined below DrawnCurveEditor.
class DrawnCurveEditor;

//==============================================================================
/**
 * Drop-down routing panel shown when the user taps a lane's msg-type button.
 *
 * The overlay fills the full editor bounds (transparent outside the panel rect)
 * so it intercepts mouse events everywhere and dismisses on a tap outside the
 * panel.  All routing child components live inside this component so z-ordering
 * is clean (no separate parent-repaint coordination needed).
 */
class RoutingOverlay : public juce::Component
{
public:
    explicit RoutingOverlay (DrawnCurveEditor& owner);
    ~RoutingOverlay() override;

    void paint   (juce::Graphics&)          override;
    void resized ()                         override;
    void mouseDown (const juce::MouseEvent&) override;

    /// Show the single-lane routing panel anchored below anchorBounds.
    void showForLane (int lane, juce::Rectangle<int> anchorBoundsInEditor);

    /// Show the all-lanes mini-matrix panel anchored below anchorBounds.
    void showForAllLanes (juce::Rectangle<int> anchorBoundsInEditor);

    /// Hide the panel.
    void dismiss();

    /// Reposition the panel when the editor is resized, without recreating APVTS attachments.
    /// Safe to call whether in single-lane or all-lanes mode.
    void reanchor (juce::Rectangle<int> anchorBoundsInEditor);

    void setLightMode (bool light);

    /// -1 = all-lanes mode, 0..kMaxLanes-1 = individual lane.
    int activeLane() const noexcept { return _lane; }

private:
    DrawnCurveEditor& _owner;
    int               _lane      { 0 };
    bool              _lightMode { true };
    juce::Rectangle<int> _panelRect;   ///< Panel bounds in local (overlay) coords

    // ── Single-lane panel components ─────────────────────────────────────────
    juce::TextButton ccBtn;     ///< msg type: CC
    juce::TextButton atBtn;     ///< msg type: AT
    juce::TextButton pbBtn;     ///< msg type: PB
    juce::TextButton noteBtn;   ///< msg type: Note (♩ — text set in constructor)

    juce::Label  detailLabel;    ///< "CC 74" / "Vel 100"
    juce::Label  channelLabel;   ///< "Ch 1"

    juce::TextButton teachBtn;    ///< MIDI learn
    juce::TextButton muteBtn;     ///< lane pause
    juce::TextButton oneShotBtn;  ///< ↦/↺ toggle — one-shot (lit) vs loop (unlit)
    juce::TextButton legatoBtn;   ///< tie symbol toggle — legato note overlap

    juce::Slider phaseSlider;
    juce::Label  phaseLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> _phaseAttach;

    // ── All-lanes mini matrix (shown when invoked from the ✤ tab) ────────────
    std::array<juce::TextButton, kMaxLanes> rowTypeBtn;   ///< one msg-type button per lane
    std::array<juce::Label,      kMaxLanes> rowDetailLbl; ///< CC# / vel per lane
    std::array<juce::Label,      kMaxLanes> rowChanLbl;   ///< channel per lane

    void applyTheme();
    void showSingleLaneMode (bool visible);   ///< show/hide single-lane components
    void showAllLanesMode   (bool visible);   ///< show/hide all-lanes matrix components
    void updateMsgTypeHighlight();
    void updateDetailLabels();
    void cycleToMsgType (MessageType t);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RoutingOverlay)
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

    struct SymbolLF : public DrawnCurveLookAndFeel
    {
        void drawButtonText (juce::Graphics& g, juce::TextButton& btn,
                             bool, bool) override
        {
            const auto b = btn.getLocalBounds().toFloat().reduced (3.0f, 2.0f);
            if (b.isEmpty()) return;    // guard: tiny button → skip to avoid assertion
            g.setColour (btn.findColour (juce::TextButton::textColourOffId));
            g.setFont (DrawnCurveLookAndFeel::makeFont (12.0f));  // fallback chain included
            g.drawFittedText (btn.getButtonText(), b.toNearestInt(),
                              juce::Justification::centred, 1);
        }
    };
    SymbolLF _symbolLF;

    /// LookAndFeel for beat preset buttons: keeps background unchanged when
    /// disabled (only dims text), so disabled buttons don't go dark in light
    /// mode or lose contrast in dark mode.
    struct PresetBtnLF : public DrawnCurveLookAndFeel
    {
        void drawButtonBackground (juce::Graphics& g, juce::Button& btn,
                                   const juce::Colour& bgColour,
                                   bool highlight, bool down) override
        {
            auto r   = btn.getLocalBounds().toFloat().reduced (0.5f);
            auto col = bgColour;
            if (highlight) col = col.contrasting (0.05f);
            if (down)      col = col.contrasting (0.10f);
            g.setColour (col);
            g.fillRoundedRectangle (r, 4.0f);
        }

        void drawButtonText (juce::Graphics& g, juce::TextButton& btn,
                             bool, bool) override
        {
            const float alpha = btn.isEnabled() ? 1.0f : 0.28f;
            g.setColour (btn.findColour (juce::TextButton::textColourOffId)
                            .withMultipliedAlpha (alpha));
            g.setFont (DrawnCurveLookAndFeel::makeFont (11.0f));
            g.drawFittedText (btn.getButtonText(),
                              btn.getLocalBounds().reduced (2, 1),
                              juce::Justification::centred, 1);
        }
    };
    PresetBtnLF _presetBtnLF;

    /// LookAndFeel that draws a dcui icon instead of text — applied to
    /// TextButton arrays (mute, teach) that cannot be changed to IconButton
    /// without structural changes.
    struct IconDrawLF : public DrawnCurveLookAndFeel
    {
        dcui::IconType iconType { dcui::IconType::mute };

        void drawButtonBackground (juce::Graphics& g, juce::Button& btn,
                                   const juce::Colour&, bool, bool) override
        {
            const auto r     = btn.getLocalBounds().toFloat().reduced (0.5f);
            const bool tog   = btn.getToggleState();
            const auto bg    = btn.findColour (juce::TextButton::buttonColourId);
            const auto hlBg  = btn.findColour (juce::TextButton::buttonOnColourId);
            g.setColour (tog ? hlBg : bg);
            g.fillRoundedRectangle (r, 4.0f);
        }

        void drawButtonText (juce::Graphics& g, juce::TextButton& btn,
                             bool isHover, bool) override
        {
            const bool tog = btn.getToggleState();
            auto col = btn.findColour (tog ? juce::TextButton::textColourOnId
                                           : juce::TextButton::textColourOffId);
            if (isHover) col = col.brighter (0.18f);
            const auto iconBounds = btn.getLocalBounds().toFloat().reduced (5.0f, 5.0f);
            if (iconBounds.isEmpty()) return;   // guard: tiny button → skip
            dcui::IconFactory::drawIcon (g, iconType, iconBounds, col);
        }
    };
    IconDrawLF _muteDrawLF;
    IconDrawLF _teachDrawLF;

    struct DensityLF : public DrawnCurveLookAndFeel
    {
        void drawButtonText (juce::Graphics& g, juce::TextButton& btn,
                             bool, bool) override
        {
            const auto  text    = btn.getButtonText();
            const bool  isX     = text.startsWithChar ('X');
            const bool  isDense = text.endsWithChar   ('+');
            const auto  col     = btn.findColour (juce::TextButton::textColourOffId);
            const auto  b       = btn.getLocalBounds().toFloat().reduced (4.0f, 5.0f);
            if (b.isEmpty()) return;    // guard: avoid assertion with negative rect
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

    /// App-level LookAndFeel — set on the editor so all child components that
    /// don't have their own LF inherit it.  Must be declared before any
    /// component that might reference it during construction.
    DrawnCurveLookAndFeel _appLF;

    // ── Core references ───────────────────────────────────────────────────────
    DrawnCurveProcessor& proc;
    CurveDisplay   curveDisplay;
    HelpOverlay    helpOverlay;
    RoutingOverlay _routingOverlay;   ///< Drop-down routing panel (initially hidden)

    // ── Utility bar ───────────────────────────────────────────────────────────
    juce::TextButton playButton  { "Play"  };
    dcui::IconButton clearButton { "Clear", dcui::IconType::clearGesture };
    juce::TextButton panicButton { "!"     };
    juce::TextButton themeButton { "Dark"  };
    dcui::IconButton syncButton  { "Sync", dcui::IconType::sync };
    juce::TextButton helpButton  { "?"     };

    // ── Transport: direction + sync ───────────────────────────────────────────
    SegmentedControl dirControl;
    static constexpr int kDirParamToVis[3] = { 2, 0, 1 };
    static constexpr int kDirVisToParam[3] = { 1, 2, 0 };

    // ── Canvas density + quantization ─────────────────────────────────────────
    juce::TextButton tickYMinusBtn { "Y-" };
    juce::TextButton tickYPlusBtn  { "Y+" };
    juce::TextButton tickXMinusBtn { "X-" };
    juce::TextButton tickXPlusBtn  { "X+" };
    dcui::IconButton xQuantizeBtn  { "xQuantize", dcui::IconType::lock };   ///< Toggle X-axis (time) quantization
    dcui::IconButton yQuantizeBtn  { "yQuantize", dcui::IconType::lock };   ///< Toggle Y-axis (pitch/value) quantization
    juce::TextButton xyLockBtn     { "#" };                                 ///< Combo: activate both X + Y quantize at once

    // ── Beat subdivision + duration presets (sync mode, X-axis row) ──────────
    static constexpr int kNumXStepPresets    = 7;
    static constexpr int kNumSyncBeatsPresets = 6;
    std::array<juce::TextButton, kNumXStepPresets>     xStepPresetBtns;
    std::array<juce::TextButton, kNumSyncBeatsPresets> syncBeatsPresetBtns;

    // ── Y-axis presets (mode-dependent, Y-axis column) ────────────────────────
    // Note mode: octave range (1,2,3,4,8) + semitone step (1,2,3,4,5)
    // CC/Pressure: dozen range (12,24,36,48,60,72) + value step (1,2,3,4,6)
    static constexpr int kNumYOctavePresets   = 5;
    static constexpr int kNumYNoteStepPresets = 5;
    static constexpr int kNumYDozenPresets    = 6;
    static constexpr int kNumYCCStepPresets   = 5;
    static constexpr int kNumYPBRangePresets  = 4;  // PitchBend: 4k, 3k, 2k, 1k (k=2048)
    static constexpr int kNumYPBStepPresets   = 4;  // PitchBend step: 64, 128, 256, 512 PB units
    std::array<juce::TextButton, kNumYOctavePresets>   yOctavePresetBtns;
    std::array<juce::TextButton, kNumYNoteStepPresets> yNoteStepPresetBtns;
    std::array<juce::TextButton, kNumYDozenPresets>    yDozenPresetBtns;
    std::array<juce::TextButton, kNumYCCStepPresets>   yCCStepPresetBtns;
    std::array<juce::TextButton, kNumYPBRangePresets>  yPBRangePresetBtns;
    std::array<juce::TextButton, kNumYPBStepPresets>   yPBStepPresetBtns;

    // ── Shared speed slider + attachment ─────────────────────────────────────
    juce::Slider speedSlider;      ///< Global speed — always in utility bar
    juce::Label  speedLabel;
    using Attach = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<Attach> speedAttach;

    // ── Per-lane speed slider (focused lane panel, below direction control) ───
#if defined(DC_HAVE_PER_LANE_PLAYBACK_PARAMS)
    juce::Slider laneSpeedSlider;  ///< Per-lane speed multiplier
    juce::Label  laneSpeedLabel;
    std::unique_ptr<Attach> laneSpeedAttach;
#endif

    // ── Shaping panel — per focused lane ─────────────────────────────────────
    SegmentedControl laneFocusCtrl;   ///< Lane 1 | Lane 2 | Lane 3

    juce::TextButton oneShotBtn;            ///< Loop (∞) / One-shot (1) — shaping panel
    juce::TextButton laneSyncBtn;           ///< Lock all lane playheads to the same phase
    juce::Slider     smoothingSlider;
    DualRangeSlider  rangeSlider;           ///< Track-drag shifts both thumbs together
    juce::Slider     phaseOffsetSlider;
    juce::Label  smoothingLabel, rangeLabel, phaseOffsetLabel;
    std::unique_ptr<Attach> smoothingAttach;
    std::unique_ptr<Attach> phaseOffsetAttach;

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

    /// Transparent hit-area buttons drawn over lane colour dots in the matrix.
    /// Click → setFocusedLane(L).
    std::array<juce::TextButton, kMaxLanes> laneSelectBtn;

    /// "+" button that appears below the last active lane row when more lanes are available.
    juce::TextButton addLaneBtn;
    /// "-" button that appears next to addLaneBtn; removes the last active lane.
    juce::TextButton deleteLaneBtn;

    /// Tick-count readout labels shown between the density stepper buttons.
    juce::Label tickXCountLabel, tickYCountLabel;
    /// Step-size annotation: semitones (Y/Note) or seconds/beats (X). Hidden when not applicable.
    juce::Label yTickStepLabel, xTickStepLabel;

    // ── Notes editor — family browser (visible in Note mode) ─────────────────

    // Family tab bar — one button per dcScale family.
    std::array<juce::TextButton, dcScale::kNumFamilies> familyBtns;

    // Subfamily chips — one per mode in the active family.
    // SubfamilyLF renders the mode name + a miniature 5+7 two-row lattice into
    // each TextButton using a custom drawButtonText.
    static constexpr int kMaxModes = 11;   // Chordal is the largest family (11 entries)

    struct SubfamilyLF : public DrawnCurveLookAndFeel
    {
        uint16_t     mask   { 0xAD5 };
        juce::Colour colOn  { 0xffC9D6E3 };
        juce::Colour colOff { 0xff2A3340 };

        void drawButtonText (juce::Graphics& g, juce::TextButton& btn,
                             bool /*isHover*/, bool /*isDown*/) override
        {
            const auto b = btn.getLocalBounds().toFloat().reduced (2.0f, 2.0f);
            if (b.isEmpty()) return;

            // ── Name (top ~60 %) ─────────────────────────────────────────
            const float dotsH    = juce::jmax (14.0f, b.getHeight() * 0.40f);
            const auto  nameRect = b.withTrimmedBottom (dotsH + 2.0f);
            const auto  dotsRect = b.withTrimmedTop    (b.getHeight() - dotsH);

            g.setColour (btn.findColour (juce::TextButton::textColourOffId));
            g.setFont (DrawnCurveLookAndFeel::makeFont (
                juce::jmin (11.0f, nameRect.getHeight() * 0.72f)));
            g.drawFittedText (btn.getButtonText(), nameRect.toNearestInt(),
                              juce::Justification::centred, 2);

            // ── Miniature 5+7 lattice (bottom ~40 %) ─────────────────────
            // Matches the full ScaleLattice geometry exactly:
            //   Bottom row: C D E F G A B   (7 naturals)
            //   Top row:  C♯ D♯  F♯ G♯ A♯  (5 chromatics)
            //   C/C♯/D and every adjacent triple form equilateral triangles.
            //
            // Geometry constraints (same formulae as ScaleLattice::buildLayout):
            //   rFromW: 6 natSteps (= 15r) + 2 edge radii (2r) + 2px margin → (dw-2)/17
            //   rFromH: two rows of circles + equilateral row-sep fit in dh
            //           → (dh-2) / (2 + 2.5*√3/2)
            static constexpr float kSqrt3  = 1.7320508f;
            static constexpr int   kNatPC[7] = { 0, 2, 4, 5, 7, 9, 11 };
            static constexpr int   kChrPC[5] = { 1, 3, 6, 8, 10 };
            static constexpr int   kChrL[5]  = { 0, 1, 3, 4, 5 };
            static constexpr int   kChrR[5]  = { 1, 2, 4, 5, 6 };

            const float dw = dotsRect.getWidth();
            const float dh = dotsRect.getHeight();

            const float rFromW  = (dw - 2.0f) / 17.0f;
            const float rFromH  = (dh - 2.0f) / (2.0f + 2.5f * kSqrt3 * 0.5f);
            const float r       = juce::jmax (1.0f, juce::jmin (rFromW, rFromH));
            const float natStep = 2.5f * r;
            const float rowSep  = natStep * kSqrt3 * 0.5f;

            // Centre the 7 naturals in the available width.
            const float margin = (dw - 6.0f * natStep) * 0.5f;
            float natX[7];
            for (int i = 0; i < 7; ++i)
                natX[i] = dotsRect.getX() + margin + static_cast<float>(i) * natStep;

            const float midY = dotsRect.getCentreY();
            const float natY = midY + rowSep * 0.5f;
            const float chrY = midY - rowSep * 0.5f;

            // Chip background colour — used as inactive-dot fill so they read like
            // the full lattice's "empty" nodes (filled circle, tinted border).
            const juce::Colour chipBg = btn.findColour (juce::TextButton::buttonColourId);

            auto drawDot = [&] (float cx, float cy, bool on)
            {
                const juce::Rectangle<float> circ (cx - r, cy - r, r * 2.0f, r * 2.0f);
                const float borderW = juce::jmax (0.6f, r * 0.18f);
                if (on)
                {
                    g.setColour (colOn);
                    g.fillEllipse (circ);
                    g.setColour (colOn.darker (0.25f));
                    g.drawEllipse (circ, borderW);
                }
                else
                {
                    // Filled like inactive nodes in the full lattice: chip-bg fill + tinted border.
                    g.setColour (chipBg.brighter (0.08f));
                    g.fillEllipse (circ);
                    g.setColour (colOff);
                    g.drawEllipse (circ, borderW);
                }
            };

            for (int i = 0; i < 7; ++i)
                drawDot (natX[i], natY, ((mask >> (11 - kNatPC[i])) & 1) != 0);

            for (int j = 0; j < 5; ++j)
            {
                const float cx = (natX[kChrL[j]] + natX[kChrR[j]]) * 0.5f;
                drawDot (cx, chrY, ((mask >> (11 - kChrPC[j])) & 1) != 0);
            }
        }
    };

    std::array<SubfamilyLF,      kMaxModes> _subfamilyLF;
    std::array<juce::TextButton, kMaxModes> subfamilyBtns;

    // Transformation / action row buttons
    juce::TextButton scaleNotationBtn;  ///< ♯/♭ toggle — switches chromatic note label style
    juce::TextButton scaleRotateBtn;    ///< ↻  transpose PCS + root up by 1 semitone
    juce::TextButton scaleAllBtn;       ///< ●  all 12 notes active
    juce::TextButton scaleNoneBtn;      ///< ○  root only
    juce::TextButton scaleInvBtn;       ///< ◑  complement (toggle membership)
    juce::TextButton scaleRootBtn;      ///< ◆  enter root-select mode

    // Status labels (mode name + decimal bitmask)
    juce::Label scaleLabel { {}, "Scale" };   ///< Dynamic mode name or "Custom"
    juce::Label maskLabel;                     ///< Decimal bitmask display (read-only)

    ScaleLattice scaleLattice;

    static constexpr int kScaleLatticeH = 100;

    // Recognition cache — updated by updateScaleStatus(), read by updateScalePresetButtons().
    int _recognisedFamily { -1 };
    int _recognisedMode   { -1 };

    // Active browsed family (tab highlight + chip row) — set by setActiveFamily().
    // kRecentFamilyIdx is a virtual sentinel meaning "show _recentMasks".
    static constexpr int kRecentFamilyIdx = dcScale::kNumFamilies;   // = 8
    static constexpr int kMaxRecentMasks  = 5;
    int _activeFamilyIdx   { 0 };
    int _numSubfamilyChips { 0 };

    // Recent-history tab: most recently applied relative masks, newest first.
    juce::TextButton      recentFamilyBtn;     ///< "Recent" tab (9th in family bar)
    std::vector<uint16_t> _recentMasks;        ///< relative masks, max kMaxRecentMasks

    /// "+" button — always in the eyebrow, next to the lane selector.
    /// Adds a lane (up to kMaxLanes). Shown only when capacity allows.
    juce::TextButton eyebrowAddLaneBtn;
    /// "−" button — always in the eyebrow, next to the lane selector.
    /// Removes the focused lane. Shown only when more than one lane is active.
    juce::TextButton eyebrowDeleteLaneBtn;

    // ── Editor state ──────────────────────────────────────────────────────────
    bool _lightMode         { true  };
    bool _useFlats          { false };   ///< Chromatic notation: false = ♯ names, true = ♭ names
    int  _focusedLane       { 0     };   ///< Which lane's shaping / notes are shown
    bool _scaleOverlayOpen  { false };   ///< true while the scale browser overlay is visible

    /// Right rail is permanently removed — layout is always full-width canvas.
    /// Kept as a compile-time constant (false) so old branches compile cleanly
    /// until they are fully pruned.
    static constexpr bool _railExpanded = false;

    /// Last mode index selected per family (index 0..kNumFamilies-1).
    /// Persists across tab switches so re-visiting a family restores the last mode used.
    std::array<int, dcScale::kNumFamilies> _lastModePerFamily {};   // default = 0 (first mode)

    // Panel rects (set in resized, read in paint).
    juce::Rectangle<int> _stagePanel;
    juce::Rectangle<int> _globalPanel;
    juce::Rectangle<int> _focusedLanePanel;
    juce::Rectangle<int> _lanesPanel;
    juce::Rectangle<int> _musicalPanel;
    /// Top-left of first matrix row (used by paint() to draw lane colour dots).
    juce::Point<int>     _matrixRowOrigin;
    int                  _matrixRowStride { 35 };  ///< px per matrix row (matRowH=32 + 3 gap)

    // Legacy aliases still referenced by parameterChanged / updateScaleVisibility.
    juce::Rectangle<int>& _secTransport   = _globalPanel;
    juce::Rectangle<int>& _secShaping     = _focusedLanePanel;
    juce::Rectangle<int>& _secRouting     = _lanesPanel;
    juce::Rectangle<int>  _secNotes;   // kept for note-editor layout in resized()

    bool _showingAllLanes  { false };   ///< true when the "*" (All Lanes) tab is active.
    bool _musicalExpanded  { false };   ///< true when the musical zone is expanded

    juce::TextButton musicalToggleBtn;  ///< collapses / expands the musical zone

#if defined(DC_HAVE_PER_LANE_PLAYBACK_PARAMS)
    /// Rebind speed slider + direction control to lane-specific params (lane ≥ 0)
    /// or to global params (lane < 0, i.e. the "*" tab).
    void bindPlaybackToLane (int lane);
#endif

    // ── Scale browser overlay ─────────────────────────────────────────────────
    /**
     * Full-editor backdrop for the scale browser overlay.
     * Dims everything behind it; scale browser components are reparented INTO this
     * overlay on open (so they paint on top of the dim, like RoutingOverlay's children)
     * and reparented back to DrawnCurveEditor on close.
     * Tapping outside panelRect dismisses it.
     */
    struct ScaleOverlay : public juce::Component
    {
        ScaleOverlay() = default;

        juce::Rectangle<int>  panelRect;
        bool                  _lightMode { true };
        std::function<void()> onDismiss;

        void setLightMode (bool b) { _lightMode = b; repaint(); }

        void paint (juce::Graphics& g) override
        {
            // Dim full background — scale components (children) paint on top.
            g.setColour (juce::Colour (_lightMode ? 0x60000000u : 0x80000000u));
            g.fillRect (getLocalBounds());

            if (! panelRect.isEmpty())
            {
                const auto panelBg = _lightMode ? juce::Colour (0xfffaf8f6u)
                                                : juce::Colour (0xff1e1e2cu);
                g.setColour (panelBg);
                g.fillRoundedRectangle (panelRect.toFloat(), 12.0f);
                g.setColour (juce::Colour (_lightMode ? 0x33000000u : 0x33ffffffu));
                g.drawRoundedRectangle (panelRect.toFloat().reduced (0.5f), 12.0f, 1.0f);
            }
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (onDismiss && ! panelRect.contains (e.getPosition()))
                onDismiss();
        }

        // Always true: scale browser children handle their own events inside the panel;
        // outside-panel taps fall through to mouseDown() for dismiss.
        bool hitTest (int, int) override { return true; }

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScaleOverlay)
    };

    ScaleOverlay _scaleOverlay;

    // ── Standalone MIDI output ─────────────────────────────────────────────────
    /// Button shown in the utility bar (standalone only); label = device name or "MIDI Out".
    juce::TextButton midiOutBtn { "MIDI Out" };
    /// Virtual MIDI source port — created once on standalone launch, kept alive so other apps
    /// retain their connection.  Muted/unmuted via _virtualPortMuted (stops sending, not destroyed).
    std::unique_ptr<juce::MidiOutput> _virtualMidiPort;
    bool _virtualPortMuted { false };
    /// Optional direct-target device (user picks from popup; nullptr = virtual port only).
    std::unique_ptr<juce::MidiOutput> _standaloneOut;
    /// Show a PopupMenu of available MIDI output devices.
    void showMidiOutputPicker();
    /// Update button label to reflect current virtual port + direct target state.
    void updateMidiOutBtnText();

    // ── Private helpers ───────────────────────────────────────────────────────
    void setupSlider (juce::Slider& s, juce::Label& l,
                      const juce::String& labelText,
                      juce::Slider::SliderStyle style = juce::Slider::LinearHorizontal);

    void applyTheme();
    void openScaleOverlay();
    void closeScaleOverlay();
    void onSyncToggled    (bool isSync);

    /// Rebind shaping sliders + range slider to the given lane's APVTS params.
    void bindShapingToLane (int lane);

    /// Set _focusedLane, update canvas, rebind shaping, refresh note editor.
    void setFocusedLane (int lane);

    void updateMsgTypeButtons();      // kept for backward compat, delegates to lane row
    void updateLaneRow      (int lane);   ///< Refresh target/detail/channel display for one lane
    void updateAllLaneRows  ();           ///< Refresh all active lane rows

    /// Rebuild laneFocusCtrl segments to match proc.activeLaneCount.
    void updateLaneFocusCtrl();

    /// Increment proc.activeLaneCount (up to kMaxLanes), refresh layout.
    void addLane();
    /// Remove lane 'lane' (shift down), adjust focus, refresh layout.
    void deleteLane (int lane);
    void updateScaleVisibility();
    void setActiveFamily (int familyIdx);     ///< Populate subfamily chips for the chosen family.
    void addRecentMask   (uint16_t relMask);  ///< Push rel mask to recent history; refresh if tab active.
    void updateScalePresetButtons();          ///< Colour-only refresh (uses cached recognition).
    void updateScaleStatus();                 ///< Full refresh: recognition → cache → labels → colours.
    void updateRangeSlider();
    void updateRangeLabel();
    void applyRangeLabelText (const juce::String& text);
    void updateSpeedLabel();

    /// Refresh both tick-count labels and their step-size annotations.
    void refreshTickLabels();
    /// Step size annotation text for Y axis (semitones in Note mode, else empty).
    juce::String yTickText (int n) const;
    /// Step size annotation text for X axis (seconds in free mode, beats in sync mode).
    juce::String xTickText (int n) const;

    void parameterChanged (const juce::String& paramID, float newValue) override;

    friend class RoutingOverlay;   ///< Needs access to proc, laneTypeBtn, updateLaneRow(), etc.

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrawnCurveEditor)
};
