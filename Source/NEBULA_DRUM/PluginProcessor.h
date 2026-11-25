#pragma once
#include <JuceHeader.h>
#include "Core/StateManager.h"

class NebulaDrumAudioProcessor : public juce::AudioProcessor,
                                 public juce::AudioProcessorValueTreeState::Listener
{
public:
    NebulaDrumAudioProcessor();
    ~NebulaDrumAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; } // Для MIDI export
    double getTailLengthSeconds() const override { return 0.0; }
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

    // Доступно для Editor
    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float>* tempoParam = nullptr;
    std::atomic<float>* swingParam = nullptr;
    std::atomic<float>* volumeParam = nullptr;

    // Состояние секвенсора: 6 инструментов x 16 шагов
    bool sequencerGrid[6][16]; 

private:
    void triggerDrum(int instrumentIndex, float velocity);
    void processDrumVoice(int voiceIndex, juce::AudioBuffer<float>& buffer, int startSample, int numSamples);

    struct DrumVoice {
        bool isActive = false;
        int instrumentIndex = 0;
        double phase = 0.0;
        float envelope = 0.0f;
        float frequency = 0.0f;
    };

    DrumVoice voices[8]; // 8-голосная полифония
    double sampleRate = 44100.0;
    int samplesPerBlock = 512;
    
    // Секвенсор
    int64 currentSample = 0;
    int64 nextStepSample = 0;
    int currentStep = 0;
    double samplesPerStep = 0.0;

    int currentUIWidth = 800;
    int currentUIHeight = 600;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NebulaDrumAudioProcessor)
};
