# DrawnCurve — User Personas

Five personas that represent the realistic breadth of DrawnCurve users.
Each drives design decisions: which controls are visible by default, what
language is used in labels, and what onboarding support is needed.

---

## Persona A — "The Experimenter"

> "I want to draw my own LFO and route it wherever I feel like it."

| Field | Detail |
|-------|--------|
| **Name** | Tariq, 34 |
| **Location** | Berlin |
| **Background** | Electronic musician, Eurorack enthusiast, uses AUM + Drambo |
| **Tech level** | High — understands MIDI channels, CC numbers, XPC routing |
| **Devices** | iPad Pro + external MIDI controller + hardware synths |
| **Typical session** | 3–5 hours; experimental composition and live A/V sets |

### Goals
- Replace a hardware LFO with a drawn, irregular waveform
- Send the same curve to multiple CCs simultaneously on different channels
- Precisely control the output range to dial in subtle modulation

### Pain Points
- Simplified UIs hide controls he needs; hates hunting for "advanced mode"
- CC numbers should always be editable directly (not only via picker)
- Wants to see raw MIDI output values while it's running (monitoring)

### Mental Model
"This is a free-form LFO with a touchscreen drawing interface."

### Key Needs
- All parameters always visible (Expert mode default)
- Direct numeric input for CC#, channel, speed
- Nothing hidden, no forced onboarding

### Scenario
Tariq has a Moog Grandmother connected via a MIDI-USB adapter. He opens
DrawnCurve in AUM, draws a slow irregular wave, routes CC74 on channel 2
to the Grandmother's filter cutoff. He adjusts the range slider to 40–80%
so the filter never fully closes or opens. He tweaks smoothing to 0.05 so
the steps are audible but not jarring.

---

## Persona B — "The Live Performer"

> "I set it up during soundcheck. During the show I need it to just work."

| Field | Detail |
|-------|--------|
| **Name** | Priya, 28 |
| **Location** | London |
| **Background** | Keys/synth player in a band; uses GarageBand Live Loops and AUM |
| **Tech level** | Medium — knows CC and MIDI channel, uncomfortable with signal graphs |
| **Devices** | iPad Air, MIDI keyboard, stage monitoring rig |
| **Typical session** | 45-min soundcheck, then 60–90-min live set |

### Goals
- Pre-draw a few expressive curves (vibrato, swell, stutter) before the show
- During performance: tap Play/Pause without touching any settings
- Loop must lock tightly to the band's click track (tempo sync is essential)

### Pain Points
- Tapping the wrong button mid-performance is catastrophic
- Tempo sync not being the default means extra setup steps
- Too many visible controls increases mis-tap risk on stage

### Mental Model
"It's an expressive gesture I pre-recorded, played back on demand."

### Key Needs
- Large Play/Pause button — primary target during performance
- Sync ON as default
- Standard mode: hide raw MIDI numbers, show only named controls
- Ability to save/recall named presets within the host

### Scenario
Before the show Priya draws three curves and saves them as plugin presets in
AUM ("Vibrato Slow", "Swell 4bar", "Stutter 16th"). During the set she loads
each preset with a long-press, then taps Play when the moment arrives.
She never looks at the parameter row.

---

## Persona C — "The Bedroom Producer"

> "I want my pad to wobble — I just don't know what knob that is."

| Field | Detail |
|-------|--------|
| **Name** | Camille, 22 |
| **Location** | Montreal |
| **Background** | Home recording in GarageBand and Logic; learns from YouTube tutorials |
| **Tech level** | Medium — uses plugins, automation, some MIDI clip editing |
| **Devices** | iPad + MacBook Air |
| **Typical session** | 1–2 hours; one track or arrangement at a time |

### Goals
- Add organic movement to a static synth pad without drawing DAW automation
- Match the modulation speed to the song's tempo
- Understand what she changed if something sounds wrong

### Pain Points
- "CC74" means nothing to her — she needs "Filter Cutoff" in plain language
- Not sure if she should use CC, Aftertouch, or Pitch Bend
- Afraid of setting the wrong MIDI channel and breaking her project

### Mental Model
"I'm automating a knob, but by drawing instead of clicking."

### Key Needs
- Named CC picker ("Filter Cutoff", "Volume", "Modulation") rather than raw numbers
  — or a text label field so she can name CC74 herself once she learns it
- Visual feedback connecting the curve to something audible
- Sync ON and 4-beat default so it "just fits" the song

### Scenario
Camille is finishing a track. She adds DrawnCurve in GarageBand, selects
"CC" mode. She doesn't know what CC74 is so she leaves it and draws a slow
wave. She hears the filter opening — she realises CC74 = Filter Cutoff and
labels it herself in the UI. She adjusts sync beats to 8 to match her verse
length.

---

## Persona D — "The Student"

> "I've heard of MIDI. I'm not sure what it actually does."

| Field | Detail |
|-------|--------|
| **Name** | Léa, 19 |
| **Location** | Lyon |
| **Background** | Music technology student, first semester; iPhone-native generation |
| **Tech level** | Low — knows GarageBand, has never routed a MIDI signal |
| **Devices** | iPad (school-issued) |
| **Typical session** | 30 min; in-class exercise or homework |

### Goals
- Complete a class exercise: "use DrawnCurve to modulate a synth"
- Understand what the plugin does, not just make it work by accident
- Feel in control, not overwhelmed

### Pain Points
- Seeing 15 controls at once causes paralysis
- Abbreviations like "Aft", "PB", "P-P" are opaque
- No idea what CC# to use or why; channel is confusing

### Mental Model
"I'm drawing something and it changes the music somehow."

### Key Needs
- Simple mode: only the canvas, Play/Pause, Clear, and output type (with full names)
- Onboarding tooltip sequence or highlighted first-use guide
- Help overlay with full names (not just abbreviations)
- Safe defaults that produce audible results immediately

### Scenario
Léa opens DrawnCurve for the first time in class. In Simple mode she sees
only the canvas and a "What to control?" dropdown. She selects "Volume"
from the list, draws a wave, taps Play. The synth volume rises and falls.
She understands. She then taps "Show more controls" and explores further.

---

## Persona E — "The Sound Designer"

> "I need precise, repeatable envelopes I can reuse across a whole sample library."

| Field | Detail |
|-------|--------|
| **Name** | Marcus, 41 |
| **Location** | Los Angeles |
| **Background** | Audio plugin developer, sound library creator, Max/MSP and Reaktor user |
| **Tech level** | Expert — builds MIDI processors from scratch, uses OSC and SysEx |
| **Devices** | Mac + iPad Pro + hardware modular system |
| **Typical session** | Long, focused sessions; creates 50–200 preset patches |

### Goals
- Design consistent CC envelopes to apply across hundreds of patch variations
- Export curve data to MIDI files or OSC for use in other tools
- Push the resolution limits (256 points might not be enough for transients)

### Pain Points
- No multi-lane support (can only modulate one target at a time)
- Curve resolution may be too coarse for fast attack transients
- No way to export the curve as a MIDI clip for offline use

### Mental Model
"This is a programmable, drawable LFO with flexible routing — and I want to
extend it."

### Key Needs
- Full Expert mode with all parameters exposed
- Eventual: multi-lane (N curves → N targets simultaneously)
- Eventual: export curve as MIDI file / automation data
- Direct numeric input everywhere

### Scenario
Marcus is building a library of "living" synth patches. He draws a precise
attack-decay-sustain-release shape in DrawnCurve, fine-tunes the range to
exactly 40–100%, sets smoothing to 0 for transient accuracy. He uses the
host's preset system to save 12 variations. He wishes he could export the
256-point table as a CSV or MIDI file.

---

## Persona Priority Matrix

| Persona | Short label | Current UI suits? | Highest gap |
|---------|-------------|-------------------|-------------|
| A — Experimenter | Expert | Yes | Multi-lane, monitoring |
| B — Live Performer | Performer | Partly | Sync default, preset recall |
| C — Bedroom Producer | Producer | Partly | CC naming, safe defaults |
| D — Student | Beginner | No | Progressive disclosure, full names |
| E — Sound Designer | Power User | Mostly | Export, resolution, multi-lane |

Personas D and C have the **largest usability gaps** in the current UI.
Persona B has the most critical **reliability** need (live performance).
