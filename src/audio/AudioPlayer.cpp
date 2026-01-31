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
    // Set shutdown flag to prevent any more callbacks
    shuttingDown = true;
    
    // Increment generation to signal any running decodes to stop
    loadGeneration++;
    
    shutdown();
    
    // Give any background threads a moment to notice the shutdown
    juce::Thread::sleep(100);
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
    loadGeneration++;  // Cancel any pending decodes
    deviceManager.removeAudioCallback(&audioSourcePlayer);
    audioSourcePlayer.setSource(nullptr);
    deviceManager.closeAudioDevice();
}

void AudioPlayer::loadLanes(const std::vector<Lane*>& lanes)
{
    stop();
    
    // Increment generation to cancel any in-progress decode
    // Old threads will check this and exit gracefully
    int newGeneration = ++loadGeneration;
    
    if (lanes.empty())
    {
        juce::ScopedLock sl(lock);
        audioBuffer.setSize(2, 0);
        readPosition = 0;
        setLoadState(LoadState::Empty);
        return;
    }

    // Copy lane info to avoid accessing Lane pointers from background thread
    std::vector<DecodeInfo> decodeInfos;
    decodeInfos.reserve(lanes.size());
    
    for (auto* lane : lanes)
    {
        DecodeInfo info;
        info.sourceFilePath = lane->sourceFile.getFullPathName();
        info.streamIndex = lane->streamIndex;
        info.channelIndex = lane->channelIndex;
        info.totalChannels = lane->totalChannels;
        info.sampleRate = lane->sampleRate;
        decodeInfos.push_back(info);
    }

    setLoadState(LoadState::Loading);
    
    // Get ffmpeg path as string before starting thread
    juce::String ffmpegPath = ffmpegLocator.getFFmpegPath().getFullPathName();
    
    // Start decode in background - thread manages its own lifetime
    decodeAudioAsync(std::move(decodeInfos), ffmpegPath, newGeneration);
}

void AudioPlayer::decodeAudioAsync(std::vector<DecodeInfo> infos, juce::String ffmpeg, int generation)
{
    // Use Thread::launch which manages its own lifetime
    // The thread will check the generation counter to know if it should stop
    juce::Thread::launch([this, decodeInfos = std::move(infos), ffmpegPath = std::move(ffmpeg), myGeneration = generation]()
    {
        // Check if this decode has been superseded or we're shutting down
        if (shuttingDown || loadGeneration != myGeneration)
        {
            DBG("AudioPlayer: Decode cancelled at start");
            return;
        }

        if (decodeInfos.empty())
        {
            onDecodeError(myGeneration);
            return;
        }

        const auto& firstInfo = decodeInfos[0];
        int numSourceChannels = firstInfo.totalChannels;
        double sampleRate = firstInfo.sampleRate > 0 ? firstInfo.sampleRate : 48000.0;

        // Build ffmpeg command to decode to raw float32
        juce::StringArray args;
        args.add(ffmpegPath);
        args.add("-v");
        args.add("error");
        args.add("-nostdin");
        args.add("-i");
        args.add(firstInfo.sourceFilePath);
        args.add("-map");
        args.add("0:a:" + juce::String(firstInfo.streamIndex));
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
            if (!shuttingDown)
                onDecodeError(myGeneration);
            return;
        }

        // Read decoded PCM data
        juce::MemoryBlock rawData;
        const int chunkSize = 65536;
        juce::HeapBlock<char> buffer(chunkSize);

        while (process.isRunning())
        {
            // Check for cancellation periodically
            if (shuttingDown || loadGeneration != myGeneration)
            {
                process.kill();
                DBG("AudioPlayer: Decode cancelled during read");
                return;
            }
            
            int bytesRead = process.readProcessOutput(buffer, chunkSize);
            if (bytesRead > 0)
                rawData.append(buffer, static_cast<size_t>(bytesRead));
            else
                juce::Thread::sleep(1);
        }

        // Read remaining data
        int bytesRead;
        while ((bytesRead = process.readProcessOutput(buffer, chunkSize)) > 0)
        {
            rawData.append(buffer, static_cast<size_t>(bytesRead));
            
            if (shuttingDown || loadGeneration != myGeneration)
                return;
        }

        process.waitForProcessToFinish(5000);

        // Final cancellation check
        if (shuttingDown || loadGeneration != myGeneration)
        {
            DBG("AudioPlayer: Decode cancelled after completion");
            return;
        }

        if (rawData.getSize() == 0)
        {
            DBG("AudioPlayer: No audio data decoded");
            if (!shuttingDown)
                onDecodeError(myGeneration);
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
            if (shuttingDown || loadGeneration != myGeneration)
                return;
                
            for (int ch = 0; ch < numSourceChannels; ++ch)
            {
                tempBuffer.setSample(ch, s, rawPtr[s * numSourceChannels + ch]);
            }
        }

        // Mix to stereo based on lane configuration
        juce::AudioBuffer<float> stereoBuffer(2, numSamples);
        stereoBuffer.clear();

        int numLanes = static_cast<int>(decodeInfos.size());
        
        for (int i = 0; i < numLanes; ++i)
        {
            if (shuttingDown || loadGeneration != myGeneration)
                return;
                
            int srcChannel = decodeInfos[static_cast<size_t>(i)].channelIndex;
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
                stereoBuffer.addSample(0, s, sample * leftGain);
                stereoBuffer.addSample(1, s, sample * rightGain);
            }
        }

        // Normalize if needed
        float maxLevel = stereoBuffer.getMagnitude(0, numSamples);
        if (maxLevel > 1.0f)
        {
            stereoBuffer.applyGain(0.9f / maxLevel);
        }

        // Final cancellation check before updating shared state
        if (shuttingDown || loadGeneration != myGeneration)
        {
            DBG("AudioPlayer: Decode cancelled before buffer swap");
            return;
        }

        // Send result to main thread
        onDecodeComplete(std::move(stereoBuffer), sampleRate, myGeneration);
    });
}

void AudioPlayer::setLoadState(LoadState newState)
{
    loadState = newState;
    
    if (shuttingDown)
        return;
    
    // Notify listeners on message thread
    juce::MessageManager::callAsync([this, newState]()
    {
        if (!shuttingDown)
            listeners.call([newState](Listener& l) { l.loadStateChanged(newState); });
    });
}

void AudioPlayer::onDecodeComplete(juce::AudioBuffer<float> decodedBuffer, double decodedSampleRate, int generation)
{
    if (shuttingDown)
        return;
        
    // Post to message thread
    juce::MessageManager::callAsync([this, buf = std::move(decodedBuffer), sr = decodedSampleRate, gen = generation]() mutable
    {
        if (shuttingDown || loadGeneration != gen)
        {
            DBG("AudioPlayer: Ignoring stale decode result");
            return;
        }
        
        {
            juce::ScopedLock sl(lock);
            audioBuffer = std::move(buf);
            currentSampleRate = sr;
            readPosition = 0;
        }
        
        setLoadState(LoadState::Ready);
        DBG("AudioPlayer: Audio loaded and ready for playback");
    });
}

void AudioPlayer::onDecodeError(int generation)
{
    if (shuttingDown)
        return;
        
    juce::MessageManager::callAsync([this, generation]()
    {
        if (!shuttingDown && loadGeneration == generation)
            setLoadState(LoadState::Error);
    });
}

void AudioPlayer::play()
{
    if (loadState != LoadState::Ready)
    {
        DBG("AudioPlayer: Cannot play - audio not ready");
        return;
    }
    
    {
        juce::ScopedLock sl(lock);
        if (audioBuffer.getNumSamples() == 0)
        {
            DBG("AudioPlayer: Cannot play - buffer empty");
            return;
        }
        readPosition = 0;
    }
    
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

    if (!playing || loadState != LoadState::Ready)
        return;

    juce::ScopedLock sl(lock);
    
    if (audioBuffer.getNumSamples() == 0)
        return;
    
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
