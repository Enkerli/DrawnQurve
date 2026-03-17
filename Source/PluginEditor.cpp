/**
 * @file PluginEditor.cpp
 *
 * Implementation of CurveDisplay and DrawnCurveEditor.
 * See PluginEditor.h for the layout and design decision overview.
 */

#include "PluginEditor.h"

//==============================================================================
// Colour palettes
//
// Two pre-built themes: dark (default) and light.
// Colours match iOS Human Interface Guidelines (system colours for light mode;
// a custom dark scheme for dark mode).  The active Theme is selected in every
// paint() call via a pointer reference — no heap allocation per repaint.

struct Theme
{
    juce::Colour background;
    juce::Colour gridLine;
    juce::Colour curve;
    juce::Colour capture;
    juce::Colour playhead;
    juce::Colour playheadDot;
    juce::Colour hint;       // axis labels + "draw a curve" text
    juce::Colour border;
    juce::Colour panelBg;
};

static const Theme kDark
{
    juce::Colour { 0xff12121f },   // background
    juce::Colour { 0x18ffffff },   // gridLine
    juce::Colour { 0xff00e5ff },   // curve (cyan)
    juce::Colour { 0xffff6b35 },   // capture (orange)
    juce::Colour { 0xffffffff },   // playhead
    juce::Colour { 0xff00e5ff },   // playheadDot
    juce::Colour { 0x66ffffff },   // hint
    juce::Colour { 0x33ffffff },   // border
    juce::Colour { 0xff1c1c2e },   // panelBg
};

static const Theme kLight
{
    juce::Colour { 0xfff2f2f7 },   // background (iOS system grey 6)
    juce::Colour { 0x16000000 },   // gridLine
    juce::Colour { 0xff007aff },   // curve (iOS blue)
    juce::Colour { 0xffff3b30 },   // capture (iOS red)
    juce::Colour { 0xff1c1c1e },   // playhead
    juce::Colour { 0xff007aff },   // playheadDot
    juce::Colour { 0x88000000 },   // hint
    juce::Colour { 0x28000000 },   // border
    juce::Colour { 0xffffffff },   // panelBg
};

//==============================================================================
// HelpOverlay
//==============================================================================

HelpOverlay::HelpOverlay()
{
    // Intercept all mouse clicks so they don't fall through to controls below.
    setInterceptsMouseClicks (true, false);
    setVisible (false);
}

void HelpOverlay::paint (juce::Graphics& g)
{
    // Semi-transparent background — dark overlay in dark mode, slightly lighter in light mode.
    g.fillAll (_lightMode ? juce::Colour (0xd0000000) : juce::Colour (0xd4000000));

    // ── Content area ──────────────────────────────────────────────────────────
    const auto bounds = getLocalBounds().toFloat().reduced (24.0f, 20.0f);

    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (15.0f, juce::Font::bold));
    g.drawText ("DrawnCurve  Quick Reference",
                bounds.withHeight (22.0f).toNearestInt(),
                juce::Justification::centred, false);

    // ── Help text ─────────────────────────────────────────────────────────────
    // Sections are laid out as two columns of labelled entries.
    // All strings use basic ASCII so the built-in JUCE font renders them.

    struct Entry { const char* label; const char* desc; };

    static const Entry kEntries[] =
    {
        { "CURVE AREA",  "Draw a curve with your finger. Left to right = time (sets loop length)."
                         " Top to bottom = MIDI value (top is highest)." },
        { "Play / Pause","Start and stop looping the drawn curve." },
        { "Clear",       "Erase the curve and stop playback." },
        { "CC / Aft / PB / Note",
                         "Output type: Control Change, Channel Pressure (Aftertouch),"
                         " Pitch Bend (14-bit), or Note On/Off." },
        { "Sync",        "Follow host transport. On play: engine starts; on stop: engine stops."
                         " Speed slider becomes Beats -- set loop length in beats." },
        { "Fwd / Rev / P-P",
                         "Loop direction: Forward, Reverse, or Ping-Pong (back and forth)." },
        { "CC# / Vel",   "CC number (0-127) in CC mode, or Note velocity (1-127) in Note mode."
                         " Greyed out when unused." },
        { "Channel",     "MIDI output channel (1-16)." },
        { "Smooth",      "Smoothing amount: 0 = instant response; higher = gentler transitions." },
        { "Range",       "Output range. Left thumb = minimum value, right thumb = maximum value." },
        { "Speed / Beats",
                         "Playback speed (0.25x-4x) in manual mode, or loop length in beats"
                         " when Sync is active." },
        { "Y- / Y+",     "Decrease or increase horizontal grid lines (visual reference only)." },
        { "X- / X+",     "Decrease or increase vertical grid lines (visual reference only)." },
    };

    const float lineH   = 14.0f;
    const float labelW  = 112.0f;
    const float gap     = 6.0f;
    const float startY  = bounds.getY() + 28.0f;
    const float descW   = bounds.getWidth() - labelW - gap;

    float y = startY;
    for (const auto& e : kEntries)
    {
        // Bold label
        g.setFont (juce::Font (11.5f, juce::Font::bold));
        g.setColour (juce::Colour (0xff80d8ff));
        g.drawText (e.label,
                    juce::roundToInt (bounds.getX()),
                    juce::roundToInt (y),
                    juce::roundToInt (labelW),
                    juce::roundToInt (lineH * 2),
                    juce::Justification::topRight, false);

        // Description (word-wrapped to 2 lines)
        g.setFont (juce::Font (11.5f));
        g.setColour (juce::Colours::white);
        g.drawMultiLineText (e.desc,
                             juce::roundToInt (bounds.getX() + labelW + gap),
                             juce::roundToInt (y + 11.5f),
                             juce::roundToInt (descW));

        y += lineH * 2 + 2.0f;
        if (y + lineH * 2 > bounds.getBottom() - 18.0f)
            break;   // safety: don't draw past the bottom
    }

    // ── Dismiss hint ──────────────────────────────────────────────────────────
    g.setFont (juce::Font (11.0f, juce::Font::italic));
    g.setColour (juce::Colours::white.withAlpha (0.6f));
    g.drawText ("Tap anywhere to close",
                getLocalBounds().withTop (getHeight() - 22),
                juce::Justification::centred, false);
}

void HelpOverlay::mouseDown (const juce::MouseEvent&)
{
    setVisible (false);
}

//==============================================================================
// CurveDisplay
//
// Margins reserved for axis labels (pixels inside the component bounds):
static constexpr float kAxisMarginL = 36.0f;   // left  — Y-axis MIDI-value labels
static constexpr float kAxisMarginB = 16.0f;   // bottom — X-axis % + time labels

CurveDisplay::CurveDisplay (DrawnCurveProcessor& p)
    : proc (p)
{
    startTimerHz (30);   // repaint at 30 fps — axis labels track live param changes
}

CurveDisplay::~CurveDisplay() { stopTimer(); }

void CurveDisplay::resized() {}

void CurveDisplay::setLightMode (bool light) { _lightMode = light; repaint(); }

void CurveDisplay::paint (juce::Graphics& g)
{
    const Theme& T = _lightMode ? kLight : kDark;

    auto bounds = getLocalBounds().toFloat();
    const float w = bounds.getWidth();
    const float h = bounds.getHeight();

    // ── Plot area (inset for axis labels) ─────────────────────────────────────
    const float plotX = kAxisMarginL;
    const float plotY = 0.0f;
    const float plotW = w - kAxisMarginL;
    const float plotH = h - kAxisMarginB;
    const auto  plot  = juce::Rectangle<float> (plotX, plotY, plotW, plotH);

    // ── Background ────────────────────────────────────────────────────────────
    g.fillAll (T.background);

    // ── Subtle grid — X and Y divisions are independent ──────────────────────
    g.setColour (T.gridLine);
    for (int i = 1; i < _xDivisions; ++i)
        g.drawVerticalLine (juce::roundToInt (plotX + plotW * (float)i / (float)_xDivisions),
                            plotY, plotY + plotH);
    for (int i = 1; i < _yDivisions; ++i)
        g.drawHorizontalLine (juce::roundToInt (plotY + plotH * (float)i / (float)_yDivisions),
                              plotX, plotX + plotW);

    // ── Recorded curve ────────────────────────────────────────────────────────
    if (proc.hasCurve())
    {
        const auto table = proc.getCurveTable();
        juce::Path curvePath;
        bool first = true;
        for (int i = 0; i < 256; ++i)
        {
            float cx = plotX + static_cast<float> (i) / 255.0f * plotW;
            float cy = plotY + (1.0f - table[static_cast<size_t> (i)]) * plotH;
            if (first) { curvePath.startNewSubPath (cx, cy); first = false; }
            else          curvePath.lineTo (cx, cy);
        }
        g.setColour (T.curve);
        g.strokePath (curvePath, juce::PathStrokeType (2.5f,
                                                       juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));
    }

    // ── Live capture trail (clipped to plot area) ─────────────────────────────
    if (isCapturing && !capturePath.isEmpty())
    {
        g.saveState();
        g.reduceClipRegion (plot.toNearestInt());
        g.setColour (T.capture);
        g.strokePath (capturePath, juce::PathStrokeType (2.0f,
                                                         juce::PathStrokeType::curved,
                                                         juce::PathStrokeType::rounded));
        g.restoreState();
    }

    // ── Playhead ──────────────────────────────────────────────────────────────
    if (proc.isPlaying() && proc.hasCurve())
    {
        const float  phase  = proc.currentPhase();
        const float  headX  = plotX + phase * plotW;
        const auto   table  = proc.getCurveTable();
        const int    idx    = juce::jlimit (0, 255, static_cast<int> (phase * 255.0f));
        const float  headY  = plotY + (1.0f - table[static_cast<size_t> (idx)]) * plotH;

        g.setColour (T.playhead.withAlpha (0.75f));
        g.drawVerticalLine (juce::roundToInt (headX), plotY, plotY + plotH);

        g.setColour (T.playheadDot);
        g.fillEllipse (headX - 5.0f, headY - 5.0f, 10.0f, 10.0f);
    }

    // ── "Draw a curve" hint ────────────────────────────────────────────────────
    if (!proc.hasCurve() && !isCapturing)
    {
        g.setColour (T.hint);
        g.setFont (juce::Font (16.0f));
        g.drawText ("Draw a curve here", plot, juce::Justification::centred, false);
    }

    // ── Axis labels + scale banding ───────────────────────────────────────────
    {
        const auto  msgType = static_cast<MessageType> (
            static_cast<int> (proc.apvts.getRawParameterValue ("messageType")->load()));
        const float minOut  = proc.apvts.getRawParameterValue ("minOutput")->load();
        const float maxOut  = proc.apvts.getRawParameterValue ("maxOutput")->load();

        // Effective loop duration (uses host-synced ratio when applicable).
        const float recDur  = proc.curveDuration();
        const float speed   = proc.getEffectiveSpeedRatio();
        const float dur     = (recDur > 0.0f) ? recDur / std::max (speed, 0.001f) : 0.0f;

        // ── Note-name helper ──────────────────────────────────────────────────
        static const char* kNoteNames[] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
        auto midiNoteName = [] (int note) -> juce::String
        {
            const int pc  = note % 12;
            const int oct = note / 12 - 1;   // MIDI 60 = C4
            return juce::String (kNoteNames[pc]) + juce::String (oct);
        };

        // ── Scale banding + Y-axis in Note mode ──────────────────────────────
        const bool isNote = (msgType == MessageType::Note);
        const ScaleConfig sc = isNote ? proc.getScaleConfig() : ScaleConfig{};
        const bool hasScale  = isNote && (sc.mask != 0xFFF);

        // Y-coordinate for a normalised plot value (0=bottom, 1=top).
        auto normToY = [&] (float norm) -> float
        {
            return plotY + (1.0f - norm) * plotH;
        };

        // Normalised value for MIDI note N.
        auto noteToNorm = [&] (int n) -> float
        {
            return (static_cast<float> (n) / 127.0f - minOut)
                   / std::max (maxOut - minOut, 0.001f);
        };

        if (hasScale)
        {
            // Collect all scale notes within the visible MIDI range.
            const int loNote = std::max (0,   juce::roundToInt (minOut * 127.0f) - 1);
            const int hiNote = std::min (127, juce::roundToInt (maxOut * 127.0f) + 1);

            struct BandNote { int note; float y; };
            std::vector<BandNote> visible;
            visible.reserve (24);

            for (int n = hiNote; n >= loNote; --n)
            {
                const int interval = ((n % 12) - (int)sc.root + 12) % 12;
                if ((sc.mask >> interval) & 1)
                {
                    const float norm = noteToNorm (n);
                    if (norm >= -0.05f && norm <= 1.05f)
                        visible.push_back ({ n, normToY (norm) });
                }
            }

            // Draw alternating bands between consecutive scale notes.
            if (visible.size() >= 2)
            {
                const juce::Colour bandA = T.gridLine.withAlpha (0.18f);
                const juce::Colour bandB = T.gridLine.withAlpha (0.08f);

                // Top edge of the topmost band = top of plot area.
                float prevBandTop = plotY;

                for (size_t i = 0; i < visible.size(); ++i)
                {
                    // Band extends from the midpoint above this note to the midpoint below.
                    const float noteY  = visible[i].y;
                    const float halfUp = (i == 0)
                        ? (noteY - plotY) * 0.5f
                        : (visible[i - 1].y - noteY) * 0.5f;
                    const float halfDn = (i + 1 < visible.size())
                        ? (noteY - visible[i + 1].y) * 0.5f
                        : (plotY + plotH - noteY) * 0.5f;

                    const float bandTop = noteY - halfUp;
                    const float bandBot = noteY + halfDn;

                    g.setColour ((i & 1) ? bandB : bandA);
                    g.fillRect (plotX, bandTop, plotW, bandBot - bandTop);
                }
            }

            // Y-axis note name labels.
            g.setFont (juce::Font (9.5f));
            g.setColour (T.hint);
            const int lblW = juce::roundToInt (kAxisMarginL) - 2;
            const int lblH = 11;

            for (const auto& bn : visible)
            {
                const int labelY = juce::jlimit (1, juce::roundToInt (plotH) - lblH - 1,
                                                 juce::roundToInt (bn.y) - lblH / 2);
                g.drawText (midiNoteName (bn.note), 0, labelY, lblW, lblH,
                            juce::Justification::centredRight, false);
            }
        }
        else
        {
            // ── Standard Y-axis (non-scale, or non-Note mode) ──────────────
            auto yLabel = [&] (float norm) -> juce::String
            {
                const float ranged = minOut + norm * (maxOut - minOut);
                switch (msgType)
                {
                    case MessageType::CC:
                    case MessageType::ChannelPressure:
                        return juce::String (juce::roundToInt (ranged * 127.0f));
                    case MessageType::PitchBend:
                    {
                        const int pb = juce::roundToInt (ranged * 16383.0f) - 8192;
                        return (pb >= 0 ? "+" : "") + juce::String (pb);
                    }
                    case MessageType::Note:
                        // Chromatic Note mode: show note names at division ticks.
                        return midiNoteName (juce::roundToInt (ranged * 127.0f));
                }
                return {};
            };

            g.setFont (juce::Font (10.0f));
            g.setColour (T.hint);

            const int lblW = juce::roundToInt (kAxisMarginL) - 2;
            const int lblH = 12;
            for (int i = 0; i <= _yDivisions; ++i)
            {
                const float norm   = (float)i / (float)_yDivisions;
                const int   yPx    = juce::roundToInt ((1.0f - norm) * plotH);
                const int   labelY = juce::jlimit (1, juce::roundToInt (plotH) - lblH - 1,
                                                   yPx - lblH / 2);
                g.drawText (yLabel (norm), 0, labelY, lblW, lblH,
                            juce::Justification::centredRight, false);
            }
        }

        // ── X axis (bottom margin) ────────────────────────────────────────────
        g.setFont (juce::Font (10.0f));
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

        // ── Duration overlay (top-right of plot) ─────────────────────────────
        g.setFont (juce::Font (10.0f));
        g.setColour (T.hint);
        if (dur > 0.0f)
        {
            g.drawText (juce::String (dur, 2) + "s",
                        juce::roundToInt (plotX + plotW - 46), 2, 46, 12,
                        juce::Justification::centredRight, false);
        }
    }

    // ── Border (full component) ────────────────────────────────────────────────
    g.setColour (T.border);
    g.drawRect (bounds, 1.0f);
}

//==============================================================================
// Touch / mouse

// Normalise a raw touch position to [0,1] relative to the plot area.
static float normX (float rawX, float componentW) noexcept
{
    return juce::jlimit (0.0f, 1.0f, (rawX - kAxisMarginL) / (componentW - kAxisMarginL));
}
static float normY (float rawY, float componentH) noexcept
{
    return juce::jlimit (0.0f, 1.0f, rawY / (componentH - kAxisMarginB));
}

void CurveDisplay::mouseDown (const juce::MouseEvent& e)
{
    captureStartTime = juce::Time::getMillisecondCounterHiRes();
    isCapturing      = true;
    capturePath.clear();
    capturePath.startNewSubPath (static_cast<float> (e.x), static_cast<float> (e.y));

    proc.beginCapture();
    proc.addCapturePoint (0.0,
                          normX (static_cast<float> (e.x), static_cast<float> (getWidth())),
                          normY (static_cast<float> (e.y), static_cast<float> (getHeight())));
    repaint();
}

void CurveDisplay::mouseDrag (const juce::MouseEvent& e)
{
    if (!isCapturing) return;

    capturePath.lineTo (static_cast<float> (e.x), static_cast<float> (e.y));

    const double t = (juce::Time::getMillisecondCounterHiRes() - captureStartTime) / 1000.0;
    proc.addCapturePoint (t,
                          normX (static_cast<float> (e.x), static_cast<float> (getWidth())),
                          normY (static_cast<float> (e.y), static_cast<float> (getHeight())));
    repaint();
}

void CurveDisplay::mouseUp (const juce::MouseEvent& e)
{
    if (!isCapturing) return;

    const double t = (juce::Time::getMillisecondCounterHiRes() - captureStartTime) / 1000.0;
    proc.addCapturePoint (t,
                          normX (static_cast<float> (e.x), static_cast<float> (getWidth())),
                          normY (static_cast<float> (e.y), static_cast<float> (getHeight())));
    proc.finalizeCapture();

    isCapturing = false;
    capturePath.clear();
    repaint();
}

void CurveDisplay::timerCallback()
{
    repaint();   // always repaint: playhead + axis labels track live param changes
}

//==============================================================================
// DrawnCurveEditor

namespace Layout
{
    static constexpr int editorW      = 640;
    static constexpr int editorH      = 560;
    static constexpr int pad          = 6;
    static constexpr int buttonRowH   = 40;
    static constexpr int buttonRow2H  = 34;    // direction buttons
    static constexpr int scaleRowH    = 28;    // scale preset + root note (Note mode only)
    static constexpr int paramLabelH  = 14;
    static constexpr int paramSliderH = 30;
    static constexpr int paramRowH    = paramLabelH + paramSliderH;  // 44
    // curveH fills whatever space remains after controls.
}

DrawnCurveEditor::DrawnCurveEditor (DrawnCurveProcessor& p)
    : AudioProcessorEditor (&p),
      proc (p),
      curveDisplay (p)
{
    setSize (Layout::editorW, Layout::editorH);

    // ── Buttons (row 1) ───────────────────────────────────────────────────────
    addAndMakeVisible (playButton);
    playButton.onClick = [this]
    {
        const bool nowPlaying = !proc.isPlaying();
        proc.setPlaying (nowPlaying);
        playButton.setButtonText (nowPlaying ? "Pause" : "Play");
        curveDisplay.repaint();
    };

    addAndMakeVisible (clearButton);
    clearButton.onClick = [this]
    {
        proc.setPlaying (false);
        proc.clearSnapshot();
        playButton.setButtonText ("Play");
        curveDisplay.repaint();
    };

    addAndMakeVisible (themeButton);
    themeButton.onClick = [this]
    {
        _lightMode = !_lightMode;
        themeButton.setButtonText (_lightMode ? "Dark" : "Light");
        curveDisplay.setLightMode (_lightMode);
        helpOverlay.setLightMode (_lightMode);
        applyTheme();
    };

    addAndMakeVisible (syncButton);
    syncButton.onClick = [this]
    {
        const bool wasSyncing =
            proc.apvts.getRawParameterValue ("syncEnabled")->load() > 0.5f;
        if (auto* param = dynamic_cast<juce::AudioParameterBool*> (
                              proc.apvts.getParameter ("syncEnabled")))
            *param = !wasSyncing;
        onSyncToggled (!wasSyncing);
    };

    addAndMakeVisible (helpButton);
    helpButton.onClick = [this]
    {
        helpOverlay.setLightMode (_lightMode);
        helpOverlay.setVisible (! helpOverlay.isVisible());
        // Bring overlay to front of the Z-order each time it's shown.
        if (helpOverlay.isVisible())
            helpOverlay.toFront (false);
    };

    // ── Sliders ───────────────────────────────────────────────────────────────
    setupSlider (ccSlider,        ccLabel,        "CC#");
    ccSlider.setNumDecimalPlacesToDisplay (0);   // CC# and Velocity are both integers

    setupSlider (channelSlider,   channelLabel,   "Channel");
    setupSlider (smoothingSlider, smoothingLabel, "Smooth");
    setupSlider (speedSlider,     speedLabel,     "Speed");

    // ── Range slider (TwoValueHorizontal: left thumb = Min Out, right = Max Out) ──
    rangeSlider.setSliderStyle (juce::Slider::TwoValueHorizontal);
    rangeSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    rangeSlider.setRange (0.0, 1.0, 0.001);
    rangeSlider.setMinValue (proc.apvts.getRawParameterValue ("minOutput")->load(),
                             juce::dontSendNotification);
    rangeSlider.setMaxValue (proc.apvts.getRawParameterValue ("maxOutput")->load(),
                             juce::dontSendNotification);
    rangeSlider.onValueChange = [this]
    {
        if (auto* p = dynamic_cast<juce::AudioParameterFloat*> (
                          proc.apvts.getParameter ("minOutput")))
            *p = static_cast<float> (rangeSlider.getMinValue());
        if (auto* p = dynamic_cast<juce::AudioParameterFloat*> (
                          proc.apvts.getParameter ("maxOutput")))
            *p = static_cast<float> (rangeSlider.getMaxValue());
        updateRangeLabel();
    };
    addAndMakeVisible (rangeSlider);
    rangeLabel.setFont (juce::Font (11.0f));
    addAndMakeVisible (rangeLabel);
    updateRangeLabel();
    speedSlider.setTextValueSuffix ("x");
    speedSlider.setNumDecimalPlacesToDisplay (2);

    // ── APVTS attachments ─────────────────────────────────────────────────────
    auto& apvts = proc.apvts;
    ccAttach        = std::make_unique<Attach> (apvts, "ccNumber",       ccSlider);
    channelAttach   = std::make_unique<Attach> (apvts, "midiChannel",    channelSlider);
    smoothingAttach = std::make_unique<Attach> (apvts, "smoothing",      smoothingSlider);
    speedAttach     = std::make_unique<Attach> (apvts, "playbackSpeed",  speedSlider);
    // rangeSlider has no SliderAttachment; external changes come via APVTS listeners.
    apvts.addParameterListener ("minOutput", this);
    apvts.addParameterListener ("maxOutput", this);

    // ── Message-type radio buttons (4: CC / Aft / PB / Note) ────────────────
    static const std::array<const char*, 4> kMsgLabels { "CC", "Aft", "PB", "Note" };
    for (int i = 0; i < 4; ++i)
    {
        msgTypeBtns[i].setButtonText (kMsgLabels[i]);
        msgTypeBtns[i].setLookAndFeel (&_symbolLF);
        addAndMakeVisible (msgTypeBtns[i]);
        msgTypeBtns[i].onClick = [this, i]
        {
            if (auto* param = dynamic_cast<juce::AudioParameterChoice*> (
                                  proc.apvts.getParameter ("messageType")))
                *param = i;
        };
    }

    // ── Direction segmented control (row 2) ───────────────────────────────────
    // Replaces the former 3-button radio group.  A SegmentPainter draws proper
    // stem+arrowhead arrows (→  ←  ←→) as juce::Path — no font dependency.
    dirControl.setSegments ({
        { "fwd", "Fwd", "Forward — curve plays left to right"       },
        { "rev", "Rev", "Reverse — curve plays right to left"       },
        { "pp",  "P-P", "Ping-Pong — alternates forward and reverse" }
    });

    // Initialise selection from current APVTS state (handles state restore).
    dirControl.setSelectedIndex (
        static_cast<int> (proc.apvts.getRawParameterValue ("playbackDirection")->load()),
        juce::dontSendNotification);

    // Write selection back to APVTS when the user taps a segment.
    dirControl.onChange = [this] (int i)
    {
        if (auto* param = dynamic_cast<juce::AudioParameterChoice*> (
                              proc.apvts.getParameter ("playbackDirection")))
            *param = i;
    };

    // Custom painter: stem + arrowhead arrows drawn as filled paths.
    // The background (active highlight or plain bg) has already been painted
    // by SegmentedControl::paint() before this lambda is called.
    // `g` colour is pre-set to activeLabel or labelColour as appropriate.
    dirControl.setSegmentPainter ([] (juce::Graphics& g,
                                      juce::Rectangle<float> bounds,
                                      int index,
                                      bool /*active*/)
    {
        const float cx  = bounds.getCentreX();
        const float cy  = bounds.getCentreY();
        const float aw  = bounds.getHeight() * 0.26f;   // arrowhead half-height
        const float al  = aw  * 0.95f;                  // arrowhead x-depth
        const float sw  = 1.8f;                          // stem stroke width
        const float ext = bounds.getWidth()  * 0.22f;   // arrow half-span

        // Draw one arrow: fromX is the stem tail, toX is the tip.
        auto drawArrow = [&] (float fromX, float toX)
        {
            const bool  right  = (toX > fromX);
            const float tipX   = toX;
            const float baseX  = tipX + (right ? -al : al);   // stem/head junction

            g.drawLine (fromX, cy, baseX, cy, sw);             // stem

            juce::Path head;
            head.addTriangle (tipX,  cy,
                              baseX, cy - aw,
                              baseX, cy + aw);
            g.fillPath (head);                                  // arrowhead
        };

        if (index == 0)       // Forward  →
            drawArrow (cx - ext, cx + ext);
        else if (index == 1)  // Reverse  ←
            drawArrow (cx + ext, cx - ext);
        else                  // Ping-Pong  ← →
        {
            const float gap = al * 0.55f;        // spacing between the two stems
            drawArrow (cx - gap, cx - ext);      // left  ←
            drawArrow (cx + gap, cx + ext);      // right →
        }
    });

    addAndMakeVisible (dirControl);

    // ── Grid tick buttons: separate Y and X controls ─────────────────────────
    addAndMakeVisible (tickYMinusBtn);
    addAndMakeVisible (tickYPlusBtn);
    addAndMakeVisible (tickXMinusBtn);
    addAndMakeVisible (tickXPlusBtn);
    tickYMinusBtn.onClick = [this] { curveDisplay.setYDivisions (curveDisplay.getYDivisions() - 1); };
    tickYPlusBtn .onClick = [this] { curveDisplay.setYDivisions (curveDisplay.getYDivisions() + 1); };
    tickXMinusBtn.onClick = [this] { curveDisplay.setXDivisions (curveDisplay.getXDivisions() - 1); };
    tickXPlusBtn .onClick = [this] { curveDisplay.setXDivisions (curveDisplay.getXDivisions() + 1); };

    // ── Scale quantization rows ───────────────────────────────────────────────
    static const std::array<const char*, 8> kScaleNames
        { "Chrom", "Major", "Minor", "Dorian", "Penta+", "Penta-", "Blues", "Custom" };

    static const std::array<const char*, 12> kRootNames
        { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    // Scale preset buttons
    addAndMakeVisible (scaleLabel);
    scaleLabel.setFont (juce::Font (11.0f));
    for (int i = 0; i < kNumScalePresets; ++i)
    {
        scalePresetBtns[i].setButtonText (kScaleNames[i]);
        scalePresetBtns[i].setLookAndFeel (&_symbolLF);
        addAndMakeVisible (scalePresetBtns[i]);
        scalePresetBtns[i].onClick = [this, i]
        {
            if (auto* p = dynamic_cast<juce::AudioParameterInt*> (proc.apvts.getParameter ("scaleMode")))
                *p = i;
            proc.updateEngineScale();
            updateScalePresetButtons();
            updateScaleVisibility();   // custom mask row may appear/disappear
        };
    }

    // Root note buttons (C ... B)
    addAndMakeVisible (rootLabel);
    rootLabel.setFont (juce::Font (11.0f));
    for (int i = 0; i < kNumPitchClasses; ++i)
    {
        rootNoteBtns[i].setButtonText (kRootNames[i]);
        rootNoteBtns[i].setLookAndFeel (&_symbolLF);
        addAndMakeVisible (rootNoteBtns[i]);
        rootNoteBtns[i].onClick = [this, i]
        {
            if (auto* p = dynamic_cast<juce::AudioParameterInt*> (proc.apvts.getParameter ("scaleRoot")))
                *p = i;
            proc.updateEngineScale();
            updateRootNoteButtons();
            curveDisplay.repaint();   // Y-axis note names update
        };
    }

    // Custom mask buttons (one per pitch class)
    addAndMakeVisible (notesLabel);
    notesLabel.setFont (juce::Font (11.0f));
    for (int i = 0; i < kNumPitchClasses; ++i)
    {
        customMaskBtns[i].setButtonText (kRootNames[i]);
        customMaskBtns[i].setLookAndFeel (&_symbolLF);
        addAndMakeVisible (customMaskBtns[i]);
        customMaskBtns[i].onClick = [this, i]
        {
            // Toggle bit i in the current custom mask.
            const int cur = static_cast<int> (proc.apvts.getRawParameterValue ("scaleCustomMask")->load());
            const int next = cur ^ (1 << i);
            if (auto* p = dynamic_cast<juce::AudioParameterInt*> (proc.apvts.getParameter ("scaleCustomMask")))
                *p = next;
            proc.updateEngineScale();
            updateCustomMaskButtons();
            curveDisplay.repaint();
        };
    }

    // Register APVTS listeners for scale params so UI stays in sync after state restore.
    proc.apvts.addParameterListener ("scaleMode",       this);
    proc.apvts.addParameterListener ("scaleRoot",       this);
    proc.apvts.addParameterListener ("scaleCustomMask", this);

    // ── Curve display ─────────────────────────────────────────────────────────
    addAndMakeVisible (curveDisplay);

    // ── Help overlay — added last so it sits above all other children ────────
    // Use addChildComponent (not addAndMakeVisible) so the overlay stays hidden
    // until the user explicitly taps "?".  addAndMakeVisible would override the
    // setVisible(false) in HelpOverlay's own constructor, causing the overlay to
    // appear unsolicited on every first launch (a "push revelation" — NN/g anti-pattern).
    addChildComponent (helpOverlay);

    // Stay in sync with external parameter changes (automation, state restore).
    proc.apvts.addParameterListener ("messageType",       this);
    proc.apvts.addParameterListener ("playbackDirection", this);
    // minOutput/maxOutput are already registered above (range slider listeners).

    // Apply correct colours and CC-slot state for the initial theme.
    applyTheme();

    // Restore sync UI state (e.g. after state load with syncEnabled=true).
    onSyncToggled (proc.apvts.getRawParameterValue ("syncEnabled")->load() > 0.5f);

    // Show/hide scale rows based on current messageType.
    updateScaleVisibility();
}

DrawnCurveEditor::~DrawnCurveEditor()
{
    // Reset custom L&F before _symbolLF is destroyed.
    for (auto& b : msgTypeBtns) b.setLookAndFeel (nullptr);

    proc.apvts.removeParameterListener ("messageType",       this);
    proc.apvts.removeParameterListener ("playbackDirection", this);
    proc.apvts.removeParameterListener ("minOutput",         this);
    proc.apvts.removeParameterListener ("maxOutput",         this);
    proc.apvts.removeParameterListener ("scaleMode",         this);
    proc.apvts.removeParameterListener ("scaleRoot",         this);
    proc.apvts.removeParameterListener ("scaleCustomMask",   this);

    // Reset L&F on scale buttons.
    for (auto& b : scalePresetBtns) b.setLookAndFeel (nullptr);
    for (auto& b : rootNoteBtns)    b.setLookAndFeel (nullptr);
    for (auto& b : customMaskBtns)  b.setLookAndFeel (nullptr);
}

void DrawnCurveEditor::setupSlider (juce::Slider&       s,
                                     juce::Label&         l,
                                     const juce::String&  labelText,
                                     juce::Slider::SliderStyle style)
{
    s.setSliderStyle (style);
    s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 18);
    addAndMakeVisible (s);

    l.setText (labelText, juce::dontSendNotification);
    l.setFont (juce::Font (11.0f));
    addAndMakeVisible (l);
}

void DrawnCurveEditor::parameterChanged (const juce::String& paramID, float)
{
    if (paramID == "messageType")
        juce::MessageManager::callAsync ([this] { updateMsgTypeButtons(); updateScaleVisibility(); });
    else if (paramID == "playbackDirection")
        juce::MessageManager::callAsync ([this] {
            dirControl.setSelectedIndex (
                static_cast<int> (proc.apvts.getRawParameterValue ("playbackDirection")->load()),
                juce::dontSendNotification);
        });
    else if (paramID == "minOutput" || paramID == "maxOutput")
        juce::MessageManager::callAsync ([this] { updateRangeSlider(); });
    else if (paramID == "scaleMode")
        juce::MessageManager::callAsync ([this] { updateScalePresetButtons(); updateScaleVisibility(); });
    else if (paramID == "scaleRoot")
        juce::MessageManager::callAsync ([this] { updateRootNoteButtons(); curveDisplay.repaint(); });
    else if (paramID == "scaleCustomMask")
        juce::MessageManager::callAsync ([this] { updateCustomMaskButtons(); curveDisplay.repaint(); });
}

void DrawnCurveEditor::updateMsgTypeButtons()
{
    const int sel = static_cast<int> (
        proc.apvts.getRawParameterValue ("messageType")->load());

    const juce::Colour inactiveBg   = _lightMode ? juce::Colour (0xffe0e0e8)      : juce::Colour (0xff333355);
    const juce::Colour inactiveText = _lightMode ? juce::Colour (0xff3a3a3c)      : juce::Colours::lightgrey;

    for (int i = 0; i < 4; ++i)
    {
        const bool active = (i == sel);
        msgTypeBtns[i].setColour (juce::TextButton::buttonColourId,
            active ? juce::Colour (0xff2979ff) : inactiveBg);
        msgTypeBtns[i].setColour (juce::TextButton::buttonOnColourId,
            juce::Colour (0xff2979ff));
        msgTypeBtns[i].setColour (juce::TextButton::textColourOffId,
            active ? juce::Colours::white : inactiveText);
    }

    updateCCSlot();
}

void DrawnCurveEditor::updateCCSlot()
{
    const int  sel    = static_cast<int> (
        proc.apvts.getRawParameterValue ("messageType")->load());
    const bool isNote = (sel == 3);
    const bool isCC   = (sel == 0);

    // Swap attachment: ccNumber <-> noteVelocity
    ccAttach.reset();
    if (isNote)
    {
        ccAttach = std::make_unique<Attach> (proc.apvts, "noteVelocity", ccSlider);
        ccLabel.setText ("Vel", juce::dontSendNotification);
        ccSlider.setEnabled (true);
        ccLabel .setEnabled (true);
        ccSlider.setAlpha (1.0f);
        ccLabel .setAlpha (1.0f);
    }
    else
    {
        ccAttach = std::make_unique<Attach> (proc.apvts, "ccNumber", ccSlider);
        ccLabel.setText ("CC#", juce::dontSendNotification);
        ccSlider.setEnabled (isCC);
        ccLabel .setEnabled (isCC);
        ccSlider.setAlpha (isCC ? 1.0f : 0.4f);
        ccLabel .setAlpha (isCC ? 1.0f : 0.4f);
    }
}


void DrawnCurveEditor::updateRangeSlider()
{
    // Called on the UI thread when minOutput or maxOutput changes externally
    // (automation, state restore).  Uses dontSendNotification to avoid
    // recursively triggering onValueChange / writing back to the parameter.
    rangeSlider.setMinValue (proc.apvts.getRawParameterValue ("minOutput")->load(),
                             juce::dontSendNotification);
    rangeSlider.setMaxValue (proc.apvts.getRawParameterValue ("maxOutput")->load(),
                             juce::dontSendNotification);
    updateRangeLabel();
}

void DrawnCurveEditor::updateRangeLabel()
{
    const float mn = static_cast<float> (rangeSlider.getMinValue());
    const float mx = static_cast<float> (rangeSlider.getMaxValue());
    rangeLabel.setText (juce::String (mn, 2) + " \xe2\x80\x93 " + juce::String (mx, 2),
                        juce::dontSendNotification);   // "0.00 – 1.00" (en-dash)
}

//==============================================================================
// Scale quantization helpers
//==============================================================================

void DrawnCurveEditor::updateScaleVisibility()
{
    const bool isNote   = (static_cast<int> (proc.apvts.getRawParameterValue ("messageType")->load()) == 3);
    const bool isCustom = isNote
                       && (static_cast<int> (proc.apvts.getRawParameterValue ("scaleMode")->load()) == 7);

    scaleLabel.setVisible (isNote);
    for (auto& b : scalePresetBtns) b.setVisible (isNote);
    rootLabel .setVisible (isNote);
    for (auto& b : rootNoteBtns)    b.setVisible (isNote);

    notesLabel.setVisible (isNote && isCustom);
    for (auto& b : customMaskBtns)  b.setVisible (isNote && isCustom);

    resized();   // redistribute vertical space now that row visibility changed
}

void DrawnCurveEditor::updateScalePresetButtons()
{
    const int sel = static_cast<int> (proc.apvts.getRawParameterValue ("scaleMode")->load());

    const juce::Colour inactiveBg   = _lightMode ? juce::Colour (0xffe0e0e8) : juce::Colour (0xff333355);
    const juce::Colour inactiveText = _lightMode ? juce::Colour (0xff3a3a3c) : juce::Colours::lightgrey;

    for (int i = 0; i < kNumScalePresets; ++i)
    {
        const bool active = (i == sel);
        scalePresetBtns[i].setColour (juce::TextButton::buttonColourId,
            active ? juce::Colour (0xff2979ff) : inactiveBg);
        scalePresetBtns[i].setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff2979ff));
        scalePresetBtns[i].setColour (juce::TextButton::textColourOffId,
            active ? juce::Colours::white : inactiveText);
    }
}

void DrawnCurveEditor::updateRootNoteButtons()
{
    const int sel = static_cast<int> (proc.apvts.getRawParameterValue ("scaleRoot")->load());

    const juce::Colour inactiveBg   = _lightMode ? juce::Colour (0xffe0e0e8) : juce::Colour (0xff333355);
    const juce::Colour inactiveText = _lightMode ? juce::Colour (0xff3a3a3c) : juce::Colours::lightgrey;

    for (int i = 0; i < kNumPitchClasses; ++i)
    {
        const bool active = (i == sel);
        rootNoteBtns[i].setColour (juce::TextButton::buttonColourId,
            active ? juce::Colour (0xff2979ff) : inactiveBg);
        rootNoteBtns[i].setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff2979ff));
        rootNoteBtns[i].setColour (juce::TextButton::textColourOffId,
            active ? juce::Colours::white : inactiveText);
    }
}

void DrawnCurveEditor::updateCustomMaskButtons()
{
    const int mask = static_cast<int> (proc.apvts.getRawParameterValue ("scaleCustomMask")->load());

    const juce::Colour inactiveBg   = _lightMode ? juce::Colour (0xffe0e0e8) : juce::Colour (0xff333355);
    const juce::Colour inactiveText = _lightMode ? juce::Colour (0xff3a3a3c) : juce::Colours::lightgrey;

    for (int i = 0; i < kNumPitchClasses; ++i)
    {
        const bool active = (mask >> i) & 1;
        customMaskBtns[i].setColour (juce::TextButton::buttonColourId,
            active ? juce::Colour (0xff2979ff) : inactiveBg);
        customMaskBtns[i].setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff2979ff));
        customMaskBtns[i].setColour (juce::TextButton::textColourOffId,
            active ? juce::Colours::white : inactiveText);
    }
}

//==============================================================================
void DrawnCurveEditor::onSyncToggled (bool isSync)
{
    syncButton.setButtonText (isSync ? "Sync ON" : "Sync");

    // Swap speed-slider attachment: playbackSpeed (manual) <-> syncBeats (sync).
    speedAttach.reset();
    if (isSync)
    {
        speedAttach = std::make_unique<Attach> (proc.apvts, "syncBeats", speedSlider);
        speedLabel.setText ("Beats", juce::dontSendNotification);
        speedSlider.setTextValueSuffix ("");
        speedSlider.setNumDecimalPlacesToDisplay (0);
    }
    else
    {
        speedAttach = std::make_unique<Attach> (proc.apvts, "playbackSpeed", speedSlider);
        speedLabel.setText ("Speed", juce::dontSendNotification);
        speedSlider.setTextValueSuffix ("x");
        speedSlider.setNumDecimalPlacesToDisplay (2);
    }

    // Dim the Play button — host transport controls play when sync is on.
    playButton.setEnabled (!isSync);
    playButton.setAlpha   (isSync ? 0.4f : 1.0f);

    helpOverlay.setLightMode (_lightMode);
    applyTheme();
}

//==============================================================================
void DrawnCurveEditor::applyTheme()
{
    const bool light = _lightMode;

    const juce::Colour textCol  = light ? juce::Colour (0xff1c1c1e) : juce::Colours::white;
    const juce::Colour dimText  = light ? juce::Colour (0xff3a3a3c) : juce::Colours::lightgrey;
    const juce::Colour tbBg     = light ? juce::Colours::white       : juce::Colour (0xff252538);
    const juce::Colour tbLine   = light ? juce::Colour (0x28000000)  : juce::Colour (0x33ffffff);
    const juce::Colour accent   = light ? juce::Colour (0xff007aff)  : juce::Colour (0xff00e5ff);
    const juce::Colour btnBg    = light ? juce::Colour (0xffe0e0e8)  : juce::Colour (0xff333355);
    const juce::Colour btnText  = light ? juce::Colour (0xff1c1c1e)  : juce::Colours::white;

    // ── Sliders (single-value) ────────────────────────────────────────────────
    for (auto* s : { &ccSlider, &channelSlider, &smoothingSlider, &speedSlider })
    {
        s->setColour (juce::Slider::textBoxTextColourId,       textCol);
        s->setColour (juce::Slider::textBoxBackgroundColourId, tbBg);
        s->setColour (juce::Slider::textBoxOutlineColourId,    tbLine);
        s->setColour (juce::Slider::thumbColourId,             accent);
        s->setColour (juce::Slider::trackColourId,             accent.withAlpha (0.45f));
        s->setColour (juce::Slider::backgroundColourId,        tbBg);
    }

    // ── Range slider (TwoValueHorizontal — no text box) ────────────────────────
    rangeSlider.setColour (juce::Slider::thumbColourId,      accent);
    rangeSlider.setColour (juce::Slider::trackColourId,      accent.withAlpha (0.45f));
    rangeSlider.setColour (juce::Slider::backgroundColourId, tbBg);

    // ── Param labels ──────────────────────────────────────────────────────────
    for (auto* l : { &ccLabel, &channelLabel, &smoothingLabel,
                     &rangeLabel, &speedLabel })
        l->setColour (juce::Label::textColourId, dimText);

    // ── Utility buttons ───────────────────────────────────────────────────────
    for (auto* b : { &playButton, &clearButton, &themeButton, &syncButton,
                     &helpButton,
                     &tickYMinusBtn, &tickYPlusBtn, &tickXMinusBtn, &tickXPlusBtn })
    {
        b->setColour (juce::TextButton::buttonColourId,  btnBg);
        b->setColour (juce::TextButton::textColourOffId, btnText);
    }

    // ── Direction segmented control ───────────────────────────────────────────
    dirControl.bgColour     = btnBg;
    dirControl.activeColour = juce::Colour (0xff2979ff);   // same accent as other radio groups
    dirControl.labelColour  = light ? juce::Colour (0xff3a3a3c) : juce::Colours::lightgrey;
    dirControl.activeLabel  = juce::Colours::white;
    dirControl.borderColour = light ? juce::Colour (0x28000000) : juce::Colour (0x33ffffff);
    dirControl.repaint();

    // ── Message-type + scale radio buttons ───────────────────────────────────
    updateMsgTypeButtons();
    updateScalePresetButtons();
    updateRootNoteButtons();
    updateCustomMaskButtons();

    // Scale / root / notes labels
    for (auto* l : { &scaleLabel, &rootLabel, &notesLabel })
        l->setColour (juce::Label::textColourId, dimText);

    repaint();
}

//==============================================================================
void DrawnCurveEditor::paint (juce::Graphics& g)
{
    const Theme& T = _lightMode ? kLight : kDark;
    g.fillAll (T.panelBg);
}

void DrawnCurveEditor::resized()
{
    using namespace Layout;
    auto area = getLocalBounds().reduced (pad);

    // ── Button row 1 (Play · Clear · [CC][Aft][PB][Note] · [Sync] · [?] · [Dark/Light]) ──
    {
        auto row = area.removeFromTop (buttonRowH);

        // Right side first (remove in reverse display order).
        themeButton.setBounds (row.removeFromRight (68));
        row.removeFromRight (pad);
        helpButton .setBounds (row.removeFromRight (30));
        row.removeFromRight (pad);
        syncButton .setBounds (row.removeFromRight (62));
        row.removeFromRight (pad);

        playButton .setBounds (row.removeFromLeft (100));
        row.removeFromLeft (pad);
        clearButton.setBounds (row.removeFromLeft (80));
        row.removeFromLeft (pad * 3);

        // 4 equal-width message-type buttons in remaining space.
        const int msgBtnW = (row.getWidth() - pad * 3) / 4;
        for (int i = 0; i < 4; ++i)
        {
            msgTypeBtns[i].setBounds (row.removeFromLeft (msgBtnW));
            if (i < 3) row.removeFromLeft (pad);
        }
    }
    area.removeFromTop (pad);

    // ── Button row 2 (Fwd/Rev/P-P  |  Y- Y+  X- X+) ─────────────────────────
    {
        auto row = area.removeFromTop (buttonRow2H);

        // Tick pairs on the right: [X-][X+] then gap then [Y-][Y+].
        // 4 buttons × 28 px + 3 inner gaps × 3 px + 1 group gap × pad.
        tickXPlusBtn .setBounds (row.removeFromRight (28));
        row.removeFromRight (3);
        tickXMinusBtn.setBounds (row.removeFromRight (28));
        row.removeFromRight (pad);
        tickYPlusBtn .setBounds (row.removeFromRight (28));
        row.removeFromRight (3);
        tickYMinusBtn.setBounds (row.removeFromRight (28));
        row.removeFromRight (pad);

        // Direction segmented control fills the rest of row 2.
        dirControl.setBounds (row);
    }
    area.removeFromTop (pad);

    // Helper: place label above slider using three equal slots.
    auto placeRow3 = [&] (auto& lbl1, auto& sl1,
                          auto& lbl2, auto& sl2,
                          auto& lbl3, auto& sl3)
    {
        auto row = area.removeFromTop (paramRowH);
        const int slotW = (area.getWidth() - pad * 2) / 3;
        auto placeOne = [&] (juce::Label& lbl, juce::Slider& sl)
        {
            auto slot = row.removeFromLeft (slotW);
            row.removeFromLeft (pad);
            lbl.setBounds (slot.removeFromTop (paramLabelH));
            sl .setBounds (slot);
        };
        placeOne (lbl1, sl1);
        placeOne (lbl2, sl2);
        lbl3.setBounds (row.removeFromTop (paramLabelH));
        sl3 .setBounds (row);
    };

    // ── Scale rows (visible in Note mode only) ────────────────────────────────
    const bool isNote   = (static_cast<int> (proc.apvts.getRawParameterValue ("messageType")->load()) == 3);
    const bool isCustom = isNote && (static_cast<int> (proc.apvts.getRawParameterValue ("scaleMode")->load()) == 7);

    if (isNote)
    {
        area.removeFromTop (pad);
        auto scaleRow = area.removeFromTop (scaleRowH);

        // Left: "Scale" label (32 px) + 8 preset buttons (equal width)
        scaleLabel.setBounds (scaleRow.removeFromLeft (32).withSizeKeepingCentre (32, 14));
        scaleRow.removeFromLeft (3);
        const int presetW = (scaleRow.getWidth() / 5 * 3 - kNumScalePresets * 2) / kNumScalePresets;
        for (int i = 0; i < kNumScalePresets; ++i)
        {
            scalePresetBtns[i].setBounds (scaleRow.removeFromLeft (presetW));
            if (i < kNumScalePresets - 1) scaleRow.removeFromLeft (2);
        }

        // Right part of same row: "Key" label + 12 root note buttons
        scaleRow.removeFromLeft (pad);
        rootLabel.setBounds (scaleRow.removeFromLeft (24).withSizeKeepingCentre (24, 14));
        scaleRow.removeFromLeft (3);
        const int rootW = (scaleRow.getWidth() - (kNumPitchClasses - 1) * 2) / kNumPitchClasses;
        for (int i = 0; i < kNumPitchClasses; ++i)
        {
            rootNoteBtns[i].setBounds (scaleRow.removeFromLeft (rootW));
            if (i < kNumPitchClasses - 1) scaleRow.removeFromLeft (2);
        }

        // Custom mask row (shown only when Custom scale is selected)
        if (isCustom)
        {
            area.removeFromTop (3);
            auto maskRow = area.removeFromTop (scaleRowH);

            notesLabel.setBounds (maskRow.removeFromLeft (32).withSizeKeepingCentre (32, 14));
            maskRow.removeFromLeft (3);
            const int maskW = (maskRow.getWidth() - (kNumPitchClasses - 1) * 2) / kNumPitchClasses;
            for (int i = 0; i < kNumPitchClasses; ++i)
            {
                customMaskBtns[i].setBounds (maskRow.removeFromLeft (maskW));
                if (i < kNumPitchClasses - 1) maskRow.removeFromLeft (2);
            }
        }
    }

    // ── Param row 1: CC#/Vel, Channel, Smooth ─────────────────────────────────
    placeRow3 (ccLabel, ccSlider, channelLabel, channelSlider, smoothingLabel, smoothingSlider);
    area.removeFromTop (pad);

    // ── Param row 2: Range (min/max out, spans 2/3) + Speed/Beats (1/3) ────────
    {
        auto row = area.removeFromTop (paramRowH);
        const int thirdW = (row.getWidth() - pad * 2) / 3;
        const int rangeW = thirdW * 2 + pad;   // two thirds + the gap between them

        auto rangeSlot = row.removeFromLeft (rangeW);
        row.removeFromLeft (pad);
        rangeLabel .setBounds (rangeSlot.removeFromTop (paramLabelH));
        rangeSlider.setBounds (rangeSlot);

        speedLabel .setBounds (row.removeFromTop (paramLabelH));
        speedSlider.setBounds (row);
    }
    area.removeFromTop (pad);

    // ── Curve display (fills all remaining vertical space) ────────────────────
    curveDisplay.setBounds (area);

    // ── Help overlay (covers the entire editor) ───────────────────────────────
    helpOverlay.setBounds (getLocalBounds());
}
