#pragma once
#include <array>
#include <cstdint>

/**
 * @file LaneSnapshot.hpp
 *
 * Plain-data types shared between the capture pipeline and the render engine.
 * No JUCE headers are included here so the engine can be unit-tested stand-alone.
 *
 * Data flow overview:
 *   GestureCaptureSession  →  LaneSnapshot  →  GestureEngine  →  MIDI out
 *
 * A LaneSnapshot is created once per recording gesture (via
 * GestureCaptureSession::finalize()) and handed to the engine via
 * GestureEngine::setSnapshot().  It is immutable after creation.
 */

// ---------------------------------------------------------------------------
/// Which MIDI message type the engine emits on each loop iteration.
enum class MessageType : uint8_t
{
    CC              = 0,   ///< Control Change   (0xB0) — data1=ccNumber, data2=value 0-127
    ChannelPressure = 1,   ///< Channel Pressure (0xD0) — data1=value 0-127 (2-byte message)
    PitchBend       = 2,   ///< Pitch Bend       (0xE0) — 14-bit unsigned; centre = 8192
    Note            = 3,   ///< Note On/Off      (0x90/0x80) — Y axis maps to pitch 0-127
};

// ---------------------------------------------------------------------------
/// Loop playback direction.
enum class PlaybackDirection : int
{
    Forward  = 0,   ///< 0 → 1 (default)
    Reverse  = 1,   ///< 1 → 0
    PingPong = 2,   ///< 0 → 1 → 0 → 1 … (seamless turnaround at both ends)
};

// ---------------------------------------------------------------------------
/**
 * Immutable, render-thread-safe snapshot of one recorded gesture curve.
 *
 * The curve is stored as a 256-sample lookup table of normalised values
 * (0.0 = bottom of the drawn area, 1.0 = top).  At playback time the engine
 * linearly interpolates between adjacent samples.
 *
 * Memory policy: snapshots are heap-allocated with `new` by the processor and
 * never deleted (MVP — each snapshot is ~1 KB so the leak is negligible).
 * The `valid` flag allows a default-constructed snapshot to act as a sentinel.
 */
struct LaneSnapshot
{
    std::array<float, 256> table{};           ///< Normalised curve samples [0, 1]
    float       durationSeconds = 1.0f;       ///< Original gesture duration (loop length at 1× speed)
    uint8_t     ccNumber        = 74;          ///< CC# used in CC mode (0-127)
    uint8_t     midiChannel     = 0;           ///< 0-indexed MIDI channel (0 = ch 1)
    float       minOut          = 0.0f;        ///< Output range lower bound (maps to 0 MIDI value)
    float       maxOut          = 1.0f;        ///< Output range upper bound (maps to max MIDI value)
    float       smoothing       = 0.08f;       ///< One-pole smoother coefficient (0 = off)
    MessageType messageType     = MessageType::CC;
    uint8_t     noteVelocity    = 100;         ///< Velocity used in Note mode (1-127)
    bool        valid           = false;       ///< false → engine treats snapshot as empty
};
