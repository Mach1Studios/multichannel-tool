/*
    ChannelStacker - Lane Component Implementation
*/

#include "LaneComponent.h"
#include "Mach1LookAndFeel.h"

LaneComponent::LaneComponent(Lane* lane, int displayIndex)
    : laneData(lane), currentDisplayIndex(displayIndex)
{
    // Setup name label with Mach1 colors
    nameLabel.setFont(juce::FontOptions(12.0f).withStyle("Bold"));
    nameLabel.setJustificationType(juce::Justification::centredLeft);
    nameLabel.setColour(juce::Label::textColourId, Mach1LookAndFeel::Colors::textPrimary);
    addAndMakeVisible(nameLabel);

    // Setup info label
    infoLabel.setFont(juce::FontOptions(10.0f));
    infoLabel.setJustificationType(juce::Justification::centredLeft);
    infoLabel.setColour(juce::Label::textColourId, Mach1LookAndFeel::Colors::textSecondary);
    addAndMakeVisible(infoLabel);

    // Setup delete button with Mach1 style (red for delete)
    deleteButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF3A1A1A));
    deleteButton.setColour(juce::TextButton::textColourOffId, Mach1LookAndFeel::Colors::statusError);
    deleteButton.onClick = [this]()
    {
        listeners.call([this](Listener& l) { l.laneDeleteRequested(this); });
    };
    addAndMakeVisible(deleteButton);

    updateLabels();
}

void LaneComponent::setDisplayIndex(int index)
{
    currentDisplayIndex = index;
    updateLabels();
}

void LaneComponent::updateLabels()
{
    if (laneData != nullptr)
    {
        // Show current position index prominently
        juce::String name;
        if (currentDisplayIndex >= 0)
            name = "[" + juce::String(currentDisplayIndex + 1) + "] ";
        name += laneData->displayName;
        nameLabel.setText(name, juce::dontSendNotification);

        juce::String info = "Src: Stream " + juce::String(laneData->streamIndex)
                          + ", Ch " + juce::String(laneData->channelIndex + 1)
                          + "/" + juce::String(laneData->totalChannels);
        if (laneData->sampleRate > 0)
            info += " @ " + juce::String(laneData->sampleRate / 1000.0, 1) + "kHz";
        infoLabel.setText(info, juce::dontSendNotification);
    }
}

void LaneComponent::waveformUpdated()
{
    repaint();
}

void LaneComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    // Background - Mach1 panel style
    g.setColour(Mach1LookAndFeel::Colors::panelBackground);
    g.fillRoundedRectangle(bounds.toFloat(), 2.0f);

    // Border - subtle
    g.setColour(Mach1LookAndFeel::Colors::border);
    g.drawRoundedRectangle(bounds.toFloat().reduced(0.5f), 2.0f, 1.0f);

    // Drag handle area
    auto dragHandle = bounds.removeFromLeft(kDragHandleWidth);
    g.setColour(Mach1LookAndFeel::Colors::headerBackground);
    g.fillRect(dragHandle.reduced(1, 4));

    // Draw grip lines
    g.setColour(Mach1LookAndFeel::Colors::textSecondary);
    int gripY = dragHandle.getCentreY();
    for (int i = -2; i <= 2; ++i)
    {
        int y = gripY + i * 4;
        g.drawHorizontalLine(y, static_cast<float>(dragHandle.getX()) + 5.0f, 
                             static_cast<float>(dragHandle.getRight()) - 5.0f);
    }

    // Waveform area
    bounds.removeFromTop(kHeaderHeight);
    auto waveformArea = bounds.reduced(kMargin);

    if (laneData != nullptr && laneData->waveform.isReady)
    {
        drawWaveform(g, waveformArea);
    }
    else
    {
        drawLoadingIndicator(g, waveformArea);
    }
}

void LaneComponent::drawWaveform(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    if (laneData == nullptr || !laneData->waveform.isReady)
        return;

    const auto& envelope = laneData->waveform;
    if (envelope.numPoints == 0)
        return;

    float width = static_cast<float>(bounds.getWidth());
    float height = static_cast<float>(bounds.getHeight());
    float centreY = static_cast<float>(bounds.getCentreY());
    float halfHeight = height * 0.5f;

    // Draw waveform as filled shape
    juce::Path waveformPath;
    bool pathStarted = false;

    // Scale factor for better visibility
    float maxPeak = 0.0f;
    for (size_t i = 0; i < static_cast<size_t>(envelope.numPoints); ++i)
    {
        maxPeak = std::max(maxPeak, std::abs(envelope.minValues[i]));
        maxPeak = std::max(maxPeak, std::abs(envelope.maxValues[i]));
    }

    float scale = maxPeak > 0.0f ? 1.0f / maxPeak : 1.0f;
    scale = std::min(scale, 1.0f);  // Don't amplify beyond 0dB

    // Draw top half (max values)
    for (size_t i = 0; i < static_cast<size_t>(envelope.numPoints); ++i)
    {
        float x = static_cast<float>(bounds.getX()) + (static_cast<float>(i) / static_cast<float>(envelope.numPoints - 1)) * width;
        float y = centreY - envelope.maxValues[i] * scale * halfHeight;

        if (!pathStarted)
        {
            waveformPath.startNewSubPath(x, y);
            pathStarted = true;
        }
        else
        {
            waveformPath.lineTo(x, y);
        }
    }

    // Draw bottom half (min values) in reverse
    for (int i = envelope.numPoints - 1; i >= 0; --i)
    {
        float x = static_cast<float>(bounds.getX()) + (static_cast<float>(i) / static_cast<float>(envelope.numPoints - 1)) * width;
        float y = centreY - envelope.minValues[static_cast<size_t>(i)] * scale * halfHeight;
        waveformPath.lineTo(x, y);
    }

    waveformPath.closeSubPath();

    // Fill waveform with Mach1 accent color
    g.setColour(Mach1LookAndFeel::Colors::waveformFill.withAlpha(0.6f));
    g.fillPath(waveformPath);

    // Draw outline
    g.setColour(Mach1LookAndFeel::Colors::waveformOutline);
    g.strokePath(waveformPath, juce::PathStrokeType(1.0f));

    // Draw center line
    g.setColour(Mach1LookAndFeel::Colors::border);
    g.drawHorizontalLine(static_cast<int>(centreY), static_cast<float>(bounds.getX()),
                         static_cast<float>(bounds.getRight()));
}

void LaneComponent::drawLoadingIndicator(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    g.setColour(Mach1LookAndFeel::Colors::textSecondary);
    g.setFont(juce::FontOptions(10.0f));
    g.drawText("Loading waveform...", bounds, juce::Justification::centred);

    // Draw simple loading bar
    auto barBounds = bounds.withSizeKeepingCentre(200, 3);
    g.setColour(Mach1LookAndFeel::Colors::border);
    g.fillRoundedRectangle(barBounds.toFloat(), 1.5f);

    // Animated fill would require timer - for now just show partial
    g.setColour(Mach1LookAndFeel::Colors::accent);
    g.fillRoundedRectangle(barBounds.withWidth(barBounds.getWidth() / 3).toFloat(), 1.5f);
}

void LaneComponent::resized()
{
    auto bounds = getLocalBounds();
    bounds.removeFromLeft(kDragHandleWidth);

    auto header = bounds.removeFromTop(kHeaderHeight);
    header.reduce(kMargin, 2);

    // Delete button on the right
    deleteButton.setBounds(header.removeFromRight(kDeleteButtonWidth));
    header.removeFromRight(kMargin);

    // Info label takes right portion
    infoLabel.setBounds(header.removeFromRight(200));

    // Name label takes the rest
    nameLabel.setBounds(header);
}

bool LaneComponent::hitTest(int x, int /*y*/)
{
    // Return false for drag handle area (left side) so events pass to parent
    // Return true for rest of component so buttons etc. work
    return x >= kDragHandleWidth;
}

void LaneComponent::mouseDown(const juce::MouseEvent& /*e*/)
{
    // Non-drag-handle clicks
}

void LaneComponent::mouseDrag(const juce::MouseEvent& /*e*/)
{
    // Not used
}

void LaneComponent::mouseUp(const juce::MouseEvent& /*e*/)
{
    // Not used
}
