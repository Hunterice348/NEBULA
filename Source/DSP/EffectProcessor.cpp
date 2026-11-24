#include "EffectProcessor.h"

#include <algorithm>
#include <cmath>

float EffectBase::processSample(float input, int channel,
                                const NebulaEffectParameters& parameters) noexcept
{
    const float wet = processWet(input, channel, parameters);
    if (!std::isfinite(wet) || !std::isfinite(input))
        return std::isfinite(input) ? input : 0.0f;
    const float amount = std::clamp(parameters.wet, 0.0f, 1.0f);
    return input + (wet - input) * amount;
}
