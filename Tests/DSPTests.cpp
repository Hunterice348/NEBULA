
#include "DSP/AudioEngine.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <vector>

namespace
{
    bool near(float a, float b, float tolerance = 0.0001f)
    {
        return std::abs(a - b) <= tolerance;
    }

    bool testEngineInitialization()
    {
        NebulaAudioEngine engine;
        engine.prepare(48000.0, 2, 64);

        const bool ok = engine.isPrepared()
            && engine.getNumChannels() == 2
            && near(static_cast<float>(engine.getSampleRate()), 48000.0f);

        engine.release();
        return ok && !engine.isPrepared();
    }

    bool testCleanAndVariableBlocks()
    {
        NebulaAudioEngine engine;
        engine.prepare(44100.0, 1, 16);

        NebulaAudioParameters p;
        p.mode = 0;
        p.inputGain = 1.0f;
        p.outputGain = 1.0f;

        engine.setParameters(p);

        std::vector<float> signal(129, 0.25f);
        float* channels[] = { signal.data() };

        engine.processBlock(channels, 1, 3);

        engine.setParameters(p);
        channels[0] += 3;
        engine.processBlock(channels, 1, 126);

        for (const auto sample : signal)
        {
            if (!near(sample, 0.25f))
                return false;
        }

        return true;
    }

    bool testMonoStereoAndNonFiniteSanitizing()
    {
        NebulaAudioEngine engine;
        engine.prepare(48000.0, 2, 32);

        NebulaAudioParameters p;
        p.mode = 2;
        p.drive = 2.0f;
        p.wet = 1.0f;

        engine.setParameters(p);

        float mono[] = {
            0.0f,
            std::numeric_limits<float>::quiet_NaN(),
            0.5f
        };

        float left[] = {
            0.25f,
            std::numeric_limits<float>::infinity(),
            -0.5f
        };

        float right[] = { -0.25f, 0.0f, 0.5f };

        float* monoChannels[] = { mono };
        engine.processBlock(monoChannels, 1, 3);

        float* stereoChannels[] = { left, right };
        engine.processBlock(stereoChannels, 2, 3);

        for (float value : mono)
            if (!std::isfinite(value))
                return false;

        for (float value : left)
            if (!std::isfinite(value))
                return false;

        for (float value : right)
            if (!std::isfinite(value))
                return false;

        return near(mono[0], 0.0f)
            && near(left[1], 0.0f);
    }

    bool testDryWetAndParameterUpdate()
    {
        NebulaAudioEngine engine;
        engine.prepare(1000.0, 1, 8);

        NebulaAudioParameters p;
        p.mode = 2;
        p.drive = 2.0f;
        p.wet = 0.0f;

        engine.setParameters(p);

        float settleDry[32] {};
        float* channels[] = { settleDry };

        engine.processBlock(channels, 1, 32);

        float dry[] = { 0.5f };
        channels[0] = dry;

        engine.processBlock(channels, 1, 1);

        if (!near(dry[0], 0.5f))
            return false;

        p.wet = 1.0f;
        engine.setParameters(p);

        // A constant signal makes it possible to check the final
        // fully-smoothed wet output rather than a silent last sample.
        float wet[64];
        std::fill(std::begin(wet), std::end(wet), 0.5f);

        channels[0] = wet;
        engine.processBlock(channels, 1, 64);

        const float expected = std::tanh(1.0f);

        return std::isfinite(wet[63])
            && near(wet[63], expected, 0.001f);
    }

    bool testDelayAtRatesAndVariableBlocks()
    {
        const double rates[] = { 1000.0, 48000.0 };

        for (const double rate : rates)
        {
            NebulaAudioEngine engine;
            engine.prepare(rate, 1, 17);

            NebulaAudioParameters p;
            p.mode = 1;
            p.delaySamples = std::round(rate * 0.005);
            p.feedback = 0.0f;
            p.wet = 1.0f;

            engine.setParameters(p);

            const int settleLength = static_cast<int>(rate * 0.025);

            std::vector<float> settling(
                static_cast<std::size_t>(settleLength), 0.0f);

            float* channels[] = { settling.data() };
            engine.processBlock(channels, 1, settleLength);
            engine.reset();

            const int delaySamples = static_cast<int>(p.delaySamples);

            std::vector<float> impulse(
                static_cast<std::size_t>(delaySamples + 8), 0.0f);

            impulse[0] = 1.0f;

            int offset = 0;
            const int blockSizes[] = { 1, 3, 11, 5, 23 };
            std::size_t blockIndex = 0;

            while (offset < static_cast<int>(impulse.size()))
            {
                const int count = std::min(
                    blockSizes[blockIndex++ % 5],
                    static_cast<int>(impulse.size()) - offset);

                channels[0] = impulse.data() + offset;
                engine.processBlock(channels, 1, count);
                offset += count;
            }

            for (int i = 0; i < static_cast<int>(impulse.size()); ++i)
            {
                const float expected = i == delaySamples ? 1.0f : 0.0f;

                if (!near(
                    impulse[static_cast<std::size_t>(i)],
                    expected,
                    0.001f))
                {
                    return false;
                }
            }
        }

        return true;
    }

    bool testSmootherConvergesAcrossBlocksWithRepeatedTarget()
    {
        NebulaParameterSmoothing smoother;

        smoother.prepare(1000.0, 0.01);
        smoother.setCurrentAndTarget(0.0f);
        smoother.setTarget(1.0f);

        // 10 ms at 1000 Hz requires 10 samples.
        // Five blocks of two samples process all 10 samples.
        for (int block = 0; block < 5; ++block)
        {
            smoother.setTarget(1.0f);

            for (int sample = 0; sample < 2; ++sample)
                smoother.getNextValue();
        }

        return !smoother.isSmoothing()
            && near(smoother.getCurrentValue(), 1.0f);
    }

    bool testProductionEffectDryBypass()
    {
        NebulaSaturationEffect saturation;
        saturation.prepare(44100.0, 1);

        NebulaEffectParameters parameters;
        parameters.wet = 0.0f;
        parameters.drive = 10.0f;

        const float dry = saturation.processSample(
            0.25f, 0, parameters);

        parameters.wet = 1.0f;

        const float wet = saturation.processSample(
            0.25f, 0, parameters);

        const float expectedWet = std::tanh(2.5f);

        return near(dry, 0.25f)
            && std::isfinite(wet)
            && near(wet, expectedWet, 0.0001f)
            && wet > dry;
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
        { "engine initialization/release",
          testEngineInitialization },

        { "clean mode and variable blocks",
          testCleanAndVariableBlocks },

        { "mono/stereo and non-finite sanitizing",
          testMonoStereoAndNonFiniteSanitizing },

        { "dry/wet and parameter update",
          testDryWetAndParameterUpdate },

        { "delay at multiple sample rates and variable blocks",
          testDelayAtRatesAndVariableBlocks },

        { "smoothing convergence across repeated block targets",
          testSmootherConvergesAcrossBlocksWithRepeatedTarget },

        { "production effect dry/wet bypass",
          testProductionEffectDryBypass }
    };

    int failures = 0;

    for (const auto& test : tests)
    {
        const bool passed = test.run();

        std::cout
            << (passed ? "[PASS] " : "[FAIL] ")
            << test.name
            << '\n';

        if (!passed)
            ++failures;
    }

    std::cout << "\nFailed tests: " << failures << '\n';

    return failures == 0
        ? EXIT_SUCCESS
        : EXIT_FAILURE;
}
