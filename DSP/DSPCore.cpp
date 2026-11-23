
#include "DSPCore.h"

namespace nebula::dsp
{
    bool isFinite(float value) noexcept
    {
        return std::isfinite(value);
    }

    float finiteOrZero(float value) noexcept
    {
        return isFinite(value) ? value : 0.0f;
    }

    float flushDenormal(float value) noexcept
    {
        if (!isFinite(value)
            || std::abs(value) < denormalThreshold)
            return 0.0f;

        return value;
    }

    float sanitizeSample(float value) noexcept
    {
        return flushDenormal(value);
    }

    float clampFinite(
        float value,
        float lower,
        float upper,
        float fallback) noexcept
    {
        if (!isFinite(lower) || !isFinite(upper))
            return finiteOrZero(fallback);

        if (lower > upper)
            std::swap(lower, upper);

        if (!isFinite(value))
            value = isFinite(fallback) ? fallback : lower;

        return std::max(lower, std::min(upper, value));
    }

    float decibelsToGain(float decibels) noexcept
    {
        const float boundedDb = clampFinite(
            decibels,
            minimumDecibels,
            maximumDecibels,
            minimumDecibels);

        return std::pow(10.0f, boundedDb * 0.05f);
    }

    float gainToDecibels(float gain) noexcept
    {
        if (!isFinite(gain) || gain <= 0.0f)
            return minimumDecibels;

        const float db = 20.0f * std::log10(gain);

        return clampFinite(
            db,
            minimumDecibels,
            maximumDecibels,
            minimumDecibels);
    }

    float onePoleCoefficient(
        double sampleRate,
        double timeMs) noexcept
    {
        if (!std::isfinite(sampleRate)
            || sampleRate <= 0.0
            || !std::isfinite(timeMs)
            || timeMs <= 0.0)
            return 0.0f;

        const double timeSeconds = timeMs * 0.001;

        const double coefficient = std::exp(
            -1.0 / (sampleRate * timeSeconds));

        if (!std::isfinite(coefficient))
            return 0.0f;

        return static_cast<float>(
            std::max(0.0, std::min(1.0, coefficient)));
    }

    void LinearSmoother::prepare(
        double sampleRate,
        double rampTimeMs) noexcept
    {
        const double safeRate =
            std::isfinite(sampleRate) && sampleRate > 0.0
                ? sampleRate
                : 44100.0;

        const double safeTime =
            std::isfinite(rampTimeMs) && rampTimeMs > 0.0
                ? rampTimeMs
                : 0.0;

        const double requestedSamples =
            safeRate * safeTime * 0.001;

        rampLength = requestedSamples >= 1.0
            ? static_cast<std::size_t>(
                std::llround(requestedSamples))
            : 0;

        samplesRemaining = 0;
        step = 0.0f;
    }

    void LinearSmoother::reset(float value) noexcept
    {
        currentValue = finiteOrZero(value);
        targetValue = currentValue;

        step = 0.0f;
        samplesRemaining = 0;
    }

    void LinearSmoother::setCurrentAndTargetValue(
        float value) noexcept
    {
        reset(value);
    }

    void LinearSmoother::setTargetValue(
        float value) noexcept
    {
        if (!isFinite(value) || value == targetValue)
            return;

        targetValue = value;

        if (rampLength == 0)
        {
            reset(targetValue);
            return;
        }

        samplesRemaining = rampLength;

        step = (targetValue - currentValue)
             / static_cast<float>(rampLength);
    }

    float LinearSmoother::getNextValue() noexcept
    {
        if (samplesRemaining == 0)
            return currentValue;

        currentValue += step;
        --samplesRemaining;

        if (samplesRemaining == 0)
            currentValue = targetValue;

        return currentValue;
    }
}
