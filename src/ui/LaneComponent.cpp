/*
    ChannelStacker - Lane Component Implementation
*/

#include "LaneComponent.h"

LaneComponent::LaneComponent(Lane* lane, int displayIndex)
    : laneData(lane), currentDisplayIndex(displayIndex)
{
    // Setup name label
    nameLabel.setFont(juce::FontOptions(14.0f).withStyle("Bold"));
    nameLabel.setJustificationType(juce::Justification::centredLeft);
    nameLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(nameLabel);

    // Setup info label
    infoLabel.setFont(juce::FontOptions(11.0f));
    infoLabel.setJustificationType(juce::Justification::centredLeft);
    infoLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    addAndMakeVisible(infoLabel);

    // Setup delete button
    deleteButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkred);
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

    // Background
    g.setColour(juce::Colour(0xff2a2a2a));
    g.fillRoundedRectangle(bounds.toFloat(), 5.0f);

    // Border
    g.setColour(juce::Colour(0xff3a3a3a));
    g.drawRoundedRectangle(bounds.toFloat().reduced(0.5f), 5.0f, 1.0f);

    // Drag handle area
    auto dragHandle = bounds.removeFromLeft(kDragHandleWidth);
    g.setColour(juce::Colour(0xff444444));
    g.fillRect(dragHandle.reduced(2, 5));

    // Draw grip lines
    g.setColour(juce::Colour(0xff666666));
    int gripY = dragHandle.getCentreY();
    for (int i = -2; i <= 2; ++i)
    {
        int y = gripY + i * 4;
        g.drawHorizontalLine(y, (float)dragHandle.getX() + 5.0f, (float)dragHandle.getRight() - 5.0f);
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

    // Waveform color
    g.setColour(juce::Colour(0xff4a9eff));

    float width = static_cast<float>(bounds.getWidth());
    float height = static_cast<float>(bounds.getHeight());
    float centreY = bounds.getCentreY();
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
        float x = bounds.getX() + (static_cast<float>(i) / static_cast<float>(envelope.numPoints - 1)) * width;
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
        float x = bounds.getX() + (static_cast<float>(i) / static_cast<float>(envelope.numPoints - 1)) * width;
        float y = centreY - envelope.minValues[static_cast<size_t>(i)] * scale * halfHeight;
        waveformPath.lineTo(x, y);
    }

    waveformPath.closeSubPath();

    // Fill waveform
    g.setColour(juce::Colour(0xff4a9eff).withAlpha(0.7f));
    g.fillPath(waveformPath);

    // Draw outline
    g.setColour(juce::Colour(0xff6ab0ff));
    g.strokePath(waveformPath, juce::PathStrokeType(1.0f));

    // Draw center line
    g.setColour(juce::Colour(0xff555555));
    g.drawHorizontalLine(static_cast<int>(centreY), static_cast<float>(bounds.getX()),
                         static_cast<float>(bounds.getRight()));
}

void LaneComponent::drawLoadingIndicator(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    g.setColour(juce::Colours::grey);
    g.setFont(12.0f);
    g.drawText("Loading waveform...", bounds, juce::Justification::centred);

    // Draw simple loading bar
    auto barBounds = bounds.withSizeKeepingCentre(200, 4);
    g.setColour(juce::Colour(0xff333333));
    g.fillRoundedRectangle(barBounds.toFloat(), 2.0f);

    // Animated fill would require timer - for now just show partial
    g.setColour(juce::Colour(0xff4a9eff));
    g.fillRoundedRectangle(barBounds.withWidth(barBounds.getWidth() / 3).toFloat(), 2.0f);
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
