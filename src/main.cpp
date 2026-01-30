/*
    ChannelStacker - Multichannel Audio Stacking Tool
    Main application entry point
*/

#include <juce_gui_basics/juce_gui_basics.h>
#include "MainWindow.h"

class ChannelStackerApplication : public juce::JUCEApplication
{
public:
    ChannelStackerApplication() = default;

    const juce::String getApplicationName() override
    {
        return JUCE_APPLICATION_NAME_STRING;
    }

    const juce::String getApplicationVersion() override
    {
        return JUCE_APPLICATION_VERSION_STRING;
    }

    bool moreThanOneInstanceAllowed() override
    {
        return true;
    }

    void initialise(const juce::String& /*commandLine*/) override
    {
        mainWindow = std::make_unique<MainWindow>(getApplicationName());
    }

    void shutdown() override
    {
        mainWindow.reset();
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    void anotherInstanceStarted(const juce::String& /*commandLine*/) override
    {
        // Bring window to front if another instance is started
        if (mainWindow != nullptr)
            mainWindow->toFront(true);
    }

private:
    std::unique_ptr<MainWindow> mainWindow;
};

// Application entry point
START_JUCE_APPLICATION(ChannelStackerApplication)
