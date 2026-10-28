#pragma once

// Counts global operator new calls made on the current thread while a Scope is alive.
// The replacement operator new/delete live in plugin/tests/AllocationCounter.cpp (test binary only).
// Over-aligned new (std::align_val_t) is not counted.
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
