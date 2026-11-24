#pragma once

#include <JuceHeader.h>

class NebulaParameterSmoothing
{
public:
    NebulaParameterSmoothing() = default;

    void prepare(double sampleRate, double rampSeconds = 0.02)
    {
        const double safeRate = juce::jmax(1.0, sampleRate);
        const double safeRamp = juce::jmax(0.001, rampSeconds);

        smoother.reset(safeRate, safeRamp);
        smoother.setCurrentAndTargetValue(0.0f);
    }

    void setRampDuration(double sampleRate, double seconds)
    {
        smoother.reset(
            juce::jmax(1.0, sampleRate),
            juce::jmax(0.001, seconds)
        );
    }

    void setTarget(float value)
    {
        if (std::isfinite(value))
            smoother.setTargetValue(value);
    }

    void setCurrentAndTarget(float value)
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
