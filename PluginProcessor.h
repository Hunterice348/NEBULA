
#pragma once

#include <JuceHeader.h>
#include <array>
#include <cstdint>

#include "Rack.h"
#include "PresetManager.h"
#include "SpectrumFrameQueue.h"

class NebulaAudioProcessor : public juce::AudioProcessor
{
public:
    NebulaAudioProcessor();
    ~NebulaAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(
        juce::AudioBuffer<float>&,
        juce::MidiBuffer&) override;

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

    static juce::AudioProcessorValueTreeState::ParameterLayout
        createParameterLayout();

    static constexpr int FFT_SIZE = 1024;

    bool tryPopSpectrumFrame(
        nebula::SpectrumFrame& frame) noexcept
    {
        return spectrumFrames.tryPop(frame);
    }

private:
    // FFT engine.
    juce::dsp::FFT fft { 10 };

    juce::dsp::WindowingFunction<float> window {
        FFT_SIZE,
        juce::dsp::WindowingFunction<float>::hann
    };

    // Incoming samples collected across arbitrary host blocks.
    std::array<float, FFT_SIZE> fftInputBuffer {};

    // JUCE real-only FFT requires a 2 * FFT_SIZE work buffer.
    std::array<float, FFT_SIZE * 2> fftWorkBuffer {};

    int fftWritePosition = 0;

    std::uint64_t spectrumSequence = 0;

    nebula::SpectrumFrameQueue spectrumFrames;

    // Global dry signal storage.
    juce::AudioBuffer<float> dryBuffer;

    // Smoothed master controls.
    juce::SmoothedValue<
        float,
        juce::ValueSmoothingTypes::Linear
    > inputGainSmoothed;

    juce::SmoothedValue<
        float,
        juce::ValueSmoothingTypes::Linear
    > outputGainSmoothed;

    juce::SmoothedValue<
        float,
        juce::ValueSmoothingTypes::Linear
    > mixSmoothed;

    // Internal helpers.
    void pushSpectrumSample(float sample) noexcept;
    void publishSpectrumFrame() noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(
        NebulaAudioProcessor
    )
};
