#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Engine/GestureEngine.hpp"
#include "Engine/GestureCaptureSession.hpp"

/**
 * @file PluginProcessor.h
 *
 * DrawnCurveProcessor — JUCE AudioProcessor for the DrawnCurve AUv3 plugin.
 *
 * Role
 * ────
 * Pure MIDI effect (no audio I/O).  Owns the GestureEngine and
 * GestureCaptureSession; bridges between the JUCE host API, the APVTS
 * parameter system, and the real-time engine.
 *
 * Multi-lane architecture
 * ───────────────────────
 * Up to kMaxLanes = 3 lanes play simultaneously.  Each lane has its own:
 *   - Curve snapshot (recorded gesture)
 *   - MIDI routing (message type, CC#, channel)
 *   - Shaping (smooth, min/max output range)
 *   - Scale (mode, root, custom mask) — used in Note mode
 *   - Enabled/mute state
 *
 * Shared across lanes: playback direction, speed/sync, grid density.
 *
 * Parameter naming convention
 * ───────────────────────────
 * Per-lane parameters use prefix "lN_" where N is the zero-based lane index:
 *   l0_msgType, l0_ccNumber, l0_smooth, …
 *   l1_msgType, l1_ccNumber, l1_smooth, …
 *   l2_msgType, …
 *
 * Use the static helper laneParam(lane, base) to build parameter IDs.
 *
 * Threading model
 * ───────────────
 * Three threads interact with the processor:
 *   1. UI / message thread  — editor calls begin/addCapturePoint/finalizeCapture
 *      / setPlaying / clearSnapshot(lane) / clearAllSnapshots.
 *   2. Audio thread  — host calls processBlock() each buffer period.
 *   3. HiRes timer thread  — fallback when audio graph is not connected.
 *
 * _engineLock (SpinLock) ensures only one of {audio, timer} drives the engine.
 */

// ---------------------------------------------------------------------------
/// Build a per-lane APVTS parameter ID.  Examples:
///   laneParam(0, "ccNumber") → "l0_ccNumber"
///   laneParam(2, "smooth")   → "l2_smooth"
inline juce::String laneParam (int lane, const juce::String& base)
{
    return "l" + juce::String (lane) + "_" + base;
}

// ---------------------------------------------------------------------------
// ParamID — stable string IDs for all APVTS parameters.
// Shared params use these directly; per-lane params via laneParam(lane, base).
// These must never change once shipped — they are part of the preset ABI.
// ---------------------------------------------------------------------------
namespace ParamID
{
    // Shared / global
    inline const juce::String playbackSpeed     { "playbackSpeed"     };
    inline const juce::String syncEnabled       { "syncEnabled"       };
    inline const juce::String syncBeats         { "syncBeats"         };
    inline const juce::String playbackDirection { "playbackDirection" };

    // Per-lane bases — always used via laneParam(lane, base)
    inline const juce::String msgType      { "msgType"      };
    inline const juce::String ccNumber     { "ccNumber"     };
    inline const juce::String midiChannel  { "midiChannel"  };
    inline const juce::String smoothing    { "smoothing"    };
    inline const juce::String minOutput    { "minOutput"    };
    inline const juce::String maxOutput    { "maxOutput"    };
    inline const juce::String noteVelocity { "noteVelocity" };
    inline const juce::String scaleMode    { "scaleMode"    };
    inline const juce::String scaleRoot    { "scaleRoot"    };
    inline const juce::String scaleMask    { "scaleMask"    };
    inline const juce::String laneEnabled  { "enabled"      };
}

// ---------------------------------------------------------------------------
class DrawnCurveProcessor : public juce::AudioProcessor,
                             private juce::HighResolutionTimer
{
public:
    DrawnCurveProcessor();
    ~DrawnCurveProcessor() override;

    // ── AudioProcessor identity ───────────────────────────────────────────────
    const juce::String getName() const override { return "DrawnCurve"; }

    // acceptsMidi = true so incoming MIDI CC messages can trigger Teach/Learn.
    bool acceptsMidi()  const override { return true;  }
    bool producesMidi() const override { return true;  }
    bool isMidiEffect() const override { return true;  }

    double getTailLengthSeconds() const override { return 0.0; }

    // ── Programs (not used) ───────────────────────────────────────────────────
    int  getNumPrograms()                             override { return 1; }
    int  getCurrentProgram()                          override { return 0; }
    void setCurrentProgram (int)                      override {}
    const juce::String getProgramName (int)           override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    // ── Lifecycle ─────────────────────────────────────────────────────────────
    void prepareToPlay  (double sampleRate, int samplesPerBlock) override;
    void releaseResources()                                      override;
    void processBlock   (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    // ── Editor ───────────────────────────────────────────────────────────────
    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;

    // ── State ─────────────────────────────────────────────────────────────────
    void getStateInformation (juce::MemoryBlock& destData)       override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ── Parameters ────────────────────────────────────────────────────────────
    /// Public so the editor can add listeners and read raw values directly.
    juce::AudioProcessorValueTreeState apvts;

    /// Builds the full parameter layout used to construct apvts.
    static juce::AudioProcessorValueTreeState::ParameterLayout createParams();

    // ── Gesture capture API (UI thread) ──────────────────────────────────────
    /// Signal the start of a drawing gesture on the given lane.
    void beginCapture (int lane = 0);

    /// Append a normalised point to the ongoing gesture.
    void addCapturePoint (double t, float x, float y, float pressure = 1.0f);

    /// Finalise the capture: bake current APVTS param values into a LaneSnapshot
    /// and load it into the engine for the given lane.
    void finalizeCapture (int lane = 0);

    /// Clear the curve for one lane and stop it from playing.
    void clearSnapshot (int lane);

    /// Clear all lane curves and stop playback entirely.
    void clearAllSnapshots();

    // ── Playback control (UI thread) ─────────────────────────────────────────
    void  setPlaying (bool on);
    bool  isPlaying()  const noexcept;

    // ── Query API (any thread) ────────────────────────────────────────────────
    /// true if the given lane has a valid recorded curve.
    bool  hasCurve (int lane = 0)     const noexcept;
    /// true if ANY lane has a valid curve (used by the fallback timer).
    bool  anyLaneHasCurve()           const noexcept;

    /// Approximate playhead phase [0, 1] — shared across all lanes.
    float currentPhase() const noexcept;

    /// Copy of the 256-sample lookup table for the given lane (zero-filled if empty).
    std::array<float, 256> getCurveTable (int lane = 0) const noexcept;

    /// Original recorded gesture duration for the given lane (0 if no curve).
    float curveDuration (int lane = 0) const noexcept;

    /// Last computed speed ratio (manual or host-synced).
    float getEffectiveSpeedRatio() const noexcept
    {
        return _effectiveSpeedRatio.load (std::memory_order_relaxed);
    }

    /// Build a ScaleConfig from the current APVTS state for the given lane.
    ScaleConfig getScaleConfig (int lane = 0) const noexcept;

    /// Re-read scale params for one lane and push to the engine.
    void updateEngineScale (int lane);

    /// Re-read scale params for ALL lanes and push to the engine.
    void updateAllLaneScales();

    // ── Teach / Learn (CC target lanes only) ─────────────────────────────────
    /// Enter learn mode for the given lane.  Next incoming CC message sets its CC#.
    void beginTeach  (int lane);
    /// Cancel learn mode (no-op if not active).
    void cancelTeach ();
    /// true if the given lane is currently waiting for a CC message.
    bool isTeachPending (int lane) const noexcept;

private:
    // ── Engine ────────────────────────────────────────────────────────────────
    GestureEngine         _engine;
    GestureCaptureSession _capture;

    /// UI-thread ownership of per-lane snapshots (checked for hasCurve).
    std::array<const LaneSnapshot*, kMaxLanes> _laneSnaps;

    // ── Teach / Learn ─────────────────────────────────────────────────────────
    /// Lane index currently waiting for a CC learn message (-1 = none).
    std::atomic<int> _teachPendingLane { -1 };

    // ── Fallback HiRes timer ──────────────────────────────────────────────────
    static constexpr int      kTimerIntervalMs      = 10;
    static constexpr int64_t  kAudioThreadTimeoutMs = 50;

    juce::SpinLock       _engineLock;
    juce::MidiBuffer     _pendingMidi;
    juce::SpinLock       _pendingMidiLock;
    std::atomic<int64_t> _lastProcessBlockMs  { 0 };
    double               _timerSampleRate     { 44100.0 };

    // ── Host-sync atomics ─────────────────────────────────────────────────────
    std::atomic<bool>  _hostWasPlaying      { false };
    std::atomic<float> _effectiveSpeedRatio { 1.0f  };

    void hiResTimerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrawnCurveProcessor)
};
