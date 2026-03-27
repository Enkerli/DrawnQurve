/**
 * @file PluginEditor.cpp
 *
 * Implementation of CurveDisplay and DrawnCurveEditor.
 */

#include "PluginEditor.h"

//==============================================================================
// Colour palettes
//==============================================================================

struct Theme
{
    juce::Colour background;
    juce::Colour stageBg;
    juce::Colour gridLine;
    juce::Colour curve;
    juce::Colour capture;
    juce::Colour playhead;
    juce::Colour playheadDot;
    juce::Colour hint;
    juce::Colour border;
    juce::Colour panelBg;
    juce::Colour panelBorder;
};

static const Theme kDark
{
    /* background  */ juce::Colour { 0xff12121f },
    /* stageBg     */ juce::Colour { 0xff0a0a15 },
    /* gridLine    */ juce::Colour { 0x18ffffff },
    /* curve       */ juce::Colour { 0xff00e5ff },
    /* capture     */ juce::Colour { 0xffff6b35 },
    /* playhead    */ juce::Colour { 0xffffffff },
    /* playheadDot */ juce::Colour { 0xff00e5ff },
    /* hint        */ juce::Colour { 0x66ffffff },
    /* border      */ juce::Colour { 0x33ffffff },
    /* panelBg     */ juce::Colour { 0xff1c1c2e },
    /* panelBorder */ juce::Colour { 0x28ffffff },
};

static const Theme kLight
{
    /* background  */ juce::Colour { 0xffF5F2EB },
    /* stageBg     */ juce::Colour { 0xffEFEBE2 },
    /* gridLine    */ juce::Colour { 0x14000000 },
    /* curve       */ juce::Colour { 0xff7A4CFF },
    /* capture     */ juce::Colour { 0xffD95C3A },
    /* playhead    */ juce::Colour { 0xff2d2b27 },
    /* playheadDot */ juce::Colour { 0xff7A4CFF },
    /* hint        */ juce::Colour { 0x99000000 },
    /* border      */ juce::Colour { 0x1E000000 },
    /* panelBg     */ juce::Colour { 0xffFCFBF7 },
    /* panelBorder */ juce::Colour { 0xffDDD6CA },
};

//==============================================================================
// Layout constants
//==============================================================================

namespace Layout
{
    static constexpr int editorW  = 800;
    static constexpr int editorH  = 760;
    static constexpr int pad      = 8;
    static constexpr int colGap   = 12;
    static constexpr int rightColW = 296;
    static constexpr int utilityRowH = 36;   // taller to fit sync controls + speed slider
    static constexpr int panelRadius = 14;   // rounded corner radius for right-rail panels
    static constexpr int panelPad    = 10;   // inner padding inside panels

    // Musical zone (bottom, full width)
    static constexpr int musicalCollapsedH = 44;
    static constexpr int musicalExpandedH  = 4 + 30 + 4 + 68 + 4 + 28 + 4 + 100 + 4 + 14 + 8;  // = 268

    // lanesPanelH removed — LANES matrix is now inside the focused lane panel.

    // Stage column inner
    // Left column density steppers
    static constexpr int yStepperW  = 28;
    static constexpr int xStepperH  = 28;

    // Note editor / musical zone — family browser strip heights
    static constexpr int kFamilyBarH    = 30;   // family tab row
    static constexpr int kSubfamilyRowH = 68;   // mode-chip row (name + dot preview)
    static constexpr int kActionRowH    = 28;   // ↻ ● ○ ◑ ◆ + status labels

    static constexpr int paramLabelH  = 14;
    static constexpr int paramSliderH = 30;

    // Routing matrix row geometry (fits 296px right column)
    // dot(12)+gap(4)+target(80)+gap(4)+detail(40)+gap(4)+chan(28)+gap(4)+teach(40)+gap(4)+mute(28) = 248 + margins(10*2) = 268 < 296
    static constexpr int matRowH     = 32;
    static constexpr int matDotW     = 12;
    static constexpr int matTargetW  = 80;
    static constexpr int matDetailW  = 40;
    static constexpr int matChanW    = 28;
    static constexpr int matTeachW   = 40;
    static constexpr int matMuteW    = 28;
    static constexpr int matInnerGap = 4;
}

// ---------------------------------------------------------------------------
// Helper: absolute pitch-class mask for lattice display
// ---------------------------------------------------------------------------

static uint16_t calcAbsLatticeMask (DrawnCurveProcessor& proc, int /*lane*/)
{
    // Scale is now global — lane argument kept for call-site compatibility.
    const int mode = static_cast<int> (proc.apvts.getRawParameterValue ("scaleMode")->load());
    const int root = static_cast<int> (proc.apvts.getRawParameterValue ("scaleRoot")->load());

    if (mode == 7)
        return static_cast<uint16_t> (proc.apvts.getRawParameterValue ("scaleMask")->load());

    const auto sc = proc.getScaleConfig (0);   // global; lane irrelevant
    // Engine mask: bit (11 - interval) = interval present.
    // Lattice mask: bit (11 - abs_pc) = absolute pitch class present.
    uint16_t abs  = 0;
    for (int i = 0; i < 12; ++i)
        if ((sc.mask >> (11 - i)) & 1)
            abs |= static_cast<uint16_t> (1u << (11 - (i + root) % 12));
    return abs;
}

//==============================================================================
// HelpOverlay
//==============================================================================

HelpOverlay::HelpOverlay()
{
    setInterceptsMouseClicks (true, false);
    setVisible (false);
}

void HelpOverlay::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xd0000000));

    const auto bounds = getLocalBounds().toFloat().reduced (24.0f, 20.0f);
    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (juce::FontOptions{}.withHeight (15.0f).withStyle ("Bold")));
    g.drawText ("DrawnCurve  Quick Reference",
                bounds.withHeight (22.0f).toNearestInt(),
                juce::Justification::centred, false);

    struct Entry { const char* label; const char* desc; };
    static const Entry kEntries[] =
    {
        { "CURVE AREA",  "Draw a curve with your finger or Pencil. Time flows left to right; MIDI value top to bottom. Each lane has its own curve." },
        { "Lane 1/2/3",  "Select the active lane. Each lane routes independently to its own MIDI target. Coloured dots show each lane's playhead position." },
        { "Direction",   "Forward, Reverse, or Ping-Pong playback. Tap the active segment to pause; tap again to resume. In SYNC mode, pause persists across host transport changes." },
        { "Clear",       "Erase ALL lane curves and stop playback." },
        { "!  (Panic)",  "Sends All Notes Off on every active channel. Use if notes get stuck." },
        { "Target",      "CC / Channel Pressure / Pitch Bend / Note — set per lane in the routing matrix below the canvas." },
        { "Teach",       "Tap Teach on a lane to solo its output. Other lanes mute so a synth can MIDI-Learn. On CC lanes, the next incoming CC message sets that lane's CC number." },
        { "Mute",        "Silence one lane without erasing its curve." },
        { "Scale",       "In Note mode: choose a scale preset and root note. Use the 12 circles (C to B, left to right) to build a custom scale. Only active pitch classes are played." },
        { "FREE / SYNC", "Toggle host-tempo sync. FREE = manual speed; SYNC = follows host BPM and transport; speed becomes loop length in Beats." },
        { "Smooth",      "Output smoothing (0 = instant). Applied per focused lane. Affects CC and Pitch Bend; bypassed for note-change detection in Note mode." },
        { "Range",       "Output range min/max per lane. In Note mode, shows the note name boundaries." },
        { "Y- / Y+",     "Decrease or increase horizontal grid lines." },
        { "X- / X+",     "Decrease or increase vertical grid lines." },
    };

    const float lineH  = 14.0f;
    const float labelW = 112.0f;
    const float gap    = 6.0f;
    float y = bounds.getY() + 28.0f;

    for (const auto& e : kEntries)
    {
        g.setFont (juce::Font (juce::FontOptions{}.withHeight (11.5f).withStyle ("Bold")));
        g.setColour (juce::Colour (0xff80d8ff));
        g.drawText (e.label,
                    juce::roundToInt (bounds.getX()), juce::roundToInt (y),
                    juce::roundToInt (labelW), juce::roundToInt (lineH * 2),
                    juce::Justification::topRight, false);

        g.setFont (juce::Font (juce::FontOptions{}.withHeight (11.5f)));
        g.setColour (juce::Colours::white);
        g.drawMultiLineText (e.desc,
                             juce::roundToInt (bounds.getX() + labelW + gap),
                             juce::roundToInt (y + 11.5f),
                             juce::roundToInt (bounds.getWidth() - labelW - gap));
        y += lineH * 2 + 2.0f;
        if (y + lineH * 2 > bounds.getBottom() - 18.0f) break;
    }

    g.setFont (juce::Font (juce::FontOptions{}.withHeight (11.0f).withStyle ("Italic")));
    g.setColour (juce::Colours::white.withAlpha (0.6f));
    g.drawText ("Tap anywhere to close", getLocalBounds().withTop (getHeight() - 22),
                juce::Justification::centred, false);
}

void HelpOverlay::mouseDown (const juce::MouseEvent&) { setVisible (false); }

//==============================================================================
// CurveDisplay
//==============================================================================

static constexpr float kAxisMarginL = 36.0f;
static constexpr float kAxisMarginB = 16.0f;

CurveDisplay::CurveDisplay (DrawnCurveProcessor& p) : proc (p) { startTimerHz (30); }
CurveDisplay::~CurveDisplay() { stopTimer(); }
void CurveDisplay::resized() {}
void CurveDisplay::setLightMode (bool light) { _lightMode = light; repaint(); }

void CurveDisplay::paint (juce::Graphics& g)
{
    const Theme& T = _lightMode ? kLight : kDark;

    const float w = static_cast<float> (getWidth());
    const float h = static_cast<float> (getHeight());
    const float plotX = kAxisMarginL, plotY = 0.0f;
    const float plotW = w - kAxisMarginL;
    const float plotH = h - kAxisMarginB;
    const auto  plot  = juce::Rectangle<float> (plotX, plotY, plotW, plotH);

    g.fillAll (T.background);

    // ── Grid ─────────────────────────────────────────────────────────────────
    g.setColour (T.gridLine);
    for (int i = 1; i < _xDivisions; ++i)
        g.drawVerticalLine (juce::roundToInt (plotX + plotW * (float)i / (float)_xDivisions),
                            plotY, plotY + plotH);
    for (int i = 1; i < _yDivisions; ++i)
        g.drawHorizontalLine (juce::roundToInt (plotY + plotH * (float)i / (float)_yDivisions),
                              plotX, plotX + plotW);

    // ── Lane curves ──────────────────────────────────────────────────────────
    // Stroke types per lane: solid / dashed / dot-dash.
    // Draw unfocused lanes first (at 40 % opacity), focused lane on top.
    static const float kDashLen[kMaxLanes][4] = {
        { 0, 0, 0, 0 },          // lane 0: solid (ignored)
        { 10.0f, 5.0f, 0, 0 },   // lane 1: dashed
        { 2.0f, 4.0f, 10.0f, 4.0f }, // lane 2: dot-dash
    };
    static const int kDashCount[kMaxLanes] = { 0, 2, 4 };

    for (int pass = 0; pass < 2; ++pass)
    {
        // pass 0 = unfocused, pass 1 = focused
        for (int lane = 0; lane < kMaxLanes; ++lane)
        {
            const bool isFocused = (lane == _focusedLane);
            if ((pass == 0) == isFocused) continue;  // skip wrong pass

            if (! proc.hasCurve (lane)) continue;

            const auto col = (_lightMode ? kLaneColourLight : kLaneColourDark)[lane];
            const auto drawCol = isFocused ? col : col.withAlpha (0.40f);
            const float strokeW = isFocused ? 2.5f : 1.8f;

            const auto table = proc.getCurveTable (lane);
            juce::Path curvePath;
            for (int i = 0; i < 256; ++i)
            {
                const float cx = plotX + static_cast<float> (i) / 255.0f * plotW;
                const float cy = plotY + (1.0f - table[static_cast<size_t> (i)]) * plotH;
                if (i == 0) curvePath.startNewSubPath (cx, cy);
                else        curvePath.lineTo (cx, cy);
            }

            g.setColour (drawCol);
            if (lane == 0 || kDashCount[lane] == 0)
            {
                g.strokePath (curvePath, juce::PathStrokeType (strokeW,
                    juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
            else
            {
                juce::Path dashed;
                juce::PathStrokeType stroke (strokeW, juce::PathStrokeType::curved,
                                              juce::PathStrokeType::butt);
                stroke.createDashedStroke (dashed, curvePath,
                                            kDashLen[lane], kDashCount[lane]);
                g.fillPath (dashed);
            }
        }
    }

    // ── Live capture trail ────────────────────────────────────────────────────
    if (isCapturing && ! capturePath.isEmpty())
    {
        g.saveState();
        g.reduceClipRegion (plot.toNearestInt());
        g.setColour (T.capture);
        g.strokePath (capturePath, juce::PathStrokeType (2.0f,
            juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.restoreState();
    }

    // ── Playheads — one per lane that has a curve and is enabled ─────────────
    // Each lane draws its own coloured dot on its curve.  The focused lane
    // also draws a thin vertical line so the time position is clear.
    // All lanes share the same speed ratio / direction, so their playheads
    // may be at different X positions if their curves have different durations.
    if (proc.isPlaying() && proc.anyLaneHasCurve())
    {
        const auto* colPalette = _lightMode ? kLaneColourLight : kLaneColourDark;

        for (int L = 0; L < kMaxLanes; ++L)
        {
            if (! proc.hasCurve (L)) continue;

            const float phase = proc.currentPhaseForLane (L);
            const float headX = plotX + phase * plotW;
            const auto  col   = colPalette[L];
            const float alpha = (L == _focusedLane) ? 1.0f : 0.55f;

            // Vertical line: only for the focused lane (cleaner when multiple lanes play)
            if (L == _focusedLane)
            {
                g.setColour (T.playhead.withAlpha (0.65f));
                g.drawVerticalLine (juce::roundToInt (headX), plotY, plotY + plotH);
            }

            // Dot at the curve's current value
            const auto table = proc.getCurveTable (L);
            const int  idx   = juce::jlimit (0, 255, static_cast<int> (phase * 255.0f));
            const float headY = plotY + (1.0f - table[static_cast<size_t> (idx)]) * plotH;
            const float r = (L == _focusedLane) ? 5.0f : 3.5f;
            g.setColour (col.withAlpha (alpha));
            g.fillEllipse (headX - r, headY - r, r * 2.0f, r * 2.0f);

            // Small lane-coloured tick on the left Y-axis so it's clear which
            // lane is at which Y value even when curves overlap.
            g.setColour (col.withAlpha (alpha * 0.8f));
            g.fillRect (plotX - 5.0f, headY - 2.0f, 5.0f, 4.0f);
        }
    }

    // ── "Draw a curve" hint ───────────────────────────────────────────────────
    if (! proc.hasCurve (_focusedLane) && ! isCapturing)
    {
        const auto col = (_lightMode ? kLaneColourLight : kLaneColourDark)[_focusedLane];
        g.setColour (col.withAlpha (0.40f));
        g.setFont (juce::Font (juce::FontOptions{}.withHeight (14.0f)));
        const juce::String hint = "Draw Lane " + juce::String (_focusedLane + 1) + " here";
        g.drawText (hint, plot, juce::Justification::centred, false);
    }

    // ── Axis labels ───────────────────────────────────────────────────────────
    {
        const auto msgParamID = laneParam (_focusedLane, "msgType");
        const auto msgType = static_cast<MessageType> (
            static_cast<int> (proc.apvts.getRawParameterValue (msgParamID)->load()));
        const float minOut = proc.apvts.getRawParameterValue (laneParam (_focusedLane, "minOutput"))->load();
        const float maxOut = proc.apvts.getRawParameterValue (laneParam (_focusedLane, "maxOutput"))->load();

        const float recDur = proc.curveDuration (_focusedLane);
        const float speed  = proc.getEffectiveSpeedRatio();
        const float dur    = (recDur > 0.0f) ? recDur / std::max (speed, 0.001f) : 0.0f;

        static const char* kNoteNamesSharp[] = { "C","C\u266f","D","D\u266f","E","F","F\u266f","G","G\u266f","A","A\u266f","B" };
        static const char* kNoteNamesFlat [] = { "C","D\u266d","D","E\u266d","E","F","G\u266d","G","A\u266d","A","B\u266d","B" };
        auto midiNoteName = [&] (int note) -> juce::String {
            const auto* names = _useFlats ? kNoteNamesFlat : kNoteNamesSharp;
            return juce::String::fromUTF8 (names[note % 12]) + juce::String (note / 12 - 1);
        };

        const bool isNote = (msgType == MessageType::Note);
        const ScaleConfig sc = isNote ? proc.getScaleConfig (_focusedLane) : ScaleConfig{};
        const bool hasScale  = isNote && (sc.mask != 0xFFF);

        auto normToY    = [&] (float norm) { return plotY + (1.0f - norm) * plotH; };
        auto noteToNorm = [&] (int n) {
            return (static_cast<float> (n) / 127.0f - minOut)
                   / std::max (maxOut - minOut, 0.001f);
        };

        if (hasScale)
        {
            const int loNote = std::max (0,   juce::roundToInt (minOut * 127.0f) - 1);
            const int hiNote = std::min (127, juce::roundToInt (maxOut * 127.0f) + 1);
            struct BandNote { int note; float y; };
            std::vector<BandNote> visible;
            visible.reserve (24);
            for (int n = hiNote; n >= loNote; --n)
            {
                const int interval = ((n % 12) - (int)sc.root + 12) % 12;
                if ((sc.mask >> (11 - interval)) & 1)
                {
                    const float norm = noteToNorm (n);
                    if (norm >= -0.05f && norm <= 1.05f)
                        visible.push_back ({ n, normToY (norm) });
                }
            }
            if (visible.size() >= 2)
            {
                for (size_t i = 0; i < visible.size(); ++i)
                {
                    const float noteY  = visible[i].y;
                    const float halfUp = (i == 0) ? (noteY - plotY) * 0.5f
                                                  : (visible[i-1].y - noteY) * 0.5f;
                    const float halfDn = (i+1 < visible.size()) ? (noteY - visible[i+1].y) * 0.5f
                                                                 : (plotY + plotH - noteY) * 0.5f;
                    g.setColour ((i & 1) ? T.gridLine.withAlpha (0.08f)
                                         : T.gridLine.withAlpha (0.18f));
                    g.fillRect (plotX, noteY - halfUp, plotW, halfUp + halfDn);
                }
            }
            g.setFont (juce::Font (juce::FontOptions{}.withHeight (9.5f)));
            g.setColour (T.hint);
            const int lblW = juce::roundToInt (kAxisMarginL) - 2, lblH = 11;
            int lastLabelY = -100;   // tracks the bottom edge of the last drawn label
            for (const auto& bn : visible)
            {
                const int labelY = juce::jlimit (1, juce::roundToInt (plotH) - lblH - 1,
                                                 juce::roundToInt (bn.y) - lblH / 2);
                if (labelY < lastLabelY + lblH + 1)
                    continue;   // would overlap with the label above — skip
                g.drawText (midiNoteName (bn.note), 0, labelY, lblW, lblH,
                            juce::Justification::centredRight, false);
                lastLabelY = labelY;
            }
        }
        else
        {
            auto yLabel = [&] (float norm) -> juce::String {
                const float ranged = minOut + norm * (maxOut - minOut);
                switch (msgType) {
                    case MessageType::CC:
                    case MessageType::ChannelPressure:
                        return juce::String (juce::roundToInt (ranged * 127.0f));
                    case MessageType::PitchBend: {
                        const int pb = juce::roundToInt (ranged * 16383.0f) - 8192;
                        return (pb >= 0 ? "+" : "") + juce::String (pb);
                    }
                    case MessageType::Note:
                        return midiNoteName (juce::roundToInt (ranged * 127.0f));
                }
                return {};
            };
            g.setFont (juce::Font (juce::FontOptions{}.withHeight (10.0f)));
            g.setColour (T.hint);
            const int lblW = juce::roundToInt (kAxisMarginL) - 2, lblH = 12;
            int lastGridLabelY = -100;
            // Iterate top→bottom so the highest-value label is drawn first and the
            // overlap guard correctly skips labels that crowd the ones already placed.
            for (int i = _yDivisions; i >= 0; --i)
            {
                const float norm  = (float)i / (float)_yDivisions;
                const int   yPx   = juce::roundToInt ((1.0f - norm) * plotH);
                const int labelY  = juce::jlimit (1, juce::roundToInt (plotH) - lblH - 1, yPx - lblH/2);
                if (labelY < lastGridLabelY + lblH + 1)
                    continue;   // skip overlapping label
                g.drawText (yLabel (norm), 0, labelY, lblW, lblH,
                            juce::Justification::centredRight, false);
                lastGridLabelY = labelY;
            }
        }

        // ── X axis ────────────────────────────────────────────────────────────
        g.setFont (juce::Font (juce::FontOptions{}.withHeight (10.0f)));
        g.setColour (T.hint);
        const int xLblY = juce::roundToInt (h - kAxisMarginB + 2);
        const int xLblH = juce::roundToInt (kAxisMarginB - 3);
        for (int i = 0; i <= _xDivisions; ++i)
        {
            const float frac = (float)i / (float)_xDivisions;
            const float xPx  = plotX + frac * plotW;
            g.drawText (juce::String (juce::roundToInt (frac * 100.0f)) + "%",
                        juce::roundToInt (xPx - 18), xLblY, 36, xLblH,
                        juce::Justification::centred, false);
        }

        // ── Duration overlay ──────────────────────────────────────────────────
        if (dur > 0.0f)
        {
            g.setFont (juce::Font (juce::FontOptions{}.withHeight (10.0f)));
            g.setColour (T.hint);
            g.drawText (juce::String (dur, 2) + "s",
                        juce::roundToInt (plotX + plotW - 46), 2, 46, 12,
                        juce::Justification::centredRight, false);
        }
    }

    // ── Lane legend (bottom-left of plot) ────────────────────────────────────
    {
        const int dotSz = 8, legH = 14, legW = 60;
        int lx = juce::roundToInt (plotX) + 4;
        const int ly = juce::roundToInt (plotY + plotH) - legH - 2;
        for (int lane = 0; lane < kMaxLanes; ++lane)
        {
            if (! proc.hasCurve (lane)) continue;
            const auto col = (_lightMode ? kLaneColourLight : kLaneColourDark)[lane];
            g.setColour (lane == _focusedLane ? col : col.withAlpha (0.50f));
            g.fillEllipse (static_cast<float> (lx), static_cast<float> (ly + (legH - dotSz) / 2),
                           static_cast<float> (dotSz), static_cast<float> (dotSz));
            g.setFont (juce::Font (juce::FontOptions{}.withHeight (9.5f)));
            g.drawText ("L" + juce::String (lane + 1),
                        lx + dotSz + 2, ly, legW, legH,
                        juce::Justification::centredLeft, false);
            lx += dotSz + 20;
        }
    }

    // ── Pause overlay ─────────────────────────────────────────────────────────
    // Shown when a curve exists but playback is stopped.
    // Draws two ▐▌ pause bars that blink at ~1.5 Hz (timer fires at 30 Hz;
    // _blinkOn toggles each tick, so one on/off cycle = 2 ticks ≈ 60 ms; we slow
    // it with a counter-free approach by only showing at even repaint ticks when
    // _blinkOn = true, giving a ~15 Hz visual rate which reads as a clear blink).
    const bool lanePausedForDisplay = proc.anyLaneHasCurve()
                                      && (! proc.isPlaying()
                                          || proc.getLanePaused (_focusedLane));
    if (lanePausedForDisplay)
    {
        const auto laneCol = (_lightMode ? kLaneColourLight : kLaneColourDark)[_focusedLane];

        // Faint tint to darken the plot while paused
        g.setColour (laneCol.withAlpha (0.08f));
        g.fillRect (plot);

        // Blinking pause bars drawn as two filled rounded rectangles
        if (_blinkOn)
        {
            const float cx   = plot.getCentreX();
            const float cy   = plot.getCentreY();
            const float barH = 22.0f;
            const float barW =  7.0f;
            const float gap  =  5.0f;   // gap between the two bars

            g.setColour (laneCol.withAlpha (0.55f));
            g.fillRoundedRectangle (cx - gap * 0.5f - barW, cy - barH * 0.5f, barW, barH, 2.5f);
            g.fillRoundedRectangle (cx + gap * 0.5f,        cy - barH * 0.5f, barW, barH, 2.5f);
        }
    }

    // ── Musical context badge (top-right pill) ────────────────────────────────
    // Shows "♪ C Dorian" for the focused lane in Note mode.
    {
        const auto msgParamIDForBadge = laneParam (_focusedLane, "msgType");
        const bool isNoteBadge = (static_cast<int> (
            proc.apvts.getRawParameterValue (msgParamIDForBadge)->load()) == 3);

        if (isNoteBadge && proc.hasCurve (_focusedLane))
        {
            // Root name
            const int root = static_cast<int> (proc.apvts.getRawParameterValue ("scaleRoot")->load());
            static const char* kNoteNamesSharpB[] = {"C","C\u266f","D","D\u266f","E","F","F\u266f","G","G\u266f","A","A\u266f","B"};
            static const char* kNoteNamesFlatB [] = {"C","D\u266d","D","E\u266d","E","F","G\u266d","G","A\u266d","A","B\u266d","B"};
            const auto* noteNames = _useFlats ? kNoteNamesFlatB : kNoteNamesSharpB;
            const juce::String rootName = juce::String::fromUTF8 (noteNames[root % 12]);

            // Scale name: derive relative mask first (scaleMask in APVTS is always absolute),
            // then recognise.  This mirrors the logic in DrawnCurveEditor::updateScaleStatus().
            const int mode = static_cast<int> (proc.apvts.getRawParameterValue ("scaleMode")->load());
            const uint16_t absMask = static_cast<uint16_t> (
                proc.apvts.getRawParameterValue ("scaleMask")->load());
            const uint16_t relMask = (mode < 7)
                ? proc.getScaleConfig (_focusedLane).mask   // already relative for presets
                : dcScale::pcsRotate (absMask, root);       // abs → rel for custom

            const auto id = dcScale::pcsRecognise (relMask);
            juce::String modeName = (relMask == 0x0FFF) ? "Chrom." : "Custom";
            if (id.exact && id.family >= 0 && id.family < (int)dcScale::kNumFamilies)
                modeName = juce::String::fromUTF8 (dcScale::kFamilies[id.family].modes[id.mode].name);

            const juce::String kNote = juce::String::charToString (juce::juce_wchar (0x266a));
            const juce::String badgeText = kNote + " " + rootName + " " + modeName;

            const float badgeH = 20.0f;
            const float badgeW = juce::jmin (w - plotX - 8.0f,
                                             static_cast<float> (badgeText.length() * 8 + 20));
            // Position below the duration label (which sits at y≈2..14) to avoid overlap.
            const juce::Rectangle<float> badge (w - badgeW - 6.0f, 18.0f, badgeW, badgeH);

            const juce::Colour pillBg   = (_lightMode ? juce::Colour (0xd0ffffff) : juce::Colour (0xd0121220));
            const juce::Colour pillText = (_lightMode ? juce::Colour (0xff2d2b27) : juce::Colour (0xccffffff));

            g.setColour (pillBg);
            g.fillRoundedRectangle (badge, badgeH * 0.5f);
            g.setColour (pillText);
            g.setFont (juce::Font (juce::FontOptions{}.withHeight (11.0f)));
            g.drawText (badgeText, badge.toNearestInt(), juce::Justification::centred, false);
        }
    }
}

// ── Touch / mouse ─────────────────────────────────────────────────────────────

static float normX (float rawX, float w) noexcept
{
    return juce::jlimit (0.0f, 1.0f, (rawX - kAxisMarginL) / (w - kAxisMarginL));
}
static float normY (float rawY, float h) noexcept
{
    return juce::jlimit (0.0f, 1.0f, rawY / (h - kAxisMarginB));
}

void CurveDisplay::mouseDown (const juce::MouseEvent& e)
{
    captureStartTime = juce::Time::getMillisecondCounterHiRes();
    isCapturing = true;
    capturePath.clear();
    capturePath.startNewSubPath (static_cast<float> (e.x), static_cast<float> (e.y));
    proc.beginCapture (_focusedLane);
    proc.addCapturePoint (0.0,
        normX (static_cast<float> (e.x), static_cast<float> (getWidth())),
        normY (static_cast<float> (e.y), static_cast<float> (getHeight())));
    repaint();
}

void CurveDisplay::mouseDrag (const juce::MouseEvent& e)
{
    if (! isCapturing) return;
    capturePath.lineTo (static_cast<float> (e.x), static_cast<float> (e.y));
    const double t = (juce::Time::getMillisecondCounterHiRes() - captureStartTime) / 1000.0;
    proc.addCapturePoint (t,
        normX (static_cast<float> (e.x), static_cast<float> (getWidth())),
        normY (static_cast<float> (e.y), static_cast<float> (getHeight())));
    repaint();
}

void CurveDisplay::mouseUp (const juce::MouseEvent& e)
{
    if (! isCapturing) return;
    const double t = (juce::Time::getMillisecondCounterHiRes() - captureStartTime) / 1000.0;
    proc.addCapturePoint (t,
        normX (static_cast<float> (e.x), static_cast<float> (getWidth())),
        normY (static_cast<float> (e.y), static_cast<float> (getHeight())));
    proc.finalizeCapture (_focusedLane);
    isCapturing = false;
    capturePath.clear();
    repaint();
}

void CurveDisplay::timerCallback()
{
    if (++_blinkCounter >= kBlinkPeriod)
    {
        _blinkCounter = 0;
        _blinkOn = ! _blinkOn;
    }
    repaint();
}

//==============================================================================
// DrawnCurveEditor — constructor
//==============================================================================

DrawnCurveEditor::DrawnCurveEditor (DrawnCurveProcessor& p)
    : AudioProcessorEditor (&p),
      proc (p),
      curveDisplay (p)
{
    // Set _appLF as BOTH the component LookAndFeel and the global default.
    //
    // Why both?
    //   • setLookAndFeel(&_appLF)  → widgets that call LookAndFeel virtual methods
    //     (drawButtonText, getLabelFont, …) use _appLF.
    //   • setDefaultLookAndFeel(&_appLF) → direct g.setFont() calls inside custom
    //     paint/drawButtonText overrides resolve the typeface through
    //     juce_getTypefaceForFont, which is wired to
    //     LookAndFeel::getDefaultLookAndFeel().getTypefaceForFont().  Without this
    //     second line, those calls go through JUCE's stock LookAndFeel_V4 which maps
    //     the default sans-serif to "Helvetica" on iOS — a font that lacks ♭ ♯ ♮.
    juce::LookAndFeel::setDefaultLookAndFeel (&_appLF);
    setLookAndFeel (&_appLF);

    setSize (Layout::editorW, Layout::editorH);
    setWantsKeyboardFocus (true);

    // ── Play (hidden) / Clear ─────────────────────────────────────────────────
    addChildComponent (playButton);
    playButton.onClick = [this]
    {
        const bool nowPlaying = ! proc.isPlaying();
        proc.setPlaying (nowPlaying);
        playButton.setButtonText (nowPlaying ? "Pause" : "Play");
        dirControl.repaint();
        curveDisplay.repaint();
    };

    addAndMakeVisible (clearButton);
    clearButton.onClick = [this]
    {
        proc.setPlaying (false);
        proc.clearAllSnapshots();
        playButton.setButtonText ("Play");
        dirControl.repaint();
        curveDisplay.repaint();
    };

    addAndMakeVisible (panicButton);
    panicButton.onClick = [this] { proc.sendPanic(); };

    addAndMakeVisible (themeButton);
    // ☾ = go to dark mode  ☀ = go to light mode (symbol shows destination)
    //
    // SF Pro (installed as primary typeface via DrawnCurveLookAndFeel) carries
    // text-form glyphs for both U+263E ☾ and U+2600 ☀ in its Miscellaneous
    // Symbols block coverage.  With SF Pro as the primary typeface, CoreText
    // uses SF Pro's glyph directly without falling back to Apple Color Emoji.
    // U+FE0E (VARIATION SELECTOR-15) is NOT appended here: JUCE renders it as
    // a visible [?] box rather than skipping it as a zero-width modifier.
    const juce::String kMoon = juce::String::charToString (0x263E);   // ☾
    const juce::String kSun  = juce::String::charToString (0x263C);   // ☼ (WHITE SUN WITH RAYS — text glyph in SF Pro, unlike U+2600 which routes to emoji)
    themeButton.setButtonText (kMoon);   // start in light mode → offer dark
    themeButton.onClick = [this, kMoon, kSun]
    {
        _lightMode = ! _lightMode;
        themeButton.setButtonText (_lightMode ? kMoon : kSun);
        curveDisplay.setLightMode (_lightMode);
        helpOverlay.setLightMode (_lightMode);
        applyTheme();
    };

    addAndMakeVisible (syncButton);
    syncButton.setClickingTogglesState (true);
    {
        const bool isSyncing = proc.apvts.getRawParameterValue (ParamID::syncEnabled)->load() > 0.5f;
        syncButton.setToggleState (isSyncing, juce::dontSendNotification);
    }
    syncButton.onClick = [this]
    {
        const bool nowSyncing = syncButton.getToggleState();  // already toggled
        if (auto* pSync = dynamic_cast<juce::AudioParameterBool*> (
                              proc.apvts.getParameter (ParamID::syncEnabled)))
            *pSync = nowSyncing;
        onSyncToggled (nowSyncing);
    };

    addAndMakeVisible (helpButton);
    helpButton.onClick = [this]
    {
        helpOverlay.setLightMode (_lightMode);
        helpOverlay.setVisible (! helpOverlay.isVisible());
        if (helpOverlay.isVisible()) helpOverlay.toFront (false);
    };

    // ── Global speed slider (utility bar — always bound to global speed param) ─
    setupSlider (speedSlider, speedLabel, "FREE");
    speedSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 42, 18);
    speedSlider.setTextValueSuffix ("x");
    speedSlider.setNumDecimalPlacesToDisplay (2);
    // speedAttach is set by bindPlaybackToLane() called at end of constructor.

#if defined(DC_HAVE_PER_LANE_PLAYBACK_PARAMS)
    // ── Per-lane speed slider (focused lane panel) ─────────────────────────────
    setupSlider (laneSpeedSlider, laneSpeedLabel, "Speed");
    laneSpeedSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 42, 18);
    laneSpeedSlider.setTextValueSuffix ("x");
    laneSpeedSlider.setNumDecimalPlacesToDisplay (2);
    // laneSpeedAttach set by bindShapingToLane() called below.
#endif

    // ── Direction control (shaping section — contextual) ──────────────────────
    dirControl.setSegments ({
        { "rev", "", "Reverse" },
        { "pp",  "", "Ping-Pong" },
        { "fwd", "", "Forward" }
    });
    dirControl.setSelectedIndex (0, juce::dontSendNotification);
    // dirControl.onChange is set by bindPlaybackToLane() called at end of constructor.
    dirControl.onTap = [this] (int, bool wasAlready)
    {
        if (_showingAllLanes)
        {
            // ── All Lanes tab: tap active segment toggles global play/pause ──
            if (wasAlready)
            {
                const bool nowPlaying = ! proc.isPlaying();
                proc.setPlaying (nowPlaying);
                playButton.setButtonText (nowPlaying ? "Pause" : "Play");
            }
            else
            {
                proc.setPlaying (true);
                playButton.setButtonText ("Pause");
            }
        }
        else
        {
            // ── Individual lane tab: tap active segment pauses/resumes that lane ──
            if (wasAlready)
            {
                const bool nowPaused = ! proc.getLanePaused (_focusedLane);
                proc.setLanePaused (_focusedLane, nowPaused);
                // Also ensure global engine is playing so other lanes keep running.
                if (! proc.isPlaying())
                {
                    proc.setPlaying (true);
                    playButton.setButtonText ("Pause");
                }
            }
            else
            {
                // Tapped a different direction: un-pause this lane and resume globally.
                proc.setLanePaused (_focusedLane, false);
                if (! proc.isPlaying())
                {
                    proc.setPlaying (true);
                    playButton.setButtonText ("Pause");
                }
            }
        }
        dirControl.repaint();
        curveDisplay.repaint();
    };
    dirControl.setSegmentPainter ([this] (juce::Graphics& g,
                                          juce::Rectangle<float> bounds,
                                          int index, bool active)
    {
        const float cx = bounds.getCentreX(), cy = bounds.getCentreY();
        const float aw = bounds.getHeight() * 0.35f;
        const float tw = aw * 0.82f;

        // Semantic states — per-lane when in a lane tab, global when in All Lanes tab
        const bool enginePlaying = proc.isPlaying();
        const bool hasCurve      = proc.anyLaneHasCurve();
        const bool lanePaused    = ! _showingAllLanes && proc.getLanePaused (_focusedLane);
        // "paused" = selected direction, curve exists, engine or this lane is paused
        const bool paused        = active && hasCurve && (! enginePlaying || lanePaused);
        // "live"   = selected direction, engine running, this lane not paused
        const bool live          = active && enginePlaying && ! lanePaused;

        // Direction arrow opacity:
        //   live  → full (shows which direction is running)
        //   paused→ 30% (ghost; pause bars overlay it)
        //   other  → normal active/inactive colour
        const juce::Colour baseCol = active ? dirControl.activeLabel : dirControl.labelColour;
        const float arrowAlpha = live ? 1.0f : (paused ? 0.28f : 1.0f);
        g.setColour (baseCol.withAlpha (arrowAlpha));

        auto fillTri = [&] (bool pointRight) {
            juce::Path triPath;
            if (pointRight) triPath.addTriangle (cx+tw, cy, cx-tw, cy-aw, cx-tw, cy+aw);
            else            triPath.addTriangle (cx-tw, cy, cx+tw, cy-aw, cx+tw, cy+aw);
            g.fillPath (triPath);
        };

        if (index == 0)      fillTri (false);
        else if (index == 2) fillTri (true);
        else { fillTri (false); fillTri (true); }

        // Pause bars: shown when paused (not when playing).
        // This lets the user know playback is suspended; tap to resume.
        if (paused)
        {
            const float ps = bounds.getHeight() * 0.52f;
            const float px = cx - ps * 0.5f, py = cy - ps * 0.5f;
            const float bw = ps * 0.24f, gap = ps * 0.20f;
            g.setColour (dirControl.activeLabel.withAlpha (0.80f));
            g.fillRoundedRectangle (px,        py, bw, ps, 2.0f);
            g.fillRoundedRectangle (px+bw+gap, py, bw, ps, 2.0f);
        }
    });
    _muteDrawLF.iconType  = dcui::IconType::mute;
    _teachDrawLF.iconType = dcui::IconType::teach;
    addAndMakeVisible (dirControl);

    // ── Grid density buttons ──────────────────────────────────────────────────
    for (auto* b : { &tickYMinusBtn, &tickYPlusBtn, &tickXMinusBtn, &tickXPlusBtn })
        b->setLookAndFeel (&_densityLF);
    addAndMakeVisible (tickYMinusBtn);
    addAndMakeVisible (tickYPlusBtn);
    addAndMakeVisible (tickXMinusBtn);
    addAndMakeVisible (tickXPlusBtn);
    tickYMinusBtn.onClick = [this] { curveDisplay.setYDivisions (curveDisplay.getYDivisions() - 1); };
    tickYPlusBtn .onClick = [this] { curveDisplay.setYDivisions (curveDisplay.getYDivisions() + 1); };
    tickXMinusBtn.onClick = [this] { curveDisplay.setXDivisions (curveDisplay.getXDivisions() - 1); };
    tickXPlusBtn .onClick = [this] { curveDisplay.setXDivisions (curveDisplay.getXDivisions() + 1); };

    // ── Lane focus selector ───────────────────────────────────────────────────
    laneFocusCtrl.setSegments ({
        { "l1", "1", "Lane 1" },
        { "l2", "2", "Lane 2" },
        { "l3", "3", "Lane 3" },
    });
    laneFocusCtrl.setSelectedIndex (0, juce::dontSendNotification);
    laneFocusCtrl.onChange = [this] (int idx)
    {
        setFocusedLane (idx);
    };
    addAndMakeVisible (laneFocusCtrl);

    // ── Shaping sliders ───────────────────────────────────────────────────────
    setupSlider (smoothingSlider, smoothingLabel, "Smooth");
    rangeSlider.setSliderStyle (juce::Slider::TwoValueHorizontal);
    rangeSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    rangeSlider.setRange (0.0, 1.0, 0.001);
    addAndMakeVisible (rangeSlider);
    rangeLabel.setFont (juce::Font (juce::FontOptions{}.withHeight (11.0f)));
    addAndMakeVisible (rangeLabel);

    rangeSlider.onValueChange = [this]
    {
        const int L = _focusedLane;
        if (auto* pMin = dynamic_cast<juce::AudioParameterFloat*> (
                             proc.apvts.getParameter (laneParam (L, "minOutput"))))
            *pMin = static_cast<float> (rangeSlider.getMinValue());
        if (auto* pMax = dynamic_cast<juce::AudioParameterFloat*> (
                             proc.apvts.getParameter (laneParam (L, "maxOutput"))))
            *pMax = static_cast<float> (rangeSlider.getMaxValue());
        updateRangeLabel();
    };

    // Phase offset slider (per focused lane, like smoothingSlider)
    setupSlider (phaseOffsetSlider, phaseOffsetLabel,
                 juce::String::charToString (juce::juce_wchar (0x03C6)) + " Phase");  // φ Phase
    phaseOffsetSlider.setTextValueSuffix ("%");
    phaseOffsetSlider.setNumDecimalPlacesToDisplay (0);

    // oneShotBtn is now unused (loop mode is per-lane in the matrix); keep constructed
    // but do not add to visible hierarchy.

    // Lane sync toggle — locks all looping lane playheads to the same phase.
    // Lives in the GLOBAL panel alongside the host-sync toggle.
    // Symbol U+2261 "≡" (identical-to, three equal lines) suggests all lanes in lock-step.
    addAndMakeVisible (laneSyncBtn);
    laneSyncBtn.setClickingTogglesState (true);
    laneSyncBtn.setButtonText (juce::String::charToString (juce::juce_wchar (0x2261)));  // ≡
    laneSyncBtn.setToggleState (proc.getLanesSynced(), juce::dontSendNotification);
    laneSyncBtn.onClick = [this]
    {
        proc.setLanesSynced (laneSyncBtn.getToggleState());
        applyTheme();
    };

    // Bind shaping to lane 0 at startup.
    bindShapingToLane (0);

    // ── Routing matrix rows ───────────────────────────────────────────────────
    // Message-type button symbols (param values: CC=0, ChannelPressure=1, PitchBend=2, Note=3)

    for (int L = 0; L < kMaxLanes; ++L)
    {
        // Message-type button — shows current mode as a symbol
        const int curType = static_cast<int> (
            proc.apvts.getRawParameterValue (laneParam (L, "msgType"))->load());
        // initial text set via updateLaneRow below; just pre-populate here
        { const juce::String kNote = juce::String::charToString (juce::juce_wchar (0x2669));  // ♩
          auto s = [&kNote] (int t) -> juce::String { switch(t){case 0:return "CC";case 1:return "AT";case 2:return "PB";case 3:return kNote;}return "?"; };
          laneTypeBtn[static_cast<size_t>(L)].setButtonText (s (curType)); }
        laneTypeBtn[static_cast<size_t>(L)].setLookAndFeel (&_symbolLF);
        laneTypeBtn[static_cast<size_t>(L)].onClick = [this, L]
        {
            static const int kCycleNext[4] = { 2, 0, 3, 1 };
            const int cur = static_cast<int> (
                proc.apvts.getRawParameterValue (laneParam (L, "msgType"))->load());
            const int next = kCycleNext[std::clamp (cur, 0, 3)];
            if (auto* pType = dynamic_cast<juce::AudioParameterChoice*> (
                              proc.apvts.getParameter (laneParam (L, "msgType"))))
                *pType = next;
            updateLaneRow (L);
            if (L == _focusedLane)
                updateScaleVisibility();
        };
        laneTypeBtn[static_cast<size_t>(L)].onStateChange = [this, L] {
            // Right-click → popup menu
            if (laneTypeBtn[static_cast<size_t>(L)].getState() == juce::Button::ButtonState::buttonDown
                && juce::ModifierKeys::currentModifiers.isRightButtonDown())
            {
                const int cur = static_cast<int> (
                    proc.apvts.getRawParameterValue (laneParam (L, "msgType"))->load());
                juce::PopupMenu m;
                m.addItem (1, "CC  (Control Change)",    true, cur == 0);
                m.addItem (2, "PB  (Pitch Bend)",        true, cur == 2);
                m.addItem (3, "N   (Note)",               true, cur == 3);
                m.addItem (4, "At  (Channel Pressure)",   true, cur == 1);
                m.addSeparator();
                m.addItem (10, "Copy type to all lanes");
                m.addItem (11, "Copy channel to all lanes");
                m.addItem (12, "Copy all settings to all lanes");
                m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&laneTypeBtn[static_cast<size_t>(L)]),
                    [this, L] (int result) {
                        if (result == 0) return;
                        if (result <= 4)
                        {
                            static const int kMenuToParam[5] = { 0, 0, 2, 3, 1 };
                            const int newType = kMenuToParam[result];
                            if (auto* pNewType = dynamic_cast<juce::AudioParameterChoice*> (
                                              proc.apvts.getParameter (laneParam (L, "msgType"))))
                                *pNewType = newType;
                            updateLaneRow (L);
                            if (L == _focusedLane) updateScaleVisibility();
                            return;
                        }
                        // Copy-to-all operations
                        for (int T = 0; T < kMaxLanes; ++T)
                        {
                            if (T == L) continue;
                            if (result == 10 || result == 12)   // type
                            {
                                const int srcType = static_cast<int> (
                                    proc.apvts.getRawParameterValue (laneParam (L, "msgType"))->load());
                                if (auto* pType = dynamic_cast<juce::AudioParameterChoice*> (
                                                  proc.apvts.getParameter (laneParam (T, "msgType"))))
                                    *pType = srcType;
                            }
                            if (result == 11 || result == 12)   // channel
                            {
                                const int srcCh = static_cast<int> (
                                    proc.apvts.getRawParameterValue (laneParam (L, "midiChannel"))->load());
                                if (auto* pCh = dynamic_cast<juce::AudioParameterInt*> (
                                                  proc.apvts.getParameter (laneParam (T, "midiChannel"))))
                                    *pCh = srcCh;
                            }
                            if (result == 12)   // all settings: also CC# and velocity
                            {
                                const int srcCC = static_cast<int> (
                                    proc.apvts.getRawParameterValue (laneParam (L, "ccNumber"))->load());
                                const int srcVel = static_cast<int> (
                                    proc.apvts.getRawParameterValue (laneParam (L, "noteVelocity"))->load());
                                if (auto* pCC = dynamic_cast<juce::AudioParameterInt*> (
                                                  proc.apvts.getParameter (laneParam (T, "ccNumber"))))
                                    *pCC = srcCC;
                                if (auto* pVel = dynamic_cast<juce::AudioParameterInt*> (
                                                  proc.apvts.getParameter (laneParam (T, "noteVelocity"))))
                                    *pVel = srcVel;
                            }
                        }
                        updateAllLaneRows();
                        updateScaleVisibility();
                    });
            }
        };
        addAndMakeVisible (laneTypeBtn[static_cast<size_t>(L)]);

        // Detail label (CC# or velocity)
        laneDetailLabel[static_cast<size_t>(L)].setFont (juce::Font (juce::FontOptions{}.withHeight (10.0f)));
        laneDetailLabel[static_cast<size_t>(L)].setJustificationType (juce::Justification::centred);
        laneDetailLabel[static_cast<size_t>(L)].setEditable (false, true, false);
        laneDetailLabel[static_cast<size_t>(L)].onEditorHide = [this, L]
        {
            const int val = juce::jlimit (0, 127, laneDetailLabel[static_cast<size_t>(L)].getText().getIntValue());
            const int type = static_cast<int> (
                proc.apvts.getRawParameterValue (laneParam (L, "msgType"))->load());
            if (type == 3)   // Note — edit velocity
            {
                if (auto* pVel = dynamic_cast<juce::AudioParameterInt*> (
                                  proc.apvts.getParameter (laneParam (L, "noteVelocity"))))
                    *pVel = juce::jlimit (1, 127, val);
            }
            else             // CC — edit cc number
            {
                if (auto* pCC = dynamic_cast<juce::AudioParameterInt*> (
                                  proc.apvts.getParameter (laneParam (L, "ccNumber"))))
                    *pCC = val;
            }
            updateLaneRow (L);
        };
        addAndMakeVisible (laneDetailLabel[static_cast<size_t>(L)]);

        // Channel label
        laneChannelLabel[static_cast<size_t>(L)].setFont (juce::Font (juce::FontOptions{}.withHeight (10.0f)));
        laneChannelLabel[static_cast<size_t>(L)].setJustificationType (juce::Justification::centred);
        laneChannelLabel[static_cast<size_t>(L)].setEditable (false, true, false);
        laneChannelLabel[static_cast<size_t>(L)].onEditorHide = [this, L]
        {
            const int val = juce::jlimit (1, 16, laneChannelLabel[static_cast<size_t>(L)].getText().getIntValue());
            if (auto* pChan = dynamic_cast<juce::AudioParameterInt*> (
                              proc.apvts.getParameter (laneParam (L, "midiChannel"))))
                *pChan = val;
            updateLaneRow (L);
        };
        addAndMakeVisible (laneChannelLabel[static_cast<size_t>(L)]);

        // Teach button
        laneTeachBtn[static_cast<size_t>(L)].setLookAndFeel (&_teachDrawLF);
        addAndMakeVisible (laneTeachBtn[static_cast<size_t>(L)]);
        laneTeachBtn[static_cast<size_t>(L)].onClick = [this, L]
        {
            if (proc.isTeachPending (L))
            {
                proc.cancelTeach();
                applyTheme();   // recolour
            }
            else
            {
                proc.cancelTeach();   // cancel any previous lane
                proc.beginTeach (L);   // all message types: isolates lane output
                applyTheme();
            }
        };

        // Loop / one-shot toggle (replaces the old mute button)
        // ∞ = loop (default), 1 = one-shot
        {
            static const juce::String kLoop = juce::String::charToString (juce::juce_wchar (0x221E));
            const bool isOneShot = proc.apvts.getRawParameterValue (laneParam (L, "loopMode"))->load() > 0.5f;
            laneMuteBtn[static_cast<size_t>(L)].setLookAndFeel (&_symbolLF);
            laneMuteBtn[static_cast<size_t>(L)].setButtonText (isOneShot ? "1" : kLoop);
        }
        addAndMakeVisible (laneMuteBtn[static_cast<size_t>(L)]);
        laneMuteBtn[static_cast<size_t>(L)].onClick = [this, L]
        {
            static const juce::String kLoop = juce::String::charToString (juce::juce_wchar (0x221E));
            if (auto* pLoop = dynamic_cast<juce::AudioParameterBool*> (
                                  proc.apvts.getParameter (laneParam (L, "loopMode"))))
            {
                const bool nowOneShot = ! pLoop->get();
                *pLoop = nowOneShot;
                laneMuteBtn[static_cast<size_t>(L)].setButtonText (nowOneShot ? "1" : kLoop);
                proc.updateLaneSnapshot (L);
            }
            applyTheme();
        };

        // Register listeners for per-lane params
        proc.apvts.addParameterListener (laneParam (L, "msgType"),      this);
        proc.apvts.addParameterListener (laneParam (L, "ccNumber"),     this);
        proc.apvts.addParameterListener (laneParam (L, "midiChannel"),  this);
        proc.apvts.addParameterListener (laneParam (L, "noteVelocity"), this);
        proc.apvts.addParameterListener (laneParam (L, "enabled"),      this);
        proc.apvts.addParameterListener (laneParam (L, "loopMode"),    this);
        proc.apvts.addParameterListener (laneParam (L, "phaseOffset"), this);
        proc.apvts.addParameterListener (laneParam (L, "minOutput"),    this);
        proc.apvts.addParameterListener (laneParam (L, "maxOutput"),    this);
        proc.apvts.addParameterListener (laneParam (L, "smoothing"),    this);
    }
    // Global scale params (outside per-lane loop — shared by all Note-mode lanes)
    proc.apvts.addParameterListener ("scaleMode", this);
    proc.apvts.addParameterListener ("scaleRoot", this);
    proc.apvts.addParameterListener ("scaleMask", this);

    // mappingDetailLabel was removed (info is already visible in the matrix rows above it).

    // ── Lane colour-dot focus selectors ───────────────────────────────────────
    // Transparent hit-area buttons positioned over the lane colour dots in the
    // routing matrix.  Tapping a dot switches the focused lane.
    for (int L = 0; L < kMaxLanes; ++L)
    {
        auto& btn = laneSelectBtn[static_cast<size_t> (L)];
        btn.setButtonText ({});
        btn.setOpaque (false);
        addAndMakeVisible (btn);
        btn.onClick = [this, L] { setFocusedLane (L); };
    }

    updateAllLaneRows();

    // ── Scale quantization controls — family browser ──────────────────────────

    addAndMakeVisible (scaleLabel);
    scaleLabel.setFont (DrawnCurveLookAndFeel::makeFont (11.0f));
    scaleLabel.setJustificationType (juce::Justification::centred);

    // Family tab buttons
    for (int f = 0; f < dcScale::kNumFamilies; ++f)
    {
        auto& btn = familyBtns[static_cast<size_t>(f)];
        btn.setButtonText (juce::String::fromUTF8 (dcScale::kFamilies[f].name));
        btn.setLookAndFeel (&_symbolLF);
        addAndMakeVisible (btn);
        btn.onClick = [this, f]
        {
            setActiveFamily (f);

            // Auto-select: if the current scale does not already belong to this
            // family, apply the last mode the user picked in it (or mode 0 if
            // this family has never been visited).  This prevents two families
            // appearing simultaneously highlighted (active tab ≠ recognised family).
            if (_recognisedFamily != f)
            {
                const auto& fam = dcScale::kFamilies[f];
                const int modeIdx = juce::jlimit (0, fam.count - 1,
                                                  _lastModePerFamily[static_cast<size_t>(f)]);
                const uint16_t relMask = fam.modes[static_cast<size_t>(modeIdx)].mask;
                const int root = static_cast<int> (
                    proc.apvts.getRawParameterValue ("scaleRoot")->load());
                const uint16_t absMask = dcScale::pcsRotate (relMask, 12 - root);
                if (auto* pMask = dynamic_cast<juce::AudioParameterInt*> (
                        proc.apvts.getParameter ("scaleMask")))
                    *pMask = static_cast<int> (absMask);
                if (auto* pMode = dynamic_cast<juce::AudioParameterInt*> (
                        proc.apvts.getParameter ("scaleMode")))
                    *pMode = 7;
                proc.updateAllLaneScales();
                scaleLattice.setMask (absMask);
                addRecentMask (relMask);
                updateScaleStatus();
                curveDisplay.repaint();
            }

            updateScalePresetButtons();   // repaint chip/tab colours
        };
    }

    // Recent-history tab button
    recentFamilyBtn.setButtonText (juce::String::charToString (juce::juce_wchar (0x2605))
                                   + " Recent");   // ★ Recent
    recentFamilyBtn.setLookAndFeel (&_symbolLF);
    addChildComponent (recentFamilyBtn);   // hidden until Note mode (like family tabs)
    recentFamilyBtn.onClick = [this]
    {
        setActiveFamily (kRecentFamilyIdx);
        updateScalePresetButtons();
    };

    // Subfamily chip buttons — populated by setActiveFamily(); hidden until Note mode.
    for (int i = 0; i < kMaxModes; ++i)
    {
        auto& btn = subfamilyBtns[static_cast<size_t>(i)];
        btn.setLookAndFeel (&_subfamilyLF[static_cast<size_t>(i)]);
        addChildComponent (btn);   // invisible until setActiveFamily() shows them
        btn.onClick = [this, i]
        {
            // Determine the relative mask (root-relative interval set).
            uint16_t relMask;
            if (_activeFamilyIdx == kRecentFamilyIdx)
            {
                if (i >= static_cast<int> (_recentMasks.size())) return;
                relMask = _recentMasks[static_cast<size_t>(i)];
            }
            else
            {
                const auto& fam = dcScale::kFamilies[_activeFamilyIdx];
                if (i >= fam.count) return;
                relMask = fam.modes[static_cast<size_t>(i)].mask;
            }
            const int root = static_cast<int> (proc.apvts.getRawParameterValue ("scaleRoot")->load());
            // Root-relative → absolute: rotate left by (12 - root).
            const uint16_t absMask = dcScale::pcsRotate (relMask, 12 - root);
            if (auto* pMask = dynamic_cast<juce::AudioParameterInt*> (proc.apvts.getParameter ("scaleMask")))
                *pMask = static_cast<int> (absMask);
            if (auto* pMode = dynamic_cast<juce::AudioParameterInt*> (proc.apvts.getParameter ("scaleMode")))
                *pMode = 7;
            proc.updateAllLaneScales();
            scaleLattice.setMask (absMask);
            addRecentMask (relMask);   // record in history (idempotent for Recent tab re-clicks)
            // Remember which mode was last used in this family so switching back restores it.
            if (_activeFamilyIdx != kRecentFamilyIdx)
                _lastModePerFamily[static_cast<size_t> (_activeFamilyIdx)] = i;
            updateScaleStatus();
            curveDisplay.repaint();
        };
    }

    addAndMakeVisible (scaleLattice);

    scaleLattice.onMaskChanged = [this] (uint16_t mask)
    {
        if (auto* pMask = dynamic_cast<juce::AudioParameterInt*> (proc.apvts.getParameter ("scaleMask")))
            *pMask = static_cast<int> (mask);
        if (auto* pMode = dynamic_cast<juce::AudioParameterInt*> (proc.apvts.getParameter ("scaleMode")))
            *pMode = 7;
        proc.updateAllLaneScales();
        // Convert abs mask → relative before storing in recent history.
        const int root = static_cast<int> (proc.apvts.getRawParameterValue ("scaleRoot")->load());
        addRecentMask (dcScale::pcsRotate (mask, root));
        updateScaleStatus();
        curveDisplay.repaint();
    };

    scaleLattice.setMask (calcAbsLatticeMask (proc, 0));
    scaleLattice.setRoot (static_cast<int> (proc.apvts.getRawParameterValue ("scaleRoot")->load()));

    // Scale action buttons
    auto applyMask = [this] (uint16_t mask)
    {
        if (auto* pMask = dynamic_cast<juce::AudioParameterInt*> (proc.apvts.getParameter ("scaleMask")))
            *pMask = static_cast<int> (mask);
        if (auto* pMode = dynamic_cast<juce::AudioParameterInt*> (proc.apvts.getParameter ("scaleMode")))
            *pMode = 7;
        proc.updateAllLaneScales();
        scaleLattice.setMask (mask);
        updateScaleStatus();
        curveDisplay.repaint();
    };

    // Scale action buttons — Unicode glyphs, no custom LF needed.
    // ● all  ○ none  ◑ invert  ◆ root
    scaleAllBtn .setButtonText (juce::String::charToString (juce::juce_wchar (0x25CF)));  // ●
    scaleNoneBtn.setButtonText (juce::String::charToString (juce::juce_wchar (0x25CB)));  // ○
    scaleInvBtn .setButtonText (juce::String::charToString (juce::juce_wchar (0x25D1)));  // ◑
    scaleRootBtn.setButtonText (juce::String::charToString (juce::juce_wchar (0x25C6)));  // ◆

    addAndMakeVisible (scaleAllBtn);
    scaleAllBtn.onClick = [applyMask] { applyMask (0x0FFF); };

    addAndMakeVisible (scaleNoneBtn);
    scaleNoneBtn.onClick = [this, applyMask]
    {
        const int root = static_cast<int> (proc.apvts.getRawParameterValue ("scaleRoot")->load());
        // Lattice convention: bit (11 - pc) = pitch class pc active.
        applyMask (static_cast<uint16_t> (1u << (11 - root)));
    };

    addAndMakeVisible (scaleInvBtn);
    scaleInvBtn.onClick = [this, applyMask]
    {
        applyMask ((~calcAbsLatticeMask (proc, 0)) & 0x0FFF);
    };

    addAndMakeVisible (scaleRootBtn);

    const auto resetRootBtn = [this]
    {
        scaleLattice.setRootSelectMode (false);
        const auto btnBg   = _lightMode ? juce::Colour (0xffF0EFE7) : juce::Colour (0xff333355);
        const auto btnText = _lightMode ? juce::Colour (0xff28261F) : juce::Colours::white;
        scaleRootBtn.setColour (juce::TextButton::buttonColourId,  btnBg);
        scaleRootBtn.setColour (juce::TextButton::textColourOffId, btnText);
    };

    scaleRootBtn.onClick = [this]
    {
        const bool entering = ! scaleLattice.isRootSelectMode();
        scaleLattice.setRootSelectMode (entering);
        const auto accent  = _lightMode ? juce::Colour (0xffF59E0B) : juce::Colour (0xffFBBF24);
        const auto btnBg   = _lightMode ? juce::Colour (0xffF0EFE7) : juce::Colour (0xff333355);
        const auto btnText = _lightMode ? juce::Colour (0xff28261F) : juce::Colours::white;
        scaleRootBtn.setColour (juce::TextButton::buttonColourId,
                                entering ? accent : btnBg);
        scaleRootBtn.setColour (juce::TextButton::textColourOffId,
                                entering ? juce::Colours::white : btnText);
    };

    scaleLattice.onRootChanged = [this, resetRootBtn] (int newRoot)
    {
        // Semantic: changing root TRANSPOSES the scale to the new root —
        // the root-relative interval pattern stays the same.
        //
        // For presets (mode 0–6) the APVTS already models this: the processor
        // stores mode + root separately, so simply updating scaleRoot gives the
        // correct preset transposition automatically.
        //
        // For custom masks (mode = 7) the absolute pitch-class mask is stored
        // directly, so we must re-derive it from the current relative pattern.
        const int mode    = static_cast<int> (proc.apvts.getRawParameterValue ("scaleMode")->load());
        const int oldRoot = static_cast<int> (proc.apvts.getRawParameterValue ("scaleRoot")->load());

        if (mode == 7 && newRoot != oldRoot)
        {
            const uint16_t absMask  = calcAbsLatticeMask (proc, 0);
            const uint16_t relMask  = dcScale::pcsRotate (absMask, oldRoot);
            const uint16_t newAbs   = dcScale::pcsRotate (relMask, (12 - newRoot) % 12);
            if (auto* pMask = dynamic_cast<juce::AudioParameterInt*> (proc.apvts.getParameter ("scaleMask")))
                *pMask = static_cast<int> (newAbs);
            scaleLattice.setMask (newAbs);  // immediate visual update; async also fires
        }

        if (auto* pRoot = dynamic_cast<juce::AudioParameterInt*> (proc.apvts.getParameter ("scaleRoot")))
            *pRoot = newRoot;
        proc.updateAllLaneScales();
        scaleLattice.setRoot (newRoot);
        curveDisplay.repaint();
        resetRootBtn();
        updateScaleStatus();
    };

    // Notation toggle — switches chromatic labels between ♯ (sharps) and ♭ (flats).
    scaleNotationBtn.setButtonText (juce::String::charToString (juce::juce_wchar (0x266F)));  // ♯
    addAndMakeVisible (scaleNotationBtn);
    scaleNotationBtn.onClick = [this]
    {
        _useFlats = !_useFlats;
        scaleLattice.setUseFlats (_useFlats);
        curveDisplay.setUseFlats (_useFlats);
        scaleNotationBtn.setButtonText (
            juce::String::charToString (juce::juce_wchar (_useFlats ? 0x266D : 0x266F)));
        updateRangeLabel();   // refreshes note-name range text if in Note mode
    };

    // Rotate button — ↻ cycle to the next mode in the current family (same root).
    scaleRotateBtn.setButtonText (juce::String::charToString (juce::juce_wchar (0x21BB)));  // ↻
    addAndMakeVisible (scaleRotateBtn);
    scaleRotateBtn.onClick = [this]
    {
        // Require a recognised family; if none, do nothing (button becomes a no-op
        // for fully custom scales, which have no ordered mode sequence to cycle).
        if (_recognisedFamily < 0)
            return;

        const int root = static_cast<int> (proc.apvts.getRawParameterValue ("scaleRoot")->load());
        const auto& fam     = dcScale::kFamilies[_recognisedFamily];
        const int   nextMode = (_recognisedMode + 1) % fam.count;
        const uint16_t relMask  = fam.modes[nextMode].mask;

        // Keep root fixed; only the interval pattern (mode) changes.
        const uint16_t absMask = dcScale::pcsRotate (relMask, (12 - root) % 12);

        if (auto* pM = dynamic_cast<juce::AudioParameterInt*> (proc.apvts.getParameter ("scaleMask")))
            *pM = static_cast<int> (absMask);
        if (auto* pMo = dynamic_cast<juce::AudioParameterInt*> (proc.apvts.getParameter ("scaleMode")))
            *pMo = 7;   // Custom — mode-specific preset slots not used for mode cycling
        proc.updateAllLaneScales();
        scaleLattice.setMask (absMask);
        addRecentMask (relMask);

        updateScaleStatus();
        if (_recognisedFamily >= 0 && _recognisedFamily != _activeFamilyIdx)
            setActiveFamily (_recognisedFamily);
        updateScalePresetButtons();
        curveDisplay.repaint();
    };

    // Initialise the family browser to match the current scale (or Diatonic if unrecognised).
    {
        const int mode = static_cast<int> (proc.apvts.getRawParameterValue ("scaleMode")->load());
        const int root = static_cast<int> (proc.apvts.getRawParameterValue ("scaleRoot")->load());
        const uint16_t relMask = (mode < 7)
            ? proc.getScaleConfig (0).mask
            : dcScale::pcsRotate (static_cast<uint16_t> (
                  proc.apvts.getRawParameterValue ("scaleMask")->load()), root);
        const auto id = dcScale::pcsRecognise (relMask);
        setActiveFamily (id.exact ? id.family : 0);
    }

    // Mask label — display only (no text editor, avoids UIKit tracking element warning).
    // The lattice is the primary editing surface for the scale mask.
    maskLabel.setFont (DrawnCurveLookAndFeel::makeFont (11.0f));
    maskLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (maskLabel);
    updateScaleStatus();

    // ── Musical zone toggle ───────────────────────────────────────────────────
    musicalToggleBtn.setLookAndFeel (&_symbolLF);
    {
        const juce::String kMusDown = juce::String::charToString (juce::juce_wchar (0x25BC));  // ▼ (large solid)
        const juce::String kMusUp   = juce::String::charToString (juce::juce_wchar (0x25B2));  // ▲ (large solid)
        musicalToggleBtn.setButtonText (kMusDown);
        addAndMakeVisible (musicalToggleBtn);
        musicalToggleBtn.onClick = [this, kMusDown, kMusUp]
        {
            _musicalExpanded = ! _musicalExpanded;
            musicalToggleBtn.setButtonText (_musicalExpanded ? kMusUp : kMusDown);
            updateScaleVisibility();
        };
    }

    // ── Curve display + help overlay ──────────────────────────────────────────
    addAndMakeVisible (curveDisplay);
    addChildComponent (helpOverlay);

    // Register global param listeners
    proc.apvts.addParameterListener (ParamID::playbackDirection, this);
    proc.apvts.addParameterListener (ParamID::syncEnabled,       this);
#if defined(DC_HAVE_PER_LANE_PLAYBACK_PARAMS)
    for (int L = 0; L < kMaxLanes; ++L)
        proc.apvts.addParameterListener (laneParam (L, ParamID::laneDirection), this);
#endif

    applyTheme();
    onSyncToggled (proc.apvts.getRawParameterValue (ParamID::syncEnabled)->load() > 0.5f);
    updateScaleVisibility();

#if defined(DC_HAVE_PER_LANE_PLAYBACK_PARAMS)
    bindPlaybackToLane (0);   // initial binding: lane 0
#endif
}

DrawnCurveEditor::~DrawnCurveEditor()
{
    // Restore defaults before _appLF is destroyed.
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    setLookAndFeel (nullptr);

    // Reset all widget-specific L&Fs before structs are destroyed.
    for (auto& b : laneTypeBtn)   b.setLookAndFeel (nullptr);
    for (auto& b : laneTeachBtn)  b.setLookAndFeel (nullptr);
    for (auto& b : laneMuteBtn)   b.setLookAndFeel (nullptr);
    for (auto& b : familyBtns)   b.setLookAndFeel (nullptr);
    recentFamilyBtn.setLookAndFeel (nullptr);
    for (auto& b : subfamilyBtns) b.setLookAndFeel (nullptr);
    for (auto* b : { &tickYMinusBtn, &tickYPlusBtn, &tickXMinusBtn, &tickXPlusBtn })
        b->setLookAndFeel (nullptr);
    // scaleAllBtn / None / Inv / Root use no custom LF; nothing to reset here.

    // Remove all APVTS listeners.
    proc.apvts.removeParameterListener (ParamID::playbackDirection, this);
    proc.apvts.removeParameterListener (ParamID::syncEnabled,       this);
    for (int L = 0; L < kMaxLanes; ++L)
    {
        for (const auto& base : { "msgType", "ccNumber", "midiChannel", "noteVelocity",
                                   "enabled", "loopMode", "phaseOffset", "minOutput", "maxOutput", "smoothing" })
            proc.apvts.removeParameterListener (laneParam (L, base), this);
#if defined(DC_HAVE_PER_LANE_PLAYBACK_PARAMS)
        proc.apvts.removeParameterListener (laneParam (L, ParamID::laneDirection), this);
#endif
    }
    // Global scale params
    for (const auto& id : { "scaleMode", "scaleRoot", "scaleMask" })
        proc.apvts.removeParameterListener (id, this);
}

//==============================================================================
// Setup helpers
//==============================================================================

void DrawnCurveEditor::setupSlider (juce::Slider& s, juce::Label& l,
                                     const juce::String& labelText,
                                     juce::Slider::SliderStyle style)
{
    s.setSliderStyle (style);
    s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 18);
    addAndMakeVisible (s);
    l.setText (labelText, juce::dontSendNotification);
    l.setFont (juce::Font (juce::FontOptions{}.withHeight (11.0f)));
    addAndMakeVisible (l);
}

bool DrawnCurveEditor::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::spaceKey)
    {
        playButton.triggerClick();
        return true;
    }
    return false;
}

//==============================================================================
// Lane focus
//==============================================================================

void DrawnCurveEditor::setFocusedLane (int lane)
{
    // Per-lane controls — visible in the individual lane (1/2/3) tabs.
    auto setPerLaneVisible = [this] (bool v)
    {
        for (juce::Component* w : { static_cast<juce::Component*> (&smoothingLabel),
                                     static_cast<juce::Component*> (&smoothingSlider),
                                     static_cast<juce::Component*> (&rangeLabel),
                                     static_cast<juce::Component*> (&rangeSlider),
                                     static_cast<juce::Component*> (&phaseOffsetLabel),
                                     static_cast<juce::Component*> (&phaseOffsetSlider),
                                     static_cast<juce::Component*> (&oneShotBtn) })
            w->setVisible (v);
    };

    if (lane < 0)
        return;   // no * tab — ignore out-of-range calls

    // ── Individual lane tab ───────────────────────────────────────────────────
    if (_showingAllLanes)
    {
        _showingAllLanes = false;
        setPerLaneVisible (true);
    }

    _focusedLane = juce::jlimit (0, kMaxLanes - 1, lane);
    curveDisplay.setFocusedLane (_focusedLane);
    laneFocusCtrl.setSelectedIndex (_focusedLane, juce::dontSendNotification);
    bindShapingToLane (_focusedLane);
#if defined(DC_HAVE_PER_LANE_PLAYBACK_PARAMS)
    bindPlaybackToLane (_focusedLane);
#endif
    updateScaleVisibility();
    updateLaneRow (_focusedLane);
    repaint();
}

void DrawnCurveEditor::bindShapingToLane (int lane)
{
    // Smoothing attachment
    smoothingAttach.reset();
    smoothingAttach = std::make_unique<Attach> (proc.apvts, laneParam (lane, "smoothing"), smoothingSlider);

    // Phase offset attachment
    phaseOffsetAttach.reset();
    phaseOffsetAttach = std::make_unique<Attach> (proc.apvts, laneParam (lane, "phaseOffset"), phaseOffsetSlider);

#if defined(DC_HAVE_PER_LANE_PLAYBACK_PARAMS)
    // Per-lane speed multiplier attachment
    laneSpeedAttach.reset();
    laneSpeedAttach = std::make_unique<Attach> (proc.apvts, laneParam (lane, ParamID::laneSpeedMul), laneSpeedSlider);
    laneSpeedSlider.setNormalisableRange (juce::NormalisableRange<double> (0.25, 4.0));
    // Disable per-lane speed when host sync is on (global SYNC slider controls all lanes)
    const bool isSyncing = proc.apvts.getRawParameterValue (ParamID::syncEnabled)->load() > 0.5f;
    laneSpeedSlider.setEnabled (! isSyncing);
    laneSpeedSlider.setAlpha  (isSyncing ? 0.4f : 1.0f);
#endif

    // Range slider — no APVTS attachment for two-value sliders; set directly.
    rangeSlider.setMinValue (proc.apvts.getRawParameterValue (laneParam (lane, "minOutput"))->load(),
                             juce::dontSendNotification);
    rangeSlider.setMaxValue (proc.apvts.getRawParameterValue (laneParam (lane, "maxOutput"))->load(),
                             juce::dontSendNotification);
    updateRangeLabel();
    // Loop mode is now per-lane in the matrix rows (laneMuteBtn repurposed); no shared oneShotBtn binding needed.
}

//==============================================================================
// Per-lane playback binding
//==============================================================================

#if defined(DC_HAVE_PER_LANE_PLAYBACK_PARAMS)

void DrawnCurveEditor::bindPlaybackToLane (int lane)
{
    // The global speedSlider in the utility bar is always bound to the global
    // playbackSpeed / syncBeats param via onSyncToggled() — do NOT rebind it here.
    // This function only rebinds the direction control to the correct per-lane param.

    if (lane >= 0 && lane < kMaxLanes)
    {
        const int dir = static_cast<int> (
            proc.apvts.getRawParameterValue (laneParam (lane, ParamID::laneDirection))->load());
        dirControl.setSelectedIndex (kDirParamToVis[juce::jlimit (0, 2, dir)],
                                     juce::dontSendNotification);
        dirControl.onChange = [this, lane] (int vis)
        {
            if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (
                    proc.apvts.getParameter (laneParam (lane, ParamID::laneDirection))))
                *p = kDirVisToParam[vis];
        };
    }
    else
    {
        const int dir = static_cast<int> (
            proc.apvts.getRawParameterValue (ParamID::playbackDirection)->load());
        dirControl.setSelectedIndex (kDirParamToVis[juce::jlimit (0, 2, dir)],
                                     juce::dontSendNotification);
        dirControl.onChange = [this] (int vis)
        {
            if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (
                    proc.apvts.getParameter (ParamID::playbackDirection)))
                *p = kDirVisToParam[vis];
        };
    }
}

#endif  // DC_HAVE_PER_LANE_PLAYBACK_PARAMS

//==============================================================================
// Lane row update
//==============================================================================

void DrawnCurveEditor::updateLaneRow (int lane)
{
    const int type = static_cast<int> (
        proc.apvts.getRawParameterValue (laneParam (lane, "msgType"))->load());
    const int ccNum = static_cast<int> (
        proc.apvts.getRawParameterValue (laneParam (lane, "ccNumber"))->load());
    const int vel   = static_cast<int> (
        proc.apvts.getRawParameterValue (laneParam (lane, "noteVelocity"))->load());
    const int ch    = static_cast<int> (
        proc.apvts.getRawParameterValue (laneParam (lane, "midiChannel"))->load());
    const bool isOneShot = proc.apvts.getRawParameterValue (laneParam (lane, "loopMode"))->load() > 0.5f;

    // Detail: CC# for CC, velocity for Note, "—" for PB/Aft
    juce::String detailText;
    if (type == 0)       detailText = juce::String (ccNum);
    else if (type == 3)  detailText = juce::String (vel);
    else                 detailText = "-";
    laneDetailLabel[static_cast<size_t>(lane)].setText (detailText, juce::dontSendNotification);

    // Type button symbol
    const juce::String kNoteSymbol = juce::String::charToString (juce::juce_wchar (0x2669));  // ♩
    auto sym = [&kNoteSymbol] (int t) -> juce::String {
        switch (t) { case 0: return "CC"; case 1: return "AT"; case 2: return "PB"; case 3: return kNoteSymbol; } return "?";
    };
    laneTypeBtn[static_cast<size_t>(lane)].setButtonText (sym (type));

    laneChannelLabel[static_cast<size_t>(lane)].setText (juce::String (ch), juce::dontSendNotification);

    // Teach button: works for all message types (solos lane output so a receiving
    // synth can MIDI-Learn; CC lanes also capture incoming CC#)
    laneTeachBtn[static_cast<size_t>(lane)].setButtonText (proc.isTeachPending (lane) ? "..." : "Teach");
    laneTeachBtn[static_cast<size_t>(lane)].setEnabled (true);
    laneTeachBtn[static_cast<size_t>(lane)].setAlpha (1.0f);

    // Loop / one-shot toggle button (previously the mute button)
    {
        static const juce::String kLoop = juce::String::charToString (juce::juce_wchar (0x221E));
        laneMuteBtn[static_cast<size_t>(lane)].setButtonText (isOneShot ? "1" : kLoop);
    }

    // Refresh range label if this is the focused lane (type affects label format)
    if (lane == _focusedLane)
        updateRangeLabel();
}

void DrawnCurveEditor::updateAllLaneRows()
{
    for (int L = 0; L < kMaxLanes; ++L)
        updateLaneRow (L);
}

void DrawnCurveEditor::updateMsgTypeButtons()
{
    // Kept for compatibility; delegates to the focused lane's row.
    updateLaneRow (_focusedLane);
}

//==============================================================================
// APVTS listener
//==============================================================================

namespace ParamID
{
    extern const juce::String playbackDirection;
    extern const juce::String syncEnabled;
}

void DrawnCurveEditor::parameterChanged (const juce::String& paramID, float)
{
    if (paramID == ParamID::playbackDirection)
    {
#if defined(DC_HAVE_PER_LANE_PLAYBACK_PARAMS)
        // Only update dirControl if we're currently showing global ("*") params.
        if (! _showingAllLanes) return;
#endif
        juce::MessageManager::callAsync ([this] {
            dirControl.setSelectedIndex (
                kDirParamToVis[static_cast<int> (
                    proc.apvts.getRawParameterValue (ParamID::playbackDirection)->load())],
                juce::dontSendNotification);
        });
        return;
    }

#if defined(DC_HAVE_PER_LANE_PLAYBACK_PARAMS)
    for (int L = 0; L < kMaxLanes; ++L)
    {
        if (paramID == laneParam (L, ParamID::laneDirection) && L == _focusedLane && ! _showingAllLanes)
        {
            juce::MessageManager::callAsync ([this, L] {
                const int dir = static_cast<int> (
                    proc.apvts.getRawParameterValue (laneParam (L, ParamID::laneDirection))->load());
                dirControl.setSelectedIndex (kDirParamToVis[juce::jlimit (0, 2, dir)],
                                             juce::dontSendNotification);
            });
            return;
        }
    }
#endif

    if (paramID == ParamID::syncEnabled)
    {
        juce::MessageManager::callAsync ([this] {
            const bool isSyncing = proc.apvts.getRawParameterValue (ParamID::syncEnabled)->load() > 0.5f;
            syncButton.setToggleState (isSyncing, juce::dontSendNotification);
            onSyncToggled (isSyncing);
        });
        return;
    }

    // Check per-lane params.
    for (int L = 0; L < kMaxLanes; ++L)
    {
        if (paramID == laneParam (L, "msgType")
            || paramID == laneParam (L, "ccNumber")
            || paramID == laneParam (L, "midiChannel")
            || paramID == laneParam (L, "noteVelocity"))
        {
            // Re-bake the snapshot so changes take effect immediately (no redraw needed).
            proc.updateLaneSnapshot (L);
            juce::MessageManager::callAsync ([this, L] {
                updateLaneRow (L);
                updateScaleVisibility();
                resized();   // recompute canvas height (anyNote may have changed)
                applyTheme();
            });
            return;
        }

        if (paramID == laneParam (L, "enabled"))
        {
            juce::MessageManager::callAsync ([this, L] { updateLaneRow (L); });
            return;
        }

        if (paramID == laneParam (L, "loopMode"))
        {
            proc.updateLaneSnapshot (L);
            juce::MessageManager::callAsync ([this, L] {
                updateLaneRow (L);   // refreshes per-lane loop button text in matrix
                applyTheme();
            });
            return;
        }

        if (paramID == laneParam (L, "minOutput") || paramID == laneParam (L, "maxOutput"))
        {
            // Re-bake range into snapshot immediately; also update the slider display.
            proc.updateLaneSnapshot (L);
            if (L == _focusedLane)
                juce::MessageManager::callAsync ([this] { updateRangeSlider(); });
            return;
        }

        if (paramID == laneParam (L, "smoothing"))
        {
            // Re-bake smoothing into snapshot immediately (attachment fires this).
            proc.updateLaneSnapshot (L);
            return;
        }

        if (paramID == laneParam (L, "phaseOffset"))
        {
            // Re-bake phase offset into snapshot immediately (attachment fires this).
            proc.updateLaneSnapshot (L);
            return;
        }

    }

    // Global scale params — update scale panel regardless of which lane is focused
    if (paramID == "scaleMode" || paramID == "scaleRoot" || paramID == "scaleMask")
    {
        proc.updateAllLaneScales();
        juce::MessageManager::callAsync ([this] {
            scaleLattice.setMask (calcAbsLatticeMask (proc, 0));
            scaleLattice.setRoot (static_cast<int> (
                proc.apvts.getRawParameterValue ("scaleRoot")->load()));
            updateScaleStatus();
            curveDisplay.repaint();
        });
    }
}

//==============================================================================
// Sync toggle
//==============================================================================

void DrawnCurveEditor::onSyncToggled (bool isSync)
{
    // ── Utility-bar global speed slider ───────────────────────────────────────
    // Always bound to the GLOBAL speed param (syncBeats when syncing,
    // playbackSpeed when free).  Never rebound to a per-lane param.
    speedAttach.reset();
    if (isSync)
    {
        speedAttach = std::make_unique<Attach> (proc.apvts, ParamID::syncBeats, speedSlider);
        // Invert the slider so right = fewer bars = faster (matches FREE direction).
        // JUCE's NormalisableRange requires start < end, so use custom convert lambdas.
        // The APVTS attachment maps slider.getValue() → param via param.convertTo0to1(),
        // which still receives the real bar count (1–32); the inversion only affects
        // the visual position on screen.
        juce::NormalisableRange<double> inv (1.0, 32.0,
            // from01: t=1 → 1 beat (right=fast), t=0 → 32 beats (left=slow)
            [](double, double, double t) { return 32.0 - juce::jlimit (0.0, 1.0, t) * 31.0; },
            // to01:   v=1 → 1.0 (right), v=32 → 0.0 (left)
            [](double, double, double v) { return juce::jlimit (0.0, 1.0, (32.0 - v) / 31.0); },
            [](double, double, double v) { return (double) juce::roundToInt (v); });
        speedSlider.setNormalisableRange (inv);
        speedSlider.setValue (proc.apvts.getRawParameterValue (ParamID::syncBeats)->load(),
                              juce::dontSendNotification);
        speedLabel.setText ("SYNC", juce::dontSendNotification);
        speedSlider.setTextValueSuffix (" beats");
        speedSlider.setNumDecimalPlacesToDisplay (0);
    }
    else
    {
        // FREE mode: bind utility bar slider to GLOBAL playbackSpeed (not per-lane).
        speedAttach = std::make_unique<Attach> (proc.apvts, ParamID::playbackSpeed, speedSlider);
        speedSlider.setNormalisableRange (juce::NormalisableRange<double> (0.25, 4.0));
        speedSlider.setValue (proc.apvts.getRawParameterValue (ParamID::playbackSpeed)->load(),
                              juce::dontSendNotification);
        speedLabel.setText ("FREE", juce::dontSendNotification);
        speedSlider.setTextValueSuffix ("x");
        speedSlider.setNumDecimalPlacesToDisplay (2);
    }

#if defined(DC_HAVE_PER_LANE_PLAYBACK_PARAMS)
    // ── Per-lane speed slider — enabled only in FREE mode ─────────────────────
    laneSpeedSlider.setEnabled (! isSync);
    laneSpeedSlider.setAlpha  (isSync ? 0.4f : 1.0f);
    // Rebind direction control to current focused lane.
    bindPlaybackToLane (_showingAllLanes ? -1 : _focusedLane);
#endif

    // Direction is always user-controlled; sync only affects speed/timing.
    dirControl.setAlpha (1.0f);
    dirControl.repaint();
    applyTheme();
}

//==============================================================================
// Scale helpers
//==============================================================================

void DrawnCurveEditor::updateScaleVisibility()
{
    // Show the scale panel whenever ANY lane uses Note mode — scale is now
    // global so it applies to all Note-mode lanes simultaneously.
    bool anyNote = false;
    for (int L = 0; L < kMaxLanes; ++L)
        if (static_cast<int> (proc.apvts.getRawParameterValue (laneParam (L, "msgType"))->load()) == 3)
            { anyNote = true; break; }

    // Musical toggle button: visible whenever any note lane is active.
    musicalToggleBtn.setVisible (anyNote);

    // Musical zone detail controls: only visible when anyNote AND zone is expanded.
    const bool showDetail = anyNote && _musicalExpanded;

    scaleLabel    .setVisible (showDetail);
    maskLabel     .setVisible (showDetail);
    scaleLattice  .setVisible (showDetail);
    // scaleNotationBtn lives in the utility bar — visible whenever any Note lane is active.
    scaleNotationBtn.setVisible (anyNote);
    scaleRotateBtn  .setVisible (showDetail);
    scaleAllBtn     .setVisible (showDetail);
    scaleNoneBtn  .setVisible (showDetail);
    scaleInvBtn   .setVisible (showDetail);
    scaleRootBtn  .setVisible (showDetail);
    for (auto& b : familyBtns) b.setVisible (showDetail);
    recentFamilyBtn.setVisible (showDetail);
    // Subfamily chips are individually shown/hidden by setActiveFamily().
    if (! showDetail)
        for (auto& b : subfamilyBtns) b.setVisible (false);

    if (anyNote)
    {
        scaleLattice.setMask (calcAbsLatticeMask (proc, 0));
        scaleLattice.setRoot (static_cast<int> (
            proc.apvts.getRawParameterValue ("scaleRoot")->load()));
        updateScaleStatus();
        if (showDetail)
            setActiveFamily (_activeFamilyIdx);   // refresh chip visibility
    }

    resized();
}

void DrawnCurveEditor::setActiveFamily (int familyIdx)
{
    // kRecentFamilyIdx (= kNumFamilies) is a valid virtual index for the Recent tab.
    _activeFamilyIdx = juce::jlimit (0, kRecentFamilyIdx, familyIdx);

    if (_activeFamilyIdx == kRecentFamilyIdx)
    {
        // ── Recent history tab ────────────────────────────────────────────────
        _numSubfamilyChips = static_cast<int> (_recentMasks.size());
        for (int i = 0; i < kMaxModes; ++i)
        {
            const bool vis = (i < _numSubfamilyChips);
            if (vis)
            {
                const uint16_t m  = _recentMasks[static_cast<size_t>(i)];
                // Use recognised name if available, otherwise "Custom"
                const auto    id  = dcScale::pcsRecognise (m);
                const juce::String name = id.exact
                    ? juce::String::fromUTF8 (dcScale::kFamilies[id.family].modes[id.mode].name)
                    : juce::String ("Custom");
                subfamilyBtns[static_cast<size_t>(i)].setButtonText (name);
                _subfamilyLF  [static_cast<size_t>(i)].mask = m;
            }
            subfamilyBtns[static_cast<size_t>(i)].setVisible (vis);
        }
    }
    else
    {
        // ── Named family tab ─────────────────────────────────────────────────
        const auto& fam    = dcScale::kFamilies[_activeFamilyIdx];
        _numSubfamilyChips = fam.count;
        for (int i = 0; i < kMaxModes; ++i)
        {
            const bool vis = (i < _numSubfamilyChips);
            if (vis)
            {
                subfamilyBtns[static_cast<size_t>(i)].setButtonText (juce::String::fromUTF8 (fam.modes[i].name));
                _subfamilyLF  [static_cast<size_t>(i)].mask = fam.modes[i].mask;
            }
            subfamilyBtns[static_cast<size_t>(i)].setVisible (vis);
        }
    }

    resized();
}

void DrawnCurveEditor::addRecentMask (uint16_t relMask)
{
    // De-duplicate: remove if already present, then prepend.
    _recentMasks.erase (std::remove (_recentMasks.begin(), _recentMasks.end(), relMask),
                        _recentMasks.end());
    _recentMasks.insert (_recentMasks.begin(), relMask);
    if (static_cast<int> (_recentMasks.size()) > kMaxRecentMasks)
        _recentMasks.resize (static_cast<size_t> (kMaxRecentMasks));

    // If the Recent tab is currently open, refresh it immediately.
    if (_activeFamilyIdx == kRecentFamilyIdx)
        setActiveFamily (kRecentFamilyIdx);
}

void DrawnCurveEditor::updateScalePresetButtons()
{
    // ── Family tab colours ────────────────────────────────────────────────────
    // Visual priority: the tab the user is LOOKING AT (active/browsed) always
    // carries the strong highlight.  The recognised tab (the family the current
    // scale actually belongs to, if different) carries a secondary dim highlight
    // so orientation is still preserved without stealing focus.
    //
    //   Active (browsed)           → famActive  (strong)
    //   Recognised, not active     → famBrowsed (dim "scale lives here" cue)
    //   All others                 → famInactive
    const juce::Colour famActive    = _lightMode ? juce::Colour (0xff0B6E4F) : juce::Colour (0xff2979ff);
    const juce::Colour famBrowsed   = _lightMode ? juce::Colour (0xffA7C4A0) : juce::Colour (0xff33557A);
    const juce::Colour famInactive  = _lightMode ? juce::Colour (0xffF0EFE7) : juce::Colour (0xff333355);
    const juce::Colour famTextAct   = juce::Colours::white;
    const juce::Colour famTextOff   = _lightMode ? juce::Colour (0xff706D64) : juce::Colours::lightgrey;

    for (int f = 0; f < dcScale::kNumFamilies; ++f)
    {
        const bool isActive     = (f == _activeFamilyIdx);              // user is viewing this tab
        const bool isRecognised = (f == _recognisedFamily) && !isActive; // scale lives here (secondary)
        const auto bg   = isActive     ? famActive
                        : isRecognised ? famBrowsed
                                       : famInactive;
        const auto text = (isActive || isRecognised) ? famTextAct : famTextOff;
        familyBtns[static_cast<size_t>(f)].setColour (juce::TextButton::buttonColourId,   bg);
        familyBtns[static_cast<size_t>(f)].setColour (juce::TextButton::buttonOnColourId,  famActive);
        familyBtns[static_cast<size_t>(f)].setColour (juce::TextButton::textColourOffId,   text);
    }
    // Recent tab: strong highlight when active (it has no recognised-family counterpart).
    {
        const bool isActive = (_activeFamilyIdx == kRecentFamilyIdx);
        recentFamilyBtn.setColour (juce::TextButton::buttonColourId,  isActive ? famActive   : famInactive);
        recentFamilyBtn.setColour (juce::TextButton::buttonOnColourId, famActive);
        recentFamilyBtn.setColour (juce::TextButton::textColourOffId,  isActive ? famTextAct : famTextOff);
    }

    // ── Subfamily chip colours ────────────────────────────────────────────────
    // Highlight the chip whose mode matches the current scale within the active family.
    const juce::Colour chipOn   = _lightMode ? juce::Colour (0xff1D4ED8) : juce::Colour (0xff60A5FA);
    const juce::Colour chipOff  = _lightMode ? juce::Colour (0xffE5E7EB) : juce::Colour (0xff374151);
    const juce::Colour dotOn    = _lightMode ? juce::Colour (0xff1E40AF) : juce::Colour (0xff93C5FD);
    const juce::Colour dotOff   = _lightMode ? juce::Colour (0xffBFDBFE) : juce::Colour (0xff1E3A5F);

    for (int i = 0; i < _numSubfamilyChips; ++i)
    {
        const bool match = (_recognisedFamily == _activeFamilyIdx) && (_recognisedMode == i);
        subfamilyBtns[static_cast<size_t>(i)].setColour (juce::TextButton::buttonColourId,   match ? chipOn : chipOff);
        subfamilyBtns[static_cast<size_t>(i)].setColour (juce::TextButton::textColourOffId,   match ? juce::Colours::white
                                                         : (_lightMode ? juce::Colour (0xff374151) : juce::Colour (0xffD1D5DB)));
        _subfamilyLF[static_cast<size_t>(i)].colOn  = dotOn;
        _subfamilyLF[static_cast<size_t>(i)].colOff = dotOff;
        subfamilyBtns[static_cast<size_t>(i)].repaint();
    }
}

void DrawnCurveEditor::updateRangeSlider()
{
    const int L = _focusedLane;
    rangeSlider.setMinValue (proc.apvts.getRawParameterValue (laneParam (L, "minOutput"))->load(),
                             juce::dontSendNotification);
    rangeSlider.setMaxValue (proc.apvts.getRawParameterValue (laneParam (L, "maxOutput"))->load(),
                             juce::dontSendNotification);
    updateRangeLabel();
}

void DrawnCurveEditor::updateRangeLabel()
{
    const float mn = static_cast<float> (rangeSlider.getMinValue());
    const float mx = static_cast<float> (rangeSlider.getMaxValue());

    const auto msgType = static_cast<MessageType> (
        static_cast<int> (proc.apvts.getRawParameterValue (
            laneParam (_focusedLane, "msgType"))->load()));

    if (msgType == MessageType::Note)
    {
        // Show MIDI note names (e.g. "C2 – G5"), honouring the ♯/♭ notation toggle.
        static const char* kSharpNames[] = { "C","C\u266f","D","D\u266f","E","F","F\u266f","G","G\u266f","A","A\u266f","B" };
        static const char* kFlatNames [] = { "C","D\u266d","D","E\u266d","E","F","G\u266d","G","A\u266d","A","B\u266d","B" };
        auto noteName = [&] (float norm) -> juce::String {
            const int midi = juce::jlimit (0, 127, juce::roundToInt (norm * 127.0f));
            const char* nm = _useFlats ? kFlatNames[midi % 12] : kSharpNames[midi % 12];
            return juce::String::fromUTF8 (nm) + juce::String (midi / 12 - 1);
        };
        rangeLabel.setText (noteName (mn) + " - " + noteName (mx),
                            juce::dontSendNotification);
    }
    else if (msgType == MessageType::PitchBend)
    {
        // Show signed pitch-bend values (-8192 – +8191)
        const int lo = juce::jlimit (-8192, 8191, juce::roundToInt (mn * 16383.0f) - 8192);
        const int hi = juce::jlimit (-8192, 8191, juce::roundToInt (mx * 16383.0f) - 8192);
        rangeLabel.setText (juce::String (lo) + " - " + juce::String (hi),
                            juce::dontSendNotification);
    }
    else
    {
        // CC (0-127) and Channel Pressure (0-127)
        const int lo = juce::jlimit (0, 127, juce::roundToInt (mn * 127.0f));
        const int hi = juce::jlimit (0, 127, juce::roundToInt (mx * 127.0f));
        rangeLabel.setText (juce::String (lo) + " - " + juce::String (hi),
                            juce::dontSendNotification);
    }

    // Repaint so the collapsed musical summary chip reflects the updated range text.
    repaint();
}

void DrawnCurveEditor::updateScaleStatus()
{
    // ── 1. Root-relative mask → recognition ──────────────────────────────────
    const int mode = static_cast<int> (proc.apvts.getRawParameterValue ("scaleMode")->load());
    const int root = static_cast<int> (proc.apvts.getRawParameterValue ("scaleRoot")->load());

    const uint16_t relMask = (mode < 7)
        ? proc.getScaleConfig (0).mask
        : dcScale::pcsRotate (
              static_cast<uint16_t> (proc.apvts.getRawParameterValue ("scaleMask")->load()),
              root);

    const auto id = dcScale::pcsRecognise (relMask);
    _recognisedFamily = id.exact ? id.family : -1;
    _recognisedMode   = id.exact ? id.mode   : -1;

    // Do NOT auto-switch the active family here.  The family tab is a browser:
    // the user navigates it manually; recognition just highlights which chip
    // matches (if any) and updates the name label.  Auto-switching would revert
    // a manual tab browse every time a parameterChanged callAsync fires.

    // ── 2. Decimal bitmask display ───────────────────────────────────────────
    const uint16_t absMask = calcAbsLatticeMask (proc, _focusedLane);
    maskLabel.setText (juce::String (static_cast<int> (absMask)).paddedLeft ('0', 4),
                       juce::dontSendNotification);

    // ── 3. Mode-name label ───────────────────────────────────────────────────
    if (id.exact)
        scaleLabel.setText (juce::String::fromUTF8 (dcScale::kFamilies[id.family].modes[id.mode].name),
                            juce::dontSendNotification);
    else
        scaleLabel.setText ((relMask == 0x0FFF) ? "Chrom." : "Custom",
                            juce::dontSendNotification);

    // ── 4. Colour highlight for tabs + chips ─────────────────────────────────
    updateScalePresetButtons();
}

//==============================================================================
// applyTheme
//==============================================================================

void DrawnCurveEditor::applyTheme()
{
    const bool light = _lightMode;

    const juce::Colour textCol  = light ? juce::Colour (0xff2d2b27) : juce::Colours::white;
    const juce::Colour dimText  = light ? juce::Colour (0xff7a766d) : juce::Colours::lightgrey;
    const juce::Colour tbBg     = light ? juce::Colour (0xffFCFBF7) : juce::Colour (0xff252538);
    const juce::Colour tbLine   = light ? juce::Colour (0xffDDD6CA) : juce::Colour (0x33ffffff);
    const juce::Colour accent   = light ? kLaneColourLight[0] : kLaneColourDark[0];   // purple
    const juce::Colour btnBg    = light ? juce::Colour (0xffF0EDE5) : juce::Colour (0xff333355);
    const juce::Colour btnText  = light ? juce::Colour (0xff2d2b27) : juce::Colours::white;

    // Sliders
    for (auto* s : { &smoothingSlider, &speedSlider, &phaseOffsetSlider
#if defined(DC_HAVE_PER_LANE_PLAYBACK_PARAMS)
                   , &laneSpeedSlider
#endif
                   })
    {
        s->setColour (juce::Slider::textBoxTextColourId,       textCol);
        s->setColour (juce::Slider::textBoxBackgroundColourId, tbBg);
        s->setColour (juce::Slider::textBoxOutlineColourId,    tbLine);
        s->setColour (juce::Slider::thumbColourId,             accent);
        s->setColour (juce::Slider::trackColourId,             accent.withAlpha (0.45f));
        s->setColour (juce::Slider::backgroundColourId,        tbBg);
    }
    rangeSlider.setColour (juce::Slider::thumbColourId,      accent);
    rangeSlider.setColour (juce::Slider::trackColourId,      accent.withAlpha (0.45f));
    rangeSlider.setColour (juce::Slider::backgroundColourId, tbBg);

    for (auto* l : { &smoothingLabel, &rangeLabel, &speedLabel, &phaseOffsetLabel
#if defined(DC_HAVE_PER_LANE_PLAYBACK_PARAMS)
                   , &laneSpeedLabel
#endif
                   })
        l->setColour (juce::Label::textColourId, dimText);

    // clearButton is a dcui::IconButton
    clearButton.setBaseColour (light ? juce::Colour (0xff706D64) : juce::Colours::lightgrey);

    // Musical toggle button
    musicalToggleBtn.setColour (juce::TextButton::buttonColourId,  btnBg);
    musicalToggleBtn.setColour (juce::TextButton::textColourOffId, dimText);

    // Lane select buttons — transparent hit areas over the colour dots
    for (auto& b : laneSelectBtn)
    {
        b.setColour (juce::TextButton::buttonColourId,  juce::Colours::transparentBlack);
        b.setColour (juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
        b.setColour (juce::TextButton::textColourOffId,  juce::Colours::transparentBlack);
    }

    // Scale notation button (in utility bar) — styled like regular utility buttons
    scaleNotationBtn.setColour (juce::TextButton::buttonColourId,  btnBg);
    scaleNotationBtn.setColour (juce::TextButton::textColourOffId, btnText);

    // Utility buttons
    for (auto* b : { &playButton, &panicButton, &themeButton, &helpButton,
                     &tickYMinusBtn, &tickYPlusBtn, &tickXMinusBtn, &tickXPlusBtn,
                     &laneSyncBtn })
    {
        b->setColour (juce::TextButton::buttonColourId,  btnBg);
        b->setColour (juce::TextButton::textColourOffId, btnText);
    }

    // Lane sync button — accent when active to make its state obvious
    {
        const auto syncAccent = light ? kLaneColourLight[1] : kLaneColourDark[1];   // teal
        laneSyncBtn.setColour (juce::TextButton::buttonOnColourId,  syncAccent.withAlpha (0.22f));
        laneSyncBtn.setColour (juce::TextButton::textColourOnId,    syncAccent);
    }

    // Panic button — red accent to signal danger
    panicButton.setColour (juce::TextButton::buttonColourId,
                           light ? juce::Colour (0xffFFE4E1) : juce::Colour (0xff5C1010));
    panicButton.setColour (juce::TextButton::textColourOffId,
                           light ? juce::Colour (0xffC0392B) : juce::Colour (0xffFF6B6B));

    // Direction control — purple accent matching Lane 0 colour
    dirControl.bgColour     = light ? juce::Colour (0xffEFE7FF) : btnBg;
    dirControl.activeColour = light ? juce::Colour (0xffffffff) : juce::Colour (0xff2979ff);
    dirControl.labelColour  = light ? juce::Colour (0xff6746d9) : juce::Colours::lightgrey;
    dirControl.activeLabel  = light ? juce::Colour (0xff6746d9) : juce::Colours::white;
    dirControl.borderColour = light ? juce::Colour (0xffDDD2FF) : juce::Colour (0x33ffffff);
    dirControl.repaint();

    // Lane focus control — active segment uses lane colour (lanes) or neutral accent (*).
    {
        const auto activeLaneCol = _showingAllLanes
            ? (light ? juce::Colour (0xff374151) : juce::Colour (0xff94A3B8))
            : (light ? kLaneColourLight[_focusedLane] : kLaneColourDark[_focusedLane]);
        const auto activeLabelCol = (activeLaneCol.getBrightness() > 0.55f)
                                    ? juce::Colour (0xdd1a1a1a)
                                    : juce::Colours::white;
        laneFocusCtrl.bgColour     = light ? juce::Colour (0xffF0EDE5) : btnBg;
        laneFocusCtrl.activeColour = activeLaneCol;
        laneFocusCtrl.labelColour  = dimText;
        laneFocusCtrl.activeLabel  = activeLabelCol;
        laneFocusCtrl.borderColour = light ? juce::Colour (0xffDDD6CA) : juce::Colour (0x33ffffff);
        laneFocusCtrl.repaint();
    }

    // Density buttons
    for (auto* b : { &tickYMinusBtn, &tickYPlusBtn, &tickXMinusBtn, &tickXPlusBtn })
    {
        b->setColour (juce::TextButton::buttonColourId,
                      light ? juce::Colour (0xffF0EFE7) : btnBg);
        b->setColour (juce::TextButton::textColourOffId,
                      light ? juce::Colour (0xff5B6985) : juce::Colours::lightgrey);
    }

    // Routing matrix rows
    for (int L = 0; L < kMaxLanes; ++L)
    {
        const auto laneCol   = light ? kLaneColourLight[L] : kLaneColourDark[L];
        const bool isOneShot = proc.apvts.getRawParameterValue (laneParam (L, "loopMode"))->load() > 0.5f;

        laneTypeBtn[static_cast<size_t>(L)].setColour (juce::TextButton::buttonColourId,  laneCol.withAlpha (0.18f));
        laneTypeBtn[static_cast<size_t>(L)].setColour (juce::TextButton::textColourOffId, laneCol);

        for (auto* lbl : { &laneDetailLabel[static_cast<size_t>(L)], &laneChannelLabel[static_cast<size_t>(L)] })
        {
            lbl->setColour (juce::Label::textColourId,       textCol);
            lbl->setColour (juce::Label::backgroundColourId, tbBg);
            lbl->setColour (juce::Label::outlineColourId,    tbLine);
            lbl->setColour (juce::Label::textWhenEditingColourId, textCol);
            lbl->setAlpha (1.0f);
        }

        // Teach button: glows amber while pending
        const bool teaching = proc.isTeachPending (L);
        const auto teachAccent = light ? juce::Colour (0xffF59E0B) : juce::Colour (0xffFBBF24);
        laneTeachBtn[static_cast<size_t>(L)].setColour (juce::TextButton::buttonColourId,
                                   teaching ? teachAccent : btnBg);
        laneTeachBtn[static_cast<size_t>(L)].setColour (juce::TextButton::textColourOffId,
                                   teaching ? juce::Colours::white : btnText);

        // Loop / one-shot button: loop = accent tint, one-shot = neutral
        laneMuteBtn[static_cast<size_t>(L)].setColour (juce::TextButton::buttonColourId,
                                  isOneShot ? btnBg : laneCol.withAlpha (0.18f));
        laneMuteBtn[static_cast<size_t>(L)].setColour (juce::TextButton::textColourOffId,
                                  isOneShot ? dimText : laneCol);
    }

    // Scale controls
    scaleLabel.setColour (juce::Label::textColourId, dimText);

    scaleLattice.colBg           = light ? juce::Colours::white          : juce::Colour (0xff252538);
    scaleLattice.colBorder       = light ? juce::Colour (0xffA9BAD5)     : juce::Colour (0x55ffffff);
    scaleLattice.colTextOff      = light ? juce::Colour (0xff8898AA)     : juce::Colour (0x88ffffff);
    scaleLattice.colActive       = light ? juce::Colour (0xffDCFCE7)     : juce::Colour (0xff22C55E);
    scaleLattice.colActiveBorder = light ? juce::Colour (0xff22C55E)     : juce::Colour (0xff4ADE80);
    scaleLattice.colTextOn       = light ? juce::Colour (0xff166534)     : juce::Colours::black;
    scaleLattice.colRoot         = light ? juce::Colour (0xffFEF3C7)     : juce::Colour (0xffF59E0B);
    scaleLattice.colRootBorder   = light ? juce::Colour (0xffF59E0B)     : juce::Colour (0xffFBBF24);
    scaleLattice.colRootRing     = light ? juce::Colour (0xffFBBF24)     : juce::Colour (0xffFDE68A);
    scaleLattice.colRootText     = light ? juce::Colour (0xff92400E)     : juce::Colours::black;
    scaleLattice.repaint();

    for (auto* b : { &scaleRotateBtn, &scaleAllBtn, &scaleNoneBtn, &scaleInvBtn, &scaleRootBtn })
    {
        b->setColour (juce::TextButton::buttonColourId,  btnBg);
        b->setColour (juce::TextButton::textColourOffId, btnText);
    }
    updateScalePresetButtons();   // re-colour family tabs + chips for new theme

    maskLabel.setColour (juce::Label::textColourId,            textCol);
    maskLabel.setColour (juce::Label::backgroundColourId,      btnBg);
    maskLabel.setColour (juce::Label::outlineColourId,         dimText);
    maskLabel.setColour (juce::Label::textWhenEditingColourId, textCol);

    // syncButton is a dcui::IconButton — use setBaseColour
    syncButton.setBaseColour (light ? juce::Colour (0xff6D28D9) : juce::Colour (0xff2979ff));
    syncButton.repaint();

    repaint();
}

//==============================================================================
// paint
//==============================================================================

void DrawnCurveEditor::paint (juce::Graphics& g)
{
    using namespace Layout;
    const Theme& T = _lightMode ? kLight : kDark;

    // ── Background ────────────────────────────────────────────────────────────
    g.fillAll (T.background);

    // ── Panel drawing helper ──────────────────────────────────────────────────
    auto drawPanel = [&] (juce::Rectangle<int> r, juce::Colour fill,
                          juce::Colour border, float radius)
    {
        if (r.isEmpty()) return;
        g.setColour (fill);
        g.fillRoundedRectangle (r.toFloat(), radius);
        g.setColour (border);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), radius, 1.0f);
    };

    const juce::Colour panelFill   = T.panelBg;
    const juce::Colour panelBorder = T.panelBorder;
    const float kR = static_cast<float> (panelRadius);

    // ── Stage panel ───────────────────────────────────────────────────────────
    drawPanel (_stagePanel, T.stageBg, panelBorder, kR + 4.0f);

    // ── Right-rail panels ─────────────────────────────────────────────────────
    drawPanel (_globalPanel,       panelFill, panelBorder, kR);
    drawPanel (_focusedLanePanel,  panelFill, panelBorder, kR);
    drawPanel (_lanesPanel,        panelFill, panelBorder, kR);
    drawPanel (_musicalPanel,      panelFill, panelBorder, kR);

    const juce::Colour eyebrowCol = T.hint;

    // ── Routing matrix: dots + focused-row highlight ──────────────────────────
    // Matrix is now embedded inside the focused lane panel. Use _matrixRowOrigin
    // (set in resized) to locate each row.
    if (! _focusedLanePanel.isEmpty() && _matrixRowOrigin != juce::Point<int>{})
    {
        const auto* laneColours = _lightMode ? kLaneColourLight : kLaneColourDark;

        for (int L = 0; L < kMaxLanes; ++L)
        {
            const int rowY = _matrixRowOrigin.getY() + L * _matrixRowStride;

            // Focused-row highlight
            if (L == _focusedLane)
            {
                const auto focusLaneCol = laneColours[_focusedLane];
                g.setColour (focusLaneCol.withAlpha (0.10f));
                g.fillRoundedRectangle (juce::Rectangle<int> (
                    _focusedLanePanel.getX() + 4, rowY,
                    _focusedLanePanel.getWidth() - 8, matRowH).toFloat(), 5.0f);
            }

            // Lane colour dot
            const float cx = static_cast<float> (_matrixRowOrigin.getX() + matDotW / 2);
            const float cy = static_cast<float> (rowY + matRowH / 2);
            const float r  = 5.0f;
            g.setColour (laneColours[L]);
            g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);

            // Add-card dashed outline for lanes with no curve
            if (! proc.hasCurve (L))
            {
                const auto typeBtn = laneTypeBtn[static_cast<size_t>(L)].getBounds();
                if (! typeBtn.isEmpty())
                {
                    const auto cardRow = juce::Rectangle<int> (
                        typeBtn.getX() - 4, rowY,
                        _focusedLanePanel.getRight() - typeBtn.getX() - panelPad + 4, matRowH);
                    g.setColour (laneColours[L].withAlpha (0.25f));
                    const float dash[] = { 5.0f, 4.0f };
                    juce::Path dashedRect;
                    dashedRect.addRoundedRectangle (cardRow.toFloat().reduced (0.5f), 5.0f);
                    juce::Path dashed;
                    juce::PathStrokeType stroke (1.0f);
                    stroke.createDashedStroke (dashed, dashedRect, dash, 2);
                    g.fillPath (dashed);
                }
            }
        }
    }

    // ── Musical collapsed summary text ────────────────────────────────────────
    if (! _musicalPanel.isEmpty() && ! _musicalExpanded)
    {
        // Show current scale + range summary
        bool anyNote = false;
        for (int L = 0; L < kMaxLanes; ++L)
            anyNote |= (static_cast<int> (proc.apvts.getRawParameterValue (
                laneParam (L, "msgType"))->load()) == 3);

        if (anyNote)
        {
            const auto inner = _musicalPanel.reduced (panelPad, 0)
                                            .withHeight (musicalCollapsedH);

            // Scale summary chip
            const juce::String scaleSummary = scaleLabel.getText();
            const juce::Colour chipCol = _lightMode ? juce::Colour (0xffF3EEFF)
                                                    : juce::Colour (0xff2a1f4a);
            const juce::Colour chipBorder = _lightMode ? juce::Colour (0xffCCBFFF)
                                                       : juce::Colour (0xff5a4a9a);
            const juce::Colour chipText = _lightMode ? juce::Colour (0xff5e40bf)
                                                     : juce::Colour (0xffA78BFA);

            float x = static_cast<float> (inner.getX() + 8);
            const float y = static_cast<float> (inner.getCentreY() - 11);
            const float chipH = 22.0f;

            // Scale chip
            {
                const float chipW = juce::jmin (110.0f, static_cast<float> (scaleSummary.length() * 9 + 20));
                const juce::Rectangle<float> chip (x, y, chipW, chipH);
                g.setColour (chipCol);
                g.fillRoundedRectangle (chip, 11.0f);
                g.setColour (chipBorder);
                g.drawRoundedRectangle (chip.reduced (0.5f), 11.0f, 1.0f);
                g.setColour (chipText);
                g.setFont (DrawnCurveLookAndFeel::makeFont (12.0f));
                g.drawText (scaleSummary, chip.toNearestInt(),
                            juce::Justification::centred, false);
                x += chipW + 8.0f;
            }

            // Range chip
            {
                const juce::String rangeSummary = rangeLabel.getText();
                if (rangeSummary.isNotEmpty())
                {
                    const float chipW = juce::jmin (100.0f, static_cast<float> (rangeSummary.length() * 8 + 20));
                    const juce::Rectangle<float> chip (x, y, chipW, chipH);
                    g.setColour (panelFill);
                    g.fillRoundedRectangle (chip, 11.0f);
                    g.setColour (panelBorder);
                    g.drawRoundedRectangle (chip.reduced (0.5f), 11.0f, 1.0f);
                    g.setColour (eyebrowCol);
                    g.setFont (DrawnCurveLookAndFeel::makeFont (12.0f));
                    g.drawText (rangeSummary, chip.toNearestInt(),
                                juce::Justification::centred, false);
                }
            }
        }
    }

}

//==============================================================================
// resized
//==============================================================================

void DrawnCurveEditor::resized()
{
    using namespace Layout;

    auto area = getLocalBounds().reduced (pad);

    // ── Utility / global bar ──────────────────────────────────────────────────
    // Full-width bar: left = global controls (sync, lane-sync, speed);
    //                 right = utility buttons (clear, panic, theme, help)
    {
        auto row = area.removeFromTop (utilityRowH);

        // Right side: utility buttons
        themeButton     .setBounds (row.removeFromRight (32).withSizeKeepingCentre (32, 28));
        row.removeFromRight (4);
        helpButton      .setBounds (row.removeFromRight (28).withSizeKeepingCentre (28, 28));
        row.removeFromRight (4);
        panicButton     .setBounds (row.removeFromRight (28).withSizeKeepingCentre (28, 28));
        row.removeFromRight (4);
        clearButton     .setBounds (row.removeFromRight (44).withSizeKeepingCentre (44, 28));
        row.removeFromRight (6);
        // ♯/♭ notation toggle — shown to the right of the speed controls
        scaleNotationBtn.setBounds (row.removeFromRight (28).withSizeKeepingCentre (28, 28));
        row.removeFromRight (6);   // visual separator gap

        // Left side: sync toggle + lane-sync toggle + speed label + speed slider
        syncButton .setBounds (row.removeFromLeft (38).withSizeKeepingCentre (38, 28));
        row.removeFromLeft (4);
        laneSyncBtn.setBounds (row.removeFromLeft (38).withSizeKeepingCentre (38, 28));
        row.removeFromLeft (8);
        speedLabel .setBounds (row.removeFromLeft (36).withSizeKeepingCentre (36, 14));
        speedSlider.setBounds (row.removeFromLeft (180));
    }
    area.removeFromTop (pad);

    // ── Determine musical zone height ─────────────────────────────────────────
    bool anyNoteMode = false;
    for (int L = 0; L < kMaxLanes; ++L)
        anyNoteMode |= (static_cast<int> (
            proc.apvts.getRawParameterValue (laneParam (L, "msgType"))->load()) == 3);

    // Always reserve at least musicalCollapsedH so the stage height stays stable
    // regardless of whether a note lane is active.
    const int musicalH = anyNoteMode
        ? (_musicalExpanded ? musicalExpandedH : musicalCollapsedH)
        : musicalCollapsedH;

    // ── Musical zone (bottom, full width) ─────────────────────────────────────
    if (musicalH > 0)
    {
        area.removeFromBottom (pad);
        _musicalPanel = area.removeFromBottom (musicalH);
        area.removeFromBottom (pad);
    }
    else
    {
        _musicalPanel = {};
    }

    // ── Musical toggle button + zone detail ──────────────────────────────────
    if (! _musicalPanel.isEmpty())
    {
        if (anyNoteMode)
        {
            // Collapsed strip: small triangle button at far right of the panel strip
            const auto toggleRect = _musicalPanel
                                        .withHeight (musicalCollapsedH)
                                        .withTrimmedLeft (_musicalPanel.getWidth() - 32)
                                        .reduced (4, 8);
            musicalToggleBtn.setBounds (toggleRect);
            musicalToggleBtn.setVisible (true);
        }
        else
        {
            // No note lanes — hide toggle, leave strip as quiet empty panel
            musicalToggleBtn.setBounds ({});
        }

        if (anyNoteMode && _musicalExpanded)
        {
            auto ne = _musicalPanel;
            ne.removeFromTop (musicalCollapsedH);   // skip the summary header strip
            ne.removeFromTop (4);

            // ── Family tab bar — toggle button shares this row ────────────────
            {
                auto fRow = ne.removeFromTop (kFamilyBarH);
                // Reserve space for the collapse (▴) button at the right end
                musicalToggleBtn.setBounds (fRow.removeFromRight (28));
                fRow.removeFromRight (4);
                const int N    = dcScale::kNumFamilies + 1;
                const int btnW = (fRow.getWidth() - (N - 1)) / N;
                for (int f = 0; f < dcScale::kNumFamilies; ++f)
                {
                    familyBtns[static_cast<size_t>(f)].setBounds (fRow.removeFromLeft (btnW));
                    fRow.removeFromLeft (1);
                }
                recentFamilyBtn.setBounds (fRow.removeFromLeft (btnW));
            }
            ne.removeFromTop (4);

            // ── Subfamily chip row ─────────────────────────────────────────────
            {
                auto sRow = ne.removeFromTop (kSubfamilyRowH);
                const int N = _numSubfamilyChips;
                if (N > 0)
                {
                    const int chipW = (sRow.getWidth() - (N - 1) * 2) / N;
                    for (int i = 0; i < kMaxModes; ++i)
                    {
                        if (i < N)
                        {
                            subfamilyBtns[static_cast<size_t>(i)].setBounds (sRow.removeFromLeft (chipW));
                            if (i < N - 1) sRow.removeFromLeft (2);
                        }
                        else { subfamilyBtns[static_cast<size_t>(i)].setBounds ({}); }
                    }
                }
            }
            ne.removeFromTop (4);

            // ── Action row ────────────────────────────────────────────────────
            {
                auto aRow = ne.removeFromTop (kActionRowH);
                maskLabel .setBounds (aRow.removeFromRight (52).withSizeKeepingCentre (52, 20));
                aRow.removeFromRight (3);
                scaleLabel.setBounds (aRow.removeFromRight (84).withSizeKeepingCentre (84, 14));
                aRow.removeFromRight (8);
                // scaleNotationBtn has moved to the utility bar (global section)
                scaleRotateBtn.setBounds (aRow.removeFromLeft (28)); aRow.removeFromLeft (6);
                scaleAllBtn   .setBounds (aRow.removeFromLeft (28)); aRow.removeFromLeft (2);
                scaleNoneBtn  .setBounds (aRow.removeFromLeft (28)); aRow.removeFromLeft (2);
                scaleInvBtn   .setBounds (aRow.removeFromLeft (28)); aRow.removeFromLeft (2);
                scaleRootBtn  .setBounds (aRow.removeFromLeft (28));
            }
            ne.removeFromTop (4);

            // ── Scale lattice: full width ──────────────────────────────────────
            scaleLattice.setBounds (ne.removeFromTop (kScaleLatticeH));
            _secNotes = _musicalPanel;
        }
        else
        {
            // Zone is collapsed (or no note lanes): explicitly clear component bounds
            // so previously-expanded components don't paint over the stage canvas.
            scaleLattice.setBounds ({});
            for (auto& b : familyBtns)    b.setBounds ({});
            recentFamilyBtn.setBounds ({});
            for (auto& b : subfamilyBtns) b.setBounds ({});
            scaleRotateBtn.setBounds ({});
            scaleAllBtn   .setBounds ({});
            scaleNoneBtn  .setBounds ({});
            scaleInvBtn   .setBounds ({});
            scaleRootBtn  .setBounds ({});
            scaleLabel    .setBounds ({});
            maskLabel     .setBounds ({});
        }
    }

    // ── Two-column split (main area) ──────────────────────────────────────────
    auto rightCol = area.removeFromRight (rightColW);
    area.removeFromRight (colGap);
    auto stageCol = area;

    // ══════════════════════════════════════════════════════════════════════════
    // RIGHT RAIL — single panel containing focused-lane controls + routing matrix
    // Global controls (sync, lane sync, speed) live in the utility bar above.
    // ══════════════════════════════════════════════════════════════════════════
    _globalPanel = {};
    _lanesPanel  = {};   // merged into focused lane panel — drawn there in paint()

    // ── Single FOCUSED LANE panel (full right rail) ───────────────────────────
    _focusedLanePanel = rightCol;
    {
        auto fp = _focusedLanePanel.reduced (panelPad);
        fp.removeFromTop (8);    // top padding

        // Lane focus selector (1 / 2 / 3)
        {
            auto focusRow = fp.removeFromTop (28);
            laneFocusCtrl.setBounds (focusRow);
        }
        fp.removeFromTop (6);

        // Direction segmented control (full width)
        dirControl.setBounds (fp.removeFromTop (40));
        fp.removeFromTop (6);

#if defined(DC_HAVE_PER_LANE_PLAYBACK_PARAMS)
        // Per-lane speed multiplier (before range)
        laneSpeedLabel .setBounds (fp.removeFromTop (paramLabelH));
        laneSpeedSlider.setBounds (fp.removeFromTop (paramSliderH));
        fp.removeFromTop (4);
#endif

        // Range
        rangeLabel .setBounds (fp.removeFromTop (paramLabelH));
        rangeSlider.setBounds (fp.removeFromTop (paramSliderH + 4));
        fp.removeFromTop (4);

        // Smooth
        smoothingLabel .setBounds (fp.removeFromTop (paramLabelH));
        smoothingSlider.setBounds (fp.removeFromTop (paramSliderH));
        fp.removeFromTop (4);

        // Phase offset
        phaseOffsetLabel .setBounds (fp.removeFromTop (paramLabelH));
        phaseOffsetSlider.setBounds (fp.removeFromTop (paramSliderH));
        fp.removeFromTop (8);

        // ── Routing matrix (immediately below sliders) ─────────────────────
        fp.removeFromTop (8);    // gap before matrix rows

        // Matrix rows — record origin for paint() dot positions
        _matrixRowOrigin = fp.getTopLeft();
        _matrixRowStride = matRowH + 3;

        for (int L = 0; L < kMaxLanes; ++L)
        {
            auto row = fp.removeFromTop (matRowH);
            fp.removeFromTop (3);

            // Dot column: reserve space (dot drawn in paint()) + invisible focus button
            auto dotCol = row.removeFromLeft (matDotW + matInnerGap);
            laneSelectBtn[static_cast<size_t> (L)].setBounds (
                dotCol.removeFromLeft (matDotW).withSizeKeepingCentre (matDotW, matDotW));

            laneTypeBtn[static_cast<size_t>(L)].setBounds (row.removeFromLeft (matTargetW));
            row.removeFromLeft (matInnerGap);
            laneDetailLabel[static_cast<size_t>(L)].setBounds (row.removeFromLeft (matDetailW));
            row.removeFromLeft (matInnerGap);
            laneChannelLabel[static_cast<size_t>(L)].setBounds (row.removeFromLeft (matChanW));
            row.removeFromLeft (matInnerGap);
            laneTeachBtn[static_cast<size_t>(L)].setBounds (row.removeFromLeft (matTeachW));
            row.removeFromLeft (matInnerGap);
            laneMuteBtn[static_cast<size_t>(L)].setBounds (row.removeFromLeft (matMuteW));
        }
        // mappingDetailLabel removed — info is already visible in the matrix rows above.
    }

    // ══════════════════════════════════════════════════════════════════════════
    // STAGE COLUMN
    // ══════════════════════════════════════════════════════════════════════════

    _stagePanel = stageCol;
    {
        auto sc = stageCol.reduced (6);   // inner padding

        // laneFocusCtrl is placed in the FOCUSED LANE panel (right rail); no placement here.

        // Y-density stepper (left edge of canvas)
        auto yStepCol = sc.removeFromLeft (yStepperW);
        sc.removeFromLeft (3);
        {
            const int btnH = 28, yMid = yStepCol.getCentreY();
            tickYPlusBtn .setBounds (yStepCol.getX(), yMid - btnH - 2, yStepperW, btnH);
            tickYMinusBtn.setBounds (yStepCol.getX(), yMid + 2,        yStepperW, btnH);
        }

        // X-density stepper (bottom edge of canvas)
        sc.removeFromBottom (3);
        {
            auto row = sc.removeFromBottom (xStepperH);
            tickXMinusBtn.setBounds (row.removeFromLeft (28));
            row.removeFromLeft (4);
            tickXPlusBtn .setBounds (row.removeFromLeft (28));
        }

        // Curve display fills remaining stage area
        curveDisplay.setBounds (sc);
    }

    // ── Help overlay ──────────────────────────────────────────────────────────
    helpOverlay.setBounds (getLocalBounds());
}
