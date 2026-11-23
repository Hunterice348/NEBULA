
#pragma once

#include <array>
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cmath>

namespace nebula::sequencer
{

constexpr int maxSteps = 64;
constexpr int maxTracks = 16;

struct Step
{
    bool enabled = false;
    float velocity = 0.8f;
    float probability = 1.0f;
    float pan = 0.0f;
    float length = 1.0f;
    int ratchet = 1;
    int microTiming = 0;
};

struct Track
{
    std::array<Step, maxSteps> steps {};
    bool muted = false;
    bool solo = false;
    float volume = 1.0f;
    float pan = 0.0f;
};

struct SequencerEvent
{
    int track = 0;
    int step = 0;
    int ratchetIndex = 0;
    int samplePosition = 0;

    float velocity = 0.0f;
    float pan = 0.0f;
    float length = 1.0f;
};

class StepSequencer
{
public:
    static constexpr int trackCount = maxTracks;

    StepSequencer() = default;

    void reset() noexcept
    {
        currentStep = 0;
        sampleCounter = 0;
        samplesPerStep = 1;
        stepStartSample = 0;
        eventCount = 0;
    }

    void prepare(double newSampleRate) noexcept
    {
        sampleRate = std::isfinite(newSampleRate)
            ? std::clamp(newSampleRate, 8000.0, 384000.0)
            : 44100.0;

        updateTiming();
        reset();
    }

    void setTempo(double bpm) noexcept
    {
        tempo.store(
            std::clamp(
                std::isfinite(bpm) ? bpm : 120.0,
                20.0,
                300.0
            ),
            std::memory_order_relaxed
        );

        updateTiming();
    }

    double getTempo() const noexcept
    {
        return tempo.load(std::memory_order_relaxed);
    }

    void setStepsPerBeat(int value) noexcept
    {
        stepsPerBeat.store(
            std::clamp(value, 1, 16),
            std::memory_order_relaxed
        );

        updateTiming();
    }

    void setSwing(float value) noexcept
    {
        swing.store(
            std::clamp(
                std::isfinite(value) ? value : 0.0f,
                0.0f,
                0.75f
            ),
            std::memory_order_relaxed
        );
    }

    float getSwing() const noexcept
    {
        return swing.load(std::memory_order_relaxed);
    }

    void setPlaying(bool shouldPlay) noexcept
    {
        playing.store(
            shouldPlay,
            std::memory_order_relaxed
        );
    }

    bool isPlaying() const noexcept
    {
        return playing.load(std::memory_order_relaxed);
    }

    void setTrackMuted(int index, bool value) noexcept
    {
        if (!validTrack(index))
            return;

        tracks[index].muted = value;
    }

    void setTrackSolo(int index, bool value) noexcept
    {
        if (!validTrack(index))
            return;

        tracks[index].solo = value;
    }

    void setTrackVolume(int index, float value) noexcept
    {
        if (!validTrack(index))
            return;

        tracks[index].volume = clamp01(value);
    }

    void setTrackPan(int index, float value) noexcept
    {
        if (!validTrack(index))
            return;

        tracks[index].pan = std::clamp(
            std::isfinite(value) ? value : 0.0f,
            -1.0f,
            1.0f
        );
    }

    void setStep(
        int track,
        int step,
        const Step& value
    ) noexcept
    {
        if (!validTrack(track) || !validStep(step))
            return;

        Step safe = value;

        safe.velocity = clamp01(safe.velocity);
        safe.probability = clamp01(safe.probability);
        safe.pan = std::clamp(safe.pan, -1.0f, 1.0f);
        safe.length = clamp01(safe.length);
        safe.ratchet = std::clamp(safe.ratchet, 1, 8);
        safe.microTiming = std::clamp(
            safe.microTiming,
            -100,
            100
        );

        tracks[track].steps[step] = safe;
    }

    Step getStep(int track, int step) const noexcept
    {
        if (!validTrack(track) || !validStep(step))
            return {};

        return tracks[track].steps[step];
    }

    void clearTrack(int track) noexcept
    {
        if (!validTrack(track))
            return;

        tracks[track].steps = {};
    }

    void clearAll() noexcept
    {
        for (auto& track : tracks)
            track = Track {};

        reset();
    }

    int getCurrentStep() const noexcept
    {
        return currentStep;
    }

    int getSamplesPerStep() const noexcept
    {
        return samplesPerStep;
    }

    // Call once for every audio sample.
    // Events are written into a fixed-size buffer.
    // No allocation or locking occurs here.
    int processSample(
        SequencerEvent* output,
        int outputCapacity
    ) noexcept
    {
        if (!isPlaying()
            || output == nullptr
            || outputCapacity <= 0)
            return 0;

        eventCount = 0;

        const int swingOffset =
            (currentStep & 1)
                ? static_cast<int>(
                    samplesPerStep * getSwing()
                  )
                : 0;

        const int effectiveLength =
            samplesPerStep + swingOffset;

        if (sampleCounter >= effectiveLength)
        {
            sampleCounter = 0;
            stepStartSample = 0;
            currentStep = (currentStep + 1) % patternLength;
        }

        const int localPosition = sampleCounter;

        if (localPosition == 0)
            triggerStep(output, outputCapacity);

        ++sampleCounter;

        return eventCount;
    }

    void setPatternLength(int value) noexcept
    {
        patternLength = std::clamp(value, 1, maxSteps);

        if (currentStep >= patternLength)
            currentStep = 0;
    }

    int getPatternLength() const noexcept
    {
        return patternLength;
    }

private:
    static bool validTrack(int index) noexcept
    {
        return index >= 0 && index < maxTracks;
    }

    static bool validStep(int index) noexcept
    {
        return index >= 0 && index < maxSteps;
    }

    static float clamp01(float value) noexcept
    {
        return std::clamp(
            std::isfinite(value) ? value : 0.0f,
            0.0f,
            1.0f
        );
    }

    void updateTiming() noexcept
    {
        const double bpm = getTempo();
        const int subdivision =
            stepsPerBeat.load(std::memory_order_relaxed);

        const double secondsPerStep =
            60.0 / bpm / static_cast<double>(subdivision);

        samplesPerStep = std::max(
            1,
            static_cast<int>(sampleRate * secondsPerStep)
        );
    }

    void triggerStep(
        SequencerEvent* output,
        int capacity
    ) noexcept
    {
        bool hasSolo = false;

        for (const auto& track : tracks)
            hasSolo = hasSolo || track.solo;

        for (int trackIndex = 0;
             trackIndex < maxTracks;
             ++trackIndex)
        {
            if (eventCount >= capacity)
                return;

            const auto& track = tracks[trackIndex];

            if (track.muted)
                continue;

            if (hasSolo && !track.solo)
                continue;

            const auto& step =
                track.steps[currentStep];

            if (!step.enabled)
                continue;

            const float probability =
                step.probability;

            // Deterministic pseudo-random probability.
            randomState ^= randomState << 13;
            randomState ^= randomState >> 17;
            randomState ^= randomState << 5;

            const float randomValue =
                static_cast<float>(
                    randomState & 0x00FFFFFFu
                ) / 16777216.0f;

            if (randomValue > probability)
                continue;

            const int ratchets =
                std::clamp(step.ratchet, 1, 8);

            for (int ratchet = 0;
                 ratchet < ratchets;
                 ++ratchet)
            {
                if (eventCount >= capacity)
                    return;

                SequencerEvent event;

                event.track = trackIndex;
                event.step = currentStep;
                event.ratchetIndex = ratchet;
                event.samplePosition =
                    ratchet * samplesPerStep / ratchets;

                event.velocity =
                    step.velocity * track.volume;

                event.pan = std::clamp(
                    step.pan + track.pan,
                    -1.0f,
                    1.0f
                );

                event.length = step.length;

                output[eventCount++] = event;
            }
        }
    }

    std::array<Track, maxTracks> tracks {};

    std::atomic<double> tempo { 120.0 };
    std::atomic<int> stepsPerBeat { 4 };
    std::atomic<float> swing { 0.0f };
    std::atomic<bool> playing { false };

    double sampleRate = 44100.0;

    int samplesPerStep = 5512;
    int sampleCounter = 0;
    int stepStartSample = 0;
    int currentStep = 0;
    int patternLength = 16;
    int eventCount = 0;

    uint32_t randomState = 0xA53C9E21u;
};

} // namespace nebula::sequencer
