
#include "Rack.h"

#include <cmath>
#include <utility>

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

// ============================================================
// AUDIO CHAIN
// ============================================================

struct Rack::AudioChain
{
    std::vector<std::unique_ptr<EffectBase>> effects;

    void process(juce::AudioBuffer<float>& buffer) noexcept
    {
        for (auto& effect : effects)
        {
            if (effect != nullptr)
                effect->process(buffer);
        }
    }

    void setEnabled(int index, bool enabled) noexcept
    {
        if (index < 0 || index >= static_cast<int>(effects.size()))
            return;

        auto& effect = effects[static_cast<size_t>(index)];

        if (effect != nullptr)
            effect->setEnabled(enabled);
    }

    void setMix(int index, float value) noexcept
    {
        if (index < 0 || index >= static_cast<int>(effects.size()))
            return;

        auto& effect = effects[static_cast<size_t>(index)];

        if (effect != nullptr)
            effect->setMix(value);
    }

    void setParameter(
        int effectIndex,
        int parameterIndex,
        float value) noexcept
    {
        if (effectIndex < 0 ||
            effectIndex >= static_cast<int>(effects.size()))
            return;

        auto& effect = effects[static_cast<size_t>(effectIndex)];

        if (effect == nullptr)
            return;

        if (parameterIndex < 0 ||
            parameterIndex >= effect->getNumParams())
            return;

        effect->setParam(parameterIndex, value);
    }
};

// ============================================================
// DESTRUCTION
// ============================================================

Rack::~Rack()
{
    // The host must stop audio processing before destroying
    // the processor and its Rack instance.

    drainRetired();

    Command command;

    while (tryPopCommand(command))
    {
        if (command.type == CommandType::replaceChain)
            delete command.chain;
    }

    delete deferredNewChain;
    deferredNewChain = nullptr;

    delete deferredRetiredChain;
    deferredRetiredChain = nullptr;

    delete activeChain;
    activeChain = nullptr;

    drainRetired();
}

// ============================================================
// NOTIFICATION
// ============================================================

void Rack::notifyChanged()
{
    revision.fetch_add(1, std::memory_order_relaxed);

    if (onChanged)
        onChanged();
}

// ============================================================
// COMMAND QUEUE
// ============================================================

bool Rack::tryPushCommand(const Command& command) noexcept
{
    const size_t write =
        commandWritePosition.load(std::memory_order_relaxed);

    const size_t next = (write + 1) & queueMask;

    const size_t read =
        commandReadPosition.load(std::memory_order_acquire);

    if (next == read)
        return false;

    commandQueue[write] = command;

    commandWritePosition.store(
        next,
        std::memory_order_release);

    return true;
}

bool Rack::tryPopCommand(Command& command) noexcept
{
    const size_t read =
        commandReadPosition.load(std::memory_order_relaxed);

    const size_t write =
        commandWritePosition.load(std::memory_order_acquire);

    if (read == write)
        return false;

    command = commandQueue[read];

    commandReadPosition.store(
        (read + 1) & queueMask,
        std::memory_order_release);

    return true;
}

// ============================================================
// RETIREMENT QUEUE
// ============================================================

bool Rack::tryPushRetired(AudioChain* chain) noexcept
{
    if (chain == nullptr)
        return true;

    const size_t write =
        retiredWritePosition.load(std::memory_order_relaxed);

    const size_t next = (write + 1) & queueMask;

    const size_t read =
        retiredReadPosition.load(std::memory_order_acquire);

    if (next == read)
        return false;

    retiredChains[write] = chain;

    retiredWritePosition.store(
        next,
        std::memory_order_release);

    return true;
}

bool Rack::tryPopRetired(AudioChain*& chain) noexcept
{
    const size_t read =
        retiredReadPosition.load(std::memory_order_relaxed);

    const size_t write =
        retiredWritePosition.load(std::memory_order_acquire);

    if (read == write)
        return false;

    chain = retiredChains[read];

    retiredReadPosition.store(
        (read + 1) & queueMask,
        std::memory_order_release);

    return true;
}

// ============================================================
// RETIRED OBJECT COLLECTION
// ============================================================

void Rack::drainRetired()
{
    RackLock lock(retiredReadLock);

    AudioChain* chain = nullptr;

    while (tryPopRetired(chain))
    {
        // Destruction happens on the control side,
        // never inside process().
        delete chain;
        chain = nullptr;
    }
}

// ============================================================
// CHAIN CREATION
// ============================================================

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

std::unique_ptr<Rack::AudioChain> Rack::buildChain(
    const std::vector<std::unique_ptr<EffectBase>>& source,
    double targetSampleRate,
    int targetBlockSize) const
{
    auto chain = std::make_unique<AudioChain>();

    chain->effects.reserve(source.size());

    for (const auto& prototype : source)
    {
        if (prototype == nullptr)
            continue;

        const auto state = prototype->saveState();

        auto effect = create(state.getType().toString());

        if (effect == nullptr)
            continue;

        effect->prepare(
            targetSampleRate,
            targetBlockSize);

        effect->loadState(state);

        chain->effects.push_back(std::move(effect));
    }

    return chain;
}

// ============================================================
// CHAIN PUBLICATION
// ============================================================

bool Rack::publishChain(std::unique_ptr<AudioChain> chain)
{
    if (chain == nullptr)
        return false;

    Command command;
    command.type = CommandType::replaceChain;
    command.chain = chain.get();

    bool queued = false;

    {
        RackLock lock(commandWriteLock);
        queued = tryPushCommand(command);
    }

    if (!queued)
        return false;

    // Ownership transfers to the audio command queue.
    chain.release();

    return true;
}

bool Rack::publishCurrentChainLocked()
{
    auto chain = buildChain(
        effects,
        sampleRate,
        blockSize);

    return publishChain(std::move(chain));
}

// ============================================================
// PREPARE
// ============================================================

void Rack::prepare(double newSampleRate, int newBlockSize)
{
    drainRetired();

    bool changed = false;

    {
        RackLock lock(rackLock);

        const double oldSampleRate = sampleRate;
        const int oldBlockSize = blockSize;

        sampleRate = juce::jmax(1.0, newSampleRate);
        blockSize = juce::jmax(1, newBlockSize);

        auto chain = buildChain(
            effects,
            sampleRate,
            blockSize);

        if (publishChain(std::move(chain)))
        {
            changed = true;
        }
        else
        {
            // Preserve the previous configuration if publication
            // cannot be completed.
            sampleRate = oldSampleRate;
            blockSize = oldBlockSize;
        }
    }

    if (changed)
        notifyChanged();
}

// ============================================================
// AUDIO PROCESSING
// ============================================================

void Rack::process(
    juce::AudioBuffer<float>& buffer) noexcept
{
    // No SpinLock, heap allocation, or object destruction here.

    // First, finish any deferred ownership transfer.
    if (deferredRetiredChain != nullptr)
    {
        if (!tryPushRetired(deferredRetiredChain))
        {
            if (activeChain != nullptr)
                activeChain->process(buffer);

            return;
        }

        deferredRetiredChain = nullptr;

        activeChain = deferredNewChain;
        deferredNewChain = nullptr;
    }

    Command command;

    while (tryPopCommand(command))
    {
        switch (command.type)
        {
            case CommandType::replaceChain:
            {
                AudioChain* replacement = command.chain;

                if (replacement == nullptr)
                    break;

                if (activeChain == nullptr)
                {
                    activeChain = replacement;
                    break;
                }

                if (tryPushRetired(activeChain))
                {
                    activeChain = replacement;
                }
                else
                {
                    // Keep both objects alive until the old chain
                    // can be returned to the control thread.
                    deferredRetiredChain = activeChain;
                    deferredNewChain = replacement;

                    if (activeChain != nullptr)
                        activeChain->process(buffer);

                    return;
                }

                break;
            }

            case CommandType::setEnabled:
            {
                if (activeChain != nullptr)
                {
                    activeChain->setEnabled(
                        command.effectIndex,
                        command.enabled);
                }

                break;
            }

            case CommandType::setMix:
            {
                if (activeChain != nullptr)
                {
                    activeChain->setMix(
                        command.effectIndex,
                        command.value);
                }

                break;
            }

            case CommandType::setParameter:
            {
                if (activeChain != nullptr)
                {
                    activeChain->setParameter(
                        command.effectIndex,
                        command.parameterIndex,
                        command.value);
                }

                break;
            }
        }
    }

    if (activeChain != nullptr)
        activeChain->process(buffer);
}

// ============================================================
// EFFECT INFORMATION
// ============================================================

int Rack::size() const
{
    RackLock lock(rackLock);
    return static_cast<int>(effects.size());
}

bool Rack::getEffectInfo(
    int index,
    EffectInfo& result) const
{
    RackLock lock(rackLock);

    if (index < 0 ||
        index >= static_cast<int>(effects.size()))
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
        result.parameterNames.push_back(
            effect->getParamName(i));

        result.parameterValues.push_back(
            effect->getParam(i));
    }

    return true;
}

// ============================================================
// PARAMETER COMMANDS
// ============================================================

bool Rack::setEffectEnabled(int index, bool enabled)
{
    drainRetired();

    RackLock lock(rackLock);

    if (index < 0 ||
        index >= static_cast<int>(effects.size()))
        return false;

    auto& effect = effects[static_cast<size_t>(index)];

    if (effect == nullptr)
        return false;

    Command command;
    command.type = CommandType::setEnabled;
    command.effectIndex = index;
    command.enabled = enabled;

    {
        RackLock queueLock(commandWriteLock);

        if (!tryPushCommand(command))
            return false;
    }

    effect->setEnabled(enabled);
    return true;
}

bool Rack::setEffectMix(int index, float mix)
{
    drainRetired();

    if (!std::isfinite(mix))
        return false;

    mix = juce::jlimit(0.0f, 1.0f, mix);

    RackLock lock(rackLock);

    if (index < 0 ||
        index >= static_cast<int>(effects.size()))
        return false;

    auto& effect = effects[static_cast<size_t>(index)];

    if (effect == nullptr)
        return false;

    Command command;
    command.type = CommandType::setMix;
    command.effectIndex = index;
    command.value = mix;

    {
        RackLock queueLock(commandWriteLock);

        if (!tryPushCommand(command))
            return false;
    }

    effect->setMix(mix);
    return true;
}

bool Rack::setEffectParameter(
    int index,
    int parameter,
    float value)
{
    drainRetired();

    if (!std::isfinite(value))
        return false;

    value = juce::jlimit(0.0f, 1.0f, value);

    RackLock lock(rackLock);

    if (index < 0 ||
        index >= static_cast<int>(effects.size()))
        return false;

    auto& effect = effects[static_cast<size_t>(index)];

    if (effect == nullptr)
        return false;

    if (parameter < 0 ||
        parameter >= effect->getNumParams())
        return false;

    Command command;
    command.type = CommandType::setParameter;
    command.effectIndex = index;
    command.parameterIndex = parameter;
    command.value = value;

    {
        RackLock queueLock(commandWriteLock);

        if (!tryPushCommand(command))
            return false;
    }

    effect->setParam(parameter, value);
    return true;
}

// ============================================================
// ADD EFFECT
// ============================================================

bool Rack::addEffect(const juce::String& typeID)
{
    drainRetired();

    bool added = false;

    {
        RackLock lock(rackLock);

        auto effect = create(typeID);

        if (effect == nullptr)
            return false;

        effects.push_back(std::move(effect));

        if (publishCurrentChainLocked())
        {
            added = true;
        }
        else
        {
            effects.pop_back();
        }
    }

    if (added)
        notifyChanged();

    return added;
}

// ============================================================
// REMOVE EFFECT
// ============================================================

void Rack::removeEffect(int index)
{
    drainRetired();

    bool changed = false;

    {
        RackLock lock(rackLock);

        if (index < 0 ||
            index >= static_cast<int>(effects.size()))
            return;

        auto removed = std::move(
            effects[static_cast<size_t>(index)]);

        effects.erase(effects.begin() + index);

        if (publishCurrentChainLocked())
        {
            changed = true;
        }
        else
        {
            effects.insert(
                effects.begin() + index,
                std::move(removed));
        }
    }

    if (changed)
        notifyChanged();
}

// ============================================================
// MOVE EFFECT
// ============================================================

void Rack::moveEffect(int from, int to)
{
    drainRetired();

    bool changed = false;

    {
        RackLock lock(rackLock);

        const int count = static_cast<int>(effects.size());

        if (from < 0 || from >= count ||
            to < 0 || to >= count ||
            from == to)
            return;

        auto effect = std::move(
            effects[static_cast<size_t>(from)]);

        effects.erase(effects.begin() + from);

        effects.insert(
            effects.begin() + to,
            std::move(effect));

        if (publishCurrentChainLocked())
        {
            changed = true;
        }
        else
        {
            auto rollback = std::move(
                effects[static_cast<size_t>(to)]);

            effects.erase(effects.begin() + to);

            effects.insert(
                effects.begin() + from,
                std::move(rollback));
        }
    }

    if (changed)
        notifyChanged();
}

// ============================================================
// CLEAR RACK
// ============================================================

void Rack::clearAll()
{
    drainRetired();

    bool changed = false;

    {
        RackLock lock(rackLock);

        std::vector<std::unique_ptr<EffectBase>> oldEffects;
        oldEffects.swap(effects);

        if (publishCurrentChainLocked())
        {
            changed = true;
        }
        else
        {
            effects.swap(oldEffects);
        }
    }

    if (changed)
        notifyChanged();
}

// ============================================================
// AVAILABLE EFFECTS
// ============================================================

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

// ============================================================
// SAVE STATE
// ============================================================

juce::ValueTree Rack::saveState() const
{
    RackLock lock(rackLock);

    juce::ValueTree tree("RACK");

    for (const auto& effect : effects)
    {
        if (effect != nullptr)
        {
            tree.appendChild(
                effect->saveState(),
                nullptr);
        }
    }

    return tree;
}

// ============================================================
// LOAD STATE
// ============================================================

void Rack::loadState(const juce::ValueTree& tree)
{
    if (!tree.isValid() || !tree.hasType("RACK"))
        return;

    drainRetired();

    std::vector<std::unique_ptr<EffectBase>> newEffects;

    newEffects.reserve(
        static_cast<size_t>(tree.getNumChildren()));

    for (int i = 0; i < tree.getNumChildren(); ++i)
    {
        const auto child = tree.getChild(i);

        auto effect = create(child.getType().toString());

        if (effect == nullptr)
            continue;

        effect->loadState(child);

        newEffects.push_back(std::move(effect));
    }

    bool changed = false;

    {
        RackLock lock(rackLock);

        effects.swap(newEffects);

        if (publishCurrentChainLocked())
        {
            changed = true;
        }
        else
        {
            effects.swap(newEffects);
        }
    }

    if (changed)
        notifyChanged();
}
