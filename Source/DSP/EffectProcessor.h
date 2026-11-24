#pragma once

#include <cstddef>

struct NebulaEffectParameters
{
    float wet = 1.0f;
    float drive = 1.0f;
    float feedback = 0.0f;
    int delaySamples = 1;
};

// Per-sample processing contract: prepare/reset may allocate only during
// preparation; processSample must be bounded and allocation-free.
class EffectProcessor
{
public:
    virtual ~EffectProcessor() = default;
    virtual void prepare(double sampleRate, int channels) = 0;
    virtual void reset() noexcept = 0;
    virtual float processSample(float input, int channel,
                                const NebulaEffectParameters& parameters) noexcept = 0;
};

// Common linear wet/dry and safe-effect-bypass behavior for concrete effects.
class EffectBase : public EffectProcessor
{
public:
    float processSample(float input, int channel,
                        const NebulaEffectParameters& parameters) noexcept final;

protected:
    virtual float processWet(float input, int channel,
                             const NebulaEffectParameters& parameters) noexcept = 0;
};
