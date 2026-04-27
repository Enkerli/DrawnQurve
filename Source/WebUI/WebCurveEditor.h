#pragma once

// WebCurveEditor — AudioProcessorEditor backed by a JUCE 8 WebBrowserComponent.
//
// The UI is the React app bundled in Source/WebUI/dist/ (index.html + bundle.js).
// Communication:
//   C++ → JS : emitEventIfBrowserIsVisible("eventId", juce::var{…})
//   JS → C++ : Options::withEventListener("eventId", …) callbacks
//
// Build note: JUCE_WEB_BROWSER must be 1 (set in CMakeLists.txt for this target).

#include <juce_audio_utils/juce_audio_utils.h>
#include "../PluginProcessor.h"

class WebCurveEditor : public juce::AudioProcessorEditor,
                       private juce::AudioProcessorValueTreeState::Listener,
                       private juce::Timer
{
public:
    explicit WebCurveEditor (DrawnCurveProcessor& p);
    ~WebCurveEditor() override;

    void paint   (juce::Graphics&) override {}
    void resized () override;

private:
    // ── APVTS listener ────────────────────────────────────────────────────────
    void parameterChanged (const juce::String& paramID, float newValue) override;

    // ── Timer — phase heartbeat at 30 Hz ─────────────────────────────────────
    void timerCallback() override;

    // ── Resource provider — serves bundled HTML/JS from BinaryData ────────────
    static std::optional<juce::WebBrowserComponent::Resource>
        provideResource (const juce::String& path);

    // ── Bridge helpers ────────────────────────────────────────────────────────
    void sendStateSnapshot();
    void sendCurveData (int lane);
    void onJsEvent (const juce::String& eventId, const juce::Array<juce::var>& args);

    static juce::WebBrowserComponent::Options buildOptions (WebCurveEditor* owner);

    // ── Members ───────────────────────────────────────────────────────────────
    DrawnCurveProcessor& proc;
    juce::WebBrowserComponent webView;

    // Parameter IDs that we listen to (populated in constructor)
    juce::StringArray listenedParams;

    // true once the JS page has loaded and sent "uiReady" — guards all outbound emits
    std::atomic<bool> pageReady { false };

    // Last known phase sent to JS — avoid redundant emits
    float lastSentPhase { -1.0f };

    // ── Standalone MIDI output ────────────────────────────────────────────────
    // The native PluginEditor used to create a virtual MIDI source so other
    // apps see "DrawnQurve" as a MIDI input.  Without this in standalone, the
    // engine generates MIDI but has nowhere to send it (proc._virtualMidiOut
    // and _directMidiOut both stay null) — every lock/quantize change reaches
    // the engine but the output is silently discarded.  We mirror the same
    // setup here when running as a standalone app.
    std::unique_ptr<juce::MidiOutput> virtualMidiPort;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WebCurveEditor)
};
