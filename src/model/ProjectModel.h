/*
    ChannelStacker - Project Model Header
    Manages the list of lanes and their state
*/

#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <vector>
#include <memory>

// Waveform envelope data for display
struct WaveformEnvelope
{
    std::vector<float> minValues;  // Minimum values per point
    std::vector<float> maxValues;  // Maximum values per point
    int numPoints = 0;
    bool isReady = false;
};

// Represents a single audio lane/channel
struct Lane
{
    juce::File sourceFile;
    int streamIndex = 0;          // Audio stream index in the file
    int channelIndex = 0;         // Channel within the stream
    int totalChannels = 1;        // Total channels in the stream
    double sampleRate = 44100.0;
    juce::String displayName;

    WaveformEnvelope waveform;

    // Unique ID for tracking
    juce::Uuid uuid;

    Lane() : uuid(juce::Uuid()) {}
};

class ProjectModel
{
public:
    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void laneAdded(Lane* lane, int index) = 0;
        virtual void laneRemoved(int index) = 0;
        virtual void lanesReordered() = 0;
        virtual void laneWaveformUpdated(Lane* lane) = 0;
    };

    ProjectModel() = default;
    ~ProjectModel() = default;

    // Lane management
    void addLane(std::unique_ptr<Lane> lane);
    void removeLane(int index);
    void removeLane(Lane* lane);
    void moveLane(int fromIndex, int toIndex);
    void clearAllLanes();

    // Accessors
    int getLaneCount() const { return static_cast<int>(lanes.size()); }
    Lane* getLane(int index);
    const Lane* getLane(int index) const;
    std::vector<Lane*> getLanes();
    int indexOfLane(Lane* lane) const;

    // Listener management
    void addListener(Listener* listener);
    void removeListener(Listener* listener);

    // Notification helper (called by waveform extractor)
    void notifyWaveformUpdated(Lane* lane);

private:
    std::vector<std::unique_ptr<Lane>> lanes;
    juce::ListenerList<Listener> listeners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProjectModel)
};
