#pragma once

#include <JuceHeader.h>
#include "Effects.h"

#include <atomic>
#include <functional>
#include <memory>
#include <vector>

class Rack
{
public:
    struct EffectInfo
    {
        juce::String typeID;
        juce::String displayName;
        bool enabled = true;
        float mix = 1.0f;
        std::vector<juce::String> parameterNames;
        std::vector<float> parameterValues;
    };

    Rack() = default;
    ~Rack() = default;

    Rack(const Rack&) = delete;
    Rack& operator=(const Rack&) = delete;

    void prepare(double sampleRate, int blockSize);
    void process(juce::AudioBuffer<float>& buffer);

    int size() const;
    bool getEffectInfo(int index, EffectInfo& result) const;

    bool setEffectEnabled(int index, bool enabled);
    bool setEffectMix(int index, float mix);
    bool setEffectParameter(int index, int parameter, float value);

    bool addEffect(const juce::String& typeID);
    void removeEffect(int index);
    void moveEffect(int from, int to);
    void clearAll();

    juce::StringArray getAvailableTypes() const;

    juce::ValueTree saveState() const;
    void loadState(const juce::ValueTree& tree);

    uint64_t getRevision() const noexcept
    {
        return revision.load(std::memory_order_relaxed);
    }

    std::function<void()> onChanged;

private:
    mutable juce::SpinLock rackLock;

    std::vector<std::unique_ptr<EffectBase>> effects;

    double sampleRate = 44100.0;
    int blockSize = 512;

    std::atomic<uint64_t> revision { 0 };

    std::unique_ptr<EffectBase> create(
        const juce::String& typeID) const;

    void notifyChanged();
};