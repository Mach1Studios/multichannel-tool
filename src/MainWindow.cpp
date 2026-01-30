/*
    ChannelStacker - Main Window Implementation
*/

#include "MainWindow.h"

MainWindow::MainWindow(const juce::String& name)
    : DocumentWindow(name,
                     juce::Desktop::getInstance().getDefaultLookAndFeel()
                         .findColour(ResizableWindow::backgroundColourId),
                     DocumentWindow::allButtons)
{
    setUsingNativeTitleBar(true);
    setContentOwned(new MainComponent(), true);

    // Set minimum size
    setResizable(true, true);
    setResizeLimits(800, 600, 4096, 4096);

    // Center on screen
    centreWithSize(getWidth(), getHeight());

    setVisible(true);
}

void MainWindow::closeButtonPressed()
{
    juce::JUCEApplication::getInstance()->systemRequestedQuit();
}
