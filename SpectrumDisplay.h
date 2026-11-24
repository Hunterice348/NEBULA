#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class SpectrumDisplay : public juce::Component, private juce::Timer
{
public:
    explicit SpectrumDisplay(NebulaAudioProcessor& p);
    ~SpectrumDisplay() override;
    void paint(juce::Graphics&) override;
    void timerCallback() override;
private:
    NebulaAudioProcessor& proc;
    nebula::SpectrumFrame localFrame {};
    bool hasFrame = false; // message-thread-only
};
