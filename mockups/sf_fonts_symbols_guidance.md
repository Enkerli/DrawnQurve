# DrawnCurveJUCE — Guidance on SF Fonts and SF Symbols for an iPad-First UI

## Purpose

This document gives practical guidance for reducing text in the DrawnCurveJUCE interface by leaning on Apple’s San Francisco (SF) font family and SF Symbols, while staying realistic about what JUCE can and cannot do directly.

The goals are:

- improve legibility on iPad
- reduce reliance on English text labels
- align symbols and text visually
- keep the interface performant and coherent
- avoid introducing brittle platform-specific hacks unless they add clear value

---

## 1. Key recommendation

### Use SF Pro as the main UI text font.

For an iPad-only plugin, this is the right default direction. SF Pro is the system font on iOS and iPadOS, and Apple explicitly positions SF Symbols as designed to integrate seamlessly with San Francisco. SF Pro includes multiple weights, widths, and variable optical sizes; SF Symbols aligns automatically with San Francisco’s metrics and scales. citeturn673914search1turn673914search4turn673914search7

### Use SF Symbols selectively, not dogmatically.

SF Symbols is excellent for common actions, states, directions, and structural concepts, but it is not a complete replacement for your custom moon/lattice icon language. Apple’s guidance emphasizes that symbols work best when they communicate familiar actions or concepts clearly and consistently, and the framework supports variants such as outline, fill, slash, and enclosed styles specifically to express state. citeturn673914search0turn673914search2

### Do not assume JUCE can use SF Symbols as “just another font.”

This is the most important implementation caveat. In Apple’s frameworks, SF Symbols is primarily exposed as system symbol images configured through UIKit and SwiftUI APIs like `UIImage(systemName:)` and `UIImage.SymbolConfiguration`, not as a text font that JUCE can reliably render by typing symbol names into strings. citeturn673914search5turn673914search16turn673914search21

So the pragmatic split is:

- **SF Pro for text**
- **SF Symbols via native image bridging, exported vectors, or JUCE drawables**
- **custom JUCE vector icons for domain-specific concepts**

---

## 2. What to use where

## 2.1 Text: use SF Pro

Recommended roles:

- control values
- short labels that still need words
- bitmask / decimal display
- micro-help / tooltips
- section titles if any remain

For iPad-first UI, SF Pro is the correct family to standardize on. Apple’s typography guidance identifies San Francisco as the primary sans-serif system typeface family across Apple platforms, and the Apple fonts page lists SF Pro as the system font for iOS and iPadOS. citeturn673914search7turn673914search10turn673914search12

### Suggested font roles

- **UI label**: SF Pro Text / regular or medium
- **Value/emphasis**: SF Pro Display / semibold if larger, or SF Pro Text / semibold if compact
- **Bitmask / numeric debug**: SF Mono only if alignment matters enough to justify a second family

Recommendation: keep SF Mono optional. It is useful for aligned notation, but the interface should not start to feel like a debugger.

---

## 2.2 Symbols: use three distinct buckets

### Bucket A — Native/common concepts → SF Symbols

Good candidates:

- play / pause / direction adjuncts
- loop / repeat / cycle
- link / lock / sync-adjacent states
- sliders / tuning / settings-like affordances if needed
- speaker-independent mute/visibility variants where appropriate
- disclosure / hierarchy / list / recent / bookmark / save-like states

These are exactly the kinds of semantically common concepts SF Symbols handles well. Apple highlights symbol variants, rendering modes, and state communication as key strengths. citeturn673914search0turn673914search2turn673914search20

### Bucket B — Music/plugin-domain concepts → custom icons

Keep custom or JUCE-drawn:

- moon-like scale operations
- pitch-class / bitmask transformations
- custom lane-routing metaphors
- note-mode semantics specific to your plugin
- scale-family symbols if they become part of a distinctive internal language

These are too domain-specific to force through SF Symbols. This is where your own icon language adds identity.

### Bucket C — Text-like symbols → Unicode glyphs in SF Pro

Good uses:

- ∞
- ±
- •
- arrows if they render well enough in your chosen font/context
- ♯ / ♭ if musically needed and legible at your target size

These can remain text glyphs as long as they render cleanly in SF Pro and do not need the richer visual behavior of a true symbol image.

---

## 3. Practical JUCE strategy

## 3.1 Adopt SF Pro centrally through LookAndFeel

JUCE lets you set a default sans-serif typeface or typeface name globally via `LookAndFeel`, and also override `getTypefaceForFont()` for more control. `LookAndFeel` additionally provides `withDefaultMetrics()` so widgets can create fonts that match the current metrics model. citeturn308408search0turn308408search3

### Recommendation

Create a plugin-specific look and feel and set SF Pro as the default UI font.

Use three roles only:

- small utility
- control label
- value/emphasis

That is enough for this interface.

---

## 3.2 Do not try to drive SF Symbols purely through JUCE text drawing

Even though some visible shapes may overlap with Unicode or private glyphs, the reliable Apple-supported path for SF Symbols is through image APIs like `UIImage(systemName:)` with symbol configuration, not generic font text rendering. citeturn673914search5turn673914search16turn673914search21

That means there are three workable implementation options.

### Option 1 — Best long-term consistency: recreate key symbols as JUCE vectors

Pros:

- fully controllable in JUCE
- easy recoloring
- easy state variants
- no UIKit bridge needed
- visually consistent with your custom icon system

Cons:

- manual work
- not using Apple’s exact symbol drawings

This is the safest option for controls you already plan to stylize.

### Option 2 — iPad-specific/native path: bridge selected SF Symbols from UIKit

Use UIKit to obtain symbol images by system name and configuration, then convert/render them in the JUCE layer.

Pros:

- access to true SF Symbols
- can use weights/scales/rendering modes

Cons:

- Objective-C++ / UIKit bridge work
- more platform-specific code
- less elegant if many icons need dynamic recoloring or vector manipulation

This makes sense only if you strongly want authentic Apple symbols for a subset of controls.

### Option 3 — Export chosen SF Symbols as vectors and treat them as assets

Apple’s SF Symbols app supports export and custom-symbol workflows. Apple’s documentation also describes creating custom symbol images and working from the design grid/guides. JUCE can load SVGs as `Drawable`s, or parse SVG into drawable trees. citeturn673914search3turn308408search2

Pros:

- no runtime UIKit bridge
- still close to Apple’s symbol shapes
- can mix with JUCE assets

Cons:

- asset pipeline overhead
- less automatic adaptation than native symbol APIs

For this plugin, this is often the sweet spot.

---

## 4. Recommended typography baseline

## Primary text family

- **SF Pro**

## Weights

- Regular for low-emphasis labels
- Medium for actionable labels
- Semibold for values and selected states

## What not to do

- too many weight changes
- ultra-light weights on small controls
- mixing too many type families
- all-caps everywhere

Apple’s SF family includes optical sizes and multiple widths/weights, which is a good reason to keep the system tight rather than introducing unrelated fonts. citeturn673914search1turn673914search7turn673914search15

---

## 5. Recommended SF Symbols usage by current UI concept

This section is advisory, not absolute. Final selection should be tested in context.

## 5.1 Strong candidates for SF Symbols

### Sync / Free

Use SF Symbols only for **Sync** or for the mode control container; keep the two-state logic visually explicit.

Possible directions:

- `link`- or `lock`-adjacent semantics for sync
- use text/value treatment or custom icon for the paired mode distinction

Reason: “Free” is not a stable universal symbol concept. “Sync” is closer to existing symbol language; “Free” may still need a word, alternate icon, or segmented-state treatment.

### Smooth

Candidate symbol family:

- waveform / slider / easing-adjacent visual

This may still be better as a custom icon if it means your plugin’s interpolation behavior rather than generic smoothing.

### Phase

Candidate symbol family:

- rotate / cycle / clock-like symbols

This is a good place where an SF Symbol may work if the concept is closer to phase offset than to a music-theory meaning.

### Recent / Saved / Recall scale sets

Excellent use of SF Symbols:

- clock/history-like
- bookmark/star/folder-like if you add saved states

### Disclosure / expand / collapse for scale families

Excellent use of SF Symbols.

---

## 5.2 Better kept custom

### N / CC / Aft / PB

These are not good candidates for replacement by generic SF Symbols alone.

- **N** (notes) may be represented by a custom pitch icon
- **CC** really wants a control/modulation identity, not a generic slider icon alone
- **Aft** is obscure enough that an icon-only treatment is risky unless it becomes a learned internal symbol
- **PB** could use a bend/arc metaphor, but likely works best in your own icon set

Recommendation: reduce some text, but do not force full symbol-only representation for all of these. A compact hybrid can work:

- icon + 1–2 letter code
- icon in normal state, expanded text in teaching/help mode

### Det
n
If “Det” means detail, depth, or some plugin-specific routing detail, avoid generic SF Symbols unless the concept is redefined more clearly in the UI.

### Teach

Keep custom. The intended meaning is not “listen” or “record,” but “assist MIDI Learn in another plugin.” That is too specific for stock Apple symbols.

### Scale families / transformations

Keep custom. This is where your moon/circle language shines.

---

## 5.3 Hybrid candidates

These may work best as SF Symbol + minimal text or SF Symbol + value.

- Sync
- Phase
- Ch
- Vel
- family browsing affordances
- recent/saved scale slots

Hybrid is often the best way to reduce text without overloading the user with unfamiliar pictograms.

---

## 6. Rendering modes to prefer if you bridge or export SF Symbols

Apple provides multiple rendering approaches for SF Symbols, including monochrome, hierarchical, palette, and multicolor. Hierarchical rendering uses a single hue with opacity differences to create depth, while palette and multicolor modes support more layered color treatment. Variable Color is also supported by many symbols. citeturn673914search6turn673914search9turn673914search14turn673914search20

### Recommendation for this plugin

Default to:

- **monochrome** or **hierarchical**

Avoid by default:

- multicolor except for rare teaching/onboarding moments
- palette mode unless it clearly supports lane-color semantics

Reason: your UI already wants a coherent lane-color system and a restrained visual language.

---

## 7. Scale-selection specific advice

For the scale-family refactor, use:

- **custom symbols** for family identity and transformations
- **SF Pro text** for occasional family/subfamily labels
- **SF Symbols** only for structural navigation actions like expand/collapse, recent/history, save/bookmark, and possibly rotate if you decide not to draw your own

This keeps the scale system distinctive without making the whole interface feel handmade in inconsistent ways.

---

## 8. Unicode guidance

If you already converted some labels to Unicode glyphs, keep doing that selectively.

Good candidates:

- ∞ for infinite/loop-like semantics
- ± for bipolar or range-related semantics
- ♯ / ♭ if scale/root UI needs them

But test them at real control sizes. The question is not only “does the glyph exist,” but whether it is readable at your actual button height in SF Pro.

If a glyph is too small or too typographic, promote it to a symbol/icon instead.

---

## 9. Concrete recommendations for this project

## Do now

1. Standardize all text on **SF Pro** through the plugin look and feel. JUCE supports setting the default sans-serif typeface name globally and resolving fonts through `LookAndFeel`. citeturn308408search0turn308408search3
2. Reduce control labels to the minimum needed for learnability.
3. Keep your moon/circle icon language for scale operations and other domain-specific controls.
4. Audit existing text labels into three categories:
   - keep as text
   - convert to Unicode glyph
   - replace with icon/SF Symbol
5. Use SF Symbols only where the concept is already widely standardized.

## Do next

1. Build a small symbol map for the current UI.
2. Decide which symbols will be:
   - JUCE vector icons
   - exported SF Symbol assets
   - Unicode glyphs in SF Pro
3. Prototype one screen using mostly symbols and minimal text.
4. Test on iPad at real scale, not just desktop preview.

## Avoid

1. assuming every short text label should become an icon
2. mixing Apple stock symbols and custom geometry without a common stroke/weight logic
3. using SF Symbols for specialized music/plugin semantics that users must learn anyway
4. depending on undocumented/private glyph tricks instead of supported symbol/image workflows

---

## 10. Suggested mapping pass for current labels

This is a starting recommendation only.

| Current label | Suggested direction |
|---|---|
| Sync | SF Symbol or hybrid |
| Free | keep text or pair with custom mode state |
| Smooth | likely custom or hybrid |
| Phase | hybrid or SF Symbol if generic enough |
| N | custom/hybrid |
| CC | custom/hybrid |
| Aft | custom/hybrid |
| PB | custom/hybrid |
| Det | likely text until semantics sharpen |
| Vel | hybrid |
| Ch | hybrid |
| Teach | custom |
| scale names | move toward family/subfamily browser |

---

## 11. Best implementation model for DrawnCurveJUCE

Given the current state of the project, the best model is probably:

- **SF Pro everywhere for text**
- **custom JUCE vectors for core plugin semantics**
- **selective SF Symbol use for common navigation/state icons**
- **Unicode glyphs only where they remain crisp and obvious**

That gives you the visual benefits of Apple’s system language without forcing JUCE into awkward symbol-font behavior.

---

## 12. References

- Apple Human Interface Guidelines: SF Symbols. SF Symbols are designed to integrate with San Francisco and support configurable variants and rendering. citeturn673914search0
- Apple SF Symbols page. SF Symbols 7 includes over 6,900 symbols with nine weights and three scales, designed to align with San Francisco. citeturn673914search4
- Apple Fonts. SF Pro is the system font for iOS, iPadOS, macOS, and tvOS; SF family includes multiple variants and script extensions. citeturn673914search1
- Apple Typography HIG. San Francisco is Apple’s primary sans-serif UI typeface family. citeturn673914search7
- UIKit symbol image configuration docs. System symbols are retrieved/configured through image APIs like `UIImage(systemName:)` and `UIImage.SymbolConfiguration`. citeturn673914search5turn673914search16turn673914search21
- Apple docs on creating custom symbol images. Useful if you export or adapt symbols rather than bridging them live. citeturn673914search3
- JUCE `LookAndFeel` and `LookAndFeel_V4` docs. Support global typeface choice, font resolution, and metrics defaults. citeturn308408search0turn308408search3
- JUCE `Drawable` docs. SVG assets can be loaded into JUCE as drawables, which is useful for exported symbol assets. citeturn308408search2
