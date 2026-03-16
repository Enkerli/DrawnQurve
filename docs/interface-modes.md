# DrawnCurve — Interface Modes

Three progressive disclosure tiers map to the five user personas.
The goal is a single codebase where `setMode()` shows/hides elements
and resizes the canvas — no separate UIs, no duplication.

---

## Overview

| Mode | Label | Personas served | Controls visible |
|------|-------|----------------|-----------------|
| 0 | Simple | D (Student), C (Producer) first-run | Draw, play, one output type picker, sync |
| 1 | Standard | B (Performer), C (Producer) day-to-day | All core controls, named CC, no grid |
| 2 | Expert | A (Experimenter), E (Sound Designer) | Current full layout — no changes |

A single gear-icon button (or segmented control) in the toolbar switches mode.
The selected mode is persisted in plugin state.

---

## Mode 0 — Simple

**Design intent**: Zero configuration required. The user draws, taps Play,
and hears something. Everything else is hidden behind a "More controls" expander.

### What is shown

- Toolbar: `[▶ Play]` `[✕ Clear]` `[? Help]` `[☾ Theme]`
- Output picker: a single dropdown / segmented row with **full names**
  → `[CC]` `[Aftertouch]` `[Pitch Bend]` `[Notes]`
- Timing row: `[⏱ Sync ON]`  `[────● 4 beats ────]`
- Canvas: maximum possible height (all freed space given to it)
- Footer disclosure: `[⚙ More controls…]` — reveals Standard mode

### What is hidden

- Direction buttons (Fwd / Rev / P-P)
- CC# / Velocity slider
- MIDI Channel slider
- Smoothing slider
- Range (two-thumb) slider
- Speed slider (manual mode)
- Grid controls (Y- Y+ X- X+)
- Sync toggle (always on in Simple; OFF path hidden)

### Defaults applied when entering Simple mode

- Sync: ON
- Beats: 4
- Output: CC (or last used)
- Direction: Forward
- Range: 0–100% (full range)
- Smoothing: 0 (off)

---

### Simple Mode Mockup (640 × 560 px)

```
┌──────────────────────────────────────────────────────────────────────┐
│                                                                      │
│  [▶ Play]   [✕ Clear]                     [? Help]   [☾ Theme]  [⚙] │
│                                                                      │
│  What to control:  [CC]  [Aftertouch]  [Pitch Bend]  [Notes]        │
│                                                                      │
│  Loop timing:  [⏱ Sync]   [─────●─────────]  4 beats               │
│                                                                      │
│ ┌──────────────────────────────────────────────────────────────────┐ │
│ │                                                                  │ │
│ │                                                                  │ │
│ │                                                                  │ │
│ │                                                                  │ │
│ │              DRAW YOUR CURVE HERE                                │ │
│ │                                                                  │ │
│ │                                                                  │ │
│ │                                                                  │ │
│ │                                                                  │ │
│ └──────────────────────────────────────────────────────────────────┘ │
│  ⚙ More controls…                                                    │
└──────────────────────────────────────────────────────────────────────┘
```

Canvas height: ~400 px (vs. ~360 px in Expert mode) — 11% larger.
Full output-type names avoid the "what does Aft mean?" problem (T6 task).

---

### Simple Mode: Output Type Picker Detail

In CC mode a secondary row appears immediately below the output picker:

```
│  Control target: [ name your CC here… ]  (e.g. "Filter Cutoff")    │
```

This is a text field attached to `ccNumber`. The user types a label for
themselves; the underlying CC number (default 74) doesn't change unless
they open More Controls. This fulfils the Persona C need for named CC
without requiring a built-in lookup table.

---

## Mode 1 — Standard

**Design intent**: Full expressive capability, clearly labelled, no raw MIDI
numbers unless the user explicitly asks. Grid controls hidden (cosmetic, not
functional). The mode a regular user settles into after outgrowing Simple.

### What is shown (compared to Expert, additions shown with +, removals with −)

- Toolbar: all toolbar buttons — identical to Expert
- Direction row: Fwd / Rev / P-P — identical to Expert
- CC label field: user-defined label shown next to CC# slider (new vs. Expert)
- CC# slider: visible, but labelled with user's custom name if set
- Channel slider: visible
- Smoothing slider: visible
- Range slider: visible
- Speed/Beats slider: visible
- Sync toggle: visible
- Canvas: same height as Expert mode
- `[⚙ Expert…]` disclosure at bottom

### What is hidden compared to Expert

- Grid controls (Y- Y+ X- X+) — moved inside Expert disclosure
- Raw CC# number display can be hidden if a custom label is set
  (shown as the label only; a small "edit" icon reveals the number)

---

### Standard Mode Mockup (640 × 560 px)

```
┌──────────────────────────────────────────────────────────────────────┐
│                                                                      │
│  [▶]  [✕]   [CC]  [Aft]  [PB]  [Note]    [⏱ Sync]   [?]   [☾]  [⚙] │
│  [→ Fwd]  [← Rev]  [↔ P-P]                                          │
│                                                                      │
│  Control: [Filter Cutoff ✎]   Channel: [──●──] 1                    │
│  Smooth:  [──●──────────]  Range: [|═══●════════════●═══|]          │
│  Timing:  [─────────●───] 4.0 beats                                 │
│                                                                      │
│ ┌──────────────────────────────────────────────────────────────────┐ │
│ │                                                                  │ │
│ │                                                                  │ │
│ │                                                                  │ │
│ │                  (curve + playhead)                              │ │
│ │                                                                  │ │
│ │                                                                  │ │
│ └──────────────────────────────────────────────────────────────────┘ │
│  ⚙ Expert controls…                                                  │
└──────────────────────────────────────────────────────────────────────┘
```

The "Filter Cutoff ✎" control label:
- Shows the user-defined name if set, otherwise "CC 74" as a default
- Tapping ✎ opens a small inline text field to rename it
- The actual CC# is still accessible by tapping the label, which reveals
  a `CC#: [74]` numeric input beneath it

---

### Standard Mode: Output Type Abbreviations

Unlike Simple mode, Standard uses the current abbreviations (CC, Aft, PB, Note)
but adds tooltips: long-press on any button shows the full name for 1.5 seconds.
This builds fluency without cluttering the toolbar.

---

## Mode 2 — Expert

**Design intent**: The current UI, unchanged. Zero regressions from today's
layout. All controls permanently visible.

### Expert Mode Mockup (current layout, for reference)

```
┌──────────────────────────────────────────────────────────────────────┐
│                                                                      │
│  [▶]  [✕]   [CC]  [Aft]  [PB]  [Note]    [Sync]   [?]   [☾]   [⚙] │
│  [→]  [←]  [↔]     Y- Y+  X- X+                                    │
│  CC#:[────●──] 74   Ch:[─●──] 1   Smooth:[──●──────]  0.08          │
│  Range:[|═══●═══════════════●═══|]  Speed:[────●───] 1.00×          │
│                                                                      │
│ ┌──────────────────────────────────────────────────────────────────┐ │
│ │                                                                  │ │
│ │                                                                  │ │
│ │                  (curve + playhead + axis labels)                │ │
│ │                                                                  │ │
│ │                                                                  │ │
│ └──────────────────────────────────────────────────────────────────┘ │
└──────────────────────────────────────────────────────────────────────┘
```

No changes from the current implementation.
The `[⚙]` icon in the top-right cycles Simple → Standard → Expert → Simple.

---

## Mode Switch: Implementation Notes

### Storage
Mode is an integer (0/1/2) stored in plugin state XML alongside curve data:
```xml
<State>
  <InterfaceMode>0</InterfaceMode>
  <!-- existing curve + parameter data -->
</State>
```

### JUCE implementation sketch
```cpp
enum class InterfaceMode { Simple = 0, Standard = 1, Expert = 2 };

// In DrawnCurveEditor:
void setMode(InterfaceMode m);   // calls setVisible() on control groups
void resized() override;         // curve display fills freed space

// Mode groups to show/hide:
// - directionGroup (Fwd/Rev/P-P): hidden in Simple
// - paramRow1 (CC#, channel, smooth): hidden in Simple
// - rangeSlider: hidden in Simple
// - speedSlider: hidden in Simple; shown in Standard/Expert
// - gridControls (Y-/Y+/X-/X+): hidden in Simple and Standard
// - outputTypeFull (long names): visible only in Simple
// - outputTypeShort (abbreviations): visible in Standard/Expert
// - ccLabelField (user text): visible in Simple and Standard
```

### Canvas resize
When controls are hidden, call `resized()` to redistribute vertical space.
The curve display component should occupy all rows not used by visible controls.
In Simple mode this frees ~80 px for the canvas (direction row + param row).

---

## Transition Behaviour

- **Simple → Standard**: No values changed, all existing parameter values preserved.
- **Standard → Expert**: Grid controls appear; all other values preserved.
- **Any mode → Simple**: Sync is forced ON to maintain Simple mode's safe defaults.
  If the user was in manual speed mode, it switches to Sync/4beats silently.
  (This is the only time mode switching changes a parameter.)
- **Expert → any lower**: All parameter values preserved; no data loss.

---

## Persona–Mode Mapping

```
Persona D (Student)      → Default: Simple
Persona C (Producer)     → Default: Simple; likely moves to Standard
Persona B (Performer)    → Default: Standard; may lock to Standard permanently
Persona A (Experimenter) → Default: Expert; ignores Simple entirely
Persona E (Sound Design) → Default: Expert
```

The mode should be part of the plugin state so each instance
in a project remembers its own mode independently.
