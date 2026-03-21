/**
 * @file PluginProcessor.cpp
 *
 * Implementation of DrawnCurveProcessor.
 * See PluginProcessor.h for the architecture overview.
 */

#include "PluginProcessor.h"
#include "PluginEditor.h"

// Helper: create a 2-byte MIDI message for channel pressure; 3-byte for everything else.
static juce::MidiMessage makeMidiMessage (uint8_t status, uint8_t d1, uint8_t d2)
{
    if ((status & 0xF0u) == 0xD0u)
        return juce::MidiMessage (status, d1);
    return juce::MidiMessage (status, d1, d2);
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout DrawnCurveProcessor::createParams()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // ── Shared / global parameters ────────────────────────────────────────────
    layout.add (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamID::playbackSpeed, 1 }, "Playback Speed",
        juce::NormalisableRange<float> (0.25f, 4.0f, 0.01f, 0.5f), 1.0f));

    layout.add (std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { ParamID::syncEnabled, 1 }, "Sync to Host", false));

    layout.add (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamID::syncBeats, 1 }, "Sync Beats",
        juce::NormalisableRange<float> (1.0f, 32.0f, 1.0f), 4.0f));

    layout.add (std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { ParamID::playbackDirection, 1 }, "Playback Direction",
        juce::StringArray { "Forward", "Reverse", "Ping-Pong" }, 0));

    // ── Per-lane parameters ───────────────────────────────────────────────────
    static const juce::StringArray kMsgTypeChoices { "CC", "Channel Pressure", "Pitch Bend", "Note" };

    // Lane-specific defaults to give each lane a distinct starting CC.
    static const int kDefaultCC[kMaxLanes]  = { 74, 1, 11 };
    static const int kDefaultCh[kMaxLanes]  = { 1, 1, 1   };

    for (int L = 0; L < kMaxLanes; ++L)
    {
        const juce::String lname = "L" + juce::String (L + 1) + " ";

        layout.add (std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID { laneParam (L, ParamID::laneEnabled), 1 },
            lname + "Enabled", true));

        layout.add (std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID { laneParam (L, ParamID::msgType), 1 },
            lname + "Message Type", kMsgTypeChoices, 0));

        layout.add (std::make_unique<juce::AudioParameterInt>(
            juce::ParameterID { laneParam (L, ParamID::ccNumber), 1 },
            lname + "CC Number", 0, 127, kDefaultCC[L]));

        layout.add (std::make_unique<juce::AudioParameterInt>(
            juce::ParameterID { laneParam (L, ParamID::midiChannel), 1 },
            lname + "MIDI Channel", 1, 16, kDefaultCh[L]));

        layout.add (std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { laneParam (L, ParamID::smoothing), 1 },
            lname + "Smoothing",
            juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.08f));

        layout.add (std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { laneParam (L, ParamID::minOutput), 1 },
            lname + "Min Output",
            juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f));

        layout.add (std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { laneParam (L, ParamID::maxOutput), 1 },
            lname + "Max Output",
            juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f));

        layout.add (std::make_unique<juce::AudioParameterInt>(
            juce::ParameterID { laneParam (L, ParamID::noteVelocity), 1 },
            lname + "Note Velocity", 1, 127, 100));

        layout.add (std::make_unique<juce::AudioParameterInt>(
            juce::ParameterID { laneParam (L, ParamID::scaleMode), 1 },
            lname + "Scale Mode", 0, 7, 0));

        layout.add (std::make_unique<juce::AudioParameterInt>(
            juce::ParameterID { laneParam (L, ParamID::scaleRoot), 1 },
            lname + "Scale Root", 0, 11, 0));

        layout.add (std::make_unique<juce::AudioParameterInt>(
            juce::ParameterID { laneParam (L, ParamID::scaleMask), 1 },
            lname + "Scale Custom Mask", 0, 4095, 4095));
    }

    return layout;
}

//==============================================================================
DrawnCurveProcessor::DrawnCurveProcessor()
    : AudioProcessor (BusesProperties()),
      apvts (*this, nullptr, "DrawnCurve", createParams())
{
    _laneSnaps.fill (nullptr);
    updateAllLaneScales();
}

DrawnCurveProcessor::~DrawnCurveProcessor()
{
    stopTimer();
}

//==============================================================================
void DrawnCurveProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    _timerSampleRate = sampleRate;
    _engine.reset();
    _hostWasPlaying.store (false, std::memory_order_relaxed);
}

void DrawnCurveProcessor::releaseResources()
{
    _engine.setPlaying (false);
}

//==============================================================================
void DrawnCurveProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                         juce::MidiBuffer&         midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused (buffer);

    _lastProcessBlockMs.store (juce::Time::currentTimeMillis(), std::memory_order_relaxed);

    // ── Teach / Learn: scan incoming MIDI for CC messages ────────────────────
    {
        const int pending = _teachPendingLane.load (std::memory_order_relaxed);
        if (pending >= 0)
        {
            for (const auto meta : midiMessages)
            {
                const auto msg = meta.getMessage();
                if (msg.isController())
                {
                    // Apply CC# to the pending lane's parameter.
                    // Note: writing an APVTS parameter from the audio thread is technically
                    // discouraged, but JUCE AudioParameterInt uses an atomic store internally
                    // and this is widely practiced for learn workflows.
                    const juce::String pid = laneParam (pending, ParamID::ccNumber);
                    if (auto* p = dynamic_cast<juce::AudioParameterInt*> (apvts.getParameter (pid)))
                        *p = msg.getControllerNumber();
                    _teachPendingLane.store (-1, std::memory_order_relaxed);
                    break;
                }
            }
        }
    }

    midiMessages.clear();

    // ── Flush MIDI buffered by the fallback timer ─────────────────────────────
    {
        juce::SpinLock::ScopedLockType lock (_pendingMidiLock);
        if (! _pendingMidi.isEmpty())
        {
            midiMessages.addEvents (_pendingMidi, 0, -1, 0);
            _pendingMidi.clear();
        }
    }

    // ── Compute effective speed + handle host transport sync ──────────────────
    const bool syncOn = apvts.getRawParameterValue (ParamID::syncEnabled)->load() > 0.5f;
    float effectiveSpeed = apvts.getRawParameterValue (ParamID::playbackSpeed)->load();

    if (syncOn)
    {
        if (auto* ph = getPlayHead())
        {
            if (auto pos = ph->getPosition())
            {
                const bool hostNowPlaying = pos->getIsPlaying();
                const bool wasPlaying     = _hostWasPlaying.exchange (hostNowPlaying,
                                                                       std::memory_order_acq_rel);
                if (hostNowPlaying && ! wasPlaying)
                {
                    juce::SpinLock::ScopedLockType lock (_engineLock);
                    _engine.reset();
                    _engine.setPlaying (true);
                }
                else if (! hostNowPlaying && wasPlaying)
                {
                    _engine.setPlaying (false);
                }

                if (auto bpmOpt = pos->getBpm())
                {
                    const float bpm        = static_cast<float> (*bpmOpt);
                    const float syncBeats  = apvts.getRawParameterValue (ParamID::syncBeats)->load();
                    const float recordedDur = curveDuration (0);  // use lane 0 as reference
                    if (bpm > 0.0f && syncBeats > 0.0f && recordedDur > 0.0f)
                        effectiveSpeed = recordedDur / (syncBeats * 60.0f / bpm);
                }
            }
        }
    }

    _effectiveSpeedRatio.store (effectiveSpeed, std::memory_order_relaxed);

    // ── Advance engine on the audio thread ────────────────────────────────────
    const auto dir = static_cast<PlaybackDirection> (
        static_cast<int> (apvts.getRawParameterValue (ParamID::playbackDirection)->load()));

    {
        juce::SpinLock::ScopedLockType lock (_engineLock);
        _engine.processBlock (
            static_cast<uint32_t> (buffer.getNumSamples()),
            getSampleRate(),
            [&midiMessages] (uint8_t status, uint8_t d1, uint8_t d2)
            {
                midiMessages.addEvent (makeMidiMessage (status, d1, d2), 0);
            },
            effectiveSpeed,
            dir);
    }
}

//==============================================================================
void DrawnCurveProcessor::hiResTimerCallback()
{
    if (! isPlaying() || ! anyLaneHasCurve()) return;

    const int64_t now    = juce::Time::currentTimeMillis();
    const int64_t lastAT = _lastProcessBlockMs.load (std::memory_order_relaxed);
    if (now - lastAT < kAudioThreadTimeoutMs) return;

    const auto nominalFrames =
        static_cast<uint32_t> (_timerSampleRate * kTimerIntervalMs / 1000.0);

    const float speed = _effectiveSpeedRatio.load (std::memory_order_relaxed);
    const auto  dir   = static_cast<PlaybackDirection> (
        static_cast<int> (apvts.getRawParameterValue (ParamID::playbackDirection)->load()));

    juce::MidiBuffer localBuf;
    {
        juce::SpinLock::ScopedTryLockType tryLock (_engineLock);
        if (! tryLock.isLocked()) return;

        _engine.processBlock (
            nominalFrames,
            _timerSampleRate,
            [&localBuf] (uint8_t status, uint8_t d1, uint8_t d2)
            {
                localBuf.addEvent (makeMidiMessage (status, d1, d2), 0);
            },
            speed,
            dir);
    }

    if (! localBuf.isEmpty())
    {
        juce::SpinLock::ScopedLockType lock (_pendingMidiLock);
        for (const auto meta : localBuf)
            _pendingMidi.addEvent (meta.getMessage(), meta.samplePosition);
    }
}

//==============================================================================
// Capture API
//==============================================================================

void DrawnCurveProcessor::beginCapture (int /*lane*/)  { _capture.begin(); }

void DrawnCurveProcessor::addCapturePoint (double t, float x, float y, float pressure)
{
    _capture.addPoint (t, x, y, pressure);
}

void DrawnCurveProcessor::finalizeCapture (int lane)
{
    if (! _capture.hasPoints()) return;

    const uint8_t ccNum  = static_cast<uint8_t> (
        static_cast<int> (apvts.getRawParameterValue (laneParam (lane, ParamID::ccNumber))->load()));
    const uint8_t ch     = static_cast<uint8_t> (
        static_cast<int> (apvts.getRawParameterValue (laneParam (lane, ParamID::midiChannel))->load()) - 1);
    const float smooth   = apvts.getRawParameterValue (laneParam (lane, ParamID::smoothing))->load();
    const float minOut   = apvts.getRawParameterValue (laneParam (lane, ParamID::minOutput))->load();
    const float maxOut   = apvts.getRawParameterValue (laneParam (lane, ParamID::maxOutput))->load();
    const auto  msgType  = static_cast<MessageType> (
        static_cast<int> (apvts.getRawParameterValue (laneParam (lane, ParamID::msgType))->load()));
    const uint8_t noteVel = static_cast<uint8_t> (
        static_cast<int> (apvts.getRawParameterValue (laneParam (lane, ParamID::noteVelocity))->load()));

    auto* snap = new LaneSnapshot (_capture.finalize (ccNum, ch, minOut, maxOut, smooth, msgType));
    snap->noteVelocity = noteVel;
    _laneSnaps[lane] = snap;

    {
        juce::SpinLock::ScopedLockType lock (_engineLock);
        _engine.setSnapshot (lane, snap);
        _engine.reset();
    }
}

void DrawnCurveProcessor::clearSnapshot (int lane)
{
    {
        juce::SpinLock::ScopedLockType lock (_engineLock);
        _engine.clearSnapshot (lane);
        if (lane == 0) _capture.clear();   // capture session belongs to whichever lane is being drawn
    }
    _laneSnaps[lane] = nullptr;
}

void DrawnCurveProcessor::clearAllSnapshots()
{
    {
        juce::SpinLock::ScopedLockType lock (_engineLock);
        _engine.clearAllSnapshots();
        _capture.clear();
    }
    _laneSnaps.fill (nullptr);
}

//==============================================================================
// Playback
//==============================================================================

void DrawnCurveProcessor::setPlaying (bool on)
{
    _engine.setPlaying (on);
    if (on) startTimer (kTimerIntervalMs);
    else    stopTimer();
}

bool DrawnCurveProcessor::isPlaying() const noexcept { return _engine.getPlaying(); }

//==============================================================================
// Query API
//==============================================================================

bool DrawnCurveProcessor::hasCurve (int lane) const noexcept
{
    if (lane < 0 || lane >= kMaxLanes) return false;
    return _laneSnaps[lane] != nullptr && _laneSnaps[lane]->valid;
}

bool DrawnCurveProcessor::anyLaneHasCurve() const noexcept
{
    for (int i = 0; i < kMaxLanes; ++i)
        if (hasCurve (i)) return true;
    return false;
}

float DrawnCurveProcessor::currentPhase() const noexcept { return _engine.getCurrentPhase(); }

std::array<float, 256> DrawnCurveProcessor::getCurveTable (int lane) const noexcept
{
    if (lane >= 0 && lane < kMaxLanes && _laneSnaps[lane] && _laneSnaps[lane]->valid)
        return _laneSnaps[lane]->table;
    return {};
}

float DrawnCurveProcessor::curveDuration (int lane) const noexcept
{
    if (lane >= 0 && lane < kMaxLanes && _laneSnaps[lane] && _laneSnaps[lane]->valid)
        return _laneSnaps[lane]->durationSeconds;
    return 0.0f;
}

//==============================================================================
// Scale quantization
//==============================================================================

static constexpr uint16_t kScalePresetMasks[8] =
{
    0xFFF,   // 0 Chromatic
    0xAB5,   // 1 Major
    0x5AD,   // 2 Natural Minor
    0x6AD,   // 3 Dorian
    0x295,   // 4 Pentatonic Major
    0x4A9,   // 5 Pentatonic Minor
    0x4E9,   // 6 Blues
    0x000,   // 7 Custom
};

ScaleConfig DrawnCurveProcessor::getScaleConfig (int lane) const noexcept
{
    const int     mode = static_cast<int> (
        apvts.getRawParameterValue (laneParam (lane, ParamID::scaleMode))->load());
    const uint8_t root = static_cast<uint8_t> (
        apvts.getRawParameterValue (laneParam (lane, ParamID::scaleRoot))->load());

    uint16_t mask;
    if (mode == 7)
        mask = static_cast<uint16_t> (
            apvts.getRawParameterValue (laneParam (lane, ParamID::scaleMask))->load());
    else
        mask = kScalePresetMasks[std::clamp (mode, 0, 7)];

    return { mask, root };
}

void DrawnCurveProcessor::updateEngineScale (int lane)
{
    _engine.setScaleConfig (lane, getScaleConfig (lane));
}

void DrawnCurveProcessor::updateAllLaneScales()
{
    for (int i = 0; i < kMaxLanes; ++i)
        updateEngineScale (i);
}

//==============================================================================
// Teach / Learn
//==============================================================================

void DrawnCurveProcessor::beginTeach (int lane)
{
    _teachPendingLane.store (lane, std::memory_order_relaxed);
}

void DrawnCurveProcessor::cancelTeach()
{
    _teachPendingLane.store (-1, std::memory_order_relaxed);
}

bool DrawnCurveProcessor::isTeachPending (int lane) const noexcept
{
    return _teachPendingLane.load (std::memory_order_relaxed) == lane;
}

//==============================================================================
// State persistence
//==============================================================================

void DrawnCurveProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();

    for (int L = 0; L < kMaxLanes; ++L)
    {
        const auto* snap = _laneSnaps[L];
        if (snap && snap->valid)
        {
            const juce::String pfx = "L" + juce::String (L) + "_";
            juce::MemoryBlock tableBlock (snap->table.data(), 256 * sizeof (float));
            state.setProperty (pfx + "tableData", tableBlock.toBase64Encoding(), nullptr);
            state.setProperty (pfx + "duration",  snap->durationSeconds,         nullptr);
            state.setProperty (pfx + "ccNum",     static_cast<int> (snap->ccNumber),    nullptr);
            state.setProperty (pfx + "midiCh",    static_cast<int> (snap->midiChannel), nullptr);
            state.setProperty (pfx + "minOut",    snap->minOut,                   nullptr);
            state.setProperty (pfx + "maxOut",    snap->maxOut,                   nullptr);
            state.setProperty (pfx + "smooth",    snap->smoothing,                nullptr);
            state.setProperty (pfx + "msgType",   static_cast<int> (snap->messageType),   nullptr);
            state.setProperty (pfx + "noteVel",   static_cast<int> (snap->noteVelocity),  nullptr);
        }
    }

    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void DrawnCurveProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (! xml) return;

    auto state = juce::ValueTree::fromXml (*xml);
    apvts.replaceState (state);
    updateAllLaneScales();

    for (int L = 0; L < kMaxLanes; ++L)
    {
        const juce::String pfx = "L" + juce::String (L) + "_";
        const juce::String tableB64 = state.getProperty (pfx + "tableData", juce::String());
        if (tableB64.isNotEmpty())
        {
            juce::MemoryBlock tableBlock;
            if (tableBlock.fromBase64Encoding (tableB64) &&
                tableBlock.getSize() == 256 * sizeof (float))
            {
                auto* snap = new LaneSnapshot();
                memcpy (snap->table.data(), tableBlock.getData(), 256 * sizeof (float));
                snap->durationSeconds = static_cast<float> (static_cast<double> (
                    state.getProperty (pfx + "duration", 1.0)));
                snap->ccNumber    = static_cast<uint8_t>  (static_cast<int> (
                    state.getProperty (pfx + "ccNum",   74)));
                snap->midiChannel = static_cast<uint8_t>  (static_cast<int> (
                    state.getProperty (pfx + "midiCh",  0)));
                snap->minOut      = static_cast<float>    (static_cast<double> (
                    state.getProperty (pfx + "minOut",   0.0)));
                snap->maxOut      = static_cast<float>    (static_cast<double> (
                    state.getProperty (pfx + "maxOut",   1.0)));
                snap->smoothing   = static_cast<float>    (static_cast<double> (
                    state.getProperty (pfx + "smooth",  0.08)));
                snap->messageType  = static_cast<MessageType> (static_cast<int> (
                    state.getProperty (pfx + "msgType", 0)));
                snap->noteVelocity = static_cast<uint8_t> (static_cast<int> (
                    state.getProperty (pfx + "noteVel", 100)));
                snap->valid = true;

                _laneSnaps[L] = snap;
                {
                    juce::SpinLock::ScopedLockType lock (_engineLock);
                    _engine.setSnapshot (L, snap);
                }
            }
        }
    }

    // Backward compat: single-lane v1 presets stored table under "tableData" (no prefix).
    // If lane 0 has no curve from the loop above, try the old keys.
    if (! hasCurve (0))
    {
        const juce::String tableB64 = state.getProperty ("tableData", juce::String());
        if (tableB64.isNotEmpty())
        {
            juce::MemoryBlock tableBlock;
            if (tableBlock.fromBase64Encoding (tableB64) &&
                tableBlock.getSize() == 256 * sizeof (float))
            {
                auto* snap = new LaneSnapshot();
                memcpy (snap->table.data(), tableBlock.getData(), 256 * sizeof (float));
                snap->durationSeconds = static_cast<float> (static_cast<double> (
                    state.getProperty ("duration", 1.0)));
                snap->ccNumber    = static_cast<uint8_t>  (static_cast<int> (state.getProperty ("ccNum",   74)));
                snap->midiChannel = static_cast<uint8_t>  (static_cast<int> (state.getProperty ("midiCh",  0)));
                snap->minOut      = static_cast<float>    (static_cast<double> (state.getProperty ("minOut",  0.0)));
                snap->maxOut      = static_cast<float>    (static_cast<double> (state.getProperty ("maxOut",  1.0)));
                snap->smoothing   = static_cast<float>    (static_cast<double> (state.getProperty ("smooth",  0.08)));
                snap->messageType  = static_cast<MessageType> (static_cast<int> (state.getProperty ("msgType", 0)));
                snap->noteVelocity = static_cast<uint8_t> (static_cast<int> (state.getProperty ("noteVel", 100)));
                snap->valid = true;

                _laneSnaps[0] = snap;
                {
                    juce::SpinLock::ScopedLockType lock (_engineLock);
                    _engine.setSnapshot (0, snap);
                    _engine.reset();
                }
            }
        }
    }
}

//==============================================================================
juce::AudioProcessorEditor* DrawnCurveProcessor::createEditor()
{
    return new DrawnCurveEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DrawnCurveProcessor();
}
