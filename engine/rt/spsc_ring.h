#pragma once

// A bounded single-producer, single-consumer ring of trivially copyable items: one thread pushes,
// one other thread pops. Wait-free, no allocation after construction, no locks. Holds exactly N
// items. The audio thread may be either end.

#include <array>
#include <atomic>
#include <cstddef>
#include <type_traits>

namespace wubansia::engine
{
    template <class T, std::size_t N>
    class SpscRing
    {
        static_assert (N > 0, "an SpscRing holds at least one item");
        static_assert (std::is_trivially_copyable_v<T>, "an SpscRing copies its items with plain assignment");

    public:
        // Producer only. false (and nothing stored) when the ring is full.
        bool push (const T& item) noexcept
        {
            const auto t = tail.load (std::memory_order_relaxed);
            if (t - head.load (std::memory_order_acquire) == N)
                return false;
            items[t % N] = item;
            tail.store (t + 1, std::memory_order_release);
            return true;
        }

        // Consumer only. false (and item untouched) when the ring is empty.
        bool pop (T& item) noexcept
        {
            const auto h = head.load (std::memory_order_relaxed);
            if (tail.load (std::memory_order_acquire) == h)
                return false;
            item = items[h % N];
            head.store (h + 1, std::memory_order_release);
            return true;
        }

        // Exact for the producer (full) and the consumer (empty); from anywhere else, a snapshot.
        bool full() const noexcept { return size() == N; }
        bool empty() const noexcept { return size() == 0; }
        std::size_t size() const noexcept { return tail.load (std::memory_order_acquire) - head.load (std::memory_order_acquire); }

        static constexpr std::size_t capacity() noexcept { return N; }

    private:
        std::array<T, N> items {};
        alignas (64) std::atomic<std::size_t> head { 0 }; // the next item to pop; only the consumer writes it
        alignas (64) std::atomic<std::size_t> tail { 0 }; // the next slot to fill; only the producer writes it

        static_assert (std::atomic<std::size_t>::is_always_lock_free);
    };
}
