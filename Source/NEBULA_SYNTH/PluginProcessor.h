#pragma once
#include <JuceHeader.h>
#include "Core/StateManager.h"

class NebulaSynthAudioProcessor : public juce::AudioProcessor,
                                  public juce::AudioProcessorValueTreeState::Listener
{
public:
    NebulaSynthAudioProcessor();
    ~NebulaSynthAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; } // Для хвоста реверберации/релиза
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;
    void parameterChanged(const juce::String& parameterID, float newValue) override;

    void setCurrentUIWidth(int w) { currentUIWidth = w; }
    void setCurrentUIHeight(int h) { currentUIHeight = h; }

    juce::AudioProcessorValueTreeState apvts;

private:
    struct Voice {
        bool isActive = false;
        int noteNumber = 0;
        double phase = 0.0;
        float envelope = 0.0f;
        float filterState = 0.0f;
        float filterZ1 = 0.0f;
        juce::dsp::StateVariableTPTFilter<float> filter;
    };

    Voice voices[16]; // 16-голосная полифония
    double sampleRate = 44100.0;
    
    int currentUIWidth = 800;
    int currentUIHeight = 600;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NebulaSynthAudioProcessor)
};
