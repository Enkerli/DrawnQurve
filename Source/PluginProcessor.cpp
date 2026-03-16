/**
 * @file PluginProcessor.cpp
 *
 * Implementation of DrawnCurveProcessor.
 * See PluginProcessor.h for the architecture overview.
 */

#include "PluginProcessor.h"
#include "PluginEditor.h"

// ---------------------------------------------------------------------------
// ParamID — stable string identifiers for all APVTS parameters.
// These are the keys written into the preset XML; changing them will break
// existing presets, so treat them as part of the ABI.
// ---------------------------------------------------------------------------
namespace ParamID
{
    static const juce::String ccNumber          { "ccNumber"          };
    static const juce::String midiChannel       { "midiChannel"       };
    static const juce::String smoothing         { "smoothing"         };
    static const juce::String minOutput         { "minOutput"         };
    static const juce::String maxOutput         { "maxOutput"         };
    static const juce::String messageType       { "messageType"       };
    static const juce::String playbackSpeed     { "playbackSpeed"     };
    static const juce::String syncEnabled       { "syncEnabled"       };
    static const juce::String syncBeats         { "syncBeats"         };
    static const juce::String playbackDirection { "playbackDirection" };
    static const juce::String noteVelocity      { "noteVelocity"      };
}

// Helper: channel pressure is a 2-byte MIDI message; everything else is 3-byte.
static juce::MidiMessage makeMidiMessage (uint8_t status, uint8_t d1, uint8_t d2)
{
    if ((status & 0xF0u) == 0xD0u)
        return juce::MidiMessage (status, d1);          // channel pressure
    return juce::MidiMessage (status, d1, d2);
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout DrawnCurveProcessor::createParams()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { ParamID::ccNumber, 1 }, "CC Number", 0, 127, 74));

    layout.add (std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { ParamID::midiChannel, 1 }, "MIDI Channel", 1, 16, 1));

    layout.add (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamID::smoothing, 1 }, "Smoothing",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.08f));

    layout.add (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamID::minOutput, 1 }, "Min Output",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f));

    layout.add (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamID::maxOutput, 1 }, "Max Output",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 1.0f));

    layout.add (std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { ParamID::messageType, 1 }, "Message Type",
        juce::StringArray { "CC", "Channel Pressure", "Pitch Bend", "Note" }, 0));

    // Note velocity (1-127), used only when messageType == Note.
    layout.add (std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { ParamID::noteVelocity, 1 }, "Note Velocity", 1, 127, 100));

    // Speed: 0.25× to 4×, log-centred at 1× (skew=0.5 → midpoint = sqrt(0.25*4) = 1.0)
    layout.add (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamID::playbackSpeed, 1 }, "Playback Speed",
        juce::NormalisableRange<float> (0.25f, 4.0f, 0.01f, 0.5f), 1.0f));

    // Sync to host transport + tempo.
    layout.add (std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { ParamID::syncEnabled, 1 }, "Sync to Host", false));

    // Loop length in beats (used when sync is enabled).
    layout.add (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamID::syncBeats, 1 }, "Sync Beats",
        juce::NormalisableRange<float> (1.0f, 32.0f, 1.0f), 4.0f));

    // Playback direction: Forward / Reverse / Ping-Pong.
    layout.add (std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { ParamID::playbackDirection, 1 }, "Playback Direction",
        juce::StringArray { "Forward", "Reverse", "Ping-Pong" }, 0));

    return layout;
}

//==============================================================================
DrawnCurveProcessor::DrawnCurveProcessor()
    : AudioProcessor (BusesProperties()),   // no audio I/O — pure MIDI effect
      apvts (*this, nullptr, "DrawnCurve", createParams())
{
}

DrawnCurveProcessor::~DrawnCurveProcessor()
{
    stopTimer();
}

//==============================================================================
void DrawnCurveProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    _timerSampleRate = sampleRate;
    _engine.reset();
    _hostWasPlaying.store (false, std::memory_order_relaxed);
}

void DrawnCurveProcessor::releaseResources()
{
    _engine.setPlaying (false);
}

//==============================================================================
void DrawnCurveProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                         juce::MidiBuffer&         midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    juce::ignoreUnused (buffer);

    _lastProcessBlockMs.store (juce::Time::currentTimeMillis(),
                                std::memory_order_relaxed);
    midiMessages.clear();

    // Flush MIDI buffered by the fallback timer.
    {
        juce::SpinLock::ScopedLockType lock (_pendingMidiLock);
        if (! _pendingMidi.isEmpty())
        {
            midiMessages.addEvents (_pendingMidi, 0, -1, 0);
            _pendingMidi.clear();
        }
    }

    // ── Compute effective speed + handle host transport sync ──────────────────
    const bool syncOn = apvts.getRawParameterValue (ParamID::syncEnabled)->load() > 0.5f;
    float effectiveSpeed = apvts.getRawParameterValue (ParamID::playbackSpeed)->load();

    if (syncOn)
    {
        if (auto* ph = getPlayHead())
        {
            if (auto pos = ph->getPosition())
            {
                // Transport sync: edge-detect host play / stop.
                const bool hostNowPlaying = pos->getIsPlaying();
                const bool wasPlaying     = _hostWasPlaying.exchange (hostNowPlaying,
                                                                       std::memory_order_acq_rel);
                if (hostNowPlaying && ! wasPlaying)
                {
                    // Rising edge — reset phase to 0 and start.
                    juce::SpinLock::ScopedLockType lock (_engineLock);
                    _engine.reset();
                    _engine.setPlaying (true);
                }
                else if (! hostNowPlaying && wasPlaying)
                {
                    // Falling edge — stop.
                    _engine.setPlaying (false);
                }

                // Tempo sync: compute speed ratio from BPM and beat count.
                if (auto bpmOpt = pos->getBpm())
                {
                    const float bpm        = static_cast<float> (*bpmOpt);
                    const float syncBeats  = apvts.getRawParameterValue (ParamID::syncBeats)->load();
                    const float recordedDur = curveDuration();
                    if (bpm > 0.0f && syncBeats > 0.0f && recordedDur > 0.0f)
                    {
                        const float targetDur = syncBeats * 60.0f / bpm;
                        effectiveSpeed = recordedDur / targetDur;
                    }
                }
            }
        }
    }

    // Cache for fallback timer thread.
    _effectiveSpeedRatio.store (effectiveSpeed, std::memory_order_relaxed);

    // ── Advance engine on the audio thread ────────────────────────────────────
    const auto dir = static_cast<PlaybackDirection> (
        static_cast<int> (apvts.getRawParameterValue (ParamID::playbackDirection)->load()));

    {
        juce::SpinLock::ScopedLockType lock (_engineLock);
        _engine.processBlock (
            static_cast<uint32_t> (buffer.getNumSamples()),
            getSampleRate(),
            [&midiMessages] (uint8_t status, uint8_t d1, uint8_t d2)
            {
                midiMessages.addEvent (makeMidiMessage (status, d1, d2), 0);
            },
            effectiveSpeed,
            dir);
    }
}

//==============================================================================
void DrawnCurveProcessor::hiResTimerCallback()
{
    if (! isPlaying() || ! hasCurve()) return;

    const int64_t now    = juce::Time::currentTimeMillis();
    const int64_t lastAT = _lastProcessBlockMs.load (std::memory_order_relaxed);
    if (now - lastAT < kAudioThreadTimeoutMs) return;

    const auto nominalFrames =
        static_cast<uint32_t> (_timerSampleRate * kTimerIntervalMs / 1000.0);

    // Use cached values so the timer stays consistent with the audio thread.
    const float speed = _effectiveSpeedRatio.load (std::memory_order_relaxed);
    const auto  dir   = static_cast<PlaybackDirection> (
        static_cast<int> (apvts.getRawParameterValue (ParamID::playbackDirection)->load()));

    juce::MidiBuffer localBuf;
    {
        juce::SpinLock::ScopedTryLockType tryLock (_engineLock);
        if (! tryLock.isLocked()) return;

        _engine.processBlock (
            nominalFrames,
            _timerSampleRate,
            [&localBuf] (uint8_t status, uint8_t d1, uint8_t d2)
            {
                localBuf.addEvent (makeMidiMessage (status, d1, d2), 0);
            },
            speed,
            dir);
    }

    if (! localBuf.isEmpty())
    {
        juce::SpinLock::ScopedLockType lock (_pendingMidiLock);
        for (const auto meta : localBuf)
            _pendingMidi.addEvent (meta.getMessage(), meta.samplePosition);
    }
}

//==============================================================================
void DrawnCurveProcessor::beginCapture()   { _capture.begin(); }
void DrawnCurveProcessor::addCapturePoint (double t, float x, float y, float pressure)
                                           { _capture.addPoint (t, x, y, pressure); }

void DrawnCurveProcessor::finalizeCapture()
{
    if (! _capture.hasPoints()) return;

    uint8_t ccNum  = static_cast<uint8_t> (
                         static_cast<int> (apvts.getRawParameterValue (ParamID::ccNumber)->load()));
    uint8_t ch     = static_cast<uint8_t> (
                         static_cast<int> (apvts.getRawParameterValue (ParamID::midiChannel)->load()) - 1);
    float smooth   = apvts.getRawParameterValue (ParamID::smoothing)->load();
    float minOut   = apvts.getRawParameterValue (ParamID::minOutput)->load();
    float maxOut   = apvts.getRawParameterValue (ParamID::maxOutput)->load();
    auto  msgType  = static_cast<MessageType> (
                         static_cast<int> (apvts.getRawParameterValue (ParamID::messageType)->load()));
    uint8_t noteVel = static_cast<uint8_t> (
                         static_cast<int> (apvts.getRawParameterValue (ParamID::noteVelocity)->load()));

    auto* snap   = new LaneSnapshot (_capture.finalize (ccNum, ch, minOut, maxOut, smooth, msgType));
    snap->noteVelocity = noteVel;
    _currentSnap = snap;

    {
        juce::SpinLock::ScopedLockType lock (_engineLock);
        _engine.setSnapshot (snap);
        _engine.reset();
    }
}

void DrawnCurveProcessor::clearSnapshot()
{
    {
        juce::SpinLock::ScopedLockType lock (_engineLock);
        _engine.clearSnapshot();
        _capture.clear();
    }
    _currentSnap = nullptr;
}

void DrawnCurveProcessor::setPlaying (bool on)
{
    _engine.setPlaying (on);
    if (on) startTimer (kTimerIntervalMs);
    else    stopTimer();
}

bool  DrawnCurveProcessor::isPlaying()    const noexcept { return _engine.getPlaying(); }
bool  DrawnCurveProcessor::hasCurve()     const noexcept { return _currentSnap != nullptr && _currentSnap->valid; }
float DrawnCurveProcessor::currentPhase() const noexcept { return _engine.getCurrentPhase(); }

std::array<float, 256> DrawnCurveProcessor::getCurveTable() const noexcept
{
    if (_currentSnap && _currentSnap->valid) return _currentSnap->table;
    return {};
}

float DrawnCurveProcessor::curveDuration() const noexcept
{
    return (_currentSnap && _currentSnap->valid) ? _currentSnap->durationSeconds : 0.0f;
}

//==============================================================================
void DrawnCurveProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();   // includes all APVTS params automatically

    if (_currentSnap && _currentSnap->valid)
    {
        juce::MemoryBlock tableBlock (_currentSnap->table.data(), 256 * sizeof (float));
        state.setProperty ("tableData", tableBlock.toBase64Encoding(),                            nullptr);
        state.setProperty ("duration",  _currentSnap->durationSeconds,                           nullptr);
        state.setProperty ("ccNum",     static_cast<int> (_currentSnap->ccNumber),                nullptr);
        state.setProperty ("midiCh",    static_cast<int> (_currentSnap->midiChannel),             nullptr);
        state.setProperty ("minOut",    _currentSnap->minOut,                                     nullptr);
        state.setProperty ("maxOut",    _currentSnap->maxOut,                                     nullptr);
        state.setProperty ("smooth",    _currentSnap->smoothing,                                  nullptr);
        state.setProperty ("msgType",   static_cast<int> (_currentSnap->messageType),             nullptr);
        state.setProperty ("noteVel",   static_cast<int> (_currentSnap->noteVelocity),            nullptr);
    }

    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void DrawnCurveProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (! xml) return;

    auto state = juce::ValueTree::fromXml (*xml);
    apvts.replaceState (state);   // restores all params automatically

    juce::String tableB64 = state.getProperty ("tableData", juce::String());
    if (tableB64.isNotEmpty())
    {
        juce::MemoryBlock tableBlock;
        if (tableBlock.fromBase64Encoding (tableB64) &&
            tableBlock.getSize() == 256 * sizeof (float))
        {
            auto* snap = new LaneSnapshot();
            memcpy (snap->table.data(), tableBlock.getData(), 256 * sizeof (float));
            snap->durationSeconds = static_cast<float>  (static_cast<double> (state.getProperty ("duration", 1.0)));
            snap->ccNumber    = static_cast<uint8_t> (static_cast<int>    (state.getProperty ("ccNum",   74)));
            snap->midiChannel = static_cast<uint8_t> (static_cast<int>    (state.getProperty ("midiCh",  0)));
            snap->minOut      = static_cast<float>   (static_cast<double> (state.getProperty ("minOut",   0.0)));
            snap->maxOut      = static_cast<float>   (static_cast<double> (state.getProperty ("maxOut",   1.0)));
            snap->smoothing   = static_cast<float>   (static_cast<double> (state.getProperty ("smooth",  0.08)));
            snap->messageType  = static_cast<MessageType> (static_cast<int> (state.getProperty ("msgType",  0)));
            snap->noteVelocity = static_cast<uint8_t>    (static_cast<int> (state.getProperty ("noteVel",  100)));
            snap->valid        = true;

            _currentSnap = snap;
            {
                juce::SpinLock::ScopedLockType lock (_engineLock);
                _engine.setSnapshot (snap);
                _engine.reset();
            }
        }
    }
}

//==============================================================================
juce::AudioProcessorEditor* DrawnCurveProcessor::createEditor()
{
    return new DrawnCurveEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DrawnCurveProcessor();
}
