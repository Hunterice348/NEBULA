#include "Core/AudioMetering.h"
#include "Core/ParameterSmoothing.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace
{
    bool near(float actual, float expected, float tolerance = 0.0001f)
    {
        return std::abs(actual - expected) <= tolerance;
    }

    bool testSmootherConvergesAcrossBlocks()
    {
        NebulaParameterSmoothing smoother;
        smoother.prepare(1000.0, 0.01);
        smoother.setCurrentAndTarget(0.0f);
        smoother.setTarget(1.0f);

        for (int block = 0; block < 5; ++block)
        {
            smoother.setTarget(1.0f);
            for (int sample = 0; sample < 2; ++sample)
                smoother.getNextValue();
        }

        return !smoother.isSmoothing()
            && near(smoother.getCurrentValue(), 1.0f);
    }

    bool testRampChangePreservesCurrentAndTarget()
    {
        NebulaParameterSmoothing smoother;
        smoother.prepare(1000.0, 0.01);
        smoother.setCurrentAndTarget(0.0f);
        smoother.setTarget(1.0f);

        for (int i = 0; i < 3; ++i)
            smoother.getNextValue();

        const float beforeChange = smoother.getCurrentValue();
        smoother.setRampDuration(2000.0, 0.02);

        if (!near(smoother.getCurrentValue(), beforeChange)
            || !smoother.isSmoothing())
            return false;

        smoother.skip(40);
        return !smoother.isSmoothing()
            && near(smoother.getCurrentValue(), 1.0f);
    }

    bool testInvalidValuesDoNotPoisonSmoother()
    {
        NebulaParameterSmoothing smoother;
        smoother.prepare(std::numeric_limits<double>::quiet_NaN(),
                         std::numeric_limits<double>::infinity());
        smoother.setCurrentAndTarget(0.25f);

        smoother.setTarget(std::numeric_limits<float>::quiet_NaN());
        smoother.setTarget(std::numeric_limits<float>::infinity());
        smoother.setCurrentAndTarget(-std::numeric_limits<float>::infinity());
        smoother.setRampDuration(std::numeric_limits<double>::infinity(),
                                 std::numeric_limits<double>::quiet_NaN());

        return std::isfinite(smoother.getCurrentValue())
            && near(smoother.getCurrentValue(), 0.25f)
            && !smoother.isSmoothing();
    }

    bool testSkipClampsNegativeSamples()
    {
        NebulaParameterSmoothing smoother;
        smoother.prepare(1000.0, 0.01);
        smoother.setCurrentAndTarget(0.0f);
        smoother.setTarget(1.0f);

        const float initial = smoother.getCurrentValue();
        smoother.skip(-10);

        if (!near(smoother.getCurrentValue(), initial))
            return false;

        smoother.skip(10);
        return !smoother.isSmoothing()
            && near(smoother.getCurrentValue(), 1.0f);
    }

    bool testMeteringIgnoresNonFiniteSamples()
    {
        NebulaAudioMetering meter;
        meter.prepare(48000.0);

        juce::AudioBuffer<float> buffer(2, 4);
        buffer.clear();
        buffer.setSample(0, 0, 0.5f);
        buffer.setSample(0, 1, -0.5f);
        buffer.setSample(0, 2, std::numeric_limits<float>::quiet_NaN());
        buffer.setSample(1, 0, std::numeric_limits<float>::infinity());

        meter.process(buffer);
        return near(meter.getPeak(), 0.5f)
            && near(meter.getRMS(), 0.25f)
            && std::isfinite(meter.getPeakDb())
            && std::isfinite(meter.getRMSDb());
    }
}

int main()
{
    struct Test
    {
        const char* name;
        bool (*run)();
    };

    const Test tests[] = {
        { "smoother convergence across blocks", testSmootherConvergesAcrossBlocks },
        { "ramp change preserves current and target", testRampChangePreservesCurrentAndTarget },
        { "non-finite values are rejected", testInvalidValuesDoNotPoisonSmoother },
        { "negative skip count is clamped", testSkipClampsNegativeSamples },
        { "metering ignores non-finite samples", testMeteringIgnoresNonFiniteSamples },
    };

    int failures = 0;
    for (const auto& test : tests)
    {
        const bool passed = test.run();
        std::cout << (passed ? "[PASS] " : "[FAIL] ") << test.name << '\n';
        failures += passed ? 0 : 1;
    }

    std::cout << "\nFailed tests: " << failures << '\n';
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
