
#pragma once

#include "Effects.h"

#include <array>
#include <atomic>
#include <vector>

// ============================================================
// NEBULA ADVANCED DSP EFFECTS
//
// All parameters are normalized to the 0.0 - 1.0 range.
// Effect state is prepared outside the audio callback.
// No dynamic allocation is intended inside processWet().
// ============================================================

class AdvancedEffect final : public EffectBase
{
public:
    explicit AdvancedEffect(const juce::String& effectType);

    ~AdvancedEffect() override = default;

    void prepare(double sampleRate, int maxBlockSize) override;
    void reset() override;
    void processWet(juce::AudioBuffer<float>& buffer) override;

    juce::String getTypeID() const override;
    juce::String getDisplayName() const override;

    int getNumParams() const override
    {
        return 2;
    }

    juce::String getParamName(int index) const override;
    float getParam(int index) const override;
    void setParam(int index, float value) override;

    static bool supports(const juce::String& typeID);

private:
    enum class Mode
    {
        stereoWidener,
        autoPan,
        noiseGate,
        transientShaper,
        tremolo,
        ringModulator,
        combFilter,
        tapeSaturation,
        lofi,
        haasDelay,
        invalid
    };

    Mode mode = Mode::invalid;
    juce::String typeID;

    std::array<std::atomic<float>, 2> parameters
    {{
        0.5f,
        0.5f
    }};

    // Per-channel DSP state.
    std::array<float, 2> envelope {};
    std::array<float, 2> filterState {};
    std::array<float, 2> heldSample {};
    std::array<int, 2> holdCounter {};

    // Delay memory is allocated in prepare(), never in processWet().
    std::array<std::vector<float>, 2> delayBuffers;

    double phase = 0.0;
    int writePosition = 0;
    int delayBufferSize = 0;

    static float clamp01(float value) noexcept;
    static float finiteOrZero(float value) noexcept;

    void processStereoWidener(
        juce::AudioBuffer<float>& buffer) noexcept;

    void processAutoPan(
        juce::AudioBuffer<float>& buffer) noexcept;

    void processNoiseGate(
        juce::AudioBuffer<float>& buffer) noexcept;

    void processTransientShaper(
        juce::AudioBuffer<float>& buffer) noexcept;

    void processTremolo(
        juce::AudioBuffer<float>& buffer) noexcept;

    void processRingModulator(
        juce::AudioBuffer<float>& buffer) noexcept;

    void processCombFilter(
        juce::AudioBuffer<float>& buffer) noexcept;

    void processTapeSaturation(
        juce::AudioBuffer<float>& buffer) noexcept;

    void processLoFi(
        juce::AudioBuffer<float>& buffer) noexcept;

    void processHaasDelay(
        juce::AudioBuffer<float>& buffer) noexcept;
};
