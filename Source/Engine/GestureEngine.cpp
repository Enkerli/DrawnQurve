#include "GestureEngine.hpp"
#include <cmath>
#include <algorithm>

void GestureEngine::setSnapshot(const LaneSnapshot* snapshot) {
    _snapshot.store(snapshot, std::memory_order_release);
}

void GestureEngine::clearSnapshot() {
    _snapshot.store(nullptr, std::memory_order_release);
    _isPlaying.store(false, std::memory_order_release);
}

void GestureEngine::setPlaying(bool playing) {
    _isPlaying.store(playing, std::memory_order_release);
    if (!playing)
        _runtime.lastSentValue = -1;   // force re-send on next play
}

void GestureEngine::reset() {
    _runtime.playheadSeconds = 0.0;
    _runtime.lastSentValue   = -1;
    _runtime.smoothedValue   = 0.0f;
    _currentPhase.store(0.0f, std::memory_order_relaxed);
}

bool  GestureEngine::getPlaying()     const { return _isPlaying.load(std::memory_order_acquire); }
float GestureEngine::getCurrentPhase() const { return _currentPhase.load(std::memory_order_relaxed); }

float GestureEngine::sampleCurve(const LaneSnapshot& snap, float phase) const {
    float idx  = phase * 255.0f;
    int   i0   = (int)idx;
    int   i1   = (i0 + 1) & 255;
    float frac = idx - (float)i0;
    return snap.table[static_cast<size_t>(i0)]
         + frac * (snap.table[static_cast<size_t>(i1)]
                 - snap.table[static_cast<size_t>(i0)]);
}

void GestureEngine::processBlock(uint32_t frameCount, double sampleRate, const MIDIOut& midiOut,
                                  float speedRatio, PlaybackDirection direction) {
    const auto* snap = _snapshot.load(std::memory_order_acquire);
    if (!snap || !snap->valid || !_isPlaying.load(std::memory_order_acquire))
        return;

    // effectiveDuration shrinks as speedRatio increases (faster = shorter loop).
    const double effectiveDur = (double)snap->durationSeconds
                                / (double)std::max(speedRatio, 0.001f);

    // ── Advance playhead ──────────────────────────────────────────────────────
    _runtime.playheadSeconds += (double)frameCount / sampleRate;

    // ── Direction-dependent phase ─────────────────────────────────────────────
    float phase;
    if (direction == PlaybackDirection::Reverse)
    {
        if (_runtime.playheadSeconds >= effectiveDur)
            _runtime.playheadSeconds = std::fmod(_runtime.playheadSeconds, effectiveDur);
        phase = 1.0f - (float)(_runtime.playheadSeconds / effectiveDur);
    }
    else if (direction == PlaybackDirection::PingPong)
    {
        // Double-length window: 0→dur = forward half, dur→2*dur = reverse half.
        const double ppDur = 2.0 * effectiveDur;
        if (_runtime.playheadSeconds >= ppDur)
            _runtime.playheadSeconds = std::fmod(_runtime.playheadSeconds, ppDur);
        const double frac = _runtime.playheadSeconds / effectiveDur;  // 0..2
        phase = (frac <= 1.0) ? (float)frac : (float)(2.0 - frac);
    }
    else  // Forward (default)
    {
        if (_runtime.playheadSeconds >= effectiveDur)
            _runtime.playheadSeconds = std::fmod(_runtime.playheadSeconds, effectiveDur);
        phase = (float)(_runtime.playheadSeconds / effectiveDur);
    }

    const float target = sampleCurve(*snap, phase);
    _currentPhase.store(phase, std::memory_order_relaxed);

    // ── One-pole smoother (smoothing=0 → instant, smoothing=1 → ~2 s TC) ─────
    const float alpha = (snap->smoothing <= 0.0f)
        ? 1.0f
        : 1.0f - std::exp(-(float)frameCount / (snap->smoothing * 2.0f * (float)sampleRate));
    _runtime.smoothedValue += alpha * (target - _runtime.smoothedValue);

    // ── Emit MIDI ─────────────────────────────────────────────────────────────
    const float ranged = snap->minOut + _runtime.smoothedValue * (snap->maxOut - snap->minOut);

    switch (snap->messageType)
    {
        case MessageType::CC:
        {
            int v = std::clamp((int)std::lround(ranged * 127.0f), 0, 127);
            if (v != _runtime.lastSentValue) {
                if (midiOut) midiOut(0xB0u | (snap->midiChannel & 0x0Fu),
                                     snap->ccNumber, (uint8_t)v);
                _runtime.lastSentValue = v;
            }
            break;
        }
        case MessageType::ChannelPressure:
        {
            // 2-byte message — data2 is unused (pass 0; JUCE layer creates 2-byte MidiMessage)
            int v = std::clamp((int)std::lround(ranged * 127.0f), 0, 127);
            if (v != _runtime.lastSentValue) {
                if (midiOut) midiOut(0xD0u | (snap->midiChannel & 0x0Fu),
                                     (uint8_t)v, 0);
                _runtime.lastSentValue = v;
            }
            break;
        }
        case MessageType::PitchBend:
        {
            // 14-bit: 0=full down, 8192=centre, 16383=full up
            int v = std::clamp((int)std::lround(ranged * 16383.0f), 0, 16383);
            if (v != _runtime.lastSentValue) {
                if (midiOut) midiOut(0xE0u | (snap->midiChannel & 0x0Fu),
                                     (uint8_t)(v & 0x7F),           // LSB
                                     (uint8_t)((v >> 7) & 0x7F));   // MSB
                _runtime.lastSentValue = v;
            }
            break;
        }
    }
}
