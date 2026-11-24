#pragma once

#include <JuceHeader.h>

#include <algorithm>
#include <cmath>

class NebulaParameterSmoothing
{
public:
    void prepare(double sampleRate, double rampSeconds = defaultRampSeconds)
    {
        configuredSampleRate = sanitiseSampleRate(sampleRate);
        configuredRampSeconds = sanitiseRampDuration(rampSeconds);

        smoother.reset(configuredSampleRate, configuredRampSeconds);
        smoother.setCurrentAndTargetValue(0.0f);
    }

    void setRampDuration(double sampleRate, double seconds) noexcept
    {
        const float currentValue = smoother.getCurrentValue();
        const float targetValue = smoother.getTargetValue();

        configuredSampleRate = sanitiseSampleRate(sampleRate);
        configuredRampSeconds = sanitiseRampDuration(seconds);

        // Resetting JUCE's ramp restarts its countdown. Restore the current
        // value before reapplying the target to avoid a discontinuity.
        smoother.reset(configuredSampleRate, configuredRampSeconds);
        smoother.setCurrentAndTargetValue(currentValue);
        smoother.setTargetValue(targetValue);
    }

    void setTarget(float value) noexcept
    {
        if (std::isfinite(value)
            && std::isfinite(value - smoother.getCurrentValue()))
        {
            smoother.setTargetValue(value);
        }
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
        smoother.skip(std::max(0, samples));
    }

private:
    static constexpr double defaultSampleRate = 44100.0;
    static constexpr double defaultRampSeconds = 0.02;
    static constexpr double minimumSampleRate = 1.0;
    static constexpr double maximumSampleRate = 768000.0;
    static constexpr double minimumRampSeconds = 0.001;
    static constexpr double maximumRampSeconds = 10.0;

    double sanitiseSampleRate(double sampleRate) const noexcept
    {
        if (!std::isfinite(sampleRate) || sampleRate <= 0.0)
            sampleRate = configuredSampleRate;

        if (!std::isfinite(sampleRate) || sampleRate <= 0.0)
            sampleRate = defaultSampleRate;

        return std::clamp(sampleRate, minimumSampleRate, maximumSampleRate);
    }

    double sanitiseRampDuration(double seconds) const noexcept
    {
        if (!std::isfinite(seconds) || seconds <= 0.0)
            seconds = configuredRampSeconds;

        if (!std::isfinite(seconds) || seconds <= 0.0)
            seconds = defaultRampSeconds;

        return std::clamp(seconds, minimumRampSeconds, maximumRampSeconds);
    }

    double configuredSampleRate = defaultSampleRate;
    double configuredRampSeconds = defaultRampSeconds;

    juce::SmoothedValue<
        float,
        juce::ValueSmoothingTypes::Linear
    > smoother;
};
