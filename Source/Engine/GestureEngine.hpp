#pragma once
#include "LaneSnapshot.hpp"
#include <atomic>
#include <functional>
#include <cstdint>

struct LaneRuntime {
    double playheadSeconds = 0.0;
    int    lastSentValue   = -1;   // CC: 0-127  PitchBend: 0-16383  resets on stop
    float  smoothedValue   = 0.0f;
};

// Real-time-safe MIDI CC playback engine.
// Snapshot ownership: caller allocates with `new`; old snapshots are never
// deleted (MVP policy — each snapshot is ~1 KB, negligible leak per session).
class GestureEngine {
public:
    using MIDIOut = std::function<void(uint8_t status, uint8_t data1, uint8_t data2)>;

    // Called from UI thread.
    void setSnapshot(const LaneSnapshot* snapshot);
    void clearSnapshot();
    void setPlaying(bool playing);
    void reset();

    // Queries (UI or render thread).
    bool  getPlaying() const;
    float getCurrentPhase() const;     // approximate, for UI display only

    // Called from render thread.
    // speedRatio > 1 = faster (shorter loop); < 1 = slower (longer loop).
    void processBlock(uint32_t frameCount, double sampleRate, const MIDIOut& midiOut,
                      float speedRatio = 1.0f);

private:
    std::atomic<const LaneSnapshot*> _snapshot{nullptr};
    std::atomic<bool>  _isPlaying{false};
    std::atomic<float> _currentPhase{0.0f};

    LaneRuntime _runtime; // render-thread only

    float sampleCurve(const LaneSnapshot& snap, float phase) const;
};
