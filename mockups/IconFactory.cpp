#include "IconFactory.h"

class PluginEditor : public juce::AudioProcessorEditor
{
public:
    PluginEditor (...)
        : juce::AudioProcessorEditor (...),
          noteButton ("Note", dcui::IconType::noteMode, juce::Colours::white),
          clearButton ("Clear", dcui::IconType::clearGesture, juce::Colours::white),
          speedButton ("Speed", dcui::IconType::speedFree, juce::Colours::white),
          syncButton ("Sync", dcui::IconType::sync, juce::Colours::white),
          teachButton ("Teach", dcui::IconType::teach, juce::Colours::white),
          muteButton ("Mute", dcui::IconType::mute, juce::Colours::white),
          targetButton ("Target", dcui::IconType::target, juce::Colours::white),
          leftButton ("Left", dcui::IconType::directionLeft, juce::Colours::white),
          rightButton ("Right", dcui::IconType::directionRight, juce::Colours::white),
          pingPongButton ("PingPong", dcui::IconType::directionPingPong, juce::Colours::white)
    {
        addAndMakeVisible (noteButton);
        addAndMakeVisible (clearButton);
        addAndMakeVisible (speedButton);
        addAndMakeVisible (syncButton);
        addAndMakeVisible (teachButton);
        addAndMakeVisible (muteButton);
        addAndMakeVisible (targetButton);
        addAndMakeVisible (leftButton);
        addAndMakeVisible (rightButton);
        addAndMakeVisible (pingPongButton);

        leftButton.setClickingTogglesState (true);
        rightButton.setClickingTogglesState (true);
        pingPongButton.setClickingTogglesState (true);
        syncButton.setClickingTogglesState (true);
        muteButton.setClickingTogglesState (true);
        noteButton.setClickingTogglesState (true);

        syncButton.onClick = [this]
        {
            auto synced = syncButton.getToggleState();
            speedButton.setIconType (synced ? dcui::IconType::lengthSync
                                            : dcui::IconType::speedFree);
            repaint();
        };
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        auto top = r.removeFromTop (44);

        auto place = [&top] (juce::Component& c, int w)
        {
            c.setBounds (top.removeFromLeft (w).reduced (4));
        };

        place (leftButton, 44);
        place (rightButton, 44);
        place (pingPongButton, 44);
        place (syncButton, 44);
        place (speedButton, 44);
        place (noteButton, 44);
        place (teachButton, 44);
        place (muteButton, 44);
        place (clearButton, 44);
        place (targetButton, 44);
    }

    void updateTransportUi (bool isPlaying, int mode)
    {
        leftButton.setToggleState (mode == 0, juce::dontSendNotification);
        rightButton.setToggleState (mode == 1, juce::dontSendNotification);
        pingPongButton.setToggleState (mode == 2, juce::dontSendNotification);

        leftButton.setShowPauseOverlay (isPlaying && mode == 0 ? false : false);
        rightButton.setShowPauseOverlay (false);
        pingPongButton.setShowPauseOverlay (false);

        // Example:
        // if paused while left mode is selected:
        if (! isPlaying && mode == 0)
            leftButton.setShowPauseOverlay (true);
    }

private:
    dcui::IconButton noteButton;
    dcui::IconButton clearButton;
    dcui::IconButton speedButton;
    dcui::IconButton syncButton;
    dcui::IconButton teachButton;
    dcui::IconButton muteButton;
    dcui::IconButton targetButton;
    dcui::IconButton leftButton;
    dcui::IconButton rightButton;
    dcui::IconButton pingPongButton;
};