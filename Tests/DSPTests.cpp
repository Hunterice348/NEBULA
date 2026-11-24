#include "DSP/AudioEngine.h"

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
        const bool ok = engine.isPrepared() && engine.getNumChannels() == 2
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
        p.inputGain = p.outputGain = 1.0f;
        p.wet = 0.2f;
        engine.setParameters(p);
        std::vector<float> signal(129, 0.25f);
        float* channels[] = { signal.data() };
        engine.processBlock(channels, 1, 3);
        engine.processBlock(channels, 1, 126);
        for (const auto sample : signal)
            if (!near(sample, 0.25f)) return false;
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
        float mono[] = { 0.0f, std::numeric_limits<float>::quiet_NaN(), 0.5f };
        float left[] = { 0.25f, std::numeric_limits<float>::infinity(), -0.5f };
        float right[] = { -0.25f, 0.0f, 0.5f };
        float* monoChannels[] = { mono };
        engine.processBlock(monoChannels, 1, 3);
        float* stereoChannels[] = { left, right };
        engine.processBlock(stereoChannels, 2, 3);
        for (float value : mono) if (!std::isfinite(value)) return false;
        for (float value : left) if (!std::isfinite(value)) return false;
        for (float value : right) if (!std::isfinite(value)) return false;
        return near(mono[0], 0.0f) && near(left[1], 0.0f);
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
        float dry[] = { 0.5f };
        float* channels[] = { dry };
        engine.processBlock(channels, 1, 1);
        if (!near(dry[0], 0.5f)) return false;

        p.wet = 1.0f;
        engine.setParameters(p);
        float wet[64] {};
        wet[0] = 0.5f;
        channels[0] = wet;
        engine.processBlock(channels, 1, 64);
        return std::isfinite(wet[63]) && wet[63] < 0.5f
            && wet[63] > 0.0f;
    }

    bool testDelayAndReset()
    {
        NebulaAudioEngine engine;
        engine.prepare(1000.0, 1, 16);
        NebulaAudioParameters p;
        p.mode = 1;
        p.delaySamples = 2.0f;
        p.feedback = 0.0f;
        p.wet = 1.0f;
        engine.setParameters(p);
        float signal[] = { 1.0f, 0.0f, 0.0f, 0.0f };
        float* channels[] = { signal };
        engine.processBlock(channels, 1, 4);
        // Wet is smoothed from the default, so repeat after settling it.
        engine.reset();
        engine.setParameters(p);
        float settle[64] {};
        channels[0] = settle;
        engine.processBlock(channels, 1, 64);
        engine.reset();
        float impulse[] = { 1.0f, 0.0f, 0.0f, 0.0f };
        channels[0] = impulse;
        engine.processBlock(channels, 1, 4);
        return near(impulse[0], 0.0f) && near(impulse[1], 0.0f)
            && impulse[2] > 0.9f && near(impulse[3], 0.0f);
    }

    bool testSmootherFiniteAndConverges()
    {
        NebulaParameterSmoothing smoother;
        smoother.prepare(1000.0, 0.01);
        smoother.setCurrentAndTarget(0.0f);
        smoother.setTarget(1.0f);
        for (int i = 0; i < 10; ++i) smoother.getNextValue();
        return near(smoother.getCurrentValue(), 1.0f);
    }
}

int main()
{
    struct Test { const char* name; bool (*run)(); };
    const Test tests[] = {
        { "engine initialization/release", testEngineInitialization },
        { "clean mode and variable blocks", testCleanAndVariableBlocks },
        { "mono/stereo and non-finite sanitizing", testMonoStereoAndNonFiniteSanitizing },
        { "dry/wet and parameter update", testDryWetAndParameterUpdate },
        { "delay and reset lifecycle", testDelayAndReset },
        { "parameter smoothing", testSmootherFiniteAndConverges }
    };
    int failures = 0;
    for (const auto& test : tests)
    {
        const bool passed = test.run();
        std::cout << (passed ? "[PASS] " : "[FAIL] ") << test.name << '\n';
        if (!passed) ++failures;
    }
    std::cout << "\nFailed tests: " << failures << '\n';
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
