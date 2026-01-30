/*
    ChannelStacker - Project Model Implementation
*/

#include "ProjectModel.h"

// Debug logging macro - prints to stderr which shows in Xcode console
#define DEBUG_LOG(msg) DBG(msg)

void ProjectModel::addLane(std::unique_ptr<Lane> lane)
{
    Lane* lanePtr = lane.get();
    int index = static_cast<int>(lanes.size());
    lanes.push_back(std::move(lane));

    DEBUG_LOG("ProjectModel::addLane - added at index " << index << ", total lanes: " << lanes.size());

    listeners.call([lanePtr, index](Listener& l) { l.laneAdded(lanePtr, index); });
}

void ProjectModel::removeLane(int index)
{
    if (index < 0 || index >= static_cast<int>(lanes.size()))
    {
        DEBUG_LOG("ProjectModel::removeLane - invalid index " << index << ", size: " << lanes.size());
        return;
    }

    DEBUG_LOG("ProjectModel::removeLane - removing index " << index);
    lanes.erase(lanes.begin() + index);
    listeners.call([index](Listener& l) { l.laneRemoved(index); });
}

void ProjectModel::removeLane(Lane* lane)
{
    int index = indexOfLane(lane);
    if (index >= 0)
        removeLane(index);
}

void ProjectModel::moveLane(int fromIndex, int toIndex)
{
    int size = static_cast<int>(lanes.size());
    
    DEBUG_LOG("ProjectModel::moveLane - from " << fromIndex << " to " << toIndex << ", size: " << size);
    
    // Validate fromIndex
    if (fromIndex < 0 || fromIndex >= size)
    {
        DEBUG_LOG("  REJECTED: fromIndex out of bounds");
        return;
    }
    
    // toIndex can be 0 to size (inclusive) - size means "at the end"
    if (toIndex < 0 || toIndex > size)
    {
        DEBUG_LOG("  REJECTED: toIndex out of bounds");
        return;
    }
    
    // No-op if same position or moving to position right after current (which is same spot)
    if (fromIndex == toIndex || fromIndex + 1 == toIndex)
    {
        DEBUG_LOG("  REJECTED: no actual move needed (from=" << fromIndex << ", to=" << toIndex << ")");
        return;
    }

    // Calculate actual insertion index after removal
    // When we remove an item, all items after it shift down by 1
    int insertIndex = toIndex;
    if (toIndex > fromIndex)
    {
        insertIndex = toIndex - 1;
    }
    
    DEBUG_LOG("  Moving: remove at " << fromIndex << ", insert at " << insertIndex);

    auto lane = std::move(lanes[static_cast<size_t>(fromIndex)]);
    lanes.erase(lanes.begin() + fromIndex);
    lanes.insert(lanes.begin() + insertIndex, std::move(lane));

    DEBUG_LOG("  Move complete. New order:");
    for (size_t i = 0; i < lanes.size(); ++i)
    {
        DEBUG_LOG("    [" << i << "] " << lanes[i]->displayName);
    }

    listeners.call([](Listener& l) { l.lanesReordered(); });
}

void ProjectModel::clearAllLanes()
{
    while (!lanes.empty())
    {
        lanes.pop_back();
        listeners.call([this](Listener& l)
        {
            l.laneRemoved(static_cast<int>(lanes.size()));
        });
    }
}

Lane* ProjectModel::getLane(int index)
{
    if (index < 0 || index >= static_cast<int>(lanes.size()))
        return nullptr;
    return lanes[static_cast<size_t>(index)].get();
}

const Lane* ProjectModel::getLane(int index) const
{
    if (index < 0 || index >= static_cast<int>(lanes.size()))
        return nullptr;
    return lanes[static_cast<size_t>(index)].get();
}

std::vector<Lane*> ProjectModel::getLanes()
{
    std::vector<Lane*> result;
    result.reserve(lanes.size());
    for (auto& lane : lanes)
        result.push_back(lane.get());
    return result;
}

int ProjectModel::indexOfLane(Lane* lane) const
{
    for (size_t i = 0; i < lanes.size(); ++i)
    {
        if (lanes[i].get() == lane)
            return static_cast<int>(i);
    }
    return -1;
}

void ProjectModel::addListener(Listener* listener)
{
    listeners.add(listener);
}

void ProjectModel::removeListener(Listener* listener)
{
    listeners.remove(listener);
}

void ProjectModel::notifyWaveformUpdated(Lane* lane)
{
    listeners.call([lane](Listener& l) { l.laneWaveformUpdated(lane); });
}
