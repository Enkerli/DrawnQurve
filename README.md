# DrawnCurve

**DrawnCurve** is an iOS AUv3 MIDI effect plugin built with JUCE.
Draw a curve with your finger or Apple Pencil — it loops continuously, converting your gesture into MIDI messages of your choice. Up to three independent lanes let you simultaneously control multiple synth parameters from a single plugin instance.

---

## Features

| Feature | Details |
|---|---|
| **Draw-to-loop** | Finger or Pencil gesture becomes a looping automation curve |
| **Three independent lanes** | Each lane has its own output target, routing, and curve |
| **Output modes** | CC, Channel Pressure (Aftertouch), Pitch Bend, Note On/Off — per lane |
| **Scale quantization** | In Note mode: quantize pitch to 7 preset scales or a custom 12-bit bitmask — per lane |
| **Loop direction** | Forward, Reverse, or Ping-Pong (shared across lanes) |
| **Speed control** | 0.25× – 4× manual, or tempo-synced in beats (shared) |
| **Host sync** | Locks play/stop and loop duration to DAW transport + BPM |
| **Output range** | Dual-thumb range slider clips/scales the output window — per lane |
| **Smoothing** | One-pole low-pass filter softens abrupt transitions — per lane |
| **Mute** | Silence one lane without erasing its curve |
| **Teach / CC Learn** | Tap Teach on any lane to MIDI-learn a CC number from an incoming controller |
| **MIDI Panic** | All-notes-off on all active channels — tap `!` in the toolbar |
| **Per-lane playheads** | Coloured dots on the curve display show each active lane's position |
| **Themes** | Light (default) and Dark colour schemes |
| **Inline help** | `?` button shows a quick-reference guide over the UI |

---

## Requirements

| Requirement | Version |
|---|---|
| Xcode | 15 or later |
| iOS deployment target | 16.0+ |
| JUCE | 7.x (included as a Git submodule at `JUCE/`) |
| CMake | 3.22+ |

The plugin is a **MIDI processor** AUv3 (`aumi` type).
It produces MIDI output only — no audio I/O.

---

## Building

```bash
# 1. Clone with submodules
git clone --recurse-submodules https://github.com/Enkerli/DrawnCurveJUCE.git
cd DrawnCurveJUCE

# 2. Configure for iOS (device or simulator)
cmake -S . -B build-ios \
      -G Xcode \
      -DCMAKE_SYSTEM_NAME=iOS \
      -DCMAKE_OSX_DEPLOYMENT_TARGET=16.0

# 3. Open in Xcode and build
open build-ios/DrawnCurve.xcodeproj
```

The scheme **DrawnCurve_Standalone** builds a standalone host for quick testing.
The scheme **DrawnCurve_AUv3** builds the app extension that hosts deploy.

---

## How It Works

### Recording a curve

1. Tap a lane button (1 / 2 / 3) to select the lane you want to edit.
2. Draw anywhere in the large curve display area.
   Left → right is time; top → bottom is MIDI value (top = high, bottom = low).
3. Lift your finger — the curve is baked into a 256-sample lookup table and
   begins looping immediately.
4. Draw again to replace the current lane's curve; tap **Clear** to erase all lanes.

Each lane plays independently. Lane colours correspond to the coloured playhead
dots visible on the curve while playback is running.

### Output mapping

The normalised curve value (0–1) is processed in this order for each lane:

```
raw Y position
  → inverted         (top = 1.0, bottom = 0.0)
  → range scaled     minOut + value × (maxOut − minOut)
  → smoothed         one-pole filter (controlled by the Smooth slider)
  → MIDI scaled      0–127 / 0–16383 / note number, depending on output mode
```

In **Note mode**, the scaled value is additionally quantized to the nearest
active pitch class in the current scale. The quantization uses raw curve value
(not the smoothed signal) to prevent glissando — the smoother only affects CC
and Pitch Bend output.

### Scale quantization

When a lane is in Note mode, a scale row appears below the routing matrix:

- **Preset buttons** — Chromatic, Major, Natural Minor, Dorian, Pentatonic Major,
  Pentatonic Minor, Blues, Custom.
- **Root selector** — Transposes the scale to any of the 12 pitch classes.
- **Custom bitmask** — 12 circles represent C through B (left = C, right = B,
  MSB = C). Toggle individual pitch classes to build any scale.

### Sync mode

Press **SYNC** to engage host-transport and tempo sync:

- The engine follows the host's play/stop state — a rising edge resets the
  playhead and starts looping; a falling edge stops it.
- The **Speed** slider changes to **Beats**. Set the desired loop length in
  beats; the plugin calculates the correct speed ratio from the host BPM so
  the curve always completes in exactly that many beats.
- While sync is active, the **Play** button toggles a manual pause that
  persists across host play/stop transitions until you unpause.

### Teach / CC Learn

Tap **Teach** in any lane's row of the routing matrix:
- The lane's output is soloed (other lanes mute) so a connected synth can
  MIDI-learn from a clean signal.
- On CC lanes: the next incoming MIDI CC message sets that lane's CC number.
- Tap **Teach** again (or tap any other lane's Teach) to cancel.

---

## Parameters

### Shared (all lanes)

| Parameter | ID | Range | Default | Notes |
|---|---|---|---|---|
| Playback Speed | `playbackSpeed` | 0.25× – 4× | 1× | Manual mode only |
| Sync Enabled | `syncEnabled` | on/off | off | |
| Sync Beats | `syncBeats` | 1 – 32 | 4 | Active when sync is on |
| Direction | `playbackDirection` | 0 – 2 | 0 (Fwd) | Forward / Reverse / Ping-Pong |

### Per-lane (prefix `lN_` where N = 0, 1, 2)

| Parameter | ID base | Range | Default | Notes |
|---|---|---|---|---|
| Message Type | `msgType` | 0 – 3 | 0 (CC) | CC / Pressure / Pitch Bend / Note |
| CC Number | `ccNumber` | 0 – 127 | 74 | CC mode only |
| MIDI Channel | `midiChannel` | 1 – 16 | 1 | |
| Smoothing | `smoothing` | 0 – 1 | 0.08 | 0 = instant |
| Min Output | `minOutput` | 0 – 1 | 0.0 | Left thumb of Range slider |
| Max Output | `maxOutput` | 0 – 1 | 1.0 | Right thumb of Range slider |
| Note Velocity | `noteVelocity` | 1 – 127 | 100 | Note mode only |
| Scale Mode | `scaleMode` | 0 – 7 | 0 (Chromatic) | Note mode only |
| Scale Root | `scaleRoot` | 0 – 11 | 0 (C) | Note mode only |
| Scale Mask | `scaleMask` | 0 – 4095 | 4095 | Custom scale bitmask; MSB = C, LSB = B |
| Lane Enabled | `enabled` | on/off | on | Mute toggle |

Full IDs are formed by concatenation: e.g. lane 0 CC number = `l0_ccNumber`.

All parameters are automatable and saved in presets via APVTS.
The recorded curve tables are also serialised into the preset (base-64 encoded).

---

## Architecture

```
Source/
  PluginProcessor.h/.cpp     — AudioProcessor: APVTS, threading, host sync, panic
  PluginEditor.h/.cpp        — AudioProcessorEditor: UI, lane matrix, sliders
  SegmentedControl.h/.cpp    — Reusable segmented/radio control component
  Engine/
    LaneSnapshot.hpp         — Plain-data curve snapshot (256 floats + metadata)
    GestureEngine.hpp/.cpp   — Real-time multi-lane MIDI playback (audio thread)
    GestureCaptureSession.hpp/.cpp — Gesture recording + baking pipeline
```

### Threading model

Three threads interact with the processor:

| Thread | Responsibilities |
|---|---|
| UI / message thread | Editor, gesture capture, parameter changes |
| Audio thread | `processBlock` — advances all lane engines, emits MIDI |
| HiRes timer (10 ms) | Fallback when host is not calling `processBlock` |

The engine array is guarded by a `juce::SpinLock`; the audio thread wins whenever
active (checked via timestamp). MIDI generated by the timer is buffered and
flushed on the next audio-thread call.

### Multi-lane snapshot pattern

Each lane owns an independent `LaneSnapshot*` managed via `std::atomic<const LaneSnapshot*>`.
When a curve is drawn or a parameter changes:

1. The UI thread bakes a new `LaneSnapshot` (curve table + all per-lane params).
2. The old pointer is atomically swapped into the engine.
3. `GestureEngine::processLane()` picks up the new snapshot on its next call.
4. The old snapshot is deleted on the UI thread after the swap.

`updateLaneSnapshot(lane)` re-bakes parameters into an existing curve table
without requiring the user to redraw — this is called by parameter change
listeners for smoothing, range, CC#, channel, velocity, and message type.

### Note-mode quantization

In Note mode, the curve value determines pitch via the `ScaleConfig` bitmask.
The **raw curve value** (before smoothing) is used for note detection; the smoother
runs internally but is bypassed for the note-change decision. This prevents the
smoother from ramping continuously through intermediate pitches (glissando). A
midpoint dead-zone hysteresis with ±0.35 semitone clearance prevents oscillation
at scale-degree boundaries.

### Why path-drawn arrows?

The AUv3 XPC sandbox on iOS prevents loading non-default fonts. JUCE's built-in
Bitstream Vera font covers only basic Latin, so Unicode arrow characters render
as missing glyphs. Direction buttons and scale lattice buttons use a custom
`LookAndFeel` subclass that draws filled `juce::Path` shapes — zero font dependency.

### Why no SliderAttachment for the range slider?

JUCE's `AudioProcessorValueTreeState::SliderAttachment` does not support
`TwoValueHorizontal` sliders. Instead, the range slider uses an `onValueChange`
lambda that writes directly to the `AudioParameterFloat` objects, plus APVTS
listeners to sync back any externally-driven parameter changes.

---

## Roadmap

See [ROADMAP.md](ROADMAP.md) for the full feature roadmap, sibling project plans,
and playflow-based feature prioritisation.

See [docs/](docs/) for design decisions, interface mode specs, personas, and
usability guidelines.

---

## License

[MIT License](LICENSE)
JUCE is used under the JUCE 7 Community license.
