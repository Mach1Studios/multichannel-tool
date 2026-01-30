/*
    ChannelStacker - Lane Component Header
    Single lane UI with waveform display, header, and controls
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../model/ProjectModel.h"

class LaneComponent : public juce::Component
{
public:
    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void laneDeleteRequested(LaneComponent* lane) = 0;
        virtual void laneDragStarted(LaneComponent* lane) = 0;
        virtual void laneDragEnded(LaneComponent* lane) = 0;
    };

    explicit LaneComponent(Lane* lane, int displayIndex = -1);
    ~LaneComponent() override = default;
    
    // Update the display index
    void setDisplayIndex(int index);

    // Accessors
    Lane* getLane() const { return laneData; }

    // Update waveform display
    void waveformUpdated();

    // Listener management
    void addListener(Listener* listener) { listeners.add(listener); }
    void removeListener(Listener* listener) { listeners.remove(listener); }

    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;

    // Mouse handling - mostly pass-through for drag handle area
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;
    
    // Allow drag handle clicks to pass through to parent
    bool hitTest(int x, int y) override;

    // Preferred height
    static constexpr int kPreferredHeight = 100;

private:
    void drawWaveform(juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawLoadingIndicator(juce::Graphics& g, juce::Rectangle<int> bounds);
    void updateLabels();

    Lane* laneData;
    int currentDisplayIndex = -1;
    juce::ListenerList<Listener> listeners;

    juce::Label nameLabel;
    juce::Label infoLabel;
    juce::TextButton deleteButton{ "X" };

    // Note: Drag handling is done by parent LaneListComponent

    // Layout constants
    static constexpr int kHeaderHeight = 30;
    static constexpr int kDragHandleWidth = 20;
    static constexpr int kDeleteButtonWidth = 30;
    static constexpr int kMargin = 5;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LaneComponent)
};
