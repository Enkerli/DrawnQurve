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
 * Threading model
 * ───────────────
 * Three threads interact with the processor:
 *
 *   1. UI / message thread  — editor calls beginCapture / addCapturePoint /
 *      finalizeCapture / setPlaying / clearSnapshot.
 *
 *   2. Audio thread  — host calls processBlock() each buffer period.
 *      Reads APVTS atomics, advances GestureEngine, emits MIDI.
 *
 *   3. HiRes timer thread  — fallback when the host isn't calling processBlock
 *      (e.g. app in background, or no audio graph connected in Loopy Pro).
 *      Advances the engine and queues MIDI into _pendingMidi; the next
 *      processBlock call flushes it.
 *
 * _engineLock (SpinLock) ensures only one of {audio, timer} drives the engine
 * at any given moment.  The audio thread wins whenever it is active.
 *
 * Host-sync mode
 * ──────────────
 * When syncEnabled is true, processBlock reads the host play head:
 *   - Rising  edge (stop→play): reset engine phase, start playback.
 *   - Falling edge (play→stop): stop playback.
 *   - BPM available: compute effectiveSpeed so the loop duration matches
 *     syncBeats beats at the current tempo.
 *
 * State persistence
 * ─────────────────
 * APVTS parameters are saved/restored automatically via getStateInformation /
 * setStateInformation.  The 256-sample curve table and LaneSnapshot fields
 * are serialised as additional ValueTree properties (base-64 encoded binary).
 */
class DrawnCurveProcessor : public juce::AudioProcessor,
                             private juce::HighResolutionTimer
{
public:
    DrawnCurveProcessor();
    ~DrawnCurveProcessor() override;

    // ── AudioProcessor identity ───────────────────────────────────────────────
    const juce::String getName() const override { return "DrawnCurve"; }

    // isMidiEffect() = true  → JUCE registers the AUv3 as type 'aumi' (MIDI processor).
    // producesMidi() = true  → JUCE wires up midiOutputEventBlock on iOS.
    bool acceptsMidi()  const override { return false; }
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

    // ── Editor ────────────────────────────────────────────────────────────────
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
    /// Signal the start of a drawing gesture.  Clears previous capture data.
    void beginCapture();

    /// Append a normalised point to the ongoing gesture.
    /// @param t         Seconds since beginCapture() (must be monotonically increasing).
    /// @param x         Horizontal position in [0, 1] (time axis; currently unused in baking).
    /// @param y         Vertical position in [0, 1] (UIKit: 0 = top, 1 = bottom).
    /// @param pressure  Touch pressure in [0, 1]; defaults to 1.0 if unavailable.
    void addCapturePoint (double t, float x, float y, float pressure = 1.0f);

    /// Finalise the capture: bake current APVTS param values into a LaneSnapshot
    /// and load it into the engine.  No-op if fewer than 2 points were captured.
    void finalizeCapture();

    /// Clear the current curve and stop playback.
    void clearSnapshot();

    // ── Playback control (UI thread) ─────────────────────────────────────────
    void  setPlaying (bool on);
    bool  isPlaying()  const noexcept;

    // ── Query API (any thread) ────────────────────────────────────────────────
    bool  hasCurve()     const noexcept;  ///< true if a valid snapshot is loaded
    float currentPhase() const noexcept;  ///< Approximate playhead phase [0, 1] — for UI display

    /// Copy of the 256-sample lookup table (or zero-filled if no curve loaded).
    std::array<float, 256> getCurveTable() const noexcept;

    /// Original recorded gesture duration (0 if no curve).
    float curveDuration() const noexcept;

    /// Last computed speed ratio (manual or host-synced).
    /// Used by the UI's duration overlay (no audio-thread synchronisation needed).
    float getEffectiveSpeedRatio() const noexcept
    {
        return _effectiveSpeedRatio.load (std::memory_order_relaxed);
    }

private:
    // ── Engine ────────────────────────────────────────────────────────────────
    GestureEngine         _engine;
    GestureCaptureSession _capture;
    const LaneSnapshot*   _currentSnap { nullptr };   ///< UI-thread ownership; checked for hasCurve()

    // ── Fallback HiRes timer ──────────────────────────────────────────────────
    // When no audio graph is connected (e.g. Loopy Pro MIDI-only routing,
    // or the app is backgrounded), processBlock is never called.  This timer
    // fires every ~10 ms to keep the engine advancing and the UI animated.
    //
    // MIDI generated by the timer is accumulated in _pendingMidi and flushed
    // the next time the audio thread does call processBlock.
    //
    // Guard: the audio thread is considered "active" if it has called
    // processBlock within the last kAudioThreadTimeoutMs milliseconds.
    // The timer checks this before touching the engine.

    static constexpr int      kTimerIntervalMs      = 10;   ///< Timer period (ms)
    static constexpr int64_t  kAudioThreadTimeoutMs = 50;   ///< Audio thread considered idle after this

    juce::SpinLock       _engineLock;         ///< Ensures only one thread drives the engine
    juce::MidiBuffer     _pendingMidi;        ///< Timer-generated events awaiting flush by audio thread
    juce::SpinLock       _pendingMidiLock;
    std::atomic<int64_t> _lastProcessBlockMs  { 0 };       ///< Timestamp of last audio-thread call
    double               _timerSampleRate     { 44100.0 }; ///< Sample rate cached from prepareToPlay

    // ── Host-sync atomics ─────────────────────────────────────────────────────
    /// Previous host transport state — used to edge-detect play/stop transitions.
    std::atomic<bool>  _hostWasPlaying      { false };

    /// Speed ratio computed in processBlock; cached here so hiResTimerCallback
    /// can use the same value without re-reading APVTS.
    std::atomic<float> _effectiveSpeedRatio { 1.0f };

    void hiResTimerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrawnCurveProcessor)
};
