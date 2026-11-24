
#pragma once

#include <JuceHeader.h>
#include <atomic>

class NebulaAudioMetering
{
public:
    void prepare(double sampleRate) noexcept;
    void process(const juce::AudioBuffer<float>& buffer) noexcept;
    void clear() noexcept;

    float getPeak() const noexcept;
    float getRMS() const noexcept;
    float getPeakDb() const noexcept;
    float getRMSDb() const noexcept;

private:
    std::atomic<float> peakLevel { 0.0f };
    std::atomic<float> rmsLevel { 0.0f };
};
