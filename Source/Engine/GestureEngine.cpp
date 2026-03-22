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
        _snapshots[i].store    (nullptr, std::memory_order_relaxed);
        _noteOffNeeded[i].store(false,   std::memory_order_relaxed);
        _scalesPacked[i].store (0xFFF,   std::memory_order_relaxed);
        _laneEnabled[i].store  (true,    std::memory_order_relaxed);
        _lanePhases[i].store   (0.0f,    std::memory_order_relaxed);
    }
}

// ---------------------------------------------------------------------------
// UI-thread methods
// ---------------------------------------------------------------------------

void GestureEngine::setSnapshot (int lane, const LaneSnapshot* snapshot)
{
    if (lane < 0 || lane >= kMaxLanes) return;
    _snapshots[lane].store (snapshot, std::memory_order_release);
}

void GestureEngine::clearSnapshot (int lane)
{
    if (lane < 0 || lane >= kMaxLanes) return;
    _snapshots[lane].store     (nullptr, std::memory_order_release);
    _noteOffNeeded[lane].store (true,    std::memory_order_release);
}

void GestureEngine::clearAllSnapshots()
{
    for (int i = 0; i < kMaxLanes; ++i)
    {
        _snapshots[i].store    (nullptr, std::memory_order_release);
        _noteOffNeeded[i].store(true,    std::memory_order_release);
    }
    _isPlaying.store (false, std::memory_order_release);
}

void GestureEngine::setPlaying (bool playing)
{
    _isPlaying.store (playing, std::memory_order_release);
    if (! playing)
        for (int i = 0; i < kMaxLanes; ++i)
            _noteOffNeeded[i].store (true, std::memory_order_release);
}

void GestureEngine::reset()
{
    for (int i = 0; i < kMaxLanes; ++i)
    {
        // Don't clear _noteOffNeeded here — let processLane send the Note Off
        // on the next block before starting the new curve.  Only reset dedup
        // state when no note-off is already pending.
        if (! _noteOffNeeded[i].load (std::memory_order_acquire))
        {
            _runtimes[i].lastSentValue = -1;
        }
        _runtimes[i].playheadSeconds = 0.0;
        _runtimes[i].smoothedValue   = 0.0f;
        _lanePhases[i].store (0.0f, std::memory_order_relaxed);
    }

    _currentPhase.store (0.0f, std::memory_order_relaxed);
}

void GestureEngine::stopLane (int lane)
{
    if (lane < 0 || lane >= kMaxLanes) return;
    // Signal processLane to send Note Off on the next processBlock call.
    // Does not stop playback on other lanes.
    _noteOffNeeded[lane].store (true, std::memory_order_release);
}

void GestureEngine::resetLane (int lane)
{
    if (lane < 0 || lane >= kMaxLanes) return;
    // Rewind playhead.  Do NOT touch _noteOffNeeded or lastSentValue
    // so any pending Note Off still fires correctly in processLane.
    _runtimes[lane].playheadSeconds = 0.0;

    // Pre-seed the smoother from the curve's value at phase 0 so that
    // playback begins at the correct pitch/value without glissanding up
    // from silence (which happened when smoothedValue started at 0.0).
    const auto* snap = _snapshots[lane].load (std::memory_order_acquire);
    _runtimes[lane].smoothedValue = (snap && snap->valid) ? sampleCurve (*snap, 0.0f) : 0.0f;

    _lanePhases[lane].store (0.0f, std::memory_order_relaxed);
}

void GestureEngine::setLaneEnabled (int lane, bool enabled)
{
    if (lane < 0 || lane >= kMaxLanes) return;
    // Detect enabled→disabled transition and queue a Note Off for any held note.
    // It is safe to call this on every processBlock iteration since the exchange
    // only triggers stopLane on the false edge.
    const bool wasEnabled = _laneEnabled[lane].exchange (enabled, std::memory_order_acq_rel);
    if (wasEnabled && ! enabled)
        stopLane (lane);    // sets _noteOffNeeded; fires in the current processLane call
}

void GestureEngine::setScaleConfig (int lane, ScaleConfig config)
{
    if (lane < 0 || lane >= kMaxLanes) return;
    _scalesPacked[lane].store (packScale (config), std::memory_order_release);
}

bool  GestureEngine::getPlaying()      const { return _isPlaying.load (std::memory_order_acquire); }
float GestureEngine::getCurrentPhase() const { return _currentPhase.load (std::memory_order_relaxed); }

float GestureEngine::getCurrentPhaseForLane (int lane) const
{
    if (lane < 0 || lane >= kMaxLanes) return 0.0f;
    return _lanePhases[lane].load (std::memory_order_relaxed);
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
                                  float speedRatio, PlaybackDirection direction)
{
    auto& rt  = _runtimes[lane];
    const auto* snap = _snapshots[lane].load (std::memory_order_acquire);

    // ── Note Off cleanup (may fire even without a valid snapshot) ─────────────
    if (_noteOffNeeded[lane].exchange (false, std::memory_order_acq_rel))
    {
        if (snap && snap->valid
            && snap->messageType == MessageType::Note
            && rt.lastSentValue >= 0 && midiOut)
        {
            midiOut (0x80u | (snap->midiChannel & 0x0Fu),
                     static_cast<uint8_t> (rt.lastSentValue), 0u);
        }
        rt.lastSentValue = -1;
    }

    if (! snap || ! snap->valid) return;
    if (! _isPlaying.load (std::memory_order_acquire)) return;
    if (! _laneEnabled[lane].load (std::memory_order_acquire)) return;   // muted

    const double effectiveDur = static_cast<double> (snap->durationSeconds)
                                / static_cast<double> (std::max (speedRatio, 0.001f));

    // ── Advance playhead ──────────────────────────────────────────────────────
    rt.playheadSeconds += static_cast<double> (frameCount) / sampleRate;

    // ── Phase (direction-dependent) ───────────────────────────────────────────
    float phase;
    if (direction == PlaybackDirection::Reverse)
    {
        if (rt.playheadSeconds >= effectiveDur)
            rt.playheadSeconds = std::fmod (rt.playheadSeconds, effectiveDur);
        phase = 1.0f - static_cast<float> (rt.playheadSeconds / effectiveDur);
    }
    else if (direction == PlaybackDirection::PingPong)
    {
        const double ppDur = 2.0 * effectiveDur;
        if (rt.playheadSeconds >= ppDur)
            rt.playheadSeconds = std::fmod (rt.playheadSeconds, ppDur);
        const double frac = rt.playheadSeconds / effectiveDur;
        phase = (frac <= 1.0) ? static_cast<float> (frac)
                              : static_cast<float> (2.0 - frac);
    }
    else  // Forward
    {
        if (rt.playheadSeconds >= effectiveDur)
            rt.playheadSeconds = std::fmod (rt.playheadSeconds, effectiveDur);
        phase = static_cast<float> (rt.playheadSeconds / effectiveDur);
    }

    // Store per-lane phase for the UI playhead.
    _lanePhases[lane].store (phase, std::memory_order_relaxed);

    // Also update _currentPhase from the lowest-indexed valid lane so the
    // CurveDisplay always shows a moving playhead when anything is playing.
    {
        bool lowerValid = false;
        for (int l = 0; l < lane; ++l)
        {
            const auto* s = _snapshots[l].load (std::memory_order_relaxed);
            if (s && s->valid) { lowerValid = true; break; }
        }
        if (! lowerValid)
            _currentPhase.store (phase, std::memory_order_relaxed);
    }

    const float target = sampleCurve (*snap, phase);

    // ── One-pole smoother ─────────────────────────────────────────────────────
    const float alpha = (snap->smoothing <= 0.0f)
        ? 1.0f
        : 1.0f - std::exp (- static_cast<float> (frameCount)
                           / (snap->smoothing * 2.0f * static_cast<float> (sampleRate)));
    rt.smoothedValue += alpha * (target - rt.smoothedValue);

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

            const float rawNoteF  = (snap->minOut + target * (snap->maxOut - snap->minOut)) * 127.0f;
            const int   committed = rt.lastSentValue;
            const ScaleConfig sc  = unpackScale (_scalesPacked[lane].load (std::memory_order_acquire));

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
                if (committed >= 0)
                    midiOut (0x80u | (snap->midiChannel & 0x0Fu),
                             static_cast<uint8_t> (committed), 0u);
                midiOut (0x90u | (snap->midiChannel & 0x0Fu),
                         static_cast<uint8_t> (candidate), snap->noteVelocity);
            }
            rt.lastSentValue = candidate;
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
    for (int lane = 0; lane < kMaxLanes; ++lane)
        processLane (lane, frameCount, sampleRate, midiOut, speedRatio, direction);
}
