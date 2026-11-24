#include "BasicEffects.h"

#include <algorithm>
#include <cmath>

void NebulaDelayEffect::prepare(double sampleRate, int channels)
{
    const double safeRate = std::isfinite(sampleRate) && sampleRate > 0.0
        ? sampleRate : 44100.0;
    const int safeChannels = std::max(1, channels);
    delayCapacity = static_cast<std::size_t>(std::ceil(safeRate * 2.0)) + 1U;
    delayLines.assign(static_cast<std::size_t>(safeChannels),
                      std::vector<float>(delayCapacity, 0.0f));
    writePositions.assign(static_cast<std::size_t>(safeChannels), 0U);
}

void NebulaDelayEffect::reset() noexcept
{
    for (auto& line : delayLines)
        std::fill(line.begin(), line.end(), 0.0f);
    std::fill(writePositions.begin(), writePositions.end(), 0U);
}

float NebulaDelayEffect::processWet(float input, int channel,
                                    const NebulaEffectParameters& parameters) noexcept
{
    if (channel < 0 || static_cast<std::size_t>(channel) >= delayLines.size()
        || delayCapacity < 2U)
        return input;

    auto& line = delayLines[static_cast<std::size_t>(channel)];
    auto& write = writePositions[static_cast<std::size_t>(channel)];
    const auto maximumDelay = delayCapacity - 1U;
    const auto delay = std::min(maximumDelay,
        static_cast<std::size_t>(std::max(1, parameters.delaySamples)));
    const auto read = (write + delayCapacity - delay) % delayCapacity;
    const float delayed = line[read];
    const float feedback = std::clamp(parameters.feedback, 0.0f, 0.95f);
    line[write] = input + delayed * feedback;
    if (!std::isfinite(line[write]))
        line[write] = 0.0f;
    if (++write == delayCapacity)
        write = 0U;
    return delayed;
}

void NebulaSaturationEffect::prepare(double sampleRate, int channels)
{
    (void) sampleRate;
    (void) channels;
}

void NebulaSaturationEffect::reset() noexcept {}

float NebulaSaturationEffect::processWet(float input, int channel,
                                         const NebulaEffectParameters& parameters) noexcept
{
    (void) channel;
    const float drive = std::clamp(parameters.drive, 1.0f, 30.0f);
    return std::tanh(input * drive);
}
