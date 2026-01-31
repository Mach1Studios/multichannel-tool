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
    };

    void addListener(Listener* listener);
    void removeListener(Listener* listener);

private:
    void decodeAudioAsync();
    void mixToStereo(juce::AudioBuffer<float>& stereoBuffer);

    FFmpegLocator& ffmpegLocator;
    juce::AudioDeviceManager deviceManager;
    juce::AudioSourcePlayer audioSourcePlayer;

    // Decoded audio buffer (interleaved stereo)
    juce::AudioBuffer<float> audioBuffer;
    int readPosition = 0;
    std::atomic<bool> playing{ false };
    std::atomic<bool> audioLoaded{ false };

    // Current lanes info
    std::vector<Lane*> currentLanes;
    double currentSampleRate = 48000.0;

    juce::ListenerList<Listener> listeners;
    juce::CriticalSection lock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioPlayer)
};
