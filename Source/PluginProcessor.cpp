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
            lname + "Message Type", kMsgTypeChoices, L == 0 ? 3 : 0));

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

    }

    // ── Global scale quantization (shared by all Note-mode lanes) ─────────────
    layout.add (std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { ParamID::scaleMode, 1 },
        "Scale Mode", 0, 7, 0));

    layout.add (std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { ParamID::scaleRoot, 1 },
        "Scale Root", 0, 11, 0));

    layout.add (std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { ParamID::scaleMask, 1 },
        "Scale Custom Mask", 0, 4095, 4095));

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
    // Signal a MIDI panic before the plugin is torn down.  The engine's
    // setPlaying(false) also sets _noteOffNeeded on all lanes; the panic
    // additionally sweeps all 128 notes so nothing stays held in the host.
    _panicNeeded.store (true, std::memory_order_release);
    _engine.setPlaying (false);
}

//==============================================================================
void DrawnCurveProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                         juce::MidiBuffer&         midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused (buffer);

    _lastProcessBlockMs.store (juce::Time::currentTimeMillis(), std::memory_order_relaxed);

    // ── Teach: scan incoming MIDI for a message matching the lane's type ─────
    // For CC lanes, captures the incoming CC number and stores it, then ends.
    // For all other types (Note, PB, Aft), the lane is isolated (solo) until
    // the user manually dismisses Teach by tapping the button again — no
    // incoming-message capture is performed.
    {
        const int pending = _teachPendingLane.load (std::memory_order_relaxed);
        if (pending >= 0)
        {
            const int msgType = static_cast<int> (
                apvts.getRawParameterValue (laneParam (pending, ParamID::msgType))->load());
            if (msgType == 0)   // CC lane: capture the first incoming CC number
            {
                for (const auto meta : midiMessages)
                {
                    const auto msg = meta.getMessage();
                    if (msg.isController())
                    {
                        // Writing APVTS param from the audio thread: JUCE AudioParameterInt
                        // uses an atomic store, so this is safe and widely used for learn.
                        const juce::String pid = laneParam (pending, ParamID::ccNumber);
                        if (auto* p = dynamic_cast<juce::AudioParameterInt*> (apvts.getParameter (pid)))
                            *p = msg.getControllerNumber();
                        _teachPendingLane.store (-1, std::memory_order_relaxed);
                        break;
                    }
                }
            }
            // For Note / PB / Aft lanes: no auto-capture; user taps Teach again to exit.
        }
    }

    midiMessages.clear();

    // ── MIDI Panic: All Notes Off + brute-force Note Off sweep ───────────────
    if (_panicNeeded.exchange (false, std::memory_order_acq_rel))
    {
        for (int ch = 0; ch < 16; ++ch)
        {
            // CC 123 = All Notes Off
            midiMessages.addEvent (juce::MidiMessage::controllerEvent (ch + 1, 123, 0), 0);
            // Belt-and-suspenders: explicit Note Off for every pitch
            for (int note = 0; note < 128; ++note)
                midiMessages.addEvent (juce::MidiMessage::noteOff (ch + 1, note, static_cast<uint8_t> (0)), 0);
        }
        // Reset all lane dedup state so next curve resumes cleanly
        _engine.reset();
    }

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
                    // Host just started — clear the user-pause latch so the
                    // plugin resumes with the transport (pressing Play in the
                    // DAW is an explicit intent to restart).
                    _userManualPauseInSync.store (false, std::memory_order_release);
                    juce::SpinLock::ScopedLockType lock (_engineLock);
                    _engine.reset();
                    _engine.setPlaying (true);
                }
                else if (! hostNowPlaying && wasPlaying)
                {
                    _engine.setPlaying (false);
                }
                else if (hostNowPlaying && wasPlaying
                         && ! _engine.getPlaying()
                         && ! _userManualPauseInSync.load (std::memory_order_acquire))
                {
                    // Engine was paused by something other than the user (e.g. audio
                    // session bounce re-set _hostWasPlaying).  Restart to stay in sync.
                    juce::SpinLock::ScopedLockType lock (_engineLock);
                    _engine.setPlaying (true);
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

    // ── Push per-lane mute state to engine (detects enabled→disabled transition) ─
    // When a Teach session is active, that lane is isolated: all other lanes
    // are suppressed regardless of their own enabled parameter.
    {
        const int soloLane = _teachPendingLane.load (std::memory_order_relaxed);
        for (int L = 0; L < kMaxLanes; ++L)
        {
            const bool paramEnabled =
                apvts.getRawParameterValue (laneParam (L, ParamID::laneEnabled))->load() > 0.5f;
            const bool soloPass = (soloLane < 0) || (L == soloLane);
            _engine.setLaneEnabled (L, paramEnabled && soloPass);
        }
    }

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

void DrawnCurveProcessor::beginCapture (int lane)
{
    // Signal the engine to send Note Off for this lane before drawing overwrites it.
    // Other lanes continue playing uninterrupted.
    _engine.stopLane (lane);
    _capture.begin();
}

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
        _engine.resetLane (lane);   // rewind only this lane; Note Off from stopLane still fires
    }
}

void DrawnCurveProcessor::updateLaneSnapshot (int lane)
{
    if (lane < 0 || lane >= kMaxLanes) return;
    const auto* existing = _laneSnaps[lane];
    if (! existing || ! existing->valid) return;   // nothing drawn yet — nothing to update

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

    // Clone the existing snapshot, then overwrite only the param-driven fields.
    auto* snap = new LaneSnapshot (*existing);
    snap->ccNumber       = ccNum;
    snap->midiChannel    = ch;
    snap->smoothing      = smooth;
    snap->minOut         = minOut;
    snap->maxOut         = maxOut;
    snap->messageType    = msgType;
    snap->noteVelocity   = noteVel;

    _laneSnaps[lane] = snap;

    // When switching TO Note mode, send Note Off for any CC/PB value in flight.
    // When switching FROM Note mode, the engine will naturally stop sending Note-Ons.
    if (msgType != existing->messageType)
        _engine.stopLane (lane);

    {
        juce::SpinLock::ScopedLockType lock (_engineLock);
        _engine.setSnapshot (lane, snap);
    }
    // Note: do NOT delete existing — it may still be in use by the audio thread until
    // the next processBlock acquires the new pointer.  Small intentional leak per update;
    // finalizeCapture() already has the same pattern (_laneSnaps overwrite without delete).
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
// MIDI Panic
//==============================================================================

void DrawnCurveProcessor::sendPanic()
{
    _panicNeeded.store (true, std::memory_order_release);
}

//==============================================================================
// Playback
//==============================================================================

void DrawnCurveProcessor::setPlaying (bool on)
{
    _engine.setPlaying (on);
    if (on)
    {
        _userManualPauseInSync.store (false, std::memory_order_release);
        startTimer (kTimerIntervalMs);
    }
    else
    {
        // Record that the user explicitly paused so sync code won't override it.
        const bool isSync = apvts.getRawParameterValue (ParamID::syncEnabled)->load() > 0.5f;
        if (isSync)
            _userManualPauseInSync.store (true, std::memory_order_release);
        stopTimer();
    }
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

float DrawnCurveProcessor::currentPhase()            const noexcept { return _engine.getCurrentPhase(); }
float DrawnCurveProcessor::currentPhaseForLane (int l) const noexcept { return _engine.getCurrentPhaseForLane (l); }

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

// Bitmask convention: bit (11 - interval) = interval present in scale.
// Bit 11 = root (interval 0 = C when root=C), bit 0 = major-7th (interval 11 = B).
// Example: 0xFFF = 0b111111111111 = all 12 notes (Chromatic).
//          0xAD5 = 0b101011010101 = C D E F G A B (Major from C).
static constexpr uint16_t kScalePresetMasks[8] =
{
    0xFFF,   // 0 Chromatic       — all 12 intervals
    0xAD5,   // 1 Major           — 0 2 4 5 7 9 11
    0xB5A,   // 2 Natural Minor   — 0 2 3 5 7 8 10
    0xB56,   // 3 Dorian          — 0 2 3 5 7 9 10
    0xA94,   // 4 Pentatonic Maj  — 0 2 4 7 9
    0x952,   // 5 Pentatonic Min  — 0 3 5 7 10
    0x972,   // 6 Blues           — 0 3 5 6 7 10
    0x000,   // 7 Custom          — stored in scaleMask param
};

ScaleConfig DrawnCurveProcessor::getScaleConfig (int /*lane*/) const noexcept
{
    // Scale is now global — the lane argument is retained for API compatibility
    // but ignored; all Note-mode lanes share the same quantization.
    const int     mode = static_cast<int> (
        apvts.getRawParameterValue (ParamID::scaleMode)->load());
    const uint8_t root = static_cast<uint8_t> (
        apvts.getRawParameterValue (ParamID::scaleRoot)->load());

    uint16_t mask;
    if (mode == 7)
        mask = static_cast<uint16_t> (
            apvts.getRawParameterValue (ParamID::scaleMask)->load());
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
