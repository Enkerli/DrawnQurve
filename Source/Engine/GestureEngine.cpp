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
            _runtimes[i].lockedNote    = -1.0f;
        }
        _runtimes[i].playheadSeconds = 0.0;
        _runtimes[i].smoothedValue   = 0.0f;
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
    // Rewind playhead and smoother.  Do NOT touch _noteOffNeeded or lastSentValue
    // so any pending Note Off still fires correctly in processLane.
    _runtimes[lane].playheadSeconds = 0.0;
    _runtimes[lane].smoothedValue   = 0.0f;
    if (lane == 0)
        _currentPhase.store (0.0f, std::memory_order_relaxed);
}

void GestureEngine::setScaleConfig (int lane, ScaleConfig config)
{
    if (lane < 0 || lane >= kMaxLanes) return;
    _scalesPacked[lane].store (packScale (config), std::memory_order_release);
}

bool  GestureEngine::getPlaying()      const { return _isPlaying.load (std::memory_order_acquire); }
float GestureEngine::getCurrentPhase() const { return _currentPhase.load (std::memory_order_relaxed); }

// ---------------------------------------------------------------------------
// Static utilities
// ---------------------------------------------------------------------------

int GestureEngine::quantizeNote (int rawNote, ScaleConfig sc, bool movingUp)
{
    if (sc.mask == 0xFFF) return rawNote;

    rawNote = std::clamp (rawNote, 0, 127);

    const int pc       = rawNote % 12;
    const int interval = (pc - (int)sc.root + 12) % 12;

    if ((sc.mask >> interval) & 1) return rawNote;

    int downNote = -1, upNote = -1;

    for (int d = 1; d <= 6; ++d)
    {
        if (downNote < 0 && rawNote - d >= 0)
        {
            const int di = (interval - d + 12) % 12;
            if ((sc.mask >> di) & 1)
                downNote = rawNote - d;
        }
        if (upNote < 0 && rawNote + d <= 127)
        {
            const int ui = (interval + d) % 12;
            if ((sc.mask >> ui) & 1)
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
        rt.lockedNote    = -1.0f;
    }

    if (! snap || ! snap->valid) return;
    if (! _isPlaying.load (std::memory_order_acquire)) return;

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

    // Store phase from lane 0 (or first valid lane) for the UI playhead display.
    if (lane == 0)
        _currentPhase.store (phase, std::memory_order_relaxed);

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
            // Hysteresis: commit a new pitch only when the curve has moved ≥ 0.6 semitones
            // away from the currently locked value.  Prevents rapid Note Off/On bursts
            // when the curve hovers near a semitone boundary.
            constexpr float kNoteHysteresis = 0.6f;
            const float rawNoteF = ranged * 127.0f;
            if (rt.lockedNote < 0.0f
                || std::fabs (rawNoteF - rt.lockedNote) >= kNoteHysteresis)
                rt.lockedNote = rawNoteF;

            const int rawNote = std::clamp (static_cast<int> (std::lround (rt.lockedNote)), 0, 127);
            const ScaleConfig sc = unpackScale (_scalesPacked[lane].load (std::memory_order_acquire));
            const bool movingUp  = (rt.lastSentValue < 0) || (rawNote >= rt.lastSentValue);
            const int  note      = quantizeNote (rawNote, sc, movingUp);

            if (note != rt.lastSentValue)
            {
                if (midiOut)
                {
                    if (rt.lastSentValue >= 0)
                        midiOut (0x80u | (snap->midiChannel & 0x0Fu),
                                 static_cast<uint8_t> (rt.lastSentValue), 0u);
                    midiOut (0x90u | (snap->midiChannel & 0x0Fu),
                             static_cast<uint8_t> (note), snap->noteVelocity);
                }
                rt.lastSentValue = note;
            }
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
