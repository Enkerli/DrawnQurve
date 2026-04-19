# DrawnCurve — Performance Instrument Design Specs (v2)

## 1. Product story

DrawnCurve is a **playful gestural MIDI instrument**.

Its primary story is:

**Draw → Constrain → Perform → Layer**

This is not a generic modulation utility. It is an instrument for:
- drawing curves
- hearing looped motion immediately
- shaping musical output through range and quantization
- adding expressive and contrapuntal layers
- learning through visible cause and effect

The interface should therefore privilege:
1. **gesture**
2. **musical constraint**
3. **performance control**
4. **layering**
5. **routing detail**

That order is intentional.

---

## 2. Non-negotiable retained interaction wins

The redesign must preserve these proven gains from the current plugin.

### A. Direction segmented control
Keep the segmented direction control as a signature interaction object.

Required order:
1. **Reverse** = left-pointing triangle
2. **Ping-pong** = opposing left/right triangles
3. **Forward** = right-pointing triangle

Required behavior:
- it remains visually compact and touch-friendly
- it can continue to support per-lane play/pause behavior
- it must clearly show active state and paused state
- it must not be visually recast as a generic transport strip

This is part of the plugin’s visual identity.

### B. Two-row piano-style scale lattice
Keep the two-row pitch-class lattice.

It is valuable because it is:
- idiomatic
- musically legible
- touch-friendly
- useful for custom scale/chord editing
- pedagogically strong

It should be treated as a signature musical object, not an advanced fallback.

### C. Grid ticks
Keep visible timing divisions and grid ticks.

They support:
- rhythmic orientation
- learnability
- repeatable gestures
- performance confidence

The drawing surface should keep:
- clear major divisions
- quieter minor divisions
- visible lane playhead relation to the grid

### D. Unified range control
Keep range as one strong musical object.

It should remain:
- visible
- direct
- tactile
- musically annotated

Range is one of the most satisfying gesture-shaping interactions in the plugin and should remain near the main playflow.

### E. Additive lane logic
Additional lanes are additive to the experience.

The redesign must preserve the feeling that:
- one lane is already rewarding
- another lane deepens the same instrument
- the user is not entering a separate subsystem

---

## 3. Design sentence

**Design DrawnCurve as an inviting touch-native instrument where drawing is the main act, musical constraint makes exploration rewarding, and additional lanes deepen play without overwhelming the first experience.**

---

## 4. Persona framework

Do not design for a single abstract user. Design for three overlapping postures.

## Persona A — Curious Sketcher
Motivation:
- immediate delight
- discovery
- happy accidents
- fast sound-making

Needs:
- one obvious place to start
- audible payoff in seconds
- limited initial complexity
- musically useful defaults

## Persona B — Live Shaper
Motivation:
- performance
- state legibility
- expressive manipulation
- additive layering

Needs:
- clear lane focus
- fast access to direction, range, sync, phase
- readable playheads and timing
- reliable layer interaction

## Persona C — Musical Learner
Motivation:
- understand rhythm, pitch sets, motion, and interaction
- explore structured cause/effect

Needs:
- visible note/range information
- scale lattice
- grid ticks
- concise summaries
- the option for clearer labels and hints

---

## 5. Core user journeys

## Journey 1 — Make it musical fast
1. open plugin
2. see one note lane
3. draw a curve
4. hear notes immediately
5. adjust range
6. switch root / scale
7. move from glissando-like output to motif-like output

This is the default first-run journey.

## Journey 2 — Make it expressive
1. change direction
2. alter speed / sync / phase
3. hear rhythmic and phrasing changes
4. perform with these controls live

This is the second-layer journey.

## Journey 3 — Make it deeper
1. add a second lane
2. route it to CC11 or a second note lane
3. sync or offset it
4. hear emergent interaction
5. continue layering as the same play story

This is the third-layer journey.

## Journey 4 — Learn through visible structure
1. draw a gesture
2. watch playhead relation to ticks
3. narrow range
4. compare scale/chord selections
5. understand why the same gesture yields different melodic or controller results

This should be supported without turning the interface into a tutorial screen.

---

## 6. Overall page model

Remain in a **single-page view**, but with anchored zones and progressive disclosure.

The page should not split into separate screens for basic use. It should remain one composed surface with different densities.

## A. Stage zone
Always visible.
This is the hero.

Contains:
- drawing surface
- curves
- playheads
- note/value grid context
- timing ticks
- active lane identity

## B. Performance zone
Always visible.
This is the user’s active control cluster.

Contains:
- lane focus
- direction segmented control
- range object
- speed
- sync
- phase
- smoothing

## C. Musical zone
Collapsed summary by default, expandable in place.
This is not a buried settings area; it is a playful constraint area.

Contains:
- family / scale / root summary
- expanded two-row piano-style lattice
- mode or chord selection
- custom pitch masking when relevant

## D. Layering / routing zone
Summaries by default, expandable per lane.

Contains:
- lane type
- route summary
- channel / target summary
- lane enable/mute
- deeper route detail when opened

### Single-page rule
Single page, yes.
Single density, no.

---

## 7. Default state

The default state should be a **single visible note lane**.

Why:
- easiest to understand
- strongest playful discovery
- best onboarding
- highlights range and quantization immediately
- avoids the first impression being “three blank routers”

### Default assumptions
- Lane 1 exists and is focused
- Lane 1 output type = Note
- Lane 2 and Lane 3 are present only as invitations
- a constrained range is active
- a musically useful quantization is active
- grid ticks are visible
- the musical zone is summarized but easy to expand

### Recommended default musical state
- family: Diatonic
- scale: Dorian or Major
- root: C
- note range: around one octave to a tenth
- direction: Forward
- speed: moderate

This produces usable melodic behavior quickly and avoids the “full-range chromatic glissando” trap.

---

## 8. Information hierarchy

## Level 1 — Primary
These must dominate visually:
- drawing surface
- active curve
- playhead(s)
- timing grid/ticks
- active lane ownership
- unified range object
- compact musical summary

## Level 2 — Secondary but performative
These should remain visible and easy to touch:
- direction segmented control
- lane focus selector
- speed
- sync
- phase
- smooth

## Level 3 — Expandable musical detail
These become prominent only when engaged:
- expanded scale lattice
- family tabs
- scale/mode cards
- root selection
- custom mask editing

## Level 4 — Layering and routing detail
These should be visible in summary form first:
- lane summaries
- route type
- target/channel summary
- add-lane invitations

## Level 5 — Utility
These remain quiet:
- help
- theme
- panic
- clear/reset
- about-like utilities

### Hierarchy rule
The more the element contributes to playful exploration or live control, the louder it should be.

---

## 9. Scope model: global vs focused lane vs lane row

This remains the most important structural problem to solve.

Use **three signals together**:
1. placement
2. framing
3. color ownership

## Global controls
Visual treatment:
- fixed placement
- neutral tint
- no lane accent
- small uppercase section label `GLOBAL`

Examples:
- host sync context
- global timeline or beat context
- utility actions

## Focused-lane controls
Visual treatment:
- adjacent to stage
- lane-accented
- stronger emphasis
- section label `LANE 1`, `LANE 2`, etc.

Examples:
- direction
- speed if per lane
- range
- phase
- smooth
- lane-specific play state

## Lane-row controls
Visual treatment:
- compact row containers
- lane-colored marker at row start
- neutral interior with concise summaries
- expandable deeper settings

Examples:
- output mode
- target / detail / channel
- teach
- on/mute

### Scope rule
Never rely on color alone.
Never rely on label alone.
Never rely on proximity alone.

---

## 10. Visual language

The plugin should feel:
- inviting
- playful
- precise
- composed
- musical rather than bureaucratic

## Shape language
Prefer:
- softly rounded rectangles
- pills for segmented and summary controls
- circular note/root tokens
- strong single objects instead of scattered microcontrols

## Density
- low-to-medium density in primary zones
- medium density in musical zone when expanded
- higher density only inside expandable route detail

## Motion language
The interface should suggest motion even when idle:
- visible playheads
- curve prominence
- subtle lane tinting
- animated collapse/expand transitions
- responsive drag feedback

---

## 11. Text label strategy

Labels should be reduced, not eliminated.

Text is costly:
- localization
- space
- reduced playfulness
- French expansion in particular

But text also helps:
- accessibility
- onboarding
- learning
- error prevention

The answer is **layered labeling**.

## A. Always-visible text where ambiguity is high
Keep visible labels for:
- Range
- Smooth
- Phase
- Teach
- Root
- Family

## B. Symbol-first where convention is strong
Prefer symbols for:
- reverse / ping-pong / forward
- lane markers
- help / theme / panic utilities

## C. Summary text instead of repeated labels
Prefer compact summaries like:
- `C Dorian`
- `C3–D4`
- `Lane 2 · CC11`
- `Chordal · Maj7`

## D. Contextual label reveal
Show fuller labels when:
- focused
- expanded
- hovered / long-pressed
- accessibility mode is enabled
- a learning-friendly presentation mode is active

### Label rule
Do not force every function into text.
Do not force every function into pure iconography.

---

## 12. Symbol system

## Keep / adopt

### Direction
Required order and symbols:
- Reverse = `◀`
- Ping-pong = `◀▶`
- Forward = `▶`

The ping-pong symbol may use stylized opposing triangles, but the meaning must be immediately readable.

### Lane ownership
Use:
- lane number
- lane dot/color

### Output type
Keep abbreviated text:
- `♪` or note glyph may support Note mode
- `CC`
- `PB`
- `Aft`

Do not over-symbolize these.

### Root / pitch
Use note names directly.

### Grid / beat context
Use visible ticks and beat divisions, not icon substitutes.

### Utilities
Keep symbols compact, but ensure discoverability via:
- help overlay
- tooltips / long-press labels
- accessibility text

---

## 13. Typography

Use SF Pro.

## Typographic roles

### Hero state
Use for:
- active scale summary
- lane title
- compact musical feedback

### Section labels
Should be:
- small
- uppercase
- lightly tracked
- subdued

Examples:
- `GLOBAL`
- `PERFORMANCE`
- `MUSICAL`
- `LANES`

### Control text
Should be:
- concise
- medium or regular weight
- non-technical where possible

### Numeric readouts
Should be:
- stable
- compact
- aligned when possible

---

## 14. Color system

Use a restrained neutral base plus vivid lane accents.

## Base
- warm/light background
- slightly darker stage field
- quiet separators
- minimal hard borders

## Lane accents
Each lane gets one strong accent.

Use lane color for:
- lane selector state
- active playhead
- focused-lane panel accent
- lane row ownership marker
- some scale/range highlights when associated with that lane

### Color rule
Lane color means ownership.
Neutral means system/global.
Avoid too many semantic colors beyond that.

---

## 15. Stage design requirements

The stage should retain visible musical/rhythmic structure.

Required:
- major timing divisions
- minor ticks or quieter subdivisions
- visible pitch/value banding where appropriate
- strong active curve prominence
- active playhead visibility
- unfocused lane curves visibly secondary but still informative
- lane focus unmistakable

The stage should feel rewarding to draw on and useful to watch.

---

## 16. Unified range object requirements

Range is a signature object and should remain visually strong.

Required:
- one unified control object
- min and max note endpoints visible
- direct manipulation feel
- placement close to stage/performance zone
- optional live highlighting of affected note region on the stage

Do not split range into disconnected values unless absolutely necessary for implementation.

---

## 17. Musical zone requirements

The musical zone should have two states.

## Collapsed state
Shows:
- current family / scale / root summary
- current range summary
- perhaps active note count / chord summary where useful

## Expanded state
Shows:
- family tabs
- scale/mode/chord choices
- two-row piano-style lattice
- root selection
- optional custom edit actions

This expanded state should still feel playful, not like opening a dense inspector.

---

## 18. Performance zone requirements

Always visible controls should prioritize what feels performative.

Recommended always-visible items:
- lane focus / lane add
- direction segmented control
- unified range object
- one or two motion controls such as speed and phase
- sync state if central to the workflow

Phase and smoothing can remain secondary within the performance cluster, but should still be readily accessible.

---

## 19. Layering / routing requirements

Additional lanes should be revealed through invitation.

## Default presentation
Show invitation objects such as:
- `Add expression lane`
- `Add note lane`
- `Add modulation lane`
- `Add counterline`

This is better than exposing all blank routing detail immediately.

## After activation
A lane gets:
- color ownership
- summary row
- default route preset
- focus affordance
- expandable deeper route detail

### Route detail model
Collapsed:
- lane type
- target summary
- on/mute state

Expanded:
- teach
- detail value (CC, etc.)
- channel
- advanced route choices

---

## 20. Accessibility / localization principles

The design should be localization-aware from the start.

Required:
- avoid long persistent labels
- prefer compact summaries where possible
- allow longer descriptive text to appear contextually
- ensure screen-reader-compatible naming in implementation
- preserve the possibility of an accessibility-friendly label mode
- avoid icon-only interactions for ambiguous controls

The design should remain playful without becoming cryptic.

---

## 21. Prototype requirements for design validation

The functional/dynamic prototype should demonstrate:
- correct direction control order
- unified range object
- visible grid ticks
- expanded two-row piano-style lattice
- default single note lane
- lane invitation model
- compact summary labeling
- collapsed vs expanded musical zone
- collapsed vs expanded lane/routing summaries
- clear global vs focused-lane distinction

The prototype does not need audio.
It must demonstrate hierarchy, state, invitation, and playflow.

---

## 22. Final design criteria

A proposed interface direction is successful if:

- it preserves the current design’s real interaction wins
- it makes one-lane note exploration immediately rewarding
- it elevates range and quantization as core musical play
- it keeps direction control as a signature object
- it supports additive lane discovery without intimidation
- it stays single-page without becoming equally dense everywhere
- it remains friendly to localization and accessibility
- it supports exploration, performance, and learning from the same composed surface
