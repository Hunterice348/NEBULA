#include "Effects.h"

// ---------- REVERB ----------
void ReverbEffect::prepare(double s, int b)
{
    sr = s; bs = b;
    juce::dsp::ProcessSpec spec { sr, (juce::uint32)bs, 2 };
    reverb.prepare(spec);
    reverb.reset();
    updateParams();
}
void ReverbEffect::reset() { reverb.reset(); }
void ReverbEffect::updateParams()
{
    juce::dsp::Reverb::Parameters p;
    p.roomSize = size;
    p.damping = damp;
    p.wetLevel = 1.0f;  // dry/wet handled by EffectBase
    p.dryLevel = 0.0f;
    p.width = 1.0f;
    p.freezeMode = 0.0f;
    reverb.setParameters(p);
}
void ReverbEffect::setParam(int i, float v)
{
    v = juce::jlimit(0.0f, 1.0f, v);
    switch (i) { case 0: size = v; break; case 1: decay = v; break; default: damp = v; }
    updateParams();
}
void ReverbEffect::processWet(juce::AudioBuffer<float>& buffer)
{
    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> ctx(block);
    reverb.process(ctx);
}

// ---------- DELAY ----------
void DelayEffect::prepare(double s, int b)
{
    sr = s; bs = b;
    maxSamples = (int)(sr * 2.0) + 1;
    bufL.assign(maxSamples, 0.0f);
    bufR.assign(maxSamples, 0.0f);
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
    if (buffer.getNumChannels() < 2 || maxSamples <= 1) return;
    int ds = juce::jlimit(1, maxSamples - 1, (int)(time * sr));
    auto* L = buffer.getWritePointer(0);
    auto* R = buffer.getWritePointer(1);
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        int rp = writePos - ds;
        if (rp < 0) rp += maxSamples;
        float dl = bufL[rp], dr = bufR[rp];
        bufL[writePos] = L[i] + dl * feedback;
        bufR[writePos] = R[i] + dr * feedback;
        L[i] = dl;
        R[i] = dr;
        writePos = (writePos + 1) % maxSamples;
    }
}

// ---------- DISTORTION ----------
void DistortionEffect::prepare(double s, int b)
{
    sr = s; bs = b;
    juce::dsp::ProcessSpec spec { sr, (juce::uint32)bs, 1 };
    toneL.prepare(spec); toneR.prepare(spec);
    toneL.reset(); toneR.reset();
    updateTone();
}
void DistortionEffect::reset() { toneL.reset(); toneR.reset(); }
void DistortionEffect::updateTone()
{
    float cut = juce::jmap(tone, 500.0f, 15000.0f);
    cut = juce::jlimit(20.0f, (float)(sr * 0.45), cut);
    auto c = juce::dsp::IIR::Coefficients<float>::makeLowPass(sr, cut, 0.707f);
    *toneL.coefficients = *c;
    *toneR.coefficients = *c;
}
void DistortionEffect::setParam(int i, float v)
{
    v = juce::jlimit(0.0f, 1.0f, v);
    if (i == 0) drive = v; else { tone = v; updateTone(); }
}
void DistortionEffect::processWet(juce::AudioBuffer<float>& buffer)
{
    float gain = 1.0f + drive * 20.0f;
    int nc = buffer.getNumChannels();
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        buffer.setSample(0, i, toneL.processSample(std::tanh(buffer.getSample(0, i) * gain)));
        if (nc > 1)
            buffer.setSample(1, i, toneR.processSample(std::tanh(buffer.getSample(1, i) * gain)));
    }
}

// ---------- CHORUS ----------
void ChorusEffect::prepare(double s, int b)
{
    sr = s; bs = b;
    juce::dsp::ProcessSpec spec { sr, (juce::uint32)bs, 2 };
    chorus.prepare(spec);
    chorus.reset();
}
void ChorusEffect::reset() { chorus.reset(); }
void ChorusEffect::processWet(juce::AudioBuffer<float>& buffer)
{
    chorus.setRate(0.1f + rate * 4.9f);
    chorus.setDepth(depth);
    chorus.setMix(1.0f);
    chorus.setCentreDelay(7.0f);
    chorus.setFeedback(0.0f);
    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> ctx(block);
    chorus.process(ctx);
}

// ---------- PHASER ----------
void PhaserEffect::prepare(double s, int b)
{
    sr = s; bs = b;
    juce::dsp::ProcessSpec spec { sr, (juce::uint32)bs, 2 };
    phaser.prepare(spec);
    phaser.reset();
}
void PhaserEffect::reset() { phaser.reset(); }
void PhaserEffect::processWet(juce::AudioBuffer<float>& buffer)
{
    phaser.setRate(0.1f + rate * 4.9f);
    phaser.setDepth(depth);
    phaser.setFeedback(feedback * 0.9f);
    phaser.setCentreFrequency(500.0f);
    phaser.setMix(1.0f);
    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> ctx(block);
    phaser.process(ctx);
}

// ---------- COMPRESSOR ----------
void CompressorEffect::prepare(double s, int b)
{
    sr = s; bs = b;
    juce::dsp::ProcessSpec spec { sr, (juce::uint32)bs, 2 };
    comp.prepare(spec);
    comp.reset();
    updateParams();
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
    v = juce::jlimit(0.0f, 1.0f, v);
    switch (i) { case 0: threshold = v; break; case 1: ratio = v; break; default: attack = v; }
    updateParams();
}
void CompressorEffect::processWet(juce::AudioBuffer<float>& buffer)
{
    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> ctx(block);
    comp.process(ctx);
}

// ---------- BITCRUSHER ----------
void BitcrusherEffect::processWet(juce::AudioBuffer<float>& buffer)
{
    int bits = juce::jlimit(2, 16, (int)(2 + bitDepth * 14));
    float levels = std::pow(2.0f, (float)bits);
    float step = 1.0f / levels;
    int dsFactor = juce::jlimit(1, 32, (int)(1 + downsample * 31));

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* data = buffer.getWritePointer(ch);
        float hold = 0.0f;
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            if (i % dsFactor == 0)
                hold = step * std::round(data[i] / step);
            data[i] = hold;
        }
    }
}

// ---------- FILTER ----------
void FilterEffect::prepare(double s, int b)
{
    sr = s; bs = b;
    juce::dsp::ProcessSpec spec { sr, (juce::uint32)bs, 2 };
    filter.prepare(spec);
    filter.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
    filter.reset();
    updateParams();
}
void FilterEffect::reset() { filter.reset(); }
void FilterEffect::updateParams()
{
    filter.setCutoffFrequency(juce::jmap(cutoff, 20.0f, 20000.0f));
    filter.setResonance(juce::jmap(resonance, 0.7f, 10.0f));
}
void FilterEffect::setParam(int i, float v)
{
    v = juce::jlimit(0.0f, 1.0f, v);
    if (i == 0) cutoff = v; else resonance = v;
    updateParams();
}
void FilterEffect::processWet(juce::AudioBuffer<float>& buffer)
{
    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> ctx(block);
    filter.process(ctx);
}
