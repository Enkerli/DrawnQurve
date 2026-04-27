#include "GestureEngine.hpp"
#include <cmath>
#include <algorithm>

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

GestureEngine::GestureEngine()
{
    for (int i = 0; i < kMaxLanes; ++i)
    {
        _snapshots[static_cast<size_t>(i)].store    (nullptr, std::memory_order_relaxed);
        _noteOffNeeded[static_cast<size_t>(i)].store(false,   std::memory_order_relaxed);
        _scalesPacked[static_cast<size_t>(i)].store (0xFFF,   std::memory_order_relaxed);
        _laneEnabled[static_cast<size_t>(i)].store  (true,    std::memory_order_relaxed);
        _lanePaused[static_cast<size_t>(i)].store   (false,   std::memory_order_relaxed);
        _lanePhases[static_cast<size_t>(i)].store   (0.0f,    std::memory_order_relaxed);
        _lastSentMirror[static_cast<size_t>(i)].store (-1,    std::memory_order_relaxed);
    }
}

int GestureEngine::getLastSentValue (int lane) const noexcept
{
    if (lane < 0 || lane >= kMaxLanes) return -1;
    return _lastSentMirror[static_cast<size_t>(lane)].load (std::memory_order_acquire);
}

// ---------------------------------------------------------------------------
// UI-thread methods
// ---------------------------------------------------------------------------

void GestureEngine::setSnapshot (int lane, const LaneSnapshot* snapshot)
{
    if (lane < 0 || lane >= kMaxLanes) return;
    _snapshots[static_cast<size_t>(lane)].store (snapshot, std::memory_order_release);
}

void GestureEngine::clearSnapshot (int lane)
{
    if (lane < 0 || lane >= kMaxLanes) return;
    _snapshots[static_cast<size_t>(lane)].store     (nullptr, std::memory_order_release);
    _noteOffNeeded[static_cast<size_t>(lane)].store (true,    std::memory_order_release);
}

void GestureEngine::clearAllSnapshots()
{
    for (int i = 0; i < kMaxLanes; ++i)
    {
        _snapshots[static_cast<size_t>(i)].store    (nullptr, std::memory_order_release);
        _noteOffNeeded[static_cast<size_t>(i)].store(true,    std::memory_order_release);
    }
    _isPlaying.store (false, std::memory_order_release);
}

void GestureEngine::setPlaying (bool playing)
{
    _isPlaying.store (playing, std::memory_order_release);
    if (! playing)
        for (int i = 0; i < kMaxLanes; ++i)
            _noteOffNeeded[static_cast<size_t>(i)].store (true, std::memory_order_release);
}

void GestureEngine::reset()
{
    for (int i = 0; i < kMaxLanes; ++i)
    {
        // Don't clear _noteOffNeeded here — let processLane send the Note Off
        // on the next block before starting the new curve.  Only reset dedup
        // state when no note-off is already pending.
        if (! _noteOffNeeded[static_cast<size_t>(i)].load (std::memory_order_acquire))
        {
            _runtimes[static_cast<size_t>(i)].lastSentValue = -1;
            _lastSentMirror[static_cast<size_t>(i)].store (-1, std::memory_order_release);
        }
        _runtimes[static_cast<size_t>(i)].playheadSeconds = 0.0;
        _runtimes[static_cast<size_t>(i)].smoothedValue   = 0.0f;
        _lanePhases[static_cast<size_t>(i)].store (0.0f,  std::memory_order_relaxed);
        _lanePaused[static_cast<size_t>(i)].store (false, std::memory_order_relaxed);
    }

    _currentPhase.store (0.0f, std::memory_order_relaxed);
    _syncMasterPlayhead = 0.0;
    _syncWasEnabled     = false;   // force re-snap on next processBlock if sync is active
}

void GestureEngine::resetForDirection (PlaybackDirection dir)
{
    // Choose raw start phase: Reverse starts from the end (1.0), everything else from 0.
    const float rawPhase = (dir == PlaybackDirection::Reverse) ? 1.0f : 0.0f;

    for (int i = 0; i < kMaxLanes; ++i)
    {
        // Preserve any pending Note Off — same policy as reset().
        if (! _noteOffNeeded[static_cast<size_t>(i)].load (std::memory_order_acquire))
        {
            _runtimes[static_cast<size_t>(i)].lastSentValue = -1;
            _lastSentMirror[static_cast<size_t>(i)].store (-1, std::memory_order_release);
        }

        _runtimes[static_cast<size_t>(i)].playheadSeconds = 0.0;

        // Seed the smoother from the curve's value at the correct starting phase
        // (raw direction phase + phaseOffset) so that the first block of output
        // begins at the right position without a "glide from zero" artefact.
        const auto* snap = _snapshots[static_cast<size_t>(i)].load (std::memory_order_acquire);
        const float offset    = (snap && snap->valid) ? snap->phaseOffset : 0.0f;
        const float seedPhase = std::fmod (rawPhase + offset + 1.0f, 1.0f);
        _runtimes[static_cast<size_t>(i)].smoothedValue =
            (snap && snap->valid) ? sampleCurve (*snap, seedPhase) : 0.0f;

        // Store the unshifted playhead phase (0 or 1 depending on direction).
        _lanePhases[static_cast<size_t>(i)].store (rawPhase, std::memory_order_relaxed);
    }

    _currentPhase.store (rawPhase, std::memory_order_relaxed);
    _syncMasterPlayhead = 0.0;
    _syncWasEnabled     = false;
}

void GestureEngine::stopLane (int lane)
{
    if (lane < 0 || lane >= kMaxLanes) return;
    // Signal processLane to send Note Off on the next processBlock call.
    // Does not stop playback on other lanes.
    _noteOffNeeded[static_cast<size_t>(lane)].store (true, std::memory_order_release);
}

void GestureEngine::resetLane (int lane)
{
    if (lane < 0 || lane >= kMaxLanes) return;
    // Rewind playhead.  Do NOT touch _noteOffNeeded or lastSentValue
    // so any pending Note Off still fires correctly in processLane.
    _runtimes[static_cast<size_t>(lane)].playheadSeconds = 0.0;
    _runtimes[static_cast<size_t>(lane)].lastXTick       = -1;   // reset X-quantize dedup

    // Pre-seed the smoother from the curve's value at the effective start phase
    // (phase 0 shifted by phaseOffset) so that playback begins at the correct
    // value without a "glide from zero" artefact.
    const auto* snap = _snapshots[static_cast<size_t>(lane)].load (std::memory_order_acquire);
    const float seedPhase = (snap && snap->valid) ? snap->phaseOffset : 0.0f;
    _runtimes[static_cast<size_t>(lane)].smoothedValue = (snap && snap->valid) ? sampleCurve (*snap, seedPhase) : 0.0f;

    _lanePhases[static_cast<size_t>(lane)].store (0.0f, std::memory_order_relaxed);
}

void GestureEngine::setLaneEnabled (int lane, bool enabled)
{
    if (lane < 0 || lane >= kMaxLanes) return;
    // Detect enabled→disabled transition and queue a Note Off for any held note.
    // It is safe to call this on every processBlock iteration since the exchange
    // only triggers stopLane on the false edge.
    const bool wasEnabled = _laneEnabled[static_cast<size_t>(lane)].exchange (enabled, std::memory_order_acq_rel);
    if (wasEnabled && ! enabled)
        stopLane (lane);    // sets _noteOffNeeded; fires in the current processLane call
}

void GestureEngine::setLanePaused (int lane, bool paused)
{
    if (lane < 0 || lane >= kMaxLanes) return;
    // Queue a Note Off on the false→true (playing→paused) edge, so any held
    // note stops immediately rather than sustaining indefinitely.
    const bool wasPaused = _lanePaused[static_cast<size_t>(lane)].exchange (paused, std::memory_order_acq_rel);
    if (! wasPaused && paused)
        stopLane (lane);
}

bool GestureEngine::getLanePaused (int lane) const noexcept
{
    if (lane < 0 || lane >= kMaxLanes) return false;
    return _lanePaused[static_cast<size_t>(lane)].load (std::memory_order_acquire);
}

void GestureEngine::setLanesSynced (bool synced)
{
    // Only write the atomic flag here — _syncMasterPlayhead is render-thread-only
    // and must not be touched from the UI thread to avoid a data race.
    // The render thread detects the false→true transition itself and resets
    // _syncMasterPlayhead at that point (see processBlock).
    _lanesSynced.store (synced, std::memory_order_release);
}

bool GestureEngine::getLanesSynced() const noexcept
{
    return _lanesSynced.load (std::memory_order_acquire);
}

void GestureEngine::setScaleConfig (int lane, ScaleConfig config)
{
    if (lane < 0 || lane >= kMaxLanes) return;
    _scalesPacked[static_cast<size_t>(lane)].store (packScale (config), std::memory_order_release);
}

bool  GestureEngine::getPlaying()      const { return _isPlaying.load (std::memory_order_acquire); }
float GestureEngine::getCurrentPhase() const { return _currentPhase.load (std::memory_order_relaxed); }

float GestureEngine::getCurrentPhaseForLane (int lane) const
{
    if (lane < 0 || lane >= kMaxLanes) return 0.0f;
    return _lanePhases[static_cast<size_t>(lane)].load (std::memory_order_relaxed);
}

// ---------------------------------------------------------------------------
// Static utilities
// ---------------------------------------------------------------------------

int GestureEngine::quantizeNote (int rawNote, ScaleConfig sc, bool movingUp)
{
    if (sc.mask == 0xFFF) return rawNote;

    rawNote = std::clamp (rawNote, 0, 127);

    const int pc       = rawNote % 12;
    const int interval = (pc - (int)sc.root + 12) % 12;

    // Bitmask convention: bit (11 - interval) = interval present in scale.
    // Bit 11 = root (interval 0), bit 0 = major-7th (interval 11).
    if ((sc.mask >> (11 - interval)) & 1) return rawNote;

    int downNote = -1, upNote = -1;

    for (int d = 1; d <= 6; ++d)
    {
        if (downNote < 0 && rawNote - d >= 0)
        {
            const int di = (interval - d + 12) % 12;
            if ((sc.mask >> (11 - di)) & 1)
                downNote = rawNote - d;
        }
        if (upNote < 0 && rawNote + d <= 127)
        {
            const int ui = (interval + d) % 12;
            if ((sc.mask >> (11 - ui)) & 1)
                upNote = rawNote + d;
        }
        if (downNote >= 0 && upNote >= 0) break;
    }

    if (downNote < 0 && upNote < 0) return rawNote;
    if (downNote < 0) return upNote;
    if (upNote < 0)   return downNote;

    const int dDown = rawNote - downNote;
    const int dUp   = upNote  - rawNote;
    if (dDown == dUp) return movingUp ? upNote : downNote;
    return (dDown < dUp) ? downNote : upNote;
}

// ---------------------------------------------------------------------------
// Render-thread helpers
// ---------------------------------------------------------------------------

float GestureEngine::sampleCurve (const LaneSnapshot& snap, float phase) const
{
    const float idx  = phase * 255.0f;
    const int   i0   = static_cast<int> (idx);
    const int   i1   = (i0 + 1) & 255;
    const float frac = idx - static_cast<float> (i0);
    return snap.table[static_cast<size_t> (i0)]
         + frac * (snap.table[static_cast<size_t> (i1)]
                 - snap.table[static_cast<size_t> (i0)]);
}

// ---------------------------------------------------------------------------
// Per-lane processing (render thread) — called for each lane in processBlock
// ---------------------------------------------------------------------------

void GestureEngine::processLane (int lane, uint32_t frameCount, double sampleRate,
                                  const MIDIOut& midiOut,
                                  float speedRatio, PlaybackDirection direction,
                                  float forcedPhase)
{
    auto& rt  = _runtimes[static_cast<size_t>(lane)];
    const auto* snap = _snapshots[static_cast<size_t>(lane)].load (std::memory_order_acquire);

    // ── Note Off cleanup (may fire even without a valid snapshot) ─────────────
    if (_noteOffNeeded[static_cast<size_t>(lane)].exchange (false, std::memory_order_acq_rel))
    {
        if (snap && snap->valid
            && snap->messageType == MessageType::Note
            && rt.lastSentValue >= 0 && midiOut)
        {
            midiOut (0x80u | (snap->midiChannel & 0x0Fu),
                     static_cast<uint8_t> (rt.lastSentValue), 0u);
        }
        rt.lastSentValue = -1;
        _lastSentMirror[static_cast<size_t>(lane)].store (-1, std::memory_order_release);
    }

    if (! snap || ! snap->valid) return;
    if (! _isPlaying.load (std::memory_order_acquire)) return;
    if (! _laneEnabled[static_cast<size_t>(lane)].load (std::memory_order_acquire)) return;   // muted
    if (  _lanePaused [static_cast<size_t>(lane)].load (std::memory_order_acquire)) return;   // individually paused

    const double effectiveDur = static_cast<double> (snap->durationSeconds)
                                / static_cast<double> (std::max (speedRatio, 0.001f));

    // ── Advance playhead ──────────────────────────────────────────────────────
    // One-shot sentinel: playheadSeconds == -1 means "already completed this pass".
    // reset() / resetLane() clear the sentinel by setting playheadSeconds = 0.
    if (snap->oneShot && rt.playheadSeconds < 0.0)
        return;   // one-shot complete — lane is silent until next reset

    rt.playheadSeconds += static_cast<double> (frameCount) / sampleRate;

    // ── Phase (direction-dependent) ───────────────────────────────────────────
    float phase;

    // When forcedPhase is provided (lane-sync master phase) and this lane loops,
    // skip own phase computation and use the master phase directly.
    // playheadSeconds is still advanced above so it stays coherent when sync is disabled.
    if (forcedPhase >= 0.0f && ! snap->oneShot)
    {
        // Keep own playheadSeconds wrapped so it doesn't drift unboundedly
        if (direction == PlaybackDirection::PingPong)
        {
            if (rt.playheadSeconds >= 2.0 * effectiveDur)
                rt.playheadSeconds = std::fmod (rt.playheadSeconds, 2.0 * effectiveDur);
        }
        else
        {
            if (rt.playheadSeconds >= effectiveDur)
                rt.playheadSeconds = std::fmod (rt.playheadSeconds, effectiveDur);
        }
        phase = forcedPhase;
    }
    else if (direction == PlaybackDirection::Reverse)
    {
        if (rt.playheadSeconds >= effectiveDur)
        {
            if (snap->oneShot)
            {
                // Reverse one-shot: curve played from end to beginning — done.
                if (snap->messageType == MessageType::Note)
                    _noteOffNeeded[static_cast<size_t>(lane)].store (true, std::memory_order_release);
                rt.playheadSeconds = -1.0;   // sentinel: skip on future blocks
                return;
            }
            rt.playheadSeconds = std::fmod (rt.playheadSeconds, effectiveDur);
        }
        phase = 1.0f - static_cast<float> (rt.playheadSeconds / effectiveDur);
    }
    else if (direction == PlaybackDirection::PingPong)
    {
        const double ppDur = 2.0 * effectiveDur;
        if (rt.playheadSeconds >= ppDur)
        {
            if (snap->oneShot)
            {
                // PingPong one-shot: one full round-trip (0→1→0) — done.
                if (snap->messageType == MessageType::Note)
                    _noteOffNeeded[static_cast<size_t>(lane)].store (true, std::memory_order_release);
                rt.playheadSeconds = -1.0;
                return;
            }
            rt.playheadSeconds = std::fmod (rt.playheadSeconds, ppDur);
        }
        const double frac = rt.playheadSeconds / effectiveDur;
        phase = (frac <= 1.0) ? static_cast<float> (frac)
                              : static_cast<float> (2.0 - frac);
    }
    else  // Forward
    {
        if (rt.playheadSeconds >= effectiveDur)
        {
            if (snap->oneShot)
            {
                // Forward one-shot: curve played from beginning to end — done.
                if (snap->messageType == MessageType::Note)
                    _noteOffNeeded[static_cast<size_t>(lane)].store (true, std::memory_order_release);
                rt.playheadSeconds = -1.0;
                return;
            }
            rt.playheadSeconds = std::fmod (rt.playheadSeconds, effectiveDur);
        }
        phase = static_cast<float> (rt.playheadSeconds / effectiveDur);
    }

    // Store per-lane phase for the UI playhead (unshifted, so the playhead
    // always tracks 0→1 regardless of phaseOffset).
    _lanePhases[static_cast<size_t>(lane)].store (phase, std::memory_order_relaxed);

    // Also update _currentPhase from the lowest-indexed valid lane so the
    // CurveDisplay always shows a moving playhead when anything is playing.
    {
        bool lowerValid = false;
        for (int l = 0; l < lane; ++l)
        {
            const auto* s = _snapshots[static_cast<size_t>(l)].load (std::memory_order_relaxed);
            if (s && s->valid) { lowerValid = true; break; }
        }
        if (! lowerValid)
            _currentPhase.store (phase, std::memory_order_relaxed);
    }

    // ── X-axis quantization (sample-and-hold in time) ─────────────────────────
    // Snap the playhead to the nearest tick-boundary so the curve is only
    // re-sampled at evenly-spaced time divisions — effectively S&H in time.
    // With N divisions and a 4-beat loop: N=8 → eighth-note quantization.
    if (snap->xQuantize && snap->xDivisions >= 2)
    {
        const float tickWidth = 1.0f / static_cast<float> (snap->xDivisions);
        const int   tickIndex = static_cast<int> (phase / tickWidth);
        rt.lastXTick = tickIndex;
        phase = static_cast<float> (tickIndex) * tickWidth;
    }

    // Apply phase offset: shifts which part of the curve is sampled without
    // affecting the playhead display or loop boundary logic above.
    const float sampledPhase = std::fmod (phase + snap->phaseOffset + 1.0f, 1.0f);
    const float target = sampleCurve (*snap, sampledPhase);

    // ── One-pole smoother ─────────────────────────────────────────────────────
    // When xQuantize is on we want true sample-and-hold: the snapped phase
    // alone produces a piecewise-constant target between ticks, but the
    // smoother would slew between adjacent tick values and turn the
    // staircase into a ramp.  Bypass it (alpha=1) so the output snaps cleanly.
    const float alpha = (snap->smoothing <= 0.0f || snap->xQuantize)
        ? 1.0f
        : 1.0f - std::exp (- static_cast<float> (frameCount)
                           / (snap->smoothing * 2.0f * static_cast<float> (sampleRate)));
    rt.smoothedValue += alpha * (target - rt.smoothedValue);

    // ── Y-axis quantization (discrete output levels) ──────────────────────────
    // Snap the smoothed output to the nearest horizontal tick level.
    // For CC/PB/AT this creates discrete parameter steps.
    // For Note mode, Y-quantize is applied separately on the raw target below
    // (bypassing the smoother, consistent with how note detection works).
    if (snap->yQuantize && snap->yDivisions >= 2)
    {
        const float step = 1.0f / static_cast<float> (snap->yDivisions);
        rt.smoothedValue = std::round (rt.smoothedValue / step) * step;
    }

    // ── Output range mapping ──────────────────────────────────────────────────
    const float ranged = snap->minOut + rt.smoothedValue * (snap->maxOut - snap->minOut);

    // ── Emit MIDI ─────────────────────────────────────────────────────────────
    switch (snap->messageType)
    {
        case MessageType::CC:
        {
            const int v = std::clamp (static_cast<int> (std::lround (ranged * 127.0f)), 0, 127);
            if (v != rt.lastSentValue)
            {
                if (midiOut) midiOut (0xB0u | (snap->midiChannel & 0x0Fu),
                                      snap->ccNumber, static_cast<uint8_t> (v));
                rt.lastSentValue = v;
                _lastSentMirror[static_cast<size_t>(lane)].store (v, std::memory_order_release);
            }
            break;
        }
        case MessageType::ChannelPressure:
        {
            const int v = std::clamp (static_cast<int> (std::lround (ranged * 127.0f)), 0, 127);
            if (v != rt.lastSentValue)
            {
                if (midiOut) midiOut (0xD0u | (snap->midiChannel & 0x0Fu),
                                      static_cast<uint8_t> (v), 0u);
                rt.lastSentValue = v;
                _lastSentMirror[static_cast<size_t>(lane)].store (v, std::memory_order_release);
            }
            break;
        }
        case MessageType::PitchBend:
        {
            const int v = std::clamp (static_cast<int> (std::lround (ranged * 16383.0f)), 0, 16383);
            if (v != rt.lastSentValue)
            {
                if (midiOut) midiOut (0xE0u | (snap->midiChannel & 0x0Fu),
                                      static_cast<uint8_t> (v & 0x7F),
                                      static_cast<uint8_t> ((v >> 7) & 0x7F));
                rt.lastSentValue = v;
                _lastSentMirror[static_cast<size_t>(lane)].store (v, std::memory_order_release);
            }
            break;
        }
        case MessageType::Note:
        {
            // Use the RAW curve value (target), NOT the smoother output, for
            // note detection.  The smoother ramps continuously between values
            // which causes it to traverse multiple note boundaries during the
            // attack phase — the classic "glissando from note 0" bug.
            // Smoothing is intentionally bypassed here: in Note mode the
            // output is quantised to discrete semitones anyway, so a
            // continuous ramp only creates unwanted glissando artefacts.
            // The smoother state (rt.smoothedValue) is still updated above so
            // that any future switch to a continuous-output mode is seamless.

            constexpr float kClearance = 0.35f;   // semitones past midpoint required to commit

            // When Y-quantize is active, snap the raw target to the nearest
            // Y-tick level before computing the pitch.  This reduces pitch
            // resolution to yDivisions levels within the output range —
            // e.g. 4 ticks over a 2-octave range ≈ 6 semitones per step.
            // The snapped value is used only for this note calculation; the
            // smoother state (rt.smoothedValue) remains unaffected by this path.
            float noteTarget = target;
            if (snap->yQuantize && snap->yDivisions >= 2)
            {
                const float step = 1.0f / static_cast<float> (snap->yDivisions);
                noteTarget = std::round (noteTarget / step) * step;
            }

            const float rawNoteF  = (snap->minOut + noteTarget * (snap->maxOut - snap->minOut)) * 127.0f;
            const int   committed = rt.lastSentValue;
            const ScaleConfig sc  = unpackScale (_scalesPacked[static_cast<size_t>(lane)].load (std::memory_order_acquire));

            // Direction based on continuous float position vs last committed integer note.
            const bool movingUp  = (committed < 0) || (rawNoteF > static_cast<float> (committed));
            const int  candidate = quantizeNote (
                std::clamp (static_cast<int> (std::lround (rawNoteF)), 0, 127), sc, movingUp);

            if (candidate == committed) break;

            // Require rawNoteF to pass the midpoint + clearance before committing.
            if (committed >= 0)
            {
                const float mid = (static_cast<float> (committed) + static_cast<float> (candidate)) * 0.5f;
                const bool crossedClearly =
                    (candidate > committed && rawNoteF >= mid + kClearance) ||
                    (candidate < committed && rawNoteF <= mid - kClearance);
                if (!crossedClearly) break;
            }

            if (midiOut)
            {
                if (snap->legatoMode)
                {
                    // Legato: send Note On BEFORE Note Off so the synth sees
                    // overlapping notes and triggers its legato/glide path.
                    midiOut (0x90u | (snap->midiChannel & 0x0Fu),
                             static_cast<uint8_t> (candidate), snap->noteVelocity);
                    if (committed >= 0)
                        midiOut (0x80u | (snap->midiChannel & 0x0Fu),
                                 static_cast<uint8_t> (committed), 0u);
                }
                else
                {
                    // Standard: Note Off first, then Note On (re-attack).
                    if (committed >= 0)
                        midiOut (0x80u | (snap->midiChannel & 0x0Fu),
                                 static_cast<uint8_t> (committed), 0u);
                    midiOut (0x90u | (snap->midiChannel & 0x0Fu),
                             static_cast<uint8_t> (candidate), snap->noteVelocity);
                }
            }
            rt.lastSentValue = candidate;
            _lastSentMirror[static_cast<size_t>(lane)].store (candidate, std::memory_order_release);
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// processBlock — render thread: iterates all lanes
// ---------------------------------------------------------------------------

void GestureEngine::processBlock (uint32_t frameCount, double sampleRate,
                                   const MIDIOut& midiOut,
                                   float speedRatio, PlaybackDirection direction)
{
    // ── Lane sync: compute master phase once and pass to all lanes ────────────
    const bool syncNow = _lanesSynced.load (std::memory_order_relaxed);
    // Detect false→true transition: reset the master clock so all lanes snap
    // to a clean phase-0 start the moment sync is engaged.
    if (syncNow && ! _syncWasEnabled)
        _syncMasterPlayhead = 0.0;
    _syncWasEnabled = syncNow;

    float masterPhase = -1.0f;
    if (syncNow && _isPlaying.load (std::memory_order_acquire))
    {
        // Use the first valid lane's duration as the master period.
        double masterDur = 0.0;
        for (int i = 0; i < kMaxLanes; ++i)
        {
            const auto* s = _snapshots[static_cast<size_t>(i)].load (std::memory_order_acquire);
            if (s && s->valid)
            {
                masterDur = static_cast<double> (s->durationSeconds)
                            / static_cast<double> (std::max (speedRatio, 0.001f));
                break;
            }
        }
        if (masterDur > 0.0)
        {
            // PingPong cycles over twice the duration; everything else over masterDur.
            const double cycleDur = (direction == PlaybackDirection::PingPong)
                                    ? 2.0 * masterDur : masterDur;

            _syncMasterPlayhead += static_cast<double> (frameCount) / sampleRate;
            if (_syncMasterPlayhead >= cycleDur)
                _syncMasterPlayhead = std::fmod (_syncMasterPlayhead, cycleDur);

            if (direction == PlaybackDirection::Reverse)
            {
                masterPhase = 1.0f - static_cast<float> (_syncMasterPlayhead / masterDur);
            }
            else if (direction == PlaybackDirection::PingPong)
            {
                const double frac = _syncMasterPlayhead / masterDur;
                masterPhase = (frac <= 1.0) ? static_cast<float> (frac)
                                            : static_cast<float> (2.0 - frac);
            }
            else
            {
                masterPhase = static_cast<float> (_syncMasterPlayhead / masterDur);
            }
        }
    }

    for (int lane = 0; lane < kMaxLanes; ++lane)
        processLane (lane, frameCount, sampleRate, midiOut, speedRatio, direction, masterPhase);
}

void GestureEngine::processBlock (uint32_t frameCount, double sampleRate,
                                   const MIDIOut& midiOut,
                                   const std::array<float, kMaxLanes>& speedRatios,
                                   const std::array<PlaybackDirection, kMaxLanes>& directions)
{
    // ── Lane sync: same logic as the single-speed overload ────────────────────
    const bool syncNow = _lanesSynced.load (std::memory_order_relaxed);
    if (syncNow && ! _syncWasEnabled)
        _syncMasterPlayhead = 0.0;
    _syncWasEnabled = syncNow;

    float masterPhase = -1.0f;
    if (syncNow && _isPlaying.load (std::memory_order_acquire))
    {
        // Use the first valid lane's speed and direction as the master clock.
        double masterDur = 0.0;
        PlaybackDirection masterDir = PlaybackDirection::Forward;
        for (int i = 0; i < kMaxLanes; ++i)
        {
            const auto* s = _snapshots[static_cast<size_t>(i)].load (std::memory_order_acquire);
            if (s && s->valid)
            {
                masterDur = static_cast<double> (s->durationSeconds)
                            / static_cast<double> (std::max (speedRatios[static_cast<size_t>(i)], 0.001f));
                masterDir = directions[static_cast<size_t>(i)];
                break;
            }
        }
        if (masterDur > 0.0)
        {
            const double cycleDur = (masterDir == PlaybackDirection::PingPong)
                                    ? 2.0 * masterDur : masterDur;

            _syncMasterPlayhead += static_cast<double> (frameCount) / sampleRate;
            if (_syncMasterPlayhead >= cycleDur)
                _syncMasterPlayhead = std::fmod (_syncMasterPlayhead, cycleDur);

            if (masterDir == PlaybackDirection::Reverse)
                masterPhase = 1.0f - static_cast<float> (_syncMasterPlayhead / masterDur);
            else if (masterDir == PlaybackDirection::PingPong)
            {
                const double frac = _syncMasterPlayhead / masterDur;
                masterPhase = (frac <= 1.0) ? static_cast<float> (frac)
                                            : static_cast<float> (2.0 - frac);
            }
            else
                masterPhase = static_cast<float> (_syncMasterPlayhead / masterDur);
        }
    }

    for (int lane = 0; lane < kMaxLanes; ++lane)
        processLane (lane, frameCount, sampleRate, midiOut,
                     speedRatios[static_cast<size_t>(lane)],
                     directions[static_cast<size_t>(lane)],
                     masterPhase);
}
