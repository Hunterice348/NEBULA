#pragma once
#include <JuceHeader.h>
#include "Effects.h"

class Rack
{
public:
    Rack() = default;

    void prepare(double sampleRate, int blockSize);
    void process(juce::AudioBuffer<float>& buffer);

    int size() const { return (int)effects.size(); }
    EffectBase* get(int i);
    const EffectBase* get(int i) const;

    bool addEffect(const juce::String& typeID);
    void removeEffect(int index);
    void moveEffect(int from, int to);
    void clearAll();

    juce::StringArray getAvailableTypes() const;

    juce::ValueTree saveState() const;
    void loadState(const juce::ValueTree& tree);

    std::function<void()> onChanged;

private:
    std::vector<std::unique_ptr<EffectBase>> effects;
    double sr = 44100.0;
    int bs = 512;

    std::unique_ptr<EffectBase> create(const juce::String& typeID) const;
};
