/*
 * StandaloneApp.cpp — custom JUCEApplication for DrawnQurve macOS standalone.
 *
 * Why this exists
 * ───────────────
 * JUCE's default StandaloneFilterApp forced a dummy 1-channel CoreAudio output
 * for MIDI-effect plugins ("add a dummy output channel so they can receive audio
 * callbacks").  For DrawnQurve — which drives MIDI via its HiResTimer fallback
 * and never needs audio callbacks — this caused two problems:
 *
 *   1. HALC_ProxyIOContext::IOWorkLoop logged "skipping cycle due to overload"
 *      because CoreAudio couldn't meet RT deadlines for the unused device.
 *
 *   2. On quit, JUCE's stop handshake waited for AudioDeviceStop to be called
 *      from the RT thread; if the IOWorkLoop was in an overload state the wait
 *      timed out, AudioDeviceDestroyIOProcID raced against the IOWorkLoop, and
 *      the process terminated with "mutex lock failed: Invalid argument".
 *
 * The root fix is in juce_StandaloneFilterWindow.h (the dummy-channel block is
 * commented out).  With 0 output channels requested, AudioDeviceManager never
 * opens a CoreAudio device, so the IOWorkLoop never starts.
 *
 * This custom app exists solely to add the TeachMidiCallback so that
 * Teach/Learn CC detection works without an audio callback driving processBlock.
 *
 * Teach/Learn mode
 * ────────────────
 * Without an audio device, AudioProcessorPlayer never calls processBlock, so
 * the CC-detect logic in processBlock would not run.  We register a
 * TeachMidiCallback adapter as a MidiInputCallback.  The adapter forwards each
 * incoming message to DrawnCurveProcessor::handleTeachMidi(), which contains
 * the same CC-detect logic.  MIDI device management (open/close, which devices
 * are enabled) is unchanged — AudioDeviceManager/StandalonePluginHolder handles
 * that.
 */

#if JucePlugin_Build_Standalone

#include <unistd.h>
#include "PluginProcessor.h"
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

//==============================================================================
// JUCE 8 + macOS 26 (Darwin 25): CVDisplayLink background-thread callbacks can
// race with NSViewComponentPeer destruction, locking a destroyed std::mutex.
// This is a known JUCE framework bug.  Install a targeted terminate handler so
// the specific EINVAL mutex crash exits cleanly instead of crashing.
static void installDisplayLinkCrashGuard()
{
    static auto previousHandler = std::get_terminate();
    std::set_terminate ([]
    {
        try
        {
            if (auto eptr = std::current_exception())
                std::rethrow_exception (eptr);
        }
        catch (const std::system_error& e)
        {
            // EINVAL from pthread_mutex_lock on a destroyed mutex
            if (e.code().value() == EINVAL)
                _exit (0);
        }
        catch (...) {}

        // Anything else: fall through to the default handler
        if (previousHandler)
            previousHandler();
        std::abort();
    });
}

//==============================================================================
/// Thin adapter: receives raw MIDI callbacks and forwards to handleTeachMidi().
struct TeachMidiCallback final : public juce::MidiInputCallback
{
    DrawnCurveProcessor& proc;
    explicit TeachMidiCallback (DrawnCurveProcessor& p) : proc (p) {}
    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& msg) override
    {
        proc.handleTeachMidi (msg);
    }
};

//==============================================================================
class DrawnQurveStandaloneApp final : public juce::JUCEApplication
{
public:
    DrawnQurveStandaloneApp()
    {
        juce::PropertiesFile::Options opts;
        opts.applicationName     = juce::CharPointer_UTF8 (JucePlugin_Name);
        opts.filenameSuffix      = ".settings";
        opts.osxLibrarySubFolder = "Application Support";
        opts.folderName          = "";
        appProperties.setStorageParameters (opts);
    }

    const juce::String getApplicationName()    override { return juce::CharPointer_UTF8 (JucePlugin_Name); }
    const juce::String getApplicationVersion() override { return JucePlugin_VersionString; }
    bool moreThanOneInstanceAllowed()          override { return true; }
    void anotherInstanceStarted (const juce::String&) override {}

    //==============================================================================
    void initialise (const juce::String&) override
    {
        installDisplayLinkCrashGuard();

        mainWindow.reset (new juce::StandaloneFilterWindow (
            getApplicationName(),
            juce::LookAndFeel::getDefaultLookAndFeel()
                .findColour (juce::ResizableWindow::backgroundColourId),
            appProperties.getUserSettings(),
            false   // don't take ownership of settings
        ));

        if (mainWindow == nullptr) return;

        // Register the TeachMidiCallback adapter so Teach/Learn CC detection
        // works without an audio callback driving processBlock.
        if (auto* proc = dynamic_cast<DrawnCurveProcessor*> (mainWindow->getAudioProcessor()))
        {
            teachCallback = std::make_unique<TeachMidiCallback> (*proc);
            mainWindow->getDeviceManager().addMidiInputDeviceCallback ({}, teachCallback.get());
        }

       #if JUCE_STANDALONE_FILTER_WINDOW_USE_KIOSK_MODE
        juce::Desktop::getInstance().setKioskModeComponent (mainWindow.get(), false);
       #endif

        mainWindow->setVisible (true);
    }

    void shutdown() override
    {
        if (mainWindow != nullptr && teachCallback != nullptr)
        {
            mainWindow->getDeviceManager().removeMidiInputDeviceCallback ({}, teachCallback.get());
            teachCallback.reset();
        }

        mainWindow = nullptr;
        appProperties.saveIfNeeded();
    }

    //==============================================================================
    void systemRequestedQuit() override
    {
        if (mainWindow != nullptr)
            mainWindow->pluginHolder->savePluginState();

        if (juce::ModalComponentManager::getInstance()->cancelAllModalComponents())
        {
            juce::Timer::callAfterDelay (100, []()
            {
                if (auto* app = juce::JUCEApplicationBase::getInstance())
                    app->systemRequestedQuit();
            });
        }
        else
        {
            quit();
        }
    }

private:
    juce::ApplicationProperties                    appProperties;
    std::unique_ptr<juce::StandaloneFilterWindow>  mainWindow;
    std::unique_ptr<TeachMidiCallback>             teachCallback;
};

//==============================================================================
juce::JUCEApplicationBase* juce_CreateApplication()
{
    return new DrawnQurveStandaloneApp();
}

#endif // JucePlugin_Build_Standalone
