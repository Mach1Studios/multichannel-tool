/*
    ChannelStacker - Main Component Implementation
*/

#include "MainComponent.h"
#include "ui/Mach1LookAndFeel.h"
#include "BinaryData.h"

//==============================================================================
// ExportSettings implementation
//==============================================================================

juce::String ExportSettings::getCodecArgs() const
{
    switch (codec)
    {
        case Codec::PCM_WAV:
            switch (bitDepth)
            {
                case BitDepth::Bit16:      return "pcm_s16le";
                case BitDepth::Bit24:      return "pcm_s24le";
                case BitDepth::Bit32Float: return "pcm_f32le";
            }
            break;
        case Codec::AAC:
            return "aac -b:a 256k";
        case Codec::VORBIS:
            return "libvorbis -q:a 6";  // Quality 6 is ~192kbps VBR
        case Codec::OPUS:
            return "libopus -b:a 128k";
    }
    return "pcm_s24le";
}

juce::String ExportSettings::getSampleRateArgs() const
{
    switch (sampleRate)
    {
        case SampleRate::SR44100:   return "44100";
        case SampleRate::SR48000:   return "48000";
        case SampleRate::SR96000:   return "96000";
        case SampleRate::SR192000:  return "192000";
        case SampleRate::SROriginal: return "";  // Empty means don't resample
    }
    return "";
}

juce::String ExportSettings::getFileExtension() const
{
    switch (codec)
    {
        case Codec::PCM_WAV:  return "wav";
        case Codec::AAC:      return "m4a";
        case Codec::VORBIS:   return "ogg";
        case Codec::OPUS:     return "opus";
    }
    return "wav";
}

//==============================================================================
// MainComponent implementation
//==============================================================================

MainComponent::MainComponent()
{
    // Initialize FFmpeg tools
    ffprobe = std::make_unique<FFProbe>(ffmpegLocator);
    waveformExtractor = std::make_unique<WaveformExtractor>(ffmpegLocator);

    // Initialize audio player
    audioPlayer = std::make_unique<AudioPlayer>(ffmpegLocator);
    audioPlayer->addListener(this);
    audioPlayer->initialize();

    // Register as listener
    projectModel.addListener(this);

    // Setup lane list
    laneListComponent = std::make_unique<LaneListComponent>(projectModel);
    addAndMakeVisible(*laneListComponent);

    // Setup play button
    playButton.setColour(juce::TextButton::buttonColourId, Mach1LookAndFeel::Colors::buttonOff);
    playButton.setColour(juce::TextButton::textColourOffId, Mach1LookAndFeel::Colors::statusActive);
    playButton.onClick = [this]()
    {
        if (audioPlayer->isPlaying())
            audioPlayer->stop();
        else
            audioPlayer->play();
    };
    addAndMakeVisible(playButton);

    // Setup stop button
    stopButton.setColour(juce::TextButton::buttonColourId, Mach1LookAndFeel::Colors::buttonOff);
    stopButton.setColour(juce::TextButton::textColourOffId, Mach1LookAndFeel::Colors::textPrimary);
    stopButton.onClick = [this]() { audioPlayer->stop(); };
    addAndMakeVisible(stopButton);

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
        audioPlayer->stop();
        projectModel.clearAllLanes();
        updateStatus("All lanes cleared");
    };
    addAndMakeVisible(clearButton);

    // Setup status label
    statusLabel.setJustificationType(juce::Justification::centredLeft);
    statusLabel.setColour(juce::Label::textColourId, Mach1LookAndFeel::Colors::textSecondary);
    statusLabel.setFont(juce::FontOptions(11.0f));
    addAndMakeVisible(statusLabel);

    // Load logo from binary data
    logoDrawable = juce::Drawable::createFromImageData(
        BinaryData::mach1logo_png, BinaryData::mach1logo_pngSize);

    updateStatus("Drop audio/video files here to add channels");
    updatePlaybackUI();

    setSize(1000, 700);
    
    // Check for ffmpeg after a short delay to allow window to appear
    juce::Timer::callAfterDelay(500, [this]() { checkFFmpegAvailability(); });
}

MainComponent::~MainComponent()
{
    audioPlayer->removeListener(this);
    audioPlayer->shutdown();
    projectModel.removeListener(this);
    waveformExtractor->cancelAll();
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(Mach1LookAndFeel::Colors::background);

    // Draw drop zone indicator
    auto dropZone = getLocalBounds().reduced(10)
        .withTrimmedTop(kToolbarHeight)
        .withTrimmedBottom(kFooterHeight);

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

    // Draw footer background
    auto footerBounds = getLocalBounds().removeFromBottom(kFooterHeight);
    g.setColour(Mach1LookAndFeel::Colors::background);
    g.fillRect(footerBounds);

    // Draw separator line
    g.setColour(Mach1LookAndFeel::Colors::border);
    g.drawHorizontalLine(footerBounds.getY(), 0.0f, static_cast<float>(getWidth()));

    // Draw logo in footer
    if (logoDrawable != nullptr)
    {
        auto logoBounds = footerBounds.reduced(10, 5).removeFromLeft(60);
        logoDrawable->drawWithin(g, logoBounds.toFloat(),
                                  juce::RectanglePlacement::yMid |
                                  juce::RectanglePlacement::onlyReduceInSize, 1.0f);
    }
}

void MainComponent::resized()
{
    auto bounds = getLocalBounds();

    // Footer at bottom
    bounds.removeFromBottom(kFooterHeight);

    // Toolbar at top
    auto toolbar = bounds.removeFromTop(kToolbarHeight);
    toolbar.reduce(10, 10);

    // Playback controls
    playButton.setBounds(toolbar.removeFromLeft(60));
    toolbar.removeFromLeft(5);
    stopButton.setBounds(toolbar.removeFromLeft(60));
    toolbar.removeFromLeft(15);

    // Export/Clear buttons
    exportButton.setBounds(toolbar.removeFromLeft(100));
    toolbar.removeFromLeft(10);
    clearButton.setBounds(toolbar.removeFromLeft(100));
    toolbar.removeFromLeft(20);
    
    // Status label fills the rest
    statusLabel.setBounds(toolbar);

    // Lane list fills the middle
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

    // Create a custom dialog component with Mach1 styling
    auto* dialogContent = new juce::Component();
    dialogContent->setSize(360, 230);

    // Helper to style labels
    auto styleLabel = [](juce::Label* label) {
        label->setColour(juce::Label::textColourId, Mach1LookAndFeel::Colors::textPrimary);
        label->setFont(juce::FontOptions(11.0f));
    };

    // Helper to style combo boxes
    auto styleCombo = [](juce::ComboBox* combo) {
        combo->setColour(juce::ComboBox::backgroundColourId, Mach1LookAndFeel::Colors::buttonOff);
        combo->setColour(juce::ComboBox::textColourId, Mach1LookAndFeel::Colors::textPrimary);
        combo->setColour(juce::ComboBox::outlineColourId, Mach1LookAndFeel::Colors::border);
        combo->setColour(juce::ComboBox::arrowColourId, Mach1LookAndFeel::Colors::textSecondary);
    };

    // Helper to style buttons
    auto styleButton = [](juce::TextButton* btn) {
        btn->setColour(juce::TextButton::buttonColourId, Mach1LookAndFeel::Colors::buttonOff);
        btn->setColour(juce::TextButton::textColourOffId, Mach1LookAndFeel::Colors::textPrimary);
    };

    // Export mode combo
    auto* modeLabel = new juce::Label("modeLabel", "Export Mode:");
    modeLabel->setBounds(15, 15, 100, 24);
    styleLabel(modeLabel);
    dialogContent->addAndMakeVisible(modeLabel);

    auto* modeCombo = new juce::ComboBox("modeCombo");
    modeCombo->addItem("Single Multichannel File", 1);
    modeCombo->addItem("Multiple Mono Files", 2);
    modeCombo->addItem("Stereo Pairs", 3);
    modeCombo->setSelectedId(1);
    modeCombo->setBounds(125, 15, 220, 24);
    styleCombo(modeCombo);
    dialogContent->addAndMakeVisible(modeCombo);

    // Codec combo
    auto* codecLabel = new juce::Label("codecLabel", "Format:");
    codecLabel->setBounds(15, 50, 100, 24);
    styleLabel(codecLabel);
    dialogContent->addAndMakeVisible(codecLabel);

    auto* codecCombo = new juce::ComboBox("codecCombo");
    codecCombo->addItem("WAV", 1);
    codecCombo->addItem("AAC", 2);
    codecCombo->addItem("Vorbis (OGG)", 3);
    codecCombo->addItem("Opus", 4);
    codecCombo->setSelectedId(1);
    codecCombo->setBounds(125, 50, 220, 24);
    styleCombo(codecCombo);
    dialogContent->addAndMakeVisible(codecCombo);

    // Bit depth combo
    auto* bitDepthLabel = new juce::Label("bitDepthLabel", "Bit Depth:");
    bitDepthLabel->setBounds(15, 85, 100, 24);
    styleLabel(bitDepthLabel);
    dialogContent->addAndMakeVisible(bitDepthLabel);

    auto* bitDepthCombo = new juce::ComboBox("bitDepthCombo");
    bitDepthCombo->addItem("16-bit", 1);
    bitDepthCombo->addItem("24-bit", 2);
    bitDepthCombo->addItem("32-bit Float", 3);
    bitDepthCombo->setSelectedId(2);  // Default to 24-bit
    bitDepthCombo->setBounds(125, 85, 220, 24);
    styleCombo(bitDepthCombo);
    dialogContent->addAndMakeVisible(bitDepthCombo);

    // Sample rate combo
    auto* sampleRateLabel = new juce::Label("sampleRateLabel", "Sample Rate:");
    sampleRateLabel->setBounds(15, 120, 100, 24);
    styleLabel(sampleRateLabel);
    dialogContent->addAndMakeVisible(sampleRateLabel);

    auto* sampleRateCombo = new juce::ComboBox("sampleRateCombo");
    sampleRateCombo->addItem("Original", 1);
    sampleRateCombo->addItem("44.1 kHz", 2);
    sampleRateCombo->addItem("48 kHz", 3);
    sampleRateCombo->addItem("96 kHz", 4);
    sampleRateCombo->addItem("192 kHz", 5);
    sampleRateCombo->setSelectedId(1);  // Default to original
    sampleRateCombo->setBounds(125, 120, 220, 24);
    styleCombo(sampleRateCombo);
    dialogContent->addAndMakeVisible(sampleRateCombo);

    // Update bit depth options based on codec selection
    // WAV supports bit depth, lossy codecs don't
    codecCombo->onChange = [codecCombo, bitDepthCombo]()
    {
        int codecId = codecCombo->getSelectedId();
        bool isWav = (codecId == 1);  // Only WAV supports bit depth selection
        bitDepthCombo->setEnabled(isWav);
        if (!isWav)
            bitDepthCombo->setSelectedId(2);  // Default to 24-bit equivalent
    };

    // Export button
    auto* exportBtn = new juce::TextButton("Export");
    exportBtn->setBounds(175, 170, 80, 28);
    styleButton(exportBtn);
    exportBtn->setColour(juce::TextButton::textColourOffId, Mach1LookAndFeel::Colors::statusActive);
    dialogContent->addAndMakeVisible(exportBtn);

    // Cancel button
    auto* cancelBtn = new juce::TextButton("Cancel");
    cancelBtn->setBounds(265, 170, 80, 28);
    styleButton(cancelBtn);
    dialogContent->addAndMakeVisible(cancelBtn);

    // Create the dialog
    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = "Export Options";
    options.content.setOwned(dialogContent);
    options.dialogBackgroundColour = Mach1LookAndFeel::Colors::panelBackground;
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = false;
    options.resizable = false;

    auto* dialog = options.create();
    dialog->setColour(juce::DocumentWindow::backgroundColourId, Mach1LookAndFeel::Colors::panelBackground);
    
    // Set button callbacks after dialog is created
    exportBtn->onClick = [this, dialog, modeCombo, codecCombo, bitDepthCombo, sampleRateCombo]()
    {
        ExportSettings settings;
        
        // Set mode
        switch (modeCombo->getSelectedId())
        {
            case 1: settings.mode = ExportSettings::ExportMode::Multichannel; break;
            case 2: settings.mode = ExportSettings::ExportMode::MonoFiles; break;
            case 3: settings.mode = ExportSettings::ExportMode::StereoPairs; break;
        }
        
        // Set codec
        switch (codecCombo->getSelectedId())
        {
            case 1: settings.codec = ExportSettings::Codec::PCM_WAV; break;
            case 2: settings.codec = ExportSettings::Codec::AAC; break;
            case 3: settings.codec = ExportSettings::Codec::VORBIS; break;
            case 4: settings.codec = ExportSettings::Codec::OPUS; break;
        }
        
        // Set bit depth
        switch (bitDepthCombo->getSelectedId())
        {
            case 1: settings.bitDepth = ExportSettings::BitDepth::Bit16; break;
            case 2: settings.bitDepth = ExportSettings::BitDepth::Bit24; break;
            case 3: settings.bitDepth = ExportSettings::BitDepth::Bit32Float; break;
        }
        
        // Set sample rate
        switch (sampleRateCombo->getSelectedId())
        {
            case 1: settings.sampleRate = ExportSettings::SampleRate::SROriginal; break;
            case 2: settings.sampleRate = ExportSettings::SampleRate::SR44100; break;
            case 3: settings.sampleRate = ExportSettings::SampleRate::SR48000; break;
            case 4: settings.sampleRate = ExportSettings::SampleRate::SR96000; break;
            case 5: settings.sampleRate = ExportSettings::SampleRate::SR192000; break;
        }
        
        dialog->exitModalState(0);
        delete dialog;
        
        performExport(settings);
    };

    cancelBtn->onClick = [dialog]()
    {
        dialog->exitModalState(0);
        delete dialog;
    };

    dialog->enterModalState(true, nullptr, true);
}

void MainComponent::performExport(const ExportSettings& settings)
{
    juce::String extension = settings.getFileExtension();
    
    if (settings.mode == ExportSettings::ExportMode::Multichannel)
    {
        // Single multichannel file
        auto chooser = std::make_shared<juce::FileChooser>(
            "Save Multichannel File",
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory),
            "*." + extension);

        chooser->launchAsync(juce::FileBrowserComponent::saveMode |
                            juce::FileBrowserComponent::canSelectFiles,
            [this, chooser, settings, extension](const juce::FileChooser& fc)
            {
                auto file = fc.getResult();
                if (file != juce::File())
                {
                    exportMultichannelWav(file.withFileExtension(extension), settings);
                }
            });
    }
    else
    {
        // Mono files or stereo pairs - select output directory
        auto chooser = std::make_shared<juce::FileChooser>(
            "Select Output Directory",
            juce::File::getSpecialLocation(juce::File::userDocumentsDirectory));

        chooser->launchAsync(juce::FileBrowserComponent::openMode |
                            juce::FileBrowserComponent::canSelectDirectories,
            [this, chooser, settings](const juce::FileChooser& fc)
            {
                auto dir = fc.getResult();
                if (dir != juce::File() && dir.isDirectory())
                {
                    if (settings.mode == ExportSettings::ExportMode::MonoFiles)
                        exportMonoWavFiles(dir, settings);
                    else
                        exportStereoPairs(dir, settings);
                }
            });
    }
}

void MainComponent::exportMultichannelWav(const juce::File& outputFile, const ExportSettings& settings)
{
    updateStatus("Exporting multichannel file...");

    auto lanes = projectModel.getLanes();
    if (lanes.empty())
        return;

    int numChannels = static_cast<int>(lanes.size());
    
    juce::Logger::writeToLog("exportMultichannelWav: " + juce::String(numChannels) + 
                             " channels to " + outputFile.getFullPathName());

    juce::StringArray args;
    args.add(ffmpegLocator.getFFmpegPath().getFullPathName());
    args.add("-y");  // Overwrite output

    // Build a map of unique source file+stream combinations
    std::map<juce::String, int> sourceToIndex;
    int inputIndex = 0;

    for (auto* lane : lanes)
    {
        auto key = lane->sourceFile.getFullPathName() + ":" + juce::String(lane->streamIndex);
        if (sourceToIndex.find(key) == sourceToIndex.end())
        {
            sourceToIndex[key] = inputIndex++;
            args.add("-i");
            args.add(lane->sourceFile.getFullPathName());
        }
    }

    // Build filter_complex using asplit + pan=mono + amerge approach
    juce::String filterComplex;
    juce::StringArray monoOutputs;

    // Count how many times each source is used
    std::map<juce::String, int> sourceUsageCount;
    for (auto* lane : lanes)
    {
        auto key = lane->sourceFile.getFullPathName() + ":" + juce::String(lane->streamIndex);
        sourceUsageCount[key]++;
    }

    // Create asplit filters for sources used multiple times
    std::map<juce::String, juce::StringArray> sourceSplitLabels;
    for (const auto& [key, count] : sourceUsageCount)
    {
        int idx = sourceToIndex[key];
        if (count > 1)
        {
            juce::String asplitFilter = "[" + juce::String(idx) + ":a]asplit=" + juce::String(count);
            juce::StringArray labels;
            for (int i = 0; i < count; ++i)
            {
                juce::String label = "[s" + juce::String(idx) + "_" + juce::String(i) + "]";
                labels.add(label);
                asplitFilter += label;
            }
            sourceSplitLabels[key] = labels;
            
            if (!filterComplex.isEmpty())
                filterComplex += ";";
            filterComplex += asplitFilter;
        }
    }

    // Track which split index we're on for each source
    std::map<juce::String, int> sourceSplitIndex;
    for (const auto& [key, _] : sourceUsageCount)
        sourceSplitIndex[key] = 0;

    // Create pan=mono filter for each lane
    for (size_t i = 0; i < lanes.size(); ++i)
    {
        auto* lane = lanes[i];
        auto key = lane->sourceFile.getFullPathName() + ":" + juce::String(lane->streamIndex);
        int idx = sourceToIndex[key];

        juce::String inputLabel;
        if (sourceUsageCount[key] > 1)
        {
            int splitIdx = sourceSplitIndex[key]++;
            inputLabel = sourceSplitLabels[key][splitIdx];
        }
        else
        {
            inputLabel = "[" + juce::String(idx) + ":a]";
        }

        juce::String monoLabel = "[m" + juce::String(static_cast<int>(i)) + "]";
        juce::String panFilter = inputLabel + "pan=mono|c0=c" + juce::String(lane->channelIndex) + monoLabel;

        if (!filterComplex.isEmpty())
            filterComplex += ";";
        filterComplex += panFilter;

        monoOutputs.add(monoLabel);
    }

    // Combine all mono channels using amerge
    filterComplex += ";";
    for (const auto& out : monoOutputs)
        filterComplex += out;
    filterComplex += "amerge=inputs=" + juce::String(numChannels) + "[out]";

    args.add("-filter_complex");
    args.add(filterComplex);
    args.add("-map");
    args.add("[out]");
    
    // Add sample rate conversion if requested
    juce::String sampleRateStr = settings.getSampleRateArgs();
    if (sampleRateStr.isNotEmpty())
    {
        args.add("-ar");
        args.add(sampleRateStr);
    }
    
    // Add codec settings
    juce::String codecArgs = settings.getCodecArgs();
    juce::StringArray codecParts;
    codecParts.addTokens(codecArgs, " ", "");
    args.add("-c:a");
    args.add(codecParts[0]);
    for (int i = 1; i < codecParts.size(); ++i)
        args.add(codecParts[i]);
    
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
            juce::String output = process.readAllProcessOutput();
            process.waitForProcessToFinish(120000);
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
                    updateStatus("Export failed (exit code " + juce::String(exitCode) + ")");
                    juce::Logger::writeToLog("Export error: " + output);
                }
            });
        }
        else
        {
            juce::MessageManager::callAsync([this]()
            {
                updateStatus("Failed to start ffmpeg process");
            });
        }
    });
}

void MainComponent::exportMonoWavFiles(const juce::File& outputDir, const ExportSettings& settings)
{
    updateStatus("Exporting mono files...");

    auto lanes = projectModel.getLanes();
    juce::String extension = settings.getFileExtension();

    // Export each lane as a separate mono file
    for (size_t i = 0; i < lanes.size(); ++i)
    {
        auto* lane = lanes[i];
        auto outputFile = outputDir.getChildFile(
            "channel_" + juce::String(static_cast<int>(i) + 1).paddedLeft('0', 2) + "_" +
            lane->sourceFile.getFileNameWithoutExtension() + "." + extension);

        juce::StringArray args;
        args.add(ffmpegLocator.getFFmpegPath().getFullPathName());
        args.add("-v");
        args.add("error");
        args.add("-y");
        args.add("-i");
        args.add(lane->sourceFile.getFullPathName());
        
        // Use filter_complex with proper input stream reference
        juce::String filterComplex = "[0:a:" + juce::String(lane->streamIndex) + "]";
        filterComplex += "pan=mono|c0=c" + juce::String(lane->channelIndex) + "[out]";
        
        args.add("-filter_complex");
        args.add(filterComplex);
        args.add("-map");
        args.add("[out]");
        
        // Add sample rate conversion if requested
        juce::String sampleRateStr = settings.getSampleRateArgs();
        if (sampleRateStr.isNotEmpty())
        {
            args.add("-ar");
            args.add(sampleRateStr);
        }
        
        // Add codec settings
        juce::String codecArgs = settings.getCodecArgs();
        juce::StringArray codecParts;
        codecParts.addTokens(codecArgs, " ", "");
        args.add("-c:a");
        args.add(codecParts[0]);
        for (int j = 1; j < codecParts.size(); ++j)
            args.add(codecParts[j]);
        
        args.add(outputFile.getFullPathName());

        size_t laneIndex = i;
        size_t totalLanes = lanes.size();
        juce::Thread::launch([this, args, outputFile, laneIndex, totalLanes]()
        {
            juce::ChildProcess process;
            if (process.start(args, juce::ChildProcess::wantStdOut | juce::ChildProcess::wantStdErr))
            {
                juce::String output = process.readAllProcessOutput();
                process.waitForProcessToFinish(60000);
                auto exitCode = process.getExitCode();
                
                if (exitCode != 0)
                    juce::Logger::writeToLog("Mono export error: " + output);

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

void MainComponent::exportStereoPairs(const juce::File& outputDir, const ExportSettings& settings)
{
    updateStatus("Exporting stereo pairs...");

    auto lanes = projectModel.getLanes();
    int numPairs = (static_cast<int>(lanes.size()) + 1) / 2;
    juce::String extension = settings.getFileExtension();

    for (int pair = 0; pair < numPairs; ++pair)
    {
        size_t leftIdx = static_cast<size_t>(pair * 2);
        size_t rightIdx = static_cast<size_t>(pair * 2 + 1);

        auto outputFile = outputDir.getChildFile(
            "stereo_" + juce::String(pair + 1).paddedLeft('0', 2) + "." + extension);

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
        bool separateInputs = false;
        if (rightIdx < lanes.size())
        {
            rightLane = lanes[rightIdx];
            if (rightLane->sourceFile != leftLane->sourceFile ||
                rightLane->streamIndex != leftLane->streamIndex)
            {
                separateInputs = true;
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
            int rightInput = separateInputs ? 1 : 0;
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
        
        // Add sample rate conversion if requested
        juce::String sampleRateStr = settings.getSampleRateArgs();
        if (sampleRateStr.isNotEmpty())
        {
            args.add("-ar");
            args.add(sampleRateStr);
        }
        
        // Add codec settings
        juce::String codecArgs = settings.getCodecArgs();
        juce::StringArray codecParts;
        codecParts.addTokens(codecArgs, " ", "");
        args.add("-c:a");
        args.add(codecParts[0]);
        for (int j = 1; j < codecParts.size(); ++j)
            args.add(codecParts[j]);
        
        args.add(outputFile.getFullPathName());

        juce::Thread::launch([this, args, outputFile, pair, numPairs]()
        {
            juce::ChildProcess process;
            if (process.start(args, juce::ChildProcess::wantStdOut | juce::ChildProcess::wantStdErr))
            {
                juce::String output = process.readAllProcessOutput();
                process.waitForProcessToFinish(60000);
                auto exitCode = process.getExitCode();
                
                if (exitCode != 0)
                    juce::Logger::writeToLog("Stereo export error: " + output);

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

void MainComponent::timerCallback()
{
    stopTimer();
    
    if (audioReloadPending)
    {
        audioReloadPending = false;
        reloadAudioNow();
    }
}

void MainComponent::scheduleAudioReload()
{
    // Debounce audio reloads - wait for lane additions to settle
    audioReloadPending = true;
    startTimer(kAudioReloadDebounceMs);
}

void MainComponent::laneAdded(Lane* /*lane*/, int /*index*/)
{
    repaint();
    scheduleAudioReload();  // Debounced
}

void MainComponent::laneRemoved(int /*index*/)
{
    repaint();
    scheduleAudioReload();  // Debounced
    if (projectModel.getLaneCount() == 0)
        updateStatus("Drop audio/video files here to add channels");
}

void MainComponent::lanesReordered()
{
    // Lane list handles its own update
    reloadAudioNow();  // Immediate for reorder (user expects instant feedback)
}

void MainComponent::laneWaveformUpdated(Lane* /*lane*/)
{
    // Lane components will be notified via the model
}

void MainComponent::playbackStarted()
{
    updatePlaybackUI();
}

void MainComponent::playbackStopped()
{
    updatePlaybackUI();
}

void MainComponent::playbackPositionChanged(double /*positionSeconds*/)
{
    // Could update a position display here
}

void MainComponent::loadStateChanged(AudioPlayer::LoadState newState)
{
    updatePlaybackUI();
    
    switch (newState)
    {
        case AudioPlayer::LoadState::Empty:
            break;
        case AudioPlayer::LoadState::Loading:
            updateStatus("Loading audio for preview...");
            break;
        case AudioPlayer::LoadState::Ready:
            updateStatus("Ready to play");
            break;
        case AudioPlayer::LoadState::Error:
            updateStatus("Failed to load audio for preview");
            break;
    }
}

void MainComponent::updatePlaybackUI()
{
    bool playing = audioPlayer->isPlaying();
    bool ready = audioPlayer->isReady();
    bool loading = audioPlayer->isLoading();
    
    // Update play button state
    if (loading)
    {
        playButton.setButtonText("Loading...");
        playButton.setEnabled(false);
        playButton.setColour(juce::TextButton::textColourOffId,
                             Mach1LookAndFeel::Colors::textSecondary);
    }
    else if (playing)
    {
        playButton.setButtonText("Pause");
        playButton.setEnabled(true);
        playButton.setColour(juce::TextButton::textColourOffId,
                             Mach1LookAndFeel::Colors::statusWarning);
    }
    else
    {
        playButton.setButtonText("Play");
        playButton.setEnabled(ready);
        playButton.setColour(juce::TextButton::textColourOffId,
                             ready ? Mach1LookAndFeel::Colors::statusActive
                                   : Mach1LookAndFeel::Colors::textSecondary);
    }
    
    // Stop button always enabled if playing
    stopButton.setEnabled(playing);
}

void MainComponent::reloadAudioNow()
{
    // Reload audio for playback after lanes change
    auto lanes = projectModel.getLanes();
    audioPlayer->loadLanes(lanes);
}

void MainComponent::checkFFmpegAvailability()
{
    bool ffmpegOk = ffmpegLocator.isFFmpegAvailable();
    bool ffprobeOk = ffmpegLocator.isFFprobeAvailable();
    
    if (ffmpegOk && ffprobeOk)
    {
        // All good - show version in status
        auto version = ffmpegLocator.getFFmpegVersion();
        if (version.isNotEmpty())
        {
            // Extract just "ffmpeg version X.X.X" part
            auto versionLine = version.upToFirstOccurrenceOf(" Copyright", false, true);
            updateStatus("Ready - " + versionLine);
        }
        return;
    }
    
    // Build the message
    juce::String missing;
    if (!ffmpegOk && !ffprobeOk)
        missing = "FFmpeg and FFprobe are";
    else if (!ffmpegOk)
        missing = "FFmpeg is";
    else
        missing = "FFprobe is";
    
    juce::String message = missing + " not found on your system.\n\n";
    message += "ChannelStacker requires FFmpeg to analyze and export audio files.\n\n";
    
#if JUCE_MAC
    message += "To install on macOS:\n\n";
    message += "Using Homebrew (recommended):\n";
    message += "    brew install ffmpeg\n\n";
    message += "Or download from: https://ffmpeg.org/download.html\n";
#elif JUCE_WINDOWS
    message += "To install on Windows:\n\n";
    message += "1. Download from: https://www.gyan.dev/ffmpeg/builds/\n";
    message += "   (Choose 'ffmpeg-release-essentials.zip')\n\n";
    message += "2. Extract to C:\\Program Files\\ffmpeg\n\n";
    message += "3. Add C:\\Program Files\\ffmpeg\\bin to your PATH:\n";
    message += "   - Search 'Environment Variables' in Start\n";
    message += "   - Edit PATH, add the bin folder\n";
#elif JUCE_LINUX
    message += "To install on Linux:\n\n";
    message += "Ubuntu/Debian:\n";
    message += "    sudo apt install ffmpeg\n\n";
    message += "Fedora:\n";
    message += "    sudo dnf install ffmpeg\n\n";
    message += "Arch:\n";
    message += "    sudo pacman -S ffmpeg\n";
#endif
    
    // Show dialog with platform-specific install button
    auto options = juce::MessageBoxOptions()
        .withIconType(juce::MessageBoxIconType::WarningIcon)
        .withTitle("FFmpeg Required")
        .withMessage(message)
#if JUCE_MAC
        .withButton("Install with Homebrew")
        .withButton("Open Download Page")
#elif JUCE_WINDOWS
        .withButton("Open Download Page")
#else
        .withButton("OK")
#endif
        .withButton("Continue Anyway");
    
    juce::AlertWindow::showAsync(options, [](int result)
    {
#if JUCE_MAC
        if (result == 1)
        {
            // Open Terminal with homebrew install command
            juce::URL("x-apple.systempreferences:").launchInDefaultBrowser();
            // Actually run the brew command in Terminal
            juce::ChildProcess terminal;
            terminal.start("open -a Terminal");
            // Show instructions since we can't easily run the command
            juce::AlertWindow::showMessageBoxAsync(
                juce::MessageBoxIconType::InfoIcon,
                "Install FFmpeg",
                "A Terminal window will open.\n\n"
                "If you have Homebrew installed, run:\n"
                "    brew install ffmpeg\n\n"
                "If you don't have Homebrew, first install it from:\n"
                "    https://brew.sh");
        }
        else if (result == 2)
        {
            juce::URL("https://ffmpeg.org/download.html#build-mac").launchInDefaultBrowser();
        }
#elif JUCE_WINDOWS
        if (result == 1)
        {
            juce::URL("https://www.gyan.dev/ffmpeg/builds/").launchInDefaultBrowser();
        }
#endif
    });
    
    updateStatus("Warning: FFmpeg not found - features will be limited");
}
