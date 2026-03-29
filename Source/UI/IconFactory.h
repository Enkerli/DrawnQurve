#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace dcui
{

enum class IconType
{
    noteMode,
    clearGesture,
    speedFree,
    lengthSync,
    sync,
    teach,
    mute,
    target,
    range,
    gridX,
    gridY,
    lock,
    lockOpen,
    directionLeft,
    directionRight,
    directionPingPong,
    pauseOverlay
};

enum class IconVisualState
{
    normal,
    hover,
    active,
    disabled
};

struct IconStyle
{
    float stroke = 1.75f;
    float dotRadius = 1.6f;
    float cornerRadius = 4.0f;
};

class IconFactory
{
public:
    static juce::Path createIcon (IconType type, juce::Rectangle<float> bounds, const IconStyle& style = {})
    {
        using Rect = juce::Rectangle<float>;
        auto b = bounds.reduced (bounds.getWidth() * 0.08f, bounds.getHeight() * 0.08f);

        switch (type)
        {
            case IconType::noteMode:          return makeNoteMode (b, style);
            case IconType::clearGesture:      return makeClearGesture (b, style);
            case IconType::speedFree:         return makeSpeedFree (b, style);
            case IconType::lengthSync:        return makeLengthSync (b, style);
            case IconType::sync:              return makeSync (b, style);
            case IconType::teach:             return makeTeach (b, style);
            case IconType::mute:              return makeMute (b, style);
            case IconType::target:            return makeTarget (b, style);
            case IconType::range:             return makeRange (b, style);
            case IconType::gridX:             return makeGridX (b, style);
            case IconType::gridY:             return makeGridY (b, style);
            case IconType::lock:              return makeLock (b, style);
            case IconType::lockOpen:          return makeLockOpen (b, style);
            case IconType::directionLeft:     return makeDirectionLeft (b, style);
            case IconType::directionRight:    return makeDirectionRight (b, style);
            case IconType::directionPingPong: return makeDirectionPingPong (b, style);
            case IconType::pauseOverlay:      return makePauseOverlay (b, style);
        }

        return {};
    }

    static void drawIcon (juce::Graphics& g,
                          IconType type,
                          juce::Rectangle<float> bounds,
                          juce::Colour colour,
                          const IconStyle& style = {})
    {
        auto p = createIcon (type, bounds, style);
        g.setColour (colour);
        g.strokePath (p, juce::PathStrokeType (style.stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    static juce::Colour colourForState (juce::Colour base, IconVisualState state)
    {
        switch (state)
        {
            case IconVisualState::normal:   return base.withAlpha (0.92f);
            case IconVisualState::hover:    return base.brighter (0.18f).withAlpha (1.0f);
            case IconVisualState::active:   return base.brighter (0.28f).withAlpha (1.0f);
            case IconVisualState::disabled: return base.withAlpha (0.28f);
        }

        return base;
    }

private:
    static juce::Point<float> lerp (juce::Point<float> a, juce::Point<float> b, float t)
    {
        return { a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t };
    }

    static void addDot (juce::Path& p, juce::Point<float> c, float r)
    {
        p.addEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
    }

    static void addOpenCircle (juce::Path& p, juce::Rectangle<float> r)
    {
        p.addEllipse (r);
    }

    static void addLine (juce::Path& p, juce::Point<float> a, juce::Point<float> b)
    {
        p.startNewSubPath (a);
        p.lineTo (b);
    }

    static void addArrowHead (juce::Path& p, juce::Point<float> tip, juce::Point<float> dir, float size)
    {
        auto len = std::sqrt (dir.x * dir.x + dir.y * dir.y);
        if (len <= 0.0001f)
            return;

        juce::Point<float> n { dir.x / len, dir.y / len };
        juce::Point<float> perp { -n.y, n.x };

        auto a = tip - n * size + perp * (size * 0.55f);
        auto b = tip - n * size - perp * (size * 0.55f);

        addLine (p, tip, a);
        addLine (p, tip, b);
    }

    static juce::Path makeNoteMode (juce::Rectangle<float> b, const IconStyle& s)
    {
        juce::Path p;

        auto size = std::min (b.getWidth(), b.getHeight());
        auto circle = juce::Rectangle<float> (size * 0.64f, size * 0.64f).withCentre (b.getCentre());
        addOpenCircle (p, circle);

        auto c = b.getCentre();
        auto d = size * 0.12f;
        addDot (p, { c.x - d, c.y + d * 0.8f }, s.dotRadius);
        addDot (p, { c.x,     c.y },           s.dotRadius);
        addDot (p, { c.x + d, c.y - d * 0.8f }, s.dotRadius);

        juce::Path arc;
        arc.startNewSubPath (c.x - d * 1.08f, c.y + d * 0.52f);
        arc.quadraticTo (c.x, c.y - d * 0.25f, c.x + d * 1.08f, c.y - d * 0.52f);
        p.addPath (arc);

        return p;
    }

    static juce::Path makeClearGesture (juce::Rectangle<float> b, const IconStyle& s)
    {
        juce::Path p;
        auto size = std::min (b.getWidth(), b.getHeight());
        auto c = b.getCentre();

        auto circle = juce::Rectangle<float> (size * 0.66f, size * 0.66f).withCentre (c);
        addOpenCircle (p, circle);

        juce::Path sweep;
        sweep.startNewSubPath (c.x - size * 0.18f, c.y + size * 0.20f);
        sweep.quadraticTo (c.x - size * 0.02f, c.y, c.x + size * 0.19f, c.y - size * 0.18f);
        p.addPath (sweep);

        addDot (p, { c.x - size * 0.24f, c.y + size * 0.27f }, s.dotRadius * 0.78f);
        addDot (p, { c.x - size * 0.31f, c.y + size * 0.34f }, s.dotRadius * 0.62f);
        addDot (p, { c.x - size * 0.37f, c.y + size * 0.40f }, s.dotRadius * 0.46f);

        return p;
    }

    static juce::Path makeSpeedFree (juce::Rectangle<float> b, const IconStyle& s)
    {
        juce::Path p;
        auto size = std::min (b.getWidth(), b.getHeight());
        auto c = b.getCentre();

        juce::Path arc;
        auto r = size * 0.28f;
        auto arcRect = juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c);
        arc.addCentredArc (c.x, c.y, r, r, 0.0f, juce::MathConstants<float>::pi * 0.20f,
                           juce::MathConstants<float>::pi * 1.72f, true);
        p.addPath (arc);

        addDot (p, { c.x + r * 0.72f, c.y - r * 0.38f }, s.dotRadius);

        return p;
    }

    static juce::Path makeLengthSync (juce::Rectangle<float> b, const IconStyle& s)
    {
        juce::Path p;
        auto size = std::min (b.getWidth(), b.getHeight());
        auto c = b.getCentre();
        auto r = size * 0.28f;

        juce::Path ring;
        ring.addCentredArc (c.x, c.y, r, r, 0.0f, 0.0f, juce::MathConstants<float>::twoPi, true);
        p.addPath (ring);

        for (int i = 0; i < 8; ++i)
        {
            auto a = juce::MathConstants<float>::twoPi * (float) i / 8.0f - juce::MathConstants<float>::halfPi;
            juce::Point<float> inner { c.x + std::cos (a) * (r * 0.78f), c.y + std::sin (a) * (r * 0.78f) };
            juce::Point<float> outer { c.x + std::cos (a) * (r * 1.08f), c.y + std::sin (a) * (r * 1.08f) };
            addLine (p, inner, outer);
        }

        return p;
    }

    static juce::Path makeSync (juce::Rectangle<float> b, const IconStyle& s)
    {
        juce::Path p;
        auto w = b.getWidth();
        auto h = b.getHeight();

        auto x1 = b.getX() + w * 0.38f;
        auto x2 = b.getX() + w * 0.62f;
        auto y0 = b.getY() + h * 0.24f;
        auto y1 = b.getBottom() - h * 0.24f;

        addLine (p, { x1, y0 }, { x1, y1 });
        addLine (p, { x2, y0 }, { x2, y1 });

        addDot (p, { x1, b.getCentreY() - h * 0.10f }, s.dotRadius);
        addDot (p, { x2, b.getCentreY() + h * 0.10f }, s.dotRadius);

        return p;
    }

    static juce::Path makeTeach (juce::Rectangle<float> b, const IconStyle& s)
    {
        // Corrected semantics:
        // assist outward mapping to another plugin's MIDI Learn,
        // not listening/capturing incoming signal.
        juce::Path p;
        auto w = b.getWidth();
        auto h = b.getHeight();

        auto source = juce::Point<float> { b.getX() + w * 0.32f, b.getY() + h * 0.52f };
        auto tip    = juce::Point<float> { b.getX() + w * 0.61f, b.getY() + h * 0.42f };
        auto targetCentre = juce::Point<float> { b.getX() + w * 0.74f, b.getY() + h * 0.34f };

        addDot (p, source, s.dotRadius);

        juce::Path stem;
        stem.startNewSubPath (source.x + w * 0.05f, source.y - h * 0.03f);
        stem.quadraticTo (b.getX() + w * 0.48f, b.getY() + h * 0.50f, tip.x, tip.y);
        p.addPath (stem);

        addArrowHead (p, tip, { 1.0f, -0.3f }, std::min (w, h) * 0.10f);

        auto targetRect = juce::Rectangle<float> (w * 0.18f, h * 0.18f).withCentre (targetCentre);
        addOpenCircle (p, targetRect);

        return p;
    }

    static juce::Path makeMute (juce::Rectangle<float> b, const IconStyle& s)
    {
        juce::Path p;
        auto size = std::min (b.getWidth(), b.getHeight());
        auto c = b.getCentre();

        auto circle = juce::Rectangle<float> (size * 0.64f, size * 0.64f).withCentre (c);
        addOpenCircle (p, circle);

        auto y = c.y;
        addLine (p, { circle.getX() + size * 0.06f, y }, { circle.getRight() - size * 0.06f, y });

        return p;
    }

    static juce::Path makeTarget (juce::Rectangle<float> b, const IconStyle& s)
    {
        juce::Path p;
        auto w = b.getWidth();
        auto h = b.getHeight();

        auto src = juce::Point<float> { b.getX() + w * 0.28f, b.getCentreY() };
        auto junction = juce::Point<float> { b.getX() + w * 0.50f, b.getCentreY() };

        addDot (p, src, s.dotRadius);
        addLine (p, src + juce::Point<float> { w * 0.05f, 0.0f }, junction);

        auto t1 = juce::Point<float> { b.getX() + w * 0.73f, b.getY() + h * 0.30f };
        auto t2 = juce::Point<float> { b.getX() + w * 0.73f, b.getCentreY() };
        auto t3 = juce::Point<float> { b.getX() + w * 0.73f, b.getY() + h * 0.70f };

        addLine (p, junction, t1);
        addLine (p, junction, t2);
        addLine (p, junction, t3);

        addDot (p, t1, s.dotRadius * 0.92f);
        addDot (p, t2, s.dotRadius * 0.92f);
        addDot (p, t3, s.dotRadius * 0.92f);

        return p;
    }

    static juce::Path makeRange (juce::Rectangle<float> b, const IconStyle& s)
    {
        juce::Path p;
        auto w = b.getWidth();
        auto h = b.getHeight();
        auto x = b.getCentreX();
        auto y0 = b.getY() + h * 0.22f;
        auto y1 = b.getBottom() - h * 0.22f;
        auto cap = w * 0.16f;

        addLine (p, { x, y0 }, { x, y1 });
        addLine (p, { x - cap, y0 }, { x + cap, y0 });
        addLine (p, { x - cap, y1 }, { x + cap, y1 });

        addDot (p, { x, b.getCentreY() }, s.dotRadius * 0.9f);

        return p;
    }

    static juce::Path makeGridX (juce::Rectangle<float> b, const IconStyle& s)
    {
        juce::Path p;
        auto w = b.getWidth();
        auto h = b.getHeight();

        for (int i = 0; i < 3; ++i)
        {
            auto y = b.getY() + h * (0.30f + 0.20f * (float) i);
            addLine (p, { b.getX() + w * 0.20f, y }, { b.getRight() - w * 0.20f, y });
        }

        auto cx = b.getRight() - w * 0.22f;
        auto cy = b.getY() + h * 0.22f;
        auto r = std::min (w, h) * 0.06f;

        addLine (p, { cx - r, cy }, { cx + r, cy });
        addLine (p, { cx, cy - r }, { cx, cy + r });

        return p;
    }

    static juce::Path makeGridY (juce::Rectangle<float> b, const IconStyle& s)
    {
        juce::Path p;
        auto w = b.getWidth();
        auto h = b.getHeight();

        for (int i = 0; i < 3; ++i)
        {
            auto x = b.getX() + w * (0.30f + 0.20f * (float) i);
            addLine (p, { x, b.getY() + h * 0.20f }, { x, b.getBottom() - h * 0.20f });
        }

        auto cx = b.getRight() - w * 0.22f;
        auto cy = b.getY() + h * 0.22f;
        auto r = std::min (w, h) * 0.06f;

        addLine (p, { cx - r, cy }, { cx + r, cy });
        addLine (p, { cx, cy - r }, { cx, cy + r });

        return p;
    }

    static juce::Path makeLock (juce::Rectangle<float> b, const IconStyle& s)
    {
        juce::Path p;
        const auto w  = b.getWidth();
        const auto h  = b.getHeight();
        const auto cx = b.getCentreX();

        // Body: rounded rect in the lower 52% of the icon
        const float bodyL = b.getX() + w * 0.22f;
        const float bodyW = w * 0.56f;
        const float bodyT = b.getY() + h * 0.44f;
        const float bodyH = h * 0.48f;
        p.addRoundedRectangle (bodyL, bodyT, bodyW, bodyH, s.cornerRadius * 0.45f);

        // Keyhole dot
        addDot (p, { cx, bodyT + bodyH * 0.50f }, s.dotRadius * 0.9f);

        // Shackle (U-shape over the top)
        const float sr    = w * 0.18f;            // arc radius = leg half-gap
        const float arcCY = bodyT - sr * 0.85f;   // arc centre sits above body top

        // Left leg: down from arc start to body top
        p.startNewSubPath (cx - sr, bodyT + h * 0.02f);
        p.lineTo (cx - sr, arcCY);

        // Top arc: from left (pi) over the top to right (2*pi) in JUCE y-down coords
        juce::Path arc;
        arc.addCentredArc (cx, arcCY, sr, sr, 0.0f,
                           juce::MathConstants<float>::pi,
                           juce::MathConstants<float>::twoPi, true);
        p.addPath (arc);

        // Right leg: down from arc end to body top
        p.startNewSubPath (cx + sr, arcCY);
        p.lineTo (cx + sr, bodyT + h * 0.02f);

        return p;
    }

    static juce::Path makeLockOpen (juce::Rectangle<float> b, const IconStyle& s)
    {
        // Same as makeLock except the right leg of the shackle is raised —
        // it does not enter the body, showing the lock is unlatched.
        juce::Path p;
        const auto w  = b.getWidth();
        const auto h  = b.getHeight();
        const auto cx = b.getCentreX();

        // Body (same as closed lock)
        const float bodyL = b.getX() + w * 0.22f;
        const float bodyW = w * 0.56f;
        const float bodyT = b.getY() + h * 0.44f;
        const float bodyH = h * 0.48f;
        p.addRoundedRectangle (bodyL, bodyT, bodyW, bodyH, s.cornerRadius * 0.45f);

        // No keyhole dot when open — the lock is disengaged

        const float sr    = w * 0.18f;
        const float arcCY = bodyT - sr * 0.85f;

        // Left leg: still goes into body (pivot point)
        p.startNewSubPath (cx - sr, bodyT + h * 0.02f);
        p.lineTo (cx - sr, arcCY);

        // Top arc: same semicircle
        juce::Path arc;
        arc.addCentredArc (cx, arcCY, sr, sr, 0.0f,
                           juce::MathConstants<float>::pi,
                           juce::MathConstants<float>::twoPi, true);
        p.addPath (arc);

        // Right leg: terminates at arc height — the shackle is unlatched (raised)
        // Only a tiny stub so the open shape reads clearly
        p.startNewSubPath (cx + sr, arcCY);
        p.lineTo (cx + sr, arcCY - h * 0.10f);   // points upward, clearly not in body

        return p;
    }

    static juce::Path makeDirectionLeft (juce::Rectangle<float> b, const IconStyle&)
    {
        juce::Path p;
        auto c = b.getCentre();
        auto w = b.getWidth();
        auto h = b.getHeight();

        auto a = juce::Point<float> { c.x + w * 0.16f, c.y };
        auto m = juce::Point<float> { c.x - w * 0.16f, c.y };
        auto up = juce::Point<float> { c.x - w * 0.02f, c.y - h * 0.16f };
        auto dn = juce::Point<float> { c.x - w * 0.02f, c.y + h * 0.16f };

        addLine (p, a, m);
        addLine (p, m, up);
        addLine (p, m, dn);

        return p;
    }

    static juce::Path makeDirectionRight (juce::Rectangle<float> b, const IconStyle&)
    {
        juce::Path p;
        auto c = b.getCentre();
        auto w = b.getWidth();
        auto h = b.getHeight();

        auto a = juce::Point<float> { c.x - w * 0.16f, c.y };
        auto m = juce::Point<float> { c.x + w * 0.16f, c.y };
        auto up = juce::Point<float> { c.x + w * 0.02f, c.y - h * 0.16f };
        auto dn = juce::Point<float> { c.x + w * 0.02f, c.y + h * 0.16f };

        addLine (p, a, m);
        addLine (p, m, up);
        addLine (p, m, dn);

        return p;
    }

    static juce::Path makeDirectionPingPong (juce::Rectangle<float> b, const IconStyle&)
    {
        juce::Path p;
        auto c = b.getCentre();
        auto w = b.getWidth();
        auto h = b.getHeight();

        auto left = juce::Point<float>  { c.x - w * 0.18f, c.y };
        auto right = juce::Point<float> { c.x + w * 0.18f, c.y };

        addLine (p, left, right);

        addLine (p, left,  { left.x + w * 0.10f,  c.y - h * 0.12f });
        addLine (p, left,  { left.x + w * 0.10f,  c.y + h * 0.12f });

        addLine (p, right, { right.x - w * 0.10f, c.y - h * 0.12f });
        addLine (p, right, { right.x - w * 0.10f, c.y + h * 0.12f });

        return p;
    }

    static juce::Path makePauseOverlay (juce::Rectangle<float> b, const IconStyle&)
    {
        juce::Path p;
        auto w = b.getWidth();
        auto h = b.getHeight();
        auto bw = w * 0.08f;
        auto bh = h * 0.34f;
        auto gap = w * 0.06f;
        auto cx = b.getCentreX();
        auto cy = b.getCentreY();

        auto r1 = juce::Rectangle<float> (bw, bh).withCentre ({ cx - gap, cy });
        auto r2 = juce::Rectangle<float> (bw, bh).withCentre ({ cx + gap, cy });

        p.addRoundedRectangle (r1, bw * 0.2f);
        p.addRoundedRectangle (r2, bw * 0.2f);

        return p;
    }
};

class IconButton : public juce::Button
{
public:
    /// Default constructor — lets std::array<IconButton, N> compile.
    /// Call setIconType() and setBaseColour() before adding to parent.
    IconButton()
        : juce::Button (""),
          icon (IconType::mute),
          colour (juce::Colours::white)
    {}

    IconButton (const juce::String& name,
                IconType iconType,
                juce::Colour baseColour = juce::Colours::white)
        : juce::Button (name),
          icon (iconType),
          colour (baseColour)
    {
    }

    void setIconType (IconType t)            { icon = t; repaint(); }
    void setBaseColour (juce::Colour c)      { colour = c; repaint(); }

    /// When set, draws \p onIcon when toggled on and \p offIcon when toggled off.
    void setToggleIcons (IconType onIcon, IconType offIcon)
    {
        icon       = onIcon;
        _offIcon   = offIcon;
        _hasOff    = true;
        repaint();
    }
    void setShowPauseOverlay (bool shouldShow)
    {
        showPauseOverlay = shouldShow;
        repaint();
    }

    void setButtonShapeRadius (float r)
    {
        cornerRadius = r;
        repaint();
    }

    void paintButton (juce::Graphics& g, bool isHovered, bool isMouseDown) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);

        auto state = ! isEnabled() ? IconVisualState::disabled
                   : (getToggleState() || isMouseDown) ? IconVisualState::active
                   : isHovered ? IconVisualState::hover
                   : IconVisualState::normal;

        auto iconColour = IconFactory::colourForState (colour, state);

        auto bg = juce::Colours::black.withAlpha (state == IconVisualState::active ? 0.22f
                                  : state == IconVisualState::hover ? 0.14f
                                  : 0.08f);

        auto outline = colour.withAlpha (state == IconVisualState::active ? 0.40f : 0.18f);

        g.setColour (bg);
        g.fillRoundedRectangle (r, cornerRadius);

        g.setColour (outline);
        g.drawRoundedRectangle (r, cornerRadius, 1.0f);

        auto iconBounds = r.reduced (r.getWidth() * 0.18f, r.getHeight() * 0.18f);
        const auto iconToDraw = (_hasOff && ! getToggleState()) ? _offIcon : icon;
        IconFactory::drawIcon (g, iconToDraw, iconBounds, iconColour);

        if (showPauseOverlay)
        {
            auto overlay = IconFactory::createIcon (IconType::pauseOverlay, iconBounds);
            g.setColour (iconColour.withAlpha (0.95f));
            g.fillPath (overlay);
        }
    }

private:
    IconType icon;
    IconType _offIcon      = IconType::mute;   ///< Icon drawn when toggled off (if _hasOff)
    bool     _hasOff       = false;            ///< true → use _offIcon when not toggled
    juce::Colour colour;
    bool showPauseOverlay = false;
    float cornerRadius = 8.0f;
};

} // namespace dcui