# DrawnCurveJUCE — Scale Selection Refactor Recommendations

## Purpose

This document captures straightforward design recommendations for refactoring the **scale selection** area in DrawnCurveJUCE.

The goal is **not** to create a universal scale browser that satisfies every theoretical approach. The goal is to provide a design that:

- works well for an unsuspecting user,
- preserves direct musical usefulness,
- supports advanced pitch-class-set workflows,
- makes room for idiosyncratic and user-defined scales,
- fits the plugin’s broader identity as a drawable MIDI modulation / note-quantization instrument.

---

## 1. Design framing

The current problem is not really “how do we choose a scale?”

It is better framed as:

> how do we let the user navigate, edit, recall, and perform with pitch-class sets?

This matters because the current preset list mixes several different things into one row of big text buttons:

- **families** (`Major`, `Minor`, `Dorian` are modes of the same diatonic PCS),
- **shortcuts** (`Penta+`, `Penta-`),
- **genre-oriented defaults** (`Blues`),
- **fallback state** (`Custom`).

That mixture is hard to scan, hard to extend, and difficult to explain.

---

## 2. What should be kept

These parts are already strong and should remain central:

### 2.1 Pitch-class selection as circles

The piano-like layout of circles is idiomatic, readable, and musically useful.
It supports direct manipulation and feels more “instrumental” than a text list.

### 2.2 Moon-like operation buttons

The moon / phase-like buttons are simple, language-agnostic, and visually memorable.
They are especially well suited to operations like rotate, invert, complement, and related set transformations.

### 2.3 Bitmask display

The decimal bitmask is a strong power-user affordance.
It should remain available, but as a **secondary** layer rather than the primary selection interface.

### 2.4 Manual note toggling as source of truth

The editable PCS itself should remain the ground truth.
Presets, families, and transformations should all resolve to that editable set.

---

## 3. Primary recommendation

Replace the current preset-button row with a **family-based browser + transformation layer + recent/user recall layer**.

This gives three complementary ways to work:

1. **Direct editing** — toggle notes manually.
2. **Structural browsing** — choose from families and subfamilies.
3. **Transformational navigation** — rotate, invert, complement, etc.

---

## 4. Recommended information architecture

## 4.1 Layer A — Current PCS editor (always visible)

Always show:

- the 12 pitch-class circle layout,
- current root,
- current PCS bitmask / canonical form,
- current family/subfamily label when recognized.

This remains the stable center of the whole interaction.

---

## 4.2 Layer B — Families (top-level browser)

Use **families** as the top-level organizational unit.

Recommended families:

- **Diatonic**
- **Pentatonic**
- **Jazz Minor**
- **Harmonic Minor**
- **Symmetric**
- **Bebop**
- **Blues**
- **Chordal**
- **Recent**
- **Saved**

### Notes

- `Diatonic` should be **expanded by default**.
- `Blues` is not really a broad family, but it is common enough to warrant first-class access.
- `Chordal` is important because chord quantization and “glorified arp” use are core to the plugin’s direction.
- `Recent` and `Saved` are essential for idiosyncratic workflows.

---

## 4.3 Layer C — Subfamilies / variants

Within a selected family, show a second strip or expandable section for **subfamilies / modes / variants**.

### Example: Diatonic

Subfamily strip might show:

- Ionian
- Dorian
- Phrygian
- Lydian
- Mixolydian
- Aeolian
- Locrian

But the system should still understand these as **rotations of one PCS**.

### Example: Pentatonic

Subfamily strip might show:

- Major pentatonic
- Minor pentatonic
- related rotations

### Example: Symmetric

Subfamily strip might show:

- Whole-tone
- Octatonic WH
- Octatonic HW
- Augmented
- Diminished chord / symmetric subsets

### Example: Bebop

Subfamily strip might show:

- Dominant bebop
- Major bebop
- Minor bebop
- melodic-minor-related bebop variants

### Example: Chordal

Subfamily strip might show:

- major triad
- minor triad
- diminished triad
- augmented triad
- sus2 / sus4
- maj7 / min7 / dom7 / half-diminished / diminished 7
- extensions as needed

### Important semantic rule

The UI may display named modes and variants, but implementation should preserve the distinction between:

- **same PCS, different rotation**, and
- **different PCS**.

That distinction matters for both pedagogy and future harmonic features.

---

## 4.4 Layer D — Transformations

Provide a compact row of moon-like operation buttons, always close to the current PCS.

Recommended core operations:

- **Rotate**
- **Invert / Mirror**
- **Complement**
- **Normalize / Canonicalize**
- **Expand**
- **Contract**

These operations should update:

- the pitch-class circles,
- the mini preview,
- the bitmask / canonical form display.

### Why this is valuable

This is musically useful, visually teachable, and connects directly to set-theoretic and Euclidean-scale ideas.

---

## 4.5 Layer E — Recent / Saved / Performance slots

This layer is important enough that it should not be postponed indefinitely.

Recommended quick-access banks:

- **Recent**: last 6–8 used PCS states
- **Saved**: user-defined favorites
- **Slots**: optional performance recall buttons for live switching

This solves an important design problem:

> the user’s favourite scales may not belong to the standard shortlist.

Instead of trying to predict all desired scales, the UI should make custom recall easy.

---

## 5. Progressive disclosure

A key requirement is to reveal complexity progressively, without needing a dedicated preferences panel.

Recommended progressive flow:

### Default state

Show:

- pitch-class circles,
- current bitmask,
- family bar,
- expanded diatonic section,
- core operation buttons.

### When a family is changed

Show that family’s subfamily row.

### When a custom edit is made

Keep the family context visible if possible, but show state as:

- `Custom from Diatonic`
- `Custom from Jazz Minor`
- `Custom / Recent`

### When advanced use emerges

Reveal:

- recent list,
- saved scales,
- optional binary/decimal/canonical displays,
- copy/paste of bitmask if implemented later.

---

## 6. Layout recommendation

### 6.1 Recommended visible hierarchy

1. **Family bar**
2. **Expanded subfamily bar** for selected family
3. **Mini preview row**
4. **Transformation row**
5. **Pitch-class circle editor**
6. **Bitmask / canonical form / root display**

### 6.2 Why this order works

- It keeps common choices visible.
- It places direct visual browsing before abstract notation.
- It makes operations discoverable but not overwhelming.
- It preserves the circle editor as the main locus of action.

---

## 7. Mini previews

Mini previews are strongly recommended.

### 7.1 Preview forms

Use both of these where space permits:

- **single-row linear preview** (compact PCS strip), and/or
- **two-row staggered preview** matching the current pitch-class button arrangement.

The staggered preview is more idiomatic for many musicians.
The single-row preview is denser and works well in lists or recent-history views.

### 7.2 Preview behavior

A preview should communicate:

- note count,
- spacing / regularity,
- family resemblance,
- root emphasis if later implemented.

---

## 8. Family-specific recommendations

## 8.1 Diatonic

- Expanded by default
- Most likely entry point for many users
- Support ordering by mode name and optionally by “dark-to-light” brightness ordering

### Recommendation

Default to familiar mode naming order:

- Ionian, Dorian, Phrygian, Lydian, Mixolydian, Aeolian, Locrian

Optional secondary ordering could be offered later, but should not replace the standard order initially.

---

## 8.2 Pentatonic

- Should feel adjacent to Diatonic, not hidden
- Avoid opaque labels like `Penta+` / `Penta-` in the main UI

### Recommendation

Use explicit names or visual previews instead of terse abbreviations.

---

## 8.3 Jazz Minor and Harmonic Minor

These deserve first-class family status.

### Why

- They are widely used in jazz and related harmonic practice.
- Their modes are musically meaningful to advanced users.
- They extend beyond standard “major/minor mode picker” expectations.

### Recommendation

Expose them as families with subfamily rows, not as miscellaneous presets.

---

## 8.4 Bebop

Bebop scales are worth explicit support.

### Why

- Their 8-note structure is useful in binary rhythmic flow.
- They are pedagogically significant.
- They fit the plugin’s performative and line-shaping goals.

### Recommendation

Treat `Bebop` as a family or near-family section, even if its internal structure is less unified than Diatonic.

---

## 8.5 Symmetric

This family is especially important for this plugin.

### Why

- Symmetric sets quantize in highly predictable ways.
- They pair naturally with cyclic gesture systems.
- They connect well with Euclidean and set-theoretic thinking.

### Recommendation

Keep this family prominent, not buried in “more”.

---

## 8.6 Blues

Blues is a special case.

It is common, useful, and musically recognizable, but not really a large family in the same sense as Diatonic.

### Recommendation

Give it direct access in the family bar, but do not force it into an artificial family hierarchy.

---

## 8.7 Chordal

This is strategically important.

### Why

- Quantization to chord tones is immediately useful.
- It supports arp-like behavior.
- It aligns with future chord-progression / leadsheet features.

### Recommendation

Treat `Chordal` as a first-class family rather than as a later add-on.

---

## 9. Root, mode, and functional future

Modes of the same PCS produce the same raw quantization result unless additional functional weighting is introduced.

That means the current interface should distinguish between:

- **structure** (PCS membership), and
- **function** (root emphasis, degree weighting, chord-scale relation).

### Recommendation

For now:

- let family/subfamily selection set the PCS,
- let root selection remain explicit,
- avoid overclaiming functional differences that are not yet implemented.

### Future-facing note

If later versions emphasize root, chord tones, or degree weighting, the current family/subfamily structure will still support that evolution cleanly.

---

## 10. Canonical form / bitmask recommendation

If the implementation uses a canonical form comparable to the one discussed at AllTheScales, that is a good foundation.

### Recommendation

Show:

- current decimal value,
- optional canonical value,
- maybe copyable text later.

But keep these secondary.

The bitmask is valuable as:

- power-user shorthand,
- documentation,
- reproducibility,
- bridge to theory and transformation.

It should not become the main entry point for new users.

---

## 11. Recommended mockup behavior

The mockup should show:

- `Diatonic` expanded by default,
- adjacent family tabs visible (`Pentatonic`, `Jazz Minor`, `Symmetric`, `Recent`, etc.),
- transformation buttons as moon-like icons,
- mini previews for subfamily items,
- current pitch-class circles below,
- bitmask / canonical form beneath.

This communicates that the design supports:

- familiar scale picking,
- visual comparison,
- structural transformation,
- custom editing,
- future recall workflows.

---

## 12. Straightforward implementation recommendations

### Must do

- Replace large text preset buttons with a family/subfamily browser.
- Expand `Diatonic` by default.
- Keep the pitch-class circle editor central.
- Keep moon-like operation icons.
- Preserve bitmask display as a secondary layer.
- Add `Recent` and `Saved` access paths.

### Should do

- Add mini previews.
- Add `Jazz Minor`, `Harmonic Minor`, `Bebop`, `Symmetric`, and `Chordal` as explicit families.
- Support custom states such as `Custom from Diatonic`.

### Nice to have

- Optional mode ordering tricks (e.g. brightness ordering)
- explicit canonical form display
- future performance slots for instant switching

---

## 13. Why this approach is preferable

This approach avoids three common failures:

### Failure 1 — over-reliance on names

Scale names are culturally loaded, inconsistent, and often incomplete.

### Failure 2 — flat preset dumps

A long list of named scales is hard to scan and hard to extend.

### Failure 3 — theory without workflow

Purely theoretical organization often ignores the fact that musicians need quick access, recall, and performance switching.

This proposal balances:

- **familiarity**,
- **visuality**,
- **transformability**,
- **customization**.

---

## 14. References for documentation

These references are useful for documenting the design rationale, even where the UI is not directly derived from them.

1. **All The Scales — Introduction / Canonical Form context**  
   https://allthescales.org/intro.php

2. **Godfried Toussaint — The Euclidean Algorithm Generates Traditional Musical Rhythms**  
   https://cgm.cs.mcgill.ca/~godfried/publications/banff.pdf

3. **Demaine, Gómez-Martín, Meijer, Rappaport, Taslakian, Toussaint, Winograd — The Distance Geometry of Music**  
   https://erikdemaine.org/papers/DeepRhythms_CGTA/paper.pdf

4. **Open Music Theory — Set Class and Prime Form**  
   https://viva.pressbooks.pub/openmusictheory/chapter/set-class-and-prime-form/

5. **Puget Sound Music Theory — Lists of Set Classes**  
   https://musictheory.pugetsound.edu/mt21c/ListsOfSetClasses.html

6. **Piano With Jonny — Bebop scales overview**  
   https://pianowithjonny.com/piano-lessons/the-ultimate-guide-to-bebop-scales/

7. **FreeJazzLessons — Dominant Bebop Scale**  
   https://www.freejazzlessons.com/dominant-scale-lesson/

---

## 15. Closing summary

The main design move is simple:

> move from a flat preset list to a structured browser of pitch-class-set families, with direct editing and visible transformations.

That keeps the interface approachable while making it much more extensible, more personal, and more aligned with the plugin’s musical ambitions.
