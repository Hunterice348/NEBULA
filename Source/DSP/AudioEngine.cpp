#include "AudioEngine.h"

#include <algorithm>
#include <cmath>

namespace
{
    float finiteOr(float value, float fallback) noexcept
    {
        return std::isfinite(value) ? value : fallback;
    }
}

void NebulaAudioEngine::prepare(double rate, int channels, int maximumBlockSize)
{
    (void) maximumBlockSize; // Processing is sample-looped and supports variable blocks.
    prepared = false;
    sampleRate = std::isfinite(rate) && rate > 0.0 ? rate : 44100.0;
    preparedChannels = std::max(1, channels);

    inputGainSmoother.prepare(sampleRate);
    outputGainSmoother.prepare(sampleRate);
    wetSmoother.prepare(sampleRate);
    driveSmoother.prepare(sampleRate);
    feedbackSmoother.prepare(sampleRate);
    delaySmoother.prepare(sampleRate);

    const NebulaAudioParameters defaults;
    inputGainSmoother.setCurrentAndTarget(defaults.inputGain);
    outputGainSmoother.setCurrentAndTarget(defaults.outputGain);
    wetSmoother.setCurrentAndTarget(defaults.wet);
    driveSmoother.setCurrentAndTarget(defaults.drive);
    feedbackSmoother.setCurrentAndTarget(defaults.feedback);
    delaySmoother.setCurrentAndTarget(defaults.delaySamples);
    mode = defaults.mode;

    delay.prepare(sampleRate, preparedChannels);
    saturation.prepare(sampleRate, preparedChannels);
    reset();
    prepared = true;
}

void NebulaAudioEngine::release() noexcept
{
    // JUCE calls this from the resource lifecycle, outside processBlock.
    prepared = false;
    delay = NebulaDelayEffect {};
    saturation = NebulaSaturationEffect {};
    preparedChannels = 0;
}

void NebulaAudioEngine::reset() noexcept
{
    delay.reset();
    saturation.reset();
}

void NebulaAudioEngine::setParameters(const NebulaAudioParameters& p) noexcept
{
    inputGainSmoother.setTarget(std::clamp(finiteOr(p.inputGain, 1.0f), 0.0f, 32.0f));
    outputGainSmoother.setTarget(std::clamp(finiteOr(p.outputGain, 1.0f), 0.0f, 32.0f));
    wetSmoother.setTarget(std::clamp(finiteOr(p.wet, 0.5f), 0.0f, 1.0f));
    driveSmoother.setTarget(std::clamp(finiteOr(p.drive, 2.0f), 1.0f, 30.0f));
    feedbackSmoother.setTarget(std::clamp(finiteOr(p.feedback, 0.35f), 0.0f, 0.95f));
    delaySmoother.setTarget(std::clamp(finiteOr(p.delaySamples, 1.0f), 1.0f,
        static_cast<float>(std::max(1.0, sampleRate * 2.0))));
    mode = std::clamp(p.mode, 0, 2);
}

void NebulaAudioEngine::processBlock(float* const* audioChannels,
                                     int numChannels, int numSamples) noexcept
{
    if (audioChannels == nullptr || numChannels <= 0 || numSamples <= 0)
        return;

    const int channelCount = std::min(numChannels, preparedChannels);
    if (!prepared || channelCount <= 0)
        return;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float inputGain = inputGainSmoother.getNextValue();
        const float outputGain = outputGainSmoother.getNextValue();
        const float wet = wetSmoother.getNextValue();
        const float drive = driveSmoother.getNextValue();
        const float feedback = feedbackSmoother.getNextValue();
        const int delaySamples = static_cast<int>(std::lround(delaySmoother.getNextValue()));

        NebulaEffectParameters effectParameters;
        effectParameters.wet = wet;
        effectParameters.drive = drive;
        effectParameters.feedback = feedback;
        effectParameters.delaySamples = std::max(1, delaySamples);

        for (int channel = 0; channel < channelCount; ++channel)
        {
            float* data = audioChannels[channel];
            if (data == nullptr)
                continue;
            const float input = std::isfinite(data[sample]) ? data[sample] : 0.0f;
            const float gained = input * inputGain;
            float output = gained;
            if (mode == 1)
                output = delay.processSample(gained, channel, effectParameters);
            else if (mode == 2)
                output = saturation.processSample(gained, channel, effectParameters);
            output *= outputGain;
            data[sample] = std::isfinite(output) ? output : 0.0f;
        }
    }
}
