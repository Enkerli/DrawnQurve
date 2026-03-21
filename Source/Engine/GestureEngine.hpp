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
 * UI thread  : setSnapshot / clearSnapshot / setPlaying / setScaleConfig
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

static constexpr int kMaxLanes = 3;

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

    /// Note-mode hysteresis: the most recently committed pitch as a continuous
    /// float (0–127 scale, -1 = no note active).  A new pitch is committed only
    /// when the raw computed note has moved at least kNoteHysteresis semitones
    /// away from this value, preventing rapid alternation when the curve hovers
    /// at a semitone boundary.
    float  lockedNote      = -1.0f;
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

    /// Update scale quantization config for one lane atomically.
    void setScaleConfig (int lane, ScaleConfig config);

    // ── Query API (UI or render thread) ──────────────────────────────────────
    bool  getPlaying()      const;
    /// Phase of lane 0 (or the first valid lane) — for UI playhead display.
    float getCurrentPhase() const;

    // ── Render-thread API ─────────────────────────────────────────────────────
    /**
     * Advance all lane playheads and emit MIDI.
     * @param speedRatio  >1 = faster; <1 = slower.  Applied equally to all lanes.
     * @param direction   Forward / Reverse / PingPong.
     */
    void processBlock (uint32_t frameCount, double sampleRate, const MIDIOut& midiOut,
                       float speedRatio = 1.0f,
                       PlaybackDirection direction = PlaybackDirection::Forward);

    // ── Utility (also called from UI for Y-axis display) ─────────────────────
    static int quantizeNote (int rawNote, ScaleConfig sc, bool movingUp);

private:
    std::array<std::atomic<const LaneSnapshot*>, kMaxLanes> _snapshots;
    std::array<std::atomic<bool>,                kMaxLanes> _noteOffNeeded;
    std::array<std::atomic<uint32_t>,            kMaxLanes> _scalesPacked;

    std::atomic<bool>  _isPlaying    { false };
    std::atomic<float> _currentPhase { 0.0f  };

    std::array<LaneRuntime, kMaxLanes> _runtimes;   ///< Render-thread only

    float sampleCurve (const LaneSnapshot& snap, float phase) const;

    void processLane (int lane, uint32_t frameCount, double sampleRate,
                      const MIDIOut& midiOut,
                      float speedRatio, PlaybackDirection direction);

    static uint32_t    packScale   (ScaleConfig s) noexcept { return (uint32_t(s.root) << 12) | s.mask; }
    static ScaleConfig unpackScale (uint32_t p)    noexcept { return { uint16_t(p & 0xFFF), uint8_t(p >> 12) }; }
};
