
#pragma once

#include <JuceHeader.h>
#include <atomic>

class NebulaSynthSound final : public juce::SynthesiserSound
{
public:
    bool appliesToNote(int) override
    {
        return true;
    }

    bool appliesToChannel(int) override
    {
        return true;
    }
};

class NebulaSynthVoice final : public juce::SynthesiserVoice
{
public:
    NebulaSynthVoice(
        std::atomic<float>* masterGain,
        std::atomic<float>* attack,
        std::atomic<float>* decay,
        std::atomic<float>* sustain,
        std::atomic<float>* release,
        std::atomic<float>* waveform);

    bool canPlaySound(juce::SynthesiserSound* sound) override;

    void startNote(
        int midiNoteNumber,
        float velocity,
        juce::SynthesiserSound* sound,
        int currentPitchWheelPosition) override;

    void stopNote(float velocity, bool allowTailOff) override;

    void pitchWheelMoved(int newPitchWheelValue) override;

    void controllerMoved(
        int controllerNumber,
        int newControllerValue) override;

    void renderNextBlock(
        juce::AudioBuffer<float>& outputBuffer,
        int startSample,
        int numSamples) override;

private:
    std::atomic<float>* masterGainParameter = nullptr;
    std::atomic<float>* attackParameter = nullptr;
    std::atomic<float>* decayParameter = nullptr;
    std::atomic<float>* sustainParameter = nullptr;
    std::atomic<float>* releaseParameter = nullptr;
    std::atomic<float>* waveformParameter = nullptr;

    juce::ADSR envelope;

    double phase = 0.0;
    double phaseDelta = 0.0;

    float level = 0.0f;

    int currentNote = -1;
    int pitchWheelPosition = 8192;

    void updateFrequency() noexcept;
    float renderOscillator(int waveform) noexcept;
    static float polyBlep(float phase, float phaseIncrement) noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NebulaSynthVoice)
};
