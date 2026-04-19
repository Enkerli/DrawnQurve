#pragma once

/**
 * @file ChromaticWheel.h
 *
 * ChromaticWheel — true 12-PC wheel companion to ScaleLattice.
 *
 * Layout: 12 pitch-class nodes evenly spaced around a circle (C at top, going
 * clockwise C, C♯, D, D♯, … B).  Active PCs are joined by a straight-edge
 * polygon whose silhouette reveals the scale's interval geometry — equilateral
 * for augmented triads, near-symmetric Stars-of-David for whole-tone, etc.
 *
 *   • Inner guide ring + 12 spokes for visual reference
 *   • Active PCs: filled ink dot.  Inactive: paper-tone dot, ink stroke.
 *   • Root PC: amber ring around the dot (matches ScaleLattice's amber hint)
 *   • Centre text: italic Domine root name + sans-serif "N notes" caption
 *
 * Interaction matches ScaleLattice's piano rows exactly so the two can sit
 * side-by-side and feel like one instrument:
 *   Short tap     → toggle pitch class in / out of the custom scale mask.
 *   Long press    → set that pitch class as the scale root.
 *   Double-tap    → also sets the root (revertes the first tap's mask flip).
 *
 * API mirrors ScaleLattice 1-to-1 so both components can be driven from the
 * same callbacks/parameter wiring without conditionals.
 *
 * Header-only — no separate .cpp required.
 */

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <functional>
#include <cmath>

//==============================================================================
class ChromaticWheel : public juce::Component,
                       public juce::Timer
{
public:
    // ── Callbacks (assigned by PluginEditor) ──────────────────────────────────
    std::function<void(uint16_t)> onMaskChanged;
    std::function<void(int)>      onRootChanged;

    // ── Root-select mode (matches ScaleLattice) ──────────────────────────────
    void setRootSelectMode (bool on) { _rootSelectMode = on; repaint(); }
    bool isRootSelectMode  () const  { return _rootSelectMode; }

    // ── Notation ─────────────────────────────────────────────────────────────
    void setUseFlats (bool b) { _useFlats = b; repaint(); }
    bool getUseFlats () const { return _useFlats; }

    // ── Colours — assign from applyTheme(); same naming as ScaleLattice ──────
    juce::Colour colBg           { 0xffFFFFFF };  ///< Inactive node fill
    juce::Colour colBorder       { 0xff2C2723 };  ///< Inactive node stroke
    juce::Colour colTextOff      { 0xffAAA195 };  ///< Inactive label
    juce::Colour colActive       { 0xff2C2723 };  ///< Active node fill (ink)
    juce::Colour colActiveBorder { 0xff2C2723 };  ///< Active node stroke
    juce::Colour colTextOn       { 0xff2C2723 };  ///< Active label
    juce::Colour colRoot         { 0xff2C2723 };  ///< Root node fill (same as active)
    juce::Colour colRootBorder   { 0xffCB9839 };  ///< Root node ring stroke (amber)
    juce::Colour colRootRing     { 0xffCB9839 };  ///< Root indicator dashed ring
    juce::Colour colRootText     { 0xff92400E };  ///< Root label
    juce::Colour colSpoke        { 0xffD4CDB7 };  ///< Inner guide ring + spoke colour
    juce::Colour colPolygonStroke{ 0xffCB9839 };  ///< Polygon outline (amber accent)
    juce::Colour colPolygonFill  { 0x30CB9839 };  ///< Polygon fill (~19 % amber)
    juce::Colour colCentreInk    { 0xff2C2723 };  ///< Centre root-name text
    juce::Colour colCentreSub    { 0xff837B6F };  ///< Centre "N notes" caption

    ChromaticWheel() = default;

    // ── API ──────────────────────────────────────────────────────────────────
    void setMask (uint16_t mask) { _mask = mask; repaint(); }
    void setRoot (int root)      { _root = juce::jlimit (0, 11, root); repaint(); }

    // ── juce::Component overrides ─────────────────────────────────────────────
    void paint     (juce::Graphics& g)         override;
    void resized   ()                          override { buildLayout(); }
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseUp   (const juce::MouseEvent& e) override;

    // ── juce::Timer override ─────────────────────────────────────────────────
    void timerCallback() override;

private:
    struct Node
    {
        int   pc;    ///< Pitch class 0–11
        float cx, cy;
        float lx, ly;   ///< Label position (further out from centre)
    };

    uint16_t _mask           { 0xFFF };
    int      _root           { 0 };
    int      _pressed        { -1 };
    bool     _longFired      { false };
    bool     _rootSelectMode { false };
    bool     _useFlats       { false };

    // Same gesture timing as ScaleLattice — keeps the two surfaces feeling
    // identical when the user moves between rows and wheel.
    static constexpr unsigned int kLongPressMs = 500;
    static constexpr unsigned int kDoubleTapMs = 350;
    juce::uint32 _lastTapMs   { 0 };
    int          _lastTapNode { -1 };

    // Geometry — recomputed by buildLayout().
    std::vector<Node>     _nodes;
    juce::Point<float>    _centre;
    float                 _midR    { 0.0f };  ///< Node ring radius
    float                 _innerR  { 0.0f };  ///< Inner guide ring
    float                 _nodeR   { 6.0f };  ///< Drawn node radius (active)
    float                 _hitR    { 12.0f }; ///< Click hit radius

    void buildLayout();
    int  nodeAt (float x, float y) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChromaticWheel)
};

//==============================================================================
inline void ChromaticWheel::buildLayout()
{
    _nodes.clear();

    const float W = static_cast<float> (getWidth());
    const float H = static_cast<float> (getHeight());
    if (W < 1.0f || H < 1.0f) return;

    // Reserve room for outer labels.  Label rectangle is 22w × 14h centred on
    // (lx, ly).  Two competing constraints at the 3 / 9 o'clock cells:
    //   (a) labels must NOT clip the component edge:
    //         labelR + 11 ≤ W/2  →  labelR − radius ≤ labelOvhg − 11
    //   (b) labels must NOT overlap the node circles (nodeR up to 8):
    //         labelR − 11 ≥ radius + nodeR  →  labelR − radius ≥ nodeR + 11
    // Combining (a) + (b) gives  labelOvhg ≥ nodeR + 22 = 30 minimum.
    // We use labelOvhg = 32 with labelR = radius + 20 for a 1-px margin on
    // each side at maximum wheel size (margins grow at smaller sizes since
    // nodeR is jlimit-capped at 8).  Vertical (12 / 6 o'clock) clearance
    // works out to ~5 px on each axis.
    const float labelOvhg = 32.0f;
    const float side      = juce::jmin (W, H) - 2.0f * labelOvhg;
    const float radius    = side * 0.5f;

    _centre = { W * 0.5f, H * 0.5f };
    _midR   = radius;
    _innerR = radius * 0.55f;
    _nodeR  = juce::jlimit (4.0f, 8.0f, radius * 0.085f);
    _hitR   = juce::jmax (12.0f, _nodeR * 2.2f);

    // Place labels 20 px past the node-ring radius — far enough that the
    // label rect's inner edge clears the node (radius + nodeR_max = +8) by
    // ≥ 1 px, and the outer edge stays inside the component (labelOvhg − 11
    // = 21, leaving 1 px of breathing room before the bounding box).
    const float labelR = radius + 20.0f;

    for (int pc = 0; pc < 12; ++pc)
    {
        // C at top, clockwise.  Angle 0 = +x axis, so subtract π/2 to put C up.
        const float a = (static_cast<float> (pc) / 12.0f) * juce::MathConstants<float>::twoPi
                      - juce::MathConstants<float>::halfPi;
        Node n;
        n.pc = pc;
        n.cx = _centre.x + _midR  * std::cos (a);
        n.cy = _centre.y + _midR  * std::sin (a);
        n.lx = _centre.x + labelR * std::cos (a);
        n.ly = _centre.y + labelR * std::sin (a);
        _nodes.push_back (n);
    }
}

//==============================================================================
inline int ChromaticWheel::nodeAt (float x, float y) const
{
    for (size_t i = 0; i < _nodes.size(); ++i)
    {
        const auto& n  = _nodes[i];
        const float dx = x - n.cx;
        const float dy = y - n.cy;
        if (dx * dx + dy * dy <= _hitR * _hitR)
            return static_cast<int> (i);
    }
    return -1;
}

//==============================================================================
inline void ChromaticWheel::mouseDown (const juce::MouseEvent& e)
{
    _pressed   = nodeAt (e.position.x, e.position.y);
    _longFired = false;
    if (_pressed >= 0 && !_rootSelectMode)
        startTimer (static_cast<int> (kLongPressMs));
}

inline void ChromaticWheel::timerCallback()
{
    stopTimer();
    if (_pressed >= 0 && !_longFired)
    {
        _longFired = true;
        _root = _nodes[static_cast<size_t> (_pressed)].pc;
        _lastTapMs   = 0;
        _lastTapNode = -1;
        repaint();
        if (onRootChanged) onRootChanged (_root);
    }
}

inline void ChromaticWheel::mouseUp (const juce::MouseEvent& /*e*/)
{
    stopTimer();
    if (_pressed >= 0 && !_longFired)
    {
        if (_rootSelectMode)
        {
            _root = _nodes[static_cast<size_t> (_pressed)].pc;
            _rootSelectMode = false;
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
                _mask ^= bit;   // revert first tap's flip
                _root = _nodes[static_cast<size_t> (_pressed)].pc;
                _lastTapMs   = 0;
                _lastTapNode = -1;
                repaint();
                if (onMaskChanged) onMaskChanged (_mask);
                if (onRootChanged) onRootChanged (_root);
            }
            else
            {
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
inline void ChromaticWheel::paint (juce::Graphics& g)
{
    if (_nodes.empty()) return;

    // Root-select mode — amber border hint (parity with ScaleLattice).
    if (_rootSelectMode)
    {
        g.setColour (colRootRing.withAlpha (0.75f));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 5.0f, 2.5f);
    }

    // ── Guide rings + spokes ─────────────────────────────────────────────────
    g.setColour (colSpoke);
    g.drawEllipse (_centre.x - _midR,   _centre.y - _midR,   _midR   * 2.0f, _midR   * 2.0f, 0.5f);
    g.drawEllipse (_centre.x - _innerR, _centre.y - _innerR, _innerR * 2.0f, _innerR * 2.0f, 0.5f);

    for (int pc = 0; pc < 12; ++pc)
    {
        const float a = (static_cast<float> (pc) / 12.0f) * juce::MathConstants<float>::twoPi
                      - juce::MathConstants<float>::halfPi;
        const float x1 = _centre.x + _innerR             * std::cos (a);
        const float y1 = _centre.y + _innerR             * std::sin (a);
        const float x2 = _centre.x + (_midR - _nodeR * 1.6f) * std::cos (a);
        const float y2 = _centre.y + (_midR - _nodeR * 1.6f) * std::sin (a);
        g.drawLine (x1, y1, x2, y2, 0.5f);
    }

    // ── Scale polygon ────────────────────────────────────────────────────────
    // Walk PCs in order; collect only the active ones; close the path.
    std::vector<juce::Point<float>> polyPts;
    polyPts.reserve (12);
    for (const auto& n : _nodes)
        if (((_mask >> (11 - n.pc)) & 1) != 0)
            polyPts.push_back ({ n.cx, n.cy });

    if (polyPts.size() >= 2)
    {
        juce::Path poly;
        poly.startNewSubPath (polyPts[0]);
        for (size_t i = 1; i < polyPts.size(); ++i)
            poly.lineTo (polyPts[i]);
        poly.closeSubPath();

        g.setColour (colPolygonFill);
        g.fillPath (poly);
        g.setColour (colPolygonStroke);
        g.strokePath (poly, juce::PathStrokeType (1.5f,
                      juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));
    }

    // ── Nodes + labels ───────────────────────────────────────────────────────
    static const char* const kNatName[12] = {
        "C", nullptr, "D", nullptr, "E", "F", nullptr, "G", nullptr, "A", nullptr, "B"
    };
    static const char  kChrSharpRoot[12] = {
        0,'C',0,'D',0,0,'F',0,'G',0,'A',0
    };
    static const char  kChrFlatRoot[12]  = {
        0,'D',0,'E',0,0,'G',0,'A',0,'B',0
    };
    const juce::juce_wchar kAccSharp = 0x266F;   // ♯
    const juce::juce_wchar kAccFlat  = 0x266D;   // ♭

    for (const auto& n : _nodes)
    {
        const bool active = ((_mask >> (11 - n.pc)) & 1) != 0;
        const bool isRoot = (n.pc == _root);
        const float r     = active ? _nodeR : _nodeR * 0.72f;

        // Root indicator ring (dashed look approximated with a thin solid ring
        // to keep the JUCE Graphics path simple).
        if (isRoot)
        {
            const float ringR = _nodeR * 1.9f;
            g.setColour (colRootRing);
            g.drawEllipse (n.cx - ringR, n.cy - ringR, ringR * 2.0f, ringR * 2.0f, 1.0f);
        }

        // Node fill + stroke
        const juce::Rectangle<float> dot (n.cx - r, n.cy - r, r * 2.0f, r * 2.0f);
        if (active)
        {
            g.setColour (isRoot ? colRoot : colActive);
            g.fillEllipse (dot);
            g.setColour (isRoot ? colRootBorder : colActiveBorder);
            g.drawEllipse (dot, isRoot ? 1.5f : 1.0f);
        }
        else
        {
            g.setColour (colBg);
            g.fillEllipse (dot);
            g.setColour (isRoot ? colRootBorder : colBorder);
            g.drawEllipse (dot, isRoot ? 1.5f : 1.0f);
        }

        // Label (italic serif — matches webapp).
        juce::String label;
        if (kNatName[n.pc] != nullptr)
        {
            label = juce::String (kNatName[n.pc]);
        }
        else
        {
            const char rootChar = _useFlats ? kChrFlatRoot[n.pc] : kChrSharpRoot[n.pc];
            label = juce::String::charToString (static_cast<juce::juce_wchar> (rootChar))
                  + juce::String::charToString (_useFlats ? kAccFlat : kAccSharp);
        }

        const auto labelCol = isRoot ? colRootText : (active ? colTextOn : colTextOff);
        g.setColour (labelCol);
        // Use sentinel name so DrawnCurveLookAndFeel routes to bundled Domine.
        g.setFont (juce::Font (juce::FontOptions{}
                                  .withName ("DC-Serif")
                                  .withHeight (11.0f)
                                  .withFallbackEnabled (true))
                       .italicised());
        const auto labelRect = juce::Rectangle<float> (n.lx - 11.0f, n.ly - 7.0f, 22.0f, 14.0f);
        g.drawText (label, labelRect.toNearestInt(), juce::Justification::centred, false);
    }

    // ── Centre text — root name (italic serif) + N-notes caption ─────────────
    int nActive = 0;
    for (int pc = 0; pc < 12; ++pc)
        if (((_mask >> (11 - pc)) & 1) != 0) ++nActive;

    static const char* const kRootSharp[12] = {
        "C","C\u266f","D","D\u266f","E","F","F\u266f","G","G\u266f","A","A\u266f","B"
    };
    static const char* const kRootFlat[12]  = {
        "C","D\u266d","D","E\u266d","E","F","G\u266d","G","A\u266d","A","B\u266d","B"
    };
    const juce::String rootName = juce::String::fromUTF8 (
        (_useFlats ? kRootFlat : kRootSharp)[_root]);

    g.setColour (colCentreInk);
    g.setFont (juce::Font (juce::FontOptions{}
                              .withName ("DC-Serif")
                              .withHeight (14.0f)
                              .withFallbackEnabled (true))
                   .italicised());
    g.drawText (rootName,
                juce::Rectangle<float> (_centre.x - 30.0f, _centre.y - 14.0f, 60.0f, 16.0f).toNearestInt(),
                juce::Justification::centred, false);

    g.setColour (colCentreSub);
    g.setFont (juce::Font (juce::FontOptions{}
                              .withName ("DC-Sans")
                              .withHeight (8.0f)
                              .withFallbackEnabled (true)));
    g.drawText (juce::String (nActive) + " NOTES",
                juce::Rectangle<float> (_centre.x - 30.0f, _centre.y + 2.0f, 60.0f, 12.0f).toNearestInt(),
                juce::Justification::centred, false);
}
