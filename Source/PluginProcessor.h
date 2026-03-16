#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Engine/GestureEngine.hpp"
#include "Engine/GestureCaptureSession.hpp"

//==============================================================================
class DrawnCurveProcessor : public juce::AudioProcessor,
                             private juce::HighResolutionTimer
{
public:
    DrawnCurveProcessor();
    ~DrawnCurveProcessor() override;

    // ── AudioProcessor identity ───────────────────────────────────────────────
    const juce::String getName() const override { return "DrawnCurve"; }

    // isMidiEffect() false → JUCE picks kAudioUnitType_Effect ('aufx').
    // producesMidi() true  → JUCE wires up midiOutputEventBlock.
    bool acceptsMidi()  const override { return false; }
    bool producesMidi() const override { return true;  }
    bool isMidiEffect() const override { return true; }

    double getTailLengthSeconds() const override { return 0.0; }

    // ── Programs (unused) ─────────────────────────────────────────────────────
    int  getNumPrograms()       override { return 1; }
    int  getCurrentProgram()    override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    // ── Lifecycle ─────────────────────────────────────────────────────────────
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    // ── Editor ────────────────────────────────────────────────────────────────
    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;

    // ── State ─────────────────────────────────────────────────────────────────
    void getStateInformation(juce::MemoryBlock& destData)       override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // ── Parameters ───────────────────────────────────────────────────────────
    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParams();

    // ── Gesture API — called from editor (UI / message thread) ───────────────
    void  beginCapture();
    void  addCapturePoint(double t, float x, float y, float pressure = 1.0f);
    void  finalizeCapture();    // bakes current param values into the snapshot
    void  clearSnapshot();

    void  setPlaying(bool on);
    bool  isPlaying()    const noexcept;
    bool  hasCurve()     const noexcept;
    float currentPhase() const noexcept;

    // Returns a copy of the 256-sample lookup table, or zero-filled if no curve.
    std::array<float, 256> getCurveTable()    const noexcept;

    // Duration of the currently loaded curve (0 if none).
    float                  curveDuration()    const noexcept;

private:
    // ── Engine ────────────────────────────────────────────────────────────────
    GestureEngine         _engine;
    GestureCaptureSession _capture;
    const LaneSnapshot*   _currentSnap { nullptr };   // UI-thread ownership

    // ── Fallback timer ────────────────────────────────────────────────────────
    // When the host isn't calling processBlock (e.g. no audio flowing through
    // the effect in Loopy Pro), this high-res timer advances the engine so the
    // UI stays responsive.  Any MIDI it generates is buffered and flushed the
    // next time the audio thread does call processBlock.
    //
    // Only ONE of {audio thread, timer thread} drives the engine at a time,
    // controlled by _engineLock.  The audio thread wins whenever it's active.

    static constexpr int      kTimerIntervalMs      = 10;
    static constexpr int64_t  kAudioThreadTimeoutMs = 50;  // ms before timer takes over

    juce::SpinLock           _engineLock;
    juce::MidiBuffer         _pendingMidi;        // timer-generated MIDI, flushed by processBlock
    juce::SpinLock           _pendingMidiLock;
    std::atomic<int64_t>     _lastProcessBlockMs  { 0 };
    double                   _timerSampleRate     { 44100.0 };  // set in prepareToPlay

    void hiResTimerCallback() override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DrawnCurveProcessor)
};
