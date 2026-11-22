#include "SpectrumDisplay.h"

SpectrumDisplay::SpectrumDisplay(NebulaAudioProcessor& p) : proc(p) { startTimerHz(30); }
SpectrumDisplay::~SpectrumDisplay() { stopTimer(); }

void SpectrumDisplay::paint(juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xFF0A0E1A));
    g.fillRoundedRectangle(b, 6.0f);
    g.setColour(juce::Colour(0xFF00E5FF).withAlpha(0.15f));
    g.drawRoundedRectangle(b, 6.0f, 1.0f);

    if (!proc.spectrumReady.load()) return;

    float w = b.getWidth(), h = b.getHeight();
    int bins = NebulaAudioProcessor::FFT_SIZE / 2;
    int visibleBins = juce::jmin(bins, 256);
    float binW = w / (float)visibleBins;

    juce::Path path;
    path.startNewSubPath(0, h);
    for (int i = 0; i < visibleBins; ++i) {
        float v = std::pow(proc.spectrumData[i], 0.6f);
        path.lineTo(i * binW, h - v * h * 0.95f);
    }
    path.lineTo(w, h);
    path.closeSubPath();

    juce::ColourGradient grad(juce::Colour(0xFF00E5FF), 0, h * 0.2f,
                              juce::Colour(0xFF7C4DFF), 0, h, false);
    g.setGradientFill(grad);
    g.fillPath(path);
    g.setColour(juce::Colour(0xFF00E5FF).withAlpha(0.8f));
    g.strokePath(path, juce::PathStrokeType(1.5f));
}

void SpectrumDisplay::timerCallback() { repaint(); }
