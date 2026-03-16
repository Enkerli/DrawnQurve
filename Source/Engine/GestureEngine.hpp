#pragma once
#include "LaneSnapshot.hpp"
#include <atomic>
#include <functional>
#include <cstdint>

/**
 * @file GestureEngine.hpp
 *
 * Real-time, lock-minimised MIDI playback engine.
 *
 * Architecture
 * ────────────
 * GestureEngine loops a LaneSnapshot continuously, sampling the 256-point
 * curve table at the current playhead position and emitting MIDI events via
 * a caller-supplied callback.
 *
 * Thread safety
 * ─────────────
 * - setSnapshot / clearSnapshot / setPlaying are called from the **UI thread**.
 * - processBlock is called from the **audio thread** (or the fallback HiRes
 *   timer thread — never both simultaneously, guarded by a SpinLock in the
 *   processor).
 * - All cross-thread communication uses std::atomic with explicit ordering.
 *
 * Ownership
 * ─────────
 * The engine holds a raw pointer to the current LaneSnapshot.  The caller
 * allocates snapshots with `new` and must NOT delete them while the engine
 * may be referencing them.  (MVP: snapshots are never deleted.)
 */

// ---------------------------------------------------------------------------
/// Per-lane runtime state — lives entirely on the render thread.
struct LaneRuntime
{
    double playheadSeconds = 0.0;   ///< Elapsed playback time within the current loop period
    int    lastSentValue   = -1;    ///< Last emitted value (-1 = nothing sent yet); for dedup + Note Off
    float  smoothedValue   = 0.0f;  ///< One-pole smoother state
};

// ---------------------------------------------------------------------------
/**
 * Real-time-safe MIDI CC / Pressure / PitchBend / Note playback engine.
 *
 * Typical usage (from DrawnCurveProcessor):
 * @code
 *   engine.setSnapshot(snap);       // UI thread: load new curve
 *   engine.setPlaying(true);        // UI thread: start looping
 *   engine.processBlock(n, sr, cb, speed, dir); // audio thread: advance + emit MIDI
 * @endcode
 */
class GestureEngine
{
public:
    /// Callback type for emitting a single MIDI message.
    /// Called synchronously from processBlock on the render thread.
    using MIDIOut = std::function<void(uint8_t status, uint8_t data1, uint8_t data2)>;

    // ── UI-thread API ─────────────────────────────────────────────────────────

    /// Load a new curve.  The engine immediately starts using it on the next
    /// processBlock call (atomic store with release ordering).
    void setSnapshot (const LaneSnapshot* snapshot);

    /// Clear the current snapshot and stop playback.
    void clearSnapshot();

    /// Start or stop looping.
    /// On stop: sets _noteOffNeeded so processBlock can emit Note Off on the
    /// very next audio block — prevents stuck notes.
    void setPlaying (bool playing);

    /// Reset the playhead to 0 and clear smoother state.
    /// Call before starting fresh playback (e.g. on a sync rising edge).
    void reset();

    // ── Query API (UI or render thread) ──────────────────────────────────────

    bool  getPlaying()      const;   ///< true if currently looping
    float getCurrentPhase() const;   ///< Approximate 0..1 playhead, for UI display only

    // ── Render-thread API ─────────────────────────────────────────────────────

    /**
     * Advance the playhead by @p frameCount frames and emit MIDI via @p midiOut.
     *
     * @param frameCount  Audio buffer size in samples.
     * @param sampleRate  Current sample rate (Hz).
     * @param midiOut     Callback for each MIDI event generated this block.
     * @param speedRatio  >1 = faster (shorter loop); <1 = slower (longer loop).
     *                    Computed from either the Speed slider or BPM sync.
     * @param direction   Forward / Reverse / PingPong.
     *
     * No-op if no valid snapshot is loaded or playback is stopped.
     * Sends a Note Off first if _noteOffNeeded is set (guarded internally).
     */
    void processBlock (uint32_t frameCount, double sampleRate, const MIDIOut& midiOut,
                       float speedRatio = 1.0f,
                       PlaybackDirection direction = PlaybackDirection::Forward);

private:
    // ── Atomic cross-thread state ─────────────────────────────────────────────
    std::atomic<const LaneSnapshot*> _snapshot     { nullptr }; ///< Current curve (set on UI thread, read on audio thread)
    std::atomic<bool>                _isPlaying     { false   }; ///< Playback running?
    std::atomic<float>               _currentPhase  { 0.0f   }; ///< Written by audio thread, read by UI (display only)
    std::atomic<bool>                _noteOffNeeded { false   }; ///< Set by setPlaying(false), cleared by processBlock

    // ── Render-thread-only state ──────────────────────────────────────────────
    LaneRuntime _runtime;   ///< Playhead + smoother (render thread only — no synchronisation needed)

    /// Linear interpolation into the 256-sample lookup table at [0,1] phase.
    float sampleCurve (const LaneSnapshot& snap, float phase) const;
};
