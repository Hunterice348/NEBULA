#pragma once

#include <JuceHeader.h>
#include <atomic>
#include "DSP/AudioEngine.h"

class NebulaFXAudioProcessor final : public juce::AudioProcessor
{
public:
    NebulaFXAudioProcessor();
    ~NebulaFXAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer,
                      juce::MidiBuffer& midiMessages) override;
    void processBlockBypassed(juce::AudioBuffer<float>& buffer,
                              juce::MidiBuffer& midiMessages) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;
    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState parameters;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    NebulaAudioEngine audioEngine;
    std::atomic<float>* inputParameter = nullptr;
    std::atomic<float>* outputParameter = nullptr;
    std::atomic<float>* mixParameter = nullptr;
    std::atomic<float>* driveParameter = nullptr;
    std::atomic<float>* delayParameter = nullptr;
    std::atomic<float>* feedbackParameter = nullptr;
    std::atomic<float>* modeParameter = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NebulaFXAudioProcessor)
};
