#pragma once

#include <cstddef> // defines __GLIBC__ on glibc systems

// Counts heap allocations and frees made on the current thread while a Scope is alive. The replacement
// allocation functions live in plugin/tests/AllocationCounter.cpp (test binary only).
// - glibc (Linux): malloc, calloc, realloc, the aligned allocators and free are counted, so JUCE's own
//   containers (HeapBlock, Array, MidiBuffer, long MidiMessages) are seen as well as new and delete.
// - Elsewhere: every form of operator new and delete. std::malloc and std::free aren't seen.
// - Sanitizer builds: nothing is replaced (the sanitizer owns these functions); available () is false
//   and the tests that need the counter skip.
// realloc counts as an allocation, and as a free when it is given a block. Locks are never counted;
// the realtime sanitizer arrives in M3.
#if defined(__SANITIZE_THREAD__) || defined(__SANITIZE_ADDRESS__)
    #define WUBANSIA_SANITIZED 1
#elif defined(__has_feature)
    #if __has_feature(thread_sanitizer) || __has_feature(address_sanitizer) || __has_feature(memory_sanitizer)
        #define WUBANSIA_SANITIZED 1
    #endif
#endif
#ifndef WUBANSIA_SANITIZED
    #define WUBANSIA_SANITIZED 0
#endif

#if ! WUBANSIA_SANITIZED && defined(__GLIBC__)
    #define WUBANSIA_COUNTS_MALLOC 1
#else
    #define WUBANSIA_COUNTS_MALLOC 0
#endif

namespace allocations
{
    struct Counts
    {
        int allocations = 0;
        int frees = 0;

        bool none() const noexcept { return allocations == 0 && frees == 0; }
    };

    // Whether this build counts at all.
    bool available() noexcept;

    void start() noexcept;
    Counts stop() noexcept;

    struct Scope
    {
        Scope() noexcept { start(); }
        ~Scope() { stop(); }
        Scope (const Scope&) = delete;
        Scope& operator= (const Scope&) = delete;

        // Stops counting; the allocations, or both counts.
        int count() noexcept { return stop().allocations; }
        Counts counts() noexcept { return stop(); }
    };
}
