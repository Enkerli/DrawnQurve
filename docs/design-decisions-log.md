# DrawnCurve — Design Decisions Log

Running record of decisions made, deferred, and still open.
Append new entries; do not remove old ones (for audit trail).

---

## Format

Each entry has:
- **Date** (YYYY-MM)
- **Status**: DECIDED / DEFERRED / OPEN / SUPERSEDED
- **Topic**
- **Decision or rationale**
- **Cross-reference** (PR / doc section / issue)

---

## Entries

---

### 2026-03 | DEFERRED | Bitmask-as-decimal input for custom scales

**Context**: The custom scale bitmask (12-bit) could be exposed as a decimal number
computed from the pitch-class bits read left-to-right (C=MSB, B=LSB). For example,
Chromatic = 4095, Major = 2741. This would allow copy-paste of scale "codes".

**Decision**: Deferred. Implement after the visual custom-scale editor (piano-key row)
is complete and tested. The decimal encoding is an Expert-tier shortcut, not a primary
interface. Bit order: C is the most significant of the 12 bits (i.e., the displayed
decimal reads C, C#, D, D#, E, F, F#, G, G#, A, A#, B from left to right).

**Cross-reference**: Session 2026-03 discussion; `GestureEngine.hpp` ScaleConfig docs.

---

### 2026-03 | OPEN | Scale/pitch-class UI: piano-keyboard layout

**Context**: The current custom mask UI uses 12 equal-width buttons in a row
(C C# D D# E F F# G G# A A# B). This is functional but deviates from the
established two-row piano-keyboard idiom used by Logic Pro, Scaler, ScaleBud,
AutoTonic, and Intellijel Scales.

**Direction**: Replace the 12-button row with a two-row piano-key layout
(5 staggered "black" toggles above 7 "white" toggles) for the custom mask selector
AND for the root-note selector. Acknowledging the piano-centric design (#PianoCentric)
but following the established idiom because it reduces learning time.

**Pending**: Benchmarking of Resonant Design projects (Arcade 2, Straylight) and
Scaler 3 to confirm the exact visual treatment. Assign before the UI redesign sprint.

---

### 2026-03 | OPEN | Playback direction: 3 buttons vs. single control

**Context**: Currently implemented as three equal TextButtons (Fwd / Rev / P-P)
with arrow symbols. Alternative: a 3-state segmented control or a cycling button.

**Direction**: Investigate what direction controls look like in comparable products
(Ableton, Logic, ROLI, Eurorack). A single segmented/radio control with icons may
be cleaner. The arrow symbols already work; the question is density and touch target.
#ToBeTested with T4 task.

---

### 2026-03 | OPEN | Help overlay: full-screen vs. contextual

**Context**: Current "?" button opens a full-screen overlay (HelpOverlay) that covers
the entire plugin UI. NN/g categorises this as a "pull revelation" (user-triggered,
better than push/unsolicited) but the full-screen replacement of context is still
problematic — users lose reference to the control they were asking about.

**Direction**: Investigate:
1. Long-press on any control → tooltip with full name + description, positioned near
   the control (NN/g best practice for contextual help)
2. "?" opens a non-blocking side drawer or smaller panel rather than a full overlay
3. Keep current overlay as fallback "quick reference" (it's better than nothing)

**References**: NN/g "Pop-ups and Adaptive Help Get A Refresh" and "Tooltip Guidelines".
Heuristic #10 (Help and Documentation).

---

### 2026-03 | OPEN | Output-type abbreviations ("Aft", "PB")

**Context**: T6 task in benchmark-protocol.md shows "Aft" is opaque for Personas C/D.
"PB" likewise. Nielsen Heuristic #2: match between system and real world.

**Direction**:
- Simple mode: always full names ("CC", "Aftertouch", "Pitch Bend", "Notes") — already
  documented in interface-modes.md
- Standard/Expert: investigate best practice. Options:
  a. Keep abbreviations; add long-press tooltip with full name
  b. Use icons (SF Symbols: pressure icon for Aftertouch, wave for Pitch Bend)
  c. Slightly less-abbreviated labels: "A.Touch", "P.Bend" — still fits button width

Resolve before UI redesign sprint; tie to T6 benchmarking results.

---

### 2026-03 | OPEN | Multi-stroke mode (spaces in a single curve)

**Context**: User draws a curve in multiple non-continuous segments along the X-axis,
creating intentional silent gaps. Tap-and-hold modifier or key held while drawing.
Segments are part of one logical curve with one loop length.

**Pending questions** (#ToBeTested):
- Input model: hold button vs. hold modifier key vs. drag handle between strokes
- Empty-gap behaviour: silence (hold last value?) vs. CC value 0 vs. custom hold value
- Data model: `LaneSnapshot` needs a segment list, not a flat 256-point array

**Priority**: After multi-lane scoping; these two features are architecturally related.

---

### 2026-03 | OPEN | Multi-lane (parallel curves)

**Context**: Multiple independent curves in the same canvas, each routed to a different
MIDI target. Color-coded; accessibility texture as secondary differentiator.

**Pending**: Major architectural change. Full design sprint required.
See `usability-guidelines.md` for summary.

---

### 2026-03 | OPEN | Incoming MIDI display in canvas background

**Context**: Note-on data received by the plugin is drawn as a piano-roll-style ghost
layer — note numbers on Y-axis, velocity as vertical bar height. Aligns with the Y-axis
note labels when Note mode is active.

**Two value propositions**:
1. Observational: shows the pitch context before drawing
2. Interactive: "draw a new version of the incoming loop" — #Playfulness

**Pending**: Ring-buffer design for audio-to-UI note data; Y-axis alignment spec;
decide whether to show only Note-on events or include CC/other.

---

### 2026-03 | OPEN | Resonant Design benchmarking

**Context**: resonant.design is a Berlin-based music plugin UX agency whose portfolio
includes Dubler 2, Arcade 2 (Note Kits keyboard redesign), Straylight, Opsix, Rosa,
BassySynth. Their visual approach to scale rows, pitch-class selectors, and MIDI
routing controls is a primary benchmarking target.

**Action items**:
- [ ] Look at Arcade 2 in Output's current product — specifically Note Kits
- [ ] Look at Straylight (NI) — modulation + scale UI if any
- [ ] Look at Opsix (Korg) — scale/key UI
- [ ] Check Resonant Design's public writing / talks for design rationale

---

### 2026-03 | NOTED | Design critique process

**Context**: The design documents (interface-modes, personas, usability-guidelines,
benchmark-protocol) are suitable as input to a structured design critique — either
via a separate chat with another LLM, or with a working designer.

**Note**: These docs are not prescriptive UI specs; they are structured hypotheses.
A design critic should be invited to challenge the tier model, persona descriptions,
and task scenarios directly. The personas in particular are generic proxies; they
need to be validated against or replaced with data from real user sessions.

---

### 2026-03 | NOTED | "Audio effect" scope boundary

**Context**: An audio effect plugin with curve-controlled parameters would have
advantages (built-in routing, no MIDI plumbing required for the end user). However:
- Apple's AUv3 audio effect APIs have different complexity/constraints
- The primary prototype value is already delivered as a MIDI processor
- Alternative: document use cases + pre-mapped routes using existing free audio
  effects (or custom patches in Plugdata)

**Decision**: Out of scope for implementation; document use cases instead.

---

### 2026-03 | NOTED | Companion app scope boundary

**Context**: A companion iOS app to manage presets, routing templates, and visual
curve editing outside of a DAW context.

**Decision**: Out of scope for the current product. Revisit after multi-lane and
preset management are mature.
