
#include "DSP/DSPCore.h"

#include <iostream>
#include <cmath>
#include <limits>

namespace
{
    bool approximatelyEqual(
        float a,
        float b,
        float tolerance = 1.0e-5f)
    {
        return std::abs(a - b) <= tolerance;
    }
}

int main()
{
    using namespace nebula::dsp;

#define CHECK(expression) \
    do { \
        if (!(expression)) { \
            std::cerr << "DSPCoreTests failed at line " \
                      << __LINE__ << ": " \
                      << #expression << std::endl; \
            return 1; \
        } \
    } while (false)

    // Non-finite values.
    CHECK(
        finiteOrZero(
            std::numeric_limits<float>::quiet_NaN()) == 0.0f);

    CHECK(
        finiteOrZero(
            std::numeric_limits<float>::infinity()) == 0.0f);

    // Denormal protection.
    CHECK(flushDenormal(1.0e-35f) == 0.0f);

    // Audio above 0 dBFS must not be clipped here.
    CHECK(sanitizeSample(2.0f) == 2.0f);
    CHECK(sanitizeSample(-2.0f) == -2.0f);

    // Clamp.
    CHECK(clampFinite(2.0f, 0.0f, 1.0f) == 1.0f);
    CHECK(clampFinite(-2.0f, 0.0f, 1.0f) == 0.0f);

    CHECK(
        clampFinite(
            std::numeric_limits<float>::quiet_NaN(),
            0.0f,
            1.0f,
            0.25f) == 0.25f);

    CHECK(clampFinite(0.5f, 1.0f, 0.0f) == 0.5f);

    // dB / gain.
    CHECK(approximatelyEqual(decibelsToGain(0.0f), 1.0f));
    CHECK(approximatelyEqual(gainToDecibels(1.0f), 0.0f));

    CHECK(gainToDecibels(0.0f) == minimumDecibels);

    CHECK(std::isfinite(
        decibelsToGain(
            std::numeric_limits<float>::infinity())));

    CHECK(std::isfinite(
        gainToDecibels(
            std::numeric_limits<float>::quiet_NaN())));

    // One-pole coefficient.
    CHECK(onePoleCoefficient(48000.0, 0.0) == 0.0f);
    CHECK(onePoleCoefficient(0.0, 10.0) == 0.0f);

    const float coefficient =
        onePoleCoefficient(48000.0, 10.0);

    CHECK(coefficient > 0.0f && coefficient < 1.0f);

    // Linear smoothing.
    LinearSmoother smoother;

    smoother.prepare(1000.0, 10.0);
    smoother.reset(0.0f);
    smoother.setTargetValue(1.0f);

    float previous = 0.0f;

    for (int i = 0; i < 9; ++i)
    {
        const float current = smoother.getNextValue();

        CHECK(current >= previous);
        CHECK(current < 1.0f);

        previous = current;
    }

    CHECK(smoother.isSmoothing());

    CHECK(
        approximatelyEqual(
            smoother.getNextValue(),
            1.0f));

    CHECK(!smoother.isSmoothing());

    // Retargeting.
    smoother.setTargetValue(0.0f);

    CHECK(smoother.isSmoothing());

    float last = smoother.getCurrentValue();

    for (int i = 0; i < 10; ++i)
    {
        const float current = smoother.getNextValue();

        CHECK(current <= last);
        last = current;
    }

    CHECK(
        approximatelyEqual(
            smoother.getCurrentValue(),
            0.0f));

    // Immediate transition when ramp time is zero.
    smoother.prepare(48000.0, 0.0);
    smoother.setTargetValue(0.75f);

    CHECK(
        approximatelyEqual(
            smoother.getCurrentValue(),
            0.75f));

    CHECK(!smoother.isSmoothing());

    return 0;
}
