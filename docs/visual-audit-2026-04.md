# DrawnQurve — Visual Design Audit (2026-04)

**Reference screenshot**: 2026-04-18, AUv3 in AUM, iPad, single-lane mode,
Diatonic family selected, E♭ root, "Both" view (lattice + wheel).
Post Option-B family-bar refactor and the vertical-mask-column / left-sidebar
label move (both landed earlier this session).

This document is a *design audit*, not an implementation plan. Each finding
ends in a fix proposal so we can argue about direction before anyone touches
code. The fix list at the bottom (§ 4) is the actionable summary.

---

## 0. Why now

The screenshot revealed that several recent micro-fixes (Option-B header,
label column, wheel labels) have each cleaned up *one* zone but the editor as
a whole still reads as several different design dialects glued together. The
specific complaint that triggered this audit — *"the scalename and bitmap are
on a layer above a scale pill with some additional chrome around"* — is one
instance of a systemic pattern, not a local bug. Fixing the local instance
without naming the pattern would just defer the same complaint to the next
zone.

The TestFlight alpha needs the editor to feel like *one app* before testers
see it. That makes this the right moment for a structural pass.

### 0.1 Two organising frames

This audit is structured around two ideas that ran through the user
feedback in 2026-04:

* **Surface discipline** — every visible chrome surface (panel, pill,
  border, fill) should serve a clear semantic role, and there should be
  *very few of them*. This frame drives §§ 2.1, 3-P1, 3-P3, 3-P4 and most
  of Tier 1.
* **Playflow tiering** — DrawnQurve is a *performance instrument as much as
  a configuration tool*. Its controls live on a temperature scale:
  performance-hot (touched constantly mid-musicking) → deliberate (touched
  per piece) → setup (touched per session) → session-stable (touched once
  per launch) → emergency (panic). A control's visual prominence and touch
  target should match its temperature. This frame drives §§ 2.5, 3-P5,
  3-P6 and is the basis for re-prioritising several Tier 2 items.

Both frames are starting positions. Argue with either before committing.

---

## 1. Zone inventory

The editor (everything below host chrome in the screenshot) decomposes into
three top-to-bottom zones plus a right rail and overlays. **Theming** sits
across all zones as a cross-cutting concern.

| Zone           | Role                              | Approx. height       | Source landmark                              |
|----------------|-----------------------------------|----------------------|----------------------------------------------|
| **A. Eyebrow** | Lane selection + global/lane chrome | 52 px (single-lane) | `PluginEditor.cpp` ~4515                     |
| **B. Canvas**  | Stage where the user draws curves | flex (most of view)  | `curveDisplay`, `curveLeftRail`, `curveBotRail` |
| **C. Musical zone** | Scale picker (collapsible)   | 44 collapsed / 260 expanded | `_musicalPanel`, ~4732                  |
| **D. Right rail** | Lane matrix (multi-lane only)  | ~280 px wide         | `_lanesPanel`, `_focusedLanePanel`, `_globalPanel` |
| **E. Overlays** | Scale browser, routing, help, settings | float          | `_scaleOverlay`, `_routingOverlay`, `helpOverlay` |
| **F. Theming** | Cross-cutting visual language     | n/a                  | `Source/UI/DrawnCurveLookAndFeel.h`          |

### A. Eyebrow strip (52 px tall in single-lane)

Reading left → right in the screenshot:
1. **`dirControl`** — 4-segment direction picker, currently rendered as two
   large purple round buttons (◀️◀ ▶▶). Largest visual element in the strip.
2. **`laneFocusCtrl`** — segmented selector with `✤ + 1` style (the small `1`
   chip + `+`/`−` buttons next to it).
3. **`syncButton` + `laneSyncBtn`** — two small dark grey square buttons
   (the ♪ + tiny stack icon).
4. **Routing/lane-type button** (`laneTypeBtn[focused]`) — small dark button
   showing the lane's MIDI message type.
5. **Three labelled sliders** — Range (`C-1 – G9`), Speed (`1.00×`),
   Smooth (`8%`). Each has the value text in what looks like an input field
   above a horizontal slider.
6. **Right cluster** — 6 small buttons in two visual sub-groups:
   `♩  ♭  ⚙` (context-ish) then `! ? ☾` (global-ish). They share neither
   spacing nor styling boundary — they read as a single run.

That's 13–14 distinct interactive elements in 52 px vertical, in at least
**five different visual treatments**:

| Treatment              | Used by                        |
|------------------------|--------------------------------|
| Large purple round     | `dirControl` segments          |
| Small dark filled      | `syncButton`, `laneSyncBtn`, `laneTypeBtn`, others |
| White outlined pill    | `laneFocusCtrl` segments       |
| Text input + slider    | range / speed / smooth         |
| Notification pill (red)| `!` alert                      |

### B. Canvas + rails

* **`curveLeftRail`** — note ladder (`G♭9` … `E♭-1`) plus stacked density
  steppers (`8 4 3 2 1` and `1 2 3 4 5` with `4 32st` between), plus a
  vertical handle, plus `#` and lock icons at the bottom. ~70 px wide,
  visually busy with at least **three column tracks** of small text.
* **`curveDisplay`** — main grid. Currently shows horizontal note rows and a
  faint percentage ruler. Empty state: centred "Draw Lane 1 here" placeholder.
* **`curveBotRail`** — `# 🔓 [|||] 4 [|||]` density quick-toggles. Tiny.

### C. Musical zone (expanded, ~260 px)

Three rows top → bottom (per Option-B):
1. **Family bar** (28 px) — 9 family tabs + ★ Recent on the left,
   `Rows | Wheel | Both` segmented selector + ▴ collapse arrow on the right.
   Gold-on-cream "Both" indicates active.
2. **Subfamily chip row** (28 px) — `Ionian Dorian Phrygian Lydian Mixolydian
   Aeolian Locrian` for the Diatonic family.
3. **Picker band** (184 px) — left sidebar (`↻ ● ○ ◑ ◆` button column +
   `Custom / 2409` label column) + lattice (5+7 piano-row note bubbles) +
   chromatic wheel.

The wheel and lattice are now visually clean (post-fixes this session). The
*remaining* friction is in the label column and in the relationship between
the panel surface and the chip pills.

### D. Right rail (multi-lane only — not in audit screenshot)

When `activeLaneCount > 1`, a ~280 px right rail appears containing:
* **`_lanesPanel`** — one row per active lane (`laneTypeBtn`,
  `laneDetailLabel`, `laneChannelLabel`, `laneTeachBtn`, `laneMuteBtn`,
  `laneSelectBtn`). Each row is the lane's "matrix" identity row.
* **`_focusedLanePanel`** — extended controls for the *currently focused*
  lane (phase offset, one-shot, per-lane speed if enabled, delete-lane).
* **`_globalPanel`** — global controls that don't fit in the eyebrow.

Sub-issues to flag for the rail (not yet visible in any audit screenshot but
known from prior sessions):
* The matrix uses **lane colors** for identity, which is correct (P3-aligned
  in § 3) — but the right rail's panel chrome competes with those colors.
* Whether the rail's three sub-panels (`_lanesPanel` / `_focusedLanePanel` /
  `_globalPanel`) read as one continuous rail or as three stacked pills is
  *the* surface-level question for this zone (§ 2.1).
* The matrix maxes out at `kMaxLanes = 4` and stacks vertically. If lanes
  scale beyond 4 (see § 6 roadmap), the rail needs a scrolling or compaction
  strategy.

A separate audit pass with a multi-lane screenshot is needed to fully
characterise this zone (see Tier 2 § T2.5 below).

### E. Overlays

Three modal-ish overlays (none of them strictly modal — they're floating
panels that dismiss on tap-outside):

* **`_scaleOverlay`** — alternative entry to the scale picker, narrower
  (340 px) panel anchored over the curve display. Mirrors the inline
  musical-zone layout. Currently used as fallback when the inline zone is
  collapsed.
* **`_routingOverlay`** — anchored to the focused lane's `laneTypeBtn`,
  shows the message-type / channel / detail picker. Persists during edit;
  dismisses on tap-outside.
* **`helpOverlay`** — onboarding / contextual help. Full-editor overlay with
  ?-button affordance.

Sub-issues for overlays:
* All three use *different* surface treatments (different shadows, borders,
  background opacities). They should share one "elevated panel" treatment.
* `helpOverlay` content is out of scope for this layout audit, but its
  *chrome* (the panel that holds the help content) should match the others.
* Anchoring vs. centring vs. fullscreen: each overlay picks differently.
  Should be consistent — anchored overlays for context-specific actions,
  centred for global / setup actions.

### F. Theming (cross-cutting)

`DrawnCurveLookAndFeel` defines colour tokens, font sentinels (`DC-Serif`,
`DC-Sans`), and stroke metrics. There are dark and light themes
(`themeButton` toggles between them — `_lightMode`).

Sub-issues:
* The theme toggle is a session-stable control (§ 0.1) parked among
  performance-hot eyebrow chrome. Wrong tier.
* Theme tokens are used inconsistently — some components hard-code colours
  (e.g. the purple `dirControl`), bypassing the LookAndFeel.
* Light theme has not been audited recently against the cream musical zone
  panel — the cream may be insufficiently distinct from the light editor
  background.
* No third "high-contrast" or "performance" theme exists, though it would be
  a natural addition once the token system is consolidated.

---

## 2. Patterns of friction

### 2.1 Surface-level proliferation

Counting from the editor background outward, several zones stack three or
more visual surface levels:

```
editor bg → musical-zone panel → label pill (with border)   — Custom/2409
editor bg → musical-zone panel → chip outlined pill         — family tabs
editor bg → eyebrow row        → input chrome (range/speed/smooth labels)
editor bg → canvas             → stepper pill chrome        — left rail
```

The user's "scale pill with chrome around" complaint is specifically item 1
in that list. Items 2-4 are the same pattern in other zones. The issue isn't
that any single component is wrong — it's that the editor has no shared rule
for *how many surface levels are allowed*.

**Reference (webapp v2 spec)**: two surface levels — editor background and
*one* elevated panel. Anything that needs to be distinguished within a panel
should use **typography weight, padding, and color** rather than a chrome
shape.

### 2.2 Button vocabulary inconsistency

The eyebrow alone contains five visual treatments (table in §1.A). Across
the whole editor I count **at least seven distinct button styles** that
serve overlapping roles. There's no rule governing which style means what.
A user can't tell from shape whether a control is global, lane-scoped,
toggleable, or destructive.

### 2.3 Information hierarchy ambiguity

In the picker label column the scale name and the bitmask are rendered with
*equal* visual weight (same font, same chrome, same justification). They
serve very different roles:

* **Scale name** (`Dorian`, `Custom`) — *primary* identification, what the
  musician thinks about
* **Bitmask** (`2409`) — *tertiary* / debug-ish, useful for sharing or pasting
  but not for everyday musicking

Same problem repeats in the eyebrow's slider trio: the value text
(`1.00×`, `8%`) is rendered with the same chrome as the slider itself,
suggesting they're the same level of importance. They aren't — the slider
is the control, the value is a readout.

### 2.4 Empty-canvas weight

In the screenshot the curve display is mostly empty (single lane, undrawn).
The note ladder + density steppers + bottom rail are visible regardless,
which means in the empty state the *chrome* outweighs the *content* by a
large margin. Once a curve is drawn this rebalances, but first impressions
matter for testers.

### 2.5 Performance-tier mismatch (the playflow pattern)

The single most consequential pattern. Sorting current controls by their
*musicking temperature* (per § 0.1) and comparing to their *visual
prominence* in the current layout reveals systematic mismatches:

| Tier               | Controls (current layout)                         | Visual prominence | Match? |
|--------------------|---------------------------------------------------|-------------------|--------|
| **Performance-hot**| Drawing the curve (canvas)                        | Largest area      | ✓ |
|                    | Scale switching (musical zone)                    | Bottom panel, prominent when expanded | ✓ |
|                    | Quantization (left-rail steppers)                 | Tiny text, three columns | ✗ undersized |
|                    | Range / transposition (eyebrow slider)            | Small touch target | ✗ undersized |
|                    | "Recent" scales as setlist                        | One tab among nine families | ✗ underpromoted |
| **Deliberate**     | Direction (`dirControl`)                          | Largest button in eyebrow (purple) | ✗ overweighted |
|                    | Speed / smooth (eyebrow sliders)                  | Same chrome as range | ~ ok |
|                    | Transport (host's job)                            | Host chrome | n/a |
| **Setup**          | Lane msg type / channel / detail (routing overlay)| Small button → overlay | ✓ |
|                    | Lane add/delete (eyebrow `+ −`)                   | Mixed with hot controls | ✗ scope-mixed |
| **Session**        | Dark/light (`themeButton`)                        | Same eyebrow as hot | ✗ scope-mixed |
|                    | MIDI Out (standalone)                             | Same eyebrow as hot | ✗ scope-mixed |
| **Emergency**      | Panic (`!` button)                                | Red pill in eyebrow | ✓ |

The biggest mismatches are (a) **direction overweighted** (large saturated
purple buttons for a deliberate-tier control), (b) **range/quantization
undersized** (tiny touch targets on performance-hot controls), and (c)
**Recent underpromoted** (the most powerful performance gesture for scale
switching is hidden as one tab among nine).

### 2.6 Touch targets don't match performance frequency

The Apple HIG nominal touch target is 44 pt. The current layout uses 28 pt
as a comfortable floor for non-hot controls — fine for setup-tier and
session-tier. But several **performance-hot controls fall below 28 pt**:

| Control                        | Current touch target | Tier | Adequate? |
|--------------------------------|----------------------|------|-----------|
| Range slider (drag)            | ~14 pt thumb         | Hot  | ✗ painfully small |
| Range thumb (transposition)    | ~14 pt               | Hot  | ✗ |
| Left-rail density steppers     | ~16 pt rows          | Hot  | ✗ |
| Note ladder labels             | n/a (display only)   | n/a  | – |
| Family tabs                    | 28 pt                | Hot  | ~ floor |
| Subfamily chips                | 28 pt                | Hot  | ~ floor |
| Lattice note circles           | ~44 pt               | Hot  | ✓ |
| Wheel nodes (with hit-radius padding) | ~24 pt           | Hot  | ✓ (hit-padding compensates) |
| `dirControl` segments          | ~40 pt               | Deliberate | ✓ (overshoots, but ok) |

**Smaller touch targets create higher mid-performance error rates** and are
specifically called out by the user as friction on the range slider:
*"sliding the range for quick transposition is just plain delightful, though
the touch target is too small"*. This is the most actionable instance of a
broader principle (§ 3 P6).

### 2.7 Global vs context-specific are visually merged

The right end of the eyebrow has `♩ ♭ ⚙ ! ? ☾` in one undivided run. Some of
those affect the focused lane's behaviour (♩ note grid, ⚙ routing); others
are global app state (? help, ☾ theme). Mixed scope + identical styling =
the user has to memorise which is which instead of reading it from the UI.

This is a special case of § 2.5 (performance-tier mismatch): session-stable
controls (`☾`, MIDI Out) sharing space with deliberate-tier controls (♩, ⚙)
sharing space with emergency (`!`).

### 2.8 Overlay chrome inconsistency

`_scaleOverlay`, `_routingOverlay`, `helpOverlay` each use slightly
different elevated-panel treatments (shadows, borders, background opacities,
corner radii). For a system that should feel like one app, three different
"floating panel" looks is two too many.

### 2.9 Theming token bypass

Some chrome (notably `dirControl`'s purple) hard-codes colours instead of
pulling from theme tokens. A theme switch (or a future high-contrast
"performance" theme) won't reach those components without code changes. The
theming layer is doing less work than its name implies.

---

## 3. Proposed design principles

These are starting positions. We can argue about each before anything ships.

### P1 — Two surface levels, period

* Level 0: editor background (current dark / cream depending on theme)
* Level 1: panel surface (musical zone, focused-lane panel, overlays)
* No level 2. If a control needs to feel "elevated" *within* a panel, do it
  with a 1-px outline and no fill, not with a stacked pill.

### P2 — Three button sizes, two styles

| Size       | Use                                         | Examples                           |
|------------|---------------------------------------------|------------------------------------|
| **L (40)** | Direction (the "transport" of musicking)    | `dirControl`                       |
| **M (28)** | Standard touch target everywhere else       | family tabs, chips, mask buttons   |
| **S (22)** | Dense info (alert/help/theme footer)        | `! ? ☾`                            |

Two styles only:
* **Solid** — primary action / current state
* **Outlined** — selectable / inactive

Drop the purple round / dark grey square / white outlined / text-input
zoo. Pick one outlined style, one solid style. Differentiate scope by
*placement and grouping*, not by re-skinning.

### P3 — Color is semantic, not decorative

* **Lane colors** (per `feedback_terminology.md`) — only used to tag content
  that belongs to a specific lane (the curve, the lane's matrix row, its
  mute/solo state)
* **Cream** — the musical zone panel surface (zone identity)
* **Gold accent** — selected / current root note
* **Red** — only destructive or error states (the `!` alert qualifies; nothing
  else should be red)

Implication: the purple `dirControl` buttons probably shouldn't be purple.
They should be the standard solid button color, possibly with a subtle accent
when the current direction is active.

### P4 — Typography carries hierarchy

Within a panel, distinguish primary / secondary / tertiary by:
* Size (15 / 13 / 11 pt — match webapp v2)
* Weight (medium / regular / regular-italic)
* Color (full text / dim text / very-dim text)

Not by chrome. The "Custom" label on the panel needs no border to be
distinct — italic regular + dim text reads as "this is metadata" already.

### P5 — Playflow tiering drives layout

Replaces the earlier "group by scope" heuristic with the richer frame from
§ 0.1. Five tiers, each with its own visual + spatial budget:

| Tier               | Treatment                                         | Where it lives |
|--------------------|---------------------------------------------------|----------------|
| **Performance-hot**| Largest, most accessible, always visible          | Canvas (drawing); musical zone (scale); eyebrow centre (range/quant); always-visible Recent strip |
| **Deliberate**     | Standard size, always visible but visually quieter than hot | Eyebrow flanks (transport / direction / speed) |
| **Setup**          | One tap into a per-lane overlay                   | `_routingOverlay` (already correct); per-lane add/delete in a dedicated lane menu, not in the hot eyebrow |
| **Session-stable** | Tucked behind an "App" / "Settings" affordance    | Top-right corner shelf, NOT in the eyebrow |
| **Emergency**      | Visually distinctive (red), single tap            | Far corner — separate from everything else |

Concrete implications:
* `themeButton`, `MIDI Out`, future "Save Setup" → out of eyebrow into a
  settings shelf
* `+`/`−` lane add/delete → into a lane menu next to `laneFocusCtrl`, not
  scattered in the eyebrow
* "Recent" scales → promoted from a single tab to an always-visible strip
  (likely above or to the right of the family bar) since it's the primary
  performance gesture for scale switching
* `dirControl` → demoted visually to deliberate-tier (smaller, less
  saturated) since the direction is rarely changed mid-musicking
* The "panic" `!` → keep red, but separated by clear empty space from
  everything else

### P6 — Touch targets scale with performance frequency

Apple HIG nominal is 44 pt. Adopt a per-tier floor:

| Tier               | Touch-target floor | Comfortable target |
|--------------------|--------------------|--------------------|
| Performance-hot    | 36 pt              | 44 pt              |
| Deliberate         | 28 pt              | 32 pt              |
| Setup              | 28 pt              | 28 pt              |
| Session-stable     | 22 pt              | 28 pt              |
| Emergency          | 36 pt              | 44 pt (red, isolated) |

Implication for the range slider specifically: thumb width grows from
~14 pt to at least 36 pt; the slider track height grows from the current
~6 pt to at least 24 pt to give vertical drag tolerance. The trio
(range / speed / smooth) deserves the same treatment.

Implication for left-rail steppers: stepper rows grow from ~16 pt to at
least 28 pt, OR they collapse into a stepper widget that exposes a 36 pt
touch target with secondary increments behind a long-press / scrub gesture.

### P7 — Reserve layout for the roadmap

Three near-term roadmap items have layout implications today (per § 6
below): preset save/load, MIDI input → curve generation, and lane scaling
beyond 4. Decisions in this audit should leave hooks for those rather
than paint into corners.

In particular:
* A **settings shelf** (P5) creates the natural home for "Save Setup",
  "Load Setup", and per-app preferences — adding presets later doesn't
  require re-architecting the eyebrow.
* The **musical zone** is currently dedicated to scale picking. If MIDI
  input is also a "performance gesture for generating qurves," it should
  live in a peer position to the musical zone (a sibling expandable panel
  on the canvas left or right), not inside it.
* The **lane matrix** in the right rail should switch to a scrolling /
  virtualised model now if we expect >4 lanes, even though current
  `kMaxLanes = 4`. A vertical scroll strategy is cheaper to design once
  than to retrofit later.

---

## 4. Concrete fixes — prioritized

Tiered by impact-to-effort and by tier-mismatch severity (the playflow
frame from § 0.1 / § 3 P5 promotes performance-hot fixes ahead of
session-stable cosmetics). Tier 1 = same-day; Tier 2 = one focused session
each; Tier 3 = needs a sketch / design conversation before code.

### Tier 1 — Same day, high signal-to-effort

* **T1.1** Strip chrome from `scaleLabel` + `maskLabel`. Zero
  `Label::backgroundColourId`, `outlineColourId`. Make `maskLabel` italic +
  dim. Result: the label column reads as "metadata floating on the panel"
  instead of "two more buttons among the buttons." (Implements P1 + P3 + P4
  for the specific zone the user complained about.)
* **T1.2** Range slider touch-target enlargement. Per § 2.6 + P6: thumb
  width 14 → 36 pt, track height 6 → 24 pt. Apply the same treatment to
  the speed and smooth sliders for consistency. **Promoted from Tier 2 in
  the v1 audit** because the user explicitly flagged this as friction on a
  performance-hot control. Pure metrics change, no layout impact.
* **T1.3** Insert a 12-px gap between the lane-context cluster and the
  session-stable cluster in the eyebrow right side. Pure spacing change,
  immediate legibility win. Stopgap until T2.6 (settings shelf) lands.
* **T1.4** Standalone iPad safe-area inset. Conditional on
  `wrapperType_Standalone`, reduce `getLocalBounds()` by
  `Desktop::getDisplays().getPrimaryDisplay()->safeAreaInsets` before
  layout. Self-contained 5-line change in `resized()`.

### Tier 2 — Each is one focused session

* **T2.1** Unify chip / tab / segmented-control chrome. Right now
  `familyBtns`, `subfamilyBtns`, `scaleViewCtrl`, `laneFocusCtrl` use
  related-but-not-identical outlined-pill treatments. One LookAndFeel
  override + shared metrics constants → single visual style across all
  four. (Foundation for the rest of Tier 2.)
* **T2.2** Demote sliders' value labels to readouts. The `C-1 – G9`,
  `1.00×`, `8%` text fields are in input chrome but aren't really being
  used as inputs. Render them as plain label text above the slider, no
  chrome. Pairs with T1.2 — touch target grows; the value chrome shrinks.
* **T2.3** Recolour + resize `dirControl` to deliberate-tier. Per § 2.5:
  the segments are currently overweighted for a control that's rarely
  touched mid-performance. Standard solid button colour (not saturated
  purple), 28-32 pt size (not 40+).
* **T2.4** Empty-canvas treatment. When the curve display is undrawn, dim
  the left-rail stepper text + the bottom rail. Bring them back to full
  intensity once a stroke exists. Reduces chrome-outweighs-content at
  first paint.
* **T2.5** Multi-lane rail audit pass. Take a multi-lane screenshot,
  characterise the rail's three sub-panels (`_lanesPanel` /
  `_focusedLanePanel` / `_globalPanel`), and apply P1 (one surface level)
  + P3 (lane colours own the identity, chrome doesn't compete). Likely
  outcome: drop the inner panel borders, let lane-coloured tags carry
  identity, single elevated rail surface only.
* **T2.6** Settings shelf (top-right corner). Per P5: move
  `themeButton`, `MIDI Out` (standalone), `?` help into a small
  top-right shelf above the eyebrow. Eyebrow is then free for
  performance-hot + deliberate-tier controls only. Creates the home for
  future preset Save/Load (P7).
* **T2.7** Scale-palette strip (replaces the v2 "Recent-as-setlist"
  design). User feedback 2026-04-18: don't model this as a setlist or
  setlist-mode. Model it as a **scale palette** — each entry is the
  bundle `(scale family, mode, root)`, savable as one unit, promoted to
  an always-visible strip for fast switching mid-musicking.
  - Auto-populated from recent picks (current "Recent" semantics) plus
    user-pinned palettes
  - Tap to switch scale + root in one gesture
  - Persisted as part of the setup save (§ 6.1) — palettes are the
    lightweight save target, full setups are the heavyweight one
  - Strip lives above the family bar, always visible even when the
    musical zone is collapsed (so palette-switching works without
    expanding the zone)
* **T2.8** Overlay chrome unification. Apply one elevated-panel treatment
  (shadow, border, radius, opacity) to `_scaleOverlay`, `_routingOverlay`,
  `helpOverlay`. Single LookAndFeel override.
* **T2.9** Theming token consolidation. Audit every hard-coded colour in
  the editor (e.g. `dirControl` purple) and route through
  `DrawnCurveLookAndFeel` tokens. Audit dark + light themes against the
  cream musical-zone panel. Lays groundwork for a future high-contrast
  "performance" theme.

### Tier 3 — Needs sketch / design conversation

* **T3.1** Density steppers redesign. The two stepper columns flanking
  `4 32st` are functional but visually noisy AND undersized for hot-tier
  controls (§ 2.6). A single combined stepper widget or a radial picker
  would shrink the left rail significantly. Worth a separate session
  with a sketch in `mockups/`.
* **T3.2** Help-overlay content + IA. T2.8 fixes the chrome; the *content
  inside* the help overlay is a separate question (what gets shown,
  ordering, search). Defer until the rest of the visual hierarchy is
  stable so help can describe a stable UI.
* **T3.3** Roadmap landing zones. P7 calls for reserved space — but the
  exact shape of the preset menu, the MIDI-input UI, and the >4-lane
  scrolling rail each deserves its own design pass with mockups. See § 6.

---

## 5. Standalone-only: iPad safe-area collision

Earlier note from the user: *"in iPadOS 26 standalone, the display overlaps
with OS content (time, date, WiFi, battery indicator…). That part isn't an
issue in the plugin."*

The screenshot we're auditing is the AUv3 in AUM, so the safe-area issue
isn't visible here — AUM provides its own chrome. But it's a real
standalone-only bug, scoped as **T1.3** above.

Sketch:
```cpp
void DrawnCurveEditor::resized()
{
    auto area = getLocalBounds();
   #if JUCE_IOS
    if (wrapperType == juce::AudioProcessor::wrapperType_Standalone)
    {
        if (auto* d = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        {
            const auto sa = d->safeAreaInsets;
            area.removeFromTop    (juce::roundToInt (sa.getTop()));
            area.removeFromBottom (juce::roundToInt (sa.getBottom()));
            area.removeFromLeft   (juce::roundToInt (sa.getLeft()));
            area.removeFromRight  (juce::roundToInt (sa.getRight()));
        }
    }
   #endif
    // existing layout uses `area`
}
```

Conditional on `wrapperType_Standalone` so AUv3 (where the host already
crops to its content rect) is untouched.

---

## 6. Roadmap-driven layout reservations

Three near-term roadmap items have layout implications today (per § 3 P7).
Decisions in this audit should leave room for them rather than paint into
corners; each gets its own design pass before code, but reserving the
*locations* now is cheap and prevents rework.

### 6.1 Save / load qurves and grids (presets)

**What it adds**: persistence of curves, grids, scale setups, possibly
full editor states ("setups"). Preset menu + save/load dialog +
preset-name affordance somewhere always-visible.

**Layout implications**:
* Naturally lives in the **settings shelf** (T2.6) under a "Presets"
  affordance — single place for Save / Load / Recent / Manage.
* The shelf needs to expand to a small dropdown / sheet when "Presets" is
  tapped — first overlay-style control launched from the shelf.
* A **current-preset name** indicator (small, italic, dim) belongs at the
  centre-top of the editor, near "DrawnQurve @M1:1" in the host chrome
  view. Tapping it opens the preset menu.
* If presets include the lane setup, the "Save Lane Snapshot" gesture also
  needs a per-lane affordance — likely an icon in the lane matrix row.
* **Scale palettes** (T2.7) are the *lightweight* save target — each
  palette is `(scale family, mode, root)` saved as one unit. Full
  *setups* are the *heavyweight* target — full editor state including
  all lanes, curves, scales, etc. The persistence layer should
  accommodate both. Scales affect the grid (which is part of the
  setup), so a setup includes its scale palette by reference rather
  than as a free-standing thing.

**Tier**: Defer to T3.3 design pass once the settings shelf (T2.6) ships.
Reserve the shelf's "Presets" slot now; defer the menu shape.

### 6.2 MIDI input → curve generation

**What it adds**: incoming MIDI from the host (or a virtual port) feeds an
analyser that proposes a curve. User can accept, edit, or discard.
Implies a *recording / armed* state per lane and a *generation* affordance
on the canvas.

**Layout implications**:
* Per-lane **input-arm** indicator in the lane matrix — small dot, lights
  up when MIDI is being captured for that lane. Naturally lives in the
  lane's matrix row alongside the existing teach / mute / solo toggles.
* **Canvas mode toggle** — drawing vs. MIDI-listening. Drawing is the
  current default; MIDI-listening shows incoming notes as a real-time
  preview overlaid on the canvas, with a "Generate Curve" affordance
  appearing once a phrase has been captured.
* **Performance temperature**: capturing a phrase is performance-hot
  (you do it mid-musicking); reviewing / accepting is deliberate. So the
  capture toggle wants a 36-44 pt touch target on or adjacent to the
  canvas; the accept button is a one-shot deliberate gesture (smaller,
  inline with the captured preview).
* This is **not a sibling of the musical zone** — it lives in/over the
  canvas itself, because that's where the resulting curve will appear.
  Earlier draft of P7 suggested otherwise; revised here: input is part
  of the canvas surface, not a peer panel.
* Reserve a **canvas overlay layer** in the layout for these
  preview/affordance elements now. Currently the canvas paints
  edge-to-edge with no provision for transient overlays.

**Tier**: Defer to T3.3 design pass with mockup. Reserve the per-lane
arm-indicator slot in the matrix row now (T2.5 should leave one
icon-width of space).

### 6.3 Lane scaling beyond 4

**What it adds**: `kMaxLanes` may grow (8? 16?). The right rail's lane
matrix currently stacks vertically with one full row per lane.

**Layout implications**:
* **Vertical scroll** in `_lanesPanel` if lane count exceeds what fits at
  the rail's natural height. JUCE `Viewport` is straightforward here.
* **Compact row mode** — if more than ~6 lanes, drop the per-row
  `laneDetailLabel` and `laneChannelLabel` to icons-only, recovering
  vertical space.
* **Focus mode** — only the focused lane shows full controls; other lanes
  collapse to a single-line summary. Already partially true (the
  `_focusedLanePanel` separation suggests this), but the threshold for
  collapsing should be lane-count-driven, not always-on.
* The right rail width (~280 px) should not grow with lane count — it's
  bounded by the canvas width budget.

**Tier**: T2.5 (multi-lane rail audit pass) should explicitly account for
this — design the rail for 8 lanes now even if `kMaxLanes` stays at 4
short-term. Cheap to design ahead; expensive to retrofit.

---

## 7. Suggested execution order

Adjusted from v1 to put the user-flagged performance-hot fixes (range
slider) ahead of cosmetic spacing changes:

1. **T1.4** (iPad safe area) — fixes a visible bug, no design implications
2. **T1.2** (range slider touch target) — closes the loop on the explicit
   user-flagged friction; pure metrics change
3. **T1.1** (label column chrome strip) — closes the loop on the user's
   original audit-triggering complaint
4. **T1.3** (eyebrow grouping gap) — 5-minute legibility win, stopgap
   until T2.6 settles the settings shelf properly
5. Revisit screenshot with all of Tier 1 applied. Re-audit before starting
   Tier 2.
6. Tier 2 starting with **T2.1** (chip / tab unification) because it's
   the foundation for the unified button vocabulary the rest of Tier 2
   assumes. Then **T2.6** (settings shelf) because it unblocks the
   roadmap items in § 6. Then T2.2-T2.5, T2.7-T2.9 in any order.
7. Tier 3 design conversations after Tier 1 + 2 land. T3.3 (roadmap
   landing zones) will be ready to act on once the settings shelf and
   the matrix audit are done.

---

## 8. Cross-references

* `docs/usability-guidelines.md` — should grow to incorporate the
  principles in § 3 (especially P5 playflow tiering and P6 touch targets)
  once they're agreed
* `docs/designer-primer.md` — needs an update once the unified button
  vocabulary lands (§ 3 P2) and once the settings shelf reshapes the
  eyebrow (§ 4 T2.6)
* `docs/design-decisions-log.md` — a corresponding OPEN entry tracks this
  audit; will be moved to DECIDED / SUPERSEDED as items land
* `docs/personas.md` — *not* used for tier validation in this audit. Per
  user feedback 2026-04-18: the same user can switch playflows, so a
  persona-by-persona check would over-fit the tiers. The tiers describe
  *modes of musicking* available to any user.
* `docs/interface-modes.md` — P7 / § 6.2 (MIDI input) introduces a canvas
  *mode* (drawing vs. listening); existing interface-modes doc is the home
  for that taxonomy
* `webapp/` — design source for surface levels, colour tokens, typography
* `Source/UI/DrawnCurveLookAndFeel.h` — the natural home for the unified
  chrome rules from § 3 P1–P4 + theme token consolidation (T2.9)
* `mockups/` — destination for sketches needed by Tier 3 items
  (steppers redesign T3.1; preset menu shape § 6.1; MIDI-input canvas
  overlay § 6.2)
