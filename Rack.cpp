
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

    // Called by the host during preparation, normally while audio is stopped.
    for (auto& effect : effects)
    {
        if (effect != nullptr)
            effect->prepare(sampleRate, blockSize);
    }
}

void Rack::process(juce::AudioBuffer<float>& buffer)
{
    // Never wait on the audio thread.
    // A later ownership-handoff stage will remove this try-lock fallback.
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

    auto& effect = effects[static_cast<size_t>(index)];

    if (effect == nullptr)
        return false;

    effect->setEnabled(enabled);
    return true;
}

bool Rack::setEffectMix(int index, float mix)
{
    RackLock lock(rackLock);

    if (index < 0 || index >= static_cast<int>(effects.size()))
        return false;

    auto& effect = effects[static_cast<size_t>(index)];

    if (effect == nullptr)
        return false;

    effect->setMix(mix);
    return true;
}

bool Rack::setEffectParameter(int index, int parameter, float value)
{
    RackLock lock(rackLock);

    if (index < 0 || index >= static_cast<int>(effects.size()))
        return false;

    auto& effect = effects[static_cast<size_t>(index)];

    if (effect == nullptr)
        return false;

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
    // Read preparation settings under the lock, but do not construct or
    // prepare the DSP object while holding the rack lock.
    double targetSampleRate = 44100.0;
    int targetBlockSize = 512;

    {
        RackLock lock(rackLock);
        targetSampleRate = sampleRate;
        targetBlockSize = blockSize;
    }

    auto effect = create(typeID);

    if (effect == nullptr)
        return false;

    effect->prepare(targetSampleRate, targetBlockSize);

    {
        RackLock lock(rackLock);

        // The host may have reconfigured the processor while this effect
        // was being constructed. Re-prepare only if the settings changed.
        if (targetSampleRate != sampleRate ||
            targetBlockSize != blockSize)
        {
            effect->prepare(sampleRate, blockSize);
        }

        effects.push_back(std::move(effect));
    }

    notifyChanged();
    return true;
}

void Rack::removeEffect(int index)
{
    std::unique_ptr<EffectBase> removedEffect;

    {
        RackLock lock(rackLock);

        if (index < 0 || index >= static_cast<int>(effects.size()))
            return;

        auto it = effects.begin() + index;

        // Move ownership out first. The effect destructor runs after
        // the lock is released.
        removedEffect = std::move(*it);
        effects.erase(it);
    }

    removedEffect.reset();
    notifyChanged();
}

void Rack::moveEffect(int from, int to)
{
    {
        RackLock lock(rackLock);

        const int count = static_cast<int>(effects.size());

        if (from < 0 || from >= count ||
            to < 0 || to >= count ||
            from == to)
        {
            return;
        }

        auto effect = std::move(effects[static_cast<size_t>(from)]);

        effects.erase(effects.begin() + from);
        effects.insert(effects.begin() + to, std::move(effect));
    }

    notifyChanged();
}

void Rack::clearAll()
{
    std::vector<std::unique_ptr<EffectBase>> removedEffects;

    {
        RackLock lock(rackLock);
        removedEffects.swap(effects);
    }

    // Destroy all DSP objects outside the rack lock.
    removedEffects.clear();

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

    double targetSampleRate = 44100.0;
    int targetBlockSize = 512;

    {
        RackLock lock(rackLock);
        targetSampleRate = sampleRate;
        targetBlockSize = blockSize;
    }

    // Build and prepare the complete replacement chain outside the lock.
    std::vector<std::unique_ptr<EffectBase>> newEffects;
    newEffects.reserve(static_cast<size_t>(tree.getNumChildren()));

    for (int i = 0; i < tree.getNumChildren(); ++i)
    {
        const auto child = tree.getChild(i);

        auto effect = create(child.getType().toString());

        if (effect == nullptr)
            continue;

        effect->prepare(targetSampleRate, targetBlockSize);
        effect->loadState(child);

        newEffects.push_back(std::move(effect));
    }

    std::vector<std::unique_ptr<EffectBase>> oldEffects;

    {
        RackLock lock(rackLock);

        // Reconcile with a host reconfiguration that occurred while the
        // replacement chain was being built.
        if (targetSampleRate != sampleRate ||
            targetBlockSize != blockSize)
        {
            for (auto& effect : newEffects)
            {
                if (effect != nullptr)
                    effect->prepare(sampleRate, blockSize);
            }
        }

        oldEffects.swap(effects);
        effects.swap(newEffects);
    }

    // Old DSP objects are destroyed only after the lock is released.
    oldEffects.clear();

    notifyChanged();
}
