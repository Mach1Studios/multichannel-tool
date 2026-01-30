/*
    ChannelStacker - FFmpeg Locator Header
    Finds ffmpeg/ffprobe executables on PATH or custom location
*/

#pragma once

#include <juce_core/juce_core.h>

class FFmpegLocator
{
public:
    FFmpegLocator();
    ~FFmpegLocator() = default;

    // Get paths to executables
    juce::File getFFmpegPath() const { return ffmpegPath; }
    juce::File getFFprobePath() const { return ffprobePath; }

    // Check if tools are available
    bool isFFmpegAvailable() const { return ffmpegPath.existsAsFile(); }
    bool isFFprobeAvailable() const { return ffprobePath.existsAsFile(); }

    // Override paths manually
    void setFFmpegPath(const juce::File& path);
    void setFFprobePath(const juce::File& path);

    // Refresh search
    void refresh();

    // Get version strings (empty if not available)
    juce::String getFFmpegVersion() const;
    juce::String getFFprobeVersion() const;

private:
    juce::File findExecutable(const juce::String& name);
    juce::File searchPath(const juce::String& name);

    juce::File ffmpegPath;
    juce::File ffprobePath;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FFmpegLocator)
};
