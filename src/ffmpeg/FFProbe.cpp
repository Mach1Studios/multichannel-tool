/*
    ChannelStacker - FFProbe Implementation
*/

#include "FFProbe.h"

FFProbe::FFProbe(FFmpegLocator& loc)
    : locator(loc)
{
}

ProbeResult FFProbe::getAudioStreams(const juce::File& file)
{
    ProbeResult result;

    if (!locator.isFFprobeAvailable())
    {
        result.errorMessage = "ffprobe not found. Please install FFmpeg.";
        return result;
    }

    if (!file.existsAsFile())
    {
        result.errorMessage = "File not found: " + file.getFullPathName();
        return result;
    }

    // Build ffprobe command
    // ffprobe -v error -select_streams a -show_streams -of json <file>
    juce::StringArray args;
    args.add(locator.getFFprobePath().getFullPathName());
    args.add("-v");
    args.add("error");
    args.add("-select_streams");
    args.add("a");
    args.add("-show_streams");
    args.add("-of");
    args.add("json");
    args.add(file.getFullPathName());

    juce::ChildProcess process;
    if (!process.start(args))
    {
        result.errorMessage = "Failed to start ffprobe process";
        return result;
    }

    // Read output
    juce::String output = process.readAllProcessOutput();
    process.waitForProcessToFinish(30000);  // 30 second timeout

    auto exitCode = process.getExitCode();
    if (exitCode != 0)
    {
        result.errorMessage = "ffprobe returned error code " + juce::String(static_cast<int>(exitCode));
        if (output.isNotEmpty())
            result.errorMessage += ": " + output;
        return result;
    }

    return parseJsonOutput(output);
}

ProbeResult FFProbe::parseJsonOutput(const juce::String& jsonOutput)
{
    ProbeResult result;

    // Parse JSON using JUCE's JSON parser
    auto parsed = juce::JSON::parse(jsonOutput);

    if (parsed.isVoid())
    {
        result.errorMessage = "Failed to parse ffprobe JSON output";
        return result;
    }

    // Get streams array
    auto* obj = parsed.getDynamicObject();
    if (obj == nullptr)
    {
        result.errorMessage = "Invalid JSON structure from ffprobe";
        return result;
    }

    auto streamsVar = obj->getProperty("streams");
    auto* streamsArray = streamsVar.getArray();

    if (streamsArray == nullptr)
    {
        // No streams array might mean no audio streams found
        result.success = true;
        return result;
    }

    // Process each stream
    for (const auto& streamVar : *streamsArray)
    {
        auto* streamObj = streamVar.getDynamicObject();
        if (streamObj == nullptr)
            continue;

        AudioStreamInfo info;

        // Get stream index
        info.streamIndex = static_cast<int>(streamObj->getProperty("index"));

        // Get channel count
        info.channels = static_cast<int>(streamObj->getProperty("channels"));

        // Get sample rate (comes as string)
        auto sampleRateStr = streamObj->getProperty("sample_rate").toString();
        info.sampleRate = sampleRateStr.getDoubleValue();

        // Get codec name
        info.codec = streamObj->getProperty("codec_name").toString();

        // Get channel layout
        info.channelLayout = streamObj->getProperty("channel_layout").toString();

        // Get duration (may be in different places)
        auto durationStr = streamObj->getProperty("duration").toString();
        if (durationStr.isEmpty())
        {
            // Try tags
            auto tagsVar = streamObj->getProperty("tags");
            if (auto* tagsObj = tagsVar.getDynamicObject())
            {
                durationStr = tagsObj->getProperty("DURATION").toString();
            }
        }
        info.duration = durationStr.getDoubleValue();

        // Get bit rate
        auto bitRateStr = streamObj->getProperty("bit_rate").toString();
        info.bitRate = bitRateStr.getLargeIntValue();

        result.streams.push_back(info);
    }

    result.success = true;
    return result;
}
