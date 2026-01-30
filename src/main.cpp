/*
    ChannelStacker - Multichannel Audio Stacking Tool
    Main application entry point
*/

#include <juce_gui_basics/juce_gui_basics.h>
#include "MainWindow.h"
#include "ui/Mach1LookAndFeel.h"

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
        // Set custom look and feel
        customLookAndFeel = std::make_unique<Mach1LookAndFeel>();
        juce::LookAndFeel::setDefaultLookAndFeel(customLookAndFeel.get());
        
        mainWindow = std::make_unique<MainWindow>(getApplicationName());
    }

    void shutdown() override
    {
        mainWindow.reset();
        
        // Clear look and feel before destroying it
        juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
        customLookAndFeel.reset();
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
    std::unique_ptr<Mach1LookAndFeel> customLookAndFeel;
    std::unique_ptr<MainWindow> mainWindow;
};

// Application entry point
START_JUCE_APPLICATION(ChannelStackerApplication)
