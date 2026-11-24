#pragma once

#include <JuceHeader.h>
#include "Effects.h"

#include <array>
#include <atomic>
#include <cstdint>
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
    ~Rack();

    Rack(const Rack&) = delete;
    Rack& operator=(const Rack&) = delete;

    void prepare(double sampleRate, int blockSize);

    // Processes mono or stereo buffers. Oversized buffers are split
    // into prepared-size chunks without allocating on the audio thread.
    void process(juce::AudioBuffer<float>& buffer) noexcept;

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
    static constexpr size_t queueCapacity = 256;
    static constexpr size_t queueMask = queueCapacity - 1;

    struct AudioChain;

    enum class CommandType : uint8_t
    {
        replaceChain,
        setEnabled,
        setMix,
        setParameter
    };

    struct Command
    {
        CommandType type = CommandType::replaceChain;
        AudioChain* chain = nullptr;

        int effectIndex = -1;
        int parameterIndex = -1;

        float value = 0.0f;
        bool enabled = true;
    };

    // --------------------------------------------------------
    // CONTROL THREAD STATE
    // --------------------------------------------------------

    mutable juce::SpinLock rackLock;

    // Control-side prototypes. The audio thread never processes
    // these objects directly.
    std::vector<std::unique_ptr<EffectBase>> effects;

    double sampleRate = 44100.0;
    int blockSize = 512;

    // --------------------------------------------------------
    // CONTROL -> AUDIO COMMAND QUEUE
    // --------------------------------------------------------

    std::array<Command, queueCapacity> commandQueue {};

    std::atomic<size_t> commandWritePosition { 0 };
    std::atomic<size_t> commandReadPosition { 0 };

    // Serializes all control-side producers.
    juce::SpinLock commandWriteLock;

    // --------------------------------------------------------
    // AUDIO -> CONTROL RETIREMENT QUEUE
    // --------------------------------------------------------

    std::array<AudioChain*, queueCapacity> retiredChains {};

    std::atomic<size_t> retiredWritePosition { 0 };
    std::atomic<size_t> retiredReadPosition { 0 };

    // Serializes control-side consumers.
    juce::SpinLock retiredReadLock;

    // --------------------------------------------------------
    // AUDIO THREAD OWNERSHIP
    // --------------------------------------------------------

    // Accessed only by the audio thread during processing.
    AudioChain* activeChain = nullptr;

    // Used if the retirement queue is temporarily full.
    AudioChain* deferredRetiredChain = nullptr;
    AudioChain* deferredNewChain = nullptr;

    // --------------------------------------------------------
    // GENERAL STATE
    // --------------------------------------------------------

    std::atomic<uint64_t> revision { 0 };

    std::unique_ptr<EffectBase> create(
        const juce::String& typeID) const;

    std::unique_ptr<AudioChain> buildChain(
        const std::vector<std::unique_ptr<EffectBase>>& source,
        double targetSampleRate,
        int targetBlockSize) const;

    bool publishChain(
        std::unique_ptr<AudioChain> chain);

    bool publishCurrentChainLocked();

    bool tryPushCommand(const Command& command) noexcept;
    bool tryPopCommand(Command& command) noexcept;

    bool tryPushRetired(AudioChain* chain) noexcept;
    bool tryPopRetired(AudioChain*& chain) noexcept;

    void drainRetired();

    void notifyChanged();
};