#pragma once

/**
 * @file SegmentedControl.h
 *
 * A single unified control that presents N segments as a horizontal strip.
 * One segment is "active" at a time, highlighted with the accent colour.
 * Clicking any segment selects it and fires onChange(index).
 *
 * ── Why this exists ─────────────────────────────────────────────────────────
 * JUCE TextButton radio groups require N separate components with manual
 * colour management.  A SegmentedControl reads as *one* control, matches
 * the iOS segmented-control idiom, and manages its own visual state.
 *
 * ── Font / symbol support ────────────────────────────────────────────────────
 * By default, SegmentedControl draws Segment::label as centred text using
 * the current JUCE default font.
 *
 * To use a symbol font (e.g. Material Symbols):
 *   1. Bundle the TTF via juce_add_binary_data in CMakeLists.txt
 *   2. Pass a SegmentPainter lambda that calls g.setFont(symbolFont) and
 *      draws the symbol via its ligature name ("arrow_forward") or codepoint
 *   The setSegmentPainter() override replaces the default text renderer.
 *
 * The "proper non-JUCE way" on iOS is:
 *   - Register the font in Info.plist (UIAppFonts)
 *   - Use UIFont(name:size:) and set the character on a UILabel
 * The JUCE equivalent is identical in concept:
 *   - Register the font via juce_add_binary_data (compiled in as a char array)
 *   - Use juce::Typeface::createSystemTypefaceFor(data, size) → juce::Font
 *   - Set g.setFont(font) and draw the character as a juce::String
 * Both load font data without filesystem access — safe inside the AUv3 sandbox.
 */

#include <juce_gui_basics/juce_gui_basics.h>
#include <vector>
#include <functional>

//==============================================================================
/**
 * Shared visual metrics for all "pill-shaped" controls (segmented controls,
 * family tabs, subfamily chips).  T2.1 (visual-audit-2026-04 §3 P4 / §4):
 * unify chip / tab / segmented-control chrome so the user sees one consistent
 * pill family across the editor instead of four related-but-different
 * outlined-pill treatments.
 *
 * Anything that draws a pill shape — `SegmentedControl::paint`,
 * `PluginEditor`'s `SymbolLF::drawButtonBackground`,
 * `SubfamilyLF::drawButtonBackground` — should reference these constants
 * so visual changes propagate through one place.
 */
namespace dcui::ChipChrome
{
    /** Corner-radius as a fraction of the control's height.  0.28 reads as
     *  "iOS-segmented-control round" without going full circular. */
    inline constexpr float kRadiusFactor    = 0.28f;

    /** Inset (in pixels) used when drawing the active-segment highlight inside
     *  a SegmentedControl, so the outer ring stays visible. */
    inline constexpr float kActiveInset     = 2.0f;

    /** Vertical inset (in pixels) for the divider lines between segments. */
    inline constexpr float kDividerInset    = 5.0f;

    /** Default text height as a fraction of the control's height. */
    inline constexpr float kFontHeightFactor = 0.42f;

    /** Hairline outline width. */
    inline constexpr float kOutlineWidth    = 1.0f;
}

//==============================================================================
class SegmentedControl : public juce::Component
{
public:
    // ── Segment descriptor ────────────────────────────────────────────────────
    struct Segment
    {
        juce::String id;       ///< Unique identifier (e.g. "fwd", "rev", "pp")
        juce::String label;    ///< Display text (or symbol codepoint / ligature name)
        juce::String tooltip;  ///< Full description for accessibility / help overlay
    };

    /**
     * Custom per-segment drawing callback.  When set, called instead of the
     * default text renderer.
     *
     * Parameters:
     *   g       — Graphics context (colour already set to labelColour or activeLabel)
     *   bounds  — Pixel bounds of this segment (full height of the control)
     *   index   — Segment index (0-based)
     *   active  — true if this segment is selected
     *
     * The callback is responsible for all drawing within `bounds`.
     * The background (bgColour / activeColour highlight) has already been drawn.
     */
    using SegmentPainter = std::function<void(juce::Graphics& g,
                                              juce::Rectangle<float> bounds,
                                              int index,
                                              bool active)>;

    SegmentedControl() = default;

    // ── API ───────────────────────────────────────────────────────────────────

    void setSegments (std::vector<Segment> segs)
    {
        _segments = std::move (segs);
        _selected = juce::jlimit (0, (int) _segments.size() - 1, _selected);
        repaint();
    }

    int getSelectedIndex() const noexcept { return _selected; }

    /**
     * Select a segment.
     * @param i            Zero-based index.
     * @param notification sendNotification (default) fires onChange; dontSendNotification
     *                     does not (use when restoring state from an external source).
     */
    void setSelectedIndex (int i,
                           juce::NotificationType notification = juce::sendNotification)
    {
        const int clamped = (_segments.empty())
                            ? 0
                            : juce::jlimit (0, (int) _segments.size() - 1, i);

        const bool changed = (clamped != _selected);
        _selected = clamped;

        if (changed) repaint();

        if (notification == juce::sendNotification && onChange)
            onChange (_selected);
    }

    /**
     * Override default text drawing.
     * Set to nullptr to restore the default text renderer.
     */
    void setSegmentPainter (SegmentPainter painter)
    {
        _painter = std::move (painter);
        repaint();
    }

    // ── Colours (set from DrawnCurveEditor::applyTheme) ──────────────────────
    juce::Colour bgColour     { 0xff333355 };   ///< Unselected segment background
    juce::Colour activeColour { 0xff2979ff };   ///< Selected segment highlight
    juce::Colour labelColour  { 0x99ffffff };   ///< Unselected label text / icon colour
    juce::Colour activeLabel  { 0xffffffff };   ///< Selected label text / icon colour
    juce::Colour borderColour { 0x33ffffff };   ///< Dividers + outer ring

    // ── Callbacks ─────────────────────────────────────────────────────────────

    /// Fired when the selected segment *changes* (not when the same segment is re-tapped).
    std::function<void (int)> onChange;

    /// Fired on *every* tap, even when the same segment is re-tapped.
    /// @param index       The tapped segment index.
    /// @param wasAlready  true if this segment was already the selected one.
    std::function<void (int index, bool wasAlready)> onTap;

    //==========================================================================
    void paint (juce::Graphics& g) override
    {
        const int n = (int) _segments.size();
        if (n == 0) return;

        const auto  b    = getLocalBounds().toFloat();
        // T2.1: pill chrome shared with family tabs + subfamily chips —
        // see dcui::ChipChrome (SegmentedControl.h).
        const float r    = b.getHeight() * dcui::ChipChrome::kRadiusFactor;

        // ── Background ────────────────────────────────────────────────────────
        g.setColour (bgColour);
        g.fillRoundedRectangle (b, r);

        // ── Per-segment ───────────────────────────────────────────────────────
        const float segW = b.getWidth() / (float) n;

        for (int i = 0; i < n; ++i)
        {
            const auto sb     = juce::Rectangle<float> (b.getX() + segW * i, b.getY(),
                                                        segW, b.getHeight());
            const bool active = (i == _selected);

            // Active highlight — slightly inset so the rounded outer ring shows.
            if (active)
            {
                const float in = dcui::ChipChrome::kActiveInset;
                g.setColour (activeColour);
                g.fillRoundedRectangle (sb.reduced (in, in), r * 0.65f);
            }

            // Divider line between segments.
            if (i > 0)
            {
                g.setColour (borderColour);
                g.drawVerticalLine (juce::roundToInt (sb.getX()),
                                    b.getY() + dcui::ChipChrome::kDividerInset,
                                    b.getBottom() - dcui::ChipChrome::kDividerInset);
            }

            // Content: custom painter or default text.
            if (_painter)
            {
                g.setColour (active ? activeLabel : labelColour);
                _painter (g, sb, i, active);
            }
            else
            {
                g.setColour (active ? activeLabel : labelColour);
                g.setFont (juce::Font (juce::FontOptions{}
                               .withHeight (b.getHeight() * dcui::ChipChrome::kFontHeightFactor)));
                g.drawText (_segments[static_cast<size_t>(i)].label, sb.toNearestInt(),
                            juce::Justification::centred, false);
            }
        }

        // ── Outer border ──────────────────────────────────────────────────────
        g.setColour (borderColour);
        g.drawRoundedRectangle (b.reduced (0.5f), r, dcui::ChipChrome::kOutlineWidth);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const int n = (int) _segments.size();
        if (n == 0) return;

        const int  idx       = juce::jlimit (0, n - 1,
                                             (int) ((float) e.x / (float) getWidth() * n));
        const bool wasAlready = (idx == _selected);
        setSelectedIndex (idx);   // fires onChange only if changed
        if (onTap) onTap (idx, wasAlready);
    }

private:
    std::vector<Segment> _segments;
    int                  _selected { 0 };
    SegmentPainter       _painter;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SegmentedControl)
};
