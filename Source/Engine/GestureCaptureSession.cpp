#include "GestureCaptureSession.hpp"
#include <algorithm>
#include <cmath>

// ---------------------------------------------------------------------------

void GestureCaptureSession::begin()
{
    rawPoints.clear();
}

void GestureCaptureSession::addPoint (double t, float x, float y, float pressure)
{
    rawPoints.push_back ({ t, x, y, pressure });
}

void GestureCaptureSession::clear()
{
    rawPoints.clear();
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/**
 * Linearly interpolate Y at time t using the raw capture sequence.
 *
 * Clamps to the first / last sample at the edges so the 256-sample table
 * doesn't extrapolate beyond what was actually drawn.
 */
float GestureCaptureSession::interpolateYAtTime (double t) const
{
    if (rawPoints.empty())   return 0.5f;
    if (rawPoints.size() == 1) return rawPoints[0].y;

    // Clamp to edges.
    if (t <= rawPoints.front().t) return rawPoints.front().y;
    if (t >= rawPoints.back().t)  return rawPoints.back().y;

    // Linear search for the surrounding segment.
    // (Point count is typically < 200 for a 0.5-2 s gesture at 60 Hz touch rate,
    //  so O(n) search is fine here; runs on the UI thread, not the audio thread.)
    for (size_t i = 1; i < rawPoints.size(); ++i)
    {
        if (rawPoints[i].t >= t)
        {
            const double t0 = rawPoints[i - 1].t;
            const double t1 = rawPoints[i].t;
            if (t1 <= t0) return rawPoints[i].y;   // guard against duplicate timestamps

            const float frac = static_cast<float> ((t - t0) / (t1 - t0));
            return rawPoints[i - 1].y + frac * (rawPoints[i].y - rawPoints[i - 1].y);
        }
    }
    return rawPoints.back().y;
}

// ---------------------------------------------------------------------------
// finalize
// ---------------------------------------------------------------------------

/**
 * Resample the gesture into a 256-sample LaneSnapshot.
 *
 * Each of the 256 table slots corresponds to an evenly-spaced fraction of
 * the total gesture duration.  The slot value is:
 *   value = 1.0 - interpolatedY
 * because UIKit y=0 is at the top of the screen and MIDI CC 127 should
 * correspond to the top of the drawn area.
 */
LaneSnapshot GestureCaptureSession::finalize (uint8_t ccNumber, uint8_t midiChannel,
                                               float minOut, float maxOut, float smoothing,
                                               MessageType messageType) const
{
    LaneSnapshot snap{};

    if (rawPoints.size() < 2)
    {
        // Not enough data for a valid curve.
        snap.table.fill (0.5f);
        snap.durationSeconds = 1.0f;
        snap.valid           = false;
        return snap;
    }

    const double t0       = rawPoints.front().t;
    const double t1       = rawPoints.back().t;
    const double duration = std::max (0.05, t1 - t0);   // minimum 50 ms to avoid zero-length loops

    snap.durationSeconds = static_cast<float> (duration);
    snap.ccNumber        = ccNumber;
    snap.midiChannel     = midiChannel;
    snap.minOut          = minOut;
    snap.maxOut          = maxOut;
    snap.smoothing       = smoothing;
    snap.messageType     = messageType;
    snap.valid           = true;

    for (int i = 0; i < 256; ++i)
    {
        const float  phase   = static_cast<float> (i) / 255.0f;
        const double targetT = t0 + phase * duration;
        const float  y       = interpolateYAtTime (targetT);

        // Invert: UIKit y=0 (top) becomes value 1.0 (max CC / pitch).
        snap.table[static_cast<size_t> (i)] = std::clamp (1.0f - y, 0.0f, 1.0f);
    }

    return snap;
}
