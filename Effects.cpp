
#include "Effects.h"

// ============================================================
// REVERB
// ============================================================

void ReverbEffect::prepare(double sampleRate, int blockSize)
{
    prepareBase(sampleRate, blockSize, 2);

    juce::dsp::ProcessSpec spec {
        sr,
        (juce::uint32)bs,
        2
    };

    reverb.prepare(spec);
    reverb.reset();
    updateParams();
}

void ReverbEffect::reset()
{
    reverb.reset();
}

void ReverbEffect::updateParams()
{
    const float currentSize = size.load(std::memory_order_relaxed);
    const float currentDecay = decay.load(std::memory_order_relaxed);
    const float currentDamp = damp.load(std::memory_order_relaxed);

    juce::dsp::Reverb::Parameters p;

    p.roomSize = juce::jlimit(
        0.05f,
        0.95f,
        currentSize * (0.2f + 0.8f * currentDecay));

    p.damping = juce::jlimit(0.0f, 1.0f, currentDamp);
    p.wetLevel = 1.0f;
    p.dryLevel = 0.0f;
    p.width = 1.0f;
    p.freezeMode = 0.0f;

    reverb.setParameters(p);
}

void ReverbEffect::setParam(int index, float value)
{
    if (!std::isfinite(value))
        return;

    value = juce::jlimit(0.0f, 1.0f, value);

    switch (index)
    {
        case 0: size.store(value, std::memory_order_relaxed); break;
        case 1: decay.store(value, std::memory_order_relaxed); break;
        case 2: damp.store(value, std::memory_order_relaxed); break;
        default: break;
    }
}

void ReverbEffect::processWet(juce::AudioBuffer<float>& buffer)
{
    updateParams();

    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);

    reverb.process(context);
}

// ============================================================
// DELAY
// ============================================================

void DelayEffect::prepare(double sampleRate, int blockSize)
{
    prepareBase(sampleRate, blockSize, 2);

    maxSamples = juce::jmax(
        2,
        (int)(sr * 2.0) + 1);

    bufL.assign((size_t)maxSamples, 0.0f);
    bufR.assign((size_t)maxSamples, 0.0f);

    writePos = 0;
}

void DelayEffect::reset()
{
    std::fill(bufL.begin(), bufL.end(), 0.0f);
    std::fill(bufR.begin(), bufR.end(), 0.0f);

    writePos = 0;
}

void DelayEffect::processWet(juce::AudioBuffer<float>& buffer)
{
    if (maxSamples <= 1 || buffer.getNumChannels() < 1)
        return;

    const float currentTime = time.load(std::memory_order_relaxed);
    const float currentFeedback = feedback.load(std::memory_order_relaxed);

    const int delaySamples = juce::jlimit(
        1,
        maxSamples - 1,
        (int)(currentTime * (float)sr));

    const float fb = juce::jlimit(0.0f, 0.95f, currentFeedback);

    auto* left = buffer.getWritePointer(0);

    auto* right = buffer.getNumChannels() > 1
        ? buffer.getWritePointer(1)
        : nullptr;

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        int readPos = writePos - delaySamples;

        if (readPos < 0)
            readPos += maxSamples;

        const float inputL = std::isfinite(left[i])
            ? left[i] : 0.0f;

        const float delayedL = bufL[(size_t)readPos];

        const float delayedR = right != nullptr
            ? bufR[(size_t)readPos]
            : 0.0f;

        bufL[(size_t)writePos] = inputL + delayedL * fb;
        left[i] = delayedL;

        if (right != nullptr)
        {
            const float inputR = std::isfinite(right[i])
                ? right[i] : 0.0f;

            bufR[(size_t)writePos] = inputR + delayedR * fb;
            right[i] = delayedR;
        }
        else
        {
            bufR[(size_t)writePos] = 0.0f;
        }

        if (++writePos >= maxSamples)
            writePos = 0;
    }
}

// ============================================================
// DISTORTION
// ============================================================

void DistortionEffect::prepare(double sampleRate, int blockSize)
{
    prepareBase(sampleRate, blockSize, 2);
    reset();
}

void DistortionEffect::reset()
{
    toneState = {};
}

void DistortionEffect::processWet(juce::AudioBuffer<float>& buffer)
{
    const float currentDrive = drive.load(std::memory_order_relaxed);
    const float currentTone = tone.load(std::memory_order_relaxed);

    const float gain = 1.0f + currentDrive * 20.0f;

    const float cutoff = juce::jlimit(
        20.0f,
        (float)(sr * 0.45),
        juce::jmap(currentTone, 500.0f, 15000.0f));

    const float alpha = 1.0f - std::exp(
        -juce::MathConstants<float>::twoPi * cutoff / (float)sr);

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* data = buffer.getWritePointer(ch);
        const size_t channel = (size_t)juce::jmin(ch, 1);

        float state = toneState[channel];

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const float input = std::isfinite(data[i])
                ? data[i] : 0.0f;

            const float driven = std::tanh(input * gain);

            state += alpha * (driven - state);

            data[i] = state;
        }

        toneState[channel] = state;
    }
}

// ============================================================
// CHORUS
// ============================================================

void ChorusEffect::prepare(double sampleRate, int blockSize)
{
    prepareBase(sampleRate, blockSize, 2);

    juce::dsp::ProcessSpec spec {
        sr,
        (juce::uint32)bs,
        2
    };

    chorus.prepare(spec);
    chorus.reset();
}

void ChorusEffect::reset()
{
    chorus.reset();
}

void ChorusEffect::processWet(juce::AudioBuffer<float>& buffer)
{
    const float currentRate = rate.load(std::memory_order_relaxed);
    const float currentDepth = depth.load(std::memory_order_relaxed);

    chorus.setRate(0.1f + currentRate * 4.9f);
    chorus.setDepth(currentDepth);
    chorus.setMix(1.0f);
    chorus.setCentreDelay(7.0f);
    chorus.setFeedback(0.0f);

    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);

    chorus.process(context);
}

// ============================================================
// PHASER
// ============================================================

void PhaserEffect::prepare(double sampleRate, int blockSize)
{
    prepareBase(sampleRate, blockSize, 2);

    juce::dsp::ProcessSpec spec {
        sr,
        (juce::uint32)bs,
        2
    };

    phaser.prepare(spec);
    phaser.reset();
}

void PhaserEffect::reset()
{
    phaser.reset();
}

void PhaserEffect::processWet(juce::AudioBuffer<float>& buffer)
{
    const float currentRate = rate.load(std::memory_order_relaxed);
    const float currentDepth = depth.load(std::memory_order_relaxed);
    const float currentFeedback = feedback.load(std::memory_order_relaxed);

    phaser.setRate(0.1f + currentRate * 4.9f);
    phaser.setDepth(currentDepth);

    phaser.setFeedback(
        juce::jlimit(0.0f, 0.85f, currentFeedback * 0.85f));

    phaser.setCentreFrequency(500.0f);
    phaser.setMix(1.0f);

    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);

    phaser.process(context);
}

// ============================================================
// COMPRESSOR
// ============================================================

void CompressorEffect::prepare(double sampleRate, int blockSize)
{
    prepareBase(sampleRate, blockSize, 2);

    juce::dsp::ProcessSpec spec {
        sr,
        (juce::uint32)bs,
        2
    };

    comp.prepare(spec);
    comp.reset();
}

void CompressorEffect::reset()
{
    comp.reset();
}

void CompressorEffect::processWet(juce::AudioBuffer<float>& buffer)
{
    const float currentThreshold = threshold.load(std::memory_order_relaxed);
    const float currentRatio = ratio.load(std::memory_order_relaxed);
    const float currentAttack = attack.load(std::memory_order_relaxed);

    comp.setThreshold(
        juce::jmap(currentThreshold, -60.0f, 0.0f));

    comp.setRatio(
        juce::jmap(currentRatio, 1.0f, 20.0f));

    comp.setAttack(
        juce::jmap(currentAttack, 0.001f, 0.5f));

    comp.setRelease(0.1f);

    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);

    comp.process(context);
}

// ============================================================
// BITCRUSHER
// ============================================================

void BitcrusherEffect::processWet(juce::AudioBuffer<float>& buffer)
{
    const float currentBitDepth = bitDepth.load(std::memory_order_relaxed);
    const float currentDownsample = downsample.load(std::memory_order_relaxed);

    const int bits = juce::jlimit(
        2,
        16,
        (int)(2.0f + currentBitDepth * 14.0f));

    const float step = 1.0f / std::pow(2.0f, (float)bits);

    const int factor = juce::jlimit(
        1,
        32,
        (int)(1.0f + currentDownsample * 31.0f));

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* data = buffer.getWritePointer(ch);

        const size_t channel = (size_t)juce::jmin(ch, 1);

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            if (phase[channel] == 0)
            {
                const float input = std::isfinite(data[i])
                    ? data[i] : 0.0f;

                held[channel] = step * std::round(input / step);
            }

            data[i] = held[channel];

            phase[channel] = (phase[channel] + 1) % factor;
        }
    }
}

// ============================================================
// FILTER
// ============================================================

void FilterEffect::prepare(double sampleRate, int blockSize)
{
    prepareBase(sampleRate, blockSize, 2);

    juce::dsp::ProcessSpec spec {
        sr,
        (juce::uint32)bs,
        2
    };

    filter.prepare(spec);

    filter.setType(
        juce::dsp::StateVariableTPTFilterType::lowpass);

    filter.reset();
}

void FilterEffect::reset()
{
    filter.reset();
}

void FilterEffect::processWet(juce::AudioBuffer<float>& buffer)
{
    const float currentCutoff = cutoff.load(std::memory_order_relaxed);
    const float currentResonance = resonance.load(std::memory_order_relaxed);

    const float frequency = juce::jlimit(
        20.0f,
        (float)(sr * 0.45),
        juce::jmap(currentCutoff, 20.0f, 20000.0f));

    const float q = juce::jlimit(
        0.7f,
        5.0f,
        juce::jmap(currentResonance, 0.7f, 5.0f));

    filter.setCutoffFrequency(frequency);
    filter.setResonance(q);

    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> context(block);

    filter.process(context);
}
