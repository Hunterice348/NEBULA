#pragma once
#include <JuceHeader.h>
#include "Core/DSP/Compressor.h"

class NebulaFXAudioProcessor : public juce::AudioProcessor,
                               public juce::AudioProcessorValueTreeState::Listener
{
public:
    NebulaFXAudioProcessor();
    ~NebulaFXAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    void parameterChanged(const juce::String& parameterID, float newValue) override;

    // Для сохранения размера UI
    void setCurrentUIWidth(int w) { currentUIWidth = w; }
    void setCurrentUIHeight(int h) { currentUIHeight = h; }

    juce::AudioProcessorValueTreeState apvts;

private:
    Nebula::Compressor compressor;
    int currentUIWidth = 800;
    int currentUIHeight = 600;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NebulaFXAudioProcessor)
};
