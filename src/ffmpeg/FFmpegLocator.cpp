/*
    ChannelStacker - FFmpeg Locator Implementation
*/

#include "FFmpegLocator.h"

FFmpegLocator::FFmpegLocator()
{
    refresh();
}

void FFmpegLocator::refresh()
{
    ffmpegPath = findExecutable("ffmpeg");
    ffprobePath = findExecutable("ffprobe");
}

void FFmpegLocator::setFFmpegPath(const juce::File& path)
{
    if (path.existsAsFile())
        ffmpegPath = path;
}

void FFmpegLocator::setFFprobePath(const juce::File& path)
{
    if (path.existsAsFile())
        ffprobePath = path;
}

juce::File FFmpegLocator::findExecutable(const juce::String& name)
{
    // First, try PATH
    auto found = searchPath(name);
    if (found.existsAsFile())
        return found;

    // Try common locations on different platforms
#if JUCE_MAC
    // Homebrew locations
    juce::File homebrewIntel("/usr/local/bin/" + name);
    if (homebrewIntel.existsAsFile())
        return homebrewIntel;

    juce::File homebrewArm("/opt/homebrew/bin/" + name);
    if (homebrewArm.existsAsFile())
        return homebrewArm;

    // MacPorts
    juce::File macPorts("/opt/local/bin/" + name);
    if (macPorts.existsAsFile())
        return macPorts;

#elif JUCE_WINDOWS
    // Common Windows locations
    juce::File programFiles(juce::File::getSpecialLocation(
        juce::File::globalApplicationsDirectory).getChildFile("ffmpeg/bin/" + name + ".exe"));
    if (programFiles.existsAsFile())
        return programFiles;

    // User's download location
    juce::File userDownload(juce::File::getSpecialLocation(
        juce::File::userHomeDirectory).getChildFile("ffmpeg/bin/" + name + ".exe"));
    if (userDownload.existsAsFile())
        return userDownload;

#elif JUCE_LINUX
    // Standard locations
    juce::File usrBin("/usr/bin/" + name);
    if (usrBin.existsAsFile())
        return usrBin;

    juce::File usrLocalBin("/usr/local/bin/" + name);
    if (usrLocalBin.existsAsFile())
        return usrLocalBin;
#endif

    return {};
}

juce::File FFmpegLocator::searchPath(const juce::String& name)
{
    auto pathEnv = juce::SystemStats::getEnvironmentVariable("PATH", "");

#if JUCE_WINDOWS
    auto separator = ";";
    auto extension = ".exe";
#else
    auto separator = ":";
    auto extension = "";
#endif

    juce::StringArray paths;
    paths.addTokens(pathEnv, separator, "");

    for (const auto& dir : paths)
    {
        juce::File candidate(dir + "/" + name + extension);
        if (candidate.existsAsFile())
            return candidate;
    }

    return {};
}

juce::String FFmpegLocator::getFFmpegVersion() const
{
    if (!isFFmpegAvailable())
        return {};

    juce::ChildProcess process;
    juce::StringArray args;
    args.add(ffmpegPath.getFullPathName());
    args.add("-version");

    if (process.start(args))
    {
        auto output = process.readAllProcessOutput();
        process.waitForProcessToFinish(5000);

        // Extract first line
        auto lines = juce::StringArray::fromLines(output);
        if (!lines.isEmpty())
            return lines[0];
    }

    return {};
}

juce::String FFmpegLocator::getFFprobeVersion() const
{
    if (!isFFprobeAvailable())
        return {};

    juce::ChildProcess process;
    juce::StringArray args;
    args.add(ffprobePath.getFullPathName());
    args.add("-version");

    if (process.start(args))
    {
        auto output = process.readAllProcessOutput();
        process.waitForProcessToFinish(5000);

        // Extract first line
        auto lines = juce::StringArray::fromLines(output);
        if (!lines.isEmpty())
            return lines[0];
    }

    return {};
}
