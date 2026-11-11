#pragma once

#include <cstddef> // defines __GLIBC__ on glibc systems

// Counts the heap allocations and frees made on the current thread while a Scope is alive.
// alloc_counter.cpp replaces the allocation functions in engine_tests only:
// - glibc (Linux): malloc, calloc, realloc, posix_memalign, aligned_alloc, memalign and free. Every
//   C++ new and delete reaches them, so everything is seen.
// - Elsewhere (macOS): every form of operator new and delete (plain, array, nothrow, sized, aligned).
//   malloc and free themselves aren't seen.
// - Sanitizer builds (TSan, ASan): nothing is replaced, because the sanitizer's runtime owns these
//   functions. available () is false, and the tests that need the counter skip.
// realloc counts as an allocation, and as a free too when it is given a block. free (nullptr) and
// delete of a null pointer count as nothing. Locks are never counted.

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

#if ! WUBANSIA_SANITIZED
    #define WUBANSIA_ALLOC_COUNTER 1
#else
    #define WUBANSIA_ALLOC_COUNTER 0
#endif

#if WUBANSIA_ALLOC_COUNTER && defined(__GLIBC__)
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

    // Whether this build counts at all (false in sanitizer builds).
    bool available() noexcept;

    void start() noexcept;
    Counts stop() noexcept;

    class Scope
    {
    public:
        Scope() noexcept { start(); }
        ~Scope() { stop(); }
        Scope (const Scope&) = delete;
        Scope& operator= (const Scope&) = delete;

        // Stops counting and returns what was counted since the Scope began.
        Counts counts() noexcept { return stop(); }
    };
}
