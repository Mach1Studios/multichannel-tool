/*
    ChannelStacker - FFProbe Header
    Runs ffprobe and parses audio stream metadata
*/

#pragma once

#include <juce_core/juce_core.h>
#include "FFmpegLocator.h"
#include <vector>

// Audio stream metadata
struct AudioStreamInfo
{
    int streamIndex = 0;
    int channels = 0;
    double sampleRate = 0.0;
    juce::String codec;
    juce::String channelLayout;
    double duration = 0.0;  // In seconds, may be 0 if unknown
    int64_t bitRate = 0;
};

// Result of probing a file
struct ProbeResult
{
    bool success = false;
    juce::String errorMessage;
    std::vector<AudioStreamInfo> streams;
};

class FFProbe
{
public:
    explicit FFProbe(FFmpegLocator& locator);
    ~FFProbe() = default;

    // Probe a file for audio streams
    // This is a blocking call - run from background thread
    ProbeResult getAudioStreams(const juce::File& file);

private:
    ProbeResult parseJsonOutput(const juce::String& jsonOutput);

    FFmpegLocator& locator;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FFProbe)
};
