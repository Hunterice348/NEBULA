#include "NebulaLookAndFeel.h"

NebulaLookAndFeel::NebulaLookAndFeel()
{
    setColour(juce::Slider::rotarySliderFillColourId, getCyan());
    setColour(juce::Slider::rotarySliderOutlineColourId, getBronze());
    setColour(juce::Slider::thumbColourId, juce::Colour(0xFFE0E0E0));
    setColour(juce::Slider::textBoxTextColourId, getBronze());
    setColour(juce::Label::textColourId, juce::Colour(0xFFB0B0B0));
    setColour(juce::TextButton::buttonColourId, getCard());
    setColour(juce::TextButton::textColourOffId, getCyan());
}

void NebulaLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                          float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                                          juce::Slider&)
{
    auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(4.0f);
    auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
    auto centreX = bounds.getCentreX(), centreY = bounds.getCentreY();
    auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    // Риски
    for (int i = 0; i < 11; ++i) {
        float tickAngle = rotaryStartAngle + (i / 10.0f) * (rotaryEndAngle - rotaryStartAngle);
        bool active = (i / 10.0f) <= sliderPos;
        g.setColour(active ? getCyan() : getBronze().withAlpha(0.4f));
        g.drawLine(centreX + std::sin(tickAngle) * (radius + 2), centreY - std::cos(tickAngle) * (radius + 2),
                   centreX + std::sin(tickAngle) * (radius + 6), centreY - std::cos(tickAngle) * (radius + 6), 1.5f);
    }

    // Тело
    g.setColour(getBronze().darker(0.4f));
    g.fillEllipse(centreX - radius, centreY - radius, radius * 2, radius * 2);
    juce::ColourGradient grad(getBronze().brighter(0.2f), centreX, centreY - radius,
                              getBronze().darker(0.4f), centreX, centreY + radius, false);
    g.setGradientFill(grad);
    g.fillEllipse(centreX - radius + 3, centreY - radius + 3, (radius - 3) * 2, (radius - 3) * 2);

    // Указатель
    g.setColour(juce::Colour(0xFFE0E0E0));
    juce::Point<float> tip(centreX + std::sin(angle) * (radius - 6), centreY - std::cos(angle) * (radius - 6));
    g.drawLine(juce::Line<float>({centreX, centreY}, tip), 2.5f);
}
