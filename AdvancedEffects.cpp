
#include "AdvancedEffects.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr double twoPi = 6.28318530717958647692;

    int wrapPosition(int position, int size) noexcept
    {
        if (size <= 0)
            return 0;

        while (position < 0)
            position += size;

        while (position >= size)
            position -= size;

        return position;
    }
}

AdvancedEffect::AdvancedEffect(const juce::String& effectType)
    : typeID(effectType.trim().toLowerCase())
{
    for (auto& parameter : parameters)
        parameter.store(0.5f, std::memory_order_relaxed);

    if (typeID == "stereo_widener")
        mode = Mode::stereoWidener;
    else if (typeID == "autopan")
        mode = Mode::autoPan;
    else if (typeID == "noise_gate")
        mode = Mode::noiseGate;
    else if (typeID == "transient")
        mode = Mode::transientShaper;
    else if (typeID == "tremolo")
        mode = Mode::tremolo;
    else if (typeID == "ring_mod")
        mode = Mode::ringModulator;
    else if (typeID == "comb_filter")
        mode = Mode::combFilter;
    else if (typeID == "tape_saturation")
        mode = Mode::tapeSaturation;
    else if (typeID == "lofi")
        mode = Mode::lofi;
    else if (typeID == "haas_delay")
        mode = Mode::haasDelay;
}

bool AdvancedEffect::supports(const juce::String& id)
{
    const auto value = id.trim().toLowerCase();

    return value == "stereo_widener"
        || value == "autopan"
        || value == "noise_gate"
        || value == "transient"
        || value == "tremolo"
        || value == "ring_mod"
        || value == "comb_filter"
        || value == "tape_saturation"
        || value == "lofi"
        || value == "haas_delay";
}

float AdvancedEffect::clamp01(float value) noexcept
{
    if (!std::isfinite(value))
        return 0.0f;

    return juce::jlimit(0.0f, 1.0f, value);
}

float AdvancedEffect::finiteOrZero(float value) noexcept
{
    return std::isfinite(value) ? value : 0.0f;
}

juce::String AdvancedEffect::getTypeID() const
{
    return typeID;
}

juce::String AdvancedEffect::getDisplayName() const
{
    if (mode == Mode::stereoWidener) return "Stereo Widener";
    if (mode == Mode::autoPan) return "Auto Pan";
    if (mode == Mode::noiseGate) return "Noise Gate";
    if (mode == Mode::transientShaper) return "Transient Shaper";
    if (mode == Mode::tremolo) return "Tremolo";
    if (mode == Mode::ringModulator) return "Ring Modulator";
    if (mode == Mode::combFilter) return "Comb Filter";
    if (mode == Mode::tapeSaturation) return "Tape Saturation";
    if (mode == Mode::lofi) return "Lo-Fi";
    if (mode == Mode::haasDelay) return "Haas Delay";

    return "Unknown Effect";
}

juce::String AdvancedEffect::getParamName(int index) const
{
    if (index < 0 || index >= 2)
        return {};

    switch (mode)
    {
        case Mode::stereoWidener:
            return index == 0 ? "Width" : "Center";

        case Mode::autoPan:
            return index == 0 ? "Rate" : "Depth";

        case Mode::noiseGate:
            return index == 0 ? "Threshold" : "Release";

        case Mode::transientShaper:
            return index == 0 ? "Attack" : "Sustain";

        case Mode::tremolo:
            return index == 0 ? "Rate" : "Depth";

        case Mode::ringModulator:
            return index == 0 ? "Frequency" : "Shape";

        case Mode::combFilter:
            return index == 0 ? "Delay" : "Feedback";

        case Mode::tapeSaturation:
            return index == 0 ? "Drive" : "Tone";

        case Mode::lofi:
            return index == 0 ? "Bits" : "Rate";

        case Mode::haasDelay:
            return index == 0 ? "Delay" : "Width";

        default:
            return {};
    }
}

float AdvancedEffect::getParam(int index) const
{
    if (index < 0 || index >= 2)
        return 0.0f;

    return parameters[static_cast<size_t>(index)].load(
        std::memory_order_relaxed);
}

void AdvancedEffect::setParam(int index, float value)
{
    if (index < 0 || index >= 2 || !std::isfinite(value))
        return;

    parameters[static_cast<size_t>(index)].store(
        clamp01(value),
        std::memory_order_relaxed);
}

void AdvancedEffect::prepare(double sampleRate, int maxBlockSize)
{
    prepareBase(sampleRate, maxBlockSize, 2);

    // 100 ms maximum storage is sufficient for the short
    // comb and Haas delay algorithms.
    delayBufferSize = juce::jmax(
        8,
        static_cast<int>(sr * 0.1) + 8);

    for (auto& channel : delayBuffers)
        channel.assign(static_cast<size_t>(delayBufferSize), 0.0f);

    reset();
}

void AdvancedEffect::reset()
{
    envelope = {};
    filterState = {};
    heldSample = {};
    holdCounter = {};

    phase = 0.0;
    writePosition = 0;

    for (auto& channel : delayBuffers)
        std::fill(channel.begin(), channel.end(), 0.0f);
}

void AdvancedEffect::processWet(juce::AudioBuffer<float>& buffer)
{
    if (buffer.getNumSamples() <= 0 || buffer.getNumChannels() <= 0)
        return;

    switch (mode)
    {
        case Mode::stereoWidener: processStereoWidener(buffer); break;
        case Mode::autoPan: processAutoPan(buffer); break;
        case Mode::noiseGate: processNoiseGate(buffer); break;
        case Mode::transientShaper: processTransientShaper(buffer); break;
        case Mode::tremolo: processTremolo(buffer); break;
        case Mode::ringModulator: processRingModulator(buffer); break;
        case Mode::combFilter: processCombFilter(buffer); break;
        case Mode::tapeSaturation: processTapeSaturation(buffer); break;
        case Mode::lofi: processLoFi(buffer); break;
        case Mode::haasDelay: processHaasDelay(buffer); break;
        default: break;
    }
}

// ============================================================
// STEREO WIDENER
// ============================================================

void AdvancedEffect::processStereoWidener(
    juce::AudioBuffer<float>& buffer) noexcept
{
    if (buffer.getNumChannels() < 2)
        return;

    const float width = juce::jmap(
        getParam(0), 0.0f, 2.0f);

    const float center = getParam(1);

    auto* left = buffer.getWritePointer(0);
    auto* right = buffer.getWritePointer(1);

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const float l = finiteOrZero(left[i]);
        const float r = finiteOrZero(right[i]);

        const float mid = (l + r) * 0.5f;
        const float side = (l - r) * 0.5f;

        const float widened = side * width;
        const float centered = mid * juce::jmap(center, 0.7f, 1.3f);

        left[i] = centered + widened;
        right[i] = centered - widened;
    }
}

// ============================================================
// AUTO PAN
// ============================================================

void AdvancedEffect::processAutoPan(
    juce::AudioBuffer<float>& buffer) noexcept
{
    const float rate = juce::jmap(getParam(0), 0.05f, 8.0f);
    const float depth = getParam(1);

    const double increment = twoPi * rate / juce::jmax(1.0, sr);

    auto* left = buffer.getWritePointer(0);
    auto* right = buffer.getNumChannels() > 1
        ? buffer.getWritePointer(1)
        : nullptr;

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const float lfo = static_cast<float>(std::sin(phase));
        const float pan = lfo * depth;

        const float leftGain = std::sqrt(
            juce::jlimit(0.0f, 1.0f, 0.5f - pan * 0.5f) * 2.0f);

        const float rightGain = std::sqrt(
            juce::jlimit(0.0f, 1.0f, 0.5f + pan * 0.5f) * 2.0f);

        left[i] = finiteOrZero(left[i]) * leftGain;

        if (right != nullptr)
            right[i] = finiteOrZero(right[i]) * rightGain;

        phase += increment;

        if (phase >= twoPi)
            phase -= twoPi;
    }
}

// ============================================================
// NOISE GATE
// ============================================================

void AdvancedEffect::processNoiseGate(
    juce::AudioBuffer<float>& buffer) noexcept
{
    const float threshold = juce::Decibels::decibelsToGain(
        juce::jmap(getParam(0), -60.0f, -12.0f));

    const float releaseMs = juce::jmap(
        getParam(1), 10.0f, 500.0f);

    const float releaseCoefficient = std::exp(
        -1.0f / (static_cast<float>(sr) * releaseMs * 0.001f));

    const float attackCoefficient = std::exp(
        -1.0f / (static_cast<float>(sr) * 0.001f));

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* data = buffer.getWritePointer(ch);
        float env = envelope[static_cast<size_t>(ch)];

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const float input = finiteOrZero(data[i]);
            const float magnitude = std::abs(input);

            const float coefficient =
                magnitude > env
                    ? attackCoefficient
                    : releaseCoefficient;

            env = coefficient * env
                + (1.0f - coefficient) * magnitude;

            const float gate = env >= threshold
                ? 1.0f
                : juce::jlimit(0.0f, 1.0f, env / threshold);

            data[i] = input * gate;
        }

        envelope[static_cast<size_t>(ch)] = env;
    }
}

// ============================================================
// TRANSIENT SHAPER
// ============================================================

void AdvancedEffect::processTransientShaper(
    juce::AudioBuffer<float>& buffer) noexcept
{
    const float attackAmount = juce::jmap(
        getParam(0), 0.0f, 2.5f);

    const float sustainAmount = juce::jmap(
        getParam(1), 0.0f, 0.75f);

    const float attackCoefficient = std::exp(
        -1.0f / (static_cast<float>(sr) * 0.002f));

    const float releaseCoefficient = std::exp(
        -1.0f / (static_cast<float>(sr) * 0.08f));

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* data = buffer.getWritePointer(ch);
        float env = envelope[static_cast<size_t>(ch)];

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const float input = finiteOrZero(data[i]);
            const float magnitude = std::abs(input);

            const float coefficient =
                magnitude > env
                    ? attackCoefficient
                    : releaseCoefficient;

            env = coefficient * env
                + (1.0f - coefficient) * magnitude;

            const float transient = input
                - std::copysign(env, input);

            data[i] = input
                + transient * attackAmount
                - input * sustainAmount * 0.25f;
        }

        envelope[static_cast<size_t>(ch)] = env;
    }
}

// ============================================================
// TREMOLO
// ============================================================

void AdvancedEffect::processTremolo(
    juce::AudioBuffer<float>& buffer) noexcept
{
    const float rate = juce::jmap(getParam(0), 0.1f, 15.0f);
    const float depth = getParam(1);

    const double increment = twoPi * rate / juce::jmax(1.0, sr);

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const float lfo = static_cast<float>(
            0.5 + 0.5 * std::sin(phase));

        const float gain = 1.0f - depth + depth * lfo;

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* data = buffer.getWritePointer(ch);
            data[i] = finiteOrZero(data[i]) * gain;
        }

        phase += increment;

        if (phase >= twoPi)
            phase -= twoPi;
    }
}

// ============================================================
// RING MODULATOR
// ============================================================

void AdvancedEffect::processRingModulator(
    juce::AudioBuffer<float>& buffer) noexcept
{
    const float frequency = juce::jmap(
        getParam(0), 20.0f, 1200.0f);

    const float shape = getParam(1);

    const double increment = twoPi * frequency / juce::jmax(1.0, sr);

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const float sine = static_cast<float>(std::sin(phase));
        const float square = sine >= 0.0f ? 1.0f : -1.0f;

        const float carrier = juce::jmap(shape, sine, square);

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* data = buffer.getWritePointer(ch);
            data[i] = finiteOrZero(data[i]) * carrier;
        }

        phase += increment;

        if (phase >= twoPi)
            phase -= twoPi;
    }
}

// ============================================================
// COMB FILTER
// ============================================================

void AdvancedEffect::processCombFilter(
    juce::AudioBuffer<float>& buffer) noexcept
{
    if (delayBufferSize <= 2)
        return;

    const int delaySamples = juce::jlimit(
        1,
        delayBufferSize - 2,
        static_cast<int>(juce::jmap(
            getParam(0), 1.0f, 30.0f) * 0.001 * sr));

    const float feedback = juce::jmap(
        getParam(1), 0.0f, 0.85f);

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto& delay = delayBuffers[static_cast<size_t>(ch)];
            auto* data = buffer.getWritePointer(ch);

            const float input = finiteOrZero(data[i]);

            const int readPosition = wrapPosition(
                writePosition - delaySamples,
                delayBufferSize);

            const float delayed =
                delay[static_cast<size_t>(readPosition)];

            delay[static_cast<size_t>(writePosition)] =
                input + delayed * feedback;

            data[i] = input + delayed * 0.7f;
        }

        if (++writePosition >= delayBufferSize)
            writePosition = 0;
    }
}

// ============================================================
// TAPE SATURATION
// ============================================================

void AdvancedEffect::processTapeSaturation(
    juce::AudioBuffer<float>& buffer) noexcept
{
    const float drive = juce::jmap(
        getParam(0), 1.0f, 8.0f);

    const float cutoff = juce::jmap(
        getParam(1), 1200.0f, 18000.0f);

    const float coefficient = 1.0f - std::exp(
        -twoPi * cutoff / juce::jmax(1.0, sr));

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* data = buffer.getWritePointer(ch);
        float state = filterState[static_cast<size_t>(ch)];

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const float input = finiteOrZero(data[i]);
            const float saturated = std::tanh(input * drive);

            state += coefficient * (saturated - state);
            data[i] = state;
        }

        filterState[static_cast<size_t>(ch)] = state;
    }
}

// ============================================================
// LO-FI
// ============================================================

void AdvancedEffect::processLoFi(
    juce::AudioBuffer<float>& buffer) noexcept
{
    const int bits = juce::jlimit(
        4,
        16,
        static_cast<int>(juce::jmap(getParam(0), 4.0f, 16.0f)));

    const float quantizationStep =
        1.0f / std::pow(2.0f, static_cast<float>(bits));

    const int holdFactor = juce::jlimit(
        1,
        48,
        static_cast<int>(juce::jmap(getParam(1), 1.0f, 48.0f)));

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* data = buffer.getWritePointer(ch);
        const auto channel = static_cast<size_t>(ch);

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            if (holdCounter[channel] <= 0)
            {
                const float input = finiteOrZero(data[i]);

                heldSample[channel] =
                    std::round(input / quantizationStep)
                    * quantizationStep;

                holdCounter[channel] = holdFactor;
            }

            data[i] = heldSample[channel];
            --holdCounter[channel];
        }
    }
}

// ============================================================
// HAAS DELAY
// ============================================================

void AdvancedEffect::processHaasDelay(
    juce::AudioBuffer<float>& buffer) noexcept
{
    if (delayBufferSize <= 2)
        return;

    const int baseDelay = juce::jlimit(
        1,
        delayBufferSize - 2,
        static_cast<int>(juce::jmap(
            getParam(0), 1.0f, 20.0f) * 0.001 * sr));

    const int stereoOffset = static_cast<int>(
        getParam(1) * 0.015f * static_cast<float>(sr));

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto& delay = delayBuffers[static_cast<size_t>(ch)];
            auto* data = buffer.getWritePointer(ch);

            const float input = finiteOrZero(data[i]);

            const int channelDelay = juce::jlimit(
                1,
                delayBufferSize - 2,
                baseDelay + (ch == 1 ? stereoOffset : 0));

            const int readPosition = wrapPosition(
                writePosition - channelDelay,
                delayBufferSize);

            const float delayed =
                delay[static_cast<size_t>(readPosition)];

            delay[static_cast<size_t>(writePosition)] = input;

            data[i] = input * 0.65f + delayed * 0.35f;
        }

        if (++writePosition >= delayBufferSize)
            writePosition = 0;
    }
}
