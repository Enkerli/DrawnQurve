# DrawnCurve — Feature Roadmap

*Last updated: 2026-03-22*

A consolidated feature inventory merging all planned work, backlog items, and exploratory ideas. Items are split between **this plugin** and **sibling projects** that would share the codebase but have a distinct plugin identity. Priority within each section is informed by the playflow perspectives below rather than by user "skill level."

---

## Contents

1. [Plugin Family Vision](#plugin-family-vision)
2. [Playflow Perspectives](#playflow-perspectives)
3. [Pending Fixes](#pending-fixes)
4. [Roadmap: This Plugin](#roadmap-this-plugin)
   - [Infrastructure](#infrastructure)
   - [User Interface](#user-interface)
   - [Curve Capabilities](#curve-capabilities)
   - [Musical Intelligence](#musical-intelligence)
   - [MIDI Routing & Protocol](#midi-routing--protocol)
   - [Per-Lane Architecture](#per-lane-architecture)
   - [Data Management](#data-management)
   - [Platform & Quality](#platform--quality)
5. [Sibling Projects](#sibling-projects)
6. [Effort Reference](#effort-reference)

---

## Plugin Family Vision

The codebase at its core is a **gesture engine**: it captures touch/Pencil paths, converts them to a lookup table, and plays them back as MIDI or audio-rate output. This engine is general enough to power several distinct plugins, each with its own creative identity.

**DrawnCurve** (this plugin) is the MIDI expression tool: draw curves, loop them, route them to synth parameters.

Some ideas require enough architectural divergence that they are better maintained as separate plugin targets sharing the core engine library, rather than as modes inside this plugin.

| Plugin | Identity | What changes from this plugin |
|--------|----------|-------------------------------|
| **DrawnCurve** (this) | MIDI effect: draw curves, loop, route | — |
| **DrawnCurve XY** | Two CCs from one gesture | X and Y axes as independent outputs; CurveDisplay becomes an X–Y plane |
| **DrawnCurve Audio** | Audio-rate LFO / CV source | Plugin type → audio output; same curve engine, different output path |
| **DrawnCurve FX** | Audio effect prewired to receive curves | Audio I/O; the "receiving" counterpart to this plugin |
| **DrawnCurve Morph** *(v2 candidate)* | 3D morphable curve tables | Multiple curves per lane forming a morph dimension; possible wavetable export |

XY gestures and audio-rate output are different enough in architecture (and in the user's mental model) that keeping them as siblings rather than modes is cleaner. The Transform mode previously discussed (remap incoming CC through the drawn curve as a transfer function) maps most naturally onto **DrawnCurve FX** rather than a mode here.

---

## Playflow Perspectives

These describe **workflows**, not skill levels. A beginner can be a Gesture Performer; an expert can be a Curve Builder. Each playflow highlights which features matter most.

### 1 — The Gesture Performer
*"I play the curve like an instrument — while the music is happening."*

Draws curves live during performance, or pre-draws and fires them on cue. The curve is a real-time instrument, not a preset. Needs instant play/pause, per-lane muting, MIDI-triggered capture (hands-free recording), legato and portamento for expressive note output, and rock-solid tempo sync. Theremin mode (note + PitchBend as linked lanes) is a centrepiece for this workflow.

**Priority features:** per-lane pause, MIDI-triggered capture, legato, theremin mode, large touch targets, reliability.

### 2 — The Curve Builder
*"I want a library of shapes I can pull up, name, and combine."*

Spends time creating, refining, and organising curves, then performs by selecting and routing them. Needs: save/load individual curves, a browsable library with mini-previews, copy/paste between lanes, editing tools for post-capture tweaking, and preset mathematical shapes as starting points.

**Priority features:** curve library, mini-thumbnails, editing tools, preset shapes, copy/paste, undo/redo.

### 3 — The Harmony Navigator
*"I'm improvising over chord changes — the scale quantizer is the point."*

Uses DrawnCurve to constrain gesture output to the right notes at the right moment. Chord progressions, NCT handling (notes that are in the scale but outside the chord), per-lane scale differences for counterpoint, and tuning system support are central. The lattice is the primary UI element; it needs to be legible and expressive.

**Priority features:** chord progressions, NCT distinction, MTS-ESP, per-lane scale, scale lattice size (bigger bitmask UI).

### 4 — The Modulation Architect
*"Lane 1 is filter cutoff, lane 2 is resonance, lane 3 is reverb send — they move together."*

Multi-lane, multi-target. Precise per-lane control over CC assignments, ranges, smoothing. Wants phase offset for polyrhythmic layering, routing presets for synth setups, and a clear "All Lanes" master override. The routing matrix is this persona's main workspace.

**Priority features:** per-lane architecture, routing database, predefined MIDI maps, phase offset, "All Lanes" master controls, phase sync.

### 5 — The Playful Creator
*"I want to try things — I don't necessarily know what will happen."*

Exploratory, low-commitment sessions. Snake/physics brush makes drawing feel alive and surprising. Preset shapes give a non-intimidating starting point. Chord progressions become a musical toy (not just a tool). Randomise + jitter = generative territory. Themes and visual feedback are part of the pleasure.

**Priority features:** snake physics brush, preset shapes, humanize/jitter, one-shot mode, themes, collapsible UI.

### 6 — The System Builder
*"This needs to talk to everything else in my setup."*

Integrates DrawnCurve with modular, DAW automation, OSC tools, and hardware. Wants OSC output, MIDI-triggered capture, MIDI 2.0/MIDI-CI (when the ecosystem is ready), and MTS-ESP for microtonal rigs. All buttons as exposed/automatable parameters is essential.

**Priority features:** OSC, MIDI-CI readiness, all buttons as MIDI-triggerable parameters, MTS-ESP, MIDI panic, export.

---

## Pending Fixes

Active bugs not yet resolved.

| # | Issue | Effort |
|---|-------|--------|
| F1 | **AudioQueue -50 noise** — skip probe entirely, always use `session.sampleRate` | S |
| F2 | **Bitmask convention** — reverse bit order (bit 11 = root/C, bit 0 = B) throughout engine, presets, and lattice | M |
| F3 | **Playback direction in sync mode** — buttons non-functional, always ping-pong | M |
| F4 | **Default to Light mode** — change `_lightMode { false }` to `{ true }` | Trivial |

---

## Roadmap: This Plugin

### Infrastructure

| # | Item | Effort | Notes |
|---|------|--------|-------|
| I1 | **Rename repo** | S | Rename folder and Git remote. Audit: CMakeLists plugin name, bundle ID (`com.enkerli.*`), Info.plist, any hardcoded strings in source. The bundle ID is the ABI-stable identifier; the folder name is cosmetic. Nothing should break if only the folder/remote changes. |
| I2 | **Increase curve table resolution** | M | Current: 256 points. Stepping to 1024 improves accuracy for fast transient-style curves and positions the codebase for the audio-rate sibling. Touches `LaneSnapshot`, `GestureCaptureSession`, and `sampleCurve`. |
| I3 | **Desktop formats — VST3, AU (macOS), CLAP** | L per format | JUCE `formats` list in CMakeLists. AU macOS is closest to AUv3; VST3 is straightforward; CLAP requires the community JUCE module. Bundle them as a "desktop release" milestone; each format needs its own platform test pass. |
| I4 | **MIDI 2.0 / MIDI-CI** | XL | JUCE 7+ has partial UMP support; AUv3 iOS 18 exposes MIDI 2.0 streams. MIDI-CI (property exchange, profile negotiation) is the discovery and capability layer of MIDI 2.0 — it is what will make automatic routing and parameter mapping practical between devices. Implementation here should be manageable once the ecosystem (synths, FX, hosts) actually supports it. Track readiness; design the implementation against real targets. |

---

### User Interface

| # | Item | Effort | Notes |
|---|------|--------|-------|
| UI1 | **More themes** | M | Add: OLED black (true black background, no greys), High-contrast (accessibility, thick borders, no transparency), optionally a user-accent theme (one colour picker; rest derived). The current `applyTheme()` architecture extends cleanly. Each preset theme is ~20 colour constants. |
| UI2 | **Collapsible sections** | M | Routing matrix, scale editor, and transport row as natural fold points. A chevron button per section; animated or instant resize. Expands the curve display area on smaller devices. |
| UI3 | **Fullscreen curve area** | M (after UI2) | A "zoom" button that effectively collapses all sections except a minimal strip, giving the curve display the full height. Useful during performance and exploratory drawing. |
| UI4 | **Icons for controls** | M total | Notehead for Note mode; sine wave for CC; bidirectional arrow for PitchBend; hand/pressure icon for Channel Pressure; broom for Clear; double-arrow for Sync. Design language: the lattice's filled/hollow circle buttons as the vocabulary. The notehead and sync arrow are already established in the codebase. Path-drawn in `SymbolLF::drawButtonText`. Seek outside icon help for final polish. |
| UI5 | **Direction control redesign** | M | Current: triangle arrows. Proposed: three filled/hollow circles on a horizontal ledger line (like note heads on a staff) — left circle = Forward, right = Reverse, centre = Ping-Pong. The selected circle fills; others are hollow. More musical, more legible at small sizes, consistent with the lattice circle vocabulary. |
| UI6 | **Scale bitmask UI — slightly larger** | S | The 12-circle lattice is correct but a touch small. Increase circle diameter and component bounds; no logic change. |
| UI7 | **Curve mini-thumbnails in routing matrix** | M | A ~40×18 px silhouette of each lane's curve table, rendered in the lane's colour. "Shape at a glance" without switching focus. Uses `getCurveTable(lane)`. |
| UI8 | **Custom scale mini-preview** | S (after UI7) | Same thumbnail idea for the active custom scale: a tiny strip showing which of the 12 pitch classes are enabled. Useful when the full lattice is collapsed. |
| UI9 | **Editing tools for drawn curves** | XL | Post-capture manipulation: scale vertically (amplitude), shift horizontally (phase), smooth a selection, flip, mirror, time-stretch. Edit overlay on `CurveDisplay` with drag handles. Transforms operate on the `table` array. High value for the Curve Builder persona; substantial interaction surface. |
| UI10 | **Preset / "snappy" shapes** | M | A strip of mathematical shapes (sine, triangle, sawtooth, exponential, S-curve, random-step) insertable as a starting point or full replacement. Each is a function `[0,1]→[0,1]` sampled into the table. Pairs naturally with editing tools for a "start from shape, refine by drawing" workflow. |
| UI11 | **"Snake / physics" brush** | L | Applies inertia and damping to the drawn path during capture. A spring-mass simulation runs over raw input points in `finalizeCapture`, making the curve trail and overshoot the stylus. Creates natural-feeling glides that are hard to draw freehand. Parameters: mass, damping (exposed as a brush-style picker or two sliders). Disable/enable toggle. |
| UI12 | **MIDI panic button** | S | All-notes-off on all active channels. Long-press on Clear, or a small dedicated button. Already documented in `PLAN.md`. |
| UI13 | **All buttons as MIDI-triggerable parameters** | L | Every button (Play/Pause, Clear, lane select, direction, sync toggle, etc.) becomes an APVTS parameter with MIDI learn or trigger assignment. Enables full external control from a footswitch or controller. Essential for Gesture Performer and System Builder personas. |

---

### Curve Capabilities

| # | Item | Effort | Notes |
|---|------|--------|-------|
| C1 | **Segmented curves — "Hold" latch** | M | A "Hold" toggle keeps the recording session alive even when the finger or Pencil is lifted. While held, the curve is flat at the last captured value. Recording ends only when Hold is released or "Done" is tapped. Requires a `heldValue` state in `GestureCaptureSession`. Creates curves with deliberate silences and pauses embedded in a single take. |
| C2 | **Discontinuous mode — silence between cycles** | S (after C1) | A "gate" amount per lane: the last N% of each loop cycle outputs zero (CC) or note-off (Note), creating rhythmic pulsing. Implemented in `processLane` by clamping near the end of the phase. |
| C3 | **One-shot mode** | S | Lane plays through once and stops rather than looping. Loop modes per lane: Loop / One-shot / Bounce-once. `processLane` calls `stopLane()` at end-of-cycle instead of wrapping phase. |
| C4 | **MIDI-triggered capture** | M | Designate a trigger note (configurable per lane or globally). An incoming note-on begins capture; note-off (or a second note-on) finalises it. Hands-free recording during performance. Requires `acceptsMidi()` and scanning incoming MIDI in `processBlock`. |
| C5 | **Timing quantize** | M | Post-capture pass: redistribute the time axis of raw input points to a rhythmic grid (8th, 16th, triplet, etc.). Makes wobbly human gestures mechanical. A "humanize" inverse adds controlled irregularity to an otherwise regular curve. |
| C6 | **Groove / rhythm template** | L | A secondary lookup table (16–32 points) warps the curve's phase during playback: phase advances non-uniformly, mimicking a groove template. Concept: identical to Ableton's groove system, applied to the playhead rather than MIDI clip events. |
| C7 | **Phase offset per lane** | S | A 0–100% parameter that offsets each lane's playhead start position within the loop cycle. Small engine change; high value for polyrhythmic layering between lanes. |
| C8 | **Curve morphing (2D)** | XL | Each lane holds two snapshots (A / B). A `morph` parameter interpolates between them sample-by-sample in `sampleCurve`. Morph can be MIDI-controlled or driven by another lane's output. Foundation for the DrawnCurve Morph sibling (3D table). |
| C9 | **Output steps / stairs quantize** | S | Snap continuous output to N equal steps (1–128). Per lane. Creates vintage sequencer / sample-and-hold texture without Note mode. |
| C10 | **Humanize / jitter** | S | Per-lane random offset added to output each block. Disabled automatically in Note mode to prevent spurious note-ons. |
| C11 | **Apple Pencil pressure and tilt** | M | Pressure is already captured in `addCapturePoint`. Tilt (azimuth/altitude) is accessible via `UITouch.azimuthAngle` and `altitudeAngle` on iPadOS. Map tilt to a secondary parameter (e.g. velocity scaling, smoothing amount) during recording. |

---

### Musical Intelligence

| # | Item | Effort | Notes |
|---|------|--------|-------|
| M1 | **Per-lane scale** | M | Engine already supports `_scalesPacked[lane]`; needs 3× APVTS params and per-lane lattice UI. |
| M2 | **Legato mode** | M | For monophonic synths and MPE: send note-on for the new pitch before note-off for the old one. `legatoMode` bool per lane. Note case in `processLane` defers the note-off until the next attack is ready. |
| M3 | **Portamento / glide** | M | Two sub-modes: (a) MIDI CC65 (portamento switch) + CC5 (time) — let the synth handle the slide; (b) PitchBend glide — hold the note and bend pitch toward the next scale degree before re-attacking. Sub-mode (b) is the mechanical foundation of theremin mode (M4). |
| M4 | **Theremin mode (Note + PitchBend)** | M (after per-lane) | Two linked lanes: Lane A sends scale-quantized notes; Lane B sends PitchBend offsets for a continuous pitch voice. Combined: smooth gliding line that still resolves to scale roots. Requires a "link to lane N" parameter so Lane B knows which channel/note to bend. |
| M5 | **Chord progressions / leadsheet** | XL | Time-varying chord sequence over the curve's timeline. At each chord boundary, the note quantization target changes. Needs a mini chord-sequence editor (chord symbol + beat duration). This is a direct bridge to MIDIcurator's domain; a shared chord format would enable importing progressions. The Harmony Navigator persona makes this a centrepiece feature. |
| M5a | **Non-Chord Tones (NCTs)** | M (after M5) | Within chord-progression mode: notes that fall in the current scale but outside the chord (suspensions, passing tones, neighbour tones, added tensions) are not snapped to chord degrees — they pass through, or are handled by a dedicated policy (approach from above/below, hold, resolve). Visually distinguished in the curve display (different colour or marker style) so the player can see where NCT territory lies in their gesture. |
| M6 | **Tuning systems / MTS-ESP** | L (after M2 + M4) | Once note + PitchBend and MPE are in place, note quantization can target non-12-TET pitches by using PitchBend offsets (or MPE per-note bend) to reach microtonal scale degrees. MTS-ESP (MIDI Tuning Standard, Extended Precision) is the de-facto plugin standard for tuning tables; it is the kind of support used to select commercial instruments. Supporting it in the note output path makes DrawnCurve a first-class microtonal performance tool. |
| M7 | **MPE output** | L | Multi-channel note output with per-note pitch bend, pressure, and slide (MIDI Polyphonic Expression). Each Note lane gets its own MPE zone channel. Pairs with Legato (M2) and Theremin (M4); prerequisite for full MTS-ESP integration (M6). |

---

### MIDI Routing & Protocol

| # | Item | Effort | Notes |
|---|------|--------|-------|
| R1 | **Predefined MIDI maps** | M | A bundled database of common synth CC assignments (Moog Grandmother, Korg Prologue, DX7, Virus TI, etc.). Selecting a synth pre-fills CC#/channel for each lane. Stored as JSON/XML alongside the plugin bundle; users can add their own entries. |
| R2 | **Routing/mapping library** | M (overlaps R1) | User-editable named configurations ("My Korg Prologue rig"). Save/load independently of curve presets. File-system-based; same library browser pattern as curve library (D3). |
| R3 | **OSC output** | L | Parallel to MIDI output — the same ranged float value sent as an OSC message to a configurable IP:port. Uses the JUCE OSC module. Connects DrawnCurve to Max/MSP, TouchDesigner, Pd, and similar tools without a MIDI bridge. |

---

### Per-Lane Architecture

The structural prerequisite for a large cluster of features in other sections. Already partially implemented; full per-lane parametrisation is the goal.

| # | Item | Effort | Notes |
|---|------|--------|-------|
| L1 | **Per-lane scale** | M | See M1. Engine already ready; needs APVTS params and UI. |
| L2 | **Per-lane direction** | M | Direction parameter keyed per lane. |
| L3 | **Per-lane speed + sync toggle** | L | Speed multiplier and independent host-sync toggle per lane. A lane can free-run while another syncs to host tempo. Enables "fire a lane at will" independently of other lanes. |
| L4 | **Per-lane play/pause** | M (after L3) | Play/Pause per lane strip; per-lane `_userManualPauseInSync` latch already partially implemented. |
| L5 | **"All Lanes" master parameters** | L (after L2 + L3) | Master speed, master sync, master play/pause override per-lane defaults but each lane can individually diverge. Clear hierarchy: master → per-lane. |
| L6 | **Tab / panel UI for lanes** | L (after L5) | Dedicated expanding panel per lane with full per-lane controls. Current routing matrix becomes a compact overview; tapping a lane opens its detail panel. |
| L7 | **More than 3 lanes (up to 8)** | M (after L6) | Bump `kMaxLanes`; compact routing matrix handles overflow with horizontal scroll or pagination. Especially useful with a mix of Note and CC lanes — e.g. one melody lane + three independent modulation lanes. |
| L8 | **Phase offset per lane** | S | See C7. |
| L9 | **Phase sync between lanes** | M | A "snap to lane 0" option that re-locks a lane's phase to the master on the next cycle boundary. Enables rhythmic synchronisation after independently-started lanes drift. |

---

### Data Management

| # | Item | Effort | Notes |
|---|------|--------|-------|
| D1 | **Save individual curves** | S | Serialise a single `LaneSnapshot` (table + params) to a named file. < 4 KB per curve; simple binary or JSON with a version header. |
| D2 | **Load curves into a lane** | S (after D1) | File picker loads a saved curve into the focused lane. Replaces that lane's snapshot; other lanes unaffected. |
| D3 | **Curve library / browser** | L | Browsable list of saved curves with mini-thumbnails (UI7) and filter by message type, tag, and date. Drag from library onto a lane. File system is the database. The main effort is the browser UI, not the serialisation. |
| D4 | **Full preset management** | L | Save/load entire plugin state (all lanes, all params) as a named file independent of the DAW session. Builds on `getStateInformation` / `setStateInformation`. |
| D5 | **Copy/paste curve between lanes** | S | In-memory clipboard using a cloned `LaneSnapshot*`. |
| D6 | **Undo/redo for drawing** | M | Ring buffer of `LaneSnapshot*` per lane (depth ~8). Push on `finalizeCapture`; pop on Undo gesture or button. |
| D7 | **Export curve as MIDI clip or CSV** | M | Write the 256-point table as a MIDI CC automation clip (standard MIDI file format 0) or a plain CSV for import into DAWs, Max/MSP, or spreadsheet tools. Useful for the Sound Designer persona. |

---

### Platform & Quality

| # | Item | Effort | Notes |
|---|------|--------|-------|
| Q1 | **Accessibility** | L | VoiceOver labels on all controls, 44 pt minimum touch targets, colour-blind-safe themes, no information conveyed by colour alone. Lane colour dots are a current violation (they also need shape or label differentiation). |
| Q2 | **Usability testing** | Ongoing | Structured sessions with musicians new to the plugin. Priority: first-run experience, lane switching, understanding the loop concept. The Harmony Navigator and Playful Creator personas are currently the least well served by the UI. |
| Q3 | **Automated tests for the engine** | L | `GestureEngine` is pure C++ with no host dependencies — directly unit-testable without a DAW. Cover: `quantizeNote`, `sampleCurve`, phase wrapping, note-off cleanup, hysteresis boundaries, multi-lane phase independence. |

---

## Sibling Projects

These share the gesture engine and curve infrastructure but are distinct plugin identities. Each would live in a separate CMakeLists target or repository.

---

### DrawnCurve XY
*Two CCs from one gesture.*

The drawn path's X and Y positions independently map to two CC values simultaneously. Time is implicit — the curve is traversed in drawing order. `CurveDisplay` becomes an X–Y plane. Natural use cases: filter cutoff + resonance, pan + send level, any two-dimensional modulation target.

Key differences from this plugin: a second output CC parameter per lane; a new "XY mode" interpretation in `processLane`; `CurveDisplay` paints an X–Y scatter view rather than time vs. value. The gesture capture infrastructure is identical.

---

### DrawnCurve Audio
*Audio-rate LFO / CV source.*

Same curve engine; plugin type changes from MIDI effect to audio source (`isMidiEffect = false`, audio output channels declared). The curve table is read at sample rate to produce an audio signal — an LFO waveform, oscillator shape, or modulation source for a CV-capable audio interface.

Meaningful table resolution increases to 2048+ points (see I2). Fundamentally different plugin identity, host routing, and App Store category. Much of the MIDI-specific code (note handling, CC routing) is replaced by audio buffer output.

---

### DrawnCurve FX
*Audio effect prewired to receive curve data.*

The "receiving" counterpart. An audio processing plugin (filter, tremolo, pitch shift, EQ tilt, saturation…) whose key parameter is controlled by a curve received from DrawnCurve via AUv3 parameter sharing or Inter-App Audio, or drawn directly within the plugin itself. "Prewired" means the routing setup that currently requires external configuration is built in.

Also covers the "modulation transformation" concept discussed in planning: incoming MIDI CC is remapped through a drawn curve acting as a transfer function (draw an exponential curve → linear CC becomes exponential response). The receiving/transform use case fits naturally here rather than as a mode in this plugin.

---

### DrawnCurve Morph *(or DrawnCurve v2)*
*3D morphable curve tables.*

Multiple curves per lane form a morph dimension — an array of `LaneSnapshot` tables indexed by a `morph` parameter (0–1). Sweeping through the morph axis interpolates between curve shapes. Natural extension of C8 (2D curve morphing) into a full 3D wavetable-style structure.

Candidate for wavetable export to Serum, Vital, or similar synths. Whether this ships as a major version of this plugin or a separate product depends on how much the UI diverges from the current drawing-and-looping metaphor.

---

## Effort Reference

| Label | Approximate time |
|-------|-----------------|
| Trivial | < 1 hour |
| S — Small | 1–4 hours |
| M — Medium | half day – 1 day |
| L — Large | 2–4 days |
| XL — Extra large | 1–2 weeks |
| Epic | months |

Effort estimates assume a single developer familiar with the codebase. UI-heavy items (curve library, editing tools, per-lane tab UI) tend to take longer than their algorithmic complexity suggests.

---

*See also: `docs/personas.md` for detailed user profiles · `docs/design-decisions-log.md` for rationale on past decisions · `PLAN.md` for the multilane implementation plan.*
