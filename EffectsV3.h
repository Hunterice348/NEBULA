#pragma once

#include "Effects.h"

#include <array>
#include <atomic>
#include <vector>
#include <cmath>

class MultibandCompressorEffect final : public EffectBase
{
public:
    static constexpr int bandCount = 3;
    static constexpr int paramsPerBand = 4;
    static constexpr int parameterCount = bandCount * paramsPerBand;

    MultibandCompressorEffect()
    {
        for (int i = 0; i < parameterCount; ++i)
            parameters[static_cast<size_t>(i)].store(
                defaultParameter(i),
                std::memory_order_relaxed);
    }

    void prepare(double sampleRate, int blockSize) override
    {
        prepareBase(sampleRate, blockSize, 2);
        reset();
    }

    void reset() override
    {
        lowState = {};
        midState = {};
        envelope = {};
        gainReduction = {};
    }

    juce::String getTypeID() const override
    {
        return "multiband_compressor";
    }

    juce::String getDisplayName() const override
    {
        return "Multiband Compressor";
    }

    int getNumParams() const override
    {
        return parameterCount;
    }

    juce::String getParamName(int index) const override
    {
        if (index < 0 || index >= parameterCount)
            return {};

        static const char* names[4] = {
            "Threshold", "Ratio", "Attack", "Release"
        };

        const int band = index / paramsPerBand;
        const int parameter = index % paramsPerBand;

        const juce::String bandName =
            band == 0 ? "Low " :
            band == 1 ? "Mid " : "High ";

        return bandName + names[parameter];
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
        const int channels = juce::jmin(
            2, buffer.getNumChannels());

        if (channels <= 0)
            return;

        constexpr float lowCut = 250.0f;
        constexpr float highCut = 2500.0f;

        const float lowCoefficient = std::exp(
            -2.0f * juce::MathConstants<float>::pi
            * lowCut / static_cast<float>(sr));

        const float highCoefficient = std::exp(
            -2.0f * juce::MathConstants<float>::pi
            * highCut / static_cast<float>(sr));

        float threshold[3];
        float ratio[3];
        float attackCoefficient[3];
        float releaseCoefficient[3];

        for (int band = 0; band < 3; ++band)
        {
            const int base = band * 4;

            const float t = getParam(base);
            const float r = getParam(base + 1);
            const float a = getParam(base + 2);
            const float rel = getParam(base + 3);

            threshold[band] = -48.0f + t * 48.0f;
            ratio[band] = 1.0f + r * 11.0f;

            const float attackMs = 1.0f + a * 99.0f;
            const float releaseMs = 20.0f + rel * 480.0f;

            attackCoefficient[band] =
                nebula::dsp::onePoleCoefficient(sr, attackMs);

            releaseCoefficient[band] =
                nebula::dsp::onePoleCoefficient(sr, releaseMs);
        }

        for (int ch = 0; ch < channels; ++ch)
        {
            auto* data = buffer.getWritePointer(ch);
            const size_t channel = static_cast<size_t>(ch);

            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                const float input =
                    nebula::dsp::sanitizeSample(data[i]);

                lowState[channel] =
                    (1.0f - lowCoefficient) * input
                    + lowCoefficient * lowState[channel];

                const float low = lowState[channel];

                midState[channel] =
                    (1.0f - highCoefficient) * input
                    + highCoefficient * midState[channel];

                const float lowMid = midState[channel];
                const float mid = lowMid - low;
                const float high = input - lowMid;

                const float bands[3] = { low, mid, high };

                float output = 0.0f;

                for (int band = 0; band < 3; ++band)
                {
                    const size_t b =
                        static_cast<size_t>(band);

                    const float magnitude =
                        std::abs(bands[band]);

                    const float coefficient =
                        magnitude > envelope[b][channel]
                            ? attackCoefficient[band]
                            : releaseCoefficient[band];

                    envelope[b][channel] =
                        magnitude
                        + coefficient
                        * (envelope[b][channel] - magnitude);

                    const float levelDb =
                        nebula::dsp::gainToDecibels(
                            envelope[b][channel]);

                    float reductionDb = 0.0f;

                    if (levelDb > threshold[band])
                    {
                        const float compressed =
                            threshold[band]
                            + (levelDb - threshold[band])
                            / ratio[band];

                        reductionDb = compressed - levelDb;
                    }

                    const float targetGain =
                        nebula::dsp::decibelsToGain(
                            reductionDb);

                    gainReduction[b][channel] =
                        targetGain;

                    output += bands[band] * targetGain;
                }

                data[i] =
                    nebula::dsp::sanitizeSample(output);
            }
        }
    }

private:
    static float defaultParameter(int index)
    {
        switch (index % 4)
        {
            case 0: return 0.70f;
            case 1: return 0.25f;
            case 2: return 0.15f;
            case 3: return 0.35f;
            default: return 0.5f;
        }
    }

    std::array<std::atomic<float>, parameterCount> parameters {};

    std::array<float, 2> lowState {};
    std::array<float, 2> midState {};

    std::array<std::array<float, 2>, 3> envelope {};
    std::array<std::array<float, 2>, 3> gainReduction {};
};