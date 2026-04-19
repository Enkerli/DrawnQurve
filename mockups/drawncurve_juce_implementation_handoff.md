# DrawnCurve — JUCE/C++ Implementation Handoff (Performance Instrument)

## Goal

Implement the new performance-instrument UI direction in the existing JUCE codebase without losing the interaction wins already present.

This handoff assumes:
- the current codebase is the starting point
- the plugin remains a single-page AUv3 MIDI effect
- implementation happens incrementally, with visual/behavioral parity preserved where the current UI is already stronger than the prototype concept

## Implementation principles

1. Preserve proven controls
   - segmented direction control
   - unified dual-thumb range control
   - two-row piano-style scale lattice
   - grid ticks
   - current-lane playhead as vertical line + moving dot
   - accidental toggle (♭ / ♯)

2. Refactor before repainting everything
   - separate layout/state concerns from drawing concerns
   - introduce reusable UI primitives where they will reduce churn

3. Keep the plugin usable at every step
   - each phase should compile
   - each phase should leave the editor in a shippable or near-shippable state

4. Prefer summary + disclosure over permanent density
   - stay single-page
   - do not expose all routing and musical detail at once

## Current codebase anchors

The current repo already gives useful anchors for implementation:
- the plugin is a JUCE AUv3 MIDI effect with three independent lanes and per-lane routing/output targets
- the README still describes forward / reverse / ping-pong, dual-thumb range, Note mode quantization, and per-lane playheads
- PluginEditor.h documents the current 640×700 editor split, including left canvas, right column, routing matrix, and note editor area
- PluginEditor.cpp already defines fixed layout constants for the right column, note editor strip heights, routing geometry, theme colors, and scale-family UI structure

These should be treated as scaffolding, not discarded.

## Recommended implementation strategy

## Phase 0 — Freeze behavior and document deltas

Objective:
Establish exactly what must be preserved from the current plugin before layout work begins.

Tasks:
- capture screenshots and short screen recordings of the current UI in default note lane state, Note mode with lattice visible, CC lane state, multi-lane state, paused/running/sync states
- list all behavior currently implemented that must survive the redesign
- explicitly mark what is changing vs what is only being restyled

Deliverables:
- UI behavior checklist
- before/after comparison matrix
- implementation branch for redesign

Why first:
The design prototype already regressed some working details. This phase prevents accidental loss during refactor.

## Phase 1 — Extract reusable visual primitives

Objective:
Reduce PluginEditor.cpp monolith risk by isolating the core UI objects that the redesign depends on.

Recommended new components:
- DirectionSegmentedControl
- RangeSliderDualThumb or RangeObject
- CurveStageComponent
- LaneSummaryRow
- MusicalSummaryBar
- ScaleModeCardStrip
- AccidentalToggle
- TickedYAxis or integrated Y-axis tick renderer
- LanePlayheadOverlay

Tasks:
- move drawing logic for the segmented direction control into its own component if it is not already sufficiently isolated
- extract the dual-thumb range control into a single reusable component
- isolate scale lattice rendering and hit-testing from the editor layout code
- isolate stage overlay rendering (grid, ticks, playheads, hints)

Deliverables:
- components compile independently
- editor still resembles current layout, but uses the new subcomponents

## Phase 2 — Refactor state ownership and naming

Objective:
Clarify what is global, per-lane, and view-only.

Tasks:
- audit current editor member variables and callbacks
- group editor state into three buckets: global UI state, focused-lane UI state, per-lane summary/configuration state
- centralize expansion/collapse flags for musical zone, lane/routing detail, help state
- decide explicitly whether direction and speed remain shared or become per-lane in this redesign path

Critical product decision:
The current README still describes direction and speed as shared across lanes. If the redesign intends per-lane direction behavior, that is a product change and requires processor parameter changes, APVTS migration, host automation compatibility review, and README/help updates.

Deliverables:
- state diagram
- ownership comments in code
- no hidden mystery scope in UI code

## Phase 3 — Rebuild the Stage as the hero area

Objective:
Make the left/main area the unquestioned focal point without losing existing usefulness.

Required retained elements:
- major timing divisions
- minor grid ticks
- Y-axis ticks
- in Note mode: Y-axis labels based on scale note names
- current-lane playhead rendered as a vertical line plus a circle following the lane curve
- secondary-lane visibility without stealing focus
- drawing/capture feedback

Tasks:
- redesign CurveDisplay / stage painting first, before touching routing detail
- add an explicit Y-axis layer: numerical labels for controller lanes, note-name labels for Note mode
- ensure tick spacing is stable under resize
- keep the current lane’s vertical playhead line plus moving dot
- render unfocused lanes with reduced prominence rather than removing them
- preserve Apple Pencil / touch drawing behavior exactly

Deliverables:
- Stage component with updated visuals
- no regression in curve drawing responsiveness
- note labels aligned to range + scale context

## Phase 4 — Restore and formalize the musical zone

Objective:
Implement the musical area as a first-class playful constraint zone rather than a generic row of scale selectors.

Required retained elements:
- two-row piano-style lattice
- proper lattice alignment
- accidental toggle (♭ / ♯)
- mode/family browsing
- recent/custom handling if already present
- note-mode-specific visibility

Tasks:
- keep the existing lattice mental model and geometry
- correct the two-row alignment as an explicit invariant
- replace single-row miniature previews with two-row miniature previews wherever previews are used
- reintroduce or preserve the ♭ / ♯ toggle near root / pitch naming
- make collapsed state summary-oriented, for example C Dorian plus note range summary
- expanded state shows family buttons, mode chips/cards, lattice, accidental toggle, root controls

Deliverables:
- collapsed and expanded musical states
- correct two-row preview language everywhere
- accidental toggle working across lattice and summaries

## Phase 5 — Rebuild the performance zone around signature controls

Objective:
Make the right-side control cluster feel like a compact cockpit instead of a stack of unrelated rows.

Required retained elements:
- segmented direction control in current order: reverse, ping-pong, forward
- unified range object
- visible speed / phase / smooth access
- lane focus clarity

Tasks:
- keep the segmented control order exactly as in the current plugin language
- use the current ping-pong glyph logic, not a generic bidirectional arrow substitute
- make pause state explicit without changing the object identity
- place range as a strong object near the top of the performance cluster
- ensure the lane focus indicator is unmistakable
- use concise labels and compact summaries instead of full-width prose labels

Deliverables:
- stable focused-lane cockpit
- visually clear per-lane ownership
- no ambiguity around paused/running state

## Phase 6 — Rework lane and routing summaries

Objective:
Move from everything exposed to summary first, detail on demand.

Tasks:
- redesign each lane row to support lane color marker, lane type summary, route summary, on/mute state, add-lane invitation where inactive
- keep deeper detail collapsible: CC number, channel, teach, advanced routing controls
- default presentation: Lane 1 active, Lane 2 and 3 as invitations or compact summaries

Suggested lane invitation language:
- Add expression lane
- Add note lane
- Add counterline
- Add modulation lane

Deliverables:
- compact lane stack
- reduced visual noise
- additive discovery path into deeper layering

## Phase 7 — Localization- and accessibility-aware label pass

Objective:
Prevent the redesign from becoming text-heavy while still remaining understandable and localizable.

Tasks:
- audit every visible label
- classify each as always visible, summary only, contextual reveal, or accessibility-only expansion
- keep symbols where conventions are strong
- avoid long labels in persistent UI
- verify French length expansion does not break layout
- ensure controls still expose meaningful accessibility names

Specific checks:
- direction control accessible names
- accidental toggle accessible names
- range endpoints exposed clearly
- note-name and scale-name summaries screen-reader friendly

Deliverables:
- label inventory
- localization-safe strings list
- accessibility naming map

## Phase 8 — Theme and look-and-feel pass

Objective:
Apply the new design language consistently after structure and behavior are stable.

Tasks:
- refine light theme first
- confirm dark theme mapping after light theme is stable
- tune lane ownership colors, panel backgrounds, border visibility, idle vs active contrast, focused-lane emphasis
- make sure the plugin feels inviting and playful, not sterile

Deliverables:
- updated color tokens
- updated LookAndFeel / custom paint refinements
- theme parity review

## Phase 9 — Processor / parameter alignment if scope changes

Objective:
Handle backend changes only if the new interaction model requires them.

Needed only if redesign changes semantics such as:
- direction from shared to per-lane
- speed from shared to per-lane
- musical zone from global to per-lane again
- new lane presets or lane types
- new persistent expansion states

Tasks:
- update APVTS parameters
- migrate parameter IDs carefully
- preserve host automation compatibility where possible
- update serialization/state restore
- update README/help text

Deliverables:
- parameter migration notes
- updated processor/editor contract
- tested state recall

## Phase 10 — QA and fit/finish

Objective:
Verify that the redesign is not merely prettier, but actually safer and clearer.

Test scenarios:
- single note lane, default exploration
- note lane with constrained range
- note lane with multiple scales and accidental toggle changes
- CC lane with learn/teach
- multi-lane playback
- sync on/off
- pause/resume through active segmented control tap
- theme switching
- AUv3 host embedding and resize behavior
- touch hit targets on iPad
- localization stress cases
- screen-reader sanity pass if available

Deliverables:
- bug list
- polish list
- release candidate UI branch

## Recommended file / class touch points

Most likely implementation work will touch:
- Source/PluginEditor.h
- Source/PluginEditor.cpp
- Source/SegmentedControl.h
- Source/ScaleLattice.h
- Source/ScaleData.h
- any custom LookAndFeel helpers
- processor/APVTS files only if interaction semantics change

If the editor remains too monolithic after Phase 1, split additional source files for:
- stage rendering
- lane/routing summary rows
- range object
- musical summary bar

## Suggested step order for the actual coding work

1. freeze and document current behavior
2. extract reusable UI primitives
3. refactor editor state ownership
4. rebuild the Stage
5. rebuild the musical zone
6. rebuild the performance zone
7. rebuild lane/routing summaries
8. do label/localization/accessibility pass
9. do theme/look-and-feel pass
10. only then make any processor/parameter changes
11. finish with QA/polish

That order minimizes regressions and keeps the strongest interactions alive throughout.

## Immediate implementation priorities

Priority 1
- Stage update
- Y-axis ticks and note labels
- current-lane playhead line + dot
- preserve drawing feel

Priority 2
- correct direction segmented control semantics and glyphs
- preserve unified range object
- focused-lane cockpit clarity

Priority 3
- restore musical zone integrity: proper two-row lattice alignment, two-row previews, accidental toggle

Priority 4
- lane summary / invitation redesign

These four priorities will get the redesign closest to the intended product story with the least wasted motion.

## Final implementation rule

Do not treat this redesign as a replacement UI pasted over the plugin.

Treat it as a structured continuation of the current interface, preserving:
- the controls users already learned
- the musical affordances that already work
- the single-page AUv3 reality of the product

The redesign succeeds when the plugin feels more coherent, more playful, and easier to grow without losing the things that already made it interesting.
