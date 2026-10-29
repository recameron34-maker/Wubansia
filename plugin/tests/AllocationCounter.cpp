#include "helpers/allocation_counter.h"

#include <cstdlib>
#include <new>

namespace
{
    thread_local bool counting = false;
    thread_local int numAllocations = 0;

    void countOne() noexcept
    {
        if (counting)
            ++numAllocations;
    }

    void* allocate (std::size_t size)
    {
#if ! WUBANSIA_COUNTS_MALLOC
        countOne(); // with glibc, the malloc below counts it
#endif
        if (auto* p = std::malloc (size == 0 ? 1 : size))
            return p;

        throw std::bad_alloc();
    }

    void* allocateNoThrow (std::size_t size) noexcept
    {
        try
        {
            return allocate (size);
        }
        catch (...)
        {
            return nullptr;
        }
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

#if WUBANSIA_COUNTS_MALLOC
// glibc lets an executable replace malloc. JUCE allocates through it, not through operator new.
extern "C"
{
    void* __libc_malloc (std::size_t) noexcept;
    void* __libc_calloc (std::size_t, std::size_t) noexcept;
    void* __libc_realloc (void*, std::size_t) noexcept;

    void* malloc (std::size_t size) noexcept
    {
        countOne();
        return __libc_malloc (size);
    }

    void* calloc (std::size_t count, std::size_t size) noexcept
    {
        countOne();
        return __libc_calloc (count, size);
    }

    void* realloc (void* p, std::size_t size) noexcept
    {
        countOne();
        return __libc_realloc (p, size);
    }
}
#endif

void* operator new (std::size_t size) { return allocate (size); }
void* operator new[] (std::size_t size) { return allocate (size); }
void* operator new (std::size_t size, const std::nothrow_t&) noexcept { return allocateNoThrow (size); }
void* operator new[] (std::size_t size, const std::nothrow_t&) noexcept { return allocateNoThrow (size); }
void operator delete (void* p) noexcept { std::free (p); }
void operator delete[] (void* p) noexcept { std::free (p); }
void operator delete (void* p, const std::nothrow_t&) noexcept { std::free (p); }
void operator delete[] (void* p, const std::nothrow_t&) noexcept { std::free (p); }

#if __cpp_sized_deallocation // on by default in GCC; Clang only with -fsized-deallocation (default from Clang 19)
void operator delete (void* p, std::size_t) noexcept { std::free (p); }
void operator delete[] (void* p, std::size_t) noexcept { std::free (p); }
#endif
