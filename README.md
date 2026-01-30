# ChannelStacker

A cross-platform desktop application for stacking and managing audio channels from multiple media files. Built with JUCE and C++20.

## Features

- **Drag and Drop**: Drop audio or video files to automatically detect and import audio channels
- **Waveform Display**: Visual waveform envelope for each channel lane
- **Lane Reordering**: Drag lanes to reorder the channel stack
- **Multiple Export Options**:
  - Single multichannel WAV file
  - Multiple mono WAV files
  - Stereo pair WAV files

### Runtime Requirements
- **FFmpeg** and **FFprobe** executables installed and accessible via PATH

The application uses ffmpeg/ffprobe as external processes for:
- Probing media files for audio stream information
- Decoding audio for waveform display
- Exporting/converting audio

## Building

### Quick Start (macOS)

```bash
make              # Build the app
make run          # Build and run
make help         # Show all available commands
```

### Build Steps

**macOS (Recommended - using Makefile):**
```bash
make                    # Build Release
make BUILD_TYPE=Debug   # Build Debug
make xcode              # Open in Xcode for debugging
```

**macOS/Linux (manual):**
```bash
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

**Windows (Visual Studio):**
```bash
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022"
cmake --build . --config Release
```

Or open the generated `.sln` file in Visual Studio.

## Packaging and Distribution (macOS)

### Create DMG Installer

```bash
# Unsigned DMG (for local testing)
make dmg-unsigned

# Signed DMG (requires Developer ID)
make sign CODESIGN_IDENTITY="Developer ID Application: Your Name (TEAMID)"
make dmg
```

### Full Release with Notarization

For distribution outside the App Store, apps must be notarized:

```bash
# Set your Apple Developer credentials
export CODESIGN_IDENTITY="Developer ID Application: Your Name (TEAMID)"
export APPLE_ID="your@email.com"
export APPLE_TEAM_ID="XXXXXXXXXX"
export APPLE_APP_PASSWORD="@keychain:notarytool-password"

# Full release: build, sign, package, notarize
make release
```

### Makefile Targets

| Target | Description |
|--------|-------------|
| `make` | Build the app |
| `make run` | Build and run |
| `make clean` | Remove build artifacts |
| `make sign-adhoc` | Ad-hoc sign for local testing |
| `make dmg-unsigned` | Create unsigned DMG |
| `make dmg` | Create signed DMG |
| `make notarize` | Notarize for Gatekeeper |
| `make release` | Full release workflow |
| `make install` | Install to /Applications |
| `make list-identities` | List signing identities |
| `make help` | Show all commands |

## Usage

1. **Add Files**: Drag audio or video files into the application window
2. **View Channels**: Each audio channel creates a lane with waveform visualization
3. **Reorder**: Drag the grip handle on the left of each lane to reorder
4. **Delete**: Click the X button on any lane to remove it
5. **Export**: Click "Export..." to choose an export format:
   - **Single Multichannel WAV**: Combines all lanes into one file with N channels
   - **Multiple Mono WAVs**: Exports each lane as a separate mono file
   - **Stereo Pairs**: Groups adjacent lanes into stereo files

## Technical Notes

- Uses `juce::ChildProcess` to run ffmpeg/ffprobe as external commands
- Audio is decoded to raw float32 PCM for waveform computation
- Waveform envelope uses 4000 points for responsive display
- Export uses ffmpeg's `filter_complex` with `pan` and `amerge` filters
- No libav* linking - pure subprocess approach for simplicity and licensing flexibility

## Future Enhancements

- Audio preview/playback
- More export formats (FLAC, AIFF, etc.)
- Batch processing
