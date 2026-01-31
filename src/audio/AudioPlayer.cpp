/*
    AudioPlayer.cpp
    ---------------
    Audio playback implementation using ffmpeg decode + JUCE output.
*/

#include "AudioPlayer.h"

AudioPlayer::AudioPlayer(FFmpegLocator& locator)
    : ffmpegLocator(locator)
{
}

AudioPlayer::~AudioPlayer()
{
    shutdown();
}

bool AudioPlayer::initialize()
{
    // Initialize audio device with default stereo output
    auto result = deviceManager.initialiseWithDefaultDevices(0, 2);
    
    if (result.isNotEmpty())
    {
        DBG("AudioPlayer: Failed to initialize audio device: " + result);
        return false;
    }

    audioSourcePlayer.setSource(this);
    deviceManager.addAudioCallback(&audioSourcePlayer);
    
    DBG("AudioPlayer: Initialized successfully");
    return true;
}

void AudioPlayer::shutdown()
{
    stop();
    deviceManager.removeAudioCallback(&audioSourcePlayer);
    audioSourcePlayer.setSource(nullptr);
    deviceManager.closeAudioDevice();
}

void AudioPlayer::loadLanes(const std::vector<Lane*>& lanes)
{
    stop();
    
    juce::ScopedLock sl(lock);
    currentLanes = lanes;
    audioLoaded = false;
    readPosition = 0;
    
    if (lanes.empty())
    {
        audioBuffer.setSize(2, 0);
        return;
    }

    // Decode audio in background
    decodeAudioAsync();
}

void AudioPlayer::decodeAudioAsync()
{
    if (currentLanes.empty())
        return;

    auto lanes = currentLanes;
    auto ffmpegPath = ffmpegLocator.getFFmpegPath().getFullPathName();

    juce::Thread::launch([this, lanes, ffmpegPath]()
    {
        // For simplicity, decode the first lane's source file
        // In the future, could mix multiple sources
        if (lanes.empty())
            return;

        auto* firstLane = lanes[0];
        int numSourceChannels = firstLane->totalChannels;
        double sampleRate = firstLane->sampleRate > 0 ? firstLane->sampleRate : 48000.0;

        // Build ffmpeg command to decode to raw float32
        juce::StringArray args;
        args.add(ffmpegPath);
        args.add("-v");
        args.add("error");
        args.add("-i");
        args.add(firstLane->sourceFile.getFullPathName());
        args.add("-map");
        args.add("0:a:" + juce::String(firstLane->streamIndex));
        args.add("-f");
        args.add("f32le");
        args.add("-acodec");
        args.add("pcm_f32le");
        args.add("-ar");
        args.add(juce::String(static_cast<int>(sampleRate)));
        args.add("-");

        juce::ChildProcess process;
        if (!process.start(args, juce::ChildProcess::wantStdOut))
        {
            DBG("AudioPlayer: Failed to start ffmpeg decode");
            return;
        }

        // Read decoded PCM data
        juce::MemoryBlock rawData;
        const int chunkSize = 65536;
        juce::HeapBlock<char> buffer(chunkSize);

        while (process.isRunning())
        {
            int bytesRead = process.readProcessOutput(buffer, chunkSize);
            if (bytesRead > 0)
                rawData.append(buffer, static_cast<size_t>(bytesRead));
            else
                juce::Thread::sleep(1);
        }

        // Read remaining data
        int bytesRead;
        while ((bytesRead = process.readProcessOutput(buffer, chunkSize)) > 0)
            rawData.append(buffer, static_cast<size_t>(bytesRead));

        process.waitForProcessToFinish(5000);

        if (rawData.getSize() == 0)
        {
            DBG("AudioPlayer: No audio data decoded");
            return;
        }

        // Convert raw data to audio buffer
        int numSamples = static_cast<int>(rawData.getSize() / (sizeof(float) * static_cast<size_t>(numSourceChannels)));
        
        DBG("AudioPlayer: Decoded " + juce::String(numSamples) + " samples, " + 
            juce::String(numSourceChannels) + " channels");

        // Create stereo mix based on lane order
        juce::AudioBuffer<float> tempBuffer(numSourceChannels, numSamples);
        const float* rawPtr = static_cast<const float*>(rawData.getData());

        // De-interleave
        for (int s = 0; s < numSamples; ++s)
        {
            for (int ch = 0; ch < numSourceChannels; ++ch)
            {
                tempBuffer.setSample(ch, s, rawPtr[s * numSourceChannels + ch]);
            }
        }

        // Mix to stereo based on lane configuration
        // Map lanes to stereo: odd lanes (0,2,4...) to left, even lanes (1,3,5...) to right
        // Or if only a few lanes, pan them across stereo field
        juce::ScopedLock sl(lock);
        audioBuffer.setSize(2, numSamples);
        audioBuffer.clear();

        int numLanes = static_cast<int>(lanes.size());
        
        for (int i = 0; i < numLanes; ++i)
        {
            int srcChannel = lanes[static_cast<size_t>(i)]->channelIndex;
            if (srcChannel >= numSourceChannels)
                continue;

            // Calculate stereo pan position based on lane index
            float pan = (numLanes > 1) ? static_cast<float>(i) / static_cast<float>(numLanes - 1) : 0.5f;
            float leftGain = std::cos(pan * juce::MathConstants<float>::halfPi);
            float rightGain = std::sin(pan * juce::MathConstants<float>::halfPi);

            // Mix this channel into stereo output
            for (int s = 0; s < numSamples; ++s)
            {
                float sample = tempBuffer.getSample(srcChannel, s);
                audioBuffer.addSample(0, s, sample * leftGain);
                audioBuffer.addSample(1, s, sample * rightGain);
            }
        }

        // Normalize if needed
        float maxLevel = audioBuffer.getMagnitude(0, numSamples);
        if (maxLevel > 1.0f)
        {
            audioBuffer.applyGain(0.9f / maxLevel);
        }

        currentSampleRate = sampleRate;
        audioLoaded = true;
        readPosition = 0;

        DBG("AudioPlayer: Audio loaded and ready for playback");
    });
}

void AudioPlayer::play()
{
    if (!audioLoaded || audioBuffer.getNumSamples() == 0)
    {
        DBG("AudioPlayer: No audio loaded");
        return;
    }

    readPosition = 0;
    playing = true;
    
    listeners.call([](Listener& l) { l.playbackStarted(); });
    DBG("AudioPlayer: Playback started");
}

void AudioPlayer::stop()
{
    if (playing)
    {
        playing = false;
        listeners.call([](Listener& l) { l.playbackStopped(); });
        DBG("AudioPlayer: Playback stopped");
    }
}

void AudioPlayer::prepareToPlay(int /*samplesPerBlockExpected*/, double /*sampleRate*/)
{
    // Audio is pre-decoded, nothing to prepare
}

void AudioPlayer::releaseResources()
{
    // Nothing to release
}

void AudioPlayer::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    bufferToFill.clearActiveBufferRegion();

    if (!playing || !audioLoaded)
        return;

    juce::ScopedLock sl(lock);
    
    int numSamples = bufferToFill.numSamples;
    int availableSamples = audioBuffer.getNumSamples() - readPosition;
    int samplesToRead = std::min(numSamples, availableSamples);

    if (samplesToRead <= 0)
    {
        // End of audio
        juce::MessageManager::callAsync([this]()
        {
            stop();
        });
        return;
    }

    // Copy audio to output buffer
    for (int ch = 0; ch < bufferToFill.buffer->getNumChannels() && ch < 2; ++ch)
    {
        bufferToFill.buffer->copyFrom(
            ch,
            bufferToFill.startSample,
            audioBuffer,
            ch,
            readPosition,
            samplesToRead);
    }

    readPosition += samplesToRead;

    // Notify position change
    double positionSec = static_cast<double>(readPosition) / currentSampleRate;
    listeners.call([positionSec](Listener& l) { l.playbackPositionChanged(positionSec); });
}

void AudioPlayer::addListener(Listener* listener)
{
    listeners.add(listener);
}

void AudioPlayer::removeListener(Listener* listener)
{
    listeners.remove(listener);
}
