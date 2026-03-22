# DrawnCurve — Multilane & Note Mode Issues: Investigation & Plan

> **Status (2026-03):** The investigation and implementation plan in this document
> is substantially complete. Multi-lane architecture (Issues 3–6), note hysteresis
> (Issue 2), and stuck-note handling (Issue 1) have all shipped. The Icon redesign
> (Issue 7) is in progress. This file is retained as an audit trail.
> For the current forward-looking roadmap, see [`ROADMAP.md`](ROADMAP.md).

---

## Codebase Reality Check (original — now historical)

~~The current code is **entirely single-lane**.~~

**Current state (2026-03):** The plugin runs up to `kMaxLanes = 3` independent
lanes. Each lane has its own `LaneSnapshot*`, per-lane APVTS parameters
(`l0_ccNumber`, `l1_ccNumber`, etc.), scale config, and mute state.
`GestureEngine` processes all active lanes per `processBlock` call.
See `Source/PluginProcessor.h` for the full architecture.

---

## Issue 1 — Stuck Notes When Drawing a New Curve

### Root Cause
In `PluginProcessor.cpp::finalizeCapture()` (line 264–272):
```cpp
auto* snap = new LaneSnapshot(...);
_currentSnap = snap;
{
    juce::SpinLock::ScopedLockType lock(_engineLock);
    _engine.setSnapshot(snap);
    _engine.reset();   // <-- THE PROBLEM
}
```

`GestureEngine::reset()` (GestureEngine.cpp line 35–45) does:
```cpp
_noteOffNeeded.store(false, ...);   // cancels any pending Note Off
_runtime.lastSentValue = -1;        // forgets what note was playing
```

So when you draw a new curve while a note is sounding:
1. `setSnapshot()` loads the new curve
2. `reset()` clears `_noteOffNeeded` and sets `lastSentValue = -1`
3. The engine will never send Note Off for the note that was active before the reset

This affects any note that was playing when drawing starts (not just when the new curve finalizes).

### Also: `beginCapture()` has no note-off logic
`DrawnCurveProcessor::beginCapture()` (line 244) just calls `_capture.begin()`. No note off. The engine keeps sending MIDI while the user is actively drawing, which means notes can pile up.

### Fix Plan (belt & suspenders)

**Fix A — Note Off Before Reset**
Change `reset()` to NOT cancel `_noteOffNeeded`, and NOT clear `lastSentValue` when a Note Off is still pending. The Note Off will then fire on the very next `processBlock()` before the new curve takes over.

Concretely:
- In `GestureEngine::reset()`: remove `_noteOffNeeded.store(false, ...)` (or move it after the Note Off is sent, inside `processBlock`)
- Preserve `lastSentValue` across `reset()` when `_noteOffNeeded` is true
- `processBlock()` already sends the Note Off and then sets `lastSentValue = -1` — this ordering is correct

**Fix B — Stop Playback on `beginCapture()`**
In `DrawnCurveProcessor::beginCapture()`, call `_engine.setPlaying(false)` first. This sets `_noteOffNeeded = true`. The Note Off will be dispatched on the next `processBlock()` before drawing completes.

**Fix C — MIDI Panic Button** (new feature)
Add a "Panic" button to the UI (or expose it as a long-press on Clear). When triggered:
1. Set an atomic `_panicNeeded` flag in `DrawnCurveProcessor`
2. On next `processBlock()`, emit:
   - `0xB0 | ch, 0x7B, 0x00` = All Notes Off (CC 123) on every active channel
   - As a belt: iterate all 128 note numbers and emit `0x80 | ch, note, 0` for completeness
3. Clear `lastSentValue = -1` for all lanes

Panic could be a small button in the UI (top row), or accessible via long-press on Clear.

---

## Issue 2 — Rapid Pulses on Two Adjacent Notes (e.g. C♯ + D♯)

### Root Cause
In `GestureEngine::processBlock()` for `MessageType::Note` (line 208–231):
```cpp
const int note = clamp(lround(ranged * 127.0f), 0, 127);
if (note != _runtime.lastSentValue) {
    // Note Off old pitch, Note On new pitch
}
```

When the curve value hovers at the boundary between two semitones (e.g. between MIDI 49 and 50), floating-point rounding causes `lround()` to alternate between the two values every few milliseconds. The result is rapid alternating Note Off/On pairs — the "pulsing" you hear.

The one-pole smoother (applied *before* the note quantization) does reduce jitter for CC and PB modes, but in Note mode the smoother output is still quantized to integers, so any oscillation near a semitone boundary still triggers alternation.

### Fix Plan

**Fix A — Note Hysteresis**
Track the "locked" note separately from the raw computed note. Only change the locked note if the new computed note differs by at least 0.6 semitones from the locked one (i.e. the pitch must cross well past the boundary before switching). This is standard hysteresis:
```
// pseudocode
float rawNote = ranged * 127.0f;
if (abs(rawNote - _runtime.lockedNote) >= 0.6f) {
    // commit the change
    _runtime.lockedNote = rawNote;
    int note = clamp(lround(rawNote), 0, 127);
    // send Note Off/On
}
```
Store `lockedNote` as `float` in `LaneRuntime` (alongside `lastSentValue`).

**Fix B — Minimum Gate Time (optional, more musical)**
Once a note is triggered, hold it for a minimum duration (e.g. 30–60 ms) before allowing another pitch change. This prevents sub-note bursts and makes the output more musical. Implemented as a `gateSamplesRemaining` counter in `LaneRuntime`.

Fix A is simpler and more robust. Fix B is optional and can be added later (or made user-configurable as a "glide quantize" feature).

---

## Issue 3 — Multi-Lane Architecture (The Big One)

### Current State
There is NO multilane implementation yet. The processor has exactly one engine. The apparent "interaction between lanes" when two lanes play simultaneously would arise from:
- A single set of APVTS parameters controlling both displays
- The parameter system being shared — changing `messageType` for "lane 1" also changes it for "lane 0"

### Design

**Data Model: `LaneData` struct**
Each lane owns:
```cpp
struct LaneData {
    GestureEngine         engine;
    GestureCaptureSession capture;
    const LaneSnapshot*   currentSnap { nullptr };
    int                   index;       // 0-based lane index
};
```

Per-lane APVTS parameters (keyed by lane index, e.g. `"lane0_messageType"`):
- `messageType` — CC / Channel Pressure / Pitch Bend / Note
- `midiChannel` — 1–16
- `ccNumber` — 0–127
- `noteVelocity` — 1–127
- `minOutput`, `maxOutput` — output range
- `smoothing`
- `playbackDirection` — Forward / Reverse / Ping-Pong

Shared (master) APVTS parameters (unchanged):
- `playbackSpeed`
- `syncEnabled`
- `syncBeats`

**Processor Changes**
- `DrawnCurveProcessor` holds `std::array<LaneData, kMaxLanes>` (kMaxLanes = 4 is a good start)
- A `_activeLaneCount` atomic (1 by default) controls how many lanes are active
- `processBlock()` iterates active lanes, calling each lane's `engine.processBlock()`
- All lanes receive the same `speedRatio` and `direction` (from master params) — but see Pattern Length section

**UI Changes**
- `DrawnCurveEditor` shows a scrollable list of `LaneStrip` components
- Each `LaneStrip` contains: a compact `CurveDisplay` + per-lane controls (message type, channel, CC#/Vel, range, smooth, direction)
- "Add Lane" button (+ icon) adds a new lane
- Default: **1 lane, Note mode** (see Issue 6)
- Lane strips are stacked vertically; the curve display shrinks to fit

---

## Issue 4 — Pattern Length & Lane Synchronisation

### How It Should Work
- **Lane 0 is master**: its effective duration (recorded duration / speed ratio) is the master loop length
- **All other lanes** loop at the same wall-clock period as Lane 0, regardless of their recorded duration
- The speed ratio for Lane N is: `lane[N].durationSeconds / masterEffectiveDur`
- This means if you draw Lane 1 twice as long as Lane 0, it plays at half speed
- If sync is enabled, `masterEffectiveDur = syncBeats * 60 / BPM`; each lane's speed is scaled from that

### Adding a Lane With Different Duration Changes the Loop?
With the design above, it doesn't — each lane's individual speed ratio is computed from the master duration. The *master* duration is always Lane 0's. Other lanes adapt. This matches the expected behaviour.

### Clearing
- Clearing Lane 0 stops all lanes (it's the master); all lane states reset
- Clearing a non-master lane stops only that lane
- Clearing all lanes resets the master duration to 0

---

## Issue 5 — Message Types Lost / Shared Between Lanes

### Root Cause
Currently, `messageType` is a single global APVTS parameter. In any multilane scenario, selecting "Note" for Lane 1 immediately affects Lane 0 as well.

### Fix
Per-lane parameters as described in Issue 3 above. Each lane's `finalizeCapture()` reads `lane[i]_messageType` etc., so lanes are fully independent.

### Default Message Type
Change the default `messageType` parameter from `0` (CC) to `3` (Note) — this aligns with the new default-single-note-lane goal.

---

## Issue 6 — Default Should Be One Note Lane, Not Three CC Lanes

### Changes Needed
1. Default `_activeLaneCount = 1`
2. Default `messageType` for Lane 0 = `Note` (index 3 in the Choice parameter)
3. The UI starts with one lane strip visible; "+ Add Lane" button appears below

---

## Issue 7 — Message Type UI Illegibility

### Current State
Four `TextButton`s with labels "CC", "Aft", "PB", "Note" — text is small, colour contrast is low, and the abbreviations are cryptic.

### Constraints
- ComboBox popups don't work in AUv3 XPC process on iOS (no `TopLevelWindow`)
- Unicode music symbols require font coverage not available in the sandbox
- The existing `SymbolLF` LookAndFeel already draws custom path-based icons (arrows for direction buttons)

### Options

**Option A — Custom Path Icons in SymbolLF (Recommended)**
Extend `SymbolLF::drawButtonText()` to recognise the four message type identifiers and draw distinctive path-based symbols:
- `"CC"` → a wavy horizontal line (sinusoidal path drawn in code)
- `"Aft"` → a downward-pointing hand-pressure icon (rectangle + arrow down)
- `"PB"` → a vertical bidirectional arrow (or zigzag)
- `"Note"` → a filled note head with a stem (simple musical note drawn as ellipse + line)

These avoid all font/glyph issues. The buttons are already the right width to hold a small icon + short label.

**Option B — Larger Button Labels + Tooltip Row**
Keep text labels but increase font size, use a wider lane strip, and add a one-line tooltip below the buttons that reads the full name of the currently selected type. No new graphics needed, minimal code change.

**Option C — Dropdown-Style Panel (complex)**
On tap, expand a small floating panel (a JUCE `Component` with `setAlwaysOnTop(true)`) showing four large icon+label choices. Implemented as a custom component — avoids ComboBox's popup limitation. Most work but best UX.

**Recommended path**: Option A (path icons) for the initial implementation, Option C as a follow-up enhancement.

---

## Issue 8 — Notes About Future Features (Record Only, No Action)

- **Sustain pedal support**: Lanes could check an incoming MIDI CC 64 hold state and suppress Note Off while the pedal is down. This would require `acceptsMidi() = true` in the processor.
- **Theremin mode / MPE**: A continuous pitch mode where the Y position sends pitch bend (or MIDI polyphonic expression) on a per-note basis. This maps naturally onto the existing PitchBend message type but with MPE would need per-channel routing.
- **Independent lane speeds (Fugue Machine style)**: Each lane has its own speed multiplier. This is a straightforward extension — store `float speedMultiplier` per lane, compute `laneSpeedRatio = masterSpeed * lane.speedMultiplier`. UI: a small speed knob per lane strip.

---

## Implementation Order

*(Status updated 2026-03. ✅ = shipped, ⚠️ = in progress / pending fix.)*

| Priority | Task | Status |
|---|---|---|
| 1 | Fix stuck notes: preserve Note Off across `reset()` | ✅ |
| 2 | Fix stuck notes on new curve: `setPlaying(false)` in `beginCapture()` | ✅ |
| 3 | Add Panic button (all notes off) | ✅ |
| 4 | Fix rapid pulse: note hysteresis in Note mode | ✅ |
| 5 | Change default messageType to Note | ✅ |
| 6 | Multilane data model + processor loop | ✅ |
| 7 | Per-lane APVTS parameters | ✅ |
| 8 | LaneStrip UI component (routing matrix) | ✅ |
| 9 | Pattern length sync (Lane 0 as master) | ✅ |
| 10 | Message type icons in SymbolLF | ⚠️ in progress |
| — | Note-mode glissando fix (bypass smoother for note detection) | ✅ |
| — | Per-lane playheads in CurveDisplay | ✅ |
| — | Sync-mode pause latch (_userManualPauseInSync) | ✅ |
| — | updateLaneSnapshot (hot-swap params without redraw) | ✅ |
| — | Bitmask convention: C=MSB, B=LSB | ⚠️ pending |
| — | Playback direction in sync mode (buttons non-functional) | ⚠️ pending |
| — | AudioQueue -50 noise (skip probe, use session.sampleRate) | ⚠️ pending |
| 10 | Message type icons in SymbolLF | PluginEditor.cpp |

Items 1–5 can ship independently without the multilane refactor. Items 6–10 are the multilane overhaul.
