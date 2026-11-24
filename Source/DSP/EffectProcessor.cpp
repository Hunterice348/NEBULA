#include "EffectProcessor.h"

#include <algorithm>
#include <cmath>

float EffectBase::processSample(float input, int channel,
                                const NebulaEffectParameters& parameters) noexcept
{
    if (!std::isfinite(input))
        return 0.0f;

    const float wet = processWet(input, channel, parameters);
    if (!std::isfinite(wet))
        return input;
    const float amount = std::isfinite(parameters.wet)
        ? std::clamp(parameters.wet, 0.0f, 1.0f) : 0.0f;
    return input + (wet - input) * amount;
}
