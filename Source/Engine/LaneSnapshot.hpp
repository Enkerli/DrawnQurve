#pragma once
#include <array>
#include <cstdint>

// Which MIDI message type the engine emits.
enum class MessageType : uint8_t {
    CC              = 0,   // Control Change   (0xB0) — data1=ccNumber, data2=value 0-127
    ChannelPressure = 1,   // Channel Pressure (0xD0) — data1=value 0-127 (2-byte message)
    PitchBend       = 2,   // Pitch Bend       (0xE0) — 14-bit value, centre=8192
};

// Playback direction for the curve loop.
enum class PlaybackDirection : int {
    Forward  = 0,   // 0→1  (default)
    Reverse  = 1,   // 1→0
    PingPong = 2,   // 0→1→0→1…  (seamless turnaround)
};

// Immutable, render-thread-safe snapshot of one recorded curve.
// 256-sample lookup table, normalised 0..1.
struct LaneSnapshot {
    std::array<float, 256> table{};
    float       durationSeconds = 1.0f;
    uint8_t     ccNumber        = 74;
    uint8_t     midiChannel     = 0;       // 0-indexed (0 = channel 1)
    float       minOut          = 0.0f;
    float       maxOut          = 1.0f;
    float       smoothing       = 0.08f;
    MessageType messageType     = MessageType::CC;
    bool        valid           = false;
};
