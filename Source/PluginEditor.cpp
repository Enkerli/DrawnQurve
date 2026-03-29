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
        { "Target",      "Tap the type button (CC / AT / PB / \xe2\x99\xa9) in the eyebrow to open per-lane routing. Choose msg type, CC#, channel, velocity." },
        { "Solo",        "Tap Solo on a lane to isolate its output. Other lanes mute so a synth can MIDI-Learn. On CC lanes, the next incoming CC message sets that lane\xe2\x80\x99s CC number." },
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
// RoutingOverlay
//==============================================================================

// ---------------------------------------------------------------------------
// Helpers shared across show* functions.
// ---------------------------------------------------------------------------
static juce::String symForMsgType (int t)
{
    switch (t)
    {
        case 0: return "CC";
        case 1: return "AT";
        case 2: return "PB";
        case 3: return juce::String::fromUTF8 ("\xe2\x99\xa9");  // ♩
        default: return "?";
    }
}

static juce::String detailForLane (DrawnCurveProcessor& proc, int lane)
{
    const int type   = static_cast<int> (proc.apvts.getRawParameterValue (laneParam (lane, "msgType"))->load());
    const int ccNum  = juce::roundToInt (proc.apvts.getRawParameterValue (laneParam (lane, ParamID::ccNumber))->load());
    const int vel    = juce::roundToInt (proc.apvts.getRawParameterValue (laneParam (lane, ParamID::noteVelocity))->load());
    switch (type)
    {
        case 0: return "CC " + juce::String (ccNum);
        case 3: return "Vel " + juce::String (vel);
        default: return {};
    }
}

// ---------------------------------------------------------------------------
RoutingOverlay::RoutingOverlay (DrawnCurveEditor& owner)
    : _owner (owner)
{
    setInterceptsMouseClicks (true, true);
    setVisible (false);

    // ── Button labels (all fromUTF8 / ASCII — no raw high bytes) ─────────────
    ccBtn  .setButtonText ("CC");
    atBtn  .setButtonText ("AT");
    pbBtn  .setButtonText ("PB");
    noteBtn.setButtonText (juce::String::fromUTF8 ("\xe2\x99\xa9"));  // ♩
    teachBtn  .setButtonText ("Solo");
    muteBtn   .setButtonText ("Mute");
    oneShotBtn.setButtonText (juce::String::fromUTF8 ("\xe2\x86\xa6")); // ↦

    // Single-lane panel components
    for (auto* btn : { &ccBtn, &atBtn, &pbBtn, &noteBtn,
                       &teachBtn, &muteBtn, &oneShotBtn, &legatoBtn })
        addChildComponent (btn);   // hidden by default; shown in showSingleLaneMode()

    detailLabel.setJustificationType (juce::Justification::centred);
    channelLabel.setJustificationType (juce::Justification::centred);
    phaseLabel.setText ("Phase", juce::dontSendNotification);
    addChildComponent (detailLabel);
    addChildComponent (channelLabel);
    addChildComponent (phaseSlider);
    addChildComponent (phaseLabel);

    // Msg type buttons — set param then refresh
    ccBtn  .onClick = [this] { cycleToMsgType (MessageType::CC);             };
    atBtn  .onClick = [this] { cycleToMsgType (MessageType::ChannelPressure);};
    pbBtn  .onClick = [this] { cycleToMsgType (MessageType::PitchBend);      };
    noteBtn.onClick = [this] { cycleToMsgType (MessageType::Note);           };

    teachBtn.setClickingTogglesState (true);
    teachBtn.onClick = [this]
    {
        if (teachBtn.getToggleState())
        {
            _owner.proc.cancelTeach();
            _owner.proc.beginTeach (_lane);   // isolates lane output (solo)
        }
        else
        {
            _owner.proc.cancelTeach();
        }
        applyTheme();
    };

    muteBtn.setClickingTogglesState (true);
    muteBtn.onClick = [this] { _owner.proc.setLanePaused (_lane, muteBtn.getToggleState()); };

    // ↦/↺ toggle: ON = one-shot mode, OFF = loop mode.
    // Going to one-shot also restarts the playhead (trigger feel).
    oneShotBtn.setButtonText (juce::String::fromUTF8 ("\xe2\x86\xa6"));  // ↦
    oneShotBtn.setClickingTogglesState (true);
    oneShotBtn.onClick = [this]
    {
        if (_lane < 0 || _lane >= kMaxLanes) return;
        const bool oneShot = oneShotBtn.getToggleState();
        if (auto* p = _owner.proc.apvts.getParameter (laneParam (_lane, ParamID::loopMode)))
            p->setValueNotifyingHost (p->convertTo0to1 (oneShot ? 1.0f : 0.0f));
        _owner.proc.updateLaneSnapshot (_lane);
        if (oneShot) _owner.proc.restartAllLanes();   // trigger feel when entering one-shot
    };

    // Legato toggle: tie note-ons before note-offs for gliding synths (Note mode only).
    legatoBtn.setButtonText (juce::String::fromUTF8 ("\xe2\x8c\xa3"));   // ⌣ (smile arc, U+2323)
    legatoBtn.setClickingTogglesState (true);
    legatoBtn.onClick = [this]
    {
        if (_lane < 0 || _lane >= kMaxLanes) return;
        const bool on = legatoBtn.getToggleState();
        if (auto* p = _owner.proc.apvts.getParameter (laneParam (_lane, ParamID::legatoMode)))
            p->setValueNotifyingHost (on ? 1.0f : 0.0f);
        _owner.proc.updateLaneSnapshot (_lane);
    };

    phaseSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    phaseSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);

    // ── Editable detail label (CC# / velocity) ────────────────────────────────
    detailLabel.setEditable (true, true, false);
    detailLabel.onTextChange = [this]
    {
        if (_lane < 0 || _lane >= kMaxLanes) return;
        const int type = static_cast<int> (
            _owner.proc.apvts.getRawParameterValue (laneParam (_lane, "msgType"))->load());
        const int val = juce::jlimit (0, 127, detailLabel.getText().retainCharacters ("0123456789").getIntValue());
        if (type == 3)   // Note — velocity
        {
            if (auto* p = dynamic_cast<juce::AudioParameterInt*> (
                    _owner.proc.apvts.getParameter (laneParam (_lane, ParamID::noteVelocity))))
                *p = juce::jlimit (1, 127, val);
        }
        else             // CC — cc number
        {
            if (auto* p = dynamic_cast<juce::AudioParameterInt*> (
                    _owner.proc.apvts.getParameter (laneParam (_lane, ParamID::ccNumber))))
                *p = val;
        }
        _owner.proc.updateLaneSnapshot (_lane);
        _owner.updateLaneRow (_lane);
        updateDetailLabels();
    };

    // ── Editable channel label ────────────────────────────────────────────────
    channelLabel.setEditable (true, true, false);
    channelLabel.onTextChange = [this]
    {
        if (_lane < 0 || _lane >= kMaxLanes) return;
        const int val = juce::jlimit (1, 16,
            channelLabel.getText().retainCharacters ("0123456789").getIntValue());
        if (auto* p = dynamic_cast<juce::AudioParameterInt*> (
                _owner.proc.apvts.getParameter (laneParam (_lane, ParamID::midiChannel))))
            *p = val;   // param range is 1-16; snapshot baking subtracts 1 for 0-indexed MIDI
        _owner.proc.updateLaneSnapshot (_lane);
        _owner.updateLaneRow (_lane);
        updateDetailLabels();
    };

    // ── All-lanes mini matrix components ─────────────────────────────────────
    for (int L = 0; L < kMaxLanes; ++L)
    {
        rowTypeBtn [static_cast<size_t>(L)].setButtonText (symForMsgType (0));

        rowDetailLbl[static_cast<size_t>(L)].setJustificationType (juce::Justification::centred);
        rowDetailLbl[static_cast<size_t>(L)].setEditable (true, true, false);
        rowDetailLbl[static_cast<size_t>(L)].onTextChange = [this, L]
        {
            if (L >= _owner.proc.activeLaneCount) return;
            const int type = static_cast<int> (
                _owner.proc.apvts.getRawParameterValue (laneParam (L, "msgType"))->load());
            const int val = juce::jlimit (0, 127,
                rowDetailLbl[static_cast<size_t>(L)].getText()
                    .retainCharacters ("0123456789").getIntValue());
            if (type == 3)
            {
                if (auto* p = dynamic_cast<juce::AudioParameterInt*> (
                        _owner.proc.apvts.getParameter (laneParam (L, ParamID::noteVelocity))))
                    *p = juce::jlimit (1, 127, val);
            }
            else
            {
                if (auto* p = dynamic_cast<juce::AudioParameterInt*> (
                        _owner.proc.apvts.getParameter (laneParam (L, ParamID::ccNumber))))
                    *p = val;
            }
            _owner.proc.updateLaneSnapshot (L);
            _owner.updateLaneRow (L);
            rowDetailLbl[static_cast<size_t>(L)].setText (
                detailForLane (_owner.proc, L), juce::dontSendNotification);
        };

        rowChanLbl[static_cast<size_t>(L)].setJustificationType (juce::Justification::centred);
        rowChanLbl[static_cast<size_t>(L)].setEditable (true, true, false);
        rowChanLbl[static_cast<size_t>(L)].onTextChange = [this, L]
        {
            if (L >= _owner.proc.activeLaneCount) return;
            const int val = juce::jlimit (1, 16,
                rowChanLbl[static_cast<size_t>(L)].getText()
                    .retainCharacters ("0123456789").getIntValue());
            if (auto* p = dynamic_cast<juce::AudioParameterInt*> (
                    _owner.proc.apvts.getParameter (laneParam (L, ParamID::midiChannel))))
                *p = val;
            _owner.proc.updateLaneSnapshot (L);
            _owner.updateLaneRow (L);
            rowChanLbl[static_cast<size_t>(L)].setText (
                "Ch " + juce::String (val), juce::dontSendNotification);
        };

        addChildComponent (rowTypeBtn [static_cast<size_t>(L)]);
        addChildComponent (rowDetailLbl[static_cast<size_t>(L)]);
        addChildComponent (rowChanLbl  [static_cast<size_t>(L)]);

        rowTypeBtn[static_cast<size_t>(L)].onClick = [this, L]
        {
            // Cycle msg type for this row's lane
            static const int kNext[4] = { 2, 0, 3, 1 };
            const int cur = static_cast<int> (
                _owner.proc.apvts.getRawParameterValue (laneParam (L, "msgType"))->load());
            const int next = kNext[std::clamp (cur, 0, 3)];
            if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (
                              _owner.proc.apvts.getParameter (laneParam (L, "msgType"))))
                *p = next;
            _owner.proc.updateLaneSnapshot (L);
            _owner.updateLaneRow (L);
            _owner.updateScaleVisibility();
            // Refresh button text and detail
            rowTypeBtn[static_cast<size_t>(L)].setButtonText (symForMsgType (next));
            rowDetailLbl[static_cast<size_t>(L)].setText (
                detailForLane (_owner.proc, L), juce::dontSendNotification);
        };
    }
}

RoutingOverlay::~RoutingOverlay() = default;

// ---------------------------------------------------------------------------
void RoutingOverlay::showSingleLaneMode (bool visible)
{
    for (auto* c : { static_cast<juce::Component*> (&ccBtn),
                     static_cast<juce::Component*> (&atBtn),
                     static_cast<juce::Component*> (&pbBtn),
                     static_cast<juce::Component*> (&noteBtn),
                     static_cast<juce::Component*> (&teachBtn),
                     static_cast<juce::Component*> (&muteBtn),
                     static_cast<juce::Component*> (&oneShotBtn),
                     static_cast<juce::Component*> (&legatoBtn),
                     static_cast<juce::Component*> (&detailLabel),
                     static_cast<juce::Component*> (&channelLabel),
                     static_cast<juce::Component*> (&phaseSlider),
                     static_cast<juce::Component*> (&phaseLabel) })
        c->setVisible (visible);
}

void RoutingOverlay::showAllLanesMode (bool visible)
{
    for (int L = 0; L < kMaxLanes; ++L)
    {
        rowTypeBtn [static_cast<size_t>(L)].setVisible (visible && L < _owner.proc.activeLaneCount);
        rowDetailLbl[static_cast<size_t>(L)].setVisible (visible && L < _owner.proc.activeLaneCount);
        rowChanLbl  [static_cast<size_t>(L)].setVisible (visible && L < _owner.proc.activeLaneCount);
    }
}

// ---------------------------------------------------------------------------
static void positionPanel (int& px, int& py, int panelW, int panelH,
                           juce::Rectangle<int> anchor, int editorW, int editorH)
{
    px = anchor.getX();
    py = anchor.getBottom() + 4;
    px = juce::jlimit (4, editorW - panelW - 4, px);
    if (py + panelH > editorH - 4)
        py = anchor.getY() - panelH - 4;
}

void RoutingOverlay::showForLane (int lane, juce::Rectangle<int> anchorInEditor)
{
    _lane = lane;
    showAllLanesMode (false);
    showSingleLaneMode (true);

    // Phase attachment
    _phaseAttach.reset();
    _phaseAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        _owner.proc.apvts, laneParam (lane, ParamID::phaseOffset), phaseSlider);

    muteBtn  .setToggleState (_owner.proc.getLanePaused  (lane), juce::dontSendNotification);
    teachBtn .setToggleState (_owner.proc.isTeachPending (lane), juce::dontSendNotification);
    // oneShotBtn and legatoBtn states synced inside applyTheme()

    updateMsgTypeHighlight();
    updateDetailLabels();
    applyTheme();

    constexpr int kPanelW = 244, kPanelH = 160;
    int px, py;
    positionPanel (px, py, kPanelW, kPanelH, anchorInEditor,
                   _owner.getWidth(), _owner.getHeight());
    _panelRect = { px, py, kPanelW, kPanelH };

    setBounds (_owner.getLocalBounds());
    resized();
    setVisible (true);
    toFront (false);
}

void RoutingOverlay::showForAllLanes (juce::Rectangle<int> anchorInEditor)
{
    _lane = -1;
    showSingleLaneMode (false);
    _phaseAttach.reset();

    // Refresh row buttons for each active lane
    for (int L = 0; L < _owner.proc.activeLaneCount; ++L)
    {
        const int type = static_cast<int> (
            _owner.proc.apvts.getRawParameterValue (laneParam (L, "msgType"))->load());
        const int chan = juce::roundToInt (
            _owner.proc.apvts.getRawParameterValue (laneParam (L, ParamID::midiChannel))->load());
        rowTypeBtn  [static_cast<size_t>(L)].setButtonText (symForMsgType (type));
        rowDetailLbl[static_cast<size_t>(L)].setText (detailForLane (_owner.proc, L), juce::dontSendNotification);
        rowChanLbl  [static_cast<size_t>(L)].setText ("Ch " + juce::String (chan), juce::dontSendNotification);
    }
    showAllLanesMode (true);
    applyTheme();

    // Panel height: title (24) + N rows × 30 + padding
    const int kPanelW = 244;
    const int kPanelH = 24 + _owner.proc.activeLaneCount * 30 + 16;
    int px, py;
    positionPanel (px, py, kPanelW, kPanelH, anchorInEditor,
                   _owner.getWidth(), _owner.getHeight());
    _panelRect = { px, py, kPanelW, kPanelH };

    setBounds (_owner.getLocalBounds());
    resized();
    setVisible (true);
    toFront (false);
}

void RoutingOverlay::dismiss()
{
    _phaseAttach.reset();
    setVisible (false);
}

void RoutingOverlay::reanchor (juce::Rectangle<int> anchorInEditor)
{
    // Recalculate panel position without touching _phaseAttach or re-initialising
    // any child components.  Safe to call from resized() whether in single or all-lanes mode.
    if (! isVisible()) return;

    int panelW, panelH;
    if (_lane >= 0 && _lane < kMaxLanes)
    {
        panelW = 244;
        panelH = 160;
    }
    else
    {
        panelW = 244;
        panelH = 24 + _owner.proc.activeLaneCount * 30 + 16;
    }

    int px, py;
    positionPanel (px, py, panelW, panelH, anchorInEditor,
                   _owner.getWidth(), _owner.getHeight());
    _panelRect = { px, py, panelW, panelH };

    setBounds (_owner.getLocalBounds());
    resized();
}

void RoutingOverlay::setLightMode (bool light)
{
    _lightMode = light;
    applyTheme();
}

void RoutingOverlay::applyTheme()
{
    // No isVisible() guard — theme must be applied before setVisible(true) so the
    // overlay doesn't flash black on first show.

    const auto btnBg   = _lightMode ? juce::Colour (0xffe8e4de) : juce::Colour (0xff2a2a3a);
    const auto btnText = _lightMode ? juce::Colour (0xff2a2620) : juce::Colour (0xffcacad6);
    const auto laneCol = (_lane >= 0 && _lane < kMaxLanes)
        ? (_lightMode ? kLaneColourLight[_lane] : kLaneColourDark[_lane])
        : btnText;

    // In dark mode the selected state needs a much stronger background so the active
    // button is immediately legible; light mode can stay subtler.
    const auto onBg  = _lightMode ? laneCol.withAlpha (0.22f) : laneCol.withAlpha (0.50f);
    const auto onTxt = _lightMode ? laneCol : juce::Colours::white;

    for (auto* btn : { &ccBtn, &atBtn, &pbBtn, &noteBtn,
                       &teachBtn, &muteBtn, &oneShotBtn, &legatoBtn })
    {
        btn->setColour (juce::TextButton::buttonColourId,   btnBg);
        btn->setColour (juce::TextButton::buttonOnColourId, onBg);
        btn->setColour (juce::TextButton::textColourOffId,  btnText);
        btn->setColour (juce::TextButton::textColourOnId,   onTxt);
    }
    phaseLabel.setColour (juce::Label::textColourId, btnText);

    // Sync toggle states from APVTS so they reflect persisted values.
    if (_lane >= 0 && _lane < kMaxLanes)
    {
        const bool oneShot = _owner.proc.apvts.getRawParameterValue (
            laneParam (_lane, ParamID::loopMode))->load() > 0.5f;
        const bool legato  = _owner.proc.apvts.getRawParameterValue (
            laneParam (_lane, ParamID::legatoMode))->load() > 0.5f;
        oneShotBtn.setToggleState (oneShot, juce::dontSendNotification);
        legatoBtn .setToggleState (legato,  juce::dontSendNotification);
    }

    // ── Editable value labels — styled like small buttons so they look tappable ──
    // Background + outline distinguishes them from plain read-only text.
    // backgroundWhenEditingColourId + explicit text colour prevent the blank-while-editing bug.
    const auto editBg     = _lightMode ? juce::Colour (0xfff0ede8) : juce::Colour (0xff32323e);
    const auto editBorder = _lightMode ? juce::Colour (0xffb0aaa0) : juce::Colour (0xff55556a);
    const auto editingBg  = _lightMode ? juce::Colours::white : juce::Colour (0xff1e1e2c);

    for (auto* lbl : { &detailLabel, &channelLabel })
    {
        lbl->setColour (juce::Label::backgroundColourId,            editBg);
        lbl->setColour (juce::Label::outlineColourId,               editBorder);
        lbl->setColour (juce::Label::textColourId,                  btnText);
        lbl->setColour (juce::Label::backgroundWhenEditingColourId, editingBg);
        lbl->setColour (juce::Label::textWhenEditingColourId,       btnText);
    }

    // All-lanes rows
    for (int L = 0; L < kMaxLanes; ++L)
    {
        const auto rc = (_lightMode ? kLaneColourLight : kLaneColourDark)[L];
        rowTypeBtn[static_cast<size_t>(L)].setColour (juce::TextButton::buttonColourId,   rc.withAlpha (0.14f));
        rowTypeBtn[static_cast<size_t>(L)].setColour (juce::TextButton::buttonOnColourId, rc.withAlpha (0.30f));
        rowTypeBtn[static_cast<size_t>(L)].setColour (juce::TextButton::textColourOffId,  rc);

        for (auto* lbl : { &rowDetailLbl[static_cast<size_t>(L)],
                           &rowChanLbl  [static_cast<size_t>(L)] })
        {
            lbl->setColour (juce::Label::backgroundColourId,            editBg);
            lbl->setColour (juce::Label::outlineColourId,               editBorder);
            lbl->setColour (juce::Label::textColourId,                  btnText);
            lbl->setColour (juce::Label::backgroundWhenEditingColourId, editingBg);
            lbl->setColour (juce::Label::textWhenEditingColourId,       btnText);
        }
    }

    repaint();
}

void RoutingOverlay::updateMsgTypeHighlight()
{
    if (_lane < 0 || _lane >= kMaxLanes) return;
    const auto curType = static_cast<MessageType> (
        static_cast<int> (_owner.proc.apvts.getRawParameterValue (
            laneParam (_lane, ParamID::msgType))->load()));

    ccBtn  .setToggleState (curType == MessageType::CC,             juce::dontSendNotification);
    atBtn  .setToggleState (curType == MessageType::ChannelPressure,juce::dontSendNotification);
    pbBtn  .setToggleState (curType == MessageType::PitchBend,      juce::dontSendNotification);
    noteBtn.setToggleState (curType == MessageType::Note,           juce::dontSendNotification);
}

void RoutingOverlay::updateDetailLabels()
{
    if (_lane < 0 || _lane >= kMaxLanes) return;
    const auto curType = static_cast<MessageType> (
        static_cast<int> (_owner.proc.apvts.getRawParameterValue (
            laneParam (_lane, ParamID::msgType))->load()));
    const int ccNum = juce::roundToInt (_owner.proc.apvts.getRawParameterValue (
                          laneParam (_lane, ParamID::ccNumber))->load());
    const int vel   = juce::roundToInt (_owner.proc.apvts.getRawParameterValue (
                          laneParam (_lane, ParamID::noteVelocity))->load());
    const int chan  = juce::roundToInt (_owner.proc.apvts.getRawParameterValue (
                          laneParam (_lane, ParamID::midiChannel))->load());

    juce::String det;
    switch (curType)
    {
        case MessageType::CC:             det = "CC " + juce::String (ccNum); break;
        case MessageType::ChannelPressure:det = "AT";                          break;
        case MessageType::PitchBend:      det = "PB";                          break;
        case MessageType::Note:           det = "Vel " + juce::String (vel);  break;
    }
    detailLabel .setText (det,                         juce::dontSendNotification);
    channelLabel.setText ("Ch " + juce::String (chan), juce::dontSendNotification);
}

void RoutingOverlay::cycleToMsgType (MessageType t)
{
    if (_lane < 0 || _lane >= kMaxLanes) return;
    if (auto* p = _owner.proc.apvts.getParameter (laneParam (_lane, ParamID::msgType)))
        p->setValueNotifyingHost (p->convertTo0to1 (static_cast<float> (static_cast<int> (t))));

    _owner.proc.updateLaneSnapshot (_lane);
    _owner.updateLaneRow (_lane);
    _owner.updateScaleVisibility();
    updateMsgTypeHighlight();
    updateDetailLabels();
    applyTheme();
}

void RoutingOverlay::paint (juce::Graphics& g)
{
    if (_panelRect.isEmpty()) return;

    g.setColour (juce::Colour (0x40000000));
    g.fillRect (getLocalBounds());

    const auto panelBg = _lightMode ? juce::Colour (0xfffaf8f5) : juce::Colour (0xff1e1e2a);
    const auto border  = _lightMode ? juce::Colour (0xffccc8c0) : juce::Colour (0xff3a3a4a);
    g.setColour (panelBg);
    g.fillRoundedRectangle (_panelRect.toFloat(), 10.0f);
    g.setColour (border);
    g.drawRoundedRectangle (_panelRect.toFloat().reduced (0.5f), 10.0f, 1.0f);

    const auto titleCol = _lightMode ? juce::Colour (0xff2a2620) : juce::Colour (0xffcacad6);
    g.setColour (titleCol);
    g.setFont (DrawnCurveLookAndFeel::makeFont (12.0f));

    if (_lane >= 0 && _lane < kMaxLanes)
    {
        // Single-lane: colour dot + "Lane N Routing"
        const auto lc = _lightMode ? kLaneColourLight[_lane] : kLaneColourDark[_lane];
        g.setColour (lc);
        g.fillEllipse (_panelRect.getX() + 10.0f, _panelRect.getY() + 8.0f, 8.0f, 8.0f);
        g.setColour (titleCol);
        g.drawText ("Lane " + juce::String (_lane + 1) + " Routing",
                    _panelRect.getX() + 24, _panelRect.getY() + 4,
                    _panelRect.getWidth() - 30, 16,
                    juce::Justification::left, false);
    }
    else
    {
        // All-lanes: colour dots per row (drawn over the row area)
        g.drawText ("All Lanes",
                    _panelRect.getX() + 12, _panelRect.getY() + 4,
                    _panelRect.getWidth() - 20, 16,
                    juce::Justification::left, false);

        for (int L = 0; L < _owner.proc.activeLaneCount; ++L)
        {
            const auto lc = _lightMode ? kLaneColourLight[L] : kLaneColourDark[L];
            const float dotY = static_cast<float> (_panelRect.getY() + 24 + L * 30 + 10);
            g.setColour (lc);
            g.fillEllipse (_panelRect.getX() + 8.0f, dotY, 8.0f, 8.0f);
        }
    }
}

void RoutingOverlay::resized()
{
    if (_panelRect.isEmpty()) return;

    if (_lane >= 0 && _lane < kMaxLanes)
    {
        // ── Single-lane panel ─────────────────────────────────────────────────
        auto p = _panelRect.reduced (10);
        p.removeFromTop (18);   // title

        {   // Msg type row
            auto row = p.removeFromTop (28);
            const int btnW = (row.getWidth() - 3 * 4) / 4;
            ccBtn  .setBounds (row.removeFromLeft (btnW)); row.removeFromLeft (4);
            atBtn  .setBounds (row.removeFromLeft (btnW)); row.removeFromLeft (4);
            pbBtn  .setBounds (row.removeFromLeft (btnW)); row.removeFromLeft (4);
            noteBtn.setBounds (row.removeFromLeft (btnW));
        }
        p.removeFromTop (4);

        {   // Detail + channel
            auto row = p.removeFromTop (18);
            detailLabel .setBounds (row.removeFromLeft (row.getWidth() / 2));
            channelLabel.setBounds (row);
        }
        p.removeFromTop (4);

        {   // Teach / Mute / One-shot / Legato
            auto row = p.removeFromTop (26);
            const int btnW = (row.getWidth() - 3 * 4) / 4;
            teachBtn  .setBounds (row.removeFromLeft (btnW)); row.removeFromLeft (4);
            muteBtn   .setBounds (row.removeFromLeft (btnW)); row.removeFromLeft (4);
            oneShotBtn.setBounds (row.removeFromLeft (btnW)); row.removeFromLeft (4);
            legatoBtn .setBounds (row.removeFromLeft (btnW));
        }
        p.removeFromTop (4);

        {   // Phase
            auto row = p.removeFromTop (20);
            phaseLabel .setBounds (row.removeFromLeft (44));
            phaseSlider.setBounds (row);
        }

        showAllLanesMode (false);
        showSingleLaneMode (true);
    }
    else
    {
        // ── All-lanes mini matrix ─────────────────────────────────────────────
        showSingleLaneMode (false);
        auto p = _panelRect.reduced (10);
        p.removeFromTop (18);   // title

        for (int L = 0; L < _owner.proc.activeLaneCount; ++L)
        {
            auto row = p.removeFromTop (26);
            p.removeFromTop (4);

            row.removeFromLeft (18);   // dot column
            rowTypeBtn  [static_cast<size_t>(L)].setBounds (row.removeFromLeft (44)); row.removeFromLeft (6);
            rowDetailLbl[static_cast<size_t>(L)].setBounds (row.removeFromLeft (70)); row.removeFromLeft (6);
            rowChanLbl  [static_cast<size_t>(L)].setBounds (row);
        }
        showAllLanesMode (true);
    }
}

void RoutingOverlay::mouseDown (const juce::MouseEvent& e)
{
    if (! _panelRect.contains (e.getPosition()))
        dismiss();
}

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

    // ── Quantize state for focused lane (read once, used by grid + staircase) ──
    const bool xQuant = proc.apvts.getRawParameterValue (
        laneParam (_focusedLane, "xQuantize"))->load() > 0.5f;
    const bool yQuant = proc.apvts.getRawParameterValue (
        laneParam (_focusedLane, "yQuantize"))->load() > 0.5f;
    const auto laneCol = (_lightMode ? kLaneColourLight : kLaneColourDark)[_focusedLane];

    // ── Grid — thin base lines (always) ──────────────────────────────────────
    g.setColour (T.gridLine);
    for (int i = 1; i < _xDivisions; ++i)
        g.drawVerticalLine (juce::roundToInt (plotX + plotW * (float)i / (float)_xDivisions),
                            plotY, plotY + plotH);
    for (int i = 1; i < _yDivisions; ++i)
        g.drawHorizontalLine (juce::roundToInt (plotY + plotH * (float)i / (float)_yDivisions),
                              plotX, plotX + plotW);

    // ── Quantize visual emphasis — thicker, tinted lines over the base grid ──
    if (xQuant)
    {
        g.setColour (laneCol.withAlpha (0.22f));
        for (int i = 1; i < _xDivisions; ++i)
        {
            const float x = plotX + plotW * (float)i / (float)_xDivisions;
            g.fillRect (x - 0.75f, plotY, 1.5f, plotH);
        }
    }
    if (yQuant)
    {
        g.setColour (laneCol.withAlpha (0.22f));
        for (int i = 1; i < _yDivisions; ++i)
        {
            const float y = plotY + plotH * (float)i / (float)_yDivisions;
            g.fillRect (plotX, y - 0.75f, plotW, 1.5f);
        }
    }

    // ── Lane curves ──────────────────────────────────────────────────────────
    // Stroke types per lane: solid / dashed / dot-dash.
    // Draw unfocused lanes first (at 40 % opacity), focused lane on top.
    static const float kDashLen[kMaxLanes][4] = {
        { 0, 0, 0, 0 },                  // lane 0: solid (ignored)
        { 10.0f, 5.0f, 0, 0 },           // lane 1: long dash
        { 2.0f, 4.0f, 10.0f, 4.0f },     // lane 2: dot-dash
        { 5.0f, 3.0f, 5.0f, 3.0f },      // lane 3: short double-dash
    };
    static const int kDashCount[kMaxLanes] = { 0, 2, 4, 4 };

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

    // ── Quantized preview overlay ─────────────────────────────────────────────
    // Shows a staircase preview for every lane that has X or Y quantize active.
    // Non-focused lanes draw at reduced opacity so focus hierarchy is preserved.
    // Stair lines are nearly as thick as the raw curve lines so they read clearly.
    //
    //  X-only  →  S&H staircase: value held constant per X-tick
    //  Y-only  →  256-sample path, each Y snapped to nearest grid level
    //  X + Y   →  Full staircase: X-tick hold + Y snap
    //
    // In Note mode, scale quantization is applied so the preview matches the engine.
    for (int stairLane = 0; stairLane < kMaxLanes; ++stairLane)
    {
        if (! proc.hasCurve (stairLane)) continue;

        const bool lxQuant = proc.apvts.getRawParameterValue (
            laneParam (stairLane, "xQuantize"))->load() > 0.5f;
        const bool lyQuant = proc.apvts.getRawParameterValue (
            laneParam (stairLane, "yQuantize"))->load() > 0.5f;
        if (! lxQuant && ! lyQuant) continue;

        const int lxDiv = static_cast<int> (proc.apvts.getRawParameterValue (
            laneParam (stairLane, ParamID::xDivisions))->load());
        const int lyDiv = static_cast<int> (proc.apvts.getRawParameterValue (
            laneParam (stairLane, ParamID::yDivisions))->load());
        const float lYstep = 1.0f / static_cast<float> (juce::jlimit (2, 24, lyDiv));

        const auto  table = proc.getCurveTable (stairLane);
        const auto  stairMsgType = static_cast<MessageType> (static_cast<int> (
            proc.apvts.getRawParameterValue (laneParam (stairLane, "msgType"))->load()));
        const float stairMinOut  = proc.apvts.getRawParameterValue (laneParam (stairLane, "minOutput"))->load();
        const float stairMaxOut  = proc.apvts.getRawParameterValue (laneParam (stairLane, "maxOutput"))->load();
        const bool  stairIsNote  = (stairMsgType == MessageType::Note);
        const ScaleConfig stairSc = stairIsNote ? proc.getScaleConfig (stairLane) : ScaleConfig{};

        auto toVisualY = [&] (float val) -> float
        {
            if (stairIsNote)
            {
                const float rawNoteF = (stairMinOut + val * (stairMaxOut - stairMinOut)) * 127.0f;
                const int   rawNote  = std::clamp (static_cast<int> (std::lround (rawNoteF)), 0, 127);
                const int   snapped  = GestureEngine::quantizeNote (rawNote, stairSc, true);
                const float snNorm   = (static_cast<float> (snapped) / 127.0f - stairMinOut)
                                       / std::max (stairMaxOut - stairMinOut, 0.001f);
                return plotY + (1.0f - juce::jlimit (0.0f, 1.0f, snNorm)) * plotH;
            }
            return plotY + (1.0f - val) * plotH;
        };

        juce::Path quantPath;
        const int safeXDiv = juce::jlimit (2, 32, lxDiv);

        if (lxQuant)
        {
            bool started = false;
            for (int i = 0; i < safeXDiv; ++i)
            {
                const float x1 = plotX + static_cast<float> (i)     / static_cast<float> (safeXDiv) * plotW;
                const float x2 = plotX + static_cast<float> (i + 1) / static_cast<float> (safeXDiv) * plotW;
                const int   tidx = juce::jlimit (0, 255,
                    juce::roundToInt (static_cast<float> (i) / static_cast<float> (safeXDiv) * 255.0f));
                const float raw = table[static_cast<size_t> (tidx)];
                const float val = lyQuant
                    ? juce::jlimit (0.0f, 1.0f, std::round (raw / lYstep) * lYstep)
                    : raw;
                const float cy = toVisualY (val);
                if (! started) { quantPath.startNewSubPath (x1, cy); started = true; }
                else             quantPath.lineTo (x1, cy);
                quantPath.lineTo (x2, cy);
            }
        }
        else
        {
            for (int i = 0; i < 256; ++i)
            {
                const float cx  = plotX + static_cast<float> (i) / 255.0f * plotW;
                const float raw = table[static_cast<size_t> (i)];
                const float val = juce::jlimit (0.0f, 1.0f, std::round (raw / lYstep) * lYstep);
                const float cy  = toVisualY (val);
                if (i == 0) quantPath.startNewSubPath (cx, cy);
                else        quantPath.lineTo (cx, cy);
            }
        }

        const bool isFocused = (stairLane == _focusedLane);
        const auto stairCol = (_lightMode ? kLaneColourLight : kLaneColourDark)[stairLane];
        // Match curve rendering: same alpha, stroke weight, and dash pattern per lane.
        const float stairAlpha  = isFocused ? 0.75f : 0.45f;
        const float stairStroke = isFocused ? 2.5f  : 1.8f;
        g.setColour (stairCol.withAlpha (stairAlpha));
        if (kDashCount[stairLane] == 0)
        {
            g.strokePath (quantPath, juce::PathStrokeType (stairStroke,
                juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        else
        {
            juce::Path dashed;
            juce::PathStrokeType stroke (stairStroke, juce::PathStrokeType::curved,
                                          juce::PathStrokeType::butt);
            stroke.createDashedStroke (dashed, quantPath,
                                        kDashLen[stairLane], kDashCount[stairLane]);
            g.fillPath (dashed);
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
                    // halfUp: distance from noteY UP to midpoint with the note above (smaller Y)
                    // halfDn: distance from noteY DOWN to midpoint with the note below (larger Y)
                    const float halfUp = (i == 0)
                        ? (noteY - plotY) * 0.5f
                        : (noteY - visible[i-1].y) * 0.5f;
                    const float halfDn = (i+1 < visible.size())
                        ? (visible[i+1].y - noteY) * 0.5f
                        : (plotY + plotH - noteY) * 0.5f;
                    const float bandH  = halfUp + halfDn;
                    if (bandH < 0.5f) continue;   // guard against degenerate zero-height rects
                    g.setColour ((i & 1) ? T.gridLine.withAlpha (0.08f)
                                         : T.gridLine.withAlpha (0.18f));
                    g.fillRect (plotX, noteY - halfUp, plotW, bandH);
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
        const auto pauseLaneCol = (_lightMode ? kLaneColourLight : kLaneColourDark)[_focusedLane];

        // Faint tint to darken the plot while paused
        g.setColour (pauseLaneCol.withAlpha (0.08f));
        g.fillRect (plot);

        // Blinking pause bars drawn as two filled rounded rectangles
        if (_blinkOn)
        {
            const float cx   = plot.getCentreX();
            const float cy   = plot.getCentreY();
            const float barH = 22.0f;
            const float barW =  7.0f;
            const float gap  =  5.0f;   // gap between the two bars

            g.setColour (pauseLaneCol.withAlpha (0.55f));
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

        if (! isNoteBadge || ! proc.hasCurve (_focusedLane))
            _badgeRect = {};

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
            _badgeRect = badge.toNearestInt();

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
    // If the tap lands within the scale badge and we're not already capturing, open scale overlay.
    if (onBadgeTap && ! isCapturing && ! _badgeRect.isEmpty()
        && _badgeRect.contains (e.getPosition()))
    {
        onBadgeTap();
        return;   // don't start a draw gesture
    }

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
    if (onCurveDrawn) onCurveDrawn();
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
      curveDisplay (p),
      _routingOverlay (*this)
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
    speedLabel.setEditable (true, true, false);
    speedLabel.onTextChange = [this]
    {
        const bool sync = proc.apvts.getRawParameterValue (ParamID::syncEnabled)->load() > 0.5f;
        // Strip suffix characters (×, ♩, x, spaces) to get bare number
        juce::String t = speedLabel.getText().trim();
        for (const juce::juce_wchar ch : { (juce::juce_wchar)0x00D7, (juce::juce_wchar)0x2669,
                                           (juce::juce_wchar)'x',    (juce::juce_wchar)'X' })
            t = t.trimCharactersAtEnd (juce::String::charToString (ch)).trim();
        const float v = t.getFloatValue();
        if (v <= 0.0f) return;
        if (sync)
        {
            const float beats = juce::jlimit (1.0f, 32.0f, v);
            if (auto* pSync = proc.apvts.getParameter (ParamID::syncBeats))
                pSync->setValueNotifyingHost (pSync->convertTo0to1 (beats));
        }
        else
        {
            const float spd = juce::jlimit (0.25f, 4.0f, v);
            if (auto* pSpeed = proc.apvts.getParameter (ParamID::playbackSpeed))
                pSpeed->setValueNotifyingHost (pSpeed->convertTo0to1 (spd));
        }
    };

    addAndMakeVisible (helpButton);
    helpButton.onClick = [this]
    {
        helpOverlay.setLightMode (_lightMode);
        helpOverlay.setVisible (! helpOverlay.isVisible());
        if (helpOverlay.isVisible()) helpOverlay.toFront (false);
    };

    // ── Standalone MIDI output ─────────────────────────────────────────────────
    // Create a virtual MIDI source port so other apps see "DrawnCurve" as a source.
    // Also show a button to optionally target a specific device.
    if (juce::JUCEApplicationBase::isStandaloneApp())
    {
        _virtualMidiPort = juce::MidiOutput::createNewDevice ("DrawnCurve");
        if (_virtualMidiPort != nullptr)
            proc.setVirtualMidiOutput (_virtualMidiPort.get());

        addAndMakeVisible (midiOutBtn);
        midiOutBtn.onClick = [this] { showMidiOutputPicker(); };
    }

    // ── Global speed slider (utility bar — always bound to global speed param) ─
    setupSlider (speedSlider, speedLabel, {});
    speedSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
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
    // Grid density — write to APVTS so the value persists across editor open/close.
    tickYMinusBtn.onClick = [this]
    {
        const int n = juce::jlimit (2, 24, curveDisplay.getYDivisions() - 1);
        curveDisplay.setYDivisions (n);
        refreshTickLabels();
        if (auto* param = proc.apvts.getParameter (laneParam (_focusedLane, ParamID::yDivisions)))
            param->setValueNotifyingHost (param->convertTo0to1 (static_cast<float> (n)));
        proc.updateLaneSnapshot (_focusedLane);
    };
    tickYPlusBtn.onClick = [this]
    {
        const int n = juce::jlimit (2, 24, curveDisplay.getYDivisions() + 1);
        curveDisplay.setYDivisions (n);
        refreshTickLabels();
        if (auto* param = proc.apvts.getParameter (laneParam (_focusedLane, ParamID::yDivisions)))
            param->setValueNotifyingHost (param->convertTo0to1 (static_cast<float> (n)));
        proc.updateLaneSnapshot (_focusedLane);
    };
    tickXMinusBtn.onClick = [this]
    {
        const int n = juce::jlimit (2, 32, curveDisplay.getXDivisions() - 1);
        curveDisplay.setXDivisions (n);
        refreshTickLabels();
        if (auto* param = proc.apvts.getParameter (laneParam (_focusedLane, ParamID::xDivisions)))
            param->setValueNotifyingHost (param->convertTo0to1 (static_cast<float> (n)));
        proc.updateLaneSnapshot (_focusedLane);
    };
    tickXPlusBtn.onClick = [this]
    {
        const int n = juce::jlimit (2, 32, curveDisplay.getXDivisions() + 1);
        curveDisplay.setXDivisions (n);
        refreshTickLabels();
        if (auto* param = proc.apvts.getParameter (laneParam (_focusedLane, ParamID::xDivisions)))
            param->setValueNotifyingHost (param->convertTo0to1 (static_cast<float> (n)));
        proc.updateLaneSnapshot (_focusedLane);
    };

    // Tick count indicator labels — shown between the − / + stepper buttons.
    for (auto* l : { &tickXCountLabel, &tickYCountLabel })
    {
        l->setFont (juce::Font (juce::FontOptions{}.withHeight (11.0f)));
        l->setJustificationType (juce::Justification::centred);
        addAndMakeVisible (*l);
    }
    // Step-size annotation labels — shown below (Y) and right-of-count (X).
    yTickStepLabel.setFont (juce::Font (juce::FontOptions{}.withHeight (9.0f)));
    yTickStepLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (yTickStepLabel);
    xTickStepLabel.setFont (juce::Font (juce::FontOptions{}.withHeight (9.0f)));
    xTickStepLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (xTickStepLabel);

    // ── Beat subdivision preset buttons (sync mode, X-axis row left side) ──────
    // Ordered finest→coarsest (left→right): 16t, 16, 8t, 8, 4t, 4 (quarter), 2 (half)
    // Each button sets xDivisions = round(syncBeats × multiplier).
    {
        static constexpr float kMults[kNumXStepPresets]       = { 6.f, 4.f, 3.f, 2.f, 1.5f, 1.f, 0.5f };
        static constexpr const char* kLabels[kNumXStepPresets] = {
            "\xe2\x99\xac\x33",  // ♬3  16th triplet
            "\xe2\x99\xac",      // ♬   16th
            "\xe2\x99\xaa\x33",  // ♪3  8th triplet
            "\xe2\x99\xaa",      // ♪   8th
            "\xe2\x99\xa9\x33",  // ♩3  quarter triplet
            "\xe2\x99\xa9",      // ♩   quarter
            "\xe2\x99\xa9\x32",  // ♩2  half
        };
        for (int i = 0; i < kNumXStepPresets; ++i)
        {
            xStepPresetBtns[static_cast<size_t>(i)].setLookAndFeel (&_presetBtnLF);
            xStepPresetBtns[static_cast<size_t>(i)].setButtonText (juce::String::fromUTF8 (kLabels[i]));
            const float mult = kMults[i];
            xStepPresetBtns[static_cast<size_t>(i)].onClick = [this, mult]
            {
                const float beats = proc.apvts.getRawParameterValue (ParamID::syncBeats)->load();
                const int   divs  = juce::jlimit (2, 32, juce::roundToInt (beats * mult));
                curveDisplay.setXDivisions (divs);
                refreshTickLabels();
                if (auto* param = proc.apvts.getParameter (laneParam (_focusedLane, ParamID::xDivisions)))
                    param->setValueNotifyingHost (param->convertTo0to1 (static_cast<float> (divs)));
                proc.updateLaneSnapshot (_focusedLane);
            };
            addAndMakeVisible (xStepPresetBtns[static_cast<size_t>(i)]);
        }
    }

    // ── Sync-beats duration preset buttons (sync mode, X-axis row right side) ──
    // Values: 1, 3, 4, 8, 16, 32 beats.
    {
        static constexpr int kDurs[kNumSyncBeatsPresets] = { 1, 3, 4, 8, 16, 32 };
        for (int i = 0; i < kNumSyncBeatsPresets; ++i)
        {
            syncBeatsPresetBtns[static_cast<size_t>(i)].setLookAndFeel (&_presetBtnLF);
            syncBeatsPresetBtns[static_cast<size_t>(i)].setButtonText (juce::String (kDurs[i]));
            const int dur = kDurs[i];
            syncBeatsPresetBtns[static_cast<size_t>(i)].onClick = [this, dur]
            {
                if (auto* param = proc.apvts.getParameter (ParamID::syncBeats))
                    param->setValueNotifyingHost (param->convertTo0to1 (static_cast<float> (dur)));
                speedSlider.setValue (dur, juce::dontSendNotification);
                refreshTickLabels();
            };
            addAndMakeVisible (syncBeatsPresetBtns[static_cast<size_t>(i)]);
        }
    }

    // ── Y-axis preset buttons ─────────────────────────────────────────────────
    // Note mode: octave range presets (set minOutput=0, maxOutput=N*12/127)
    {
        static constexpr int kOcts[kNumYOctavePresets] = { 8, 4, 3, 2, 1 };
        for (int i = 0; i < kNumYOctavePresets; ++i)
        {
            yOctavePresetBtns[static_cast<size_t>(i)].setLookAndFeel (&_presetBtnLF);
            yOctavePresetBtns[static_cast<size_t>(i)].setButtonText (juce::String (kOcts[i]));
            const int oct = kOcts[i];
            yOctavePresetBtns[static_cast<size_t>(i)].onClick = [this, oct]
            {
                const float newMax = juce::jmin (1.0f, oct * 12.0f / 127.0f);
                if (auto* pMin = proc.apvts.getParameter (laneParam (_focusedLane, ParamID::minOutput)))
                    pMin->setValueNotifyingHost (0.0f);
                if (auto* pMax = proc.apvts.getParameter (laneParam (_focusedLane, ParamID::maxOutput)))
                    pMax->setValueNotifyingHost (newMax);
                updateRangeSlider();
                proc.updateLaneSnapshot (_focusedLane);
                refreshTickLabels();
            };
            addAndMakeVisible (yOctavePresetBtns[static_cast<size_t>(i)]);
        }
    }

    // Note mode: semitone step presets (set yDivisions = round(span_semitones / N))
    {
        static constexpr int kNoteSteps[kNumYNoteStepPresets] = { 1, 2, 3, 4, 5 };
        for (int i = 0; i < kNumYNoteStepPresets; ++i)
        {
            yNoteStepPresetBtns[static_cast<size_t>(i)].setLookAndFeel (&_presetBtnLF);
            yNoteStepPresetBtns[static_cast<size_t>(i)].setButtonText (juce::String (kNoteSteps[i]));
            const int step = kNoteSteps[i];
            yNoteStepPresetBtns[static_cast<size_t>(i)].onClick = [this, step]
            {
                const float minOut = proc.apvts.getRawParameterValue (laneParam (_focusedLane, ParamID::minOutput))->load();
                const float maxOut = proc.apvts.getRawParameterValue (laneParam (_focusedLane, ParamID::maxOutput))->load();
                const int divs = juce::jlimit (2, 24, juce::roundToInt ((maxOut - minOut) * 127.0f / step));
                curveDisplay.setYDivisions (divs);
                if (auto* param = proc.apvts.getParameter (laneParam (_focusedLane, ParamID::yDivisions)))
                    param->setValueNotifyingHost (param->convertTo0to1 (static_cast<float> (divs)));
                proc.updateLaneSnapshot (_focusedLane);
                refreshTickLabels();
            };
            addAndMakeVisible (yNoteStepPresetBtns[static_cast<size_t>(i)]);
        }
    }

    // CC/Pressure mode: dozen range presets (set minOutput=0, maxOutput=N/127)
    {
        static constexpr int kDozens[kNumYDozenPresets] = { 72, 60, 48, 36, 24, 12 };
        for (int i = 0; i < kNumYDozenPresets; ++i)
        {
            yDozenPresetBtns[static_cast<size_t>(i)].setLookAndFeel (&_presetBtnLF);
            yDozenPresetBtns[static_cast<size_t>(i)].setButtonText (juce::String (kDozens[i]));
            const int val = kDozens[i];
            yDozenPresetBtns[static_cast<size_t>(i)].onClick = [this, val]
            {
                const float newMax = juce::jmin (1.0f, val / 127.0f);
                if (auto* pMin = proc.apvts.getParameter (laneParam (_focusedLane, ParamID::minOutput)))
                    pMin->setValueNotifyingHost (0.0f);
                if (auto* pMax = proc.apvts.getParameter (laneParam (_focusedLane, ParamID::maxOutput)))
                    pMax->setValueNotifyingHost (newMax);
                updateRangeSlider();
                proc.updateLaneSnapshot (_focusedLane);
                refreshTickLabels();
            };
            addAndMakeVisible (yDozenPresetBtns[static_cast<size_t>(i)]);
        }
    }

    // CC/Pressure mode: value step presets (set yDivisions = round(span_values / N))
    {
        static constexpr int kCCSteps[kNumYCCStepPresets] = { 1, 2, 3, 4, 6 };
        for (int i = 0; i < kNumYCCStepPresets; ++i)
        {
            yCCStepPresetBtns[static_cast<size_t>(i)].setLookAndFeel (&_presetBtnLF);
            yCCStepPresetBtns[static_cast<size_t>(i)].setButtonText (juce::String (kCCSteps[i]));
            const int step = kCCSteps[i];
            yCCStepPresetBtns[static_cast<size_t>(i)].onClick = [this, step]
            {
                const float minOut = proc.apvts.getRawParameterValue (laneParam (_focusedLane, ParamID::minOutput))->load();
                const float maxOut = proc.apvts.getRawParameterValue (laneParam (_focusedLane, ParamID::maxOutput))->load();
                const int divs = juce::jlimit (2, 24, juce::roundToInt ((maxOut - minOut) * 127.0f / step));
                curveDisplay.setYDivisions (divs);
                if (auto* param = proc.apvts.getParameter (laneParam (_focusedLane, ParamID::yDivisions)))
                    param->setValueNotifyingHost (param->convertTo0to1 (static_cast<float> (divs)));
                proc.updateLaneSnapshot (_focusedLane);
                refreshTickLabels();
            };
            addAndMakeVisible (yCCStepPresetBtns[static_cast<size_t>(i)]);
        }
    }

    // PitchBend mode: range presets centred at 0 (k = 2048, so 4k = ±8192 = full range)
    // Labels "4k","3k","2k","1k" (large → small). No step presets for PB.
    {
        // N × 2048 = half-span in pitch-bend units; normalised: centre = 0.5, span = N/8
        static constexpr int kPBK[kNumYPBRangePresets] = { 4, 3, 2, 1 };
        static constexpr const char* kPBLabels[kNumYPBRangePresets] = { "4k", "3k", "2k", "1k" };
        for (int i = 0; i < kNumYPBRangePresets; ++i)
        {
            yPBRangePresetBtns[static_cast<size_t>(i)].setLookAndFeel (&_presetBtnLF);
            yPBRangePresetBtns[static_cast<size_t>(i)].setButtonText (kPBLabels[i]);
            const int k = kPBK[i];
            yPBRangePresetBtns[static_cast<size_t>(i)].onClick = [this, k]
            {
                const float halfSpan = k / 8.0f;               // k × 2048 / 16384
                const float newMin = juce::jmax (0.0f, 0.5f - halfSpan);
                const float newMax = juce::jmin (1.0f, 0.5f + halfSpan);
                if (auto* pMin = proc.apvts.getParameter (laneParam (_focusedLane, ParamID::minOutput)))
                    pMin->setValueNotifyingHost (newMin);
                if (auto* pMax = proc.apvts.getParameter (laneParam (_focusedLane, ParamID::maxOutput)))
                    pMax->setValueNotifyingHost (newMax);
                updateRangeSlider();
                proc.updateLaneSnapshot (_focusedLane);
                refreshTickLabels();
            };
            addAndMakeVisible (yPBRangePresetBtns[static_cast<size_t>(i)]);
        }
    }

    // PitchBend mode: step presets (yDivisions = round(span_pb / N))
    {
        static constexpr int kPBSteps[kNumYPBStepPresets] = { 64, 128, 256, 512 };
        for (int i = 0; i < kNumYPBStepPresets; ++i)
        {
            yPBStepPresetBtns[static_cast<size_t>(i)].setLookAndFeel (&_presetBtnLF);
            yPBStepPresetBtns[static_cast<size_t>(i)].setButtonText (juce::String (kPBSteps[i]));
            const int step = kPBSteps[i];
            yPBStepPresetBtns[static_cast<size_t>(i)].onClick = [this, step]
            {
                const float minOut = proc.apvts.getRawParameterValue (laneParam (_focusedLane, ParamID::minOutput))->load();
                const float maxOut = proc.apvts.getRawParameterValue (laneParam (_focusedLane, ParamID::maxOutput))->load();
                const int divs = juce::jlimit (2, 24, juce::roundToInt ((maxOut - minOut) * 16384.0f / step));
                curveDisplay.setYDivisions (divs);
                if (auto* param = proc.apvts.getParameter (laneParam (_focusedLane, ParamID::yDivisions)))
                    param->setValueNotifyingHost (param->convertTo0to1 (static_cast<float> (divs)));
                proc.updateLaneSnapshot (_focusedLane);
                refreshTickLabels();
            };
            addAndMakeVisible (yPBStepPresetBtns[static_cast<size_t>(i)]);
        }
    }

    // Seed display from lane 0's persisted APVTS values.
    {
        const int xDiv = static_cast<int> (proc.apvts.getRawParameterValue (laneParam (0, ParamID::xDivisions))->load());
        const int yDiv = static_cast<int> (proc.apvts.getRawParameterValue (laneParam (0, ParamID::yDivisions))->load());
        curveDisplay.setXDivisions (xDiv);
        curveDisplay.setYDivisions (yDiv);
        refreshTickLabels();
    }

    // ── Axis quantization toggle buttons (icon-based, no LF needed) ──────────
    xQuantizeBtn.setClickingTogglesState (true);
    yQuantizeBtn.setClickingTogglesState (true);
    // Closed lock = quantize on; open lock = quantize off
    xQuantizeBtn.setToggleIcons (dcui::IconType::lock, dcui::IconType::lockOpen);
    yQuantizeBtn.setToggleIcons (dcui::IconType::lock, dcui::IconType::lockOpen);
    addAndMakeVisible (xQuantizeBtn);
    addAndMakeVisible (yQuantizeBtn);

    xQuantizeBtn.onClick = [this]
    {
        const bool on = xQuantizeBtn.getToggleState();
        if (auto* param = proc.apvts.getParameter (laneParam (_focusedLane, ParamID::xQuantize)))
            param->setValueNotifyingHost (on ? 1.0f : 0.0f);
        proc.updateLaneSnapshot (_focusedLane);
    };
    yQuantizeBtn.onClick = [this]
    {
        const bool on = yQuantizeBtn.getToggleState();
        if (auto* param = proc.apvts.getParameter (laneParam (_focusedLane, ParamID::yQuantize)))
            param->setValueNotifyingHost (on ? 1.0f : 0.0f);
        proc.updateLaneSnapshot (_focusedLane);
    };

    // ── '#' combo: activates both X and Y quantize simultaneously ────────────
    addAndMakeVisible (xyLockBtn);
    xyLockBtn.onClick = [this]
    {
        const bool xOn = xQuantizeBtn.getToggleState();
        const bool yOn = yQuantizeBtn.getToggleState();
        // If both are already on, turn both off; otherwise turn both on.
        const bool target = !(xOn && yOn);
        xQuantizeBtn.setToggleState (target, juce::dontSendNotification);
        yQuantizeBtn.setToggleState (target, juce::dontSendNotification);
        if (auto* px = proc.apvts.getParameter (laneParam (_focusedLane, ParamID::xQuantize)))
            px->setValueNotifyingHost (target ? 1.0f : 0.0f);
        if (auto* py = proc.apvts.getParameter (laneParam (_focusedLane, ParamID::yQuantize)))
            py->setValueNotifyingHost (target ? 1.0f : 0.0f);
        proc.updateLaneSnapshot (_focusedLane);
    };

    // Seed quantize button toggle states from saved APVTS params (lane 0 at startup).
    // setFocusedLane() syncs these when the lane changes, but must also be set here
    // so a newly opened editor reflects the persisted values immediately.
    xQuantizeBtn.setToggleState (
        proc.apvts.getRawParameterValue (laneParam (0, ParamID::xQuantize))->load() > 0.5f,
        juce::dontSendNotification);
    yQuantizeBtn.setToggleState (
        proc.apvts.getRawParameterValue (laneParam (0, ParamID::yQuantize))->load() > 0.5f,
        juce::dontSendNotification);

    // ── Lane focus selector ───────────────────────────────────────────────────
    updateLaneFocusCtrl();   // builds ✤ + lane segments from proc.activeLaneCount
    laneFocusCtrl.onChange = [this] (int idx)
    {
        if (idx == 0)
        {
            // "✤" All Lanes tab
            _showingAllLanes = true;
#if defined(DC_HAVE_PER_LANE_PLAYBACK_PARAMS)
            bindPlaybackToLane (-1);
#endif
            updateScaleVisibility();
            resized();   // re-show the routing button for lane 0
            repaint();
        }
        else
        {
            setFocusedLane (idx - 1);
        }
    };
    addAndMakeVisible (laneFocusCtrl);

    // ── Eyebrow lane management buttons ──────────────────────────────────────
    eyebrowAddLaneBtn.setButtonText ("+");
    eyebrowAddLaneBtn.onClick = [this] { addLane(); };
    addAndMakeVisible (eyebrowAddLaneBtn);

    eyebrowDeleteLaneBtn.setButtonText ("-");
    eyebrowDeleteLaneBtn.onClick = [this] { deleteLane (_focusedLane); };
    addAndMakeVisible (eyebrowDeleteLaneBtn);

    // Right-rail add/delete kept for legacy reasons but always hidden.
    addLaneBtn.setButtonText ("+");
    addLaneBtn.onClick = [this] { addLane(); };
    addAndMakeVisible (addLaneBtn);

    deleteLaneBtn.setButtonText ("-");
    deleteLaneBtn.onClick = [this] { deleteLane (_focusedLane); };
    addAndMakeVisible (deleteLaneBtn);

    // Routing overlay — child covering the full editor, shown on demand.
    addChildComponent (_routingOverlay);

    // ── Shaping sliders ───────────────────────────────────────────────────────
    setupSlider (smoothingSlider, smoothingLabel, {});
    smoothingSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    smoothingLabel.setEditable (true, true, false);
    smoothingLabel.onTextChange = [this]
    {
        juce::String t = smoothingLabel.getText().trim().trimCharactersAtEnd ("%");
        const float v = juce::jlimit (0.0f, 100.0f, t.getFloatValue());
        if (auto* param = proc.apvts.getParameter (laneParam (_focusedLane, "smoothing")))
            param->setValueNotifyingHost (param->convertTo0to1 (v / 100.0f));
    };
    rangeSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    rangeSlider.setRange (0.0, 1.0, 0.001);
    addAndMakeVisible (rangeSlider);
    rangeLabel.setFont (juce::Font (juce::FontOptions{}.withHeight (11.0f)));
    rangeLabel.setEditable (true, true, false);
    rangeLabel.onTextChange = [this] { applyRangeLabelText (rangeLabel.getText()); };
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
            const auto btnBounds = laneTypeBtn[static_cast<size_t>(L)].getBoundsInParent();

            // All-lanes mode: open mini matrix overlay regardless of which lane button is shown
            if (_showingAllLanes)
            {
                if (_routingOverlay.isVisible() && _routingOverlay.activeLane() == -1)
                    { _routingOverlay.dismiss(); return; }
                _routingOverlay.showForAllLanes (btnBounds);
                return;
            }

            // Individual lane mode
            if (_routingOverlay.isVisible() && _routingOverlay.activeLane() == L)
                { _routingOverlay.dismiss(); return; }
            _routingOverlay.showForLane (L, btnBounds);
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
        proc.apvts.addParameterListener (laneParam (L, ParamID::xDivisions), this);
        proc.apvts.addParameterListener (laneParam (L, ParamID::yDivisions), this);
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

    // ── Scale overlay backdrop — added AFTER all other components so it draws on top.
    // hitTest punches through the panel area so scale components below remain interactive.
    _scaleOverlay.onDismiss = [this] { closeScaleOverlay(); };
    _scaleOverlay.setVisible (false);
    addChildComponent (_scaleOverlay);

    // Badge tap → open scale overlay
    curveDisplay.onBadgeTap    = [this] { openScaleOverlay(); };
    curveDisplay.onCurveDrawn  = [this] { refreshTickLabels(); };

    // Register global param listeners
    proc.apvts.addParameterListener (ParamID::playbackDirection, this);
    proc.apvts.addParameterListener (ParamID::syncEnabled,       this);
    proc.apvts.addParameterListener (ParamID::playbackSpeed,     this);
    proc.apvts.addParameterListener (ParamID::syncBeats,         this);
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

    // Defer tick-label refresh so lane snapshots are fully populated before we read durations.
    juce::MessageManager::callAsync ([this] { refreshTickLabels(); });
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
    proc.apvts.removeParameterListener (ParamID::playbackSpeed,     this);
    proc.apvts.removeParameterListener (ParamID::syncBeats,         this);
    for (int L = 0; L < kMaxLanes; ++L)
    {
        for (const auto& base : { "msgType", "ccNumber", "midiChannel", "noteVelocity",
                                   "enabled", "loopMode", "phaseOffset", "minOutput", "maxOutput", "smoothing",
                                   "xDivisions", "yDivisions" })
            proc.apvts.removeParameterListener (laneParam (L, base), this);
#if defined(DC_HAVE_PER_LANE_PLAYBACK_PARAMS)
        proc.apvts.removeParameterListener (laneParam (L, ParamID::laneDirection), this);
#endif
    }
    // Global scale params
    for (const auto& id : { "scaleMode", "scaleRoot", "scaleMask" })
        proc.apvts.removeParameterListener (id, this);

    // Disconnect standalone MIDI outputs before destroying them.
    proc.setVirtualMidiOutput (nullptr);
    proc.setDirectMidiOutput (nullptr);
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

void DrawnCurveEditor::showMidiOutputPicker()
{
    const auto devices = juce::MidiOutput::getAvailableDevices();
    const bool virtualOn = (_virtualMidiPort != nullptr && !_virtualPortMuted);

    juce::PopupMenu menu;

    // Virtual port toggle
    menu.addItem (1, "Virtual port (DrawnCurve)", true, virtualOn);
    menu.addSeparator();

    // Direct target section
    menu.addSectionHeader ("Direct target:");
    menu.addItem (2, "None", true, _standaloneOut == nullptr);

    for (int i = 0; i < devices.size(); ++i)
    {
        const bool isCurrent = (_standaloneOut != nullptr
                                && _standaloneOut->getIdentifier() == devices[i].identifier);
        menu.addItem (i + 3, devices[i].name, true, isCurrent);
    }

    menu.showMenuAsync (juce::PopupMenu::Options()
                            .withTargetComponent (&midiOutBtn),
        [this, devices, virtualOn] (int result)
        {
            if (result <= 0) return;   // dismissed

            // Toggle virtual port (mute/unmute — port stays alive so other apps keep their connection)
            if (result == 1)
            {
                if (virtualOn)
                {
                    // 1. Disconnect — stops the audio thread from sending new messages
                    proc.setVirtualMidiOutput (nullptr);
                    // 2. Brute-force Note Off for all 128 pitches on all 16 channels.
                    //    This catches any note that slipped through during the race
                    //    window.  CC 123/120 are also sent but many iOS synths ignore
                    //    them, so explicit Note Offs are the reliable path.
                    if (_virtualMidiPort != nullptr)
                        for (int ch = 1; ch <= 16; ++ch)
                        {
                            _virtualMidiPort->sendMessageNow (
                                juce::MidiMessage::controllerEvent (ch, 123, 0));
                            _virtualMidiPort->sendMessageNow (
                                juce::MidiMessage::controllerEvent (ch, 120, 0));
                            for (int note = 0; note < 128; ++note)
                                _virtualMidiPort->sendMessageNow (
                                    juce::MidiMessage::noteOff (ch, note, (uint8_t) 0));
                        }
                    _virtualPortMuted = true;
                }
                else
                {
                    if (_virtualMidiPort != nullptr)
                        proc.setVirtualMidiOutput (_virtualMidiPort.get());
                    _virtualPortMuted = false;
                }
                updateMidiOutBtnText();
                return;
            }

            // "None" direct target
            if (result == 2)
            {
                proc.setDirectMidiOutput (nullptr);
                _standaloneOut.reset();
                updateMidiOutBtnText();
                return;
            }

            const int idx = result - 3;
            if (idx < 0 || idx >= devices.size()) return;

            auto newOut = juce::MidiOutput::openDevice (devices[idx].identifier);
            if (newOut == nullptr) return;

            proc.setDirectMidiOutput (nullptr);
            _standaloneOut = std::move (newOut);
            proc.setDirectMidiOutput (_standaloneOut.get());
            updateMidiOutBtnText();
        });
}

void DrawnCurveEditor::updateMidiOutBtnText()
{
    const bool virtualOn = (_virtualMidiPort != nullptr && !_virtualPortMuted);
    const bool hasDirect = (_standaloneOut != nullptr);

    if (hasDirect)
    {
        const auto name = _standaloneOut->getName();
        midiOutBtn.setButtonText (name.length() > 9
            ? name.substring (0, 8) + juce::String::fromUTF8 ("\xe2\x80\xa6")
            : name);
    }
    else if (virtualOn)
    {
        midiOutBtn.setButtonText ("MIDI Out");
    }
    else
    {
        midiOutBtn.setButtonText ("MIDI Off");
    }
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

    _focusedLane = juce::jlimit (0, proc.activeLaneCount - 1, lane);
    curveDisplay.setFocusedLane (_focusedLane);
    // Index 0 is ✤ (all-lanes); individual lanes are at index (lane+1)
    laneFocusCtrl.setSelectedIndex (_focusedLane + 1, juce::dontSendNotification);
    resized();   // re-position eyebrow routing button for the new focused lane
    bindShapingToLane (_focusedLane);

    // Sync quantize toggle buttons and grid density to the newly focused lane's params.
    xQuantizeBtn.setToggleState (
        proc.apvts.getRawParameterValue (laneParam (_focusedLane, ParamID::xQuantize))->load() > 0.5f,
        juce::dontSendNotification);
    yQuantizeBtn.setToggleState (
        proc.apvts.getRawParameterValue (laneParam (_focusedLane, ParamID::yQuantize))->load() > 0.5f,
        juce::dontSendNotification);
    {
        const int xDiv = static_cast<int> (proc.apvts.getRawParameterValue (laneParam (_focusedLane, ParamID::xDivisions))->load());
        const int yDiv = static_cast<int> (proc.apvts.getRawParameterValue (laneParam (_focusedLane, ParamID::yDivisions))->load());
        curveDisplay.setXDivisions (xDiv);
        curveDisplay.setYDivisions (yDiv);
        refreshTickLabels();
    }
#if defined(DC_HAVE_PER_LANE_PLAYBACK_PARAMS)
    bindPlaybackToLane (_focusedLane);
#endif
    updateScaleVisibility();
    updateLaneRow (_focusedLane);
    repaint();
}

void DrawnCurveEditor::updateLaneFocusCtrl()
{
    std::vector<SegmentedControl::Segment> segs;
    // Index 0 — "✤" All Lanes
    segs.push_back ({ "all",
                      juce::String::fromUTF8 ("\xe2\x9c\xa4"),   // ✤
                      "All Lanes" });
    // Indices 1..N — individual lanes
    for (int L = 0; L < proc.activeLaneCount; ++L)
        segs.push_back ({ "l" + juce::String (L + 1),
                          juce::String (L + 1),
                          "Lane " + juce::String (L + 1) });
    laneFocusCtrl.setSegments (std::move (segs));

    // Selected index: 0 = all-lanes, (lane+1) = specific lane
    const int selIdx = _showingAllLanes
        ? 0
        : juce::jlimit (1, proc.activeLaneCount, _focusedLane + 1);
    laneFocusCtrl.setSelectedIndex (selIdx, juce::dontSendNotification);
}

void DrawnCurveEditor::addLane()
{
    if (proc.activeLaneCount >= kMaxLanes) return;
    ++proc.activeLaneCount;

    updateLaneFocusCtrl();
    resized();
    applyTheme();
    repaint();
}

void DrawnCurveEditor::deleteLane (int lane)
{
    if (proc.activeLaneCount <= 1) return;

    // Keep focused lane in bounds: if we're deleting it or it would be above the new last lane,
    // move focus down by one.
    const int newCount = proc.activeLaneCount - 1;
    if (_focusedLane >= lane)
        setFocusedLane (juce::jmax (0, _focusedLane - 1));
    else if (_focusedLane >= newCount)
        setFocusedLane (newCount - 1);

    proc.deleteLane (lane);

    updateLaneFocusCtrl();
    updateAllLaneRows();
    resized();
    applyTheme();
    repaint();
}

void DrawnCurveEditor::bindShapingToLane (int lane)
{
    // Smoothing attachment
    smoothingAttach.reset();
    smoothingAttach = std::make_unique<Attach> (proc.apvts, laneParam (lane, "smoothing"), smoothingSlider);
    // Update smooth label to show the current value as a percentage
    {
        const float sv = proc.apvts.getRawParameterValue (laneParam (lane, "smoothing"))->load();
        smoothingLabel.setText (juce::String (juce::roundToInt (sv * 100)) + "%",
                                juce::dontSendNotification);
    }

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
    for (int L = 0; L < proc.activeLaneCount; ++L)
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
            refreshTickLabels();
        });
        return;
    }

    if (paramID == ParamID::playbackSpeed || paramID == ParamID::syncBeats)
    {
        juce::MessageManager::callAsync ([this] { updateSpeedLabel(); refreshTickLabels(); });
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
                if (L == _focusedLane) refreshTickLabels();   // Y step unit depends on msgType
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
                juce::MessageManager::callAsync ([this] { updateRangeSlider(); refreshTickLabels(); });
            return;
        }

        if (paramID == laneParam (L, "smoothing"))
        {
            // Re-bake smoothing into snapshot immediately (attachment fires this).
            proc.updateLaneSnapshot (L);
            if (L == _focusedLane)
            {
                juce::MessageManager::callAsync ([this, L] {
                    const float sv = proc.apvts.getRawParameterValue (laneParam (L, "smoothing"))->load();
                    smoothingLabel.setText (juce::String (juce::roundToInt (sv * 100)) + "%",
                                           juce::dontSendNotification);
                });
            }
            return;
        }

        if (paramID == laneParam (L, "phaseOffset"))
        {
            // Re-bake phase offset into snapshot immediately (attachment fires this).
            proc.updateLaneSnapshot (L);
            return;
        }

        if (paramID == laneParam (L, ParamID::xQuantize)
            || paramID == laneParam (L, ParamID::yQuantize)
            || paramID == laneParam (L, ParamID::legatoMode))
        {
            proc.updateLaneSnapshot (L);
            if (L == _focusedLane)
            {
                juce::MessageManager::callAsync ([this, L] {
                    const bool xq = proc.apvts.getRawParameterValue (laneParam (L, ParamID::xQuantize))->load() > 0.5f;
                    const bool yq = proc.apvts.getRawParameterValue (laneParam (L, ParamID::yQuantize))->load() > 0.5f;
                    xQuantizeBtn.setToggleState (xq, juce::dontSendNotification);
                    yQuantizeBtn.setToggleState (yq, juce::dontSendNotification);
                });
            }
            return;
        }

    }

    // Per-lane grid density — rebake snapshot and sync display if it's the focused lane
    for (int L = 0; L < kMaxLanes; ++L)
    {
        if (paramID == laneParam (L, ParamID::xDivisions) || paramID == laneParam (L, ParamID::yDivisions))
        {
            proc.updateLaneSnapshot (L);
            if (L == _focusedLane)
            {
                juce::MessageManager::callAsync ([this, L] {
                    const int xDiv = static_cast<int> (proc.apvts.getRawParameterValue (laneParam (L, ParamID::xDivisions))->load());
                    const int yDiv = static_cast<int> (proc.apvts.getRawParameterValue (laneParam (L, ParamID::yDivisions))->load());
                    curveDisplay.setXDivisions (xDiv);
                    curveDisplay.setYDivisions (yDiv);
                    refreshTickLabels();
                });
            }
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
        speedSlider.setTextValueSuffix ("x");
        speedSlider.setNumDecimalPlacesToDisplay (2);
    }
    updateSpeedLabel();

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
    resized();   // re-layout X-axis row to show/hide beat preset buttons
}

//==============================================================================
// Scale helpers
//==============================================================================

void DrawnCurveEditor::updateScaleVisibility()
{
    // When the overlay is open, it manages scale component visibility via resized().
    // Don't call resized() here — this function itself may be called from within resized().
    if (_scaleOverlayOpen) return;

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
        rangeLabel.setText (noteName (mn) + " \xe2\x80\x93 " + noteName (mx),
                            juce::dontSendNotification);
    }
    else if (msgType == MessageType::PitchBend)
    {
        // Show signed pitch-bend values (-8192 – +8191)
        const int lo = juce::jlimit (-8192, 8191, juce::roundToInt (mn * 16383.0f) - 8192);
        const int hi = juce::jlimit (-8192, 8191, juce::roundToInt (mx * 16383.0f) - 8192);
        rangeLabel.setText (juce::String (lo) + " \xe2\x80\x93 " + juce::String (hi),
                            juce::dontSendNotification);
    }
    else
    {
        // CC (0-127) and Channel Pressure (0-127)
        const int lo = juce::jlimit (0, 127, juce::roundToInt (mn * 127.0f));
        const int hi = juce::jlimit (0, 127, juce::roundToInt (mx * 127.0f));
        rangeLabel.setText (juce::String (lo) + " \xe2\x80\x93 " + juce::String (hi),
                            juce::dontSendNotification);
    }

    // Repaint so the collapsed musical summary chip reflects the updated range text.
    repaint();
}

// ---------------------------------------------------------------------------
// Parse a MIDI note name string such as "C3", "C#3", "Db-1", "C♯4", "D♭5".
// Returns the MIDI note number [0,127] or -1 on failure.
static int parseMidiNoteName (const juce::String& s)
{
    if (s.isEmpty()) return -1;
    int pos = 0;

    // Letter (A–G, case-insensitive)
    const juce::juce_wchar letter = juce::CharacterFunctions::toUpperCase (s[pos++]);
    int semitone;
    switch (letter)
    {
        case 'C': semitone = 0;  break;
        case 'D': semitone = 2;  break;
        case 'E': semitone = 4;  break;
        case 'F': semitone = 5;  break;
        case 'G': semitone = 7;  break;
        case 'A': semitone = 9;  break;
        case 'B': semitone = 11; break;
        default:  return -1;
    }

    // Optional accidental: #, b, ♯ (U+266F), ♭ (U+266D)
    if (pos < s.length())
    {
        const juce::juce_wchar acc = s[pos];
        if (acc == '#' || acc == 0x266F) { semitone++; pos++; }
        else if (acc == 'b' || acc == 0x266D) { semitone--; pos++; }
    }

    // Octave number (may be negative, e.g. "C-1")
    if (pos >= s.length()) return -1;
    const juce::String octStr = s.substring (pos).trim();
    if (octStr.isEmpty()) return -1;
    const int oct = octStr.getIntValue();
    const int midi = (oct + 1) * 12 + semitone;
    return std::clamp (midi, 0, 127);
}

void DrawnCurveEditor::applyRangeLabelText (const juce::String& rawText)
{
    const juce::String text = rawText.trim();

    // Find the separator — en-dash "–", " - ", or hyphen between non-negative tokens
    static const juce::juce_wchar kEnDash = 0x2013;
    const juce::String enDash = juce::String::charToString (kEnDash);

    int sepIdx = -1;
    int sepLen = 0;

    // Try " – " (en-dash with spaces)
    sepIdx = text.indexOf (" " + enDash + " ");
    if (sepIdx >= 0) { sepLen = 3; }
    else
    {
        // Bare en-dash
        sepIdx = text.indexOf (enDash);
        if (sepIdx >= 0) { sepLen = 1; }
        else
        {
            // " - " (hyphen with spaces)
            sepIdx = text.indexOf (" - ");
            if (sepIdx >= 0) { sepLen = 3; }
            else
            {
                // Hyphen not at start (avoid negative numbers like "-8192")
                for (int i = 1; i < text.length(); ++i)
                {
                    if (text[i] == '-') { sepIdx = i; sepLen = 1; break; }
                }
            }
        }
    }

    if (sepIdx < 0) return;

    const juce::String left  = text.substring (0, sepIdx).trim();
    const juce::String right = text.substring (sepIdx + sepLen).trim();
    if (left.isEmpty() || right.isEmpty()) return;

    const auto msgType = static_cast<MessageType> (static_cast<int> (
        proc.apvts.getRawParameterValue (laneParam (_focusedLane, "msgType"))->load()));

    float minNorm = -1.0f, maxNorm = -1.0f;

    if (msgType == MessageType::Note)
    {
        // Accept note names ("C#3") or raw MIDI integers ("60")
        int lo = parseMidiNoteName (left);
        int hi = parseMidiNoteName (right);
        if (lo < 0) lo = juce::jlimit (0, 127, left .getIntValue());
        if (hi < 0) hi = juce::jlimit (0, 127, right.getIntValue());
        minNorm = static_cast<float> (lo) / 127.0f;
        maxNorm = static_cast<float> (hi) / 127.0f;
    }
    else if (msgType == MessageType::PitchBend)
    {
        const int lo = left .getIntValue();   // signed: -8192 – +8191
        const int hi = right.getIntValue();
        minNorm = (static_cast<float> (lo) + 8192.0f) / 16383.0f;
        maxNorm = (static_cast<float> (hi) + 8192.0f) / 16383.0f;
    }
    else  // CC / Channel Pressure (0-127)
    {
        const int lo = juce::jlimit (0, 127, left .getIntValue());
        const int hi = juce::jlimit (0, 127, right.getIntValue());
        minNorm = static_cast<float> (lo) / 127.0f;
        maxNorm = static_cast<float> (hi) / 127.0f;
    }

    minNorm = juce::jlimit (0.0f, 1.0f, minNorm);
    maxNorm = juce::jlimit (0.0f, 1.0f, maxNorm);
    if (minNorm >= maxNorm) return;   // degenerate range — ignore

    rangeSlider.setMinValue (static_cast<double> (minNorm), juce::sendNotification);
    rangeSlider.setMaxValue (static_cast<double> (maxNorm), juce::sendNotification);
}

void DrawnCurveEditor::updateSpeedLabel()
{
    const bool sync = proc.apvts.getRawParameterValue (ParamID::syncEnabled)->load() > 0.5f;
    if (sync)
    {
        const int beats = juce::roundToInt (
            proc.apvts.getRawParameterValue (ParamID::syncBeats)->load());
        speedLabel.setText (juce::String (beats) + "\xe2\x99\xa9",   // ♩
                            juce::dontSendNotification);
    }
    else
    {
        const float spd = proc.apvts.getRawParameterValue (ParamID::playbackSpeed)->load();
        speedLabel.setText (juce::String (spd, 2) + "\xc3\x97",      // ×
                            juce::dontSendNotification);
    }
}

juce::String DrawnCurveEditor::yTickText (int n) const
{
    if (n <= 0) return {};
    const int msgType = static_cast<int> (
        proc.apvts.getRawParameterValue (laneParam (_focusedLane, ParamID::msgType))->load());
    const float minOut = proc.apvts.getRawParameterValue (laneParam (_focusedLane, ParamID::minOutput))->load();
    const float maxOut = proc.apvts.getRawParameterValue (laneParam (_focusedLane, ParamID::maxOutput))->load();
    const float range  = maxOut - minOut;
    if (range <= 0.0f) return {};

    // Format a float value without trailing zeros, max 1 decimal place.
    auto fmt = [] (float v) -> juce::String {
        if (v >= 9.5f)  return juce::String (juce::roundToInt (v));
        if (v >= 0.95f) return juce::String (v, 1);
        return juce::String (v, 2);
    };

    switch (msgType)
    {
        case 3:  // Note — semitone steps
        {
            const float step = range * 127.0f / static_cast<float> (n);
            if (step < 0.05f) return {};
            return fmt (step) + "st";
        }
        case 0:  // CC — 0-127 value steps
        case 1:  // AT — 0-127 value steps
        {
            const float step = range * 127.0f / static_cast<float> (n);
            if (step < 0.1f) return {};
            return fmt (step);
        }
        case 2:  // PB — percentage of range per step
        {
            const float step = range * 100.0f / static_cast<float> (n);
            if (step < 0.1f) return {};
            return fmt (step) + "%";
        }
        default: return {};
    }
}

juce::String DrawnCurveEditor::xTickText (int n) const
{
    if (n <= 0) return {};
    const bool isSync = proc.apvts.getRawParameterValue (ParamID::syncEnabled)->load() > 0.5f;
    if (isSync)
    {
        const float beats = proc.apvts.getRawParameterValue (ParamID::syncBeats)->load();
        const float step  = beats / static_cast<float> (n);

        // Match known note values and return the corresponding symbol.
        struct NoteVal { float val; const char* sym; };
        static constexpr NoteVal kNoteVals[] = {
            { 2.f,        "\xe2\x99\xa9\x32"       },  // ♩2  half note (2 beats)
            { 1.f,        "\xe2\x99\xa9"            },  // ♩   quarter note (1 beat)
            { 2.f / 3.f,  "\xe2\x99\xa9\x33"       },  // ♩3  quarter triplet
            { 0.5f,       "\xe2\x99\xaa"            },  // ♪   eighth note
            { 1.f / 3.f,  "\xe2\x99\xaa\x33"       },  // ♪3  eighth triplet
            { 0.25f,      "\xe2\x99\xac"            },  // ♬   sixteenth note
            { 1.f / 6.f,  "\xe2\x99\xac\x33"       },  // ♬3  sixteenth triplet
        };
        for (const auto& nv : kNoteVals)
            if (std::fabs (step - nv.val) < 0.005f)
                return juce::String::fromUTF8 (nv.sym);

        // Fallback: whole-beat integer or decimal
        const juce::String noteQ = juce::String::fromUTF8 ("\xe2\x99\xa9");
        if (step >= 0.5f && std::fmod (step, 1.0f) < 0.01f)
            return juce::String (juce::roundToInt (step)) + noteQ;
        return juce::String (step, 2) + noteQ;
    }
    else
    {
        const float dur = proc.curveDuration (_focusedLane);
        if (dur <= 0.0f) return {};
        const float speedRatio = proc.getEffectiveSpeedRatio();
        const float loopDur    = (speedRatio > 0.001f) ? dur / speedRatio : dur;
        const float step       = loopDur / static_cast<float> (n);
        if (step >= 9.5f)   return juce::String (juce::roundToInt (step)) + "s";
        if (step >= 0.095f) return juce::String (step, 2) + "s";
        return juce::String (juce::roundToInt (step * 1000.0f)) + "ms";
    }
}

void DrawnCurveEditor::refreshTickLabels()
{
    const int xDiv = curveDisplay.getXDivisions();
    const int yDiv = curveDisplay.getYDivisions();
    tickXCountLabel.setText (juce::String (xDiv), juce::dontSendNotification);
    tickYCountLabel.setText (juce::String (yDiv), juce::dontSendNotification);
    xTickStepLabel .setText (xTickText (xDiv),    juce::dontSendNotification);
    yTickStepLabel .setText (yTickText (yDiv),    juce::dontSendNotification);

    // X step preset enabled states (sync mode only)
    const bool isSync = proc.apvts.getRawParameterValue (ParamID::syncEnabled)->load() > 0.5f;
    if (isSync)
    {
        static constexpr float kStepMults[kNumXStepPresets] = { 6.f, 4.f, 3.f, 2.f, 1.5f, 1.f, 0.5f };
        const float beats = proc.apvts.getRawParameterValue (ParamID::syncBeats)->load();
        for (int i = 0; i < kNumXStepPresets; ++i)
        {
            const int divs = juce::roundToInt (beats * kStepMults[i]);
            xStepPresetBtns[static_cast<size_t>(i)].setEnabled (divs >= 2 && divs <= 32);
        }
    }

    // Y step preset enabled states (based on current range span)
    {
        const int yMsgType = static_cast<int> (
            proc.apvts.getRawParameterValue (laneParam (_focusedLane, ParamID::msgType))->load());
        const float minOut = proc.apvts.getRawParameterValue (laneParam (_focusedLane, ParamID::minOutput))->load();
        const float maxOut = proc.apvts.getRawParameterValue (laneParam (_focusedLane, ParamID::maxOutput))->load();
        const float span   = (maxOut - minOut) * 127.0f;

        if (yMsgType == 3)  // Note — semitone steps
        {
            static constexpr int kNoteSteps[kNumYNoteStepPresets] = { 1, 2, 3, 4, 5 };
            for (int i = 0; i < kNumYNoteStepPresets; ++i)
                yNoteStepPresetBtns[static_cast<size_t>(i)].setEnabled (span >= 0.5f &&
                    juce::roundToInt (span / kNoteSteps[i]) >= 2 &&
                    juce::roundToInt (span / kNoteSteps[i]) <= 24);
        }
        else if (yMsgType == 0 || yMsgType == 1)  // CC / Pressure — value steps
        {
            static constexpr int kCCSteps[kNumYCCStepPresets] = { 1, 2, 3, 4, 6 };
            for (int i = 0; i < kNumYCCStepPresets; ++i)
                yCCStepPresetBtns[static_cast<size_t>(i)].setEnabled (span >= 0.5f &&
                    juce::roundToInt (span / kCCSteps[i]) >= 2 &&
                    juce::roundToInt (span / kCCSteps[i]) <= 24);
        }
        else if (yMsgType == 2)  // PitchBend — absolute PB unit steps
        {
            const float pbSpan = (maxOut - minOut) * 16384.0f;
            static constexpr int kPBSteps[kNumYPBStepPresets] = { 64, 128, 256, 512 };
            for (int i = 0; i < kNumYPBStepPresets; ++i)
            {
                const int divs = juce::roundToInt (pbSpan / kPBSteps[i]);
                yPBStepPresetBtns[static_cast<size_t>(i)].setEnabled (divs >= 2 && divs <= 24);
            }
        }
    }
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

    for (auto* l : { &smoothingLabel, &rangeLabel, &speedLabel, &phaseOffsetLabel,
                     &tickXCountLabel, &tickYCountLabel,
                     &xTickStepLabel, &yTickStepLabel
#if defined(DC_HAVE_PER_LANE_PLAYBACK_PARAMS)
                   , &laneSpeedLabel
#endif
                   })
        l->setColour (juce::Label::textColourId, dimText);

    // Editable value labels — styled like small buttons so they read as tappable,
    // with explicit editing colours to prevent the blank-while-editing bug.
    {
        const auto editBg     = light ? juce::Colour (0xfff0ede8) : juce::Colour (0xff32323e);
        const auto editBorder = light ? juce::Colour (0xffc0bab2) : juce::Colour (0xff55556a);
        for (auto* l : { &rangeLabel, &speedLabel, &smoothingLabel })
        {
            l->setColour (juce::Label::backgroundColourId,            editBg);
            l->setColour (juce::Label::outlineColourId,               editBorder);
            l->setColour (juce::Label::textWhenEditingColourId,       textCol);
            l->setColour (juce::Label::backgroundWhenEditingColourId, tbBg);
            l->setColour (juce::Label::outlineWhenEditingColourId,    accent);
        }
    }

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

    // Standalone MIDI out button
    midiOutBtn.setColour (juce::TextButton::buttonColourId,  btnBg);
    midiOutBtn.setColour (juce::TextButton::textColourOffId, btnText);

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

    // Beat preset buttons (sync mode) + Y-axis preset buttons — same palette as density buttons
    {
        const juce::Colour presetBg   = light ? juce::Colour (0xffF0EFE7) : btnBg;
        const juce::Colour presetText = light ? juce::Colour (0xff5B6985) : juce::Colours::lightgrey;
        auto colourPreset = [&] (juce::TextButton& b)
        {
            b.setColour (juce::TextButton::buttonColourId,  presetBg);
            b.setColour (juce::TextButton::textColourOffId, presetText);
        };
        for (auto& b : xStepPresetBtns)    colourPreset (b);
        for (auto& b : syncBeatsPresetBtns) colourPreset (b);
        for (auto& b : yOctavePresetBtns)   colourPreset (b);
        for (auto& b : yNoteStepPresetBtns) colourPreset (b);
        for (auto& b : yDozenPresetBtns)    colourPreset (b);
        for (auto& b : yCCStepPresetBtns)   colourPreset (b);
        for (auto& b : yPBRangePresetBtns)  colourPreset (b);
        for (auto& b : yPBStepPresetBtns)   colourPreset (b);
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

    // Add / delete lane buttons: subdued appearance inside the matrix
    addLaneBtn.setColour (juce::TextButton::buttonColourId, btnBg);
    addLaneBtn.setColour (juce::TextButton::textColourOffId, dimText);
    deleteLaneBtn.setColour (juce::TextButton::buttonColourId, btnBg);
    deleteLaneBtn.setColour (juce::TextButton::textColourOffId, dimText);

    // Eyebrow lane management buttons
    for (auto* b : { &eyebrowAddLaneBtn, &eyebrowDeleteLaneBtn })
    {
        b->setColour (juce::TextButton::buttonColourId,  btnBg);
        b->setColour (juce::TextButton::textColourOffId, dimText);
    }

    // Routing overlay theme
    _routingOverlay.setLightMode (light);

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

    // xQuantize / yQuantize lock buttons — use lane 0's colour when focused on lane 0,
    // otherwise use a neutral accent that reads clearly in both themes.
    const auto lockColour = light ? juce::Colour (0xff706D64) : juce::Colours::lightgrey;
    xQuantizeBtn.setBaseColour (lockColour);
    yQuantizeBtn.setBaseColour (lockColour);
    xQuantizeBtn.repaint();
    yQuantizeBtn.repaint();

    // '#' combo button — neutral style, slightly smaller text
    xyLockBtn.setColour (juce::TextButton::buttonColourId,
        light ? juce::Colour (0xffe0dcd5) : juce::Colour (0xff2a2a38));
    xyLockBtn.setColour (juce::TextButton::buttonOnColourId, lockColour.withAlpha (0.3f));
    xyLockBtn.setColour (juce::TextButton::textColourOffId, lockColour);
    xyLockBtn.setColour (juce::TextButton::textColourOnId,  lockColour.brighter (0.4f));

    _scaleOverlay.setLightMode (_lightMode);

    repaint();
}

//==============================================================================
// Scale overlay
//==============================================================================

void DrawnCurveEditor::openScaleOverlay()
{
    _scaleOverlayOpen = true;
    _scaleOverlay.setLightMode (_lightMode);
    _scaleOverlay.setBounds (getLocalBounds());

    // Reparent scale browser components into the overlay so they paint above the dim.
    // ScaleOverlay has the same bounds / coordinate origin as DrawnCurveEditor, so
    // the setBounds() calls in resized() are valid for children of either.
    for (auto& b : familyBtns)    _scaleOverlay.addAndMakeVisible (b);
    _scaleOverlay.addAndMakeVisible (recentFamilyBtn);
    for (auto& b : subfamilyBtns)  _scaleOverlay.addAndMakeVisible (b);
    _scaleOverlay.addAndMakeVisible (scaleLattice);
    _scaleOverlay.addAndMakeVisible (scaleRotateBtn);
    _scaleOverlay.addAndMakeVisible (scaleAllBtn);
    _scaleOverlay.addAndMakeVisible (scaleNoneBtn);
    _scaleOverlay.addAndMakeVisible (scaleInvBtn);
    _scaleOverlay.addAndMakeVisible (scaleRootBtn);
    _scaleOverlay.addAndMakeVisible (scaleLabel);
    _scaleOverlay.addAndMakeVisible (maskLabel);

    resized();                          // positions components within ScaleOverlay
    _scaleOverlay.setVisible (true);
    _scaleOverlay.toFront (false);
    repaint();
}

void DrawnCurveEditor::closeScaleOverlay()
{
    _scaleOverlayOpen = false;
    _scaleOverlay.setVisible (false);

    // Reparent scale browser components back to DrawnCurveEditor (hidden — visibility
    // is restored by updateScaleVisibility() which also calls resized()).
    for (auto& b : familyBtns)    addChildComponent (b);
    addChildComponent (recentFamilyBtn);
    for (auto& b : subfamilyBtns)  addChildComponent (b);
    addChildComponent (scaleLattice);
    addChildComponent (scaleRotateBtn);
    addChildComponent (scaleAllBtn);
    addChildComponent (scaleNoneBtn);
    addChildComponent (scaleInvBtn);
    addChildComponent (scaleRootBtn);
    addChildComponent (scaleLabel);
    addChildComponent (maskLabel);

    updateScaleVisibility();   // restores correct visibility + repositions via resized()
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

        for (int L = 0; L < proc.activeLaneCount; ++L)
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
                    const juce::Rectangle<float> rangeChip (x, y, chipW, chipH);
                    g.setColour (panelFill);
                    g.fillRoundedRectangle (rangeChip, 11.0f);
                    g.setColour (panelBorder);
                    g.drawRoundedRectangle (rangeChip.reduced (0.5f), 11.0f, 1.0f);
                    g.setColour (eyebrowCol);
                    g.setFont (DrawnCurveLookAndFeel::makeFont (12.0f));
                    g.drawText (rangeSummary, rangeChip.toNearestInt(),
                                juce::Justification::centred, false);
                }
            }
        }
    }

    // ── Scale overlay panel background (drawn on top of stage, below the ScaleOverlay component) ──
    if (_scaleOverlayOpen && _scaleOverlay.isVisible() && ! _scaleOverlay.panelRect.isEmpty())
    {
        const auto& pRect = _scaleOverlay.panelRect;
        const juce::Colour overlayBg     = _lightMode ? juce::Colour (0xfffaf8f6) : juce::Colour (0xff1e1e2c);
        const juce::Colour overlayBorder = _lightMode ? juce::Colour (0x33000000) : juce::Colour (0x33ffffff);
        g.setColour (overlayBg);
        g.fillRoundedRectangle (pRect.toFloat(), 12.0f);
        g.setColour (overlayBorder);
        g.drawRoundedRectangle (pRect.toFloat().reduced (0.5f), 12.0f, 1.0f);
    }

}

//==============================================================================
// resized
//==============================================================================

void DrawnCurveEditor::resized()
{
    using namespace Layout;

    auto area = getLocalBounds().reduced (pad);
    // "singleLane" now means the rail is hidden, regardless of lane count.
    // This drives the layout branch: full-width canvas + eyebrow controls vs. two-column.
    const bool singleLane = !_railExpanded;

    // ── Eyebrow / utility bar ─────────────────────────────────────────────────
    // Single-lane mode: one expanded 52px bar with performance controls + utilities.
    // Multi-lane mode:  36px bar with sync/speed on the left, utilities on the right.
    {
        const int eyebrowH = singleLane ? 52 : utilityRowH;
        auto row = area.removeFromTop (eyebrowH);

        // ── Right side: utility buttons (always present) ─────────────────────
        themeButton     .setBounds (row.removeFromRight (32).withSizeKeepingCentre (32, 28));
        row.removeFromRight (4);
        helpButton      .setBounds (row.removeFromRight (28).withSizeKeepingCentre (28, 28));
        row.removeFromRight (4);
        panicButton     .setBounds (row.removeFromRight (28).withSizeKeepingCentre (28, 28));
        row.removeFromRight (4);
        clearButton     .setBounds (row.removeFromRight (44).withSizeKeepingCentre (44, 28));
        row.removeFromRight (6);
        scaleNotationBtn.setBounds (row.removeFromRight (28).withSizeKeepingCentre (28, 28));
        row.removeFromRight (8);   // visual separator gap

        // Standalone only: MIDI output device picker
        if (juce::JUCEApplicationBase::isStandaloneApp())
        {
            midiOutBtn.setBounds (row.removeFromRight (72).withSizeKeepingCentre (72, 28));
            row.removeFromRight (6);
        }

        if (singleLane)
        {
            // ── Single-lane eyebrow: proportional slider layout ───────────────
            // Layout order (left → right): [dir] [sync] [range+label] [speed] [smooth] [routing]
            // All fixed elements are placed first; remaining space is split proportionally
            // among the three sliders.
            //
            // Responsive breakpoints (based on remaining space after fixed elements):
            //   sliderSpace >= 220px → all 3 sliders visible  (~800px+ window)
            //   sliderSpace >= 100px → range + speed only      (~680px+ window)
            //   sliderSpace <  100px → range only              (< ~680px window)

            // Hide legacy right-rail buttons.
            addLaneBtn.setBounds ({});
            addLaneBtn.setVisible (false);
            deleteLaneBtn.setBounds ({});
            deleteLaneBtn.setVisible (false);

            // ── Fixed: routing button (right edge) ───────────────────────────
            // All-lanes tab: show laneTypeBtn[0] with ✤ text.
            // Individual lane tab: show laneTypeBtn[focusedLane] with its type symbol.
            const int focL = _focusedLane;
            laneTypeBtn[static_cast<size_t>(focL)].setBounds (
                row.removeFromRight (56).withSizeKeepingCentre (56, 28));
            laneTypeBtn[static_cast<size_t>(focL)].setVisible (true);
            // When in all-lanes mode, override label to ✤
            if (_showingAllLanes)
                laneTypeBtn[static_cast<size_t>(focL)].setButtonText (
                    juce::String::fromUTF8 ("\xe2\x9c\xa4"));  // ✤
            // else: text is set by updateLaneRow() to the lane's msg type symbol
            // Hide routing buttons for other lanes
            for (int L = 0; L < kMaxLanes; ++L)
                if (L != focL)
                    laneTypeBtn[static_cast<size_t>(L)].setBounds ({});
            row.removeFromRight (6);

            // ── Fixed: direction + sync (left edge) ───────────────────────────
            dirControl.setBounds (row.removeFromLeft (100).withSizeKeepingCentre (100, 40));
            row.removeFromLeft (8);

            // ── Lane selector — always visible: ✤ + lane numbers ─────────────
            {
                const int nSegs    = proc.activeLaneCount + 1;   // +1 for ✤
                const int selectorW = 28 * nSegs + (nSegs - 1);
                laneFocusCtrl.setBounds (row.removeFromLeft (selectorW)
                                             .withSizeKeepingCentre (selectorW, 26));
                laneFocusCtrl.setVisible (true);
                row.removeFromLeft (4);

                if (proc.activeLaneCount < kMaxLanes)
                {
                    eyebrowAddLaneBtn.setBounds (row.removeFromLeft (24).withSizeKeepingCentre (24, 22));
                    eyebrowAddLaneBtn.setVisible (true);
                    row.removeFromLeft (2);
                }
                else
                {
                    eyebrowAddLaneBtn.setBounds ({});
                    eyebrowAddLaneBtn.setVisible (false);
                }

                if (proc.activeLaneCount > 1)
                {
                    eyebrowDeleteLaneBtn.setBounds (row.removeFromLeft (24).withSizeKeepingCentre (24, 22));
                    eyebrowDeleteLaneBtn.setVisible (true);
                    row.removeFromLeft (6);
                }
                else
                {
                    eyebrowDeleteLaneBtn.setBounds ({});
                    eyebrowDeleteLaneBtn.setVisible (false);
                }
            }

            syncButton .setBounds (row.removeFromLeft (38).withSizeKeepingCentre (38, 28));
            row.removeFromLeft (4);
            laneSyncBtn.setBounds (row.removeFromLeft (32).withSizeKeepingCentre (32, 28));
            row.removeFromLeft (6);

            // ── Proportional slider allocation ────────────────────────────────
            const int sliderSpace = juce::jmax (0, row.getWidth());
            constexpr int kSliderGap = 6;

            const bool showSpeed  = sliderSpace >= 100;
            const bool showSmooth = sliderSpace >= 220;
            const int  numGaps    = showSmooth ? 2 : (showSpeed ? 1 : 0);
            const int  availW     = juce::jmax (0, sliderSpace - numGaps * kSliderGap);

            const int rangeW = showSpeed
                ? juce::roundToInt (availW * (showSmooth ? 0.38f : 0.52f))
                : availW;
            const int speedW = showSpeed
                ? (showSmooth ? juce::roundToInt (availW * 0.37f) : availW - rangeW)
                : 0;
            const int smoothW = showSmooth ? (availW - rangeW - speedW) : 0;

            // Range / Speed / Smooth — label on top, slider below.
            // Labels get a 22 px strip (full width, full height) so the touch
            // target is reliably tappable; slider gets the remaining ~30 px.
            constexpr int kLblH = 22;

            if (rangeW > 0)
            {
                auto rangeSection = row.removeFromLeft (rangeW);
                rangeLabel .setBounds (rangeSection.removeFromTop (kLblH));
                rangeSlider.setBounds (rangeSection.withSizeKeepingCentre (rangeW, 24));
            }
            else
            {
                rangeLabel .setBounds ({});
                rangeSlider.setBounds ({});
            }

            if (showSpeed && speedW > 0)
            {
                row.removeFromLeft (kSliderGap);
                auto speedSection = row.removeFromLeft (speedW);
                speedLabel .setBounds (speedSection.removeFromTop (kLblH));
                speedSlider.setBounds (speedSection.withSizeKeepingCentre (speedW, 24));
            }
            else
            {
                speedLabel .setBounds ({});
                speedSlider.setBounds ({});
            }

            if (showSmooth && smoothW > 0)
            {
                row.removeFromLeft (kSliderGap);
                auto smoothSection = row.removeFromLeft (smoothW);
                smoothingLabel .setBounds (smoothSection.removeFromTop (kLblH));
                smoothingSlider.setBounds (smoothSection.withSizeKeepingCentre (smoothW, 24));
            }
            else
            {
                smoothingLabel .setBounds ({});
                smoothingSlider.setBounds ({});
            }
        }
        else
        {
            // ── Left side (multi-lane): [sync] [lane-sync] [speed label] [speed slider] ──
            syncButton .setBounds (row.removeFromLeft (38).withSizeKeepingCentre (38, 28));
            row.removeFromLeft (4);
            laneSyncBtn.setBounds (row.removeFromLeft (38).withSizeKeepingCentre (38, 28));
            row.removeFromLeft (8);
            speedLabel .setBounds (row.removeFromLeft (36).withSizeKeepingCentre (36, 14));
            speedSlider.setBounds (row.removeFromLeft (180));
        }
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

        if (anyNoteMode && _musicalExpanded && ! _scaleOverlayOpen)
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

    // ── Column layout: single-lane = full width; multi-lane = two columns ────
    juce::Rectangle<int> stageCol;

    if (singleLane)
    {
        // ── Single-lane: no right rail — stage takes the full width ──────────
        stageCol = area;
        _globalPanel      = {};
        _lanesPanel       = {};
        _focusedLanePanel = {};
        _matrixRowOrigin  = {};

        // Hide right-rail-only components.
        // Note: laneFocusCtrl bounds are managed in the eyebrow section above —
        // it may be visible there as a mini lane selector; don't zero it here.
        phaseOffsetLabel .setBounds ({});
        phaseOffsetSlider.setBounds ({});
        oneShotBtn.setBounds ({});
        oneShotBtn.setVisible (false);
        // Hide deleteLaneBtn — only visible inside the matrix rail
        deleteLaneBtn.setBounds ({});
        deleteLaneBtn.setVisible (false);
#if defined(DC_HAVE_PER_LANE_PLAYBACK_PARAMS)
        laneSpeedLabel .setBounds ({});
        laneSpeedSlider.setBounds ({});
#endif

        // Hide inactive lane matrix rows (lanes 1-3 not visible in single-lane mode).
        // Exception: laneTypeBtn[_focusedLane] was already positioned in the eyebrow above;
        // don't zero it out here or the routing button disappears for lanes 2+.
        for (int L = 0; L < kMaxLanes; ++L)
        {
            const bool isEyebrowBtn = (L == _focusedLane);
            laneTypeBtn    [static_cast<size_t>(L)].setVisible (isEyebrowBtn);
            laneDetailLabel[static_cast<size_t>(L)].setVisible (false);
            laneChannelLabel[static_cast<size_t>(L)].setVisible (false);
            laneTeachBtn   [static_cast<size_t>(L)].setVisible (false);
            laneMuteBtn    [static_cast<size_t>(L)].setVisible (false);
            laneSelectBtn  [static_cast<size_t>(L)].setVisible (false);

            if (! isEyebrowBtn)
            {
                laneTypeBtn    [static_cast<size_t>(L)].setBounds ({});
                laneDetailLabel[static_cast<size_t>(L)].setBounds ({});
                laneChannelLabel[static_cast<size_t>(L)].setBounds ({});
                laneTeachBtn   [static_cast<size_t>(L)].setBounds ({});
                laneMuteBtn    [static_cast<size_t>(L)].setBounds ({});
                laneSelectBtn  [static_cast<size_t>(L)].setBounds ({});
            }
        }
    }
    else
    {
        // ── Multi-lane: two-column split ─────────────────────────────────────
        auto rightCol = area.removeFromRight (rightColW);
        area.removeFromRight (colGap);
        stageCol = area;

        // Re-show controls that may have been hidden in single-lane mode
        oneShotBtn.setVisible (true);

        // ══════════════════════════════════════════════════════════════════════
        // RIGHT RAIL
        // ══════════════════════════════════════════════════════════════════════
        _globalPanel = {};
        _lanesPanel  = {};

        _focusedLanePanel = rightCol;
        {
            auto fp = _focusedLanePanel.reduced (panelPad);
            fp.removeFromTop (8);

            // Lane focus selector (1 / 2 / 3)
            laneFocusCtrl.setBounds (fp.removeFromTop (28));
            fp.removeFromTop (6);

            // Direction segmented control (full width)
            dirControl.setBounds (fp.removeFromTop (40));
            fp.removeFromTop (6);

#if defined(DC_HAVE_PER_LANE_PLAYBACK_PARAMS)
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

            // ── Routing matrix ────────────────────────────────────────────────
            fp.removeFromTop (8);

        // Matrix rows — record origin for paint() dot positions.
        // Only lay out active lanes; hide / zero-bound inactive ones.
        _matrixRowOrigin = fp.getTopLeft();
        _matrixRowStride = matRowH + 3;

        for (int L = 0; L < kMaxLanes; ++L)
        {
            const bool active = (L < proc.activeLaneCount);

            laneTypeBtn    [static_cast<size_t>(L)].setVisible (active);
            laneDetailLabel[static_cast<size_t>(L)].setVisible (active);
            laneChannelLabel[static_cast<size_t>(L)].setVisible (active);
            laneTeachBtn   [static_cast<size_t>(L)].setVisible (active);
            laneMuteBtn    [static_cast<size_t>(L)].setVisible (active);
            laneSelectBtn  [static_cast<size_t>(L)].setVisible (active);

            if (active)
            {
                auto row = fp.removeFromTop (matRowH);
                fp.removeFromTop (3);

                // Dot column: space for painted dot + transparent focus button
                auto dotCol = row.removeFromLeft (matDotW + matInnerGap);
                laneSelectBtn[static_cast<size_t> (L)].setBounds (
                    dotCol.removeFromLeft (matDotW).withSizeKeepingCentre (matDotW, matDotW));

                laneTypeBtn    [static_cast<size_t>(L)].setBounds (row.removeFromLeft (matTargetW));
                row.removeFromLeft (matInnerGap);
                laneDetailLabel[static_cast<size_t>(L)].setBounds (row.removeFromLeft (matDetailW));
                row.removeFromLeft (matInnerGap);
                laneChannelLabel[static_cast<size_t>(L)].setBounds (row.removeFromLeft (matChanW));
                row.removeFromLeft (matInnerGap);
                laneTeachBtn   [static_cast<size_t>(L)].setBounds (row.removeFromLeft (matTeachW));
                row.removeFromLeft (matInnerGap);
                laneMuteBtn    [static_cast<size_t>(L)].setBounds (row.removeFromLeft (matMuteW));
            }
            else
            {
                laneTypeBtn    [static_cast<size_t>(L)].setBounds ({});
                laneDetailLabel[static_cast<size_t>(L)].setBounds ({});
                laneChannelLabel[static_cast<size_t>(L)].setBounds ({});
                laneTeachBtn   [static_cast<size_t>(L)].setBounds ({});
                laneMuteBtn    [static_cast<size_t>(L)].setBounds ({});
                laneSelectBtn  [static_cast<size_t>(L)].setBounds ({});
            }
        }

            // ── + / − buttons below the last active row ──────────────────────
            // "+" adds a lane (hidden at max capacity); "−" removes the last lane
            // (hidden when only one lane remains).  Both sit on the same row.
            if (fp.getHeight() >= 24)
            {
                fp.removeFromTop (4);
                auto btnRow = fp.removeFromTop (24);

                if (proc.activeLaneCount < kMaxLanes)
                {
                    addLaneBtn.setBounds (btnRow.removeFromLeft (24));
                    addLaneBtn.setVisible (true);
                    btnRow.removeFromLeft (4);
                }
                else
                {
                    addLaneBtn.setBounds ({});
                    addLaneBtn.setVisible (false);
                }

                if (proc.activeLaneCount > 1)
                {
                    deleteLaneBtn.setBounds (btnRow.removeFromLeft (24));
                    deleteLaneBtn.setVisible (true);
                }
                else
                {
                    deleteLaneBtn.setBounds ({});
                    deleteLaneBtn.setVisible (false);
                }
            }
            else
            {
                addLaneBtn   .setBounds ({});   addLaneBtn   .setVisible (false);
                deleteLaneBtn.setBounds ({});   deleteLaneBtn.setVisible (false);
            }
        }
    }   // end else (multi-lane right rail)

    // ══════════════════════════════════════════════════════════════════════════
    // STAGE COLUMN
    // ══════════════════════════════════════════════════════════════════════════

    _stagePanel = stageCol;
    {
        auto sc = stageCol.reduced (6);   // inner padding

        // laneFocusCtrl is placed in the FOCUSED LANE panel (right rail); no placement here.

        // ── Y-axis column (left edge) ────────────────────────────────────────
        // Bottom zone: [yQ lock] 28px + 4px gap + [# corner] 28px = 60px total.
        // Y-stepper cluster is centred in the remaining space above.
        auto yStepCol = sc.removeFromLeft (yStepperW);
        sc.removeFromLeft (3);
        {
            constexpr int btnH = 22, lblH = 14, stepH = 11;
            constexpr int kStepperH = btnH + lblH + stepH + btnH;   // 69px
            constexpr int kBotZone  = 28 + 4 + xStepperH;           // yQ + gap + #
            constexpr int kPH = 20, kPG = 2, kGG = 6;               // preset btn height, gap, inter-group gap
            const int availTop = yStepCol.getY();
            const int availH   = yStepCol.getHeight() - kBotZone - 8;
            const int xLeft    = yStepCol.getX();

            const int msgType = static_cast<int> (
                proc.apvts.getRawParameterValue (laneParam (_focusedLane, ParamID::msgType))->load());
            const bool isNote = (msgType == 3);
            const bool isCC01 = (msgType == 0 || msgType == 1);
            const bool isPB   = (msgType == 2);

            // Hide all Y preset buttons; re-show the relevant ones below.
            for (auto& b : yOctavePresetBtns)   b.setBounds ({});
            for (auto& b : yNoteStepPresetBtns)  b.setBounds ({});
            for (auto& b : yDozenPresetBtns)     b.setBounds ({});
            for (auto& b : yCCStepPresetBtns)    b.setBounds ({});
            for (auto& b : yPBRangePresetBtns)   b.setBounds ({});
            for (auto& b : yPBStepPresetBtns)    b.setBounds ({});

            const int nRange = isNote ? kNumYOctavePresets
                                      : (isCC01 ? kNumYDozenPresets
                                                : (isPB ? kNumYPBRangePresets : 0));
            const int nStep  = isNote ? kNumYNoteStepPresets
                                      : (isCC01 ? kNumYCCStepPresets
                                                : (isPB ? kNumYPBStepPresets : 0));
            const int rangeGroupH = nRange > 0 ? nRange * kPH + (nRange - 1) * kPG : 0;
            const int stepGroupH  = nStep  > 0 ? nStep  * kPH + (nStep  - 1) * kPG : 0;
            const int totalH = rangeGroupH + (rangeGroupH > 0 ? kGG : 0)
                             + kStepperH
                             + (stepGroupH  > 0 ? kGG : 0) + stepGroupH;

            int y = availTop + (availH - totalH) / 2;

            // ── Range presets (above Y+) ──────────────────────────────────────
            auto placeGroup = [&] (auto& arr, int n)
            {
                for (int i = 0; i < n; ++i)
                {
                    arr[static_cast<size_t>(i)].setBounds (xLeft, y, yStepperW, kPH);
                    y += kPH + kPG;
                }
                if (n > 0) y += kGG - kPG;   // replace last small gap with inter-group gap
            };
            if (isNote)       placeGroup (yOctavePresetBtns,  kNumYOctavePresets);
            else if (isCC01)  placeGroup (yDozenPresetBtns,   kNumYDozenPresets);
            else if (isPB)    placeGroup (yPBRangePresetBtns, kNumYPBRangePresets);

            // ── Stepper cluster ───────────────────────────────────────────────
            tickYPlusBtn   .setBounds (xLeft, y, yStepperW, btnH);  y += btnH;
            tickYCountLabel.setBounds (xLeft, y, yStepperW, lblH);  y += lblH;
            yTickStepLabel .setBounds (xLeft, y, yStepperW, stepH); y += stepH;
            tickYMinusBtn  .setBounds (xLeft, y, yStepperW, btnH);  y += btnH;

            // ── Step presets (below Y-) ───────────────────────────────────────
            if (isNote)      { y += kGG; placeGroup (yNoteStepPresetBtns, kNumYNoteStepPresets); }
            else if (isCC01) { y += kGG; placeGroup (yCCStepPresetBtns,   kNumYCCStepPresets); }
            else if (isPB)   { y += kGG; placeGroup (yPBStepPresetBtns,   kNumYPBStepPresets); }
        }
        // yQ just above the corner #; # at the very bottom (aligned with X row).
        yQuantizeBtn.setBounds (yStepCol.getX(),
                                yStepCol.getBottom() - xStepperH - 4 - 28,
                                yStepperW, 28);
        xyLockBtn   .setBounds (yStepCol.getX(),
                                yStepCol.getBottom() - xStepperH,
                                yStepperW, xStepperH);

        // ── X-axis row (bottom edge) ─────────────────────────────────────────
        // Layout: [xQ] gap … [X− count X+ centred] …
        // xQ is the right half of the ⌞ corner (# is at yStepCol bottom, xQ is
        // immediately to the right — together they form ⌞).
        sc.removeFromBottom (3);
        {
            auto row = sc.removeFromBottom (xStepperH);
            constexpr int kCountW = 24, kStepLblW = 42;

            // xQ at the leftmost position in the row (flush against the # corner)
            xQuantizeBtn.setBounds (row.removeFromLeft (28));

            const bool isSync = proc.apvts.getRawParameterValue (ParamID::syncEnabled)->load() > 0.5f;
            if (isSync)
            {
                // ── Sync mode: entire cluster centred in the space after xQ ────
                constexpr int kPW  = 32, kPG  = 3;   // preset button width + gap
                constexpr int kDW  = 28, kDG  = 3;   // duration button width + gap
                constexpr int kGG  = 6;               // gap between groups
                constexpr int kStepGroupW = kNumXStepPresets    * kPW + (kNumXStepPresets    - 1) * kPG;
                constexpr int kDurGroupW  = kNumSyncBeatsPresets * kDW + (kNumSyncBeatsPresets - 1) * kDG;
                constexpr int kStepperW   = 28 + 2 + kCountW + 2 + kStepLblW + 2 + 28;
                constexpr int kTotalW     = kStepGroupW + kGG + kStepperW + kGG + kDurGroupW;

                const int cx = row.getX() + (row.getWidth() - kTotalW) / 2;
                int x = cx;

                for (auto& b : xStepPresetBtns)
                {
                    b.setBounds (x, row.getY(), kPW, row.getHeight());
                    x += kPW + kPG;
                }
                x += kGG - kPG;   // replace last gap with inter-group gap

                tickXMinusBtn  .setBounds (x,                          row.getY(), 28,         row.getHeight());  x += 28 + 2;
                tickXCountLabel.setBounds (x,                          row.getY(), kCountW,    row.getHeight());  x += kCountW + 2;
                xTickStepLabel .setBounds (x,                          row.getY(), kStepLblW,  row.getHeight());  x += kStepLblW + 2;
                tickXPlusBtn   .setBounds (x,                          row.getY(), 28,         row.getHeight());  x += 28 + kGG;

                for (auto& b : syncBeatsPresetBtns)
                {
                    b.setBounds (x, row.getY(), kDW, row.getHeight());
                    x += kDW + kDG;
                }
            }
            else
            {
                // ── Free mode: hide presets, centre the stepper cluster ───────
                for (auto& b : xStepPresetBtns)     b.setBounds ({});
                for (auto& b : syncBeatsPresetBtns)  b.setBounds ({});

                row.removeFromLeft (8);
                constexpr int kStepW = 28 + 2 + kCountW + 2 + kStepLblW + 2 + 28;
                const int xStart = row.getX() + (row.getWidth() - kStepW) / 2;
                tickXMinusBtn  .setBounds (xStart,                                       row.getY(), 28,         row.getHeight());
                tickXCountLabel.setBounds (xStart + 30,                                  row.getY(), kCountW,    row.getHeight());
                xTickStepLabel .setBounds (xStart + 30 + kCountW + 2,                   row.getY(), kStepLblW,  row.getHeight());
                tickXPlusBtn   .setBounds (xStart + 30 + kCountW + 2 + kStepLblW + 2,  row.getY(), 28,         row.getHeight());
            }
        }

        // Curve display fills remaining stage area
        curveDisplay.setBounds (sc);
    }

    // ── Scale browser overlay panel ────────────────────────────────────────────
    if (_scaleOverlayOpen && ! _musicalPanel.isEmpty())
    {
        using namespace Layout;
        constexpr int kOvW   = 340;
        constexpr int kOvPad = 8;
        constexpr int kOvH   = kOvPad + kFamilyBarH + 4 + kSubfamilyRowH + 4 + kActionRowH + 4 + kScaleLatticeH + kOvPad;
        const int kOvX = getWidth() - kOvW - 10;
        const int kOvY = (curveDisplay.isVisible()
                          ? curveDisplay.getBoundsInParent().getY() + 8 : 60);
        const juce::Rectangle<int> ovPanel (kOvX, kOvY, kOvW, kOvH);

        _scaleOverlay.panelRect = ovPanel;
        _scaleOverlay.setBounds (getLocalBounds());

        // Make scale components visible — they may have been hidden while the zone was collapsed.
        // NOTE: do NOT call setActiveFamily() here — it calls resized() and would recurse.
        // Restore subfamily chip visibility directly instead.
        for (auto& b : familyBtns) b.setVisible (true);
        recentFamilyBtn.setVisible (true);
        scaleLattice   .setVisible (true);
        scaleRotateBtn .setVisible (true);
        scaleAllBtn    .setVisible (true);
        scaleNoneBtn   .setVisible (true);
        scaleInvBtn    .setVisible (true);
        scaleRootBtn   .setVisible (true);
        scaleLabel     .setVisible (true);
        maskLabel      .setVisible (true);
        // Restore per-chip visibility without calling setActiveFamily (avoids re-entrant resized()).
        for (int i = 0; i < kMaxModes; ++i)
            subfamilyBtns[static_cast<size_t>(i)].setVisible (i < _numSubfamilyChips);

        auto ne = ovPanel.reduced (kOvPad, kOvPad);

        // Family tab bar
        {
            auto fRow = ne.removeFromTop (kFamilyBarH);
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

        // Subfamily chips
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

        // Action row
        {
            auto aRow = ne.removeFromTop (kActionRowH);
            maskLabel .setBounds (aRow.removeFromRight (52).withSizeKeepingCentre (52, 20));
            aRow.removeFromRight (3);
            scaleLabel.setBounds (aRow.removeFromRight (84).withSizeKeepingCentre (84, 14));
            aRow.removeFromRight (8);
            scaleRotateBtn.setBounds (aRow.removeFromLeft (28)); aRow.removeFromLeft (6);
            scaleAllBtn   .setBounds (aRow.removeFromLeft (28)); aRow.removeFromLeft (2);
            scaleNoneBtn  .setBounds (aRow.removeFromLeft (28)); aRow.removeFromLeft (2);
            scaleInvBtn   .setBounds (aRow.removeFromLeft (28)); aRow.removeFromLeft (2);
            scaleRootBtn  .setBounds (aRow.removeFromLeft (28));
        }
        ne.removeFromTop (4);

        // Lattice
        scaleLattice.setBounds (ne.removeFromTop (kScaleLatticeH));
    }

    // ── Full-editor overlays ──────────────────────────────────────────────────
    helpOverlay.setBounds (getLocalBounds());
    // Routing overlay: if visible, reposition the panel without recreating attachments.
    // anchor = the routing button for the currently focused lane (valid in both
    // single-lane and all-lanes mode since focL is always a valid lane index).
    if (_routingOverlay.isVisible())
    {
        const int anchorL = juce::jlimit (0, kMaxLanes - 1, _focusedLane);
        _routingOverlay.reanchor (laneTypeBtn[static_cast<size_t> (anchorL)].getBoundsInParent());
    }
    else
    {
        _routingOverlay.setBounds (getLocalBounds());
    }
}
