
#pragma once

#include <JuceHeader.h>
#include <cmath>

class NebulaParameterSmoothing
{
public:
    void prepare(double sampleRate, double rampSeconds = 0.02)
    {
        smoother.reset(
            juce::jmax(1.0, sampleRate),
            juce::jmax(0.001, rampSeconds)
        );

        smoother.setCurrentAndTargetValue(0.0f);
    }

    void setTarget(float value) noexcept
    {
        if (std::isfinite(value))
            smoother.setTargetValue(value);
    }

    void setCurrentAndTarget(float value) noexcept
    {
        if (std::isfinite(value))
            smoother.setCurrentAndTargetValue(value);
    }

    float getNextValue() noexcept
    {
        return smoother.getNextValue();
    }

    float getCurrentValue() const noexcept
    {
        return smoother.getCurrentValue();
    }

    bool isSmoothing() const noexcept
    {
        return smoother.isSmoothing();
    }

    void skip(int samples) noexcept
    {
        smoother.skip(juce::jmax(0, samples));
    }

private:
    juce::SmoothedValue<
        float,
        juce::ValueSmoothingTypes::Linear
    > smoother;
};
