#pragma once

#include <array>
#include <atomic>
#include <cstdint>

namespace nebula
{
// One fixed-size spectrum record. The queue has exactly one audio producer
// and one UI consumer; each slot is exclusively owned until its index advances.
struct SpectrumFrame
{
    static constexpr std::size_t binCount = 512;
    std::array<float, binCount> bins {};
    std::uint64_t sequence = 0;
};

// Bounded SPSC FIFO for audio-to-UI telemetry. On full, tryPush drops the new
// frame; it never blocks and never overwrites a slot the consumer may read.
// This type is not safe for multiple editors/consumers or multiple producers.
class SpectrumFrameQueue
{
public:
    static constexpr std::uint32_t capacity = 4;
    static_assert((capacity & (capacity - 1)) == 0, "capacity must be a power of two");
    static_assert(std::atomic<std::uint32_t>::is_always_lock_free,
                  "spectrum queue requires lock-free 32-bit atomics");

    bool tryPush(const SpectrumFrame& frame) noexcept
    {
        const auto write = writeIndex.load(std::memory_order_relaxed);
        const auto read = readIndex.load(std::memory_order_acquire);
        if (static_cast<std::uint32_t>(write - read) >= capacity)
            return false;

        slots[write & (capacity - 1)] = frame;
        writeIndex.store(write + 1, std::memory_order_release);
        return true;
    }

    bool tryPop(SpectrumFrame& frame) noexcept
    {
        const auto read = readIndex.load(std::memory_order_relaxed);
        const auto write = writeIndex.load(std::memory_order_acquire);
        if (read == write)
            return false;

        frame = slots[read & (capacity - 1)];
        readIndex.store(read + 1, std::memory_order_release);
        return true;
    }

private:
    std::array<SpectrumFrame, capacity> slots {};
    alignas(64) std::atomic<std::uint32_t> writeIndex { 0 };
    alignas(64) std::atomic<std::uint32_t> readIndex { 0 };
};
} // namespace nebula
