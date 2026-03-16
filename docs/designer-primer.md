# DrawnCurve — Designer Primer

> A plain-language introduction for designers who have never touched MIDI,
> plugin hosts, or DAW routing.

---

## Elevator Pitch

**DrawnCurve is like drawing a heartbeat with your finger — and then your music
follows that heartbeat forever.**

You open the app inside a music tool on your iPad. You draw a wave, a zigzag, a
slow arch. The moment you lift your finger the shape starts looping. While it
loops it gently pushes and pulls on some aspect of a synth: the brightness of a
pad, the wobble of a note, the swell of volume. The shape you drew becomes a
living motion in the music.

That's it. Everything else is detail.

---

## A Three-Step Flow

```
  1. DRAW            2. LOOP              3. HEAR
  ┌────────┐         ┌────────┐           ┌────────┐
  │ ~~~~~  │  ──▶    │ ~~~~▶  │   ──▶     │ synth  │
  │ finger │         │  loop  │           │ moves  │
  └────────┘         └────────┘           └────────┘
```

1. **Draw** — drag your finger across the big canvas area to sketch a curve.
   The vertical position of your line will become the "amount of motion."
2. **Loop** — when you lift your finger the plugin memorises the shape and
   plays it back on a continuous loop, like a tiny film reel.
3. **Hear** — the looping shape sends a stream of messages to a synthesiser or
   instrument app. Those messages move a parameter (filter, vibrato, volume,
   pitch…) in exactly the rhythm of your drawing.

---

## What Is MIDI? (Plain Language)

Imagine a universal remote control for music apps. MIDI is the language that
remote control speaks. Instead of buttons that say "turn the TV up", MIDI
sends messages like:

- "Move knob #74 to position 63" (that's a CC message — more below)
- "The pitch should bend up by 20%" (Pitch Bend)
- "This note is being pressed harder" (Aftertouch)
- "Play the note at position 60 with force 80" (Note On)

DrawnCurve generates a constant stream of these messages as the loop plays.
No sound comes out of DrawnCurve itself — it only sends remote-control signals
to other apps.

---

## What Is a Plugin Host?

A **plugin host** is a music app that can load other apps (plugins) inside it.
Think of it like a browser that can load web extensions.

Common iOS plugin hosts:
- **AUM** — a flexible audio mixer/router, very popular with advanced users
- **GarageBand** — Apple's free recording studio; loads AUv3 plugins directly
- **Loopy Pro** — loop-based live performance tool
- **Drambo** — modular-style sequencer

DrawnCurve is an **AUv3 plugin** — a specific standard for iOS music plugins.
You load it inside a host app, connect its output to a synth's input, and it
starts controlling that synth.

---

## What Is Routing?

Routing means connecting the output of one thing to the input of another —
like plugging a cable between two boxes.

In this case:
- DrawnCurve outputs MIDI messages (the looping curve)
- The user "routes" those messages to a synthesiser app
- The synth receives them and moves one of its knobs in real time

Most hosts handle routing visually — the user drags a virtual cable from
DrawnCurve's output to the synth's input.

```
  ┌────────────────┐       virtual cable       ┌──────────────┐
  │  DrawnCurve    │ ──── MIDI messages ──▶    │  Synth app   │
  │  (the curve    │                           │  (filter     │
  │   loops here)  │                           │   moves)     │
  └────────────────┘                           └──────────────┘
```

---

## The Four Output Modes

When the curve loops, it has to send *some kind* of message. There are four
kinds the user can pick. Think of them as four different "remote control
channels":

| Button | Name | Plain meaning | Typical use |
|--------|------|---------------|-------------|
| **CC** | Control Change | Move a named knob on the synth | Filter, volume, LFO depth, any assignable knob |
| **Aft** | Aftertouch | "Pressing harder" signal | Expression, vibrato depth, breath |
| **PB** | Pitch Bend | Sliding the pitch up/down | Vibrato, pitch glide, whammy-bar effect |
| **Note** | Note On/Off | Triggering notes like keys on a piano | Arpeggiated chords, melody following the curve height |

The user picks one mode before drawing. The curve's vertical position maps
to the value range of the chosen mode.

---

## Key UI Areas (Annotated)

```
┌──────────────────────────────────────────────────────────────────┐
│ [▶Play][✕Clear]  [CC][Aft][PB][Note]  [Sync]  [?Help]  [☾Theme] │  ← toolbar
│ [→Fwd][←Rev][↔P-P]     Y- Y+  X- X+                            │  ← direction + grid
│ CC#:[74] Channel:[1]  Smooth:[────●──]                           │  ← parameter row
│ Range: [|══●══════════════════●══|]  Speed: [──●──] 1.0×         │  ← range + speed
│ ┌────────────────────────────────────────────────────────────┐   │
│ │                                                            │   │
│ │              DRAW YOUR CURVE HERE                          │   │  ← main canvas
│ │                    (playhead moves through it)             │   │
│ └────────────────────────────────────────────────────────────┘   │
└──────────────────────────────────────────────────────────────────┘
```

**Toolbar (top row)**
- ▶/⏸ Play / Pause — starts and stops the looping curve
- ✕ Clear — wipes the canvas, ready for a new drawing
- CC / Aft / PB / Note — the four output mode buttons (radio group)
- Sync — locks playback speed to the host app's tempo (BPM)
- ? — opens a help overlay listing every control
- ☾/☀ — dark/light theme toggle

**Direction row**
- → Fwd — plays the curve left to right (default)
- ← Rev — plays right to left
- ↔ P-P — "Ping-Pong": bounces back and forth
- Y-/Y+/X-/X+ — adjusts the visual grid lines on the canvas (cosmetic)

**Parameter row**
- CC# — which "knob number" to send when in CC mode (0–127)
- Channel — which MIDI channel (1–16; like a TV channel, keeps signals separate)
- Smooth — how much to round off sharp corners in the curve's output

**Range slider**
- The two-thumbed slider clips the output to a window — for example, only
  use the top half of the range, so the motion never goes below 50%.

**Speed / Beats**
- In manual mode: multiplies the loop speed (0.25× = slow, 4× = fast)
- In Sync mode: the loop length in beats (e.g. "4 beats" = one bar at any BPM)

**Canvas**
- The large area where the user draws. A moving playhead (vertical line)
  shows the current position in the loop.

---

## Six Use-Case Sketches

1. **Filter sweep pad** — Draw a slow rising arch. Route CC74 to a synth's
   filter cutoff. The pad gradually brightens over 4 bars, then resets.

2. **Tremolo / amplitude wobble** — Draw a fast zigzag. Route CC7 (volume) to
   a guitar tone. Creates a rapid tremolo effect.

3. **Breath / swell** — Draw a gentle S-curve. Route Aftertouch to a string
   ensemble's expression. The strings breathe in and out organically.

4. **Pitch vibrato** — Draw a tight sine-like wave. Route Pitch Bend to a
   synth lead. Produces a natural vibrato without automation tracks.

5. **Note melody** — Draw a staircase shape (flat → step up → flat → step up).
   In Note mode the curve's height triggers different pitches, creating a
   looping melody of held notes.

6. **Glitchy stutter** — Draw jagged spikes. Route CC to a delay mix knob.
   Rapid on/off stuttering creates a rhythmic glitch effect.

---

## Glossary (for this document)

| Term | Definition |
|------|-----------|
| AUv3 | Apple's standard format for iOS audio/MIDI plugins |
| CC | Control Change — a MIDI message that moves a numbered knob (0–127) |
| Aftertouch | A MIDI message for "pressing harder" after a note is struck |
| Pitch Bend | A MIDI message that slides pitch up or down |
| Note On/Off | MIDI messages that trigger and release notes |
| DAW | Digital Audio Workstation — any music recording/production app |
| BPM | Beats Per Minute — the tempo of a musical piece |
| Loop | A section that repeats continuously |
| Playhead | The cursor/indicator showing the current position in a loop |
| Plugin host | An app that loads and runs plugins (GarageBand, AUM, etc.) |
| Routing | Connecting the output of one module to the input of another |
