# ChannelStacker

A cross-platform desktop application for stacking and managing audio channels from multiple media files. Built with JUCE and C++20.

## Features

- **Drag and Drop**: Drop audio or video files to automatically detect and import audio channels
- **Waveform Display**: Visual waveform envelope for each channel lane
- **Lane Reordering**: Drag lanes to reorder the channel stack
- **Audio Preview**: Play back all channels mixed to stereo for auditioning
- **Multiple Export Options**:
  - Single multichannel file (WAV, AAC, Vorbis, Opus)
  - Multiple mono files
  - Stereo pair files
  - Configurable bit depth and sample rate

## Prerequisites: Installing FFmpeg

ChannelStacker requires **FFmpeg** to be installed on your system. The app will show a helpful dialog on first launch if FFmpeg is not detected.

### macOS

**Option 1: Using Homebrew (Recommended)**
```bash
# Install Homebrew if you don't have it
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install FFmpeg
brew install ffmpeg
```

**Option 2: Using MacPorts**
```bash
sudo port install ffmpeg
```

**Option 3: Manual Download**
1. Download a static build from [FFmpeg.org](https://ffmpeg.org/download.html#build-mac)
2. Extract and move `ffmpeg` and `ffprobe` to `/usr/local/bin/`

### Windows

**Option 1: Using winget**
```powershell
winget install ffmpeg
```

**Option 2: Manual Download**
1. Download from [gyan.dev](https://www.gyan.dev/ffmpeg/builds/) (choose "ffmpeg-release-essentials.zip")
2. Extract to `C:\Program Files\ffmpeg`
3. Add `C:\Program Files\ffmpeg\bin` to your PATH:
   - Press Win+X, select "System"
   - Click "Advanced system settings"
   - Click "Environment Variables"
   - Under "System variables", find "Path", click "Edit"
   - Click "New" and add `C:\Program Files\ffmpeg\bin`
   - Click "OK" on all dialogs
4. Restart any open terminals or applications

### Linux

**Ubuntu/Debian:**
```bash
sudo apt update
sudo apt install ffmpeg
```

**Fedora:**
```bash
sudo dnf install ffmpeg
```

**Arch Linux:**
```bash
sudo pacman -S ffmpeg
```

### Verifying Installation

After installation, verify FFmpeg is working:
```bash
ffmpeg -version
ffprobe -version
```

Both commands should display version information.

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
- Export uses ffmpeg's `asplit` with `pan=mono` and `amerge` filters
- No libav* linking - pure subprocess approach for simplicity and licensing flexibility

## Future Enhancements

- Batch processing
