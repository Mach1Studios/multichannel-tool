/*
    ChannelStacker - Lane List Component Implementation
*/

#include "LaneListComponent.h"
#include "Mach1LookAndFeel.h"

// Debug logging macro
#define DEBUG_LOG(msg) DBG(msg)

LaneListComponent::LaneListComponent(ProjectModel& model)
    : projectModel(model)
{
    projectModel.addListener(this);

    // Setup viewport for scrolling
    viewport.setViewedComponent(&contentComponent, false);
    viewport.setScrollBarsShown(true, false);
    addAndMakeVisible(viewport);
    
    // Add mouse listener to contentComponent to catch events from lane components
    contentComponent.addMouseListener(this, true);  // true = listen to children too

    rebuildLaneComponents();
}

LaneListComponent::~LaneListComponent()
{
    contentComponent.removeMouseListener(this);
    projectModel.removeListener(this);
}

void LaneListComponent::paint(juce::Graphics& g)
{
    // Background
    g.fillAll(Mach1LookAndFeel::Colors::background);
    
    // Draw drop indicator during drag
    if (isDragging && dragInsertIndex >= 0)
    {
        // Convert content coordinate to viewport coordinate
        auto contentPos = viewport.getViewPosition();
        int y = dragInsertIndex * (kLaneHeight + kLaneSpacing) - contentPos.y;

        // Clamp to visible area
        if (y >= 0 && y <= getHeight())
        {
            // Draw insertion line - Mach1 accent color
            g.setColour(Mach1LookAndFeel::Colors::accent);
            g.fillRect(5, y - 1, getWidth() - 10, 2);

            // Draw arrow indicators
            juce::Path leftArrow;
            leftArrow.addTriangle(0.0f, 4.0f, 8.0f, 0.0f, 8.0f, 8.0f);
            g.fillPath(leftArrow, juce::AffineTransform::translation(2.0f, static_cast<float>(y) - 4.0f));

            juce::Path rightArrow;
            rightArrow.addTriangle(8.0f, 4.0f, 0.0f, 0.0f, 0.0f, 8.0f);
            g.fillPath(rightArrow, juce::AffineTransform::translation(static_cast<float>(getWidth()) - 10.0f, static_cast<float>(y) - 4.0f));
        }
    }
}

void LaneListComponent::resized()
{
    viewport.setBounds(getLocalBounds());
    updateLayout();
}

void LaneListComponent::mouseDown(const juce::MouseEvent& e)
{
    // Get position in content component coordinates
    auto posInContent = contentComponent.getLocalPoint(e.eventComponent, e.getPosition());
    
    DEBUG_LOG("LaneListComponent::mouseDown - eventComponent: " << e.eventComponent->getName() 
              << ", pos in content: " << posInContent.x << "," << posInContent.y);
    
    for (size_t i = 0; i < laneComponents.size(); ++i)
    {
        auto* comp = laneComponents[i].get();
        if (comp->getBounds().contains(posInContent))
        {
            // Check if in drag handle area (left 20 pixels of the lane)
            auto posInLane = comp->getLocalPoint(&contentComponent, posInContent);
            DEBUG_LOG("  Hit lane " << i << ", posInLane: " << posInLane.x << "," << posInLane.y);
            
            if (posInLane.x < 20)
            {
                isDragging = true;
                draggedLaneComponent = comp;
                draggedOriginalIndex = static_cast<int>(i);
                dragInsertIndex = draggedOriginalIndex;
                dragStartPos = e.getPosition();
                
                DEBUG_LOG("  Started drag from index " << draggedOriginalIndex);
                repaint();
                return;
            }
        }
    }
}

void LaneListComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (!isDragging)
        return;

    // Convert to content coordinates
    auto posInContent = contentComponent.getLocalPoint(e.eventComponent, e.getPosition());
    int newInsertIndex = getDropIndexFromY(posInContent.y);
    
    if (newInsertIndex != dragInsertIndex)
    {
        dragInsertIndex = newInsertIndex;
        DEBUG_LOG("LaneListComponent::mouseDrag - insertIndex now " << dragInsertIndex);
        repaint();
    }
    
    // Auto-scroll if near edges - convert to our local coordinates
    auto posInThis = getLocalPoint(e.eventComponent, e.getPosition());
    int scrollMargin = 30;
    auto viewPos = viewport.getViewPosition();
    
    if (posInThis.y < scrollMargin && viewPos.y > 0)
    {
        viewport.setViewPosition(viewPos.x, std::max(0, viewPos.y - 10));
    }
    else if (posInThis.y > getHeight() - scrollMargin && 
             viewPos.y < contentComponent.getHeight() - viewport.getHeight())
    {
        viewport.setViewPosition(viewPos.x, viewPos.y + 10);
    }
}

void LaneListComponent::mouseUp(const juce::MouseEvent& /*e*/)
{
    if (!isDragging)
        return;

    DEBUG_LOG("LaneListComponent::mouseUp - drop from " << draggedOriginalIndex << " to " << dragInsertIndex);
    
    if (dragInsertIndex >= 0 && draggedOriginalIndex >= 0)
    {
        DEBUG_LOG("  Calling projectModel.moveLane(" << draggedOriginalIndex << ", " << dragInsertIndex << ")");
        projectModel.moveLane(draggedOriginalIndex, dragInsertIndex);
    }
    
    isDragging = false;
    draggedLaneComponent = nullptr;
    draggedOriginalIndex = -1;
    dragInsertIndex = -1;
    repaint();
}

void LaneListComponent::laneAdded(Lane* /*lane*/, int /*index*/)
{
    rebuildLaneComponents();
}

void LaneListComponent::laneRemoved(int /*index*/)
{
    rebuildLaneComponents();
}

void LaneListComponent::lanesReordered()
{
    DEBUG_LOG("LaneListComponent::lanesReordered - rebuilding components");
    rebuildLaneComponents();
}

void LaneListComponent::laneWaveformUpdated(Lane* lane)
{
    // Find and update the corresponding lane component
    for (auto& comp : laneComponents)
    {
        if (comp->getLane() == lane)
        {
            comp->waveformUpdated();
            break;
        }
    }
}

void LaneListComponent::laneDeleteRequested(LaneComponent* laneComp)
{
    if (laneComp != nullptr && laneComp->getLane() != nullptr)
    {
        projectModel.removeLane(laneComp->getLane());
    }
}

void LaneListComponent::laneDragStarted(LaneComponent* laneComp)
{
    // This is called from LaneComponent - but we now handle drag in mouseDown
    // Keep for API compatibility
    DEBUG_LOG("LaneListComponent::laneDragStarted (from LaneComponent)");
    juce::ignoreUnused(laneComp);
}

void LaneListComponent::laneDragEnded(LaneComponent* /*laneComp*/)
{
    DEBUG_LOG("LaneListComponent::laneDragEnded (from LaneComponent)");
}

void LaneListComponent::rebuildLaneComponents()
{
    DEBUG_LOG("LaneListComponent::rebuildLaneComponents - lane count: " << projectModel.getLaneCount());
    
    // Clear existing
    for (auto& comp : laneComponents)
    {
        comp->removeListener(this);
        contentComponent.removeChildComponent(comp.get());
    }
    laneComponents.clear();

    // Create new components for each lane
    for (int i = 0; i < projectModel.getLaneCount(); ++i)
    {
        Lane* lane = projectModel.getLane(i);
        auto comp = std::make_unique<LaneComponent>(lane, i);  // Pass index for display
        comp->addListener(this);
        contentComponent.addAndMakeVisible(*comp);
        laneComponents.push_back(std::move(comp));
        
        DEBUG_LOG("  Created lane component [" << i << "] for: " << lane->displayName);
    }

    updateLayout();
    repaint();
}

void LaneListComponent::updateLayout()
{
    int totalHeight = 0;
    int width = viewport.getWidth() - viewport.getScrollBarThickness() - 2;

    for (size_t i = 0; i < laneComponents.size(); ++i)
    {
        auto& comp = laneComponents[i];
        int y = static_cast<int>(i) * (kLaneHeight + kLaneSpacing);
        comp->setBounds(0, y, width, kLaneHeight);
        totalHeight = y + kLaneHeight;
    }

    // Add some padding at bottom
    totalHeight += kLaneSpacing;

    contentComponent.setSize(width, std::max(totalHeight, viewport.getHeight()));
}

int LaneListComponent::getDropIndexFromY(int y) const
{
    if (laneComponents.empty())
        return 0;

    // Find which slot the y coordinate falls into
    for (size_t i = 0; i < laneComponents.size(); ++i)
    {
        int slotY = static_cast<int>(i) * (kLaneHeight + kLaneSpacing);
        int slotCenter = slotY + kLaneHeight / 2;

        if (y < slotCenter)
            return static_cast<int>(i);
    }

    // Below all items = insert at end
    return static_cast<int>(laneComponents.size());
}

LaneComponent* LaneListComponent::findLaneComponentAt(juce::Point<int> pos)
{
    auto posInContent = contentComponent.getLocalPoint(this, pos);
    
    for (auto& comp : laneComponents)
    {
        if (comp->getBounds().contains(posInContent))
            return comp.get();
    }
    return nullptr;
}
