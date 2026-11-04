#pragma once

// Signed 128-bit integers in two 64-bit halves (two's complement), just enough for the exact lag-1
// autocorrelation of G7 (measures.cpp): its numerator n^2 P passes 2^64 once a lane has some 30,000
// residuals, and the audit computes it exactly, with BigInt. Portable on purpose: no __int128 (MSVC
// has none, and clang-cl would need compiler-rt for 128-bit division), and no division at all:
// permilleHalfUp () finds its quotient by bisection.

#include <cstdint>

namespace wubansia::metrics::detail
{
    struct Int128
    {
        std::uint64_t hi = 0;
        std::uint64_t lo = 0;

        static constexpr Int128 from (std::int64_t v) noexcept
        {
            return { v < 0 ? ~std::uint64_t { 0 } : std::uint64_t { 0 }, static_cast<std::uint64_t> (v) };
        }

        constexpr bool negative() const noexcept { return (hi >> 63) != 0; }

        friend constexpr Int128 operator+ (Int128 a, Int128 b) noexcept
        {
            const std::uint64_t low = a.lo + b.lo;
            return { a.hi + b.hi + (low < a.lo ? 1u : 0u), low };
        }

        friend constexpr Int128 operator- (Int128 a, Int128 b) noexcept
        {
            const std::uint64_t low = a.lo - b.lo;
            return { a.hi - b.hi - (a.lo < b.lo ? 1u : 0u), low };
        }

        // The product modulo 2^128, which is the signed product whenever that fits.
        friend constexpr Int128 operator* (Int128 a, Int128 b) noexcept
        {
            // a.lo * b.lo in full, from 32-bit halves; the cross terms only reach the high half.
            constexpr std::uint64_t mask32 = 0xFFFFFFFFu;
            const std::uint64_t a0 = a.lo & mask32, a1 = a.lo >> 32, b0 = b.lo & mask32, b1 = b.lo >> 32;
            const std::uint64_t p00 = a0 * b0, p01 = a0 * b1, p10 = a1 * b0, p11 = a1 * b1;
            const std::uint64_t mid = (p00 >> 32) + (p01 & mask32) + (p10 & mask32);
            const std::uint64_t low = (mid << 32) | (p00 & mask32);
            const std::uint64_t high = p11 + (p01 >> 32) + (p10 >> 32) + (mid >> 32);
            return { high + a.hi * b.lo + a.lo * b.hi, low };
        }

        friend constexpr bool operator< (Int128 a, Int128 b) noexcept
        {
            if (a.negative() != b.negative())
                return a.negative();
            return a.hi != b.hi ? a.hi < b.hi : a.lo < b.lo;
        }

        friend constexpr bool operator== (Int128, Int128) noexcept = default;
    };

    // floor ((2000 num + den) / (2 den)): num / den in thousandths, rounded half up toward +infinity,
    // as the audit's permilleBig rounds. For 0 < den <= 2^115 and |num| <= den (an autocorrelation),
    // so the answer is in [-1000, 1000]; every product below stays under 2^127.
    constexpr std::int64_t permilleHalfUp (Int128 num, Int128 den) noexcept
    {
        const Int128 n = Int128::from (2000) * num + den;
        const Int128 d = Int128::from (2) * den;
        // The largest r in [-1000, 1000] with d * r <= n. d * -1000 <= n holds for |num| <= den.
        std::int64_t lo = -1000;
        std::int64_t hi = 1000;
        while (lo < hi) // at most 11 passes
        {
            const std::int64_t mid = lo + (hi - lo + 1) / 2;
            if (n < d * Int128::from (mid))
                hi = mid - 1;
            else
                lo = mid;
        }
        return lo;
    }
}
