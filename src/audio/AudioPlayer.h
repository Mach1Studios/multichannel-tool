/*
    AudioPlayer.h
    -------------
    Audio playback using ffmpeg for decoding and JUCE for output.
    Mixes all lanes to stereo for auditioning.
*/

#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include "../model/ProjectModel.h"
#include "../ffmpeg/FFmpegLocator.h"

class AudioPlayer : public juce::AudioSource
{
public:
    enum class LoadState
    {
        Empty,      // No lanes loaded
        Loading,    // Currently decoding audio
        Ready,      // Audio loaded and ready to play
        Error       // Loading failed
    };

    AudioPlayer(FFmpegLocator& locator);
    ~AudioPlayer() override;

    // Setup audio device
    bool initialize();
    void shutdown();

    // Playback control
    void loadLanes(const std::vector<Lane*>& lanes);
    void play();
    void stop();
    bool isPlaying() const { return playing; }
    
    // Loading state
    LoadState getLoadState() const { return loadState.load(); }
    bool isReady() const { return loadState.load() == LoadState::Ready; }
    bool isLoading() const { return loadState.load() == LoadState::Loading; }

    // AudioSource interface
    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void releaseResources() override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;

    // Listener for playback state changes
    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void playbackStarted() = 0;
        virtual void playbackStopped() = 0;
        virtual void playbackPositionChanged(double positionSeconds) = 0;
        virtual void loadStateChanged(LoadState newState) = 0;
    };

    void addListener(Listener* listener);
    void removeListener(Listener* listener);

private:
    // Info needed for decoding (copied from lanes to avoid threading issues)
    struct DecodeInfo
    {
        juce::String sourceFilePath;  // Store path as string, not File
        int streamIndex = 0;
        int channelIndex = 0;
        int totalChannels = 0;
        double sampleRate = 48000.0;
    };

    void decodeAudioAsync(std::vector<DecodeInfo> infos, juce::String ffmpeg, int generation);
    void setLoadState(LoadState newState);
    void onDecodeComplete(juce::AudioBuffer<float> buffer, double sampleRate, int generation);
    void onDecodeError(int generation);

    FFmpegLocator& ffmpegLocator;
    juce::AudioDeviceManager deviceManager;
    juce::AudioSourcePlayer audioSourcePlayer;

    // Decoded audio buffer (stereo)
    juce::AudioBuffer<float> audioBuffer;
    int readPosition = 0;
    std::atomic<bool> playing{ false };
    std::atomic<LoadState> loadState{ LoadState::Empty };
    std::atomic<int> loadGeneration{ 0 };  // Incremented on each load to cancel stale decodes
    std::atomic<bool> shuttingDown{ false };  // Flag to prevent callbacks during shutdown

    double currentSampleRate = 48000.0;

    juce::ListenerList<Listener> listeners;
    juce::CriticalSection lock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPlayer)
};
