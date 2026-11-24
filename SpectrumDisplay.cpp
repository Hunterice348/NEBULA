#include "PluginEditor.h"

SpectrumDisplay::SpectrumDisplay(NebulaAudioProcessor& p) : proc(p) { startTimerHz(30); }
SpectrumDisplay::~SpectrumDisplay() { stopTimer(); }

void SpectrumDisplay::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xFF0A0E1A));
    g.fillRoundedRectangle(bounds, 6.0f);
    g.setColour(juce::Colour(0xFF00E5FF).withAlpha(0.15f));
    g.drawRoundedRectangle(bounds, 6.0f, 1.0f);
    if (!hasFrame) return;

    const float width = bounds.getWidth();
    const float height = bounds.getHeight();
    const int visibleBins = 256;
    const float binWidth = width / (float) visibleBins;
    juce::Path path;
    path.startNewSubPath(0.0f, height);
    for (int i = 0; i < visibleBins; ++i)
        path.lineTo((float) i * binWidth, height - std::pow(localFrame.bins[(size_t) i], 0.6f) * height * 0.95f);
    path.lineTo(width, height);
    path.closeSubPath();
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xFF00E5FF), 0, height * 0.2f,
                                            juce::Colour(0xFF7C4DFF), 0, height, false));
    g.fillPath(path);
    g.setColour(juce::Colour(0xFF00E5FF).withAlpha(0.8f));
    g.strokePath(path, juce::PathStrokeType(1.5f));
}

void SpectrumDisplay::timerCallback()
{
    nebula::SpectrumFrame next;
    bool received = false;
    while (proc.tryPopSpectrumFrame(next))
    {
        localFrame = next;
        received = true;
    }
    if (received)
    {
        hasFrame = true;
        repaint();
    }
}
