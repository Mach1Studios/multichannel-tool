/*
    ChannelStacker - Main Component Implementation
*/

#include "MainComponent.h"
#include "ui/Mach1LookAndFeel.h"

MainComponent::MainComponent()
{
    // Initialize FFmpeg tools
    ffprobe = std::make_unique<FFProbe>(ffmpegLocator);
    waveformExtractor = std::make_unique<WaveformExtractor>(ffmpegLocator);

    // Register as listener
    projectModel.addListener(this);

    // Setup lane list
    laneListComponent = std::make_unique<LaneListComponent>(projectModel);
    addAndMakeVisible(*laneListComponent);

    // Setup export button with Mach1 style
    exportButton.setColour(juce::TextButton::buttonColourId, Mach1LookAndFeel::Colors::buttonOff);
    exportButton.setColour(juce::TextButton::textColourOffId, Mach1LookAndFeel::Colors::textPrimary);
    exportButton.onClick = [this]() { showExportDialog(); };
    addAndMakeVisible(exportButton);

    // Setup clear button
    clearButton.setColour(juce::TextButton::buttonColourId, Mach1LookAndFeel::Colors::buttonOff);
    clearButton.setColour(juce::TextButton::textColourOffId, Mach1LookAndFeel::Colors::textPrimary);
    clearButton.onClick = [this]()
    {
        projectModel.clearAllLanes();
        updateStatus("All lanes cleared");
    };
    addAndMakeVisible(clearButton);

    // Setup status label
    statusLabel.setJustificationType(juce::Justification::centredLeft);
    statusLabel.setColour(juce::Label::textColourId, Mach1LookAndFeel::Colors::textSecondary);
    statusLabel.setFont(juce::FontOptions(11.0f));
    addAndMakeVisible(statusLabel);

    updateStatus("Drop audio/video files here to add channels");

    setSize(1000, 700);
}

MainComponent::~MainComponent()
{
    projectModel.removeListener(this);
    waveformExtractor->cancelAll();
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(Mach1LookAndFeel::Colors::background);

    // Draw drop zone indicator
    auto dropZone = getLocalBounds().reduced(10).withTrimmedTop(kToolbarHeight);

    if (projectModel.getLaneCount() == 0)
    {
        // Show drop hint when empty - Mach1 style
        g.setColour(isDragOver ? Mach1LookAndFeel::Colors::accent.withAlpha(0.2f)
                               : Mach1LookAndFeel::Colors::panelBackground);
        g.fillRoundedRectangle(dropZone.toFloat(), 2.0f);

        g.setColour(isDragOver ? Mach1LookAndFeel::Colors::accent 
                               : Mach1LookAndFeel::Colors::border);
        g.drawRoundedRectangle(dropZone.toFloat(), 2.0f, 1.0f);

        g.setColour(Mach1LookAndFeel::Colors::textSecondary);
        g.setFont(juce::FontOptions(14.0f));
        g.drawText("Drop audio/video files here",
                   dropZone, juce::Justification::centred);
    }
    else if (isDragOver)
    {
        // Show drag overlay when dragging over existing content
        g.setColour(Mach1LookAndFeel::Colors::accent.withAlpha(0.1f));
        g.fillAll();
    }
}

void MainComponent::resized()
{
    auto bounds = getLocalBounds();

    // Toolbar at top
    auto toolbar = bounds.removeFromTop(kToolbarHeight);
    toolbar.reduce(10, 10);

    exportButton.setBounds(toolbar.removeFromLeft(100));
    toolbar.removeFromLeft(10);
    clearButton.setBounds(toolbar.removeFromLeft(100));
    toolbar.removeFromLeft(20);
    statusLabel.setBounds(toolbar);

    // Lane list fills the rest
    bounds.reduce(10, 0);
    bounds.removeFromBottom(10);
    laneListComponent->setBounds(bounds);
}

bool MainComponent::isInterestedInFileDrag(const juce::StringArray& files)
{
    // Accept any files - we'll check for audio streams later
    return !files.isEmpty();
}

void MainComponent::filesDropped(const juce::StringArray& files, int /*x*/, int /*y*/)
{
    isDragOver = false;
    repaint();

    for (const auto& path : files)
    {
        handleDroppedFile(juce::File(path));
    }
}

void MainComponent::fileDragEnter(const juce::StringArray& /*files*/, int /*x*/, int /*y*/)
{
    isDragOver = true;
    repaint();
}

void MainComponent::fileDragExit(const juce::StringArray& /*files*/)
{
    isDragOver = false;
    repaint();
}

void MainComponent::handleDroppedFile(const juce::File& file)
{
    updateStatus("Analyzing: " + file.getFileName());

    // Run ffprobe in a background thread
    auto* probe = ffprobe.get();
    auto* extractor = waveformExtractor.get();
    auto* model = &projectModel;

    juce::Thread::launch([this, file, probe, extractor, model]()
    {
        auto result = probe->getAudioStreams(file);

        juce::MessageManager::callAsync([this, file, result, extractor, model]()
        {
            if (!result.success)
            {
                updateStatus("Error: " + result.errorMessage);
                return;
            }

            if (result.streams.empty())
            {
                updateStatus("No audio streams found in: " + file.getFileName());
                return;
            }

            // Use first audio stream (structured for future stream selection dialog)
            const auto& stream = result.streams[0];

            updateStatus(juce::String::formatted(
                "Found %d channel(s) in stream %d of %s",
                stream.channels, stream.streamIndex, file.getFileName().toRawUTF8()));

            // Create a lane for each channel
            for (int ch = 0; ch < stream.channels; ++ch)
            {
                auto lane = std::make_unique<Lane>();
                lane->sourceFile = file;
                lane->streamIndex = stream.streamIndex;
                lane->channelIndex = ch;
                lane->totalChannels = stream.channels;
                lane->sampleRate = stream.sampleRate;
                lane->displayName = file.getFileNameWithoutExtension()
                    + " [" + juce::String(stream.streamIndex)
                    + ":" + juce::String(ch) + "]";

                Lane* lanePtr = lane.get();
                model->addLane(std::move(lane));

                // Start waveform extraction
                extractor->extractWaveform(lanePtr, [model](Lane* l)
                {
                    juce::MessageManager::callAsync([model, l]()
                    {
                        model->notifyWaveformUpdated(l);
                    });
                });
            }
        });
    });
}

void MainComponent::showExportDialog()
{
    if (projectModel.getLaneCount() == 0)
    {
        updateStatus("Nothing to export - add some files first");
        return;
    }

    auto* alertWindow = new juce::AlertWindow("Export Options",
                                               "Choose export format:",
                                               juce::MessageBoxIconType::QuestionIcon);

    alertWindow->addButton("Single Multichannel WAV", 1);
    alertWindow->addButton("Multiple Mono WAVs", 2);
    alertWindow->addButton("Stereo Pairs", 3);
    alertWindow->addButton("Cancel", 0);

    alertWindow->enterModalState(true, juce::ModalCallbackFunction::create(
        [this, alertWindow](int result)
        {
            delete alertWindow;
            if (result > 0)
                performExport(result);
        }));
}

void MainComponent::performExport(int exportMode)
{
    if (exportMode == 1)
    {
        // Single multichannel WAV
        auto chooser = std::make_shared<juce::FileChooser>(
            "Save Multichannel WAV",
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
            "*.wav");

        chooser->launchAsync(juce::FileBrowserComponent::saveMode |
                            juce::FileBrowserComponent::canSelectFiles,
            [this, chooser](const juce::FileChooser& fc)
            {
                auto file = fc.getResult();
                if (file != juce::File())
                {
                    exportMultichannelWav(file.withFileExtension("wav"));
                }
            });
    }
    else if (exportMode == 2 || exportMode == 3)
    {
        // Mono WAVs or stereo pairs - select output directory
        auto chooser = std::make_shared<juce::FileChooser>(
            "Select Output Directory",
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory));

        chooser->launchAsync(juce::FileBrowserComponent::openMode |
                            juce::FileBrowserComponent::canSelectDirectories,
            [this, chooser, exportMode](const juce::FileChooser& fc)
            {
                auto dir = fc.getResult();
                if (dir != juce::File() && dir.isDirectory())
                {
                    if (exportMode == 2)
                        exportMonoWavFiles(dir);
                    else
                        exportStereoPairs(dir);
                }
            });
    }
}

void MainComponent::exportMultichannelWav(const juce::File& outputFile)
{
    updateStatus("Exporting multichannel WAV...");

    auto lanes = projectModel.getLanes();
    if (lanes.empty())
        return;

    int numChannels = static_cast<int>(lanes.size());
    
    juce::Logger::writeToLog("exportMultichannelWav: " + juce::String(numChannels) + 
                             " channels to " + outputFile.getFullPathName());

    juce::StringArray args;
    args.add(ffmpegLocator.getFFmpegPath().getFullPathName());
    args.add("-y");  // Overwrite output

    // Check if all lanes come from the same source file/stream
    // If so, we can use a simpler single pan filter
    bool singleSource = true;
    juce::File firstFile = lanes[0]->sourceFile;
    int firstStream = lanes[0]->streamIndex;
    
    for (auto* lane : lanes)
    {
        if (lane->sourceFile != firstFile || lane->streamIndex != firstStream)
        {
            singleSource = false;
            break;
        }
    }

    juce::String filterComplex;

    if (singleSource)
    {
        // Simple case: all channels from same source, use single pan filter
        // Format: pan=Nc|c0=cX|c1=cY|...
        args.add("-i");
        args.add(firstFile.getFullPathName());
        
        filterComplex = "pan=" + juce::String(numChannels) + "c";
        for (int i = 0; i < numChannels; ++i)
        {
            filterComplex += "|c" + juce::String(i) + "=c" + juce::String(lanes[static_cast<size_t>(i)]->channelIndex);
        }
    }
    else
    {
        // Complex case: multiple source files, use channelsplit + amerge approach
        std::map<juce::String, int> fileToIndex;
        int inputIndex = 0;

        for (auto* lane : lanes)
        {
            auto key = lane->sourceFile.getFullPathName() + ":" + juce::String(lane->streamIndex);
            if (fileToIndex.find(key) == fileToIndex.end())
            {
                fileToIndex[key] = inputIndex++;
                args.add("-i");
                args.add(lane->sourceFile.getFullPathName());
            }
        }

        // Build filter: extract each channel to mono, then amerge
        juce::StringArray monoOutputs;

        for (size_t i = 0; i < lanes.size(); ++i)
        {
            auto* lane = lanes[i];
            auto key = lane->sourceFile.getFullPathName() + ":" + juce::String(lane->streamIndex);
            int idx = fileToIndex[key];

            juce::String panFilter = "[" + juce::String(idx) + ":a]";
            panFilter += "pan=mono|c0=c" + juce::String(lane->channelIndex);
            panFilter += "[m" + juce::String(static_cast<int>(i)) + "]";

            if (!filterComplex.isEmpty())
                filterComplex += ";";
            filterComplex += panFilter;

            monoOutputs.add("[m" + juce::String(static_cast<int>(i)) + "]");
        }

        // Combine using amerge
        filterComplex += ";";
        for (const auto& out : monoOutputs)
            filterComplex += out;
        filterComplex += "amerge=inputs=" + juce::String(numChannels);
    }

    if (singleSource)
    {
        // Single source: use simple -af filter
        args.add("-af");
        args.add(filterComplex);
    }
    else
    {
        // Multiple sources: use -filter_complex with output label
        filterComplex += "[out]";
        args.add("-filter_complex");
        args.add(filterComplex);
        args.add("-map");
        args.add("[out]");
    }
    
    args.add("-c:a");
    args.add("pcm_s24le");
    args.add(outputFile.getFullPathName());
    
    // Debug: print the full command
    juce::String cmdStr = "FFmpeg command:\n";
    for (const auto& arg : args)
        cmdStr += "  " + arg + "\n";
    juce::Logger::writeToLog(cmdStr);

    // Run ffmpeg
    juce::Thread::launch([this, args, outputFile]()
    {
        juce::ChildProcess process;
        if (process.start(args, juce::ChildProcess::wantStdOut | juce::ChildProcess::wantStdErr))
        {
            // Read any output (errors go to stderr)
            juce::String output = process.readAllProcessOutput();
            process.waitForProcessToFinish(120000);  // 120 second timeout
            auto exitCode = process.getExitCode();
            
            juce::Logger::writeToLog("FFmpeg exit code: " + juce::String(exitCode));
            if (output.isNotEmpty())
                juce::Logger::writeToLog("FFmpeg output: " + output);

            juce::MessageManager::callAsync([this, exitCode, outputFile, output]()
            {
                if (exitCode == 0)
                    updateStatus("Exported: " + outputFile.getFullPathName());
                else
                {
                    updateStatus("Export failed (exit code " + juce::String(exitCode) + "): " + output.substring(0, 100));
                    juce::Logger::writeToLog("Export error: " + output);
                }
            });
        }
        else
        {
            DBG("Failed to start ffmpeg process");
            juce::MessageManager::callAsync([this]()
            {
                updateStatus("Failed to start ffmpeg process");
            });
        }
    });
}

void MainComponent::exportMonoWavFiles(const juce::File& outputDir)
{
    updateStatus("Exporting mono WAV files...");

    auto lanes = projectModel.getLanes();

    // Export each lane as a separate mono file
    for (size_t i = 0; i < lanes.size(); ++i)
    {
        auto* lane = lanes[i];
        auto outputFile = outputDir.getChildFile(
            "channel_" + juce::String(static_cast<int>(i) + 1).paddedLeft('0', 2) + "_" +
            lane->sourceFile.getFileNameWithoutExtension() + ".wav");

        juce::StringArray args;
        args.add(ffmpegLocator.getFFmpegPath().getFullPathName());
        args.add("-v");
        args.add("error");
        args.add("-y");
        args.add("-i");
        args.add(lane->sourceFile.getFullPathName());
        args.add("-map");
        args.add("0:a:" + juce::String(lane->streamIndex));
        args.add("-filter_complex");
        args.add("pan=mono|c0=c" + juce::String(lane->channelIndex));
        args.add("-c:a");
        args.add("pcm_s24le");
        args.add(outputFile.getFullPathName());

        size_t laneIndex = i;
        size_t totalLanes = lanes.size();
        juce::Thread::launch([this, args, outputFile, laneIndex, totalLanes]()
        {
            juce::ChildProcess process;
            if (process.start(args))
            {
                process.waitForProcessToFinish(60000);
                auto exitCode = process.getExitCode();

                juce::MessageManager::callAsync([this, exitCode, outputFile, laneIndex, totalLanes]()
                {
                    if (exitCode == 0)
                    {
                        if (laneIndex == totalLanes - 1)
                            updateStatus("Exported " + juce::String(static_cast<int>(totalLanes)) + " mono files");
                    }
                    else
                    {
                        updateStatus("Export failed for " + outputFile.getFileName());
                    }
                });
            }
        });
    }
}

void MainComponent::exportStereoPairs(const juce::File& outputDir)
{
    updateStatus("Exporting stereo pairs...");

    auto lanes = projectModel.getLanes();
    int numPairs = (static_cast<int>(lanes.size()) + 1) / 2;

    for (int pair = 0; pair < numPairs; ++pair)
    {
        size_t leftIdx = static_cast<size_t>(pair * 2);
        size_t rightIdx = static_cast<size_t>(pair * 2 + 1);

        auto outputFile = outputDir.getChildFile(
            "stereo_" + juce::String(pair + 1).paddedLeft('0', 2) + ".wav");

        juce::StringArray args;
        args.add(ffmpegLocator.getFFmpegPath().getFullPathName());
        args.add("-v");
        args.add("error");
        args.add("-y");

        // Add inputs
        auto* leftLane = lanes[leftIdx];
        args.add("-i");
        args.add(leftLane->sourceFile.getFullPathName());

        Lane* rightLane = nullptr;
        if (rightIdx < lanes.size())
        {
            rightLane = lanes[rightIdx];
            if (rightLane->sourceFile != leftLane->sourceFile ||
                rightLane->streamIndex != leftLane->streamIndex)
            {
                args.add("-i");
                args.add(rightLane->sourceFile.getFullPathName());
            }
        }

        // Build filter
        juce::String filterComplex;

        // Left channel
        filterComplex = "[0:a:" + juce::String(leftLane->streamIndex) + "]";
        filterComplex += "pan=mono|c0=c" + juce::String(leftLane->channelIndex) + "[left];";

        // Right channel (or duplicate left if no right)
        if (rightLane != nullptr)
        {
            int rightInput = (rightLane->sourceFile != leftLane->sourceFile ||
                              rightLane->streamIndex != leftLane->streamIndex) ? 1 : 0;
            filterComplex += "[" + juce::String(rightInput) + ":a:" +
                             juce::String(rightLane->streamIndex) + "]";
            filterComplex += "pan=mono|c0=c" + juce::String(rightLane->channelIndex) + "[right];";
        }
        else
        {
            filterComplex += "[0:a:" + juce::String(leftLane->streamIndex) + "]";
            filterComplex += "pan=mono|c0=c" + juce::String(leftLane->channelIndex) + "[right];";
        }

        filterComplex += "[left][right]amerge=inputs=2[out]";

        args.add("-filter_complex");
        args.add(filterComplex);
        args.add("-map");
        args.add("[out]");
        args.add("-c:a");
        args.add("pcm_s24le");
        args.add(outputFile.getFullPathName());

        juce::Thread::launch([this, args, outputFile, pair, numPairs]()
        {
            juce::ChildProcess process;
            if (process.start(args))
            {
                process.waitForProcessToFinish(60000);
                auto exitCode = process.getExitCode();

                juce::MessageManager::callAsync([this, exitCode, outputFile, pair, numPairs]()
                {
                    if (exitCode == 0)
                    {
                        if (pair == numPairs - 1)
                            updateStatus("Exported " + juce::String(numPairs) + " stereo pairs");
                    }
                    else
                    {
                        updateStatus("Export failed for " + outputFile.getFileName());
                    }
                });
            }
        });
    }
}

void MainComponent::updateStatus(const juce::String& message)
{
    statusLabel.setText(message, juce::dontSendNotification);
}

void MainComponent::laneAdded(Lane* /*lane*/, int /*index*/)
{
    repaint();
}

void MainComponent::laneRemoved(int /*index*/)
{
    repaint();
    if (projectModel.getLaneCount() == 0)
        updateStatus("Drop audio/video files here to add channels");
}

void MainComponent::lanesReordered()
{
    // Lane list handles its own update
}

void MainComponent::laneWaveformUpdated(Lane* /*lane*/)
{
    // Lane components will be notified via the model
}
