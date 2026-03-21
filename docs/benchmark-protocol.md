# DrawnCurve — Benchmarking & Usability Test Protocol

Use this document to run structured usability evaluations of the current
interface and any proposed changes. It covers task scenarios, success metrics,
a heuristic checklist, and a post-test survey.

---

## Participants

### Recruitment criteria (one session per persona tier)

| Session | Proxy persona | Recruitment filter |
|---------|--------------|-------------------|
| S1 | Persona D (Student) | Never routed a MIDI signal; uses GarageBand casually |
| S2 | Persona C (Producer) | Makes music at home; knows "what CC is" but not CC numbers |
| S3 | Persona B (Performer) | Performs live with iOS apps; uses MIDI regularly |
| S4 | Persona A (Experimenter) | Programs MIDI, uses AUM or Drambo, familiar with routing |

Run at least 5 participants per session (n ≥ 20 total for statistical validity).
Think-aloud protocol with screen + audio recording.

---

## Equipment & Setup

- iPad running the test build of DrawnCurve
- Host app: **AUM** (consistent across all sessions)
- Routing pre-configured: DrawnCurve → Moog Model D (AUv3) on the test device
  so participants don't need to route — unless routing is the task being tested
- Screen recording on (ReplayKit or external camera on stand)
- Facilitator script (see below)
- Paper SUS survey or digital equivalent

---

## Facilitator Script (opening)

> "Thank you for joining us. We're testing a music plugin called DrawnCurve.
> We're testing the app, not you — there are no wrong answers.
>
> Please think out loud as you work: tell us what you're looking at, what you
> expect to happen, and how you feel. We'll watch and take notes, but we
> won't help unless you're fully stuck.
>
> We'll do a few short tasks. Ready?"

---

## Core Tasks (all sessions unless noted)

### T1 — First curve (all personas)

> "Open the plugin. Draw any shape you like with your finger. Then make it play."

**Success**: Curve drawn and looping within 90 seconds.
**Partial**: Curve drawn but participant doesn't find Play button.
**Fail**: Participant gives up or asks for help within 90 seconds.

**Observe**: Where does the participant touch first? Do they draw in the right area?
Do they expect Play to start automatically after drawing?

---

### T2 — Sync to tempo (Personas B, C, D)

> "Make the curve loop in time with the host's tempo (120 BPM). Set it to loop
> once every 4 beats."

**Success**: Sync enabled, Beats set to 4, loop speed noticeably locked to click.
**Partial**: Sync enabled but Beats not adjusted.
**Fail**: Participant doesn't find Sync button.

**Observe**: Does the participant understand "Beats" vs. "Speed"? Is the Sync
button discoverable?

---

### T3 — Change output mode (all personas)

> "Change the plugin so it controls pitch instead of a filter.
> (Hint: look for Pitch Bend.)"

**Success**: PB mode selected within 60 seconds.
**Partial**: Participant selects Aft instead.
**Fail**: Participant can't identify the mode buttons.

**Observe**: Do participants understand the abbreviation "PB"? Do they try to
explain the abbreviation before selecting?

---

### T4 — Reverse playback (Personas A, B, E)

> "Make the curve play backwards."

**Success**: Rev or P-P direction selected.
**Partial**: Participant finds direction row but picks P-P thinking it means something else.
**Fail**: Participant doesn't find direction buttons.

**Observe**: Are the arrow icons (or symbols) immediately understood?

---

### T5 — Constrain output range (Personas A, C, E)

> "Set it so the curve only uses the upper half of the range —
> never goes below halfway."

**Success**: Range slider minimum thumb moved to ~0.5.
**Partial**: Only one thumb moved; participant uncertain which end is which.
**Fail**: Participant doesn't find range slider or confuses it with speed slider.

**Observe**: Is a two-thumb slider on a horizontal line self-explanatory?
Do participants know which thumb is minimum and which is maximum?

---

### T6 — Identify "Aft" (Personas C, D) — comprehension task

> "Without pressing it, tell me what you think the 'Aft' button does."

**Success**: Participant correctly describes Aftertouch / pressing harder / expression.
**Partial**: Participant is unsure but says "something about pressure" or "after the note."
**Fail**: Participant has no idea; uses help button to find out.

**Observe**: How long does identification take? Does the participant open Help (?)
to find the answer?

---

### T7 — Adjust speed (Personas B, C)

> "Make the loop play twice as fast."

**Success**: Speed slider moved to approximately 2×.
**Partial**: Participant adjusts Beats slider instead (if Sync is on).
**Fail**: Wrong slider touched; participant unaware of speed control.

**Observe**: Is "2×" the right mental model? Does the slider label make the
current value clear?

---

### T8 — Save and recall (Personas A, B, C, E)

> "Save the current curve so you can come back to it later, then clear the
> canvas and reload it."

**Success**: User finds host preset save/recall and the curve is restored.
**Partial**: User doesn't know how to save (relies on host; plugin has no own UI for this).
**Fail**: User expects a Save button inside DrawnCurve itself.

**Observe**: Do participants expect a Save button inside the plugin? This reveals
a potential missing feature.

---

## Metrics

| Metric | Collection method |
|--------|-----------------|
| **Time on task (seconds)** | Stopwatch from task start to success or give-up |
| **Error count** | Log each wrong control touched or incorrect action taken |
| **Help button uses** | Count taps on ? button per task |
| **Completion rate** | Success / Partial / Fail per task per participant |
| **Think-aloud code** | Utterances tagged: Confused / Neutral / Delighted |
| **SUS score** | Post-session 10-question survey (0–100; ≥68 = above average) |

---

## SUS Survey (post-session)

Rate each statement 1 (Strongly Disagree) to 5 (Strongly Agree):

1. I think that I would like to use this app frequently.
2. I found the app unnecessarily complex.
3. I thought the app was easy to use.
4. I think that I would need the support of a technical person to be able to use this app.
5. I found the various functions in this app were well integrated.
6. I thought there was too much inconsistency in this app.
7. I would imagine that most people would learn to use this app very quickly.
8. I found the app very cumbersome to use.
9. I felt very confident using the app.
10. I needed to learn a lot of things before I could get going with this app.

**Scoring**: Odd items: score − 1. Even items: 5 − score. Sum × 2.5. ≥68 = acceptable.

---

## Heuristic Evaluation Checklist

Run this checklist in a static review before any user test, by one or two
evaluators familiar with Nielsen's 10 heuristics.

### Visibility of system status
- [ ] Is it obvious whether the loop is playing or stopped?
- [ ] Is the playhead position visible in the canvas?
- [ ] Is the current output mode (CC/Aft/PB/Note) clearly highlighted?
- [ ] Is the current direction (Fwd/Rev/P-P) clearly highlighted?

### Match between system and real world
- [ ] Are button labels meaningful without MIDI knowledge? (Aft, PB, P-P…)
- [ ] Is the two-thumb range slider self-explanatory?
- [ ] Does the canvas area clearly communicate "draw here"?

### User control and freedom
- [ ] Is there an undo for accidental draws?
- [ ] Can a partially-drawn curve be abandoned easily (Clear button reachable)?
- [ ] Can the loop be paused without losing the curve?

### Consistency and standards
- [ ] Are the four output modes visually grouped and treated as a radio group?
- [ ] Is the direction group visually distinct from the output type group?
- [ ] Do sliders behave as expected (left = less, right = more)?

### Error prevention
- [ ] Is there any confirmation before Clear wipes the canvas?
- [ ] Can the user accidentally change CC# while drawing?

### Recognition rather than recall
- [ ] Are all controls visible (not hidden in menus)?
- [ ] Does the ? help overlay cover every visible control?
- [ ] Are abbreviated labels ("Aft") explained somewhere accessible?

### Flexibility and efficiency of use
- [ ] Can expert users access raw CC# directly?
- [ ] Can users start drawing immediately without any configuration?

### Aesthetic and minimalist design
- [ ] Is the drawing canvas the most prominent element?
- [ ] Are controls proportional to their importance?
- [ ] Is the layout free of decorative clutter?

---

## What to Do with Results

1. **Task completion rate < 80%** on any task → that control/label needs redesign.
2. **SUS score < 68** on any persona tier → that tier needs a dedicated interface mode.
3. **Help button used for a specific control > 30% of participants** → label or icon is unclear.
4. **Think-aloud "Confused" tags clustered around one area** → information architecture issue.
5. **Consistent "Save button" expectation (T8 fail)** → add explicit save/preset UI.

---

## Benchmark Targets

| Persona | SUS target | T1 time target | T3 completion target |
|---------|-----------|----------------|---------------------|
| D (Student) | ≥ 65 | ≤ 60s | ≥ 80% |
| C (Producer) | ≥ 72 | ≤ 45s | ≥ 90% |
| B (Performer) | ≥ 80 | ≤ 30s | ≥ 95% |
| A (Experimenter) | ≥ 85 | ≤ 20s | ≥ 98% |

---

## UI Pattern Benchmarking — "Solved Problems"

Before redesigning any control, check whether there is an established idiom
in the music-software space. Following the idiom reduces learning time.
Deviating requires strong justification.

### Sources to trace

| Source | Focus | What to look for |
|--------|-------|-----------------|
| [Resonant Design](https://resonant.design) | Plugin UX agency; worked on Dubler 2, Arcade 2, Straylight, Opsix, Rosa, BassySynth | Scale rows, keyboard layouts, MIDI routing UI, direction controls |
| [Scaler 2/3](https://scalermusic.com) | Scale + chord intelligence plugin (VST/AU/AAX) | Scale/key selector, chord mode, piano-roll integration |
| [ScaleBud AUv3](https://keybudapp.com/scalebud) | iOS scale quantizer; AUv3 | Scale picker UI, pitch-class keyboard layout on small screen |
| Logic Pro scale quantizer | Built-in; widest install base | Piano keyboard row for scale selection |
| Intellijel Scales (Eurorack) | Hardware scale quantizer | LED button layout (5 banks × 7 scales; black/white key metaphor) |
| AutoTonic | MIDI scale transposer | White key / black key functional mapping |

### Known solved patterns

| Control | Established pattern | Notes / Caveats |
|---------|--------------------|-|
| Scale/pitch-class picker | Two rows: 5 "black" buttons staggered above 7 "white" buttons (piano keyboard idiom) | Acknowledged as piano-centric (#PianoCentric) but is the dominant expectation |
| Root note selection | Same piano-row layout, single selection | Same idiom as scale picker |
| Output message type | Segmented control (radio group) with full names or clear icons | "Aft" is a known comprehension failure (see T6 task) |
| Playback direction | 3-state segmented control; arrow icons are understood; Fwd/Rev/Loop | A single rotary with 3 detents is also valid (Eurorack convention) — #ToBeTested |
| Loop/play transport | Large play/pause; smaller stop; clear visual state | Standard across all music apps |
| MIDI channel | Numeric stepper or small slider (1–16); 1 as default | Channel is rarely changed; hide in Simple mode |
| Smoothing / slew | Horizontal slider labelled with low=sharp, high=smooth | Some plugins use a "glide" icon |
| Quick reference / help | Long-press on a control → tooltip (NN/g "pull revelation") is preferred over a full-screen overlay | Full-screen overlay covers context; NN/g heuristics show they are often dismissed |
| Abbreviations | Full names in Simple; abbreviations in Standard/Expert with tooltip | Nielsen H2: match between system and real world |

### Open investigations (assign and close before UI redesign)

- [ ] Trace Resonant Design → Arcade 2 Note Kits: how do they lay out the pitch-class keyboard?
- [ ] Trace Resonant Design → Straylight: any scale/mode selector present?
- [ ] Trace Opsix (Korg FM synth): Expert-level scale UI pattern
- [ ] Investigate: how do Ableton Live/Logic handle loop direction in a small control space?
- [ ] Investigate: iOS SF Symbols — which symbol best represents "aftertouch / channel pressure"?
- [ ] Validate: does the direction arrow icon set (filled triangles) test well for "Fwd/Rev/P-P"
      or does something else test better? (T4 task)
