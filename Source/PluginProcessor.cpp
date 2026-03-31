/**
 * @file PluginProcessor.cpp
 *
 * Implementation of DrawnCurveProcessor.
 * See PluginProcessor.h for the architecture overview.
 *
 * @brief Core processing logic for DrawnCurve MIDI modulation plugin.
 *
 * @thread_safety
 * This class processes audio and MIDI data on the audio thread,
 * with a hi-res timer thread for fallback MIDI output.
 * Thread synchronization is managed via spinlocks and atomics.
 *
 * @note The lane snapshot lifecycle involves deliberate snapshot replacements
 * with intentional leaks to avoid realtime deletion issues.
 * Future refactoring should consider shared_ptr-based ownership models.
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

#if JUCE_DEBUG
#include <atomic>

static std::atomic<uint64_t> gHiResTimerTryLockFailures { 0 };
static std::atomic<uint64_t> gHiResTimerDispatches      { 0 };
static std::atomic<uint64_t> gFallbackMidiFlushes       { 0 };
static std::atomic<uint64_t> gSnapshotReplacements      { 0 };

static void dbgLogCountersIfNeeded()
{
    static juce::int64 lastLogMs = 0;
    const juce::int64 now = static_cast<juce::int64> (juce::Time::getMillisecondCounterHiRes());

    if (now - lastLogMs >= 5000)
    {
        lastLogMs = now;

        uint64_t tries = gHiResTimerTryLockFailures.load(std::memory_order_relaxed);
        uint64_t dispatches = gHiResTimerDispatches.load(std::memory_order_relaxed);
        uint64_t fallbackFlushes = gFallbackMidiFlushes.load(std::memory_order_relaxed);
        uint64_t snapRepls = gSnapshotReplacements.load(std::memory_order_relaxed);

        juce::Logger::writeToLog ("[DrawnCurve Debug] HiResTimer Dispatches: " + juce::String(dispatches)
            + ", TryLock Failures: " + juce::String(tries)
            + ", Fallback MIDI Flushes: " + juce::String(fallbackFlushes)
            + ", Snapshot Replacements: " + juce::String(snapRepls));
    }
}
#endif // JUCE_DEBUG

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
    static const int kDefaultCC[kMaxLanes]  = { 74, 1, 11, 10 };   // filter, mod, expression, pan
    static const int kDefaultCh[kMaxLanes]  = { 1, 1, 1, 1 };

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

        layout.add (std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID { laneParam (L, ParamID::loopMode), 1 },
            lname + "One-Shot", false));  ///< false = Loop, true = One-Shot

        layout.add (std::make_unique<juce::AudioParameterInt>(
            juce::ParameterID { laneParam (L, ParamID::phaseOffset), 1 },
            lname + "Phase Offset", 0, 100, 0));  ///< Curve start offset in percent

        layout.add (std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID { laneParam (L, ParamID::xQuantize), 1 },
            lname + "X Quantize", false));  ///< Snap playhead to X-grid boundaries (S&H in time)

        layout.add (std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID { laneParam (L, ParamID::yQuantize), 1 },
            lname + "Y Quantize", false));  ///< Snap output to nearest Y-grid tick level

        layout.add (std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID { laneParam (L, ParamID::legatoMode), 1 },
            lname + "Legato", false));  ///< Note On before Note Off for legato ties (Note mode only)

        layout.add (std::make_unique<juce::AudioParameterInt>(
            juce::ParameterID { laneParam (L, ParamID::xDivisions), 1 },
            lname + "X Grid Divisions", 2, 32, 4));

        layout.add (std::make_unique<juce::AudioParameterInt>(
            juce::ParameterID { laneParam (L, ParamID::yDivisions), 1 },
            lname + "Y Grid Divisions", 2, 24, 4));

#if defined(DC_HAVE_PER_LANE_PLAYBACK_PARAMS)
        // TODO: add ParamID entries for per-lane playback
        // Prep: Per-lane playback controls (currently unused by engine; UI TBD)
        layout.add (std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID { laneParam (L, ParamID::useGlobalPlayback), 1 },
            lname + "Use Global Playback", false));  // false = per-lane values active by default

        layout.add (std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { laneParam (L, ParamID::laneSpeedMul), 1 },
            lname + "Speed Multiplier",
            juce::NormalisableRange<float> (0.25f, 4.0f, 0.01f, 0.5f), 1.0f));

        layout.add (std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID { laneParam (L, ParamID::laneDirection), 1 },
            lname + "Playback Direction (Lane)", juce::StringArray { "Forward", "Reverse", "Ping-Pong" }, 0));

        layout.add (std::make_unique<juce::AudioParameterInt>(
            juce::ParameterID { laneParam (L, ParamID::laneSyncGroup), 1 },
            lname + "Sync Group", 0, 4, 0));  // 0 = none/global; 1..4 = group ids
#endif
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

//==============================================================================
// Prep: helpers to compute per-lane effective playback (currently unused by engine)
static inline float laneEffectiveSpeed (const juce::AudioProcessorValueTreeState& apvts, int lane, float globalSpeed)
{
#if defined(DC_HAVE_PER_LANE_PLAYBACK_PARAMS)
    const bool useGlobal = apvts.getRawParameterValue (laneParam (lane, ParamID::useGlobalPlayback))->load() > 0.5f;
    if (useGlobal) return globalSpeed;
    const float mul = apvts.getRawParameterValue (laneParam (lane, ParamID::laneSpeedMul))->load();
    return juce::jlimit (0.01f, 16.0f, globalSpeed * juce::jmax (0.01f, mul));
#else
    return globalSpeed;
#endif
}

static inline PlaybackDirection laneEffectiveDirection (const juce::AudioProcessorValueTreeState& apvts, int lane, PlaybackDirection globalDir)
{
#if defined(DC_HAVE_PER_LANE_PLAYBACK_PARAMS)
    const bool useGlobal = apvts.getRawParameterValue (laneParam (lane, ParamID::useGlobalPlayback))->load() > 0.5f;
    if (useGlobal) return globalDir;
    const int idx = static_cast<int> (apvts.getRawParameterValue (laneParam (lane, ParamID::laneDirection))->load());
    return static_cast<PlaybackDirection> (juce::jlimit (0, 2, idx));
#else
    return globalDir;
#endif
}
// TODO: lane sync groups can be handled by aligning phase across lanes that share the same non-zero group id.

//==============================================================================
// Internal helper for potential future per-lane scale support.
// Currently, all lanes use the global scale config.
#if 0 // TODO: replace with per-lane parameter when introduced.
static inline bool useGlobalScaleForLane(int /*lane*/) { return true; }
#endif

ScaleConfig DrawnCurveProcessor::getScaleConfig (int /*lane*/) const noexcept
{
    // Note: lane argument ignored; all lanes share global scale config as useGlobalScaleForLane(lane) is hardcoded true.
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

#if JUCE_DEBUG
            ++gFallbackMidiFlushes;
#endif
        }
    }

    // ── Read direction first — needed by the sync reset below ─────────────────
    // getRawParameterValue() returns the denormalized actual index for
    // AudioParameterChoice (confirmed from JUCE 8 source).  static_cast<int>
    // of exactly 0.0f / 1.0f / 2.0f is always correct.
    const auto dir = static_cast<PlaybackDirection> (
        static_cast<int> (apvts.getRawParameterValue (ParamID::playbackDirection)->load()));

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
                    //
                    // Use resetForDirection() instead of reset() so the smoother
                    // is seeded from the correct starting phase for the current
                    // direction.  Without this, Reverse would glide from 0 to the
                    // end-of-curve value on the first block of every sync cycle,
                    // making it sound indistinguishable from Forward.
                    _userManualPauseInSync.store (false, std::memory_order_release);
                    juce::SpinLock::ScopedLockType lock (_engineLock);
                    _engine.resetForDirection (dir);
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

#if defined(DC_HAVE_PER_LANE_PLAYBACK_PARAMS)
    // Compute per-lane speed and direction.  When sync is on, speed comes from the
    // host BPM calculation (all lanes share the same tempo-derived rate); only the
    // direction can differ.  In free mode each lane may also have its own multiplier.
    std::array<float, kMaxLanes>            laneSpeedRatios;
    std::array<PlaybackDirection, kMaxLanes> laneDirs;
    for (int L = 0; L < kMaxLanes; ++L)
    {
        laneSpeedRatios[static_cast<size_t>(L)] =
            syncOn ? effectiveSpeed : laneEffectiveSpeed (apvts, L, effectiveSpeed);
        laneDirs[static_cast<size_t>(L)] = laneEffectiveDirection (apvts, L, dir);
    }
    _effectiveSpeedRatio.store (laneSpeedRatios[0], std::memory_order_relaxed);
#else
    _effectiveSpeedRatio.store (effectiveSpeed, std::memory_order_relaxed);
#endif

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
    // Use a temporary buffer to minimize time spent holding the spinlock.
    // The engine writes to tempMidiBuffer while locked, then we swap it into
    // midiMessages after releasing the lock. This prevents the audio thread
    // from blocking the hi-res timer for extended periods.
    juce::MidiBuffer tempMidiBuffer;
    {
        juce::SpinLock::ScopedLockType lock (_engineLock);
        auto midiOut = [&tempMidiBuffer] (uint8_t status, uint8_t d1, uint8_t d2)
        {
            tempMidiBuffer.addEvent (makeMidiMessage (status, d1, d2), 0);
        };
#if defined(DC_HAVE_PER_LANE_PLAYBACK_PARAMS)
        _engine.processBlock (static_cast<uint32_t> (buffer.getNumSamples()),
                              getSampleRate(), midiOut, laneSpeedRatios, laneDirs);
#else
        _engine.processBlock (static_cast<uint32_t> (buffer.getNumSamples()),
                              getSampleRate(), midiOut, effectiveSpeed, dir);
#endif
    }
    
    // Add generated MIDI to output buffer (lock is now released)
    for (const auto meta : tempMidiBuffer)
        midiMessages.addEvent (meta.getMessage(), meta.samplePosition);

    // In standalone mode, forward generated MIDI to virtual port + direct target.
    {
        auto* vOut = _virtualMidiOut.load (std::memory_order_acquire);
        auto* dOut = _directMidiOut.load  (std::memory_order_acquire);
        if (vOut != nullptr || dOut != nullptr)
        {
            for (const auto meta : midiMessages)
            {
                const auto& msg = meta.getMessage();
                if (vOut) vOut->sendMessageNow (msg);
                if (dOut) dOut->sendMessageNow (msg);
            }
        }
    }
}

//==============================================================================
void DrawnCurveProcessor::hiResTimerCallback()
{
    if (! isPlaying() || ! anyLaneHasCurve()) return;

#if JUCE_DEBUG
    ++gHiResTimerDispatches;
#endif

    const int64_t now    = juce::Time::currentTimeMillis();
    const int64_t lastAT = _lastProcessBlockMs.load (std::memory_order_relaxed);
    if (now - lastAT < kAudioThreadTimeoutMs) return;

    const auto nominalFrames =
        static_cast<uint32_t> (_timerSampleRate * kTimerIntervalMs / 1000.0);

    // Read speed directly from the parameter tree — _effectiveSpeedRatio is only
    // updated by processBlock(), which never runs in standalone mode (no audio device).
    const float globalSpeed = apvts.getRawParameterValue (ParamID::playbackSpeed)->load();
    const auto  globalDir   = static_cast<PlaybackDirection> (
        static_cast<int> (apvts.getRawParameterValue (ParamID::playbackDirection)->load()));
#if defined(DC_HAVE_PER_LANE_PLAYBACK_PARAMS)
    std::array<float, kMaxLanes>            laneSpeedRatios;
    std::array<PlaybackDirection, kMaxLanes> laneDirs;
    for (int L = 0; L < kMaxLanes; ++L)
    {
        laneSpeedRatios[static_cast<size_t>(L)] = laneEffectiveSpeed  (apvts, L, globalSpeed);
        laneDirs       [static_cast<size_t>(L)] = laneEffectiveDirection (apvts, L, globalDir);
    }
#endif

    juce::MidiBuffer localBuf;
    {
        juce::SpinLock::ScopedTryLockType tryLock (_engineLock);
        if (! tryLock.isLocked()) {
#if JUCE_DEBUG
            gHiResTimerTryLockFailures.fetch_add(1, std::memory_order_relaxed);
#endif
            return;
        }

        auto timerMidiOut = [&localBuf] (uint8_t status, uint8_t d1, uint8_t d2)
        {
            localBuf.addEvent (makeMidiMessage (status, d1, d2), 0);
        };
#if defined(DC_HAVE_PER_LANE_PLAYBACK_PARAMS)
        _engine.processBlock (nominalFrames, _timerSampleRate,
                              timerMidiOut, laneSpeedRatios, laneDirs);
#else
        _engine.processBlock (nominalFrames, _timerSampleRate,
                              timerMidiOut, globalSpeed, globalDir);
#endif
    }

    if (! localBuf.isEmpty())
    {
        // In standalone mode, send to virtual port + direct target.
        {
            auto* vOut = _virtualMidiOut.load (std::memory_order_acquire);
            auto* dOut = _directMidiOut.load  (std::memory_order_acquire);
            if (vOut != nullptr || dOut != nullptr)
            {
                for (const auto meta : localBuf)
                {
                    const auto& msg = meta.getMessage();
                    if (vOut) vOut->sendMessageNow (msg);
                    if (dOut) dOut->sendMessageNow (msg);
                }
            }
        }

        // Also buffer for processBlock (flushed to the host/AudioProcessorPlayer path).
        juce::SpinLock::ScopedLockType lock (_pendingMidiLock);
        for (const auto meta : localBuf)
            _pendingMidi.addEvent (meta.getMessage(), meta.samplePosition);
    }

#if JUCE_DEBUG
    dbgLogCountersIfNeeded();
#endif
}

//==============================================================================
// Standalone Teach/Learn helper (no audio device required)
//==============================================================================

void DrawnCurveProcessor::handleTeachMidi (const juce::MidiMessage& msg)
{
    // Called from the TeachMidiCallback adapter in StandaloneApp.cpp.
    // Mirrors the Teach/Learn CC-detect logic in processBlock.
    const int pending = _teachPendingLane.load (std::memory_order_relaxed);
    if (pending < 0) return;

    const int msgType = static_cast<int> (
        apvts.getRawParameterValue (laneParam (pending, ParamID::msgType))->load());
    if (msgType == 0 && msg.isController())
    {
        const juce::String pid = laneParam (pending, ParamID::ccNumber);
        if (auto* p = dynamic_cast<juce::AudioParameterInt*> (apvts.getParameter (pid)))
            *p = msg.getControllerNumber();
        _teachPendingLane.store (-1, std::memory_order_relaxed);
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

    const bool  oneShot     = apvts.getRawParameterValue (laneParam (lane, ParamID::loopMode))->load() > 0.5f;
    const float phaseOffPct = apvts.getRawParameterValue (laneParam (lane, ParamID::phaseOffset))->load();

    /* Snapshot replacement strategy:
     * Each new snapshot is allocated and assigned to _laneSnaps without deleting
     * the previous one, intentionally leaking the old snapshot. This avoids
     * realtime unsafe deallocation while the audio thread might still access the old snapshot.
     * TODO: migrate to shared_ptr<const LaneSnapshot> ownership to allow safe
     * reclamation without leaks.
     */
    const bool  xQuant  = apvts.getRawParameterValue (laneParam (lane, ParamID::xQuantize))->load() > 0.5f;
    const bool  yQuant  = apvts.getRawParameterValue (laneParam (lane, ParamID::yQuantize))->load() > 0.5f;
    const bool  legato  = apvts.getRawParameterValue (laneParam (lane, ParamID::legatoMode))->load() > 0.5f;
    const auto  xDiv    = static_cast<uint8_t> (static_cast<int> (apvts.getRawParameterValue (laneParam (lane, ParamID::xDivisions))->load()));
    const auto  yDiv    = static_cast<uint8_t> (static_cast<int> (apvts.getRawParameterValue (laneParam (lane, ParamID::yDivisions))->load()));

    auto* snap = new LaneSnapshot (_capture.finalize (ccNum, ch, minOut, maxOut, smooth, msgType));
    snap->noteVelocity = noteVel;
    snap->oneShot      = oneShot;
    snap->phaseOffset  = phaseOffPct / 100.0f;
    snap->xQuantize    = xQuant;
    snap->yQuantize    = yQuant;
    snap->legatoMode   = legato;
    snap->xDivisions   = xDiv;
    snap->yDivisions   = yDiv;

#if JUCE_DEBUG
    ++gSnapshotReplacements;
#endif

    _laneSnaps[static_cast<size_t>(lane)] = snap;

    {
        juce::SpinLock::ScopedLockType lock (_engineLock);
        _engine.setSnapshot (lane, snap);
        _engine.resetLane (lane);   // rewind only this lane; Note Off from stopLane still fires
    }
}

void DrawnCurveProcessor::updateLaneSnapshot (int lane)
{
    if (lane < 0 || lane >= kMaxLanes) return;
    const auto* existing = _laneSnaps[static_cast<size_t>(lane)];
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

    const bool  oneShot     = apvts.getRawParameterValue (laneParam (lane, ParamID::loopMode))->load() > 0.5f;
    const float phaseOffPct = apvts.getRawParameterValue (laneParam (lane, ParamID::phaseOffset))->load();

    /* Snapshot replacement strategy:
     * Each new snapshot is allocated and assigned to _laneSnaps without deleting
     * the previous one, intentionally leaking the old snapshot. This avoids
     * realtime unsafe deallocation while the audio thread might still access the old snapshot.
     * TODO: migrate to shared_ptr<const LaneSnapshot> ownership to allow safe
     * reclamation without leaks.
     */
    const bool  xQuant  = apvts.getRawParameterValue (laneParam (lane, ParamID::xQuantize))->load() > 0.5f;
    const bool  yQuant  = apvts.getRawParameterValue (laneParam (lane, ParamID::yQuantize))->load() > 0.5f;
    const bool  legato  = apvts.getRawParameterValue (laneParam (lane, ParamID::legatoMode))->load() > 0.5f;
    const auto  xDiv    = static_cast<uint8_t> (static_cast<int> (apvts.getRawParameterValue (laneParam (lane, ParamID::xDivisions))->load()));
    const auto  yDiv    = static_cast<uint8_t> (static_cast<int> (apvts.getRawParameterValue (laneParam (lane, ParamID::yDivisions))->load()));

    // Clone the existing snapshot, then overwrite only the param-driven fields.
    auto* snap = new LaneSnapshot (*existing);
    snap->ccNumber       = ccNum;
    snap->midiChannel    = ch;
    snap->smoothing      = smooth;
    snap->minOut         = minOut;
    snap->maxOut         = maxOut;
    snap->messageType    = msgType;
    snap->noteVelocity   = noteVel;
    snap->oneShot        = oneShot;
    snap->phaseOffset    = phaseOffPct / 100.0f;
    snap->xQuantize      = xQuant;
    snap->yQuantize      = yQuant;
    snap->legatoMode     = legato;
    snap->xDivisions     = xDiv;
    snap->yDivisions     = yDiv;

#if JUCE_DEBUG
    ++gSnapshotReplacements;
#endif

    _laneSnaps[static_cast<size_t>(lane)] = snap;

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
    _laneSnaps[static_cast<size_t>(lane)] = nullptr;
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

void DrawnCurveProcessor::deleteLane (int lane)
{
    if (activeLaneCount <= 1 || lane < 0 || lane >= activeLaneCount) return;

    // Shift every lane above 'lane' down by one slot.
    // Both APVTS parameters and curve snapshots are copied to the destination.
    for (int src = lane + 1; src < activeLaneCount; ++src)
    {
        const int dst = src - 1;

        // ── Copy APVTS parameters ──────────────────────────────────────────
        // Using getValue() / setValueNotifyingHost() so all UI listeners fire.
        static const char* const kBases[] = {
            "msgType", "ccNumber", "midiChannel", "smoothing",
            "minOutput", "maxOutput", "noteVelocity", "loopMode",
            "phaseOffset", "scaleMode", "scaleRoot", "scaleMask",
            "enabled", "xQuantize", "yQuantize", "legatoMode", "xDivisions", "yDivisions", nullptr
        };
        for (int i = 0; kBases[i] != nullptr; ++i)
        {
            const juce::String base (kBases[i]);
            if (auto* pSrc = apvts.getParameter (laneParam (src, base)))
            if (auto* pDst = apvts.getParameter (laneParam (dst, base)))
                pDst->setValueNotifyingHost (pSrc->getValue());
        }

        // ── Copy curve snapshot ───────────────────────────────────────────
        // A new heap copy is made so the audio thread can safely finish
        // reading the original while the UI thread installs the new one.
        const auto* srcSnap = _laneSnaps[static_cast<size_t> (src)];
        if (srcSnap && srcSnap->valid)
        {
            auto* newSnap = new LaneSnapshot (*srcSnap);
            _laneSnaps[static_cast<size_t> (dst)] = newSnap;
            juce::SpinLock::ScopedLockType lock (_engineLock);
            _engine.setSnapshot (dst, newSnap);
        }
        else
        {
            _laneSnaps[static_cast<size_t> (dst)] = nullptr;
            juce::SpinLock::ScopedLockType lock (_engineLock);
            _engine.clearSnapshot (dst);
        }
    }

    // Clear the vacated top slot and shrink the active count.
    clearSnapshot (activeLaneCount - 1);
    --activeLaneCount;
}

//==============================================================================
// MIDI Panic
//==============================================================================

void DrawnCurveProcessor::setLanePaused (int lane, bool paused)
{
    _engine.setLanePaused (lane, paused);
}

bool DrawnCurveProcessor::getLanePaused (int lane) const noexcept
{
    return _engine.getLanePaused (lane);
}

void DrawnCurveProcessor::setLanesSynced (bool synced)
{
    _engine.setLanesSynced (synced);
}

bool DrawnCurveProcessor::getLanesSynced() const noexcept
{
    return _engine.getLanesSynced();
}

void DrawnCurveProcessor::restartAllLanes()
{
    const auto dir = static_cast<PlaybackDirection> (
        static_cast<int> (apvts.getRawParameterValue (ParamID::playbackDirection)->load()));
    juce::SpinLock::ScopedLockType lock (_engineLock);
    _engine.resetForDirection (dir);
}

void DrawnCurveProcessor::sendPanic()
{
    _panicNeeded.store (true, std::memory_order_release);
}

void DrawnCurveProcessor::setVirtualMidiOutput (juce::MidiOutput* output) noexcept
{
    _virtualMidiOut.store (output, std::memory_order_release);
}

void DrawnCurveProcessor::setDirectMidiOutput (juce::MidiOutput* output) noexcept
{
    _directMidiOut.store (output, std::memory_order_release);
}

juce::MidiOutput* DrawnCurveProcessor::getDirectMidiOutput() const noexcept
{
    return _directMidiOut.load (std::memory_order_acquire);
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
// Query API
//==============================================================================

bool DrawnCurveProcessor::hasCurve (int lane) const noexcept
{
    if (lane < 0 || lane >= kMaxLanes) return false;
    return _laneSnaps[static_cast<size_t>(lane)] != nullptr && _laneSnaps[static_cast<size_t>(lane)]->valid;
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
    if (lane >= 0 && lane < kMaxLanes && _laneSnaps[static_cast<size_t>(lane)] && _laneSnaps[static_cast<size_t>(lane)]->valid)
        return _laneSnaps[static_cast<size_t>(lane)]->table;
    return {};
}

float DrawnCurveProcessor::curveDuration (int lane) const noexcept
{
    if (lane >= 0 && lane < kMaxLanes && _laneSnaps[static_cast<size_t>(lane)] && _laneSnaps[static_cast<size_t>(lane)]->valid)
        return _laneSnaps[static_cast<size_t>(lane)]->durationSeconds;
    return 0.0f;
}

//==============================================================================
// Scale quantization
//==============================================================================

void DrawnCurveProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();

    // Increment when making breaking schema changes.
    state.setProperty ("stateVersion", 2, nullptr);
    state.setProperty ("activeLaneCount", activeLaneCount, nullptr);

    for (int L = 0; L < kMaxLanes; ++L)
    {
        const auto* snap = _laneSnaps[static_cast<size_t>(L)];
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

    // Retrieve version for possible future migration logic.
    const int stateVersion = static_cast<int> (state.getProperty ("stateVersion", 1));
    juce::ignoreUnused (stateVersion); // Future upgrades can branch based on stateVersion to migrate old state formats.

    // Restore active lane count (clamped; 0 → 1 as safe minimum).
    activeLaneCount = juce::jlimit (1, kMaxLanes,
                          (int) state.getProperty ("activeLaneCount", 1));

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

                _laneSnaps[static_cast<size_t>(L)] = snap;
                {
                    juce::SpinLock::ScopedLockType lock (_engineLock);
                    _engine.setSnapshot (L, snap);
                }
            }
        }
    }

    // If state pre-dates activeLaneCount (e.g. stateVersion < 3), ensure activeLaneCount
    // is at least large enough to expose every lane that has a curve.
    if (! state.hasProperty ("activeLaneCount"))
        for (int L = 0; L < kMaxLanes; ++L)
            if (hasCurve (L))
                activeLaneCount = juce::jmax (activeLaneCount, L + 1);

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

                _laneSnaps[0u] = snap;
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

