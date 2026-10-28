#include "helpers/allocation_counter.h"

#include <cstdlib>
#include <new>

namespace
{
    thread_local bool counting = false;
    thread_local int numAllocations = 0;

    void* allocate (std::size_t size)
    {
        if (counting)
            ++numAllocations;

        if (auto* p = std::malloc (size == 0 ? 1 : size))
            return p;

        throw std::bad_alloc();
    }
}

void allocations::start() noexcept
{
    numAllocations = 0;
    counting = true;
}

int allocations::stop() noexcept
{
    counting = false;
    return numAllocations;
}

void* operator new (std::size_t size) { return allocate (size); }
void* operator new[] (std::size_t size) { return allocate (size); }
void operator delete (void* p) noexcept { std::free (p); }
void operator delete[] (void* p) noexcept { std::free (p); }

#if __cpp_sized_deallocation // on by default in GCC; Clang only with -fsized-deallocation (default from Clang 19)
void operator delete (void* p, std::size_t) noexcept { std::free (p); }
void operator delete[] (void* p, std::size_t) noexcept { std::free (p); }
#endif
