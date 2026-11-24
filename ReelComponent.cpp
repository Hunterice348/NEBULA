#include "ReelComponent.h"

ReelComponent::ReelComponent(NebulaAudioProcessor& p, bool isLeft) : proc(p), left(isLeft) { startTimerHz(60); }
ReelComponent::~ReelComponent() { stopTimer(); }

void ReelComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(6.0f);
    float r = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
    auto cx = bounds.getCentreX(), cy = bounds.getCentreY();

    g.setColour(juce::Colour(0xFF8B6F47));
    g.fillEllipse(cx - r, cy - r, r * 2, r * 2);
    g.setColour(juce::Colour(0xFF1A1A1A));
    g.fillEllipse(cx - r * 0.8f, cy - r * 0.8f, r * 1.6f, r * 1.6f);

    g.setColour(juce::Colour(0xFF2C1810));
    float a = left ? angle : -angle;
    for (int i = 0; i < 6; ++i) {
        float ang = a + i * juce::MathConstants<float>::pi / 3.0f;
        g.drawLine(cx + std::cos(ang) * r * 0.2f, cy + std::sin(ang) * r * 0.2f,
                   cx + std::cos(ang) * r * 0.75f, cy + std::sin(ang) * r * 0.75f, 3.0f);
    }
    g.setColour(juce::Colour(0xFFD4A574));
    g.fillEllipse(cx - r * 0.15f, cy - r * 0.15f, r * 0.3f, r * 0.3f);
}

void ReelComponent::mouseDown(const juce::MouseEvent&)
{
    // Клик по катушке замедляет/ускоряет вращение (визуально)
    angle += 1.0f;
}

void ReelComponent::timerCallback()
{
    angle += 0.05f;
    if (angle > juce::MathConstants<float>::twoPi) angle -= juce::MathConstants<float>::twoPi;
    repaint();
}
