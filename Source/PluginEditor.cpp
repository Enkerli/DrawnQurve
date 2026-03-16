#include "PluginEditor.h"

//==============================================================================
// Colour palette
namespace Colours
{
    static const juce::Colour background  { 0xff12121f };
    static const juce::Colour gridLine    { 0x18ffffff };
    static const juce::Colour curve       { 0xff00e5ff };   // cyan
    static const juce::Colour capture     { 0xffff6b35 };   // orange (live draw)
    static const juce::Colour playhead    { 0xffffffff };
    static const juce::Colour playheadDot { 0xff00e5ff };
    static const juce::Colour hint        { 0x55ffffff };
    static const juce::Colour border      { 0x33ffffff };
    static const juce::Colour panelBg     { 0xff1c1c2e };
}

//==============================================================================
// CurveDisplay
//
// Margins reserved for axis labels (pixels inside the component bounds):
static constexpr float kAxisMarginL = 27.0f;   // left  — Y-axis MIDI-value labels
static constexpr float kAxisMarginB = 14.0f;   // bottom — X-axis time labels

CurveDisplay::CurveDisplay (DrawnCurveProcessor& p)
    : proc (p)
{
    startTimerHz (30);   // repaint at 30 fps (axis labels update with params too)
}

CurveDisplay::~CurveDisplay() { stopTimer(); }

void CurveDisplay::resized() {}

void CurveDisplay::paint (juce::Graphics& g)
{
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
    g.fillAll (Colours::background);

    // ── Subtle grid (confined to the plot area) ────────────────────────────────
    g.setColour (Colours::gridLine);
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
        g.setColour (Colours::curve);
        g.strokePath (curvePath, juce::PathStrokeType (2.5f,
                                                       juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));
    }

    // ── Live capture trail (clipped to plot area) ─────────────────────────────
    if (isCapturing && !capturePath.isEmpty())
    {
        g.saveState();
        g.reduceClipRegion (plot.toNearestInt());
        g.setColour (Colours::capture);
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

        g.setColour (Colours::playhead.withAlpha (0.75f));
        g.drawVerticalLine (juce::roundToInt (headX), plotY, plotY + plotH);

        g.setColour (Colours::playheadDot);
        g.fillEllipse (headX - 5.0f, headY - 5.0f, 10.0f, 10.0f);
    }

    // ── "Draw a curve" hint ────────────────────────────────────────────────────
    if (!proc.hasCurve() && !isCapturing)
    {
        g.setColour (Colours::hint);
        g.setFont (juce::Font (16.0f));
        g.drawText ("Draw a curve here", plot, juce::Justification::centred, false);
    }

    // ── Axis labels ────────────────────────────────────────────────────────────
    {
        const auto msgType = static_cast<MessageType> (
            static_cast<int> (proc.apvts.getRawParameterValue ("messageType")->load()));
        const float minOut = proc.apvts.getRawParameterValue ("minOutput")->load();
        const float maxOut = proc.apvts.getRawParameterValue ("maxOutput")->load();

        // Convert a normalised curve output (0=bottom, 1=top) to a display string.
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
        g.setColour (Colours::hint);

        // Y axis — three ticks matching the 25 / 50 / 75 % grid lines
        const int lblW  = juce::roundToInt (kAxisMarginL) - 2;
        const int lblH  = 12;
        g.drawText (yLabel (1.0f), 0, 1,
                    lblW, lblH, juce::Justification::centredRight, false);
        g.drawText (yLabel (0.5f), 0, juce::roundToInt (plotH * 0.5f - 6),
                    lblW, lblH, juce::Justification::centredRight, false);
        g.drawText (yLabel (0.0f), 0, juce::roundToInt (plotH - 13),
                    lblW, lblH, juce::Justification::centredRight, false);

        // X axis — left edge "0" and right edge "X.Xs"
        const float dur = proc.curveDuration();
        if (dur > 0.0f)
        {
            const int xLblY = juce::roundToInt (h - kAxisMarginB + 1);
            const int xLblH = juce::roundToInt (kAxisMarginB - 1);
            g.drawText ("0",
                        juce::roundToInt (plotX), xLblY,
                        32, xLblH,
                        juce::Justification::centredLeft, false);
            g.drawText (juce::String (dur, 2) + "s",
                        juce::roundToInt (plotX + plotW - 40), xLblY,
                        40, xLblH,
                        juce::Justification::centredRight, false);
        }
    }

    // ── Border (full component) ────────────────────────────────────────────────
    g.setColour (Colours::border);
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
    static constexpr int editorW       = 640;
    static constexpr int editorH       = 420;
    static constexpr int curveH        = 240;
    static constexpr int pad           = 6;
    static constexpr int buttonRowH    = 44;
    static constexpr int paramLabelH   = 16;
    static constexpr int paramSliderH  = 36;
    static constexpr int paramRowH     = paramLabelH + paramSliderH;  // 52
}

DrawnCurveEditor::DrawnCurveEditor (DrawnCurveProcessor& p)
    : AudioProcessorEditor (&p),
      proc (p),
      curveDisplay (p)
{
    setSize (Layout::editorW, Layout::editorH);

    // ── Curve display ─────────────────────────────────────────────────────────
    addAndMakeVisible (curveDisplay);

    // ── Buttons ───────────────────────────────────────────────────────────────
    addAndMakeVisible (playButton);
    playButton.onClick = [this]
    {
        const bool nowPlaying = !proc.isPlaying();
        proc.setPlaying (nowPlaying);
        playButton.setButtonText (nowPlaying ? "Stop" : "Play");
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

    // ── Sliders ───────────────────────────────────────────────────────────────
    setupSlider (ccSlider,        ccLabel,        "CC#");
    setupSlider (channelSlider,   channelLabel,   "Channel");
    setupSlider (smoothingSlider, smoothingLabel, "Smooth");
    setupSlider (minOutSlider,    minOutLabel,    "Min Out");
    setupSlider (maxOutSlider,    maxOutLabel,    "Max Out");

    // ── APVTS attachments ─────────────────────────────────────────────────────
    auto& apvts = proc.apvts;
    ccAttach        = std::make_unique<Attach> (apvts, "ccNumber",    ccSlider);
    channelAttach   = std::make_unique<Attach> (apvts, "midiChannel", channelSlider);
    smoothingAttach = std::make_unique<Attach> (apvts, "smoothing",   smoothingSlider);
    minAttach       = std::make_unique<Attach> (apvts, "minOutput",   minOutSlider);
    maxAttach       = std::make_unique<Attach> (apvts, "maxOutput",   maxOutSlider);

    // ── Message-type radio buttons ────────────────────────────────────────────
    // ComboBox popups fail silently in AUv3 on iOS (no TopLevelWindow), so we
    // use three TextButtons as a popup-free radio group instead.
    static const std::array<const char*, 3> kLabels { "CC", "Ch Press", "Pitch Bend" };
    for (int i = 0; i < 3; ++i)
    {
        msgTypeBtns[i].setButtonText (kLabels[i]);
        addAndMakeVisible (msgTypeBtns[i]);
        msgTypeBtns[i].onClick = [this, i]
        {
            if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (
                              proc.apvts.getParameter ("messageType")))
                *p = i;   // operator=(int) sets index and notifies host
        };
    }

    // Stay in sync with external parameter changes (automation, state restore).
    proc.apvts.addParameterListener ("messageType", this);
    updateMsgTypeButtons();   // reflect current (possibly restored) value
}

DrawnCurveEditor::~DrawnCurveEditor()
{
    proc.apvts.removeParameterListener ("messageType", this);
}

void DrawnCurveEditor::setupSlider (juce::Slider&       s,
                                     juce::Label&         l,
                                     const juce::String&  labelText,
                                     juce::Slider::SliderStyle style)
{
    s.setSliderStyle (style);
    s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 48, 20);
    addAndMakeVisible (s);

    l.setText (labelText, juce::dontSendNotification);
    l.setFont (juce::Font (12.0f));
    l.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible (l);
}

void DrawnCurveEditor::parameterChanged (const juce::String& paramID, float)
{
    if (paramID == "messageType")
        juce::MessageManager::callAsync ([this] { updateMsgTypeButtons(); });
}

void DrawnCurveEditor::updateMsgTypeButtons()
{
    const int sel = static_cast<int> (
        proc.apvts.getRawParameterValue ("messageType")->load());

    for (int i = 0; i < 3; ++i)
    {
        const bool active = (i == sel);
        msgTypeBtns[i].setColour (juce::TextButton::buttonColourId,
            active ? juce::Colour (0xff2979ff) : juce::Colour (0xff333355));
        msgTypeBtns[i].setColour (juce::TextButton::buttonOnColourId,
            juce::Colour (0xff2979ff));
        msgTypeBtns[i].setColour (juce::TextButton::textColourOffId,
            active ? juce::Colours::white : juce::Colours::lightgrey);
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

//==============================================================================
void DrawnCurveEditor::paint (juce::Graphics& g)
{
    g.fillAll (Colours::panelBg);
}

void DrawnCurveEditor::resized()
{
    using namespace Layout;
    auto area = getLocalBounds().reduced (pad);

    // ── Curve display ─────────────────────────────────────────────────────────
    curveDisplay.setBounds (area.removeFromTop (curveH));
    area.removeFromTop (pad);

    // ── Button row (Play · Clear ·········· [CC] [Ch Press] [Pitch Bend]) ─────
    {
        auto row = area.removeFromTop (buttonRowH);
        playButton .setBounds (row.removeFromLeft (160));
        row.removeFromLeft (pad);
        clearButton.setBounds (row.removeFromLeft (120));

        // Message-type radio buttons on the right — no popup needed.
        row.removeFromLeft (pad * 4);
        static constexpr std::array<int, 3> kBtnW { 55, 100, 110 };
        for (int i = 0; i < 3; ++i)
        {
            msgTypeBtns[i].setBounds (row.removeFromLeft (kBtnW[i]));
            if (i < 2) row.removeFromLeft (pad);
        }
    }
    area.removeFromTop (pad);

    // ── Param row 1: CC#, Channel, Smooth ─────────────────────────────────────
    {
        auto row = area.removeFromTop (paramRowH);
        const int slotW = (area.getWidth() + pad * 2) / 3;  // three equal slots

        auto placeParam = [&] (juce::Label& lbl, juce::Slider& sl)
        {
            auto slot = row.removeFromLeft (slotW - pad);
            row.removeFromLeft (pad);
            lbl.setBounds (slot.removeFromTop (paramLabelH));
            sl .setBounds (slot);
        };

        placeParam (ccLabel,      ccSlider);
        placeParam (channelLabel, channelSlider);
        // Remaining width goes to smoothing
        smoothingLabel.setBounds (row.removeFromTop (paramLabelH));
        smoothingSlider.setBounds (row);
    }
    area.removeFromTop (pad);

    // ── Param row 2: Min Out, Max Out ─────────────────────────────────────────
    {
        auto row  = area.removeFromTop (paramRowH);
        int  half = (area.getWidth() + pad) / 2;

        auto minSlot = row.removeFromLeft (half - pad);
        row.removeFromLeft (pad);

        minOutLabel .setBounds (minSlot.removeFromTop (paramLabelH));
        minOutSlider.setBounds (minSlot);

        maxOutLabel .setBounds (row.removeFromTop (paramLabelH));
        maxOutSlider.setBounds (row);
    }
}
