#pragma once
#include <JuceHeader.h>

namespace Nebula {
    class Compressor {
    public:
        Compressor() = default;

        void prepare(const juce::dsp::ProcessSpec& spec) {
            sampleRate = spec.sampleRate;
            z1 = 0.0f;
            envelope = 0.0f;
        }

        void reset() {
            z1 = 0.0f;
            envelope = 0.0f;
        }

        // Вызывать из processBlock
        void process(juce::AudioBuffer<float>& buffer, float thresholdDb, float ratio, float attackMs, float releaseMs) {
            float threshold = juce::Decibels::decibelsToGain(thresholdDb);
            float alphaAttack = juce::jmin(1.0f, static_cast<float>(1.0 - std::exp(-1.0 / (sampleRate * attackMs / 1000.0))));
            float alphaRelease = juce::jmin(1.0f, static_cast<float>(1.0 - std::exp(-1.0 / (sampleRate * releaseMs / 1000.0))));
            float slope = 1.0f - (1.0f / ratio);

            for (int channel = 0; channel < buffer.getNumChannels(); ++channel) {
                float* data = buffer.getWritePointer(channel);
                for (int i = 0; i < buffer.getNumSamples(); ++i) {
                    float in = data[i];
                    
                    // NaN/Infinity защита
                    if (!std::isfinite(in)) { in = 0.0f; }

                    float rectified = std::abs(in);
                    if (rectified > threshold) {
                        envelope = alphaAttack * (rectified - envelope) + envelope;
                    } else {
                        envelope = alphaRelease * (rectified - envelope) + envelope;
                    }

                    float overshoot = juce::jmax(0.0f, envelope - threshold);
                    float gainReduction = juce::Decibels::gainToDecibels(1.0f - (overshoot * slope));
                    
                    // Сглаживание применения гейна для избежания щелчков
                    z1 = (0.99f * z1) + (0.01f * gainReduction);
                    float appliedGain = juce::Decibels::decibelsToGain(z1);

                    data[i] = in * appliedGain;
                }
            }
        }

    private:
        double sampleRate = 44100.0;
        float z1 = 0.0f;
        float envelope = 0.0f;
    };
}
