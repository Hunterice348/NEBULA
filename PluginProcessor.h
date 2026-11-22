#pragma once
#include <JuceHeader.h>
#include "Rack.h"
#include "PresetManager.h"

class NebulaAudioProcessor : public juce::AudioProcessor
{
public:
    NebulaAudioProcessor();
    ~NebulaAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;
    const juce::String getName() const override;

    double getTailLengthSeconds() const override { return 2.0; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int) override;
    const juce::String getProgramName(int) override;
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    Rack rack;
    PresetManager presetManager;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Для визуализации
    static constexpr int FFT_SIZE = 1024;
    std::array<float, FFT_SIZE / 2> spectrumData {};
    std::atomic<bool> spectrumReady { false };

private:
    juce::dsp::FFT fft { 10 };
    juce::dsp::WindowingFunction<float> window { FFT_SIZE, juce::dsp::WindowingFunction<float>::hann };
    std::array<float, FFT_SIZE> fftBuffer {};

    juce::AudioBuffer<float> dryBuffer;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> inputGainSmoothed, outputGainSmoothed, mixSmoothed;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NebulaAudioProcessor)
};
