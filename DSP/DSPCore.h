
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace nebula::dsp
{
    constexpr float denormalThreshold = 1.0e-30f;
    constexpr float minimumDecibels = -160.0f;
    constexpr float maximumDecibels = 48.0f;

    bool isFinite(float value) noexcept;
    float finiteOrZero(float value) noexcept;
    float flushDenormal(float value) noexcept;
    float sanitizeSample(float value) noexcept;

    float clampFinite(
        float value,
        float lower,
        float upper,
        float fallback = 0.0f) noexcept;

    float decibelsToGain(float decibels) noexcept;
    float gainToDecibels(float gain) noexcept;

    // Coefficient for:
    // y[n] = target + coefficient * (y[n-1] - target).
    float onePoleCoefficient(
        double sampleRate,
        double timeMs) noexcept;

    class LinearSmoother
    {
    public:
        void prepare(
            double sampleRate,
            double rampTimeMs) noexcept;

        void reset(float value) noexcept;

        void setCurrentAndTargetValue(
            float value) noexcept;

        void setTargetValue(float value) noexcept;

        float getNextValue() noexcept;

        float getCurrentValue() const noexcept
        {
            return currentValue;
        }

        float getTargetValue() const noexcept
        {
            return targetValue;
        }

        bool isSmoothing() const noexcept
        {
            return samplesRemaining > 0;
        }

    private:
        float currentValue = 0.0f;
        float targetValue = 0.0f;
        float step = 0.0f;

        std::size_t rampLength = 0;
        std::size_t samplesRemaining = 0;
    };
}
