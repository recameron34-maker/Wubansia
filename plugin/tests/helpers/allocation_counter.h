#pragma once

#include <cstddef> // defines __GLIBC__ on glibc systems

// Counts heap allocations made on the current thread while a Scope is alive. The replacement
// allocation functions live in plugin/tests/AllocationCounter.cpp (test binary only).
// - glibc (Linux): malloc, calloc and realloc are counted, so JUCE's own containers
//   (HeapBlock, Array, MidiBuffer, long MidiMessages) are seen as well as operator new.
// - Elsewhere: only operator new and new[] (plain and nothrow). std::malloc isn't seen.
// Over-aligned new (std::align_val_t) and locks are never counted; the realtime sanitizer
// arrives in M3.
#if defined(__GLIBC__)
    #define WUBANSIA_COUNTS_MALLOC 1
#else
    #define WUBANSIA_COUNTS_MALLOC 0
#endif

namespace allocations
{
    void start() noexcept;
    int stop() noexcept;

    struct Scope
    {
        Scope() noexcept { start(); }
        ~Scope() { stop(); }
        int count() noexcept { return stop(); }
    };
}
