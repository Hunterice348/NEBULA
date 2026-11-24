
#include "AudioMetering.h"

#include <cmath>

void NebulaAudioMetering::prepare(double sampleRate) noexcept
{
    juce::ignoreUnused(sampleRate);
    clear();
}

void NebulaAudioMetering::process(
    const juce::AudioBuffer<float>& buffer) noexcept
{
    const int channels = buffer.getNumChannels();
    const int samples = buffer.getNumSamples();

    if (channels <= 0 || samples <= 0)
        return;

    double sumSquares = 0.0;
    float peak = 0.0f;

    for (int channel = 0; channel < channels; ++channel)
    {
        const float* data = buffer.getReadPointer(channel);

        for (int i = 0; i < samples; ++i)
        {
            const float value = data[i];

            if (!std::isfinite(value))
                continue;

            peak = juce::jmax(peak, std::abs(value));
            sumSquares += static_cast<double>(value) * value;
        }
    }

    const double count =
        static_cast<double>(channels) * samples;

    const float rms = static_cast<float>(
        std::sqrt(sumSquares / count)
    );

    peakLevel.store(juce::jlimit(0.0f, 1.0f, peak),
                    std::memory_order_relaxed);
    rmsLevel.store(juce::jlimit(0.0f, 1.0f, rms),
                   std::memory_order_relaxed);
}

void NebulaAudioMetering::clear() noexcept
{
    peakLevel.store(0.0f, std::memory_order_relaxed);
    rmsLevel.store(0.0f, std::memory_order_relaxed);
}

float NebulaAudioMetering::getPeak() const noexcept
{
    return peakLevel.load(std::memory_order_relaxed);
}

float NebulaAudioMetering::getRMS() const noexcept
{
    return rmsLevel.load(std::memory_order_relaxed);
}

float NebulaAudioMetering::getPeakDb() const noexcept
{
    return juce::Decibels::gainToDecibels(getPeak(), -100.0f);
}

float NebulaAudioMetering::getRMSDb() const noexcept
{
    return juce::Decibels::gainToDecibels(getRMS(), -100.0f);
}
