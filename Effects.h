#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <vector>

// Common effect contract. Preparation owns all buffer allocation; process() only
// uses the prepared storage and safely bypasses oversized/unprepared blocks.
class EffectBase
{
public:
    virtual ~EffectBase() = default;

    virtual void prepare(double sampleRate, int maxBlockSize) = 0;
    virtual void reset() = 0;

    void process(juce::AudioBuffer<float>& buffer)
    {
        const int ns = buffer.getNumSamples();
        const int nc = buffer.getNumChannels();
        if (!enabled.load(std::memory_order_relaxed) || mixAmount.load(std::memory_order_relaxed) < 0.001f)
            return;

        // Never grow storage on the audio thread. Oversized blocks pass through
        // this effect until the host re-prepares it with a suitable capacity.
        if (ns > dryBuffer.getNumSamples() || nc > dryBuffer.getNumChannels())
            return;

        for (int ch = 0; ch < nc; ++ch)
            dryBuffer.copyFrom(ch, 0, buffer, ch, 0, ns);

        processWet(buffer);
        const float wetAmount = juce::jlimit(0.0f, 1.0f, mixAmount.load(std::memory_order_relaxed));
        for (int ch = 0; ch < nc; ++ch)
        {
            auto* wet = buffer.getWritePointer(ch);
            const auto* dry = dryBuffer.getReadPointer(ch);
            for (int i = 0; i < ns; ++i)
            {
                float sample = std::isfinite(wet[i]) ? wet[i] : 0.0f;
                if (std::abs(sample) < 1.0e-30f) sample = 0.0f;
                wet[i] = dry[i] * (1.0f - wetAmount) + sample * wetAmount;
            }
        }
    }

    virtual juce::String getTypeID() const = 0;
    virtual juce::String getDisplayName() const = 0;
    virtual int getNumParams() const = 0;
    virtual juce::String getParamName(int i) const = 0;
    virtual float getParam(int i) const = 0;
    virtual void setParam(int i, float v) = 0;

    bool isEnabled() const { return enabled.load(std::memory_order_relaxed); }
    void setEnabled(bool e) { enabled.store(e, std::memory_order_relaxed); }
    float getMix() const { return mixAmount.load(std::memory_order_relaxed); }
    void setMix(float m) { mixAmount.store(juce::jlimit(0.0f, 1.0f, std::isfinite(m) ? m : 1.0f), std::memory_order_relaxed); }

    virtual juce::ValueTree saveState() const
    {
        juce::ValueTree t(getTypeID());
        t.setProperty("on", isEnabled(), nullptr);
        t.setProperty("mix", getMix(), nullptr);
        for (int i = 0; i < getNumParams(); ++i)
            t.setProperty("p" + juce::String(i), getParam(i), nullptr);
        return t;
    }
    virtual void loadState(const juce::ValueTree& t)
    {
        if (!t.hasType(getTypeID())) return;
        setEnabled((bool)t.getProperty("on", true));
        setMix((float)t.getProperty("mix", 1.0f));
        for (int i = 0; i < getNumParams(); ++i)
            if (t.hasProperty("p" + juce::String(i)))
                setParam(i, (float)t.getProperty("p" + juce::String(i)));
    }

protected:
    virtual void processWet(juce::AudioBuffer<float>&) = 0;
    void prepareBase(double sampleRate, int maxBlockSize, int maxChannels = 2)
    {
        sr = sampleRate;
        bs = juce::jmax(1, maxBlockSize);
        dryBuffer.setSize(juce::jmax(1, maxChannels), bs, false, true, true);
    }

    std::atomic<bool> enabled { true };
    std::atomic<float> mixAmount { 1.0f };
    double sr = 44100.0;
    int bs = 512;
    juce::AudioBuffer<float> dryBuffer;
};

class ReverbEffect : public EffectBase
{
public:
    void prepare(double s, int b) override;
    void reset() override;
    void processWet(juce::AudioBuffer<float>& buffer) override;
    juce::String getTypeID() const override { return "reverb"; }
    juce::String getDisplayName() const override { return "Reverb"; }
    int getNumParams() const override { return 3; }
    juce::String getParamName(int i) const override { switch (i) { case 0: return "Size"; case 1: return "Decay"; default: return "Damp"; } }
    float getParam(int i) const override { switch (i) { case 0: return size; case 1: return decay; default: return damp; } }
    void setParam(int i, float v) override;
private:
    juce::dsp::Reverb reverb;
    float size = 0.5f, decay = 0.5f, damp = 0.5f;
    void updateParams();
};

class DelayEffect : public EffectBase
{
public:
    void prepare(double s, int b) override;
    void reset() override;
    void processWet(juce::AudioBuffer<float>& buffer) override;
    juce::String getTypeID() const override { return "delay"; }
    juce::String getDisplayName() const override { return "Delay"; }
    int getNumParams() const override { return 2; }
    juce::String getParamName(int i) const override { return i == 0 ? "Time" : "Fdbk"; }
    float getParam(int i) const override { return i == 0 ? time : feedback; }
    void setParam(int i, float v) override { v = juce::jlimit(0.0f, 1.0f, std::isfinite(v) ? v : 0.0f); if (i == 0) time = v; else feedback = v; }
private:
    std::vector<float> bufL, bufR;
    int writePos = 0, maxSamples = 0;
    float time = 0.3f, feedback = 0.4f;
};

class DistortionEffect : public EffectBase
{
public:
    void prepare(double s, int b) override;
    void reset() override;
    void processWet(juce::AudioBuffer<float>& buffer) override;
    juce::String getTypeID() const override { return "distortion"; }
    juce::String getDisplayName() const override { return "Distortion"; }
    int getNumParams() const override { return 2; }
    juce::String getParamName(int i) const override { return i == 0 ? "Drive" : "Tone"; }
    float getParam(int i) const override { return i == 0 ? drive : tone; }
    void setParam(int i, float v) override;
private:
    juce::dsp::IIR::Filter<float> toneL, toneR;
    float drive = 0.5f, tone = 0.7f;
    void updateTone();
};

class ChorusEffect : public EffectBase
{
public:
    void prepare(double s, int b) override;
    void reset() override;
    void processWet(juce::AudioBuffer<float>& buffer) override;
    juce::String getTypeID() const override { return "chorus"; }
    juce::String getDisplayName() const override { return "Chorus"; }
    int getNumParams() const override { return 2; }
    juce::String getParamName(int i) const override { return i == 0 ? "Rate" : "Depth"; }
    float getParam(int i) const override { return i == 0 ? rate : depth; }
    void setParam(int i, float v) override { v = juce::jlimit(0.0f, 1.0f, std::isfinite(v) ? v : 0.0f); if (i == 0) rate = v; else depth = v; }
private:
    juce::dsp::Chorus<float> chorus;
    float rate = 0.3f, depth = 0.5f;
};

class PhaserEffect : public EffectBase
{
public:
    void prepare(double s, int b) override;
    void reset() override;
    void processWet(juce::AudioBuffer<float>& buffer) override;
    juce::String getTypeID() const override { return "phaser"; }
    juce::String getDisplayName() const override { return "Phaser"; }
    int getNumParams() const override { return 3; }
    juce::String getParamName(int i) const override { switch (i) { case 0: return "Rate"; case 1: return "Depth"; default: return "Fdbk"; } }
    float getParam(int i) const override { switch (i) { case 0: return rate; case 1: return depth; default: return feedback; } }
    void setParam(int i, float v) override { v = juce::jlimit(0.0f, 1.0f, std::isfinite(v) ? v : 0.0f); switch (i) { case 0: rate = v; break; case 1: depth = v; break; default: feedback = v; } }
private:
    juce::dsp::Phaser<float> phaser;
    float rate = 0.3f, depth = 0.5f, feedback = 0.4f;
};

class CompressorEffect : public EffectBase
{
public:
    void prepare(double s, int b) override;
    void reset() override;
    void processWet(juce::AudioBuffer<float>& buffer) override;
    juce::String getTypeID() const override { return "compressor"; }
    juce::String getDisplayName() const override { return "Compressor"; }
    int getNumParams() const override { return 3; }
    juce::String getParamName(int i) const override { switch (i) { case 0: return "Thresh"; case 1: return "Ratio"; default: return "Attack"; } }
    float getParam(int i) const override { switch (i) { case 0: return threshold; case 1: return ratio; default: return attack; } }
    void setParam(int i, float v) override;
private:
    juce::dsp::Compressor<float> comp;
    float threshold = 0.5f, ratio = 0.5f, attack = 0.5f;
    void updateParams();
};

class BitcrusherEffect : public EffectBase
{
public:
    void prepare(double s, int b) override { prepareBase(s, b, 2); reset(); }
    void reset() override { held = {}; phase = {}; }
    void processWet(juce::AudioBuffer<float>& buffer) override;
    juce::String getTypeID() const override { return "bitcrusher"; }
    juce::String getDisplayName() const override { return "Bitcrusher"; }
    int getNumParams() const override { return 2; }
    juce::String getParamName(int i) const override { return i == 0 ? "Bits" : "Rate"; }
    float getParam(int i) const override { return i == 0 ? bitDepth : downsample; }
    void setParam(int i, float v) override { v = juce::jlimit(0.0f, 1.0f, std::isfinite(v) ? v : 0.0f); if (i == 0) bitDepth = v; else downsample = v; }
private:
    float bitDepth = 0.5f, downsample = 0.5f;
    std::array<float, 2> held {};
    std::array<int, 2> phase {};
};

class FilterEffect : public EffectBase
{
public:
    void prepare(double s, int b) override;
    void reset() override;
    void processWet(juce::AudioBuffer<float>& buffer) override;
    juce::String getTypeID() const override { return "filter"; }
    juce::String getDisplayName() const override { return "Filter"; }
    int getNumParams() const override { return 2; }
    juce::String getParamName(int i) const override { return i == 0 ? "Cutoff" : "Reso"; }
    float getParam(int i) const override { return i == 0 ? cutoff : resonance; }
    void setParam(int i, float v) override;
private:
    juce::dsp::StateVariableTPTFilter<float> filter;
    float cutoff = 0.5f, resonance = 0.3f;
    void updateParams();
};
