#include "Effects.h"

void ReverbEffect::prepare(double s, int b)
{
    prepareBase(s, b, 2);
    juce::dsp::ProcessSpec spec { sr, (juce::uint32)bs, 2 };
    reverb.prepare(spec);
    reverb.reset();
    updateParams();
}
void ReverbEffect::reset() { reverb.reset(); }
void ReverbEffect::updateParams()
{
    juce::dsp::Reverb::Parameters p;
    // JUCE's algorithm exposes room size rather than a separate T60 control.
    // Decay therefore maps monotonically to the algorithm's stable room-size range.
    p.roomSize = juce::jlimit(0.05f, 0.95f, size * (0.2f + 0.8f * decay));
    p.damping = juce::jlimit(0.0f, 1.0f, damp);
    p.wetLevel = 1.0f;
    p.dryLevel = 0.0f;
    p.width = 1.0f;
    p.freezeMode = 0.0f;
    reverb.setParameters(p);
}
void ReverbEffect::setParam(int i, float v)
{
    v = juce::jlimit(0.0f, 1.0f, std::isfinite(v) ? v : 0.0f);
    switch (i) { case 0: size = v; break; case 1: decay = v; break; default: damp = v; }
    updateParams();
}
void ReverbEffect::processWet(juce::AudioBuffer<float>& buffer)
{
    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> ctx(block);
    reverb.process(ctx);
}

void DelayEffect::prepare(double s, int b)
{
    prepareBase(s, b, 2);
    maxSamples = juce::jmax(2, (int) (sr * 2.0) + 1);
    bufL.assign((size_t) maxSamples, 0.0f);
    bufR.assign((size_t) maxSamples, 0.0f);
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
    if (maxSamples <= 1 || buffer.getNumChannels() < 1) return;
    const int delaySamples = juce::jlimit(1, maxSamples - 1, (int) (time * (float) sr));
    const float fb = juce::jlimit(0.0f, 0.95f, feedback);
    auto* left = buffer.getWritePointer(0);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        int readPos = writePos - delaySamples;
        if (readPos < 0) readPos += maxSamples;
        const float inL = std::isfinite(left[i]) ? left[i] : 0.0f;
        const float delayedL = bufL[(size_t) readPos];
        const float delayedR = right != nullptr ? bufR[(size_t) readPos] : 0.0f;
        bufL[(size_t) writePos] = inL + delayedL * fb;
        left[i] = delayedL;

        if (right != nullptr)
        {
            const float inR = std::isfinite(right[i]) ? right[i] : 0.0f;
            bufR[(size_t) writePos] = inR + delayedR * fb;
            right[i] = delayedR;
        }
        else
        {
            // Mono uses the left delay line only; clear the unused channel's
            // corresponding feedback sample so a prior stereo state cannot leak.
            bufR[(size_t) writePos] = 0.0f;
        }
        if (++writePos == maxSamples) writePos = 0;
    }
}

void DistortionEffect::prepare(double s, int b)
{
    prepareBase(s, b, 2);
    juce::dsp::ProcessSpec spec { sr, (juce::uint32) bs, 1 };
    toneL.prepare(spec); toneR.prepare(spec);
    toneL.reset(); toneR.reset();
    updateTone();
}
void DistortionEffect::reset() { toneL.reset(); toneR.reset(); }
void DistortionEffect::updateTone()
{
    const float cut = juce::jlimit(20.0f, (float) (sr * 0.45), juce::jmap(tone, 500.0f, 15000.0f));
    auto c = juce::dsp::IIR::Coefficients<float>::makeLowPass(sr, cut, 0.707f);
    *toneL.coefficients = *c;
    *toneR.coefficients = *c;
}
void DistortionEffect::setParam(int i, float v)
{
    v = juce::jlimit(0.0f, 1.0f, std::isfinite(v) ? v : 0.0f);
    if (i == 0) drive = v; else { tone = v; updateTone(); }
}
void DistortionEffect::processWet(juce::AudioBuffer<float>& buffer)
{
    const float gain = 1.0f + drive * 20.0f;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* data = buffer.getWritePointer(ch);
        auto& filter = ch == 0 ? toneL : toneR;
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const float x = std::isfinite(data[i]) ? data[i] : 0.0f;
            data[i] = filter.processSample(std::tanh(x * gain));
        }
    }
}

void ChorusEffect::prepare(double s, int b)
{
    prepareBase(s, b, 2);
    juce::dsp::ProcessSpec spec { sr, (juce::uint32) bs, 2 };
    chorus.prepare(spec); chorus.reset();
}
void ChorusEffect::reset() { chorus.reset(); }
void ChorusEffect::processWet(juce::AudioBuffer<float>& buffer)
{
    chorus.setRate(0.1f + rate * 4.9f);
    chorus.setDepth(depth); chorus.setMix(1.0f); chorus.setCentreDelay(7.0f); chorus.setFeedback(0.0f);
    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> ctx(block);
    chorus.process(ctx);
}

void PhaserEffect::prepare(double s, int b)
{
    prepareBase(s, b, 2);
    juce::dsp::ProcessSpec spec { sr, (juce::uint32) bs, 2 };
    phaser.prepare(spec); phaser.reset();
}
void PhaserEffect::reset() { phaser.reset(); }
void PhaserEffect::processWet(juce::AudioBuffer<float>& buffer)
{
    phaser.setRate(0.1f + rate * 4.9f); phaser.setDepth(depth);
    phaser.setFeedback(juce::jlimit(0.0f, 0.85f, feedback * 0.85f));
    phaser.setCentreFrequency(500.0f); phaser.setMix(1.0f);
    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> ctx(block);
    phaser.process(ctx);
}

void CompressorEffect::prepare(double s, int b)
{
    prepareBase(s, b, 2);
    juce::dsp::ProcessSpec spec { sr, (juce::uint32) bs, 2 };
    comp.prepare(spec); comp.reset(); updateParams();
}
void CompressorEffect::reset() { comp.reset(); }
void CompressorEffect::updateParams()
{
    comp.setThreshold(juce::jmap(threshold, -60.0f, 0.0f));
    comp.setRatio(juce::jmap(ratio, 1.0f, 20.0f));
    comp.setAttack(juce::jmap(attack, 0.001f, 0.5f));
    comp.setRelease(0.1f);
}
void CompressorEffect::setParam(int i, float v)
{
    v = juce::jlimit(0.0f, 1.0f, std::isfinite(v) ? v : 0.0f);
    switch (i) { case 0: threshold = v; break; case 1: ratio = v; break; default: attack = v; }
    updateParams();
}
void CompressorEffect::processWet(juce::AudioBuffer<float>& buffer)
{
    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> ctx(block);
    comp.process(ctx);
}

void BitcrusherEffect::processWet(juce::AudioBuffer<float>& buffer)
{
    const int bits = juce::jlimit(2, 16, (int) (2 + bitDepth * 14));
    const float step = 1.0f / std::pow(2.0f, (float) bits);
    const int factor = juce::jlimit(1, 32, (int) (1 + downsample * 31));
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* data = buffer.getWritePointer(ch);
        const size_t stateIndex = (size_t) juce::jmin(ch, 1);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            if (phase[stateIndex] == 0)
            {
                const float x = std::isfinite(data[i]) ? data[i] : 0.0f;
                held[stateIndex] = step * std::round(x / step);
            }
            data[i] = held[stateIndex];
            phase[stateIndex] = (phase[stateIndex] + 1) % factor;
        }
    }
}

void FilterEffect::prepare(double s, int b)
{
    prepareBase(s, b, 2);
    juce::dsp::ProcessSpec spec { sr, (juce::uint32) bs, 2 };
    filter.prepare(spec); filter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
    filter.reset(); updateParams();
}
void FilterEffect::reset() { filter.reset(); }
void FilterEffect::updateParams()
{
    filter.setCutoffFrequency(juce::jlimit(20.0f, (float) (sr * 0.45), juce::jmap(cutoff, 20.0f, 20000.0f)));
    filter.setResonance(juce::jlimit(0.7f, 5.0f, juce::jmap(resonance, 0.7f, 5.0f)));
}
void FilterEffect::setParam(int i, float v)
{
    v = juce::jlimit(0.0f, 1.0f, std::isfinite(v) ? v : 0.0f);
    if (i == 0) cutoff = v; else resonance = v;
    updateParams();
}
void FilterEffect::processWet(juce::AudioBuffer<float>& buffer)
{
    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> ctx(block);
    filter.process(ctx);
}
