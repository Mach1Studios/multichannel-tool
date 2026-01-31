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
#include "audio/AudioPlayer.h"

// Export settings structure
struct ExportSettings
{
    enum class ExportMode { Multichannel, MonoFiles, StereoPairs };
    enum class BitDepth { Bit16, Bit24, Bit32Float };
    enum class SampleRate { SR44100, SR48000, SR96000, SR192000, SROriginal };
    enum class Codec { PCM_WAV, AAC, VORBIS, OPUS };

    ExportMode mode = ExportMode::Multichannel;
    BitDepth bitDepth = BitDepth::Bit24;
    SampleRate sampleRate = SampleRate::SROriginal;
    Codec codec = Codec::PCM_WAV;

    juce::String getCodecArgs() const;
    juce::String getSampleRateArgs() const;
    juce::String getFileExtension() const;
};

class MainComponent : public juce::Component,
                      public juce::FileDragAndDropTarget,
                      public ProjectModel::Listener,
                      public AudioPlayer::Listener,
                      private juce::Timer
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

    // AudioPlayer::Listener overrides
    void playbackStarted() override;
    void playbackStopped() override;
    void playbackPositionChanged(double positionSeconds) override;
    void loadStateChanged(AudioPlayer::LoadState newState) override;

private:
    // Timer callback for debounced audio reload
    void timerCallback() override;
    
    void handleDroppedFile(const juce::File& file);
    void showExportDialog();
    void performExport(const ExportSettings& settings);
    void updateStatus(const juce::String& message);
    void updatePlaybackUI();
    void scheduleAudioReload();  // Debounced reload
    void reloadAudioNow();       // Immediate reload
    void checkFFmpegAvailability();  // First-launch check

    // Export helpers
    void exportMultichannelWav(const juce::File& outputFile, const ExportSettings& settings);
    void exportMonoWavFiles(const juce::File& outputDir, const ExportSettings& settings);
    void exportStereoPairs(const juce::File& outputDir, const ExportSettings& settings);

    ProjectModel projectModel;
    FFmpegLocator ffmpegLocator;
    std::unique_ptr<FFProbe> ffprobe;
    std::unique_ptr<WaveformExtractor> waveformExtractor;
    std::unique_ptr<AudioPlayer> audioPlayer;

    // UI Components
    std::unique_ptr<LaneListComponent> laneListComponent;
    juce::TextButton playButton{ "Play" };
    juce::TextButton stopButton{ "Stop" };
    juce::TextButton exportButton{ "Export..." };
    juce::TextButton clearButton{ "Clear All" };
    juce::Label statusLabel;

    // Logo
    std::unique_ptr<juce::Drawable> logoDrawable;

    // Drag state
    bool isDragOver = false;
    
    // Debounce state for audio reload
    bool audioReloadPending = false;

    // Constants
    static constexpr int kToolbarHeight = 50;
    static constexpr int kFooterHeight = 20;
    static constexpr int kDropZoneMinHeight = 100;
    static constexpr int kAudioReloadDebounceMs = 200;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
