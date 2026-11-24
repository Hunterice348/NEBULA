#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class ReelComponent : public juce::Component, private juce::Timer
{
public:
    ReelComponent(NebulaAudioProcessor& p, bool isLeft);
    ~ReelComponent() override;
    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void timerCallback() override;
private:
    NebulaAudioProcessor& proc;
    bool left;
    float angle = 0.0f;
};
