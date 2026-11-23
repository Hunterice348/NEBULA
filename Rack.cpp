#include "Rack.h"

namespace
{
    class RackLock
    {
    public:
        explicit RackLock(const juce::SpinLock& lock)
            : scopedLock(lock)
        {
        }

    private:
        juce::SpinLock::ScopedLockType scopedLock;
    };
}

void Rack::notifyChanged()
{
    revision.fetch_add(1, std::memory_order_relaxed);

    if (onChanged)
        onChanged();
}

void Rack::prepare(double newSampleRate, int newBlockSize)
{
    RackLock lock(rackLock);

    sampleRate = juce::jmax(1.0, newSampleRate);
    blockSize = juce::jmax(1, newBlockSize);

    for (auto& effect : effects)
        effect->prepare(sampleRate, blockSize);
}

void Rack::process(juce::AudioBuffer<float>& buffer)
{
    // Audio thread never waits for the UI thread.
    // If the rack is being modified, this block passes through unchanged.
    juce::SpinLock::ScopedTryLockType lock(rackLock);

    if (!lock.isLocked())
        return;

    for (auto& effect : effects)
    {
        if (effect != nullptr)
            effect->process(buffer);
    }
}

int Rack::size() const
{
    RackLock lock(rackLock);
    return static_cast<int>(effects.size());
}

bool Rack::getEffectInfo(int index, EffectInfo& result) const
{
    RackLock lock(rackLock);

    if (index < 0 || index >= static_cast<int>(effects.size()))
        return false;

    const auto& effect = effects[static_cast<size_t>(index)];

    if (effect == nullptr)
        return false;

    result.typeID = effect->getTypeID();
    result.displayName = effect->getDisplayName();
    result.enabled = effect->isEnabled();
    result.mix = effect->getMix();

    result.parameterNames.clear();
    result.parameterValues.clear();

    const int count = effect->getNumParams();

    result.parameterNames.reserve(static_cast<size_t>(count));
    result.parameterValues.reserve(static_cast<size_t>(count));

    for (int i = 0; i < count; ++i)
    {
        result.parameterNames.push_back(effect->getParamName(i));
        result.parameterValues.push_back(effect->getParam(i));
    }

    return true;
}

bool Rack::setEffectEnabled(int index, bool enabled)
{
    RackLock lock(rackLock);

    if (index < 0 || index >= static_cast<int>(effects.size()))
        return false;

    effects[static_cast<size_t>(index)]->setEnabled(enabled);
    return true;
}

bool Rack::setEffectMix(int index, float mix)
{
    RackLock lock(rackLock);

    if (index < 0 || index >= static_cast<int>(effects.size()))
        return false;

    effects[static_cast<size_t>(index)]->setMix(mix);
    return true;
}

bool Rack::setEffectParameter(int index, int parameter, float value)
{
    RackLock lock(rackLock);

    if (index < 0 || index >= static_cast<int>(effects.size()))
        return false;

    auto& effect = effects[static_cast<size_t>(index)];

    if (parameter < 0 || parameter >= effect->getNumParams())
        return false;

    effect->setParam(parameter, value);
    return true;
}

std::unique_ptr<EffectBase> Rack::create(
    const juce::String& id) const
{
    if (id == "reverb")
        return std::make_unique<ReverbEffect>();

    if (id == "delay")
        return std::make_unique<DelayEffect>();

    if (id == "distortion")
        return std::make_unique<DistortionEffect>();

    if (id == "chorus")
        return std::make_unique<ChorusEffect>();

    if (id == "phaser")
        return std::make_unique<PhaserEffect>();

    if (id == "compressor")
        return std::make_unique<CompressorEffect>();

    if (id == "bitcrusher")
        return std::make_unique<BitcrusherEffect>();

    if (id == "filter")
        return std::make_unique<FilterEffect>();

    return nullptr;
}

bool Rack::addEffect(const juce::String& typeID)
{
    {
        RackLock lock(rackLock);

        auto effect = create(typeID);

        if (effect == nullptr)
            return false;

        effect->prepare(sampleRate, blockSize);
        effects.push_back(std::move(effect));
    }

    notifyChanged();
    return true;
}

void Rack::removeEffect(int index)
{
    bool changed = false;

    {
        RackLock lock(rackLock);

        if (index >= 0 && index < static_cast<int>(effects.size()))
        {
            effects.erase(effects.begin() + index);
            changed = true;
        }
    }

    if (changed)
        notifyChanged();
}

void Rack::moveEffect(int from, int to)
{
    bool changed = false;

    {
        RackLock lock(rackLock);

        const int count = static_cast<int>(effects.size());

        if (from < 0 || from >= count ||
            to < 0 || to >= count ||
            from == to)
            return;

        auto effect = std::move(effects[static_cast<size_t>(from)]);

        effects.erase(effects.begin() + from);
        effects.insert(effects.begin() + to, std::move(effect));

        changed = true;
    }

    if (changed)
        notifyChanged();
}

void Rack::clearAll()
{
    {
        RackLock lock(rackLock);
        effects.clear();
    }

    notifyChanged();
}

juce::StringArray Rack::getAvailableTypes() const
{
    return {
        "Reverb",
        "Delay",
        "Distortion",
        "Chorus",
        "Phaser",
        "Compressor",
        "Bitcrusher",
        "Filter"
    };
}

juce::ValueTree Rack::saveState() const
{
    RackLock lock(rackLock);

    juce::ValueTree tree("RACK");

    for (const auto& effect : effects)
    {
        if (effect != nullptr)
            tree.appendChild(effect->saveState(), nullptr);
    }

    return tree;
}

void Rack::loadState(const juce::ValueTree& tree)
{
    if (!tree.isValid() || !tree.hasType("RACK"))
        return;

    {
        RackLock lock(rackLock);

        std::vector<std::unique_ptr<EffectBase>> newEffects;
        newEffects.reserve(static_cast<size_t>(tree.getNumChildren()));

        for (int i = 0; i < tree.getNumChildren(); ++i)
        {
            const auto child = tree.getChild(i);

            auto effect = create(child.getType().toString());

            if (effect == nullptr)
                continue;

            effect->prepare(sampleRate, blockSize);
            effect->loadState(child);

            newEffects.push_back(std::move(effect));
        }

        effects.swap(newEffects);
    }

    notifyChanged();
}