#include "GestureEngine.hpp"
#include <cmath>
#include <algorithm>

// ---------------------------------------------------------------------------
// UI-thread methods
// ---------------------------------------------------------------------------

void GestureEngine::setSnapshot (const LaneSnapshot* snapshot)
{
    // release ordering: any writes to *snapshot by the UI thread are visible
    // to the audio thread after it loads this pointer with acquire ordering.
    _snapshot.store (snapshot, std::memory_order_release);
}

void GestureEngine::clearSnapshot()
{
    _snapshot.store  (nullptr, std::memory_order_release);
    _isPlaying.store (false,   std::memory_order_release);
}

void GestureEngine::setPlaying (bool playing)
{
    _isPlaying.store (playing, std::memory_order_release);

    if (! playing)
    {
        // Signal the audio thread to send a Note Off on the very next block.
        // We do NOT reset lastSentValue here: processBlock needs the current
        // pitch in order to emit the correct Note Off byte.
        _noteOffNeeded.store (true, std::memory_order_release);
    }
}

void GestureEngine::reset()
{
    // Cancel any pending note-off (host sync reset will restart cleanly).
    _noteOffNeeded.store (false, std::memory_order_relaxed);

    _runtime.playheadSeconds = 0.0;
    _runtime.lastSentValue   = -1;     // -1 forces re-send on next play (all modes)
    _runtime.lockedNote      = -1.0f;  // reset hysteresis state along with dedup
    _runtime.smoothedValue   = 0.0f;

    _currentPhase.store (0.0f, std::memory_order_relaxed);
}

bool  GestureEngine::getPlaying()     const { return _isPlaying.load (std::memory_order_acquire); }
float GestureEngine::getCurrentPhase() const { return _currentPhase.load (std::memory_order_relaxed); }

// ---------------------------------------------------------------------------
// Render-thread helpers
// ---------------------------------------------------------------------------

/// Bilinear interpolation into the 256-sample table at normalised phase [0, 1].
float GestureEngine::sampleCurve (const LaneSnapshot& snap, float phase) const
{
    const float idx  = phase * 255.0f;
    const int   i0   = static_cast<int> (idx);
    const int   i1   = (i0 + 1) & 255;   // wraps 255 → 0 (avoids out-of-bounds)
    const float frac = idx - static_cast<float> (i0);
    return snap.table[static_cast<size_t> (i0)]
         + frac * (snap.table[static_cast<size_t> (i1)]
                 - snap.table[static_cast<size_t> (i0)]);
}

// ---------------------------------------------------------------------------
// processBlock — render thread
// ---------------------------------------------------------------------------

void GestureEngine::processBlock (uint32_t frameCount, double sampleRate,
                                   const MIDIOut& midiOut,
                                   float speedRatio, PlaybackDirection direction)
{
    const auto* snap = _snapshot.load (std::memory_order_acquire);
    if (! snap || ! snap->valid)
        return;

    // ── Note Off cleanup path ─────────────────────────────────────────────────
    // _noteOffNeeded is set by setPlaying(false) on the UI thread.
    // exchange() clears the flag atomically so we handle it exactly once.
    if (_noteOffNeeded.exchange (false, std::memory_order_acq_rel))
    {
        if (snap->messageType == MessageType::Note
            && _runtime.lastSentValue >= 0
            && midiOut)
        {
            // Note Off (0x80) for the last active pitch, velocity 0.
            midiOut (0x80u | (snap->midiChannel & 0x0Fu),
                     static_cast<uint8_t> (_runtime.lastSentValue), 0u);
        }

        // Reset dedup and hysteresis state so the next play sends a fresh event.
        _runtime.lastSentValue = -1;
        _runtime.lockedNote    = -1.0f;
    }

    if (! _isPlaying.load (std::memory_order_acquire))
        return;

    // effectiveDur shrinks as speedRatio increases (2× speed → half duration).
    const double effectiveDur = static_cast<double> (snap->durationSeconds)
                                / static_cast<double> (std::max (speedRatio, 0.001f));

    // ── Advance playhead ──────────────────────────────────────────────────────
    _runtime.playheadSeconds += static_cast<double> (frameCount) / sampleRate;

    // ── Direction-dependent phase [0, 1] ──────────────────────────────────────
    // Forward / Reverse wrap the playhead modulo effectiveDur.
    // PingPong uses a 2× window: first half is forward, second half is reversed.
    float phase;

    if (direction == PlaybackDirection::Reverse)
    {
        if (_runtime.playheadSeconds >= effectiveDur)
            _runtime.playheadSeconds = std::fmod (_runtime.playheadSeconds, effectiveDur);

        // Read table backwards: phase=0 → table[255], phase=1 → table[0].
        phase = 1.0f - static_cast<float> (_runtime.playheadSeconds / effectiveDur);
    }
    else if (direction == PlaybackDirection::PingPong)
    {
        // Double-length window: [0, dur) = forward half, [dur, 2*dur) = reverse half.
        const double ppDur = 2.0 * effectiveDur;
        if (_runtime.playheadSeconds >= ppDur)
            _runtime.playheadSeconds = std::fmod (_runtime.playheadSeconds, ppDur);

        const double frac = _runtime.playheadSeconds / effectiveDur;  // 0..2
        // frac ≤ 1 → forward (phase = frac);  frac > 1 → reverse (phase = 2 - frac).
        phase = (frac <= 1.0) ? static_cast<float> (frac)
                              : static_cast<float> (2.0 - frac);
    }
    else  // Forward (default)
    {
        if (_runtime.playheadSeconds >= effectiveDur)
            _runtime.playheadSeconds = std::fmod (_runtime.playheadSeconds, effectiveDur);

        phase = static_cast<float> (_runtime.playheadSeconds / effectiveDur);
    }

    const float target = sampleCurve (*snap, phase);
    _currentPhase.store (phase, std::memory_order_relaxed);   // approximate — UI display only

    // ── One-pole low-pass smoother ────────────────────────────────────────────
    // Equivalent to an RC filter with time-constant τ = smoothing * 2 * sampleRate samples.
    // smoothing = 0.0 → alpha = 1.0 → passthrough (no lag)
    // smoothing = 1.0 → alpha ≈ 0.0 → very slow response (~2 s at 44.1 kHz)
    const float alpha = (snap->smoothing <= 0.0f)
        ? 1.0f
        : 1.0f - std::exp (- static_cast<float> (frameCount)
                           / (snap->smoothing * 2.0f * static_cast<float> (sampleRate)));

    _runtime.smoothedValue += alpha * (target - _runtime.smoothedValue);

    // ── Map smooth value through output range ─────────────────────────────────
    // ranged ∈ [minOut, maxOut]  (both normalised 0..1)
    const float ranged = snap->minOut + _runtime.smoothedValue * (snap->maxOut - snap->minOut);

    // ── Emit MIDI ─────────────────────────────────────────────────────────────
    // All modes use value deduplication (only send on change) to avoid flooding
    // the MIDI bus with identical messages every block.

    switch (snap->messageType)
    {
        case MessageType::CC:
        {
            // 7-bit value [0, 127].
            const int v = std::clamp (static_cast<int> (std::lround (ranged * 127.0f)), 0, 127);
            if (v != _runtime.lastSentValue)
            {
                if (midiOut)
                    midiOut (0xB0u | (snap->midiChannel & 0x0Fu),
                             snap->ccNumber,
                             static_cast<uint8_t> (v));
                _runtime.lastSentValue = v;
            }
            break;
        }

        case MessageType::ChannelPressure:
        {
            // 2-byte message — data2 unused (JUCE detects 0xD0 and creates a 2-byte MidiMessage).
            const int v = std::clamp (static_cast<int> (std::lround (ranged * 127.0f)), 0, 127);
            if (v != _runtime.lastSentValue)
            {
                if (midiOut)
                    midiOut (0xD0u | (snap->midiChannel & 0x0Fu),
                             static_cast<uint8_t> (v), 0u);
                _runtime.lastSentValue = v;
            }
            break;
        }

        case MessageType::PitchBend:
        {
            // 14-bit unsigned [0, 16383]; 8192 = centre (no bend).
            // Split into 7-bit LSB (data1) and 7-bit MSB (data2).
            const int v = std::clamp (static_cast<int> (std::lround (ranged * 16383.0f)), 0, 16383);
            if (v != _runtime.lastSentValue)
            {
                if (midiOut)
                    midiOut (0xE0u | (snap->midiChannel & 0x0Fu),
                             static_cast<uint8_t> (v & 0x7F),          // LSB
                             static_cast<uint8_t> ((v >> 7) & 0x7F));  // MSB
                _runtime.lastSentValue = v;
            }
            break;
        }

        case MessageType::Note:
        {
            // Vertical position maps to MIDI pitch [0, 127].
            // When the pitch changes: send Note Off for the old pitch,
            // then Note On for the new one (monophonic glide / pitch scan).
            //
            // Hysteresis: the curve value is compared against _runtime.lockedNote
            // (a float, not yet quantised to a semitone).  A pitch change is only
            // committed when the raw value has moved at least kNoteHysteresis
            // semitones away from the locked value.  This prevents rapid alternating
            // Note Off/On bursts when the curve hovers near a semitone boundary.
            constexpr float kNoteHysteresis = 0.6f;   // semitones; < 1.0 so a clean
                                                       // step still triggers immediately

            const float rawNote = ranged * 127.0f;

            if (_runtime.lockedNote < 0.0f                                    // nothing playing yet
                || std::fabs (rawNote - _runtime.lockedNote) >= kNoteHysteresis)
            {
                _runtime.lockedNote = rawNote;   // commit the new pitch position
            }

            const int note = std::clamp (static_cast<int> (std::lround (_runtime.lockedNote)), 0, 127);
            if (note != _runtime.lastSentValue)
            {
                if (midiOut)
                {
                    // Note Off for previous pitch (if any).
                    if (_runtime.lastSentValue >= 0)
                        midiOut (0x80u | (snap->midiChannel & 0x0Fu),
                                 static_cast<uint8_t> (_runtime.lastSentValue), 0u);

                    // Note On for new pitch.
                    midiOut (0x90u | (snap->midiChannel & 0x0Fu),
                             static_cast<uint8_t> (note),
                             snap->noteVelocity);
                }
                _runtime.lastSentValue = note;
            }
            break;
        }
    }
}
