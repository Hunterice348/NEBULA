#pragma once
#include <JuceHeader.h>

class NebulaLookAndFeel : public juce::LookAndFeel_V4
{
public:
    NebulaLookAndFeel();
    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height,
                          float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider&) override;
    static juce::Colour getCyan()   { return juce::Colour(0xFF00E5FF); }
    static juce::Colour getViolet() { return juce::Colour(0xFF7C4DFF); }
    static juce::Colour getBronze() { return juce::Colour(0xFFD4A574); }
    static juce::Colour getBg()     { return juce::Colour(0xFF0A0E1A); }
    static juce::Colour getCard()   { return juce::Colour(0xFF161A26); }
};
