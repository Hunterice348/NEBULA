#pragma once

#include <cmath>

// Audio-thread-owned linear ramp. Targets are set and consumed on the audio
// thread after parameter values have been copied from their atomics.
class NebulaParameterSmoothing
{
public:
    void prepare(double sampleRate, double rampSeconds = 0.02) noexcept
    {
        const double safeRate = std::isfinite(sampleRate) && sampleRate > 0.0
            ? sampleRate : 44100.0;
        const double safeRamp = std::isfinite(rampSeconds) && rampSeconds > 0.0
            ? rampSeconds : 0.02;
        rampLength = safeRate * safeRamp;
        if (rampLength < 1.0)
            rampLength = 1.0;
        remaining = 0;
        step = 0.0f;
    }

    void setTarget(float value) noexcept
    {
        if (!std::isfinite(value) || value == target)
            return;

        target = value;
        remaining = rampLength;
        if (remaining <= 1.0)
        {
            current = target;
            remaining = 0;
            step = 0.0f;
        }
        else
        {
            step = static_cast<float>((static_cast<double>(target) - current) / remaining);
        }
    }

    void setCurrentAndTarget(float value) noexcept
    {
        if (!std::isfinite(value))
            return;
        current = target = value;
        remaining = 0;
        step = 0.0f;
    }

    float getNextValue() noexcept
    {
        if (remaining > 0)
        {
            current += step;
            if (--remaining <= 0)
            {
                current = target;
                step = 0.0f;
            }
        }
        return current;
    }

    float getCurrentValue() const noexcept { return current; }
    bool isSmoothing() const noexcept { return remaining > 0; }

    void skip(int samples) noexcept
    {
        if (samples <= 0 || remaining <= 0)
            return;
        const auto count = samples < remaining ? samples : static_cast<int>(remaining);
        current += step * static_cast<float>(count);
        remaining -= count;
        if (remaining <= 0)
        {
            current = target;
            step = 0.0f;
        }
    }

private:
    float current = 0.0f;
    float target = 0.0f;
    float step = 0.0f;
    double rampLength = 1.0;
    double remaining = 0.0;
};
