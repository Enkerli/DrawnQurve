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

### 2026-03 | DECIDED | Help overlay first-launch push — fixed

**Bug**: `addAndMakeVisible(helpOverlay)` in the DrawnCurveEditor constructor
calls `setVisible(true)` on the overlay, overriding the `setVisible(false)` set
in `HelpOverlay`'s own constructor. Result: the overlay appeared unsolicited on
every first launch — exactly the "push revelation" NN/g identifies as an anti-pattern.

**Fix**: Changed to `addChildComponent(helpOverlay)`, which registers the component
in the Z-order without affecting its visibility. The overlay now remains hidden until
the user explicitly taps "?" (pull revelation, correct behaviour).

**Cross-reference**: `PluginEditor.cpp` line ~721; commit following 2026-03 decisions.

---

### 2026-03 | DECIDED | Direction control: SegmentedControl replaces 3 TextButtons

**Context**: Row 2 used three `juce::TextButton` instances in a radio-button pattern
(Fwd / Rev / P-P). The buttons used a `SymbolLF` (LookAndFeel_V4 subclass) to draw
filled-triangle arrowheads. Triangles alone (▶) are visually ambiguous — they do not
clearly communicate "play direction" without additional context.

**Decision**: Replace with a single `SegmentedControl` component (3 segments).
Arrows are drawn by a `SegmentPainter` lambda using stem+arrowhead `juce::Path` objects:
- Forward: `→` (single arrow, tip right)
- Reverse: `←` (single arrow, tip left)
- Ping-Pong: `← →` (two outward arrows with a small gap)

No font required. The `SegmentedControl` is generic and reusable; it accepts an optional
`SegmentPainter` callback for fully custom per-segment drawing.

**Future 4th segment**: Adding "Random" later is a one-line `setSegments()` update.
The APVTS `playbackDirection` parameter would need an additional choice; that is deferred
until there is a concrete design for random playback behaviour.

**Cross-reference**: `Source/SegmentedControl.h` (new); `Source/PluginEditor.h/cpp`.

---

### 2026-03 | DECIDED | Symbol fonts in JUCE: juce_add_binary_data approach

**Context**: The project needs icon/symbol glyphs (arrows, possibly pressure icons for
Aftertouch, note symbols) that the built-in JUCE Bitstream Vera font does not contain.
AUv3 XPC sandbox forbids filesystem access, ruling out loading fonts from a file path.

**The "proper non-JUCE" iOS approach**:
1. Add the TTF to the Xcode project bundle
2. Declare it in `Info.plist` under `UIAppFonts`
3. Use `UIFont(name:size:)` to load it at runtime — iOS CoreText resolves the font
   by name from the app bundle, with no filesystem path needed

**The JUCE equivalent**:
1. Add `juce_add_binary_data(DrawnCurveAssets SOURCES Assets/SymbolFont.ttf)` to
   `CMakeLists.txt` — JUCE compiles the font file into a C++ `const char[]` array
2. Link `DrawnCurveAssets` to the plugin target
3. At runtime:
   ```cpp
   static const juce::Typeface::Ptr tp =
       juce::Typeface::createSystemTypefaceFor(BinaryData::SymbolFont_ttf,
                                               BinaryData::SymbolFont_ttfSize);
   juce::Font f (juce::FontOptions{}.withTypeface(tp));
   g.setFont(f.withHeight(20.f));
   g.drawText(u8"\U000Exxxx", bounds, juce::Justification::centred);
   ```
Both approaches load font data from memory (the binary blob), bypassing the filesystem.
Both are safe in a sandboxed XPC process.

**Current status**: The `CMakeLists.txt` block is present but commented out, ready to
activate when a font file is chosen. The `SegmentedControl` infrastructure already
supports a `SegmentPainter` callback that can call `g.setFont(symbolFont)`.

**Candidate fonts** (all open-source / permissive licence):
- **Material Symbols** (Google, Apache 2.0) — variable font; ligature-name API
  (`g.drawText("arrow_forward", ...)`) and Unicode PUA codepoints both work
- **Phosphor Icons** — clean, consistent, available as TTF
- **Bootstrap Icons** — broad coverage, MIT licence

**SF Symbols**: Not a traditional font; accessed via `UIImage(systemName:)` in UIKit.
Requires an ObjC++ bridge in JUCE. Deferred (see SF Symbols investigation entry below).

**Cross-reference**: `CMakeLists.txt` (commented block); `Source/SegmentedControl.h`
header comment.

---

### 2026-03 | OPEN | Icons and symbols — SF Symbols access from JUCE/Xcode

**Context**: The current direction-button arrow symbols work but are minimal (JUCE
`Path`-drawn triangles). The iOS/macOS SF Symbols library (available in any Xcode
project; iOS 13+) contains thousands of system-quality icons including:
- `arrow.forward` / `arrow.backward` / `arrow.triangle.2.circlepath` (direction)
- `waveform.and.mic` / `hand.point.up.left` (gesture/curve metaphors)
- `music.note` / `metronome` (musical operations)
- Pressure/force symbols relevant to Aftertouch

**JUCE complication**: JUCE's font/typeface system doesn't natively expose SF Symbols.
Candidate approaches (in ascending complexity):
1. **Export to asset** — use the SF Symbols macOS app to export specific symbols as
   SVG/PDF; include in the Xcode bundle; load as `juce::DrawableSVG` / `juce::Image`.
   Lowest friction; symbols are static but always match intended appearance.
2. **ObjC++ bridge** — `juce::Image` from UIKit/NSImage rendering of
   `UIImage(systemName:)`. Dynamic; respects SF Symbols multicolour / hierarchical
   rendering. Requires a thin platform bridge file.
3. **CoreText** — render the `.SFNS` / `.SFUIDisplay` typeface directly through
   CoreText into a `juce::Image` at the button's required size. Most flexible;
   highest complexity.

**Recommendation**: Start with approach 1 for the UI redesign sprint; migrate to 2 if
animation or adaptive colour is needed.

**Pending**: Identify the exact set of symbols needed before implementation.
Test: do SF Symbol icons test significantly better than the current path-drawn arrows
for the T4 (direction) and T3 (output type) benchmark tasks?

---

### 2026-03 | NOTED | This build as a playground — eventual feature split

**Context**: DrawnCurve in its current form is explicitly a development playground.
Adding features like scale quantization, multi-stroke, incoming MIDI display makes
sense here because:
1. No full Apple developer account available yet for a separate product
2. No design system exists yet — adding features here generates design evidence
3. The plugin is already useful and fun with current features; additions are additive
4. Testing multiple features together reveals interactions that would be missed in
   separate products

**Implication**: Some features implemented here may eventually belong in a separate
plugin. Scale quantization is the primary candidate — a standalone scale quantizer
for iOS/AUv3 is a legitimate product. When the design system exists and the dev account
allows it, splitting is the right call.

**For now**: implement, test, learn. #Playground.

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
