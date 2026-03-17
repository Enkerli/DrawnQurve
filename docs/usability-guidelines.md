# DrawnCurve — Usability Guidelines for New Features

This document sets the standards for adding features to DrawnCurve while
maintaining usability across all three interface modes and all five personas.
Every new feature must pass the checklist at the end of this document before
it is merged.

---

## Core Principles

### 0. Playfulness is the Product

> **This is the foundational design principle. All others serve it.**

DrawnCurve's core value is already present: drawing a curve and hearing something
change is *immediately* playful. Low cognitive load, no setup required, results
in seconds. Every design decision should be filtered through the question:
*does this increase or decrease the playfulness of the interaction?*

Implications:
- The canvas is large and inviting — never compress it for a control panel
- Drawing must require zero configuration to produce audible results
- Controls that appear only when needed don't interrupt the flow; those that are
  always visible compete for attention with the drawing surface
- Feedback is immediate and visible (banding, note names, playhead) — not a
  number in a box somewhere
- "Casual, unthinking, low cognitive load" is a feature, not an accident

#Playfulness. If it makes users smile, it ships. If it makes them think, it
needs justification.

---

### 1. Simple Mode Must Stay Safe

Every new feature must have a sensible default that works silently in Simple
mode. A user who has never configured anything should not hear broken output
because a new feature defaulted to an unexpected state.

**Test**: Open the plugin fresh in Simple mode. Do nothing. Draw a curve. Tap
Play. Does it work without touching the new control?

---

### 2. Named Before Numbered

Never force a user to interact with a raw MIDI number as their primary
interface. When a parameter is numeric by nature (CC#, channel, velocity),
provide at minimum:

- A user-editable text label that stores their own name for the value
- Or a curated set of named presets that covers the most common cases

Raw numbers may be visible in Expert mode. They should not be the only
representation in Standard or Simple mode.

---

### 3. The Canvas is the Product

The curve drawing area is the core interaction of DrawnCurve. It must remain
the largest single element in all modes and all screen sizes.

**Rule**: The canvas must never occupy less than 55% of the total editor height
(308 px out of 560 px). New controls must fit within the existing parameter
rows or in a disclosure section — not between the parameter area and the canvas.

---

### 4. One Control, One Outcome

Each UI control does exactly one thing. Contextual controls that change meaning
based on another setting are difficult to understand and test.

**Existing exceptions** (do not add more of this pattern):
- CC# ↔ Velocity slider swap (when output mode changes to/from Note)
- Speed ↔ Beats slider swap (when Sync toggles)

New features must not reuse a control for a second purpose. Add a new control
or use a secondary panel instead.

---

### 5. Feedback Closes Loops

Any new capability that affects output must have visible confirmation that it
is active. Silence after a settings change is confusing — the user doesn't know
if they changed something or if the plugin is broken.

**Minimum feedback for any new feature**:
- A state indicator (button highlight, label change, or colour shift) showing
  whether the feature is on or off
- Ideally: a visual change in the canvas or parameter display that reflects
  the feature's effect

---

### 6. Progressive Disclosure Over Mode Toggles

Prefer revealing complexity through expandable sections rather than requiring
the user to switch modes to access a control. The ideal form is *contextual*
disclosure: the control appears because the user's current action makes it
relevant (example: scale row appears when Note mode is selected; custom mask
row appears when Custom scale is selected).

Mode tiers (Simple/Standard/Expert) are the *fallback* for controls that have
no obvious contextual trigger. Before assigning a feature to a tier, ask:
"Can this appear automatically when a related control is active instead?"

**Preferred pattern**: Contextual appearance first; then disclosure sections;
then tier gating as a last resort.

**Avoid**: Features that only exist in Expert mode and are completely invisible
in Standard. Custom scales are a good example — they are intriguing to
moderately-engaged users, not just "experts". A user's curiosity about a feature
does not require them to be an expert.

Everything in this document is a hypothesis until user-tested. #TETO.

---

### 7. Real-Time Safety is Non-Negotiable

DrawnCurve runs on the audio thread. New features that affect output must:

- Never allocate memory on the audio thread
- Never acquire a non-real-time-safe lock (use SpinLock or atomics)
- Never call system APIs (file I/O, UI frameworks) from `processBlock()`
- Use the atomic snapshot pattern (`std::atomic<T*>` with release/acquire
  ordering) for any data passed from the UI to the engine

Any feature that requires non-trivial state exchange must go through the
existing `LaneSnapshot` + `GestureEngine::setSnapshot()` pathway or an
equivalent atomic handshake.

---

### 8. Preset Compatibility

New parameters added to the plugin state must:

- Have a documented default value
- Be written to `getStateInformation()` with a versioned key
- Be read from `setStateInformation()` with a fallback to the default if
  the key is absent (for compatibility with older presets)
- Not break the loading of existing saved states

---

## Decision Framework for New Features

When a feature request arrives, answer these questions in order:

```
1. WHO needs this?
   → Which persona(s)? If only Persona E, it belongs in Expert mode only.
   → If Personas C or D, it must work in Simple or Standard without friction.

2. WHAT mode does it appear in?
   → Simple: only if it has a safe, zero-config default AND is a core workflow.
   → Standard: if it's part of the regular use case but needs configuration.
   → Expert: if it requires MIDI knowledge or is infrequently used.

3. WHAT is the default?
   → Define the exact default value before writing any code.
   → The default must produce correct output for Persona D without adjustment.

4. HOW does the user know it's on?
   → Define the feedback mechanism (indicator, label, canvas change).

5. DOES it affect the audio thread?
   → Yes: use atomic snapshot or SpinLock pattern; no memory allocation.
   → No: standard JUCE parameter attachment.

6. DOES it break existing presets?
   → Add version fallback in setStateInformation().
```

---

## Feature Addition Checklist

Run this checklist for every PR that adds a user-visible feature.

### Design
- [ ] Feature is assigned to one or more personas by name
- [ ] Interface mode (Simple/Standard/Expert) is declared in the PR description
- [ ] A default value is documented and justified
- [ ] ASCII or image mockup showing placement in the affected mode(s)
- [ ] Feedback mechanism for the feature is described (how does the user know it's working?)

### Implementation
- [ ] New parameter(s) added to `createParams()` with correct type, range, default
- [ ] New parameter(s) saved in `getStateInformation()`
- [ ] New parameter(s) restored in `setStateInformation()` with fallback
- [ ] Control(s) hidden in modes where the feature is not applicable
- [ ] Canvas height tested in all three modes — still ≥ 308 px
- [ ] No memory allocation on the audio thread
- [ ] No non-RT-safe locks acquired in `processBlock()`

### Content
- [ ] New control(s) added to the help overlay (HelpOverlay in PluginEditor.cpp)
- [ ] Label uses plain language, not raw MIDI jargon, as the primary text
- [ ] Abbreviations (if any) are consistent with the existing style

### Testing
- [ ] New feature works correctly with a fresh (no prior state) plugin instance
- [ ] Existing presets load without error after the change
- [ ] Simple mode produces correct audio output with default settings
- [ ] Expert mode preserves existing behaviour with no regressions

---

## Specific Guidance for Known Upcoming Features

### Multi-stroke (gaps within a single curve)

- **Definition**: The user lifts their finger/Pencil mid-draw and places it
  again further along the X-axis. The result is a single logical curve with
  one or more empty segments — think `V_______m____/` rather than a continuous line.
- **Input model (two candidates; #ToBeTested)**:
  1. Tap-and-hold a "multi-stroke" button in the toolbar while drawing each segment
  2. Hold a software/hardware key modifier during each stroke
- **Empty segment behaviour**: sections where no stroke was drawn produce no MIDI output
  (silence/hold-last-value? → #ToBeTested) for that duration
- **Mode**: Standard and Expert; Simple mode draws only continuous curves
- **Implementation note**: requires `LaneSnapshot` to store a segment list rather than
  a single 256-point table. Non-trivial architectural change; plan before implementation.

---

### Multi-lane (N curves → N targets)

- **Definition**: Multiple independent curves displayed in parallel within the same
  canvas area, each routed to a different MIDI target (different CC, channel, or
  message type).
- **Belongs in**: **Expert and Standard** modes only.
  Simple mode: single curve only; lane selector hidden.
- **Visual differentiation**:
  - Primary: colour-coded (each lane gets a distinct accent colour)
  - Secondary (accessibility): subtle texture or dash pattern on inactive lanes
    so colour is not the only differentiator (#Accessibility)
- **UI**: a horizontal lane strip above (or left of) the canvas — compact coloured
  tabs or circles. Active lane's curve is full-opacity; inactive lanes are ghost traces.
- **Each lane is independent**: own output mode, CC#, channel, scale config, range, direction.
- **Real-time**: engine processes all active lanes per block.
- **Preset**: lane count + all lane snapshots serialised; default is 1 lane (backwards compatible).
- **Major change**: requires significant redesign of both `LaneSnapshot`, `GestureEngine`,
  and the editor layout. Do not begin until multi-stroke is complete or scoped out.

---

### Incoming MIDI display (piano-roll background)

- **Definition**: Note data received by the plugin is drawn in the background of the
  canvas as a real-time "ghost" layer — note numbers on the Y-axis, velocity as
  column height (like a standard piano-roll velocity lane), time flowing left to right.
- **Two interaction modes**:
  1. *Observation*: incoming notes appear passively, helping the user understand
     what pitch range they are working with while drawing a curve over them.
  2. *Redraw loop*: if the input is also the previous output (feedback/loop), the
     user is drawing a new version of the same gesture — #Playfulness through
     visible interplay.
- **Mode**: Expert and Standard (Standard: simplified display; Expert: full velocity).
- **Real-time safety**: incoming MIDI is available in `processBlock`; write to a
  ring buffer for the UI thread to read — no direct UI calls from audio thread.
- **Y-axis alignment**: note numbers in the background should align with the note-name
  labels on the Y-axis when Note mode is active.

---

### Multi-lane (N curves → N targets)

  *(moved here from above — see separate entry)*

---

### Named CC labels (user-defined)

### Named CC labels (user-defined)

- A text input field in Simple and Standard modes
- Stored per-instance (not globally) as part of plugin state
- Does not affect the CC number sent — purely cosmetic
- Expert mode: shows both the label and the raw CC# side by side

### Export / MIDI file

- Expert mode only
- A share button (iOS share sheet) that exports the 256-point table as:
  - MIDI CC automation clip (format 0, 1 bar, 128 time steps)
  - Or plain CSV (time, value)
- No audio thread involvement — purely UI thread + file I/O on share action
- Not part of plugin state; on-demand action only

### Preset recall / named presets within the plugin

- Standard and Expert modes
- A preset slot strip (4–8 labelled slots) below the toolbar
- Each slot stores the full plugin state (curve + all parameters)
- Slots are in addition to, not instead of, the host's own preset system
- Simple mode: slots hidden; host preset system is the only save mechanism

### Playhead position indicator (numeric)

- Standard and Expert: a small "0.0s / 2.4s" readout in the top-right of the canvas
  — this already exists for duration; extend it to show current position
- Simple mode: hidden (reduces cognitive load)

---

## Anti-Patterns to Avoid

| Anti-pattern | Why it's a problem | Better approach |
|---|---|---|
| Hiding controls in a deep menu | Undiscoverable for all personas | Use disclosure rows in-panel |
| Reusing a button for two functions | Violates "one control, one outcome" | Add a separate control |
| Raw MIDI numbers as primary labels | Opaque for Personas C, D | Add user label or named option |
| Allocating memory in processBlock | Real-time safety violation | Pre-allocate; use atomic snapshot |
| Compressing the canvas for new controls | Violates "canvas is the product" | Use disclosure section instead |
| Adding controls with no visible feedback | User doesn't know if it works | Always add a state indicator |
| Requiring configuration before first use | Fails Persona D's zero-config need | Set safe defaults; config is optional |
| Full-screen help overlay that hides the UI | "Push revelation" trap: covers context, gets dismissed | Prefer contextual tooltips (long-press) or a non-blocking sidebar; keep the UI visible behind help |
| Abbreviations as primary label | Violates Nielsen H2 (match real world); T6 task reveals "Aft" is opaque | Full names in Simple; abbreviations only where space forces it, always with tooltip |
| Tier-gating features that "feel curious" | Curiosity is not expertise; scale customization, multi-stroke, incoming MIDI should be accessible below Expert | Use contextual disclosure; tier gates are a last resort |
| Piano-key idiom violation for pitch classes | Users have an established mental model from decades of scale-quantizer UI | Follow the two-row piano layout (5 black / 7 white) for pitch-class selectors; #SolvedProblem |
