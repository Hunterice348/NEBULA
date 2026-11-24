#pragma once

#include "EffectProcessor.h"
#include <vector>

class NebulaDelayEffect final : public EffectBase
{
public:
    void prepare(double sampleRate, int channels) override;
    void reset() noexcept override;

protected:
    float processWet(float input, int channel,
                     const NebulaEffectParameters& parameters) noexcept override;

private:
    std::vector<std::vector<float>> delayLines;
    std::vector<std::size_t> writePositions;
    std::size_t delayCapacity = 0;
};

class NebulaSaturationEffect final : public EffectBase
{
public:
    void prepare(double sampleRate, int channels) override;
    void reset() noexcept override;

protected:
    float processWet(float input, int channel,
                     const NebulaEffectParameters& parameters) noexcept override;
};
