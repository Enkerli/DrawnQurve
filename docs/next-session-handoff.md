# DrawnCurveJUCE — Handoff & Advice Document for Main Branch Work

## Context

This document is a handoff guide for a new chat session working on the DrawnCurve AUv3
JUCE plugin (main branch + local Xcode build on macOS/iOS). It captures lessons from a
prior investigation branch (`claude/multilane-note-mode-issues-sk8w4`) and provides
precise, code-level guidance for the next round of fixes and features.

The prior branch contributed two things already merged (or ready to merge) into main:

1. **Note hysteresis fix** — prevents rapid C♯/D♯ pulsing at semitone boundaries
2. **Message-type button icons** — path-drawn icons for CC / Aft / PB / Note in `SymbolLF`

Everything below describes work that still needs to land on main.

---

## Current Codebase State (main)

Single-lane only. One `GestureEngine`, one `GestureCaptureSession`, one global APVTS
parameter set. The engine is real-time safe (atomic cross-thread communication, SpinLock
between audio thread and fallback HiRes timer). Four MIDI message types: CC, Channel
Pressure, Pitch Bend, Note.

Key files:
```
Source/
  PluginProcessor.h / .cpp    — DrawnCurveProcessor (host bridge, APVTS, timer)
  PluginEditor.h / .cpp       — DrawnCurveEditor + CurveDisplay + HelpOverlay
  Engine/
    LaneSnapshot.hpp          — immutable curve data + MessageType / PlaybackDirection enums
    GestureEngine.hpp / .cpp  — real-time playback engine
    GestureCaptureSession.hpp / .cpp — touch capture + curve baking
```

---

## Priority 1 — Stuck Notes Fix (two-part, quick)

### Part A: `beginCapture()` should silence any active note before drawing starts

**File:** `Source/PluginProcessor.cpp`, line 244
**Current:**
```cpp
void DrawnCurveProcessor::beginCapture() { _capture.begin(); }
```
**Change to:**
```cpp
void DrawnCurveProcessor::beginCapture()
{
    _engine.setPlaying (false);   // sends Note Off on next processBlock if needed
    _capture.begin();
}
```
`setPlaying(false)` sets the atomic `_noteOffNeeded` flag; the engine's next
`processBlock()` call will emit the Note Off before any new playback starts.
The timer is NOT stopped here intentionally — the timer's existing guard
(`if (! isPlaying() || ! hasCurve()) return;`) means it won't advance the engine.

### Part B: `reset()` must not cancel a pending Note Off

**File:** `Source/Engine/GestureEngine.cpp`, lines 35–46
**Current `reset()`:**
```cpp
_noteOffNeeded.store (false, std::memory_order_relaxed);  // ← cancels Note Off! bug
_runtime.playheadSeconds = 0.0;
_runtime.lastSentValue   = -1;
_runtime.lockedNote      = -1.0f;
_runtime.smoothedValue   = 0.0f;
_currentPhase.store (0.0f, std::memory_order_relaxed);
```
**Change:** Remove the `_noteOffNeeded.store(false, ...)` line entirely.
The Note Off will then fire on the very next `processBlock()` call before the new
curve starts playing (the cleanup path at the top of `processBlock()` runs before
the `_isPlaying` check, so it always executes).

Caution: host-sync rising edge calls `_engine.reset()` before `setPlaying(true)`.
That's fine — if no note was playing, `lastSentValue == -1` and the Note Off path
is a no-op. The flag is only set when `setPlaying(false)` was called first.

---

## Priority 2 — MIDI Panic (new feature)

Provides "all notes off" as a safety valve. Classic MIDI muscle memory.

### Implementation

**`Source/Engine/GestureEngine.hpp`** — add to the UI-thread API section:
```cpp
/// Immediately silence all notes: call before or instead of setPlaying(false)
/// when you want to guarantee no stuck notes regardless of playback state.
/// Sets a flag; the actual MIDI is emitted by the next processBlock() call.
void triggerPanic();
```
Add private atomic:
```cpp
std::atomic<bool> _panicNeeded { false };
```

**`Source/Engine/GestureEngine.cpp`** — implement:
```cpp
void GestureEngine::triggerPanic()
{
    _isPlaying.store (false, std::memory_order_release);
    _panicNeeded.store (true, std::memory_order_release);
}
```
At the top of `processBlock()`, before the existing `_noteOffNeeded` check:
```cpp
if (_panicNeeded.exchange (false, std::memory_order_acq_rel))
{
    if (midiOut)
    {
        // All Notes Off (CC 123) on every channel that could be active.
        for (uint8_t ch = 0; ch < 16; ++ch)
            midiOut (0xB0u | ch, 0x7Bu, 0x00u);
    }
    _runtime.lastSentValue = -1;
    _runtime.lockedNote    = -1.0f;
    return;   // skip normal playback this block
}
```

**`Source/PluginProcessor.h`** — expose:
```cpp
void triggerPanic();   // UI thread: silences all notes immediately
```

**`Source/PluginProcessor.cpp`** — delegate:
```cpp
void DrawnCurveProcessor::triggerPanic() { _engine.triggerPanic(); }
```

**`Source/PluginEditor.h`** — add button:
```cpp
juce::TextButton panicButton { "!!" };   // short label fits in row
```

**`Source/PluginEditor.cpp`** — wire up in the constructor and `resized()`:
```cpp
// Constructor
panicButton.onClick = [this] { proc.triggerPanic(); };
addAndMakeVisible (panicButton);

// Long-press on Clear is an alternative UX — for now a dedicated button in Row 1
// sits to the right of clearButton. Size: ~36px wide.
```
In the help overlay text, document it as "!! — MIDI Panic: silences all notes on all channels."

---

## Priority 3 — Default to Note Mode

One-line change. **`Source/PluginProcessor.cpp`**, `createParams()`:
```cpp
// Current (line ~62):
juce::StringArray { "CC", "Channel Pressure", "Pitch Bend", "Note" }, 0));
// Change index 0 → 3:
juce::StringArray { "CC", "Channel Pressure", "Pitch Bend", "Note" }, 3));
```
New presets and first-run experience will open in Note mode.
Existing saved state still restores correctly (the parameter value is serialised).

---

## Multilane Architecture (larger scope — design notes)

The following is not a single-session task, but the design is settled enough to
implement incrementally. Start with the data model and processor changes; the UI
can follow separately.

### Data Model

Add to `Source/Engine/` a new `LaneData.hpp` (or inline in `PluginProcessor.h`):
```cpp
struct LaneData {
    GestureEngine         engine;
    GestureCaptureSession capture;
    const LaneSnapshot*   currentSnap { nullptr };
};
```

In `DrawnCurveProcessor`:
```cpp
static constexpr int kMaxLanes = 4;
std::array<LaneData, kMaxLanes> _lanes;
std::atomic<int>                _activeLaneCount { 1 };
int                             _activeLaneIndex { 0 };  // UI-thread: which lane is selected
```

### Per-Lane APVTS Parameters

Name pattern: `"lane0_messageType"`, `"lane1_ccNumber"`, etc.
Add them in `createParams()` in a loop:
```cpp
for (int i = 0; i < kMaxLanes; ++i) {
    const juce::String prefix = "lane" + juce::String(i) + "_";
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { prefix + "messageType", 1 }, "Lane " + juce::String(i) + " Type",
        juce::StringArray { "CC", "Channel Pressure", "Pitch Bend", "Note" }, 3));
    layout.add(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { prefix + "midiChannel", 1 }, "...", 1, 16, 1));
    layout.add(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { prefix + "ccNumber", 1 }, "...", 0, 127, 74));
    layout.add(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { prefix + "noteVelocity", 1 }, "...", 1, 127, 100));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { prefix + "minOutput", 1 }, "...",
        juce::NormalisableRange<float>(0.f, 1.f, 0.001f), 0.f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { prefix + "maxOutput", 1 }, "...",
        juce::NormalisableRange<float>(0.f, 1.f, 0.001f), 1.f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { prefix + "smoothing", 1 }, "...",
        juce::NormalisableRange<float>(0.f, 1.f, 0.001f), 0.08f));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { prefix + "direction", 1 }, "...",
        juce::StringArray { "Forward", "Reverse", "Ping-Pong" }, 0));
}
```
Shared (master) parameters stay as-is: `playbackSpeed`, `syncEnabled`, `syncBeats`.

### `processBlock()` Loop

Replace the single `_engine.processBlock(...)` call with:
```cpp
const int count = _activeLaneCount.load(std::memory_order_relaxed);
const double masterDur = (_lanes[0].currentSnap && _lanes[0].currentSnap->valid)
    ? _lanes[0].currentSnap->durationSeconds : 0.0;

for (int i = 0; i < count; ++i) {
    auto& lane = _lanes[i];
    // Compute per-lane speedRatio relative to master loop length
    float laneSpeed = effectiveSpeed;
    if (i > 0 && masterDur > 0.0 && lane.currentSnap && lane.currentSnap->valid) {
        // Stretch this lane's recorded duration to match Lane 0's effective loop length
        const double masterEffectiveDur = masterDur / effectiveSpeed;
        laneSpeed = lane.currentSnap->durationSeconds / masterEffectiveDur;
    }
    juce::SpinLock::ScopedLockType lock(_engineLock);
    lane.engine.processBlock(
        static_cast<uint32_t>(buffer.getNumSamples()),
        getSampleRate(),
        [&midiMessages](uint8_t s, uint8_t d1, uint8_t d2) {
            midiMessages.addEvent(makeMidiMessage(s, d1, d2), 0);
        },
        laneSpeed, dir);
}
```
One SpinLock per lane would be better long-term, but start with shared lock for safety.

### Pattern Length Rule

- Lane 0's `effectiveDur` = `lane[0].durationSeconds / masterSpeedRatio` is the master loop
- Lane N loops at `lane[N].durationSeconds / laneSpeedRatio` where `laneSpeedRatio` is chosen
  so that `lane[N].durationSeconds / laneSpeedRatio == masterEffectiveDur`
- This means `laneSpeedRatio = lane[N].durationSeconds / masterEffectiveDur`
- Net result: all lanes loop in exactly the same wall-clock period
- Clearing Lane 0 stops all lanes; clearing Lane N (non-master) stops only that lane

### UI: LaneStrip Component

Each lane gets a compact horizontal strip containing:
- Miniature `CurveDisplay` (e.g. 80–100px tall)
- A column of per-lane controls on the right: message type icons row, channel, CC#/Vel, range, smooth, direction

The main editor stacks strips vertically; the curve display per-lane scales down to fit.
An "+ Add Lane" button below the last strip, disabled when `_activeLaneCount == kMaxLanes`.

---

## Things to Keep a Note Of (Future / Low Priority)

- **Sustain pedal support**: `acceptsMidi() = true`, detect CC 64 hold state, suppress
  Note Off while pedal is held. Requires storing "pedal down" bool per channel.
- **Theremin / continuous pitch mode**: Pitch sent as PitchBend on a held note rather than
  discrete Note On/Off glide. Works especially well with MPE (per-note PB).
- **MPE**: Per-channel routing for multilane. Each lane uses its own MIDI channel.
  Channels 2–16 for note lanes, channel 1 as global channel.
- **Independent lane speeds (Fugue Machine style)**: `float speedMultiplier` per lane,
  applied on top of master speed. UI: small speed knob per lane strip.
- **Quantize-by-curve-length on new draw**: Rather than immediately replacing the
  playing curve, queue the new snapshot and swap it at the natural loop boundary.
  Requires a `_pendingSnap` atomic pointer in GestureEngine and a "swap at phase 0"
  check inside `processBlock()`.

---

## Verification Plan

After applying Priority 1 + 2 fixes on main + Xcode build, test:

1. **Stuck notes, basic**: Play a note curve → draw a new curve → confirm old note stops cleanly
2. **Stuck notes, race**: Draw rapidly (multiple curves in quick succession) → confirm no stuck notes
3. **MIDI Panic**: Start playback → press `!!` button → confirm all notes off in receiving app
4. **Note hysteresis**: Draw a curve that crosses a single semitone boundary slowly → confirm
   no rapid pulsing, clean single transition
5. **CC mode unchanged**: Switch to CC mode, draw curve, verify smooth CC output (not note data)
6. **Pitch Bend**: Verify 14-bit PB messages are correct (centre = 8192, full range 0–16383)
7. **Message type icons**: Confirm all four buttons display icons + labels; active button highlights
8. **State persistence**: Set parameters, save/reload preset, confirm all values restored including curve table
