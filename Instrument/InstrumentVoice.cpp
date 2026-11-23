
#include "InstrumentVoice.h"
#include <cmath>

NebulaSynthVoice::NebulaSynthVoice(
    std::atomic<float>* masterGain,
    std::atomic<float>* attack,
    std::atomic<float>* decay,
    std::atomic<float>* sustain,
    std::atomic<float>* release,
    std::atomic<float>* waveform)
    : masterGainParameter(masterGain),
      attackParameter(attack),
      decayParameter(decay),
      sustainParameter(sustain),
      releaseParameter(release),
      waveformParameter(waveform)
{
}

bool NebulaSynthVoice::canPlaySound(juce::SynthesiserSound* sound)
{
    return dynamic_cast<NebulaSynthSound*>(sound) != nullptr;
}

void NebulaSynthVoice::startNote(
    int midiNoteNumber,
    float velocity,
    juce::SynthesiserSound*,
    int currentPitchWheelPosition)
{
    currentNote = midiNoteNumber;
    pitchWheelPosition = currentPitchWheelPosition;
    level = juce::jlimit(0.0f, 1.0f, velocity);

    phase = 0.0;
    updateFrequency();

    juce::ADSR::Parameters parameters;

    parameters.attack = attackParameter
        ? attackParameter->load(std::memory_order_relaxed) : 0.01f;

    parameters.decay = decayParameter
        ? decayParameter->load(std::memory_order_relaxed) : 0.2f;

    parameters.sustain = sustainParameter
        ? sustainParameter->load(std::memory_order_relaxed) : 0.7f;

    parameters.release = releaseParameter
        ? releaseParameter->load(std::memory_order_relaxed) : 0.3f;

    envelope.setSampleRate(getSampleRate());
    envelope.setParameters(parameters);
    envelope.noteOn();
}

void NebulaSynthVoice::stopNote(float, bool allowTailOff)
{
    if (allowTailOff)
    {
        envelope.noteOff();
    }
    else
    {
        envelope.reset();
        clearCurrentNote();
    }
}

void NebulaSynthVoice::pitchWheelMoved(int newPitchWheelValue)
{
    pitchWheelPosition = juce::jlimit(0, 16383, newPitchWheelValue);
    updateFrequency();
}

void NebulaSynthVoice::controllerMoved(int, int)
{
}

void NebulaSynthVoice::updateFrequency() noexcept
{
    if (getSampleRate() <= 0.0 || currentNote < 0)
    {
        phaseDelta = 0.0;
        return;
    }

    const double baseFrequency =
        juce::MidiMessage::getMidiNoteInHertz(currentNote);

    const double wheel =
        static_cast<double>(pitchWheelPosition - 8192) / 8192.0;

    const double semitones = wheel * 2.0;

    const double frequency =
        baseFrequency * std::pow(2.0, semitones / 12.0);

    phaseDelta = frequency / getSampleRate();
    phaseDelta = juce::jlimit(0.0, 0.49, phaseDelta);
}

float NebulaSynthVoice::polyBlep(float t, float dt) noexcept
{
    if (dt <= 0.0f)
        return 0.0f;

    if (t < dt)
    {
        t /= dt;
        return t + t - t * t - 1.0f;
    }

    if (t > 1.0f - dt)
    {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }

    return 0.0f;
}

float NebulaSynthVoice::renderOscillator(int waveform) noexcept
{
    const float t = static_cast<float>(phase);
    const float dt = static_cast<float>(phaseDelta);

    float output = 0.0f;

    switch (waveform)
    {
        case 1:
            output = 2.0f * t - 1.0f;
            output -= polyBlep(t, dt);
            break;

        case 2:
            output = t < 0.5f ? 1.0f : -1.0f;
            output += polyBlep(t, dt);

            {
                float shifted = t + 0.5f;

                if (shifted >= 1.0f)
                    shifted -= 1.0f;

                output -= polyBlep(shifted, dt);
            }
            break;

        case 0:
        default:
            output = std::sin(
                juce::MathConstants<double>::twoPi * phase);
            break;
    }

    phase += phaseDelta;

    if (phase >= 1.0)
        phase -= 1.0;

    return output;
}

void NebulaSynthVoice::renderNextBlock(
    juce::AudioBuffer<float>& outputBuffer,
    int startSample,
    int numSamples)
{
    if (!isVoiceActive())
        return;

    const int channels = outputBuffer.getNumChannels();

    if (channels <= 0 || numSamples <= 0)
        return;

    const float masterGain = masterGainParameter
        ? masterGainParameter->load(std::memory_order_relaxed)
        : 0.8f;

    const int waveform = waveformParameter
        ? static_cast<int>(waveformParameter->load(std::memory_order_relaxed))
        : 0;

    const float safeGain = juce::jlimit(0.0f, 1.0f, masterGain);

    for (int sample = 0; sample < numSamples; ++sample)
    {
        if (!envelope.isActive())
        {
            clearCurrentNote();
            break;
        }

        const float envelopeValue = envelope.getNextSample();
        const float oscillator = renderOscillator(waveform);

        const float value =
            oscillator * level * envelopeValue * safeGain;

        const int outputPosition = startSample + sample;

        for (int channel = 0; channel < channels; ++channel)
        {
            outputBuffer.addSample(channel, outputPosition, value);
        }

        if (!envelope.isActive())
        {
            clearCurrentNote();
            break;
        }
    }
}
