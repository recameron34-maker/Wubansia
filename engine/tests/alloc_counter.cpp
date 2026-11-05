// engine_tests' allocation and free counter (alloc_counter.h says what it sees where). The replacements
// below are global: they serve the whole test binary, and count only on a thread inside a Scope.

#include "alloc_counter.h"

#include <cerrno>
#include <cstdlib>
#include <new>

#if defined(_WIN32)
    #include <malloc.h> // _aligned_malloc, _aligned_free
#endif

namespace
{
    thread_local bool counting = false;
    thread_local int numAllocations = 0;
    thread_local int numFrees = 0;

    void countAllocation() noexcept
    {
        if (counting)
            ++numAllocations;
    }

    void countFree() noexcept
    {
        if (counting)
            ++numFrees;
    }
}

bool allocations::available() noexcept
{
    return WUBANSIA_ALLOC_COUNTER != 0;
}

void allocations::start() noexcept
{
    numAllocations = 0;
    numFrees = 0;
    counting = true;
}

allocations::Counts allocations::stop() noexcept
{
    counting = false;
    return { numAllocations, numFrees };
}

#if WUBANSIA_ALLOC_COUNTER

    #if WUBANSIA_COUNTS_MALLOC
// glibc lets an executable replace its allocation functions; these forward to glibc's own.
extern "C"
{
    void* __libc_malloc (std::size_t) noexcept;
    void* __libc_calloc (std::size_t, std::size_t) noexcept;
    void* __libc_realloc (void*, std::size_t) noexcept;
    void* __libc_memalign (std::size_t, std::size_t) noexcept;
    void __libc_free (void*) noexcept;
    void* memalign (std::size_t alignment, std::size_t size) noexcept;

    void* malloc (std::size_t size) noexcept
    {
        countAllocation();
        return __libc_malloc (size);
    }

    void* calloc (std::size_t count, std::size_t size) noexcept
    {
        countAllocation();
        return __libc_calloc (count, size);
    }

    void* realloc (void* p, std::size_t size) noexcept
    {
        countAllocation();
        if (p != nullptr)
            countFree();
        return __libc_realloc (p, size);
    }

    void* memalign (std::size_t alignment, std::size_t size) noexcept
    {
        countAllocation();
        return __libc_memalign (alignment, size);
    }

    void* aligned_alloc (std::size_t alignment, std::size_t size) noexcept
    {
        countAllocation();
        return __libc_memalign (alignment, size);
    }

    int posix_memalign (void** out, std::size_t alignment, std::size_t size) noexcept
    {
        if (alignment < sizeof (void*) || (alignment & (alignment - 1)) != 0)
            return EINVAL;
        countAllocation();
        void* p = __libc_memalign (alignment, size);
        if (p == nullptr)
            return ENOMEM;
        *out = p;
        return 0;
    }

    void free (void* p) noexcept
    {
        if (p != nullptr)
            countFree();
        __libc_free (p);
    }
}
    #endif

namespace
{
    // Where malloc and free are counted (glibc), new and delete only forward to them.
    void* allocate (std::size_t size)
    {
    #if ! WUBANSIA_COUNTS_MALLOC
        countAllocation();
    #endif
        if (void* p = std::malloc (size == 0 ? 1 : size))
            return p;
        throw std::bad_alloc();
    }

    void* allocateAligned (std::size_t size, std::align_val_t alignment)
    {
    #if ! WUBANSIA_COUNTS_MALLOC
        countAllocation();
    #endif
        const auto a = static_cast<std::size_t> (alignment);
        const auto rounded = size == 0 ? a : (size + a - 1) / a * a; // aligned_alloc takes whole multiples
    #if defined(_WIN32)
        void* p = _aligned_malloc (rounded, a);
    #else
        void* p = std::aligned_alloc (a, rounded);
    #endif
        if (p != nullptr)
            return p;
        throw std::bad_alloc();
    }

    template <class Fn>
    void* allocateNoThrow (Fn&& fn) noexcept
    {
        try
        {
            return fn();
        }
        catch (...)
        {
            return nullptr;
        }
    }

    void release (void* p) noexcept
    {
    #if ! WUBANSIA_COUNTS_MALLOC
        if (p != nullptr)
            countFree();
    #endif
        std::free (p);
    }

    void releaseAligned (void* p) noexcept
    {
    #if ! WUBANSIA_COUNTS_MALLOC
        if (p != nullptr)
            countFree();
    #endif
    #if defined(_WIN32)
        _aligned_free (p);
    #else
        std::free (p);
    #endif
    }
}

void* operator new (std::size_t size) { return allocate (size); }
void* operator new[] (std::size_t size) { return allocate (size); }
void* operator new (std::size_t size, const std::nothrow_t&) noexcept { return allocateNoThrow ([size] { return allocate (size); }); }
void* operator new[] (std::size_t size, const std::nothrow_t&) noexcept { return allocateNoThrow ([size] { return allocate (size); }); }
void* operator new (std::size_t size, std::align_val_t a) { return allocateAligned (size, a); }
void* operator new[] (std::size_t size, std::align_val_t a) { return allocateAligned (size, a); }
void* operator new (std::size_t size, std::align_val_t a, const std::nothrow_t&) noexcept { return allocateNoThrow ([size, a] { return allocateAligned (size, a); }); }
void* operator new[] (std::size_t size, std::align_val_t a, const std::nothrow_t&) noexcept { return allocateNoThrow ([size, a] { return allocateAligned (size, a); }); }

void operator delete (void* p) noexcept { release (p); }
void operator delete[] (void* p) noexcept { release (p); }
void operator delete (void* p, const std::nothrow_t&) noexcept { release (p); }
void operator delete[] (void* p, const std::nothrow_t&) noexcept { release (p); }
void operator delete (void* p, std::align_val_t) noexcept { releaseAligned (p); }
void operator delete[] (void* p, std::align_val_t) noexcept { releaseAligned (p); }
void operator delete (void* p, std::align_val_t, const std::nothrow_t&) noexcept { releaseAligned (p); }
void operator delete[] (void* p, std::align_val_t, const std::nothrow_t&) noexcept { releaseAligned (p); }

    #if __cpp_sized_deallocation // on in GCC; in Clang with -fsized-deallocation (the default from Clang 19)
void operator delete (void* p, std::size_t) noexcept { release (p); }
void operator delete[] (void* p, std::size_t) noexcept { release (p); }
void operator delete (void* p, std::size_t, std::align_val_t) noexcept { releaseAligned (p); }
void operator delete[] (void* p, std::size_t, std::align_val_t) noexcept { releaseAligned (p); }
    #endif

#endif
