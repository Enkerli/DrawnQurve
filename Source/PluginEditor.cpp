#include "PluginEditor.h"

//==============================================================================
// Colour palettes

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

    // ── Subtle grid (confined to the plot area) ────────────────────────────────
    g.setColour (T.gridLine);
    for (int i = 1; i < 4; ++i)
    {
        g.drawVerticalLine   (juce::roundToInt (plotX + plotW * 0.25f * i), plotY, plotY + plotH);
        g.drawHorizontalLine (juce::roundToInt (plotY + plotH * 0.25f * i), plotX, plotX + plotW);
    }

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

    // ── Axis labels ────────────────────────────────────────────────────────────
    {
        const auto  msgType = static_cast<MessageType> (
            static_cast<int> (proc.apvts.getRawParameterValue ("messageType")->load()));
        const float minOut  = proc.apvts.getRawParameterValue ("minOutput")->load();
        const float maxOut  = proc.apvts.getRawParameterValue ("maxOutput")->load();

        // Effective loop duration (uses host-synced ratio when applicable).
        const float recDur  = proc.curveDuration();
        const float speed   = proc.getEffectiveSpeedRatio();
        const float dur     = (recDur > 0.0f) ? recDur / std::max (speed, 0.001f) : 0.0f;

        // Convert a normalised output (0=bottom of plot, 1=top) to a display string.
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
            }
            return {};
        };

        g.setFont (juce::Font (10.0f));
        g.setColour (T.hint);

        // ── Y axis (left margin) ─────────────────────────────────────────────
        const int lblW = juce::roundToInt (kAxisMarginL) - 2;
        const int lblH = 12;
        g.drawText (yLabel (1.0f), 0, 1,
                    lblW, lblH, juce::Justification::centredRight, false);
        g.drawText (yLabel (0.5f), 0, juce::roundToInt (plotH * 0.5f - 6),
                    lblW, lblH, juce::Justification::centredRight, false);
        g.drawText (yLabel (0.0f), 0, juce::roundToInt (plotH - 13),
                    lblW, lblH, juce::Justification::centredRight, false);

        // ── X axis (bottom margin) — percentage marks ────────────────────────
        static const std::array<const char*, 5> kPct { "0%", "25%", "50%", "75%", "100%" };
        const int xLblY = juce::roundToInt (h - kAxisMarginB + 2);
        const int xLblH = juce::roundToInt (kAxisMarginB - 3);
        for (int i = 0; i <= 4; ++i)
        {
            const float xPx = plotX + (i / 4.0f) * plotW;
            g.drawText (kPct[static_cast<size_t> (i)],
                        juce::roundToInt (xPx - 18), xLblY,
                        36, xLblH,
                        juce::Justification::centred, false);
        }

        // ── Duration overlay (top-right of plot) ────────────────────────────
        if (dur > 0.0f)
        {
            g.drawText (juce::String (dur, 2) + "s",
                        juce::roundToInt (plotX + plotW - 46), 2,
                        46, 12,
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
    static constexpr int editorH      = 560;   // +40 for direction row
    static constexpr int pad          = 6;
    static constexpr int buttonRowH   = 40;
    static constexpr int buttonRow2H  = 34;    // direction buttons
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
        applyTheme();   // re-colours sliders, labels, buttons
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

    // ── Sliders ───────────────────────────────────────────────────────────────
    setupSlider (ccSlider,        ccLabel,        "CC#");
    setupSlider (channelSlider,   channelLabel,   "Channel");
    setupSlider (smoothingSlider, smoothingLabel, "Smooth");
    setupSlider (minOutSlider,    minOutLabel,    "Min Out");
    setupSlider (maxOutSlider,    maxOutLabel,    "Max Out");
    setupSlider (speedSlider,     speedLabel,     "Speed");
    speedSlider.setTextValueSuffix ("x");
    speedSlider.setNumDecimalPlacesToDisplay (2);

    // ── APVTS attachments ─────────────────────────────────────────────────────
    auto& apvts = proc.apvts;
    ccAttach        = std::make_unique<Attach> (apvts, "ccNumber",       ccSlider);
    channelAttach   = std::make_unique<Attach> (apvts, "midiChannel",    channelSlider);
    smoothingAttach = std::make_unique<Attach> (apvts, "smoothing",      smoothingSlider);
    minAttach       = std::make_unique<Attach> (apvts, "minOutput",      minOutSlider);
    maxAttach       = std::make_unique<Attach> (apvts, "maxOutput",      maxOutSlider);
    speedAttach     = std::make_unique<Attach> (apvts, "playbackSpeed",  speedSlider);

    // ── Message-type radio buttons ────────────────────────────────────────────
    // ComboBox popups fail silently in AUv3 on iOS (no TopLevelWindow), so we
    // use three TextButtons as a popup-free radio group instead.
    static const std::array<const char*, 3> kMsgLabels { "CC", "Ch Press", "Pitch Bend" };
    for (int i = 0; i < 3; ++i)
    {
        msgTypeBtns[i].setButtonText (kMsgLabels[i]);
        addAndMakeVisible (msgTypeBtns[i]);
        msgTypeBtns[i].onClick = [this, i]
        {
            if (auto* param = dynamic_cast<juce::AudioParameterChoice*> (
                                  proc.apvts.getParameter ("messageType")))
                *param = i;
        };
    }

    // ── Direction radio buttons (row 2) ───────────────────────────────────────
    static const std::array<const char*, 3> kDirLabels { "-> Fwd", "<- Rev", "<> P-P" };
    for (int i = 0; i < 3; ++i)
    {
        dirBtns[i].setButtonText (kDirLabels[i]);
        addAndMakeVisible (dirBtns[i]);
        dirBtns[i].onClick = [this, i]
        {
            if (auto* param = dynamic_cast<juce::AudioParameterChoice*> (
                                  proc.apvts.getParameter ("playbackDirection")))
                *param = i;
        };
    }

    // ── Curve display ─────────────────────────────────────────────────────────
    addAndMakeVisible (curveDisplay);

    // Stay in sync with external parameter changes (automation, state restore).
    proc.apvts.addParameterListener ("messageType",       this);
    proc.apvts.addParameterListener ("playbackDirection", this);

    // Apply correct colours for the initial (dark) theme.
    applyTheme();

    // Restore sync UI state (e.g. after state load with syncEnabled=true).
    onSyncToggled (proc.apvts.getRawParameterValue ("syncEnabled")->load() > 0.5f);
}

DrawnCurveEditor::~DrawnCurveEditor()
{
    proc.apvts.removeParameterListener ("messageType",       this);
    proc.apvts.removeParameterListener ("playbackDirection", this);
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
        juce::MessageManager::callAsync ([this] { updateMsgTypeButtons(); });
    else if (paramID == "playbackDirection")
        juce::MessageManager::callAsync ([this] { updateDirButtons(); });
}

void DrawnCurveEditor::updateMsgTypeButtons()
{
    const int sel = static_cast<int> (
        proc.apvts.getRawParameterValue ("messageType")->load());

    const juce::Colour inactiveBg   = _lightMode ? juce::Colour (0xffe0e0e8)      : juce::Colour (0xff333355);
    const juce::Colour inactiveText = _lightMode ? juce::Colour (0xff3a3a3c)      : juce::Colours::lightgrey;

    for (int i = 0; i < 3; ++i)
    {
        const bool active = (i == sel);
        msgTypeBtns[i].setColour (juce::TextButton::buttonColourId,
            active ? juce::Colour (0xff2979ff) : inactiveBg);
        msgTypeBtns[i].setColour (juce::TextButton::buttonOnColourId,
            juce::Colour (0xff2979ff));
        msgTypeBtns[i].setColour (juce::TextButton::textColourOffId,
            active ? juce::Colours::white : inactiveText);
    }

    updateCCVisibility();
}

void DrawnCurveEditor::updateCCVisibility()
{
    // CC# slider is only meaningful for CC messages; dim it for other types.
    const int  sel  = static_cast<int> (
        proc.apvts.getRawParameterValue ("messageType")->load());
    const bool isCC = (sel == 0);
    ccSlider.setEnabled (isCC);
    ccLabel .setEnabled (isCC);
    ccSlider.setAlpha   (isCC ? 1.0f : 0.4f);
    ccLabel .setAlpha   (isCC ? 1.0f : 0.4f);
}

void DrawnCurveEditor::updateDirButtons()
{
    const int sel = static_cast<int> (
        proc.apvts.getRawParameterValue ("playbackDirection")->load());

    const juce::Colour inactiveBg   = _lightMode ? juce::Colour (0xffe0e0e8) : juce::Colour (0xff333355);
    const juce::Colour inactiveText = _lightMode ? juce::Colour (0xff3a3a3c) : juce::Colours::lightgrey;

    for (int i = 0; i < 3; ++i)
    {
        const bool active = (i == sel);
        dirBtns[i].setColour (juce::TextButton::buttonColourId,
            active ? juce::Colour (0xff2979ff) : inactiveBg);
        dirBtns[i].setColour (juce::TextButton::buttonOnColourId,
            juce::Colour (0xff2979ff));
        dirBtns[i].setColour (juce::TextButton::textColourOffId,
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

    applyTheme();
}

//==============================================================================
void DrawnCurveEditor::applyTheme()
{
    // Resolve palette colours for the current mode.
    const bool light = _lightMode;

    const juce::Colour textCol  = light ? juce::Colour (0xff1c1c1e) : juce::Colours::white;
    const juce::Colour dimText  = light ? juce::Colour (0xff3a3a3c) : juce::Colours::lightgrey;
    const juce::Colour tbBg     = light ? juce::Colours::white       : juce::Colour (0xff252538);
    const juce::Colour tbLine   = light ? juce::Colour (0x28000000)  : juce::Colour (0x33ffffff);
    const juce::Colour accent   = light ? juce::Colour (0xff007aff)  : juce::Colour (0xff00e5ff);
    const juce::Colour btnBg    = light ? juce::Colour (0xffe0e0e8)  : juce::Colour (0xff333355);
    const juce::Colour btnText  = light ? juce::Colour (0xff1c1c1e)  : juce::Colours::white;

    // ── Sliders ───────────────────────────────────────────────────────────────
    for (auto* s : { &ccSlider, &channelSlider, &smoothingSlider,
                     &minOutSlider, &maxOutSlider, &speedSlider })
    {
        s->setColour (juce::Slider::textBoxTextColourId,       textCol);
        s->setColour (juce::Slider::textBoxBackgroundColourId, tbBg);
        s->setColour (juce::Slider::textBoxOutlineColourId,    tbLine);
        s->setColour (juce::Slider::thumbColourId,             accent);
        s->setColour (juce::Slider::trackColourId,             accent.withAlpha (0.45f));
        s->setColour (juce::Slider::backgroundColourId,        tbBg);
    }

    // ── Param labels ──────────────────────────────────────────────────────────
    for (auto* l : { &ccLabel, &channelLabel, &smoothingLabel,
                     &minOutLabel, &maxOutLabel, &speedLabel })
        l->setColour (juce::Label::textColourId, dimText);

    // ── Utility buttons ───────────────────────────────────────────────────────
    for (auto* b : { &playButton, &clearButton, &themeButton, &syncButton })
    {
        b->setColour (juce::TextButton::buttonColourId,  btnBg);
        b->setColour (juce::TextButton::textColourOffId, btnText);
    }

    // ── Message-type radio buttons (active button stays blue) ─────────────────
    updateMsgTypeButtons();

    // ── Direction radio buttons ────────────────────────────────────────────────
    updateDirButtons();

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

    // ── Button row 1 (Play · Clear · [CC][Ch Press][Pitch Bend] · [Sync] · [Light/Dark]) ──
    {
        auto row = area.removeFromTop (buttonRowH);

        // Right side first so removeFromRight doesn't shrink the left portion.
        themeButton.setBounds (row.removeFromRight (68));
        row.removeFromRight (pad);
        syncButton .setBounds (row.removeFromRight (62));
        row.removeFromRight (pad);

        playButton .setBounds (row.removeFromLeft (100));
        row.removeFromLeft (pad);
        clearButton.setBounds (row.removeFromLeft (80));

        // Message-type radio buttons in remaining space.
        row.removeFromLeft (pad * 3);
        static constexpr std::array<int, 3> kMsgW { 55, 100, 110 };
        for (int i = 0; i < 3; ++i)
        {
            msgTypeBtns[i].setBounds (row.removeFromLeft (kMsgW[i]));
            if (i < 2) row.removeFromLeft (pad);
        }
    }
    area.removeFromTop (pad);

    // ── Button row 2 (Direction: Fwd · Rev · Ping-Pong) ───────────────────────
    {
        auto row = area.removeFromTop (buttonRow2H);
        const int dirBtnW = (row.getWidth() - pad * 2) / 3;
        for (int i = 0; i < 3; ++i)
        {
            dirBtns[i].setBounds (row.removeFromLeft (dirBtnW));
            if (i < 2) row.removeFromLeft (pad);
        }
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

    // ── Param row 1: CC#, Channel, Smooth ─────────────────────────────────────
    placeRow3 (ccLabel, ccSlider, channelLabel, channelSlider, smoothingLabel, smoothingSlider);
    area.removeFromTop (pad);

    // ── Param row 2: Min Out, Max Out, Speed/Beats ────────────────────────────
    placeRow3 (minOutLabel, minOutSlider, maxOutLabel, maxOutSlider, speedLabel, speedSlider);
    area.removeFromTop (pad);

    // ── Curve display (fills all remaining vertical space) ────────────────────
    curveDisplay.setBounds (area);
}
