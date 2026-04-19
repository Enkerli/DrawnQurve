#pragma once

/**
 * @file ScaleLattice.h
 *
 * ScaleLattice — interactive 5+7 pitch-class lattice for the scale quantizer UI.
 *
 * Layout: two rows of equal-size circles — the "neutral 5+7" layout
 * (no piano-key skeuomorphism; natural and chromatic notes as peers).
 *
 *   Bottom row (7 natural notes):  C  D  E  F  G  A  B
 *   Top row    (5 chromatic notes): C# D#    F# G# A#
 *                                   (gap between E and F — no chromatic there)
 *
 * Spacing: gap between adjacent circles = 0.5 × radius (≈ 25 % of diameter).
 * Centre-to-centre in the bottom row = 2.5 × radius.
 * Because each chromatic is placed at the midpoint of its two flanking naturals,
 * every C–C#–D (and F–F#–G, etc.) triple forms an equilateral triangle.
 *
 * Interaction:
 *   Short tap     → toggle pitch class in / out of the custom scale mask.
 *   Long press    → set that pitch class as the scale root (outer ring appears).
 *   Double-tap    → also sets the root.  Matches the webapp's usePressGesture
 *                   shared between rows and wheel — second tap on the same node
 *                   within kDoubleTapMs is treated as "set root", not as a
 *                   second toggle.  The first tap's mask change is reverted so
 *                   double-tapping a previously-inactive note doesn't leave it
 *                   toggled in addition to becoming the root.
 *
 * Integration with APVTS:
 *   setMask(uint16_t) / setRoot(int)  — called from parameterChanged(); no callbacks fired.
 *   onMaskChanged / onRootChanged     — fired on user interaction; caller writes to APVTS.
 *
 * All colour members are public so applyTheme() in PluginEditor can set them
 * directly without any method overhead; the component repaints on setMask/setRoot.
 *
 * Header-only — no separate .cpp required.
 */

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <functional>
#include <cmath>

//==============================================================================
class ScaleLattice : public juce::Component,
                     public juce::Timer
{
public:
    // ── Callbacks (assigned by PluginEditor) ──────────────────────────────────

    /// Called when the user short-taps a node (toggles the mask bit).
    std::function<void(uint16_t)> onMaskChanged;

    /// Called when the user long-presses a node (sets the root).
    std::function<void(int)> onRootChanged;

    // ── Root-select mode ──────────────────────────────────────────────────────
    // When active, the next short tap sets the root instead of toggling the mask.
    // The mode is deactivated automatically after a root is set.
    // A visual amber border indicates the mode is active.

    void setRootSelectMode (bool on) { _rootSelectMode = on; repaint(); }
    bool isRootSelectMode  () const  { return _rootSelectMode; }

    // ── Notation ─────────────────────────────────────────────────────────────
    /// false = sharp names (C♯ D♯ F♯ G♯ A♯), true = flat names (D♭ E♭ G♭ A♭ B♭).
    void setUseFlats (bool b) { _useFlats = b; repaint(); }
    bool getUseFlats () const { return _useFlats; }

    // ── Colours — assign from applyTheme() ───────────────────────────────────

    juce::Colour colBg           { 0xffFFFFFF };  ///< Inactive node fill
    juce::Colour colBorder       { 0xffA9BAD5 };  ///< Inactive node border
    juce::Colour colTextOff      { 0xffA9A59B };  ///< Label on an inactive node

    juce::Colour colActive       { 0xffDCFCE7 };  ///< Active (non-root) fill — light green tint
    juce::Colour colActiveBorder { 0xff22C55E };  ///< Active (non-root) border
    juce::Colour colTextOn       { 0xff166534 };  ///< Label on an active non-root node

    juce::Colour colRoot         { 0xffFEF3C7 };  ///< Root node fill — amber tint
    juce::Colour colRootBorder   { 0xffF59E0B };  ///< Root node border
    juce::Colour colRootRing     { 0xffFBBF24 };  ///< Root indicator ring stroke
    juce::Colour colRootText     { 0xff92400E };  ///< Label on the root node

    /// colAccent kept for backward compat — unused in paint(); set colRootRing instead.
    juce::Colour colAccent       { 0xffFBBF24 };

    ScaleLattice() = default;

    // ── API ──────────────────────────────────────────────────────────────────

    /// Update the active-note bitmask without firing onMaskChanged.
    void setMask (uint16_t mask) { _mask = mask; repaint(); }

    /// Update the root pitch class (0–11) without firing onRootChanged.
    void setRoot (int root) { _root = juce::jlimit (0, 11, root); repaint(); }

    // ── juce::Component overrides ─────────────────────────────────────────────

    void paint     (juce::Graphics& g)          override;
    void resized   ()                            override { buildLayout(); }
    void mouseDown (const juce::MouseEvent& e)  override;
    void mouseUp   (const juce::MouseEvent& e)  override;

    // ── juce::Timer override ─────────────────────────────────────────────────

    void timerCallback() override;

private:
    struct Node
    {
        int   pc;    ///< Pitch class 0–11
        float cx, cy, r;
        int   idx;   ///< 0–6 for naturals, 0–4 for chromatics
        bool  nat;   ///< true = natural note, false = chromatic
    };

    uint16_t _mask           { 0xFFF };
    int      _root           { 0 };
    int      _pressed        { -1 };
    bool     _longFired      { false };
    bool     _rootSelectMode { false };
    bool     _useFlats       { false };

    // Double-tap detection (matches webapp's usePressGesture).
    // _lastTapMs is juce::Time::getMillisecondCounter() at the previous tap;
    // _lastTapNode is the index that was tapped.  A new tap on the same node
    // within kDoubleTapMs upgrades to "set root".
    static constexpr unsigned int kLongPressMs = 500;
    static constexpr unsigned int kDoubleTapMs = 350;
    juce::uint32 _lastTapMs   { 0 };
    int          _lastTapNode { -1 };

    std::vector<Node> _nodes;   ///< [0..6] = naturals, [7..11] = chromatics

    void buildLayout();
    int  nodeAt (float x, float y) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScaleLattice)
};

//==============================================================================
inline void ScaleLattice::buildLayout()
{
    _nodes.clear();

    const float W = static_cast<float> (getWidth());
    const float H = static_cast<float> (getHeight());
    if (W < 1.0f || H < 1.0f) return;

    // ── Geometry ─────────────────────────────────────────────────────────────
    //
    // Gap between adjacent circles in the bottom row = 0.5 r  (≈ 25 % diameter).
    // Therefore centre-to-centre spacing natStep = 2.5 r.
    //
    // Chromatics sit at horizontal midpoints between their flanking naturals,
    // so chrX = (natX[left] + natX[right]) / 2.  The vertical separation for
    // an equilateral triangle with side = natStep is  natStep × √3/2.
    //
    // Radius is capped by:
    //   height  → rFromH: both rows of circles + root-ring overhang fit in H
    //   width   → rFromW: 7 naturals at 2.5 r spacing fit in W with 2 px margins

    static constexpr float kSqrt3      = 1.7320508f;
    static constexpr float kRingOvhg   = 4.0f;   // root ring extends this far beyond the node edge
    static constexpr float kEdgeMargin = kRingOvhg + 1.0f;  // min gap from component edge to ring

    // Vertical: chrY = r + edgeMargin,  natY = chrY + natStep*√3/2
    //           natY + r + edgeMargin ≤ H
    //           ⟹  r*(2 + 2.5*√3/2) ≤ H - 2*edgeMargin
    const float rFromH = (H - 2.0f * kEdgeMargin) / (2.0f + 2.5f * kSqrt3 * 0.5f);

    // Horizontal: 6 gaps × natStep + 2 × (r + 2 px edge) ≤ W
    //             15r + 2r + 4 ≤ W  ⟹  r ≤ (W-4)/17
    const float rFromW = (W - 4.0f) / 17.0f;

    const float r      = juce::jmin (rFromH, rFromW);
    const float natStep = 2.5f * r;
    const float rowSep  = natStep * kSqrt3 * 0.5f;  // equilateral triangle height

    const float chrY   = r + kEdgeMargin;
    const float natY   = chrY + rowSep;

    // Centre the 7 naturals horizontally.
    const float margin = (W - 6.0f * natStep) * 0.5f;

    // ── Node data ─────────────────────────────────────────────────────────────

    // Natural notes (bottom row)
    static const int kNatPC   [7] = {  0,  2,  4,  5,  7,  9, 11 };
    // Chromatic notes (top row)
    static const int kChrPC   [5] = {  1,  3,  6,  8, 10 };
    static const int kChrLeft [5] = { 0, 1, 3, 4, 5 };
    static const int kChrRight[5] = { 1, 2, 4, 5, 6 };

    // Naturals first (rendered below, lose hit tests to overlapping chromatics)
    float natX[7] = {};
    for (int i = 0; i < 7; ++i)
    {
        natX[i] = margin + static_cast<float>(i) * natStep;
        _nodes.push_back ({ kNatPC[i], natX[i], natY, r, i, true });
    }

    // Chromatics last (rendered on top, win hit tests)
    for (int j = 0; j < 5; ++j)
    {
        const float cx = (natX[kChrLeft[j]] + natX[kChrRight[j]]) * 0.5f;
        _nodes.push_back ({ kChrPC[j], cx, chrY, r, j, false });
    }
}

//==============================================================================
inline int ScaleLattice::nodeAt (float x, float y) const
{
    // Iterate backwards: chromatics (end of vector) win over naturals on overlap.
    for (int i = static_cast<int> (_nodes.size()) - 1; i >= 0; --i)
    {
        const auto& n = _nodes[static_cast<size_t>(i)];
        const float dx = x - n.cx;
        const float dy = y - n.cy;
        if (dx * dx + dy * dy <= n.r * n.r)
            return i;
    }
    return -1;
}

//==============================================================================
inline void ScaleLattice::mouseDown (const juce::MouseEvent& e)
{
    _pressed   = nodeAt (e.position.x, e.position.y);
    _longFired = false;
    // In root-select mode (◆ button active), any short tap sets the root —
    // suppress the long-press timer to avoid the two paths firing at once.
    if (_pressed >= 0 && !_rootSelectMode)
        startTimer (static_cast<int> (kLongPressMs));
}

inline void ScaleLattice::timerCallback()
{
    stopTimer();
    if (_pressed >= 0 && !_longFired)
    {
        _longFired = true;
        _root = _nodes[static_cast<size_t> (_pressed)].pc;
        // Long-press counts as a fresh "set root" event, not a tap — reset
        // the double-tap window so a follow-up tap toggles cleanly.
        _lastTapMs   = 0;
        _lastTapNode = -1;
        repaint();
        if (onRootChanged) onRootChanged (_root);
    }
}

inline void ScaleLattice::mouseUp (const juce::MouseEvent& /*e*/)
{
    stopTimer();
    if (_pressed >= 0 && !_longFired)
    {
        if (_rootSelectMode)
        {
            // Root-select mode: set root instead of toggling mask.
            _root = _nodes[static_cast<size_t> (_pressed)].pc;
            _rootSelectMode = false;   // one-shot: deactivate after use
            _lastTapMs   = 0;
            _lastTapNode = -1;
            repaint();
            if (onRootChanged) onRootChanged (_root);
        }
        else
        {
            const auto now = juce::Time::getMillisecondCounter();
            const bool isDoubleTap = (_pressed == _lastTapNode)
                                  && (_lastTapMs != 0)
                                  && ((now - _lastTapMs) < kDoubleTapMs);
            const uint16_t bit = static_cast<uint16_t> (1u << (11 - _nodes[static_cast<size_t> (_pressed)].pc));

            if (isDoubleTap)
            {
                // Revert the first tap's mask flip so the visible state stays
                // consistent — double-tap is a "set root" gesture, not a toggle.
                _mask ^= bit;
                _root = _nodes[static_cast<size_t> (_pressed)].pc;
                _lastTapMs   = 0;
                _lastTapNode = -1;
                repaint();
                if (onMaskChanged) onMaskChanged (_mask);
                if (onRootChanged) onRootChanged (_root);
            }
            else
            {
                // Bitmask convention: bit (11 - pc) = pitch class pc active.
                _mask ^= bit;
                _lastTapMs   = now;
                _lastTapNode = _pressed;
                repaint();
                if (onMaskChanged) onMaskChanged (_mask);
            }
        }
    }
    _pressed = -1;
}

//==============================================================================
inline void ScaleLattice::paint (juce::Graphics& g)
{
    if (_nodes.empty()) return;

    // ── Note name tables ──────────────────────────────────────────────────────
    // Natural names are ASCII; chromatic names use charToString for ♯/♭ (U+266F/U+266D)
    // so they go through JUCE's internal Unicode storage, not the ASCII String ctor.
    static const char* const kNatName[7] = { "C","D","E","F","G","A","B" };
    // Chromatic labels constructed at call time from the toggle state (see below).

    // Root-select mode: amber border hints that the next tap sets the root.
    if (_rootSelectMode)
    {
        g.setColour (juce::Colour (0xffFBBF24).withAlpha (0.75f));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 5.0f, 2.5f);
    }

    static constexpr float kRingOvhg = 4.0f;

    // Sharp: C♯ D♯ F♯ G♯ A♯   Flat: D♭ E♭ G♭ A♭ B♭
    static const char  kChrSharpRoot[5] = { 'C','D','F','G','A' };
    static const char  kChrFlatRoot [5] = { 'D','E','G','A','B' };
    const juce::juce_wchar kAccSharp = 0x266F;   // ♯
    const juce::juce_wchar kAccFlat  = 0x266D;   // ♭

    for (const auto& n : _nodes)
    {
        // Bitmask convention: bit (11 - pc) = pitch class pc active.
        const bool active = ((_mask >> (11 - n.pc)) & 1) != 0;
        const bool isRoot = (n.pc == _root);
        const bool isNat  = n.nat;
        const juce::Rectangle<float> circ (n.cx - n.r, n.cy - n.r,
                                            n.r * 2.0f, n.r * 2.0f);

        // ── Root indicator ring (behind the node) ────────────────────────────
        if (isRoot)
        {
            const float ringR = n.r + kRingOvhg;
            g.setColour (colRootRing);
            g.drawEllipse (n.cx - ringR, n.cy - ringR,
                           ringR * 2.0f, ringR * 2.0f, 2.5f);
        }

        // ── Node fill ────────────────────────────────────────────────────────
        if (isRoot)
        {
            g.setColour (colRoot);
            g.fillEllipse (circ);
            g.setColour (colRootBorder);
            g.drawEllipse (circ, 1.5f);
        }
        else if (active)
        {
            g.setColour (colActive);
            g.fillEllipse (circ);
            g.setColour (colActiveBorder);
            g.drawEllipse (circ, 1.5f);
        }
        else
        {
            g.setColour (colBg);
            g.fillEllipse (circ);
            g.setColour (colBorder);
            g.drawEllipse (circ, 1.2f);
        }

        // ── Note name label ───────────────────────────────────────────────────
        const juce::Colour textCol = isRoot  ? colRootText
                                   : active  ? colTextOn
                                             : colTextOff;
        g.setColour (textCol);
        const float fontSize = juce::jmax (9.0f, n.r * 0.62f);
        g.setFont (juce::Font (juce::FontOptions{}.withHeight (fontSize).withStyle ("Bold")));

        juce::String label;
        if (isNat)
        {
            label = juce::String (kNatName[n.idx]);
        }
        else
        {
            const int ci = n.idx;   // 0–4 chromatic index
            const char rootChar = _useFlats ? kChrFlatRoot[ci] : kChrSharpRoot[ci];
            label = juce::String::charToString (static_cast<juce::juce_wchar> (rootChar))
                  + juce::String::charToString (_useFlats ? kAccFlat : kAccSharp);
        }
        g.drawFittedText (label, circ.toNearestInt(), juce::Justification::centred, 1);
    }
}
