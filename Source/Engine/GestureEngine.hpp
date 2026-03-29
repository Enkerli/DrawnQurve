#pragma once
#include "LaneSnapshot.hpp"
#include <atomic>
#include <functional>
#include <array>
#include <cstdint>

/**
 * @file GestureEngine.hpp
 *
 * Real-time, lock-minimised MIDI playback engine.
 *
 * Threading
 * ─────────
 * UI thread  : setSnapshot / clearSnapshot / setPlaying / setScaleConfig / setLaneEnabled
 * Audio thread: processBlock (or fallback HiRes timer — never both at once)
 * All cross-thread state uses std::atomic with explicit ordering.
 *
 * Multi-lane
 * ──────────
 * Up to kMaxLanes lanes play simultaneously.  Each lane has its own snapshot,
 * runtime state, and scale config.  Playback (isPlaying) and the global phase
 * display (currentPhase) are shared across all lanes so they stay in sync.
 * Each lane's curve advances its own playheadSeconds using the common speed
 * ratio, so lanes recorded at different durations loop at their natural rates.
 */

static constexpr int kMaxLanes = 4;

// ---------------------------------------------------------------------------
/**
 * Scale quantization configuration.
 *
 * mask  — 12-bit interval pattern, root-relative.
 *         Bit 0 = root is active, bit 1 = root+1 semitone active, …, bit 11 = root+11.
 *         0xFFF = chromatic (all notes) = no quantization.
 *
 * root  — root pitch class (0=C, 1=C#/Db, …, 11=B).
 */
struct ScaleConfig
{
    uint16_t mask { 0xFFF };   ///< Root-relative 12-bit interval mask; 0xFFF = chromatic
    uint8_t  root { 0     };   ///< Root pitch class (0=C … 11=B)
};

// ---------------------------------------------------------------------------
/// Per-lane runtime state — render thread only.
struct LaneRuntime
{
    double playheadSeconds = 0.0;   ///< Elapsed playback time within the current loop period
    int    lastSentValue   = -1;    ///< Last emitted value (-1 = nothing sent yet); for dedup + Note Off
    float  smoothedValue   = 0.0f;  ///< One-pole smoother state
    int    lastXTick       = -1;    ///< Last committed X-grid tick index (for xQuantize dedup; -1 = none)
};

// ---------------------------------------------------------------------------
/**
 * Real-time-safe MIDI playback engine supporting up to kMaxLanes lanes.
 */
class GestureEngine
{
public:
    using MIDIOut = std::function<void(uint8_t status, uint8_t data1, uint8_t data2)>;

    GestureEngine();

    // ── UI-thread API ─────────────────────────────────────────────────────────
    void setSnapshot    (int lane, const LaneSnapshot* snapshot);
    void clearSnapshot  (int lane);
    void clearAllSnapshots();
    void setPlaying     (bool playing);
    void reset          ();

    /// Reset all lanes and seed each smoother from the correct starting phase
    /// for the given direction.  Use this in host-sync start events so that
    /// Reverse/PingPong begin at their correct curve position without a
    /// "glide from zero" artefact caused by the standard reset()'s cold-start.
    void resetForDirection (PlaybackDirection dir);

    /// Send a Note Off for one lane (sets _noteOffNeeded) without stopping other lanes.
    /// Call from beginCapture() on the UI thread before drawing a new curve.
    void stopLane       (int lane);

    /// Rewind one lane's playhead and smoother without clearing its note-off flag
    /// or lastSentValue.  Call from finalizeCapture() after loading a new snapshot.
    void resetLane      (int lane);

    /// Update scale quantization config for one lane atomically.
    void setScaleConfig (int lane, ScaleConfig config);

    /// Mute/unmute a lane.  When transitioning from enabled→disabled, queues a
    /// Note Off for any held note so playback on that lane silences cleanly.
    /// Safe to call from the render thread (processBlock) on every block.
    void setLaneEnabled (int lane, bool enabled);

    /// Pause / resume an individual lane without affecting other lanes.
    /// On pause the lane's Note Off is queued immediately; on resume playback
    /// continues from the same playhead position.
    void setLanePaused (int lane, bool paused);
    bool getLanePaused (int lane) const noexcept;

    /// Lock all looping lanes to the same normalized phase (0..1).
    /// When enabled, the first valid lane acts as the phase master;
    /// all other looping lanes sample their curves at the same phase.
    /// One-shot lanes are unaffected and still run their own playheads.
    void setLanesSynced (bool synced);
    bool getLanesSynced () const noexcept;

    // ── Query API (UI or render thread) ──────────────────────────────────────
    bool  getPlaying()      const;
    /// Phase of the first valid playing lane — for backward-compat UI use.
    float getCurrentPhase() const;
    /// Per-lane phase (0-1); returns 0 if the lane has no valid snapshot.
    float getCurrentPhaseForLane (int lane) const;

    // ── Render-thread API ─────────────────────────────────────────────────────
    /**
     * Advance all lane playheads and emit MIDI.
     * @param speedRatio  >1 = faster; <1 = slower.  Applied equally to all lanes.
     * @param direction   Forward / Reverse / PingPong.
     */
    void processBlock (uint32_t frameCount, double sampleRate, const MIDIOut& midiOut,
                       float speedRatio = 1.0f,
                       PlaybackDirection direction = PlaybackDirection::Forward);

    /// Per-lane overload: each lane gets its own speed multiplier and direction.
    void processBlock (uint32_t frameCount, double sampleRate, const MIDIOut& midiOut,
                       const std::array<float, kMaxLanes>& speedRatios,
                       const std::array<PlaybackDirection, kMaxLanes>& directions);

    // ── Utility (also called from UI for Y-axis display) ─────────────────────
    static int quantizeNote (int rawNote, ScaleConfig sc, bool movingUp);

private:
    std::array<std::atomic<const LaneSnapshot*>, kMaxLanes> _snapshots;
    std::array<std::atomic<bool>,                kMaxLanes> _noteOffNeeded;
    std::array<std::atomic<uint32_t>,            kMaxLanes> _scalesPacked;
    std::array<std::atomic<bool>,                kMaxLanes> _laneEnabled;   ///< false = muted
    std::array<std::atomic<bool>,                kMaxLanes> _lanePaused;    ///< true = lane individually paused

    std::atomic<bool>  _isPlaying    { false };
    std::atomic<float> _currentPhase { 0.0f  };   ///< First valid lane's phase (compat)
    std::array<std::atomic<float>, kMaxLanes> _lanePhases;  ///< Per-lane phase 0..1

    std::atomic<bool> _lanesSynced  { false };

    std::array<LaneRuntime, kMaxLanes> _runtimes;   ///< Render-thread only

    /// Render-thread-only master clock used when _lanesSynced is true.
    double _syncMasterPlayhead  { 0.0 };
    /// Render-thread-only: tracks previous sync state so we can reset the
    /// master playhead cleanly the moment sync is first enabled.
    bool   _syncWasEnabled      { false };

    float sampleCurve (const LaneSnapshot& snap, float phase) const;

    /// @param forcedPhase  When >= 0, looping lanes use this phase instead of their own
    ///                     playheadSeconds-derived phase.  One-shot lanes always use their own.
    void processLane (int lane, uint32_t frameCount, double sampleRate,
                      const MIDIOut& midiOut,
                      float speedRatio, PlaybackDirection direction,
                      float forcedPhase = -1.0f);

    static uint32_t    packScale   (ScaleConfig s) noexcept { return (uint32_t(s.root) << 12) | s.mask; }
    static ScaleConfig unpackScale (uint32_t p)    noexcept { return { uint16_t(p & 0xFFF), uint8_t(p >> 12) }; }
};
