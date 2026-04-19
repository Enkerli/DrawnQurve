# DrawnQurve — Idea Catalogue

*Started: 2026-03-29 · Living document — add freely, remove nothing*

This is where ideas live before they earn a row in `ROADMAP.md`. The tone is exploratory — we're thinking out loud. Some of these are half-formed, some contradict each other, and that's fine. The point is to have them written down so we can return to them with fresh eyes.

Cross-references to the [ROADMAP](../ROADMAP.md) use its item codes (M4, I4, etc.).

---

## Contents

1. [The Qurve-Native Synth](#1-the-qurve-native-synth)
2. [The Plugin Family](#2-the-plugin-family)
3. [MIDI Input as Creative Material](#3-midi-input-as-creative-material)
4. [Beyond 12TET](#4-beyond-12tet)
5. [Segmented Qurves and Accents](#5-segmented-qurves-and-accents)
6. [Curve Curation](#6-curve-curation)
7. [Cross-Lane Modulation](#7-cross-lane-modulation)
8. [Connectivity](#8-connectivity)
9. [For Testers: The Synth Question](#9-for-testers-the-synth-question)
10. [Why Quantization? A Rationale](#10-why-quantization-a-rationale)
11. [Rhythm, Groove, and Serpe](#11-rhythm-groove-and-serpe)
12. [Harmonic Intelligence and MIDIcurator](#12-harmonic-intelligence-and-midicurator)
13. [Circle of Fifths and PCS Navigation](#13-circle-of-fifths-and-pcs-navigation)
14. [GUI and Look & Feel](#14-gui-and-look--feel)
15. [Out-of-Scope Ideas (Documented Anyway)](#15-out-of-scope-ideas-documented-anyway)
16. [Ecosystem: Comparisons and Inspirations](#16-ecosystem-comparisons-and-inspirations)

---

## 1. The Qurve-Native Synth

The single biggest expansion on the horizon. People *will* ask "where's the built-in synth?" during alpha testing, and we should have a thoughtful answer — not just "maybe later."

### Why build one?

DrawnQurve currently speaks MIDI to external synths. That works, but it means the thing that makes qurves special — smooth continuous change as a first-class citizen — gets filtered through whatever resolution the receiving synth offers. A qurve-native synth could consume the curve data directly, bypassing MIDI's quantization entirely for internal parameters.

### Engine sketch

- **Wavetable / wavefolding** core. Timbre is the obvious thing qurves should control, and wavetable position + wavefolding amount are inherently continuous parameters. Pulse-width modulation fits too.
- **Filter cutoff** as a qurve target. The most intuitive "I drew this shape and I hear it" mapping.
- **One-shot qurves as envelopes.** Instead of traditional ADSR, you draw the envelope. This reuses the existing curve infrastructure wholesale — a one-shot lane *is* an envelope. The playback mode already exists (see ROADMAP C3).
- **Sharp attacks are the hard part.** Drawing a fast transient on a touchscreen is awkward. We'll need either a dedicated attack control (a knob that shapes the first N ms independently of the drawn curve) or a way to "stamp" a sharp onset onto the beginning of a qurve.
- **Theremin-like continuous pitch.** The synth should be able to track a qurve's continuous output as pitch — no quantization, no steps, just a smooth frequency line. This is the full realisation of the theremin mode concept (ROADMAP M4), but without the MIDI pitch-bend resolution ceiling.
- **Accepts MIDI input.** The synth isn't a closed system. It receives notes (from a keyboard, from another app, from its own qurves) and uses them. This opens the arp/collection dimension below.
- **Smoothness is key to the experience.** The whole point is that parameter changes feel *drawn*, not stepped. Audio-rate interpolation everywhere.

### Relationship to the plugin family

The [ROADMAP's sibling plugins](#2-the-plugin-family) already sketch adjacent ideas: DrawnCurve Audio (audio-rate LFO), DrawnCurve FX (audio effect with curve-controlled params). The internal synth is arguably the convergence point — it's what happens when you stop routing MIDI *out* and start routing qurves *in*. Whether it ships as a mode in this plugin, a sibling, or a separate product depends on how heavy the audio engine becomes.

---

## 2. The Plugin Family

*Absorbed from ROADMAP.md and expanded.*

The gesture engine at the core — capturing touch/Pencil paths, converting them to lookup tables, playing them back — is general enough to power several distinct plugins. Each would share the engine library but have its own creative identity.

| Plugin | Identity | Key divergence |
|--------|----------|----------------|
| **DrawnQurve** (this) | MIDI looper: draw qurves, loop, route | — |
| **DrawnQurve XY** | Two CCs from one gesture | X–Y plane display; two independent outputs per lane |
| **DrawnQurve Audio** | Audio-rate LFO / CV source | Plugin type changes to audio output; table resolution ≥2048 |
| **DrawnQurve FX** | Audio effect with curve-controlled params | Audio I/O; the "receiving" counterpart; also the transfer-function / remap use case |
| **DrawnQurve Morph** | 3D morphable curve tables | Multiple snapshots per lane with a morph dimension; wavetable export potential |
| **DrawnQurve Synth** *(new)* | Qurve-native synthesizer | See section 1 above; the internal synth as a standalone instrument |

The XY and Audio siblings are architecturally close to the current codebase. The Synth is the most ambitious. FX and Morph sit somewhere in between.

An open question: does the Synth *include* the MIDI looper, or does it *receive* from the MIDI looper? The latter is simpler (two plugins cooperating) but the former is more satisfying as a standalone instrument.

---

## 3. MIDI Input as Creative Material

Right now, DrawnQurve is a source — it generates MIDI. But incoming MIDI is a rich creative input too.

### The "glorified arp"

Route incoming notes to a lane, and those notes become the pitch class set (PCS) or "note collection." The qurve then selects *which* of those held notes plays, across octaves. Hold a chord on a keyboard → DrawnQurve arpeggiates through it in whatever shape you drew. The existing scale quantization infrastructure (bitmask + `quantizeNote`) is already most of the way there — instead of a static scale, the bitmask updates dynamically from held notes.

This is more expressive than a traditional arpeggiator because the "pattern" is a continuous curve, not a sequence of slots. Speed, direction, smoothing, and range all apply. You could draw a slow glide through a chord, or a rapid staircase, or something in between.

### Drawing qurves from MIDI input

Listen to specific incoming messages (the input side of routing) and render them as a background qurve. Even if the input is mostly staircase-shaped (discrete note-ons), it becomes a visible shape in the lane. Then you draw over it — refining, smoothing, extending. And you keep it.

This inverts the current flow: instead of "draw first, hear it loop," it's "play first, see it drawn, refine it." Both are valid playflows. The second one especially suits people who think in notes and want to see what they played.

Implementation: a ring buffer of incoming values (already partially designed — see design-decisions-log.md "Incoming MIDI display in canvas background"), quantized to the lane's X resolution, rendered as a ghost layer. A "capture" button freezes the ghost into the lane's curve table. From there, it's a normal qurve.

### Keeping and curating

Once you can capture qurves from MIDI input *and* from drawing, you start accumulating them. That leads to the curve curation ideas in section 6.

---

## 4. Beyond 12TET

The tuning ideas cluster around a central insight: pitch bend range varies wildly, and that variation changes everything.

### Pitch bend range awareness

- A monophonic synth typically defaults to **±2 semitones**. A drawn pitch curve in this range gives you vibrato and note bending.
- An MPE synth typically allows **±48 semitones** (4 octaves). The same curve now covers a huge melodic range.
- Many synths sit somewhere in between (±12, ±24).

DrawnQurve currently sends raw pitch bend values without knowing the receiver's range. For theremin mode (ROADMAP M4) to work musically, we need either:
1. A user-set "pitch bend range" parameter (simple, explicit)
2. MIDI-CI Property Exchange to query it automatically (elegant, distant future)
3. Both — manual override with auto-detection when available

### MPE and per-note bending

MPE gives each note its own MIDI channel, which means per-note pitch bend. This maps beautifully onto per-lane architecture — each lane already has its own channel assignment. An MPE zone configuration would let Lane 1 bend note A independently of Lane 2 bending note B. The curve shapes become per-voice expression, not global modulation.

This is where DrawnQurve goes from "MIDI looper" to "expressive instrument." (See ROADMAP M7.)

### Portamento

Mostly a synth-side feature (CC 65 portamento switch + CC 5 portamento time), but we can influence it. Beyond just sending legato note transitions:
- **Adjustable portamento time via CC 5** — a qurve controlling portamento time would make glide speed *itself* an expressive parameter. Slow glides in one section, fast snaps in another.
- **Pitch-bend-based portamento** — hold the current note and bend toward the next pitch before re-attacking. This is the mechanical foundation of theremin mode (ROADMAP M3 sub-mode b).

### Tuning standards

- **MTS-ESP** is the de-facto plugin standard for microtonal tuning tables. On desktop, it's well-supported (Surge, Vital, PhasePlant). On iPadOS, it basically doesn't exist. No AUv3 synths we're aware of support it.
- **MIDI 2.0 per-note pitch** could eventually replace MTS-ESP for real-time tuning. MIDI 2.0 UMP messages include a per-note pitch field with much higher resolution than MIDI 1.0 pitch bend. But iOS support for MIDI 2.0 is still nascent (AUv3 iOS 18 has partial UMP support; JUCE 8 has partial UMP support).
- **Practical path for now:** pitch bend offsets (per-note in MPE mode) to approximate microtonal scales. Not perfect, but functional. DrawnQurve's existing scale quantization + pitch bend output could combine to produce microtonal output today, if we add bend-range awareness.

---

## 5. Segmented Qurves and Accents

A qurve doesn't have to be uniformly smooth. Some note transitions should glide; others should snap. The "quantized curve" in "qurve" already implies this duality — part curve, part step function.

### Mixed continuous and discrete

Imagine a single qurve where:
- The first half is a smooth glide (portamento between notes)
- The middle snaps discretely (staccato arpeggiation)
- The end returns to smooth

This requires per-segment metadata: each region of the qurve knows whether it's "smooth" or "stepped." The segmented curve concept (ROADMAP C1, "Hold" latch) is the architectural foundation — extending it with a per-segment mode flag.

### Accents and velocity

A natural companion: if some transitions are discrete (note attacks), those attacks should have velocity. Where does velocity come from?

**Option A:** Pressure during drawing. Already captured (ROADMAP C11). The pressure at each point becomes the velocity at that point.

**Option B:** Cross-lane modulation. One qurve drives another's velocity. Lane 1 plays notes; Lane 2's curve shape becomes Lane 1's velocity contour. Challenging because it creates dependencies between lanes that the current architecture treats as independent. But valuable — it's a deeply musical interaction. (See section 7.)

**Option C:** A secondary "accent" layer within a single lane. Effectively two curves stacked: one for pitch/CC, one for velocity/intensity. The drawing UI would need a mode switch or a two-finger gesture.

Sharp attacks are the hardest part of drawing — your finger or Pencil can't move infinitely fast. Solutions: a "snap to grid edge" mode for Y transitions, a dedicated attack-sharpness parameter, or post-capture editing tools (ROADMAP UI9) that let you steepen a slope after drawing it.

---

## 6. Curve Curation

### History

Every qurve you draw could be saved automatically — a running history, like an undo stack but persistent across sessions. Scroll back through your drawing history, find that shape you liked ten minutes ago, pull it into a lane.

The state-saving infrastructure is already surprisingly robust. Extending it to save a timestamped ring buffer of `LaneSnapshot` objects per lane is architecturally straightforward (see ROADMAP D6 for undo/redo, which is the same mechanism).

### Sharing

Qurves should be shareable in a format that's both **machine-readable** (importable back into DrawnQurve) and **human-readable** (you can look at it and understand it). Options:
- **JSON** — natural for the machine side; readable enough if formatted nicely. A 256-point array plus metadata (type, range, scale, name).
- **SVG** — the curve *is* a visual shape. Exporting as SVG gives you a file that's simultaneously a curve definition and a picture of it. Import parses the path data back into a table.
- **Both** — JSON for data exchange, SVG for visual sharing. A URL scheme (`drawnqurve://import?data=...`) for tap-to-import on iOS.

### Modification tools

Beyond the editing tools in ROADMAP UI9 (scale, shift, smooth, flip, mirror, time-stretch), one specific tool stands out:

### Rolling quantization

A tool that applies quantization as you "roll" over a qurve with your finger. Touch and drag across a section → that section snaps to grid; lift → the rest stays raw. The same qurve ends up with quantized *and* raw sections, in both X and Y axes. This is fundamentally different from a global quantize toggle — it's a *local, gestural* operation. "Quantize here, leave this part organic."

This pairs with the segmented qurve concept above: rolling quantization creates segments implicitly, by the gesture itself.

---

## 7. Cross-Lane Modulation

One qurve affecting another. Lane 1 plays notes; Lane 2's output modulates Lane 1's velocity. Or Lane 3's curve controls Lane 1's smoothing amount. Or Lane 2 modulates Lane 1's pitch bend offset.

This is both **challenging and valuable.**

**Challenging** because the current architecture treats lanes as independent streams processed in parallel. Adding cross-lane dependencies means processing order matters, introduces potential feedback loops, and complicates the mental model.

**Valuable** because it's where qurves become a modular system. "Wire this shape to that parameter" is exactly what modular synthesis is about, but with drawn gestures instead of patch cables. It's the kind of thing that could make DrawnQurve feel genuinely novel rather than "another MIDI CC tool."

### Possible implementation paths

1. **Explicit mod routing.** A dropdown per lane: "Modulate Lane N's [parameter]." One-to-one, no feedback, clear processing order (modulator lanes first, then target lanes).
2. **Mod matrix.** Each lane's output can feed into any parameter of any other lane, with amount. More flexible, more complex UI, risk of feedback.
3. **Dedicated "mod lane" type.** A lane that doesn't output MIDI directly but instead modulates another lane's parameter. Clean separation of roles.

Start with option 1 — it's the least disruptive and covers the main use case (velocity contour from a secondary qurve).

---

## 8. Connectivity

### Ableton Link

A specific sync protocol worth adding. Ableton Link provides tempo and phase sync across devices on the same network — no MIDI clock, no wired connection. JUCE doesn't include it natively, but the [Link SDK](https://github.com/Ableton/link) is open-source (GPLv2 + commercial licence) and integrates well with C++ audio applications.

Value: anyone using DrawnQurve alongside Ableton Live, AUM, or other Link-enabled apps (Loopy Pro, Koala, Drambo, Modstep) gets tight tempo lock without any routing setup. It's the kind of thing that "just works" and connects to an existing ecosystem. Relatively straightforward compared to most items on this list.

### MIDI-CI and parameter discovery

The dream: DrawnQurve queries a connected synth via MIDI-CI Property Exchange, gets back a list of parameters with names and ranges, and offers them as qurve targets. "Filter Cutoff (0–127)" instead of "CC 74." No manual CC lookup, no synth-specific presets (ROADMAP R1).

Reality: MIDI-CI support in iOS synths is essentially zero as of 2026. The M2WG (MIDI 2.0 Working Group) specifications are solid, but the ecosystem hasn't caught up. We should:
- **Design the data model now** — parameter name, range, CC mapping, source (manual / MIDI-CI / preset database)
- **Implement manual + preset first** (ROADMAP R1)
- **Add MIDI-CI when there's something to talk to**

When it does arrive, it'll be transformative. Automatic routing, automatic range mapping, named parameters. The System Builder persona (ROADMAP playflow 6) lives for this.

### OSC

Already in ROADMAP R3. The same ranged float that goes to MIDI CC also goes out as an OSC message. Connects to Max/MSP, Pure Data, TouchDesigner, and other creative coding environments.

### VJing via Imaginando VS2

[VS2 (Visual Synthesizer)](https://imaginando.pt/products/vs-visual-synthesizer) is Imaginando's visual performance app for iPad. Its latest version accepts MIDI input for parameter control — more flexibly than the original, though still with limits.

This is a use case that may need **zero adaptation** from our side. DrawnQurve's virtual MIDI port is already visible to VS2. Drawing a curve that controls a visual parameter in VS2 is the same playflow as controlling a synth — the MIDI routing is identical.

Worth testing explicitly during alpha: draw a qurve → see the visuals respond. If it works, it's a compelling demo. If VS2's MIDI mapping is too limited, document what's missing and revisit after their next update.

---

## 9. For Testers: The Synth Question

Alpha testers will inevitably ask: "Why doesn't this have its own sound? Why do I need GarageBand?"

### The honest answer

> DrawnQurve is a MIDI looper — it draws shapes and turns them into MIDI messages. It talks to synths the same way a keyboard does. We chose this because MIDI is universal: any synth you already love can receive DrawnQurve's output.
>
> We *are* thinking about a qurve-native synth — one designed from the ground up for smooth, drawn parameter changes. Wavetable, wavefolding, drawn envelopes instead of ADSR. It's the natural next step. But we want to get the qurve-drawing experience right first, with the broadest possible range of sound sources.
>
> For now, GarageBand is your free, zero-setup sound engine. And honestly? The alchemy of DrawnQurve + a synth you already know produces surprises that a built-in synth wouldn't.

### In the tester guide

The [tester guide](tester-guide.html) already frames this implicitly (GarageBand as the default host, "hear it through any synth"). We should add a brief "why no built-in sound?" sidebar in the #MIDIsplainer path — it's exactly the kind of thing a MIDI-curious tester would wonder about.

---

## 10. Why Quantization? A Rationale

Quantization runs through DrawnQurve's design at every level: pitch class sets, X-axis grid snapping, Y-axis step quantization, output steps, rolling quantization. It deserves an explicit rationale rather than being treated as a technical detail.

### The unquantized experience

Playing a note-mode qurve with no scale, no quantization, across the full pitch range produces something specific: a glissando through all 128 MIDI notes in whatever time the playhead takes to travel. It's endearing in a childish way — recognisably musical, slightly ridiculous — but it wears out quickly. Every curve sounds like a slide whistle. The output is continuous in value but meaningless in pitch.

The problem isn't continuity; it's that 128 semitones with equal weight is too much freedom. The ear has no expectation, so nothing resolves, nothing surprises. It's noise with a pitch contour.

### What quantization gives back

Applying *any* constraint — a scale, a small set of pitch classes, a grid — immediately makes patterns audible. A pentatonic scale with a wavy curve produces something that sounds like an ostinato or a melodic fragment, not a slide. A chromatic cluster still feels dense, but the repetition of the loop creates expectation. The quantization doesn't straighten the curve; it *channels* it into territory where the ear can track and anticipate.

This is the "channeling creativity" argument: constraints don't reduce expression, they redirect it into a space where it produces results. The fully open case exhausts itself quickly; the constrained case keeps revealing new things each time you change the scale or the curve shape.

### Why this matters for design

It justifies a few decisions that might otherwise seem arbitrary:
- **Scale presets as defaults, not afterthoughts.** The first thing a note-mode lane should do is sound musical. A chromatic quantization to a sensible scale (Major, Pentatonic) by default gives the new user that experience.
- **Rolling quantization as an expression tool.** Choosing *where* to apply quantization and where to leave raw continuity is an expressive act. It's not a cleanup operation — it's composition.
- **The Q in DrawnQurve.** It's there for a reason.

### The spectrum

Raw → stepped → scale-quantized → chord-quantized → rhythm-quantized → phrase-quantized. Each step adds constraint and musical legibility. DrawnQurve currently lives in the "scale-quantized" zone. Rhythm quantization (section 11) and leadsheet support (section 12) push further along the spectrum.

---

## 11. Rhythm, Groove, and Serpe

Pitch quantization gets a lot of attention. Rhythm quantization is at least as interesting.

### What Serpe does

Serpe is a separate project — a rhythm engine built around Euclidean and pattern-based beat generation. Its core insight: rhythm is a parameter, not a fixed property of the timeline. You choose how many pulses fit in a cycle, and the pattern space opens up in non-obvious ways (Bjorklund's algorithm, necklace equivalences, rotations).

### Bringing rhythm into DrawnQurve

The X-axis of a qurve is currently a uniform timeline: the playhead sweeps left to right at constant speed. But note *attacks* only fire when the Y value crosses a threshold. What if the X axis itself was non-uniform — advancing faster through some regions, slower through others?

This is essentially what Ableton's groove templates do for MIDI clips, and what the groove/rhythm template item (ROADMAP C6) captures. Applied to DrawnQurve:
- A Euclidean rhythm template could determine *when* in the cycle note attacks can occur, while the curve shape still determines *which* note fires.
- A swing/humanize layer could push note timings off the strict grid by a percentage.
- Serpe-style pattern logic could generate rhythmic skeletons that the qurve's pitch content then fills.

### Microrhythms

Beyond standard swing: arbitrary sub-division offsets, non-dyadic rhythms (7-over-4, 5-over-3), groove templates derived from recorded performances. The X-axis becomes a stretchable timeline rather than a ruler. Each sub-section of the loop has its own time weight.

This interacts with the segmented qurve concept (section 5): some segments are "rhythmically tight" (on the grid), others breathe (slightly late, slightly early). The drawing gesture itself could encode timing — a faster finger movement means tighter rhythm; a slower movement means more rubato.

### A practical first step

The simplest version: a "swing" slider per lane. Odd beats pushed by N% relative to even beats. Standard, well-understood, immediately useful. From there: Euclidean rhythm as an X-axis quantization mode. Then: Serpe integration if the projects converge.

---

## 12. Harmonic Intelligence and MIDIcurator

### The leadsheet idea

MIDIcurator is a separate project concerned with chord progressions, leadsheets, and harmonic structure over time. The overlap with DrawnQurve is in one specific feature: **time-varying chord context** (ROADMAP M5).

A leadsheet tells you what chord is playing at each moment. If DrawnQurve knows the current chord, it can quantize note output to chord tones rather than (or in addition to) a static scale. The qurve shape selects *within* the chord — a rising curve plays chord tones from root to seventh; a falling curve descends. Change the chord, and the same curve plays different notes.

This is one of those features that sounds complicated to add but could make DrawnQurve into something genuinely unique. Most MIDI loopers have no concept of harmonic context. A looper that "knows what key we're in right now" — and adjusts its output without the user re-drawing — is a different kind of tool.

### Why this has been a daydream for years

The combination of drawn gestures + real-time harmonic context has been a persistent idea precisely because it bridges two worlds that rarely meet:
- **Gesture-based expression** (theremin, ribbon controllers, draw tools): continuous, intuitive, hard to keep in key
- **Harmonic intelligence** (Scaler, AutoTonic, scale quantizers): discrete, structured, can feel mechanical

A qurve-in-a-chord-context is both. The gesture is expressive and free-form; the harmonic context keeps it musical. Neither property cancels the other.

### Non-Chord Tones

Already in ROADMAP M5a. The most interesting question in leadsheet mode: what do you do with a scale tone that isn't in the current chord? Options:
- **Snap to nearest chord tone** (safe, can feel over-corrected)
- **Pass through as NCT** (creates tension and colour, needs some kind of policy for resolution)
- **Visually mark NCT territory in the curve display** — the player can see where the "tension zones" are in their gesture

The third option is the most exciting: you draw with awareness of the harmonic consequences. The curve display becomes a map of consonance and dissonance.

### A shared chord format with MIDIcurator

If MIDIcurator has a serialisation format for chord progressions (likely some variant of chord symbol + duration), DrawnQurve could import and follow it. No re-entry required. The two tools in the same workflow: MIDIcurator sets the harmonic frame; DrawnQurve plays within it.

---

## 13. Circle of Fifths and PCS Navigation

The current pitch-class selector is a 12-button linear row (C through B), or optionally a two-row piano keyboard layout. Both are functional but neither is harmonically intelligent.

### Why the circle of fifths

The circle of fifths is both a cliché *and* genuinely useful — but not just for selecting diatonic families. A few specific advantages:

- **Adjacent = related.** Notes and scales that sound good together are literally next to each other. The "accidental" discovery of a good scale subset is more likely when you're navigating by harmonic proximity rather than chromatic order.
- **"Diatonic family" is a special case.** Seven adjacent notes on the circle = any major or natural minor scale. But six notes gives you a hexatonic scale; five gives you pentatonic; four gives you something more exotic. Navigation by "how many adjacent fifths?" covers the whole spectrum.
- **Root selection becomes directional.** Moving clockwise or counterclockwise transposes by fifth. This is a musically natural transposition operation — much more so than semitone steps.

### What this doesn't solve

The circle of fifths UI is piano-centric in a different way than the keyboard row: it privileges tonal Western harmony and the "fifths cycle" as the primary way of relating pitches. Microtonalists, xenharmonicists, and people working in non-Western tuning systems are not served by it. This is a known limitation worth acknowledging rather than pretending it doesn't exist.

### A concrete design idea

Two concentric rings. Outer ring: the 12 pitch classes arranged as a circle of fifths. Inner ring: the 12 pitch classes arranged chromatically (for direct selection without the fifths framing). Toggle between ring displays. The inner/outer arrangement makes it possible to show both orderings simultaneously — the selected PCS is highlighted on both rings, showing its structure in each domain.

---

## 14. GUI and Look & Feel

### Honest assessment

DrawnQurve's current look is functional and mostly consistent within its own system, but it's not as visually pleasant as it could be. Some things that are genuinely inconsistent or "too basic":

- **Button styles are not unified.** Some buttons use the custom `SymbolLF`; others use plain `TextButton` with system fonts; others use the `SegmentedControl`. They don't quite share a visual grammar.
- **The colour palette is ad hoc.** The warm beige system (introduced in the 2026-03 redesign) is a good start, but the accent colours, hover states, disabled states, and background layers were not designed together — they were added as needed.
- **Typography is thin.** A single size of JUCE's default font dominates. There's no typographic hierarchy. Labels, values, and headings look the same.
- **The routing matrix feels like a form, not a UI.** It works, but it doesn't feel *designed*. The information is there; the visual flow isn't.
- **The curve display is the best part** — it's visually clear, the playhead dots work, the grid is readable. This is the one area that feels thought-through.

### What would actually fix it

Good GUI work on a project like this requires visual design skills that aren't part of the current development setup. That's not a complaint — it's an honest scoping statement. Options:

1. **Find a visual designer to collaborate with.** Even a few hours of design critique and a style guide would help significantly. The Resonant Design benchmarking (see benchmark-protocol.md) is a step in this direction — it identifies where established patterns exist that could be followed.
2. **Adopt an existing design system.** Rather than continuing to improvise, pick a source of truth: Material Design adapted for audio, or an established iOS music app UI pattern set, and follow it consistently. Less original, more coherent.
3. **Incremental polish passes.** Fix one inconsistency at a time: unify button heights, standardise font sizes, clean up disabled states. Slower, but feasible without a design partner.

### Specific niceties worth noting

- **Haptic feedback** on iPad (via UIImpactFeedbackGenerator or similar). A small tap when the playhead crosses a note threshold. Subtle, optional, makes the experience feel more physical.
- **Playhead trail / ghost.** The curve display currently shows the playhead dot. A short trail — the last 200ms of path — would make the loop's momentum visible without adding noise.
- **Lane colour application.** The lane accent colours are used for the playhead dots. They could be used more — for the curve line itself (each lane's curve in its lane colour), for the active lane highlight in the routing matrix, for subtle background tints.
- **Animated transitions.** When switching lanes, the curve display currently switches instantly. A brief crossfade or slide would make the transition feel intentional.
- **Dark/light mode polish.** The `_lightMode` flag exists throughout, but light mode hasn't been as carefully tuned as dark mode. The beige background works; some elements (borders, disabled buttons) need attention in bright conditions.

---

## 15. Out-of-Scope Ideas (Documented Anyway)

These are ideas we've considered and consciously set aside — either because they represent too much architectural divergence, because the iOS ecosystem doesn't support them yet, or because they belong more naturally in a different product. Documenting them here prevents re-litigating the same decisions.

### Audio-rate qurves / CV output

DrawnQurve Audio (section 2) is the proper vehicle for this. At audio rate, the curve table needs to be read at sample rate (44.1k or 48k samples per second), which means 256 points is far too coarse — you'd hear aliasing and stepping. The table resolution would need to jump to at least 2048, and the interpolation scheme would need to change.

More importantly: the creative context changes. An audio-rate LFO is a different tool with a different mental model. Users who want CV-rate modulation are coming from a modular synthesis world where the "draw a waveform" concept is already well-established (Arturia MicroFreak, Make Noise Maths, Mannequins W/). DrawnQurve Audio would enter that world; this plugin stays in the MIDI world.

### Connection with Eurorack curve tools

Direct inspiration: Instruo Arbhar, Make Noise Maths, Bastl Kastle. These tools share the "drawn/shaped waveform as a creative parameter" philosophy. The differences are significant though:
- Eurorack operates at audio/CV rate (not MIDI)
- Physical patch cables replace routing UI
- The "drawing" metaphor in hardware is typically a knob position or a touch strip, not a free-form gesture on a touchscreen

DrawnQurve went in a different direction: free-form gesture, MIDI output, loop playback with tempo sync. The modular world is an inspiration, not a template. The divergence is intentional.

### Standalone audio synthesis in the current plugin

Already noted in design-decisions-log.md: the plugin type is "MIDI effect." Changing it to include audio output would affect every AUv3 host's routing model. GarageBand, AUM, and Drambo all treat MIDI effects and audio instruments differently. The separation is load-bearing.

The right answer is DrawnQurve Synth as a separate audio instrument AUv3 target that communicates with this plugin, not a mode toggle inside it.

### MTS-ESP on iPadOS

As noted in section 4: no AUv3 synths on iPadOS support MTS-ESP as of 2026. Implementing it as a transmitter would be wasted effort until the receiver side exists. Watch for: Surge XT iOS (currently in development), any port of open-source synths that support MTS-ESP on desktop.

---

## 16. Ecosystem: Comparisons and Inspirations

Context for where DrawnQurve sits relative to existing tools — both to understand what it does differently, and to identify patterns worth following.

### MIDI loopers / CC modulators

| Tool | Platform | What it does | Key difference from DrawnQurve |
|------|----------|--------------|-------------------------------|
| **MIDI Designer** | iOS | Build custom MIDI controller UIs | Fixed sliders/knobs; no looping; no curve drawing |
| **Mozaic** | iOS AUv3 | Scriptable MIDI processor (LFO, arp, quantize) | Code-based; powerful but not gestural |
| **Drambo** | iOS | Modular MIDI/audio sequencer | Step sequencer + modular routing; no free-form gesture drawing |
| **TouchOSC** | iOS/macOS | Custom controller with MIDI/OSC output | Controller, not a looper; no autonomous playback |
| **Loopy Pro** | iOS | Audio loop recorder with MIDI triggering | Audio, not MIDI modulation; no curve drawing |
| **StepPolyArp** | iOS | Step arpeggiator | Fixed step grid; no continuous gestures |

The gap DrawnQurve fills: **free-form gesture → looping MIDI output**, with quantization and scale intelligence. None of the above do all three together.

### Scale quantizers

| Tool | Platform | Approach |
|------|----------|----------|
| **Scaler 2/3** | VST/AU/AAX | Chord + scale intelligence; most complete harmonic feature set |
| **ScaleBud AUv3** | iOS | Lightweight scale quantizer; clean piano-key UI |
| **AutoTonic** | iOS | Key-relative MIDI remapping; white keys always play in key |
| **Intellijel Scales** | Eurorack | Hardware; bank+scale matrix; CV input and output |
| **Logic Pro Quantize** | macOS | Built-in to Logic; scale quantize for MIDI regions |

DrawnQurve's scale quantization is real-time and tightly integrated with the gesture — it's not a post-processing step. That's the distinction. Scaler is the most relevant comparison for the harmonic intelligence features (section 12); its chord progression model is what we'd be building toward.

### Arpeggiators

| Tool | Approach | Key difference |
|------|----------|---------------|
| **ARP MIDI** (iOS) | Traditional arp patterns | Fixed note order (up/down/random); no continuous gesture |
| **Cthulhu** (VST) | Chord-to-arp with pattern memory | Pattern-based, not curve-based |
| **Patterning 2** (iOS) | Circular step sequencer | Rhythm-centric; visual but step-based |

The "glorified arp" in section 3 is what makes DrawnQurve's arp mode distinct: the curve *is* the pattern, and it's continuous rather than stepped by default.

### Drawing / gesture tools

| Tool | Approach | Overlap |
|------|----------|---------|
| **Dorico** draw tool | Note velocity drawing in a piano roll | Similar drawing metaphor; fixed to note grid |
| **Live's Draw Mode** | Automation drawing | Same metaphor; timeline automation; not looping |
| **Good Ol' Draw** (conceptual) | Free-form gesture as MIDI | The closest conceptual ancestor we're aware of |
| **Lemur** (iOS) | Multitouch custom interfaces | Gesture-to-MIDI; programmable; not a looper |

DrawnQurve's contribution: drawing is the *primary* interaction, not a mode. And the drawn curve *loops autonomously* — it's not written into a fixed timeline. That's the creative distinction.

### What the ecosystem comparison tells us

1. **The gap is real.** No existing tool combines free-form gesture drawing + autonomous MIDI looping + scale quantization in one interface.
2. **Competition comes at the edges.** Mozaic can do some of this via scripting; Drambo can get there with modular routing. But neither makes it *this easy*.
3. **The harmonic intelligence features (section 12) would be the most distinctive.** A qurve that follows a chord progression has no direct competitor on iOS.
4. **VJing via VS2 is an underexplored market.** Most MIDI-to-visual tools are on desktop (TouchDesigner, MadMapper). An iPad-native drawing-to-visuals workflow via DrawnQurve + VS2 is genuinely novel.

---

*See also: [ROADMAP.md](../ROADMAP.md) for implementation-ready features · [personas.md](personas.md) for user profiles · [design-decisions-log.md](design-decisions-log.md) for past decisions · [benchmark-protocol.md](benchmark-protocol.md) for UI pattern benchmarking · [tester-guide.html](tester-guide.html) for the alpha guide*
