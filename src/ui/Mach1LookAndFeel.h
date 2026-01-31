/*
    Mach1LookAndFeel.h
    ------------------
    Custom LookAndFeel matching Mach1 Spatial System style.
    Dark theme with flat buttons and subtle rounded corners.
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class Mach1LookAndFeel : public juce::LookAndFeel_V4
{
public:
    // Color palette matching Mach1 style
    struct Colors
    {
        static inline const juce::Colour background      { 0xFF0D0D0D };  // Main background
        static inline const juce::Colour panelBackground { 0xFF1A1A1A };  // Panel/component background
        static inline const juce::Colour headerBackground{ 0xFF1F1F1F };  // Headers, toolbars
        static inline const juce::Colour border          { 0xFF2A2A2A };  // Borders
        static inline const juce::Colour borderLight     { 0xFF3A3A3A };  // Lighter borders
        
        static inline const juce::Colour textPrimary     { 0xFFCCCCCC };  // Primary text
        static inline const juce::Colour textSecondary   { 0xFF808080 };  // Secondary/dimmed text
        static inline const juce::Colour textDark        { 0xFF0D0D0D };  // Text on light backgrounds
        
        static inline const juce::Colour buttonOff       { 0xFF1F1F1F };  // Button normal state
        static inline const juce::Colour buttonOn        { 0xFF939393 };  // Button toggled/active
        static inline const juce::Colour buttonHover     { 0xFF2A2A2A };  // Button hover
        static inline const juce::Colour buttonDown      { 0xFF3A3A3A };  // Button pressed
        
        static inline const juce::Colour accent          { 0xFF808080 };  // Accent/highlight color
        static inline const juce::Colour accentDim       { 0xFF0D0D0D };  // Dimmed accent
        
        static inline const juce::Colour waveformFill    { 0xFF808080 };  // Waveform fill
        static inline const juce::Colour waveformOutline { 0xFF2A2A2A };  // Waveform outline
        
        static inline const juce::Colour statusActive    { 0xFF4CAF50 };  // Green - active/streaming
        static inline const juce::Colour statusWarning   { 0xFFFF9800 };  // Orange - warning/stale
        static inline const juce::Colour statusError     { 0xFFE53935 };  // Red - error/offline
        static inline const juce::Colour statusNeutral   { 0xFF808080 };  // Gray - neutral/idle
        
        static inline const juce::Colour scrollbarThumb  { 0xFF3A3A3A };  // Scrollbar thumb
        static inline const juce::Colour scrollbarTrack  { 0xFF1A1A1A };  // Scrollbar track
    };

    Mach1LookAndFeel()
    {
        // Set default colors
        setColour(juce::ResizableWindow::backgroundColourId, Colors::background);
        setColour(juce::DocumentWindow::backgroundColourId, Colors::background);
        
        // Text button colors
        setColour(juce::TextButton::buttonColourId, Colors::buttonOff);
        setColour(juce::TextButton::buttonOnColourId, Colors::buttonOn);
        setColour(juce::TextButton::textColourOffId, Colors::textPrimary);
        setColour(juce::TextButton::textColourOnId, Colors::textDark);
        
        // Label colors
        setColour(juce::Label::textColourId, Colors::textPrimary);
        setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        
        // Scrollbar colors
        setColour(juce::ScrollBar::thumbColourId, Colors::scrollbarThumb);
        setColour(juce::ScrollBar::trackColourId, Colors::scrollbarTrack);
        
        // List box colors
        setColour(juce::ListBox::backgroundColourId, Colors::panelBackground);
        setColour(juce::ListBox::textColourId, Colors::textPrimary);
        setColour(juce::ListBox::outlineColourId, Colors::border);
        
        // Alert window colors
        setColour(juce::AlertWindow::backgroundColourId, Colors::panelBackground);
        setColour(juce::AlertWindow::textColourId, Colors::textPrimary);
        setColour(juce::AlertWindow::outlineColourId, Colors::border);
    }

    void drawButtonBackground(juce::Graphics& g,
                              juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
        
        juce::Colour fillColour = backgroundColour;
        
        if (shouldDrawButtonAsDown)
            fillColour = Colors::buttonDown;
        else if (shouldDrawButtonAsHighlighted)
            fillColour = Colors::buttonHover;
        
        // Flat button with subtle rounded corners (2px radius)
        g.setColour(fillColour);
        g.fillRoundedRectangle(bounds, 2.0f);
    }

    void drawButtonText(juce::Graphics& g,
                        juce::TextButton& button,
                        bool /*shouldDrawButtonAsHighlighted*/,
                        bool /*shouldDrawButtonAsDown*/) override
    {
        auto font = juce::FontOptions(11.0f);
        g.setFont(font);
        
        auto textColour = button.findColour(button.getToggleState() 
            ? juce::TextButton::textColourOnId 
            : juce::TextButton::textColourOffId);
        
        g.setColour(textColour);
        
        auto bounds = button.getLocalBounds();
        g.drawText(button.getButtonText(), bounds, juce::Justification::centred, false);
    }

    void drawScrollbar(juce::Graphics& g,
                       juce::ScrollBar& /*scrollbar*/,
                       int x, int y, int width, int height,
                       bool isScrollbarVertical,
                       int thumbStartPosition,
                       int thumbSize,
                       bool isMouseOver,
                       bool isMouseDown) override
    {
        // Draw track
        g.setColour(Colors::scrollbarTrack);
        g.fillRect(x, y, width, height);
        
        // Draw thumb
        juce::Colour thumbColour = Colors::scrollbarThumb;
        if (isMouseDown)
            thumbColour = thumbColour.brighter(0.2f);
        else if (isMouseOver)
            thumbColour = thumbColour.brighter(0.1f);
        
        g.setColour(thumbColour);
        
        if (isScrollbarVertical)
            g.fillRoundedRectangle(static_cast<float>(x + 2), 
                                   static_cast<float>(thumbStartPosition), 
                                   static_cast<float>(width - 4), 
                                   static_cast<float>(thumbSize), 
                                   3.0f);
        else
            g.fillRoundedRectangle(static_cast<float>(thumbStartPosition), 
                                   static_cast<float>(y + 2), 
                                   static_cast<float>(thumbSize), 
                                   static_cast<float>(height - 4), 
                                   3.0f);
    }

    void drawAlertBox(juce::Graphics& g,
                      juce::AlertWindow& alert,
                      const juce::Rectangle<int>& textArea,
                      juce::TextLayout& textLayout) override
    {
        auto bounds = alert.getLocalBounds().toFloat();
        
        // Background
        g.setColour(Colors::panelBackground);
        g.fillRoundedRectangle(bounds, 4.0f);
        
        // Border
        g.setColour(Colors::border);
        g.drawRoundedRectangle(bounds.reduced(0.5f), 4.0f, 1.0f);
        
        // Draw text
        textLayout.draw(g, textArea.toFloat());
    }

    int getDefaultScrollbarWidth() override
    {
        return 10;
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Mach1LookAndFeel)
};
