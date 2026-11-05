#pragma once

// Flush-to-zero while the mix runs (ADR 0021): long reverb and filter tails would otherwise decay into subnormal
// numbers, slow on x86 and pointless in audio. The plugin's processBlock already sets this mode (ScopedNoDenormals);
// groovecore render and the tests don't, so the mixer sets it itself and puts it back. Normal numbers are untouched,
// so results don't change.

#include <cstdint>

#if defined(__SSE2__) || defined(_M_X64) || defined(__x86_64__)
    #include <xmmintrin.h>
    #define WUBANSIA_FTZ_SSE 1
#elif defined(__aarch64__) || defined(_M_ARM64)
    #define WUBANSIA_FTZ_ARM64 1
#endif

namespace wubansia::engine::fx
{
    class ScopedFlushDenormals
    {
    public:
        ScopedFlushDenormals() noexcept
        {
#if defined(WUBANSIA_FTZ_SSE)
            const unsigned int csr = _mm_getcsr();
            saved = csr;
            _mm_setcsr (csr | 0x8040u); // FTZ (bit 15) and DAZ (bit 6)
#elif defined(WUBANSIA_FTZ_ARM64) && (defined(__GNUC__) || defined(__clang__))
            std::uint64_t fpcr;
            asm volatile ("mrs %0, fpcr" : "=r"(fpcr));
            saved = fpcr;
            asm volatile ("msr fpcr, %0" : : "r"(fpcr | (std::uint64_t { 1 } << 24))); // FZ
#endif
        }

        ~ScopedFlushDenormals()
        {
#if defined(WUBANSIA_FTZ_SSE)
            _mm_setcsr (static_cast<unsigned int> (saved));
#elif defined(WUBANSIA_FTZ_ARM64) && (defined(__GNUC__) || defined(__clang__))
            asm volatile ("msr fpcr, %0" : : "r"(saved));
#endif
        }

        ScopedFlushDenormals (const ScopedFlushDenormals&) = delete;
        ScopedFlushDenormals& operator= (const ScopedFlushDenormals&) = delete;

    private:
        std::uint64_t saved = 0;
    };
}
