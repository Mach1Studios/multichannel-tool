/*
    ChannelStacker - Main Component Header
    Root component containing the lane list and controls
*/

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "model/ProjectModel.h"
#include "ui/LaneListComponent.h"
#include "ffmpeg/FFmpegLocator.h"
#include "ffmpeg/FFProbe.h"
#include "audio/WaveformExtractor.h"

class MainComponent : public juce::Component,
                      public juce::FileDragAndDropTarget,
                      public ProjectModel::Listener
{
public:
    MainComponent();
    ~MainComponent() override;

    // Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;

    // FileDragAndDropTarget overrides
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;
    void fileDragEnter(const juce::StringArray& files, int x, int y) override;
    void fileDragExit(const juce::StringArray& files) override;

    // ProjectModel::Listener overrides
    void laneAdded(Lane* lane, int index) override;
    void laneRemoved(int index) override;
    void lanesReordered() override;
    void laneWaveformUpdated(Lane* lane) override;

private:
    void handleDroppedFile(const juce::File& file);
    void showExportDialog();
    void performExport(int exportMode);
    void updateStatus(const juce::String& message);

    // Export helpers
    void exportMultichannelWav(const juce::File& outputFile);
    void exportMonoWavFiles(const juce::File& outputDir);
    void exportStereoPairs(const juce::File& outputDir);

    ProjectModel projectModel;
    FFmpegLocator ffmpegLocator;
    std::unique_ptr<FFProbe> ffprobe;
    std::unique_ptr<WaveformExtractor> waveformExtractor;

    // UI Components
    std::unique_ptr<LaneListComponent> laneListComponent;
    juce::TextButton exportButton{ "Export..." };
    juce::TextButton clearButton{ "Clear All" };
    juce::Label statusLabel;

    // Logo
    std::unique_ptr<juce::Drawable> logoDrawable;

    // Drag state
    bool isDragOver = false;

    // Constants
    static constexpr int kToolbarHeight = 50;
    static constexpr int kFooterHeight = 40;
    static constexpr int kDropZoneMinHeight = 100;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
