#pragma once

// Where the audio thread puts objects it has finished with (a Kit no voice plays any more), for the
// service thread to free. The audio thread never frees: it pushes the pointer here, and the service
// pops it and drops its own reference. The audio thread adopts a new object only while hasSpace (),
// so it always has room to retire the old one.

#include "rt/spsc_ring.h"

#include <cstddef>

namespace wubansia::engine
{
    template <class T, std::size_t N>
    class RetireFifo
    {
    public:
        // Audio thread. false when full (the object stays the caller's). nullptr is nothing to retire:
        // true, and nothing is stored.
        bool push (const T* object) noexcept { return object == nullptr || ring.push (object); }

        // Service thread. The oldest retired object, or nullptr when there is none.
        const T* pop() noexcept
        {
            const T* object = nullptr;
            return ring.pop (object) ? object : nullptr;
        }

        // Audio thread: whether push () would succeed now.
        bool hasSpace() const noexcept { return !ring.full(); }

        // Service thread: whether pop () would return nullptr now.
        bool empty() const noexcept { return ring.empty(); }

        static constexpr std::size_t capacity() noexcept { return N; }

    private:
        SpscRing<const T*, N> ring;
    };
}
