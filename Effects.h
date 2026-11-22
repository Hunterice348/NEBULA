 
#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <vector>
#include <cmath>
#include <algorithm>

class EffectBase
{
public:
    virtual ~EffectBase() = default;

    virtual void prepare(double sampleRate, int maxBlockSize) = 0;
    virtual void reset() = 0;

    void process(juce::AudioBuffer<float>& buffer)
    {
        const int numSamples = buffer.getNumSamples();
        const int numChannels = buffer.getNumChannels();

        if (!enabled.load(std::memory_order_relaxed)
            || mixAmount.load(std::memory_order_relaxed) < 0.001f)
            return;

        if (numSamples > dryBuffer.getNumSamples()
            || numChannels > dryBuffer.getNumChannels())
            return;

        for (int ch = 0; ch < numChannels; ++ch)
            dryBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);

        processWet(buffer);

        const float wetAmount = juce::jlimit(
            0.0f, 1.0f,
            mixAmount.load(std::memory_order_relaxed));

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* wet = buffer.getWritePointer(ch);
            const auto* dry = dryBuffer.getReadPointer(ch);

            for (int i = 0; i < numSamples; ++i)
            {
                float sample = std::isfinite(wet[i]) ? wet[i] : 0.0f;

                if (std::abs(sample) < 1.0e-30f)
                    sample = 0.0f;

                wet[i] = dry[i] * (1.0f - wetAmount)
                       + sample * wetAmount;
            }
        }
    }

    virtual juce::String getTypeID() const = 0;
    virtual juce::String getDisplayName() const = 0;
    virtual int getNumParams() const = 0;
    virtual juce::String getParamName(int index) const = 0;
    virtual float getParam(int index) const = 0;
    virtual void setParam(int index, float value) = 0;

    bool isEnabled() const
    {
        return enabled.load(std::memory_order_relaxed);
    }

    void setEnabled(bool value)
    {
        enabled.store(value, std::memory_order_relaxed);
    }

    float getMix() const
    {
        return mixAmount.load(std::memory_order_relaxed);
    }

    void setMix(float value)
    {
        if (!std::isfinite(value))
            value = 1.0f;

        mixAmount.store(
            juce::jlimit(0.0f, 1.0f, value),
            std::memory_order_relaxed);
    }

    virtual juce::ValueTree saveState() const
    {
        juce::ValueTree tree(getTypeID());

        tree.setProperty("on", isEnabled(), nullptr);
        tree.setProperty("mix", getMix(), nullptr);

        for (int i = 0; i < getNumParams(); ++i)
            tree.setProperty("p" + juce::String(i), getParam(i), nullptr);

        return tree;
    }

    virtual void loadState(const juce::ValueTree& tree)
    {
        if (!tree.hasType(getTypeID()))
            return;

        setEnabled((bool)tree.getProperty("on", true));
        setMix((float)tree.getProperty("mix", 1.0f));

        for (int i = 0; i < getNumParams(); ++i)
        {
            const juce::Identifier property("p" + juce::String(i));

            if (tree.hasProperty(property))
                setParam(i, (float)tree.getProperty(property));
        }
    }

protected:
    virtual void processWet(juce::AudioBuffer<float>& buffer) = 0;

    void prepareBase(double sampleRate, int maxBlockSize, int maxChannels = 2)
    {
        sr = sampleRate;
        bs = juce::jmax(1, maxBlockSize);

        dryBuffer.setSize(
            juce::jmax(1, maxChannels),
            bs,
            false,
            true,
            true);
    }

    std::atomic<bool> enabled { true };
    std::atomic<float> mixAmount { 1.0f };

    double sr = 44100.0;
    int bs = 512;

    juce::AudioBuffer<float> dryBuffer;
};

// ============================================================
// REVERB
// ============================================================

class ReverbEffect : public EffectBase
{
public:
    void prepare(double sampleRate, int blockSize) override;
    void reset() override;
    void processWet(juce::AudioBuffer<float>& buffer) override;

    juce::String getTypeID() const override { return "reverb"; }
    juce::String getDisplayName() const override { return "Reverb"; }

    int getNumParams() const override { return 3; }

    juce::String getParamName(int i) const override
    {
        switch (i)
        {
            case 0: return "Size";
            case 1: return "Decay";
            case 2: return "Damp";
            default: return {};
        }
    }

    float getParam(int i) const override
    {
        switch (i)
        {
            case 0: return size.load(std::memory_order_relaxed);
            case 1: return decay.load(std::memory_order_relaxed);
            case 2: return damp.load(std::memory_order_relaxed);
            default: return 0.0f;
        }
    }

    void setParam(int i, float value) override;

private:
    juce::dsp::Reverb reverb;

    std::atomic<float> size { 0.5f };
    std::atomic<float> decay { 0.5f };
    std::atomic<float> damp { 0.5f };

    void updateParams();
};

// ============================================================
// DELAY
// ============================================================

class DelayEffect : public EffectBase
{
public:
    void prepare(double sampleRate, int blockSize) override;
    void reset() override;
    void processWet(juce::AudioBuffer<float>& buffer) override;

    juce::String getTypeID() const override { return "delay"; }
    juce::String getDisplayName() const override { return "Delay"; }

    int getNumParams() const override { return 2; }

    juce::String getParamName(int i) const override
    {
        switch (i)
        {
            case 0: return "Time";
            case 1: return "Fdbk";
            default: return {};
        }
    }

    float getParam(int i) const override
    {
        switch (i)
        {
            case 0: return time.load(std::memory_order_relaxed);
            case 1: return feedback.load(std::memory_order_relaxed);
            default: return 0.0f;
        }
    }

    void setParam(int i, float value) override
    {
        if (!std::isfinite(value))
            return;

        value = juce::jlimit(0.0f, 1.0f, value);

        if (i == 0)
            time.store(value, std::memory_order_relaxed);
        else if (i == 1)
            feedback.store(value, std::memory_order_relaxed);
    }

private:
    std::vector<float> bufL, bufR;

    int writePos = 0;
    int maxSamples = 0;

    std::atomic<float> time { 0.3f };
    std::atomic<float> feedback { 0.4f };
};

// ============================================================
// DISTORTION
// ============================================================

class DistortionEffect : public EffectBase
{
public:
    void prepare(double sampleRate, int blockSize) override;
    void reset() override;
    void processWet(juce::AudioBuffer<float>& buffer) override;

    juce::String getTypeID() const override { return "distortion"; }
    juce::String getDisplayName() const override { return "Distortion"; }

    int getNumParams() const override { return 2; }

    juce::String getParamName(int i) const override
    {
        switch (i)
        {
            case 0: return "Drive";
            case 1: return "Tone";
            default: return {};
        }
    }

    float getParam(int i) const override
    {
        if (i == 0)
            return drive.load(std::memory_order_relaxed);

        if (i == 1)
            return tone.load(std::memory_order_relaxed);

        return 0.0f;
    }

    void setParam(int i, float value) override
    {
        if (!std::isfinite(value))
            return;

        value = juce::jlimit(0.0f, 1.0f, value);

        if (i == 0)
            drive.store(value, std::memory_order_relaxed);
        else if (i == 1)
            tone.store(value, std::memory_order_relaxed);
    }

private:
    std::atomic<float> drive { 0.5f };
    std::atomic<float> tone { 0.7f };

    std::array<float, 2> toneState {};
};

// ============================================================
// CHORUS
// ============================================================

class ChorusEffect : public EffectBase
{
public:
    void prepare(double sampleRate, int blockSize) override;
    void reset() override;
    void processWet(juce::AudioBuffer<float>& buffer) override;

    juce::String getTypeID() const override { return "chorus"; }
    juce::String getDisplayName() const override { return "Chorus"; }

    int getNumParams() const override { return 2; }

    juce::String getParamName(int i) const override
    {
        switch (i)
        {
            case 0: return "Rate";
            case 1: return "Depth";
            default: return {};
        }
    }

    float getParam(int i) const override
    {
        if (i == 0)
            return rate.load(std::memory_order_relaxed);

        if (i == 1)
            return depth.load(std::memory_order_relaxed);

        return 0.0f;
    }

    void setParam(int i, float value) override
    {
        if (!std::isfinite(value))
            return;

        value = juce::jlimit(0.0f, 1.0f, value);

        if (i == 0)
            rate.store(value, std::memory_order_relaxed);
        else if (i == 1)
            depth.store(value, std::memory_order_relaxed);
    }

private:
    juce::dsp::Chorus<float> chorus;

    std::atomic<float> rate { 0.3f };
    std::atomic<float> depth { 0.5f };
};

// ============================================================
// PHASER
// ============================================================

class PhaserEffect : public EffectBase
{
public:
    void prepare(double sampleRate, int blockSize) override;
    void reset() override;
    void processWet(juce::AudioBuffer<float>& buffer) override;

    juce::String getTypeID() const override { return "phaser"; }
    juce::String getDisplayName() const override { return "Phaser"; }

    int getNumParams() const override { return 3; }

    juce::String getParamName(int i) const override
    {
        switch (i)
        {
            case 0: return "Rate";
            case 1: return "Depth";
            case 2: return "Fdbk";
            default: return {};
        }
    }

    float getParam(int i) const override
    {
        switch (i)
        {
            case 0: return rate.load(std::memory_order_relaxed);
            case 1: return depth.load(std::memory_order_relaxed);
            case 2: return feedback.load(std::memory_order_relaxed);
            default: return 0.0f;
        }
    }

    void setParam(int i, float value) override
    {
        if (!std::isfinite(value))
            return;

        value = juce::jlimit(0.0f, 1.0f, value);

        switch (i)
        {
            case 0: rate.store(value, std::memory_order_relaxed); break;
            case 1: depth.store(value, std::memory_order_relaxed); break;
            case 2: feedback.store(value, std::memory_order_relaxed); break;
            default: break;
        }
    }

private:
    juce::dsp::Phaser<float> phaser;

    std::atomic<float> rate { 0.3f };
    std::atomic<float> depth { 0.5f };
    std::atomic<float> feedback { 0.4f };
};

// ============================================================
// COMPRESSOR
// ============================================================

class CompressorEffect : public EffectBase
{
public:
    void prepare(double sampleRate, int blockSize) override;
    void reset() override;
    void processWet(juce::AudioBuffer<float>& buffer) override;

    juce::String getTypeID() const override { return "compressor"; }
    juce::String getDisplayName() const override { return "Compressor"; }

    int getNumParams() const override { return 3; }

    juce::String getParamName(int i) const override
    {
        switch (i)
        {
            case 0: return "Thresh";
            case 1: return "Ratio";
            case 2: return "Attack";
            default: return {};
        }
    }

    float getParam(int i) const override
    {
        switch (i)
        {
            case 0: return threshold.load(std::memory_order_relaxed);
            case 1: return ratio.load(std::memory_order_relaxed);
            case 2: return attack.load(std::memory_order_relaxed);
            default: return 0.0f;
        }
    }

    void setParam(int i, float value) override
    {
        if (!std::isfinite(value))
            return;

        value = juce::jlimit(0.0f, 1.0f, value);

        switch (i)
        {
            case 0: threshold.store(value, std::memory_order_relaxed); break;
            case 1: ratio.store(value, std::memory_order_relaxed); break;
            case 2: attack.store(value, std::memory_order_relaxed); break;
            default: break;
        }
    }

private:
    juce::dsp::Compressor<float> comp;

    std::atomic<float> threshold { 0.5f };
    std::atomic<float> ratio { 0.5f };
    std::atomic<float> attack { 0.5f };
};

// ============================================================
// BITCRUSHER
// ============================================================

class BitcrusherEffect : public EffectBase
{
public:
    void prepare(double sampleRate, int blockSize) override
    {
        prepareBase(sampleRate, blockSize, 2);
        reset();
    }

    void reset() override
    {
        held = {};
        phase = {};
    }

    void processWet(juce::AudioBuffer<float>& buffer) override;

    juce::String getTypeID() const override { return "bitcrusher"; }
    juce::String getDisplayName() const override { return "Bitcrusher"; }

    int getNumParams() const override { return 2; }

    juce::String getParamName(int i) const override
    {
        switch (i)
        {
            case 0: return "Bits";
            case 1: return "Rate";
            default: return {};
        }
    }

    float getParam(int i) const override
    {
        if (i == 0)
            return bitDepth.load(std::memory_order_relaxed);

        if (i == 1)
            return downsample.load(std::memory_order_relaxed);

        return 0.0f;
    }

    void setParam(int i, float value) override
    {
        if (!std::isfinite(value))
            return;

        value = juce::jlimit(0.0f, 1.0f, value);

        if (i == 0)
            bitDepth.store(value, std::memory_order_relaxed);
        else if (i == 1)
            downsample.store(value, std::memory_order_relaxed);
    }

private:
    std::atomic<float> bitDepth { 0.5f };
    std::atomic<float> downsample { 0.5f };

    std::array<float, 2> held {};
    std::array<int, 2> phase {};
};

// ============================================================
// FILTER
// ============================================================

class FilterEffect : public EffectBase
{
public:
    void prepare(double sampleRate, int blockSize) override;
    void reset() override;
    void processWet(juce::AudioBuffer<float>& buffer) override;

    juce::String getTypeID() const override { return "filter"; }
    juce::String getDisplayName() const override { return "Filter"; }

    int getNumParams() const override { return 2; }

    juce::String getParamName(int i) const override
    {
        switch (i)
        {
            case 0: return "Cutoff";
            case 1: return "Reso";
            default: return {};
        }
    }

    float getParam(int i) const override
    {
        if (i == 0)
            return cutoff.load(std::memory_order_relaxed);

        if (i == 1)
            return resonance.load(std::memory_order_relaxed);

        return 0.0f;
    }

    void setParam(int i, float value) override
    {
        if (!std::isfinite(value))
            return;

        value = juce::jlimit(0.0f, 1.0f, value);

        if (i == 0)
            cutoff.store(value, std::memory_order_relaxed);
        else if (i == 1)
            resonance.store(value, std::memory_order_relaxed);
    }

private:
    juce::dsp::StateVariableTPTFilter<float> filter;

    std::atomic<float> cutoff { 0.5f };
    std::atomic<float> resonance { 0.3f };
};
