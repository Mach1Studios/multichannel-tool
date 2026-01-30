/*
    ChannelStacker - Lane List Component Header
    Scrollable vertical list of lanes with drag-reorder support
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../model/ProjectModel.h"
#include "LaneComponent.h"
#include <vector>
#include <memory>

class LaneListComponent : public juce::Component,
                          public ProjectModel::Listener,
                          public LaneComponent::Listener
{
public:
    explicit LaneListComponent(ProjectModel& model);
    ~LaneListComponent() override;

    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;
    
    // MouseListener overrides - these catch events from child components too
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;

    // ProjectModel::Listener overrides
    void laneAdded(Lane* lane, int index) override;
    void laneRemoved(int index) override;
    void lanesReordered() override;
    void laneWaveformUpdated(Lane* lane) override;

    // LaneComponent::Listener overrides
    void laneDeleteRequested(LaneComponent* laneComp) override;
    void laneDragStarted(LaneComponent* laneComp) override;
    void laneDragEnded(LaneComponent* laneComp) override;

private:
    void rebuildLaneComponents();
    void updateLayout();
    int getDropIndexFromY(int y) const;
    LaneComponent* findLaneComponentAt(juce::Point<int> pos);

    ProjectModel& projectModel;
    std::vector<std::unique_ptr<LaneComponent>> laneComponents;

    // Scrolling
    juce::Viewport viewport;
    juce::Component contentComponent;

    // Drag state
    bool isDragging = false;
    LaneComponent* draggedLaneComponent = nullptr;
    int draggedOriginalIndex = -1;
    int dragInsertIndex = -1;
    juce::Point<int> dragStartPos;

    // Layout
    static constexpr int kLaneSpacing = 5;
    static constexpr int kLaneHeight = LaneComponent::kPreferredHeight;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LaneListComponent)
};
