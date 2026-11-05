#pragma once

// A one-object mailbox from a producer thread (the kit service) to the audio thread. The producer
// leaves the newest object; the audio thread takes it when it is ready to adopt it. Latest wins: an
// object the audio thread never took comes back from the producer's next publish (), and the
// producer frees it. Wait-free; it never allocates and never frees anything.

#include <atomic>

namespace wubansia::engine
{
    template <class T>
    class PendingSlot
    {
    public:
        // Producer. Leaves next (nullptr withdraws the pending object) and returns the object it
        // replaces if the consumer never took it, else nullptr. What comes back was never seen by the
        // consumer, so the producer may free it at once.
        const T* publish (const T* next) noexcept { return slot.exchange (next, std::memory_order_acq_rel); }

        // Consumer (the audio thread). The pending object, or nullptr; the slot is empty afterwards.
        const T* take() noexcept { return slot.exchange (nullptr, std::memory_order_acq_rel); }

        // Whether an object is waiting. A snapshot: the other thread may change it at once.
        bool pending() const noexcept { return slot.load (std::memory_order_acquire) != nullptr; }

    private:
        std::atomic<const T*> slot { nullptr };

        static_assert (std::atomic<const T*>::is_always_lock_free);
    };
}
