#pragma once

// The FIRs' dot products (ADR 0021). Summed in four interleaved partial sums, joined in a fixed order: about four
// times the speed of one running sum (whose every add waits for the last), and the same bytes on every run and every
// IEEE platform, because the order is in the code, not left to the compiler (no fast math).

#include <array>
#include <cstddef>

namespace wubansia::engine::fx
{
    // Whether a FIR's taps are symmetric (h[k] = h[N - 1 - k]), as the dot products below take them to be.
    template <std::size_t N>
    constexpr bool symmetricTaps (const std::array<double, N>& h) noexcept
    {
        for (std::size_t k = 0; k < N; ++k)
            if (h[k] < h[N - 1 - k] || h[N - 1 - k] < h[k])
                return false;
        return true;
    }

    // A symmetric FIR of 2 half + 1 taps, x pointing at the newest sample of a History:
    // sum over k < half of taps[k] (x[-k] + x[-(2 half - k)]) + taps[half] x[-half].
    inline double dotSymmetric (const double* taps, const double* x, int half) noexcept
    {
        const int last = 2 * half;
        double a0 = taps[half] * x[-half], a1 = 0.0, a2 = 0.0, a3 = 0.0;
        int k = 0;
        for (; k + 3 < half; k += 4)
        {
            a0 += taps[k] * (x[-k] + x[-(last - k)]);
            a1 += taps[k + 1] * (x[-k - 1] + x[-(last - k - 1)]);
            a2 += taps[k + 2] * (x[-k - 2] + x[-(last - k - 2)]);
            a3 += taps[k + 3] * (x[-k - 3] + x[-(last - k - 3)]);
        }
        for (; k < half; ++k)
            a0 += taps[k] * (x[-k] + x[-(last - k)]);
        return (a0 + a1) + (a2 + a3);
    }

    // A symmetric FIR of an even n taps (taps[j] = taps[n - 1 - j]; only the first n / 2 are read):
    // sum over j < n / 2 of taps[j] (x[-j] + x[-(n - 1 - j)]).
    inline double dotSymmetricEven (const double* taps, const double* x, int n) noexcept
    {
        const int half = n / 2, last = n - 1;
        double a0 = 0.0, a1 = 0.0, a2 = 0.0, a3 = 0.0;
        int j = 0;
        for (; j + 3 < half; j += 4)
        {
            a0 += taps[j] * (x[-j] + x[-(last - j)]);
            a1 += taps[j + 1] * (x[-j - 1] + x[-(last - j - 1)]);
            a2 += taps[j + 2] * (x[-j - 2] + x[-(last - j - 2)]);
            a3 += taps[j + 3] * (x[-j - 3] + x[-(last - j - 3)]);
        }
        for (; j < half; ++j)
            a0 += taps[j] * (x[-j] + x[-(last - j)]);
        return (a0 + a1) + (a2 + a3);
    }
}
