// The real-time hand-off helpers (engine/rt) and the allocation counter that checks them.

#include "alloc_counter.h"

#include "rt/pending_slot.h"
#include "rt/retire_fifo.h"
#include "rt/spsc_ring.h"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <thread>
#include <vector>

using namespace wubansia::engine;

namespace
{
    // Where the counter tests park their pointers, so the optimiser can't drop the allocations.
    void* volatile sink = nullptr;

    struct alignas (64) OverAligned
    {
        char bytes[64];
    };
}

TEST_CASE ("the allocation counter counts", "[alloc]")
{
    // Without this, a counter that silently counts nothing would make every zero-allocation check pass.
    if (! allocations::available())
        SKIP ("a sanitizer owns the allocation functions in this build");

    SECTION ("new and delete")
    {
        allocations::Scope scope;
        auto* p = new int (1);
        sink = p;
        delete p;
        const auto c = scope.counts();
        CHECK (c.allocations == 1);
        CHECK (c.frees == 1);
    }

    SECTION ("new[] and delete[]")
    {
        allocations::Scope scope;
        auto* p = new int[16];
        sink = p;
        delete[] p;
        const auto c = scope.counts();
        CHECK (c.allocations == 1);
        CHECK (c.frees == 1);
    }

    SECTION ("over-aligned new and delete")
    {
        allocations::Scope scope;
        auto* p = new OverAligned();
        sink = p;
        CHECK (reinterpret_cast<std::uintptr_t> (p) % 64 == 0);
        delete p;
        const auto c = scope.counts();
        CHECK (c.allocations == 1);
        CHECK (c.frees == 1);
    }

    SECTION ("nothrow new")
    {
        allocations::Scope scope;
        auto* p = new (std::nothrow) int (2);
        sink = p;
        delete p;
        const auto c = scope.counts();
        CHECK (c.allocations == 1);
        CHECK (c.frees == 1);
    }

    SECTION ("a null pointer deleted is no free")
    {
        int* p = nullptr;
        sink = &p;
        allocations::Scope scope;
        delete p;
        CHECK (scope.counts().none());
    }

#if WUBANSIA_COUNTS_MALLOC
    SECTION ("malloc, calloc, realloc and free")
    {
        allocations::Scope scope;
        void* a = std::malloc (16);
        sink = a;
        void* b = std::calloc (4, 8);
        sink = b;
        a = std::realloc (a, 4096); // an allocation, and a free of the old block
        sink = a;
        std::free (a);
        std::free (b);
        const auto c = scope.counts();
        CHECK (c.allocations == 3);
        CHECK (c.frees == 3);
    }

    SECTION ("posix_memalign and aligned_alloc")
    {
        allocations::Scope scope;
        void* a = nullptr;
        REQUIRE (posix_memalign (&a, 64, 100) == 0);
        sink = a;
        void* b = std::aligned_alloc (64, 128);
        sink = b;
        std::free (a);
        std::free (b);
        const auto c = scope.counts();
        CHECK (c.allocations == 2);
        CHECK (c.frees == 2);
    }

    SECTION ("free (nullptr) is no free")
    {
        void* volatile nothing = nullptr; // volatile: the compiler may drop a visible free (nullptr)
        allocations::Scope scope;
        std::free (nothing);
        CHECK (scope.counts().none());
    }
#endif

    SECTION ("nothing outside a scope, and only the counting thread")
    {
        delete new int (3); // before the scope

        std::atomic<int> step { 0 };
        std::thread other ([&step] {
            while (step.load() != 1)
                std::this_thread::yield();
            auto* p = new int (4); // on another thread, while this one counts
            sink = p;
            delete p;
            step.store (2);
        });

        {
            allocations::Scope scope;
            step.store (1);
            while (step.load() != 2)
                std::this_thread::yield();
            CHECK (scope.counts().none());
        }
        other.join();
    }
}

TEST_CASE ("PendingSlot hands back the object the audio thread never took", "[rt]")
{
    const int a = 1, b = 2, c = 3;
    PendingSlot<int> slot;

    CHECK_FALSE (slot.pending());
    CHECK (slot.take() == nullptr);

    CHECK (slot.publish (&a) == nullptr);
    CHECK (slot.pending());
    CHECK (slot.publish (&b) == &a); // a was never taken: it goes back to the producer to free

    CHECK (slot.take() == &b); // the audio thread adopts b
    CHECK_FALSE (slot.pending());
    CHECK (slot.take() == nullptr);

    CHECK (slot.publish (&c) == nullptr); // b was taken, so nothing comes back
    CHECK (slot.publish (nullptr) == &c); // withdrawn before the audio thread saw it
    CHECK_FALSE (slot.pending());
}

TEST_CASE ("RetireFifo keeps order, refuses when full and treats nullptr as nothing", "[rt]")
{
    const int objects[4] = { 10, 11, 12, 13 };
    RetireFifo<int, 3> fifo;

    CHECK (fifo.empty());
    CHECK (fifo.hasSpace());
    CHECK (fifo.pop() == nullptr);

    CHECK (fifo.push (nullptr)); // nothing to retire
    CHECK (fifo.empty());

    CHECK (fifo.push (&objects[0]));
    CHECK (fifo.push (&objects[1]));
    CHECK (fifo.push (&objects[2]));
    CHECK_FALSE (fifo.hasSpace());
    CHECK_FALSE (fifo.push (&objects[3])); // full: the caller keeps it
    CHECK (fifo.push (nullptr)); // still nothing to retire, full or not

    CHECK (fifo.pop() == &objects[0]);
    CHECK (fifo.hasSpace());
    CHECK (fifo.push (&objects[3]));
    CHECK (fifo.pop() == &objects[1]);
    CHECK (fifo.pop() == &objects[2]);
    CHECK (fifo.pop() == &objects[3]);
    CHECK (fifo.pop() == nullptr);
    CHECK (fifo.empty());
    STATIC_CHECK (RetireFifo<int, 16>::capacity() == 16);
}

TEST_CASE ("SpscRing holds exactly N items and wraps", "[rt]")
{
    SpscRing<std::uint32_t, 4> ring;
    STATIC_CHECK (SpscRing<std::uint32_t, 4>::capacity() == 4);

    std::uint32_t value = 99;
    CHECK_FALSE (ring.pop (value));
    CHECK (value == 99);

    // Many times round, so the indices wrap the storage over and over.
    std::uint32_t next = 0, expected = 0;
    for (int round = 0; round < 100; ++round)
    {
        while (ring.push (next))
            ++next;
        CHECK (ring.full());
        CHECK (ring.size() == 4);
        for (int i = 0; i < 3; ++i)
        {
            REQUIRE (ring.pop (value));
            CHECK (value == expected++);
        }
    }
    while (ring.pop (value))
        CHECK (value == expected++);
    CHECK (ring.empty());
    CHECK (expected == next);
}

TEST_CASE ("RetireFifo moves every object between two threads exactly once, in order", "[rt][threads]")
{
    // The audio thread retires; the service thread collects. Run it under TSan too (engine_tests in a
    // -fsanitize=thread build).
    constexpr int count = 200000;
    std::vector<int> objects (count);
    RetireFifo<int, 16> fifo;

    std::vector<const int*> received;
    received.reserve (count);
    std::thread service ([&] {
        while (static_cast<int> (received.size()) < count)
        {
            if (const int* p = fifo.pop())
                received.push_back (p);
            else
                std::this_thread::yield();
        }
    });

    int refusals = 0;
    for (int i = 0; i < count; ++i)
    {
        while (! fifo.hasSpace())
            std::this_thread::yield();
        if (! fifo.push (&objects[static_cast<std::size_t> (i)]))
            ++refusals; // hasSpace () promised room: only this thread fills it
    }
    service.join();

    CHECK (refusals == 0);
    REQUIRE (received.size() == objects.size());
    bool inOrder = true;
    for (std::size_t i = 0; i < received.size(); ++i)
        inOrder = inOrder && received[i] == &objects[i];
    CHECK (inOrder);
    CHECK (fifo.empty());
}

TEST_CASE ("the kit hand-off loses no object and hands none back twice", "[rt][threads]")
{
    // The service publishes objects as fast as it can; the audio thread adopts the pending one at each
    // "block" while the retire FIFO has room, and retires the one it replaces. Every object must come
    // back to the service exactly once (unadopted from publish, or retired), except the one still
    // adopted at the end.
    constexpr int count = 50000;
    std::vector<int> objects (count);
    PendingSlot<int> pending;
    RetireFifo<int, 16> retired;
    std::atomic<bool> serviceDone { false };
    std::atomic<bool> audioDone { false };

    std::vector<int> returned (count, 0);
    auto giveBack = [&] (const int* p) {
        if (p != nullptr)
            ++returned[static_cast<std::size_t> (p - objects.data())];
    };

    const int* adopted = nullptr; // the audio thread's
    std::thread audio ([&] {
        for (;;)
        {
            const bool finishing = serviceDone.load();
            if (retired.hasSpace())
            {
                if (const int* next = pending.take())
                {
                    retired.push (adopted);
                    adopted = next;
                }
            }
            if (finishing && ! pending.pending())
            {
                audioDone.store (true);
                return;
            }
            std::this_thread::yield();
        }
    });

    for (int i = 0; i < count; ++i)
    {
        giveBack (pending.publish (&objects[static_cast<std::size_t> (i)]));
        while (const int* p = retired.pop())
            giveBack (p);
    }
    serviceDone.store (true);
    while (! audioDone.load()) // keep draining, or a full FIFO would stop the last adoption
    {
        while (const int* p = retired.pop())
            giveBack (p);
        std::this_thread::yield();
    }
    audio.join();
    while (const int* p = retired.pop())
        giveBack (p);

    REQUIRE (adopted != nullptr);
    int lost = 0, twice = 0;
    for (int i = 0; i < count; ++i)
    {
        const int expected = &objects[static_cast<std::size_t> (i)] == adopted ? 0 : 1;
        const int got = returned[static_cast<std::size_t> (i)];
        if (got < expected)
            ++lost;
        if (got > expected)
            ++twice;
    }
    CHECK (lost == 0);
    CHECK (twice == 0);
}

TEST_CASE ("PendingSlot, RetireFifo and SpscRing never allocate or free", "[rt][alloc]")
{
    if (! allocations::available())
        SKIP ("a sanitizer owns the allocation functions in this build");

    const int a = 1, b = 2;
    PendingSlot<int> slot;
    RetireFifo<int, 4> fifo;
    SpscRing<std::uint64_t, 8> ring;

    allocations::Scope scope;
    sink = const_cast<int*> (slot.publish (&a));
    sink = const_cast<int*> (slot.publish (&b));
    sink = const_cast<int*> (slot.take());
    fifo.push (&a);
    fifo.push (&b);
    sink = const_cast<int*> (fifo.pop());
    sink = const_cast<int*> (fifo.pop());
    std::uint64_t value = 0;
    for (std::uint64_t i = 0; i < 20; ++i)
        ring.push (i);
    while (ring.pop (value))
        sink = &value;
    CHECK (scope.counts().none());
}
