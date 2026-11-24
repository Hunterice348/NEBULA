#pragma once

#include "Effects.h"

#include <array>
#include <atomic>
#include <vector>
#include <cmath>
#include <algorithm>

class GranularDelayEffect final : public EffectBase
{
public:
    static constexpr int grainCount = 16;
    static constexpr int parameterCount = 6;

    GranularDelayEffect()
    {
        parameters[0].store(0.45f);
        parameters[1].store(0.35f);
        parameters[2].store(0.40f);
        parameters[3].store(0.20f);
        parameters[4].store(0.35f);
        parameters[5].store(0.50f);
    }

    void prepare(double sampleRate, int blockSize) override
    {
        prepareBase(sampleRate, blockSize, 2);

        const int capacity = juce::jmax(
            4096,
            static_cast<int>(sr * 2.5));

        for (auto& channel : delayBuffer)
            channel.assign(static_cast<size_t>(capacity), 0.0f);

        bufferSize = capacity;
        writePosition = 0;
        spawnPhase = 0.0;
        reset();
    }

    void reset() override
    {
        for (auto& channel : delayBuffer)
            std::fill(channel.begin(), channel.end(), 0.0f);

        for (auto& grain : grains)
            grain = Grain {};

        writePosition = 0;
        spawnPhase = 0.0;
        randomState = 0x12345678u;
        lastOutput = {};
    }

    juce::String getTypeID() const override
    {
        return "granular_delay";
    }

    juce::String getDisplayName() const override
    {
        return "Granular Delay";
    }

    int getNumParams() const override
    {
        return parameterCount;
    }

    juce::String getParamName(int index) const override
    {
        switch (index)
        {
            case 0: return "Time";
            case 1: return "Grain Size";
            case 2: return "Density";
            case 3: return "Pitch Scatter";
            case 4: return "Feedback";
            case 5: return "Stereo Spread";
            default: return {};
        }
    }

    float getParam(int index) const override
    {
        if (index < 0 || index >= parameterCount)
            return 0.0f;

        return parameters[static_cast<size_t>(index)].load(
            std::memory_order_relaxed);
    }

    void setParam(int index, float value) override
    {
        if (index < 0 || index >= parameterCount)
            return;

        value = nebula::dsp::clampFinite(
            value, 0.0f, 1.0f, 0.5f);

        parameters[static_cast<size_t>(index)].store(
            value,
            std::memory_order_relaxed);
    }

protected:
    void processWet(juce::AudioBuffer<float>& buffer) override
    {
        if (bufferSize < 2 || buffer.getNumChannels() <= 0)
            return;

        const int channels = juce::jmin(
            2, buffer.getNumChannels());

        const float timeSeconds =
            0.05f + getParam(0) * 1.45f;

        const float grainMs =
            20.0f + getParam(1) * 180.0f;

        const float density =
            2.0f + getParam(2) * 28.0f;

        const float pitchScatter =
            getParam(3) * 12.0f;

        const float feedback =
            getParam(4) * 0.85f;

        const float stereoSpread =
            getParam(5);

        const int grainLength = juce::jlimit(
            64,
            bufferSize / 2,
            static_cast<int>(sr * grainMs * 0.001f));

        const int baseDelay = juce::jlimit(
            1,
            bufferSize - grainLength - 2,
            static_cast<int>(sr * timeSeconds));

        const double spawnInterval =
            sr / static_cast<double>(density);

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            float input[2] = {};

            for (int ch = 0; ch < channels; ++ch)
            {
                input[ch] = nebula::dsp::sanitizeSample(
                    buffer.getSample(ch, i));
            }

            for (int ch = 0; ch < channels; ++ch)
            {
                const float feedbackInput =
                    input[ch] + lastOutput[ch] * feedback;

                delayBuffer[static_cast<size_t>(ch)]
                           [static_cast<size_t>(writePosition)] =
                    nebula::dsp::sanitizeSample(feedbackInput);
            }

            spawnPhase += 1.0;

            if (spawnPhase >= spawnInterval)
            {
                spawnPhase -= spawnInterval;
                spawnGrain(baseDelay, grainLength, pitchScatter);
            }

            float output[2] = {};
            float windowSum[2] = {};

            for (auto& grain : grains)
            {
                if (!grain.active)
                    continue;

                const float phase =
                    static_cast<float>(grain.age)
                    / static_cast<float>(grain.length);

                const float window =
                    0.5f - 0.5f * std::cos(
                        juce::MathConstants<float>::twoPi * phase);

                for (int ch = 0; ch < channels; ++ch)
                {
                    const float channelOffset =
                        ch == 0
                            ? -stereoSpread * grain.stereoOffset
                            : stereoSpread * grain.stereoOffset;

                    const float position =
                        grain.readPosition
                        + channelOffset * static_cast<float>(sr) * 0.01f;

                    const float sample =
                        readInterpolated(ch, position);

                    output[ch] += sample * window;
                    windowSum[ch] += window;
                }

                grain.readPosition += grain.rate;
                ++grain.age;

                if (grain.age >= grain.length)
                    grain.active = false;
            }

            for (int ch = 0; ch < channels; ++ch)
            {
                if (windowSum[ch] > 0.0001f)
                    output[ch] /= windowSum[ch];

                output[ch] = nebula::dsp::sanitizeSample(
                    output[ch]);

                lastOutput[ch] = output[ch];
                buffer.setSample(ch, i, output[ch]);
            }

            if (channels == 1)
                lastOutput[1] = lastOutput[0];

            if (++writePosition >= bufferSize)
                writePosition = 0;
        }
    }

private:
    struct Grain
    {
        bool active = false;
        int age = 0;
        int length = 0;
        float readPosition = 0.0f;
        float rate = 1.0f;
        float stereoOffset = 0.0f;
    };

    uint32_t randomState = 0x12345678u;

    float randomBipolar() noexcept
    {
        randomState ^= randomState << 13;
        randomState ^= randomState >> 17;
        randomState ^= randomState << 5;

        const float normalized =
            static_cast<float>(randomState & 0x00FFFFFFu)
            / static_cast<float>(0x00800000u);

        return normalized - 1.0f;
    }

    void spawnGrain(
        int baseDelay,
        int grainLength,
        float pitchScatter)
    {
        Grain* target = nullptr;

        for (auto& grain : grains)
        {
            if (!grain.active)
            {
                target = &grain;
                break;
            }
        }

        if (target == nullptr)
            return;

        const float pitch =
            randomBipolar() * pitchScatter;

        const float rate = std::pow(
            2.0f, pitch / 12.0f);

        const int jitter =
            static_cast<int>(
                randomBipolar() * grainLength * 0.5f);

        int readPosition =
            writePosition - baseDelay - jitter;

        while (readPosition < 0)
            readPosition += bufferSize;

        while (readPosition >= bufferSize)
            readPosition -= bufferSize;

        target->active = true;
        target->age = 0;
        target->length = grainLength;
        target->readPosition =
            static_cast<float>(readPosition);
        target->rate = rate;
        target->stereoOffset = randomBipolar();
    }

    float readInterpolated(
        int channel,
        float position) const noexcept
    {
        if (bufferSize <= 0)
            return 0.0f;

        while (position < 0.0f)
            position += static_cast<float>(bufferSize);

        while (position >= static_cast<float>(bufferSize))
            position -= static_cast<float>(bufferSize);

        const int indexA = static_cast<int>(position);
        const int indexB = (indexA + 1) % bufferSize;

        const float fraction =
            position - static_cast<float>(indexA);

        const auto& data =
            delayBuffer[static_cast<size_t>(channel)];

        const float a = data[static_cast<size_t>(indexA)];
        const float b = data[static_cast<size_t>(indexB)];

        return a + (b - a) * fraction;
    }

    std::array<std::atomic<float>, parameterCount> parameters {};

    std::array<std::vector<float>, 2> delayBuffer;
    std::array<Grain, grainCount> grains {};

    std::array<float, 2> lastOutput {};

    int bufferSize = 0;
    int writePosition = 0;

    double spawnPhase = 0.0;
};