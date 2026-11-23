
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace nebula::dsp
{
    constexpr float kMinSampleRate = 8000.0f;
    constexpr float kMaxSampleRate = 768000.0f;
    constexpr float kMinParameter = -1000000.0f;
    constexpr float kMaxParameter = 1000000.0f;
    constexpr float kSilenceThreshold = 1.0e-30f;

    // =========================================================
    // 1. NUMERIC SAFETY
    // =========================================================

    inline bool isFinite(float x) noexcept
    {
        return std::isfinite(x);
    }

    inline float finiteOr(float x, float fallback = 0.0f) noexcept
    {
        return isFinite(x) ? x : fallback;
    }

    inline float clampFinite(
        float x,
        float minimum,
        float maximum,
        float fallback = 0.0f) noexcept
    {
        if (!isFinite(x))
            x = fallback;

        if (!isFinite(minimum))
            minimum = 0.0f;

        if (!isFinite(maximum))
            maximum = 1.0f;

        if (minimum > maximum)
            std::swap(minimum, maximum);

        return std::max(minimum, std::min(x, maximum));
    }

    inline float sanitizeSample(float x) noexcept
    {
        if (!isFinite(x) || std::abs(x) < kSilenceThreshold)
            return 0.0f;

        return clampFinite(x, -64.0f, 64.0f);
    }

    inline float flushDenormal(float x) noexcept
    {
        return std::abs(x) < kSilenceThreshold ? 0.0f : x;
    }

    // =========================================================
    // 2. DECIBEL CONVERSION
    // =========================================================

    inline float decibelsToGain(float db) noexcept
    {
        db = clampFinite(db, -120.0f, 24.0f, 0.0f);

        if (db <= -120.0f)
            return 0.0f;

        return std::pow(10.0f, db * 0.05f);
    }

    inline float gainToDecibels(float gain) noexcept
    {
        gain = std::abs(finiteOr(gain));

        if (gain < 1.0e-6f)
            return -120.0f;

        return clampFinite(
            20.0f * std::log10(gain),
            -120.0f,
            24.0f,
            -120.0f);
    }

    // =========================================================
    // 3. LINEAR PARAMETER SMOOTHER
    // =========================================================

    class LinearSmoother
    {
    public:
        void prepare(
            double sampleRate,
            double rampMilliseconds,
            float initialValue = 0.0f) noexcept
        {
            const double sr =
                std::isfinite(sampleRate)
                ? std::clamp(sampleRate, 8000.0, 768000.0)
                : 44100.0;

            const double ms =
                std::isfinite(rampMilliseconds)
                ? std::clamp(rampMilliseconds, 0.0, 10000.0)
                : 10.0;

            sampleRate_ = sr;

            const double count = sr * ms * 0.001;

            rampSamples_ = static_cast<uint32_t>(
                std::clamp(
                    count,
                    0.0,
                    static_cast<double>(
                        std::numeric_limits<uint32_t>::max() / 2)));

            reset(initialValue);
        }

        void reset(float value) noexcept
        {
            value = finiteOr(value);

            current_ = value;
            target_ = value;
            increment_ = 0.0f;
            remaining_ = 0;
        }

        void setCurrentAndTargetValue(float value) noexcept
        {
            reset(value);
        }

        void setTargetValue(float value) noexcept
        {
            target_ = finiteOr(value, current_);

            if (rampSamples_ == 0)
            {
                reset(target_);
                return;
            }

            remaining_ = rampSamples_;

            increment_ =
                (target_ - current_) /
                static_cast<float>(remaining_);
        }

        float getNextValue() noexcept
        {
            if (remaining_ > 0)
            {
                current_ += increment_;
                --remaining_;

                if (remaining_ == 0)
                    current_ = target_;
            }

            return flushDenormal(current_);
        }

        float getCurrentValue() const noexcept
        {
            return current_;
        }

        float getTargetValue() const noexcept
        {
            return target_;
        }

        bool isSmoothing() const noexcept
        {
            return remaining_ > 0;
        }

        void skip(uint32_t samples) noexcept
        {
            if (remaining_ == 0)
                return;

            if (samples >= remaining_)
            {
                current_ = target_;
                remaining_ = 0;
                increment_ = 0.0f;
                return;
            }

            current_ += increment_ * static_cast<float>(samples);
            remaining_ -= samples;
        }

    private:
        double sampleRate_ = 44100.0;

        float current_ = 0.0f;
        float target_ = 0.0f;
        float increment_ = 0.0f;

        uint32_t rampSamples_ = 441;
        uint32_t remaining_ = 0;
    };

    // =========================================================
    // 4. ONE POLE FILTER
    // =========================================================

    class OnePole
    {
    public:
        void prepare(
            double sampleRate,
            float cutoffHz) noexcept
        {
            const double sr =
                std::isfinite(sampleRate)
                ? std::clamp(sampleRate, 8000.0, 768000.0)
                : 44100.0;

            const float cutoff = clampFinite(
                cutoffHz,
                1.0f,
                static_cast<float>(sr * 0.45),
                1000.0f);

            const double coefficient =
                1.0 - std::exp(
                    -6.28318530717958647692 *
                    static_cast<double>(cutoff) / sr);

            coefficient_ = static_cast<float>(
                std::clamp(coefficient, 0.0, 1.0));
        }

        void reset(float value = 0.0f) noexcept
        {
            state_ = finiteOr(value);
        }

        float process(float input) noexcept
        {
            input = sanitizeSample(input);

            state_ += coefficient_ * (input - state_);
            state_ = sanitizeSample(state_);

            return state_;
        }

        void setCoefficient(float coefficient) noexcept
        {
            coefficient_ = clampFinite(
                coefficient, 0.0f, 1.0f, 0.0f);
        }

    private:
        float coefficient_ = 0.1f;
        float state_ = 0.0f;
    };

    // =========================================================
    // 5. DC BLOCKER
    // =========================================================

    class DCBlocker
    {
    public:
        void prepare(
            double sampleRate,
            float cutoffHz = 10.0f) noexcept
        {
            const double sr =
                std::isfinite(sampleRate)
                ? std::clamp(sampleRate, 8000.0, 768000.0)
                : 44100.0;

            const float cutoff = clampFinite(
                cutoffHz, 1.0f, 40.0f, 10.0f);

            pole_ = static_cast<float>(
                std::exp(
                    -6.28318530717958647692 *
                    static_cast<double>(cutoff) / sr));

            pole_ = clampFinite(pole_, 0.0f, 0.99999f, 0.99f);

            reset();
        }

        void reset() noexcept
        {
            previousInput_ = 0.0f;
            previousOutput_ = 0.0f;
        }

        float process(float input) noexcept
        {
            input = sanitizeSample(input);

            const float output =
                input - previousInput_ +
                pole_ * previousOutput_;

            previousInput_ = input;
            previousOutput_ = sanitizeSample(output);

            return previousOutput_;
        }

    private:
        float pole_ = 0.995f;
        float previousInput_ = 0.0f;
        float previousOutput_ = 0.0f;
    };

    // =========================================================
    // 6. SOFT CLIPPER
    // =========================================================

    class SoftClipper
    {
    public:
        void setDrive(float drive) noexcept
        {
            drive_ = clampFinite(drive, 0.01f, 32.0f, 1.0f);
        }

        float process(float input) const noexcept
        {
            input = sanitizeSample(input);

            const float driven = input * drive_;

            // Smooth, bounded saturation.
            const float output = std::tanh(driven);

            return sanitizeSample(output);
        }

        void reset() noexcept
        {
            drive_ = 1.0f;
        }

    private:
        float drive_ = 1.0f;
    };

    // =========================================================
    // 7. SMOOTHED GAIN
    // =========================================================

    class SmoothedGain
    {
    public:
        void prepare(
            double sampleRate,
            float rampMilliseconds = 15.0f) noexcept
        {
            smoother_.prepare(
                sampleRate,
                rampMilliseconds,
                1.0f);
        }

        void setGain(float gain) noexcept
        {
            smoother_.setTargetValue(
                clampFinite(gain, 0.0f, 16.0f, 1.0f));
        }

        void setGainDecibels(float db) noexcept
        {
            setGain(decibelsToGain(db));
        }

        float process(float input) noexcept
        {
            return sanitizeSample(
                input * smoother_.getNextValue());
        }

        float getCurrentGain() const noexcept
        {
            return smoother_.getCurrentValue();
        }

        void reset(float gain = 1.0f) noexcept
        {
            smoother_.reset(
                clampFinite(gain, 0.0f, 16.0f, 1.0f));
        }

    private:
        LinearSmoother smoother_;
    };

    // =========================================================
    // 8. STEREO UTILITIES
    // =========================================================

    struct StereoSample
    {
        float left = 0.0f;
        float right = 0.0f;
    };

    inline StereoSample sanitizeStereo(
        StereoSample sample) noexcept
    {
        sample.left = sanitizeSample(sample.left);
        sample.right = sanitizeSample(sample.right);

        return sample;
    }

    inline StereoSample applyGain(
        StereoSample sample,
        float gain) noexcept
    {
        gain = clampFinite(gain, 0.0f, 16.0f, 1.0f);

        sample.left *= gain;
        sample.right *= gain;

        return sanitizeStereo(sample);
    }

    inline StereoSample softClipStereo(
        StereoSample sample,
        float drive) noexcept
    {
        SoftClipper clipper;
        clipper.setDrive(drive);

        sample.left = clipper.process(sample.left);
        sample.right = clipper.process(sample.right);

        return sample;
    }

    // =========================================================
    // 9. BUFFER SAFETY
    // =========================================================

    inline void sanitizeBuffer(
        float* samples,
        std::size_t numSamples) noexcept
    {
        if (samples == nullptr)
            return;

        for (std::size_t i = 0; i < numSamples; ++i)
            samples[i] = sanitizeSample(samples[i]);
    }

    inline void clearBuffer(
        float* samples,
        std::size_t numSamples) noexcept
    {
        if (samples == nullptr)
            return;

        for (std::size_t i = 0; i < numSamples; ++i)
            samples[i] = 0.0f;
    }

} // namespace nebula::dsp
