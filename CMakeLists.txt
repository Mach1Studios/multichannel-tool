# ChannelStacker - Multichannel Audio Tool
# CMake build configuration

cmake_minimum_required(VERSION 3.22)

project(ChannelStacker
    VERSION 1.0.0
    LANGUAGES C CXX
)

# C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Use folders in IDEs
set_property(GLOBAL PROPERTY USE_FOLDERS YES)

# Add JUCE from submodule
add_subdirectory(JUCE)

# Define the GUI application
juce_add_gui_app(ChannelStacker
    PRODUCT_NAME "ChannelStacker"
    COMPANY_NAME "Mach1"
    BUNDLE_ID "com.mach1.channelstacker"
    VERSION "1.0.0"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/res/icon.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/res/icon.png"
)

# Binary resources (logo, icons)
juce_add_binary_data(ChannelStackerResources
    NAMESPACE BinaryData
    SOURCES
        res/mach1logo.png
        res/icon.png
)

# Source files
target_sources(ChannelStacker PRIVATE
    src/main.cpp
    src/MainWindow.h
    src/MainWindow.cpp
    src/MainComponent.h
    src/MainComponent.cpp
    src/model/ProjectModel.h
    src/model/ProjectModel.cpp
    src/ffmpeg/FFmpegLocator.h
    src/ffmpeg/FFmpegLocator.cpp
    src/ffmpeg/FFProbe.h
    src/ffmpeg/FFProbe.cpp
    src/audio/WaveformExtractor.h
    src/audio/WaveformExtractor.cpp
    src/audio/AudioPlayer.h
    src/audio/AudioPlayer.cpp
    src/ui/Mach1LookAndFeel.h
    src/ui/LaneComponent.h
    src/ui/LaneComponent.cpp
    src/ui/LaneListComponent.h
    src/ui/LaneListComponent.cpp
)

# Include directories
target_include_directories(ChannelStacker PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Compile definitions
target_compile_definitions(ChannelStacker PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:ChannelStacker,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:ChannelStacker,JUCE_VERSION>"
)

# Link JUCE modules and resources
target_link_libraries(ChannelStacker PRIVATE
    ChannelStackerResources
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

# Platform-specific settings
if(APPLE)
    # macOS bundle settings
    set_target_properties(ChannelStacker PROPERTIES
        MACOSX_BUNDLE TRUE
    )
elseif(WIN32)
    # Windows settings
    set_target_properties(ChannelStacker PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()
