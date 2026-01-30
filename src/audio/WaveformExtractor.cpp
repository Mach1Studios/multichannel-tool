/*
    ChannelStacker - Waveform Extractor Implementation
*/

#include "WaveformExtractor.h"
#include <cmath>

WaveformExtractor::WaveformExtractor(FFmpegLocator& loc)
    : locator(loc)
{
}

WaveformExtractor::~WaveformExtractor()
{
    cancelAll();
}

void WaveformExtractor::extractWaveform(Lane* lane, CompletionCallback callback)
{
    if (lane == nullptr)
        return;

    // Cancel any existing job for this lane
    cancelExtraction(lane);

    auto job = std::make_unique<ExtractionJob>();
    job->lane = lane;
    job->callback = std::move(callback);

    ExtractionJob* jobPtr = job.get();
    juce::Uuid laneId = lane->uuid;

    {
        std::lock_guard<std::mutex> lock(jobsMutex);
        jobs[laneId] = std::move(job);
    }

    // Start extraction in background
    juce::Thread::launch([this, jobPtr, laneId]()
    {
        runExtraction(jobPtr);

        // Remove job when done
        std::lock_guard<std::mutex> lock(jobsMutex);
        jobs.erase(laneId);
    });
}

void WaveformExtractor::cancelExtraction(Lane* lane)
{
    if (lane == nullptr)
        return;

    std::lock_guard<std::mutex> lock(jobsMutex);
    auto it = jobs.find(lane->uuid);
    if (it != jobs.end())
    {
        it->second->cancelled = true;
        if (it->second->process)
            it->second->process->kill();
    }
}

void WaveformExtractor::cancelAll()
{
    std::lock_guard<std::mutex> lock(jobsMutex);
    for (auto& pair : jobs)
    {
        pair.second->cancelled = true;
        if (pair.second->process)
            pair.second->process->kill();
    }
}

void WaveformExtractor::runExtraction(ExtractionJob* job)
{
    if (job->cancelled)
        return;

    Lane* lane = job->lane;

    if (!locator.isFFmpegAvailable())
    {
        // Can't extract without ffmpeg
        return;
    }

    // Build ffmpeg command to decode to raw float32 PCM
    // ffmpeg -v error -nostdin -i <file> -map 0:a:<streamIndex> -f f32le -acodec pcm_f32le -
    juce::StringArray args;
    args.add(locator.getFFmpegPath().getFullPathName());
    args.add("-v");
    args.add("error");
    args.add("-nostdin");
    args.add("-i");
    args.add(lane->sourceFile.getFullPathName());
    args.add("-map");
    args.add("0:a:" + juce::String(lane->streamIndex));
    args.add("-f");
    args.add("f32le");
    args.add("-acodec");
    args.add("pcm_f32le");
    args.add("-");  // Output to stdout

    job->process = std::make_unique<juce::ChildProcess>();

    if (!job->process->start(args, juce::ChildProcess::wantStdOut))
    {
        return;
    }

    // Read audio data from stdout
    juce::MemoryBlock rawData;

    // Read in chunks
    constexpr int kBufferSize = 65536;
    juce::HeapBlock<char> buffer(kBufferSize);

    while (!job->cancelled)
    {
        int bytesRead = job->process->readProcessOutput(buffer.getData(), kBufferSize);

        if (bytesRead <= 0)
            break;

        rawData.append(buffer.getData(), static_cast<size_t>(bytesRead));

        // Limit memory usage - cap at ~500MB raw data
        if (rawData.getSize() > 500 * 1024 * 1024)
            break;
    }

    job->process->waitForProcessToFinish(5000);

    if (job->cancelled)
        return;

    // Process the raw audio data into envelope
    processAudioData(lane, rawData);

    // Call completion callback
    if (job->callback && !job->cancelled)
    {
        job->callback(lane);
    }
}

void WaveformExtractor::processAudioData(Lane* lane, const juce::MemoryBlock& rawData)
{
    // Raw data is float32 interleaved samples
    // We need to extract just our channel and compute min/max envelope

    const float* samples = static_cast<const float*>(rawData.getData());
    size_t numFloats = rawData.getSize() / sizeof(float);
    size_t numSamplesTotal = numFloats / static_cast<size_t>(lane->totalChannels);

    if (numSamplesTotal == 0)
        return;

    // Target envelope resolution
    int numPoints = kDefaultEnvelopePoints;

    // Samples per envelope point
    size_t samplesPerPoint = std::max<size_t>(1, numSamplesTotal / static_cast<size_t>(numPoints));

    // Adjust numPoints based on actual data
    numPoints = static_cast<int>((numSamplesTotal + samplesPerPoint - 1) / samplesPerPoint);

    // Initialize envelope
    WaveformEnvelope& envelope = lane->waveform;
    envelope.minValues.clear();
    envelope.maxValues.clear();
    envelope.minValues.resize(static_cast<size_t>(numPoints), 0.0f);
    envelope.maxValues.resize(static_cast<size_t>(numPoints), 0.0f);
    envelope.numPoints = numPoints;

    int channelIndex = lane->channelIndex;
    int totalChannels = lane->totalChannels;

    // Process each envelope point
    for (int point = 0; point < numPoints; ++point)
    {
        size_t startSample = static_cast<size_t>(point) * samplesPerPoint;
        size_t endSample = std::min(startSample + samplesPerPoint, numSamplesTotal);

        float minVal = 0.0f;
        float maxVal = 0.0f;

        for (size_t s = startSample; s < endSample; ++s)
        {
            // Get sample for our channel (interleaved)
            size_t idx = s * static_cast<size_t>(totalChannels) + static_cast<size_t>(channelIndex);
            if (idx < numFloats)
            {
                float sample = samples[idx];
                minVal = std::min(minVal, sample);
                maxVal = std::max(maxVal, sample);
            }
        }

        envelope.minValues[static_cast<size_t>(point)] = minVal;
        envelope.maxValues[static_cast<size_t>(point)] = maxVal;
    }

    envelope.isReady = true;
}
