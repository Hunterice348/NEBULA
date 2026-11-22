#include "SpectrumFrameQueue.h"
#include <cassert>
#include <cstdint>
#include <thread>

int main()
{
    nebula::SpectrumFrameQueue queue;
    nebula::SpectrumFrame frame;
    for (std::uint64_t i = 1; i <= nebula::SpectrumFrameQueue::capacity; ++i)
    {
        frame.sequence = i;
        frame.bins[0] = (float) i;
        assert(queue.tryPush(frame));
    }
    frame.sequence = 99;
    assert(!queue.tryPush(frame)); // Full queue drops, does not overwrite.
    for (std::uint64_t expected = 1; expected <= nebula::SpectrumFrameQueue::capacity; ++expected)
    {
        assert(queue.tryPop(frame));
        assert(frame.sequence == expected);
        assert(frame.bins[0] == (float) expected);
    }
    assert(!queue.tryPop(frame));

    constexpr std::uint64_t count = 200000;
    std::atomic<bool> failed { false };
    std::thread producer([&]
    {
        nebula::SpectrumFrame outgoing;
        for (std::uint64_t n = 1; n <= count; ++n)
        {
            outgoing.sequence = n;
            outgoing.bins[0] = (float) n;
            while (!queue.tryPush(outgoing)) std::this_thread::yield();
        }
    });
    std::thread consumer([&]
    {
        nebula::SpectrumFrame incoming;
        std::uint64_t expected = 1;
        while (expected <= count)
        {
            if (!queue.tryPop(incoming)) { std::this_thread::yield(); continue; }
            if (incoming.sequence != expected || incoming.bins[0] != (float) expected)
                failed.store(true, std::memory_order_relaxed);
            ++expected;
        }
    });
    producer.join();
    consumer.join();
    assert(!failed.load(std::memory_order_relaxed));
    return 0;
}
