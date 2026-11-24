#pragma once

#include "BasicEffects.h"
#include "../Core/ParameterSmoothing.h"

struct NebulaAudioParameters
{
    float inputGain = 1.0f;
    float outputGain = 1.0f;
    float wet = 0.5f;
    float drive = 2.0f;
    float feedback = 0.35f;
    float delaySamples = 15435.0f;
    int mode = 0; // 0=clean, 1=delay, 2=saturation
};

// Reusable mono/multichannel engine. Channel-major buffers are passed through
// without copying. Variable callback sizes are accepted; prepare() allocates
// effect storage off the audio path.
class NebulaAudioEngine final
{
public:
    void prepare(double sampleRate, int channels, int maximumBlockSize);
    void release() noexcept;
    void reset() noexcept;
    void setParameters(const NebulaAudioParameters& parameters) noexcept;
    void processBlock(float* const* channels, int numChannels, int numSamples) noexcept;

    double getSampleRate() const noexcept { return sampleRate; }
    int getNumChannels() const noexcept { return preparedChannels; }
    bool isPrepared() const noexcept { return prepared; }

private:
    double sampleRate = 44100.0;
    int preparedChannels = 0;
    bool prepared = false;
    int mode = 0;

    NebulaParameterSmoothing inputGainSmoother;
    NebulaParameterSmoothing outputGainSmoother;
    NebulaParameterSmoothing wetSmoother;
    NebulaParameterSmoothing driveSmoother;
    NebulaParameterSmoothing feedbackSmoother;
    NebulaParameterSmoothing delaySmoother;

    NebulaDelayEffect delay;
    NebulaSaturationEffect saturation;
};
