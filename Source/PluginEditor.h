#pragma once

/**
 * @file PluginEditor.h
 *
 * DrawnCurveEditor — JUCE AudioProcessorEditor for DrawnCurve AUv3.
 *
 * UI Layout (640 x 560 px)
 * ────────────────────────
 *   Row 1  (40 px)  : Play · Clear | CC Aft PB Note | Sync · ? · Dark/Light
 *   Row 2  (34 px)  : [Fwd][Rev][P-P]               [Y-][Y+]  [X-][X+]
 *   Param 1 (44 px) : CC# / Vel  |  Channel  |  Smooth
 *   Param 2 (44 px) : Range (min/max out, 2/3 width)  |  Speed / Beats (1/3)
 *   Curve display   : remainder — live grid + recorded curve + playhead
 *
 * Key design decisions
 * ────────────────────
 * - Range slider uses JUCE TwoValueHorizontal with manual APVTS write-back
 *   because JUCE SliderAttachment does not support two-value sliders.
 * - Direction / message-type buttons use a custom SymbolLF (LookAndFeel_V4
 *   subclass) that draws path-based arrows, bypassing the font system.
 *   This is required because the AUv3 sandboxed XPC process on iOS cannot
 *   load non-default fonts; JUCE's built-in Bitstream Vera font covers only
 *   basic Latin and lacks arrow or music note glyphs.
 * - The Speed slider attachment is swapped at runtime between "playbackSpeed"
 *   and "syncBeats" when sync mode is toggled, so no extra slider is needed.
 * - The "?" button opens a full-editor help overlay (HelpOverlay) since
 *   JUCE tooltips do not function inside an AUv3 XPC process on iOS.
 */

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

// Colour palette struct — defined in PluginEditor.cpp.
// Forward-declared here so CurveDisplay can hold a pointer without
// pulling in the full definition.
struct Theme;

//==============================================================================
/**
 * Interactive display of the recorded curve, playhead, grid, and axis labels.
 *
 * Also handles touch / mouse gesture capture: mouseDown starts recording,
 * mouseDrag appends points, mouseUp finalises the snapshot.
 *
 * A 30 Hz timer drives continuous repaints so the playhead and axis labels
 * stay in sync with live parameter changes even when the user is not touching
 * the screen.
 */
class CurveDisplay : public juce::Component,
                     public juce::Timer
{
public:
    explicit CurveDisplay (DrawnCurveProcessor& p);
    ~CurveDisplay() override;

    void paint     (juce::Graphics&)          override;
    void resized   ()                         override;
    void mouseDown (const juce::MouseEvent&)  override;
    void mouseDrag (const juce::MouseEvent&)  override;
    void mouseUp   (const juce::MouseEvent&)  override;
    void timerCallback()                      override;

    /// Switch between dark (default) and light colour themes.
    void setLightMode (bool light);

    /// Number of vertical grid divisions (X axis).  Range 2-8, default 4.
    void setXDivisions (int n) { _xDivisions = juce::jlimit (2, 8, n); repaint(); }
    /// Number of horizontal grid divisions (Y axis).  Range 2-8, default 4.
    void setYDivisions (int n) { _yDivisions = juce::jlimit (2, 8, n); repaint(); }
    int  getXDivisions() const { return _xDivisions; }
    int  getYDivisions() const { return _yDivisions; }

private:
    DrawnCurveProcessor& proc;
    double     captureStartTime { 0.0   };   ///< High-res timestamp of mouseDown
    bool       isCapturing      { false };   ///< True while finger/mouse is down
    bool       _lightMode       { false };
    int        _xDivisions      { 4 };       ///< Vertical grid line count
    int        _yDivisions      { 4 };       ///< Horizontal grid line count
    juce::Path capturePath;                  ///< Live visual trail drawn in screen coordinates

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CurveDisplay)
};

//==============================================================================
/**
 * Full-editor semi-transparent help overlay.
 *
 * Shown when the user taps "?".  Draws a quick-reference guide over all
 * other controls.  Tapping anywhere dismisses it.
 *
 * JUCE tooltips do not work inside an AUv3 XPC sandbox process on iOS
 * (they require a TopLevelWindow), so this overlay is the chosen alternative.
 */
class HelpOverlay : public juce::Component
{
public:
    HelpOverlay();

    void paint     (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;   ///< Any tap dismisses the overlay

    /// true = dark background / light text (default), false = lighter tint
    void setLightMode (bool light) { _lightMode = light; repaint(); }

private:
    bool _lightMode { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HelpOverlay)
};

//==============================================================================
/**
 * Main plugin editor window.
 *
 * ComboBox popups don't work inside an AUv3 XPC process on iOS (no
 * TopLevelWindow), so every multi-option parameter uses TextButton radio
 * groups instead.
 */
class DrawnCurveEditor : public juce::AudioProcessorEditor,
                         private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit DrawnCurveEditor (DrawnCurveProcessor&);
    ~DrawnCurveEditor() override;

    void paint   (juce::Graphics&) override;
    void resized ()                override;

private:
    // ── Custom LookAndFeel for direction buttons ──────────────────────────────
    //
    // Draws filled-triangle arrows as juce::Path objects so no Unicode or
    // font-coverage dependency is introduced.  The button text string is used
    // as an identifier:
    //   "Fwd" → right-pointing triangle + "Fwd" label
    //   "Rev" → left-pointing triangle  + "Rev" label
    //   "P-P" → outward-facing pair of triangles + "P-P" label
    //   anything else → plain centred text (CC, Aft, PB, Note, Y-, Y+, …)
    struct SymbolLF : public juce::LookAndFeel_V4
    {
        void drawButtonText (juce::Graphics& g, juce::TextButton& btn,
                             bool /*highlighted*/, bool /*down*/) override
        {
            const auto b    = btn.getLocalBounds().toFloat().reduced (3.0f, 2.0f);
            const auto col  = btn.findColour (juce::TextButton::textColourOffId);
            const auto text = btn.getButtonText();

            g.setColour (col);

            const float h  = b.getHeight() * 0.50f;   // arrow height
            const float w  = h * 0.65f;               // arrow depth
            const float cy = b.getCentreY();

            // Filled triangle pointing right (tip at tipX) or left.
            auto fillTri = [&] (float tipX, bool pointRight)
            {
                juce::Path p;
                if (pointRight)
                    p.addTriangle (tipX,     cy,
                                   tipX - w, cy - h / 2.0f,
                                   tipX - w, cy + h / 2.0f);
                else
                    p.addTriangle (tipX,     cy,
                                   tipX + w, cy - h / 2.0f,
                                   tipX + w, cy + h / 2.0f);
                g.fillPath (p);
            };

            g.setFont (juce::Font (12.0f));

            if (text == "Fwd")
            {
                // Right-pointing arrow + label
                fillTri (b.getX() + w + 4.0f, true);
                g.drawFittedText ("Fwd",
                                  b.withLeft (b.getX() + w + 10.0f).toNearestInt(),
                                  juce::Justification::centredLeft, 1);
            }
            else if (text == "Rev")
            {
                // Left-pointing arrow + label
                fillTri (b.getX() + 4.0f, false);
                g.drawFittedText ("Rev",
                                  b.withLeft (b.getX() + w + 10.0f).toNearestInt(),
                                  juce::Justification::centredLeft, 1);
            }
            else if (text == "P-P")
            {
                // Outward-facing pair of arrows + centred label
                fillTri (b.getX()     + 4.0f, false);   // left arrow
                fillTri (b.getRight() - 4.0f, true);    // right arrow
                const auto mid = b.withLeft  (b.getX()     + w + 8.0f)
                                  .withRight (b.getRight() - w - 8.0f);
                g.drawFittedText ("P-P", mid.toNearestInt(),
                                  juce::Justification::centred, 1);
            }
            // ── Message-type buttons: icon on the left, label to its right ──────
            // All icons are path-drawn (no font / Unicode glyph dependency).
            // Coordinate helpers shared by all four cases:
            //   ix  = left edge of icon area
            //   icx = horizontal centre of icon
            //   icy = vertical centre of button (same as cy above)
            else if (text == "CC")
            {
                // Wavy tilde line — represents smooth continuous control change.
                const float iw  = b.getHeight() * 0.80f;
                const float ix  = b.getX() + 2.0f;
                const float amp = b.getHeight() * 0.18f;

                juce::Path wave;
                wave.startNewSubPath (ix,              cy);
                wave.cubicTo (ix + iw * 0.125f, cy - amp,
                              ix + iw * 0.375f, cy - amp,
                              ix + iw * 0.50f,  cy);
                wave.cubicTo (ix + iw * 0.625f, cy + amp,
                              ix + iw * 0.875f, cy + amp,
                              ix + iw,          cy);
                g.strokePath (wave, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved));

                g.drawFittedText ("CC",
                                  b.withLeft (ix + iw + 3.0f).toNearestInt(),
                                  juce::Justification::centredLeft, 1);
            }
            else if (text == "Aft")
            {
                // Filled downward arrow — represents pressing a key after the
                // initial strike (channel pressure / aftertouch).
                const float ih     = b.getHeight() * 0.62f;
                const float iw     = ih * 0.68f;
                const float icx    = b.getX() + 2.0f + iw * 0.5f;
                const float iy     = cy - ih * 0.5f;
                const float shaftW = iw * 0.36f;
                const float shaftH = ih * 0.52f;

                juce::Path arrow;
                arrow.addRectangle (icx - shaftW * 0.5f, iy, shaftW, shaftH);
                arrow.addTriangle  (icx - iw * 0.5f, iy + shaftH,
                                    icx + iw * 0.5f, iy + shaftH,
                                    icx,             iy + ih);
                g.fillPath (arrow);

                g.drawFittedText ("Aft",
                                  b.withLeft (b.getX() + iw + 5.0f).toNearestInt(),
                                  juce::Justification::centredLeft, 1);
            }
            else if (text == "PB")
            {
                // Bidirectional vertical arrow — pitch bends both up and down.
                const float ih    = b.getHeight() * 0.72f;
                const float iw    = ih * 0.55f;
                const float icx   = b.getX() + 2.0f + iw * 0.5f;
                const float iy    = cy - ih * 0.5f;
                const float headH = ih * 0.30f;
                const float barW  = 1.8f;

                juce::Path pbPath;
                pbPath.addRectangle (icx - barW * 0.5f, iy + headH, barW, ih - 2.0f * headH);
                pbPath.addTriangle  (icx - iw * 0.5f, iy + headH,        // top arrowhead
                                     icx + iw * 0.5f, iy + headH,
                                     icx,             iy);
                pbPath.addTriangle  (icx - iw * 0.5f, iy + ih - headH,   // bottom arrowhead
                                     icx + iw * 0.5f, iy + ih - headH,
                                     icx,             iy + ih);
                g.fillPath (pbPath);

                g.drawFittedText ("PB",
                                  b.withLeft (b.getX() + iw + 5.0f).toNearestInt(),
                                  juce::Justification::centredLeft, 1);
            }
            else if (text == "Note")
            {
                // Eighth-note glyph: filled oval head + vertical stem + curved flag.
                // The head sits at the bottom-left of the icon area; the stem rises
                // to the top-right; the flag curves away from the stem tip.
                const float ih     = b.getHeight() * 0.68f;
                const float ix     = b.getX() + 2.0f;
                const float iy     = cy - ih * 0.5f;
                const float headW  = ih * 0.50f;
                const float headH  = ih * 0.36f;
                const float headY  = iy + ih - headH * 1.1f;
                const float stemX  = ix + headW * 0.82f;
                const float stemTop = iy;

                juce::Path notePath;
                notePath.addEllipse    (ix, headY, headW, headH);
                notePath.addRectangle  (stemX, stemTop, 1.8f, headY - stemTop + headH * 0.35f);
                g.fillPath (notePath);

                juce::Path flag;
                flag.startNewSubPath (stemX + 0.9f, stemTop);
                flag.cubicTo (stemX + headW * 0.80f, stemTop + ih * 0.18f,
                              stemX + headW * 0.65f, stemTop + ih * 0.36f,
                              stemX + headW * 0.20f, stemTop + ih * 0.44f);
                g.strokePath (flag, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved));

                g.drawFittedText ("Note",
                                  b.withLeft (stemX + headW * 0.5f + 4.0f).toNearestInt(),
                                  juce::Justification::centredLeft, 1);
            }
            else
            {
                // All other buttons: centred plain text
                g.drawFittedText (text, b.toNearestInt(),
                                  juce::Justification::centred, 1);
            }
        }
    };
    SymbolLF _symbolLF;   ///< Shared by all direction + message-type buttons

    // ── Core references ───────────────────────────────────────────────────────
    DrawnCurveProcessor& proc;   ///< Named 'proc' to avoid shadowing AudioProcessorEditor::processor
    CurveDisplay curveDisplay;
    HelpOverlay  helpOverlay;    ///< Sits above all other children; hidden by default

    // ── Row 1 utility buttons ─────────────────────────────────────────────────
    juce::TextButton playButton  { "Play"  };
    juce::TextButton clearButton { "Clear" };
    juce::TextButton themeButton { "Light" };   ///< Toggles dark <-> light label
    juce::TextButton syncButton  { "Sync"  };   ///< Toggles host transport + tempo sync
    juce::TextButton helpButton  { "?"     };   ///< Shows/hides the help overlay

    // ── Row 2: playback direction (radio) + grid tick controls ────────────────
    /// Radio group: [0]=Forward  [1]=Reverse  [2]=Ping-Pong
    std::array<juce::TextButton, 3> dirBtns;

    /// Grid division controls — adjust how many lines appear on each axis.
    juce::TextButton tickYMinusBtn { "Y-" };
    juce::TextButton tickYPlusBtn  { "Y+" };
    juce::TextButton tickXMinusBtn { "X-" };
    juce::TextButton tickXPlusBtn  { "X+" };

    // ── Parameter sliders + labels ────────────────────────────────────────────
    juce::Slider ccSlider;         ///< CC number (0-127) or Note velocity (1-127)
    juce::Slider channelSlider;    ///< MIDI channel (1-16)
    juce::Slider smoothingSlider;  ///< One-pole smoother amount (0-1)
    juce::Slider rangeSlider;      ///< TwoValueHorizontal: left = Min Out, right = Max Out
    juce::Slider speedSlider;      ///< Playback speed (x) or loop beats, depending on sync mode

    juce::Label ccLabel, channelLabel, smoothingLabel, rangeLabel, speedLabel;

    // ── APVTS slider attachments ──────────────────────────────────────────────
    using Attach = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<Attach> ccAttach, channelAttach, smoothingAttach, speedAttach;
    // rangeSlider has no SliderAttachment — JUCE does not support TwoValue sliders
    // in APVTS.  Instead onValueChange writes back directly to the parameter, and
    // addParameterListener catches external changes (automation, state restore).

    // ── Message-type radio buttons ────────────────────────────────────────────
    /// [0]=CC  [1]=Channel Pressure  [2]=Pitch Bend  [3]=Note
    std::array<juce::TextButton, 4> msgTypeBtns;

    // ── State ─────────────────────────────────────────────────────────────────
    bool _lightMode { false };

    // ── Private helpers ───────────────────────────────────────────────────────

    /// Configures style, text box, and visibility for a standard horizontal slider.
    void setupSlider (juce::Slider& s, juce::Label& l,
                      const juce::String& labelText,
                      juce::Slider::SliderStyle style = juce::Slider::LinearHorizontal);

    /// Re-colours every child component to match the current _lightMode.
    void applyTheme();

    /// Called when the Sync button is toggled.
    /// Swaps the speed-slider attachment (playbackSpeed <-> syncBeats) and
    /// dims the Play button while host transport is in control.
    void onSyncToggled (bool isSync);

    /// Highlights the active message-type button and updates the CC#/Vel slot.
    void updateMsgTypeButtons();

    /// Swaps the ccSlider attachment and label between CC# and Velocity.
    void updateCCSlot();

    /// Highlights the active direction button.
    void updateDirButtons();

    /// Syncs rangeSlider thumbs from APVTS (called after external parameter changes).
    void updateRangeSlider();

    /// Refreshes the rangeLabel text ("0.00 - 1.00" format).
    void updateRangeLabel();

    /// APVTS listener: marshals parameter changes onto the message thread
    /// and calls the appropriate update* helper.
    void parameterChanged (const juce::String& paramID, float newValue) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrawnCurveEditor)
};
