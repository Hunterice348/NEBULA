#include "Rack.h"

void Rack::prepare(double sampleRate, int blockSize)
{
    sr = sampleRate; bs = blockSize;
    for (auto& e : effects) e->prepare(sr, bs);
}

void Rack::process(juce::AudioBuffer<float>& buffer)
{
    for (auto& e : effects) e->process(buffer);
}

std::unique_ptr<EffectBase> Rack::create(const juce::String& id) const
{
    if (id == "reverb")      return std::make_unique<ReverbEffect>();
    if (id == "delay")       return std::make_unique<DelayEffect>();
    if (id == "distortion")  return std::make_unique<DistortionEffect>();
    if (id == "chorus")      return std::make_unique<ChorusEffect>();
    if (id == "phaser")      return std::make_unique<PhaserEffect>();
    if (id == "compressor")  return std::make_unique<CompressorEffect>();
    if (id == "bitcrusher")  return std::make_unique<BitcrusherEffect>();
    if (id == "filter")      return std::make_unique<FilterEffect>();
    return nullptr;
}

bool Rack::addEffect(const juce::String& typeID)
{
    if (auto fx = create(typeID))
    {
        fx->prepare(sr, bs);
        effects.push_back(std::move(fx));
        if (onChanged) onChanged();
        return true;
    }
    return false;
}

void Rack::removeEffect(int i)
{
    if (i >= 0 && i < (int)effects.size())
    {
        effects.erase(effects.begin() + i);
        if (onChanged) onChanged();
    }
}

void Rack::moveEffect(int from, int to)
{
    if (from < 0 || from >= (int)effects.size()) return;
    if (to < 0 || to >= (int)effects.size()) return;
    if (from == to) return;
    auto fx = std::move(effects[from]);
    effects.erase(effects.begin() + from);
    effects.insert(effects.begin() + to, std::move(fx));
    if (onChanged) onChanged();
}

void Rack::clearAll()
{
    effects.clear();
    if (onChanged) onChanged();
}

EffectBase* Rack::get(int i)
{
    return (i >= 0 && i < (int)effects.size()) ? effects[i].get() : nullptr;
}
const EffectBase* Rack::get(int i) const
{
    return (i >= 0 && i < (int)effects.size()) ? effects[i].get() : nullptr;
}

juce::StringArray Rack::getAvailableTypes() const
{
    return { "Reverb", "Delay", "Distortion", "Chorus",
             "Phaser", "Compressor", "Bitcrusher", "Filter" };
}

juce::ValueTree Rack::saveState() const
{
    juce::ValueTree t("RACK");
    for (auto& e : effects) t.appendChild(e->saveState(), nullptr);
    return t;
}

void Rack::loadState(const juce::ValueTree& tree)
{
    effects.clear();
    if (!tree.hasType("RACK")) return;
    for (int i = 0; i < tree.getNumChildren(); ++i)
    {
        auto child = tree.getChild(i);
        if (auto fx = create(child.getType().toString()))
        {
            fx->prepare(sr, bs);
            fx->loadState(child);
            effects.push_back(std::move(fx));
        }
    }
    if (onChanged) onChanged();
}
