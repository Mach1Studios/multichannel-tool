/*
    ChannelStacker - Waveform Extractor Header
    Decodes audio via ffmpeg and computes waveform envelopes
*/

#pragma once

#include <juce_core/juce_core.h>
#include "../ffmpeg/FFmpegLocator.h"
#include "../model/ProjectModel.h"
#include <functional>
#include <map>
#include <mutex>

class WaveformExtractor
{
public:
    using CompletionCallback = std::function<void(Lane*)>;

    explicit WaveformExtractor(FFmpegLocator& locator);
    ~WaveformExtractor();

    // Start extracting waveform for a lane (async)
    void extractWaveform(Lane* lane, CompletionCallback callback);

    // Cancel extraction for a specific lane
    void cancelExtraction(Lane* lane);

    // Cancel all extractions
    void cancelAll();

    // Default envelope resolution
    static constexpr int kDefaultEnvelopePoints = 4000;

private:
    struct ExtractionJob
    {
        Lane* lane = nullptr;
        CompletionCallback callback;
        std::unique_ptr<juce::ChildProcess> process;
        std::atomic<bool> cancelled{ false };
    };

    void runExtraction(ExtractionJob* job);
    void processAudioData(Lane* lane, const juce::MemoryBlock& rawData);

    FFmpegLocator& locator;

    std::mutex jobsMutex;
    std::map<juce::Uuid, std::unique_ptr<ExtractionJob>> jobs;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformExtractor)
};
