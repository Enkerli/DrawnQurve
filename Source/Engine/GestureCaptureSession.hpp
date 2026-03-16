#pragma once
#include <vector>
#include <cstdint>
#include "LaneSnapshot.hpp"

/**
 * @file GestureCaptureSession.hpp
 *
 * Collects raw touch / mouse points during a drawing gesture and bakes
 * them into a 256-sample LaneSnapshot.
 *
 * Usage
 * ─────
 *   session.begin();
 *   while (dragging)
 *       session.addPoint(t, x, y);     // t = seconds since begin()
 *   if (session.hasPoints())
 *       LaneSnapshot snap = session.finalize(...);
 *
 * Coordinate conventions
 * ──────────────────────
 *   x  : 0.0 = left edge of plot area,  1.0 = right edge  (time axis)
 *   y  : 0.0 = top of plot area,        1.0 = bottom       (UIKit convention)
 *   value (stored in table) = 1.0 - y   (inverted so top = high MIDI value)
 *
 * All methods run on the UI/message thread.
 */

// ---------------------------------------------------------------------------
/// One raw sample collected during a drawing gesture.
struct CapturePoint
{
    double t;         ///< Seconds since gesture start
    float  x;         ///< 0..1, left → right (time axis; ignored during finalize — time comes from t)
    float  y;         ///< 0..1, top → bottom (UIKit screen coordinates)
    float  pressure;  ///< 0..1 (always 1.0 on devices without force touch; reserved for future use)
};

// ---------------------------------------------------------------------------
/**
 * Accumulates CapturePoints and converts them into a LaneSnapshot.
 *
 * The conversion samples the piecewise-linear gesture at 256 evenly-spaced
 * time positions, producing the lookup table used by GestureEngine.
 */
class GestureCaptureSession
{
public:
    /// Start a new capture (clears any previous points).
    void begin();

    /// Append a point.  t must increase monotonically across calls.
    void addPoint (double t, float x, float y, float pressure = 1.0f);

    /// True if at least 2 points have been collected (minimum for a valid gesture).
    bool hasPoints() const { return rawPoints.size() >= 2; }

    /// Clear all collected points without producing a snapshot.
    void clear();

    /**
     * Resample the gesture into a 256-sample LaneSnapshot.
     *
     * @param ccNumber     MIDI CC number (0-127) — stored in snapshot for CC mode.
     * @param midiChannel  0-indexed MIDI channel (0 = ch 1).
     * @param minOut       Output range lower bound [0, 1].
     * @param maxOut       Output range upper bound [0, 1].
     * @param smoothing    One-pole smoother coefficient copied into snapshot.
     * @param messageType  Determines what MIDI messages the engine emits.
     *
     * Returns a snapshot with valid=false if fewer than 2 points were captured.
     *
     * Implementation detail: each table entry is computed by linearly
     * interpolating the raw point sequence at the corresponding time offset,
     * then inverting the y coordinate (top = 1.0, bottom = 0.0).
     */
    LaneSnapshot finalize (uint8_t ccNumber, uint8_t midiChannel,
                           float minOut, float maxOut, float smoothing,
                           MessageType messageType = MessageType::CC) const;

private:
    std::vector<CapturePoint> rawPoints;

    /// Linearly interpolate Y at time t using the raw point sequence.
    float interpolateYAtTime (double t) const;
};
