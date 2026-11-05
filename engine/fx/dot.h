#pragma once

// The FIRs' dot products (ADR 0021). Summed in four interleaved partial sums, joined in a fixed order: about four
// times the speed of one running sum (whose every add waits for the last), and the same bytes on every run and every
// IEEE platform, because the order is in the code, not left to the compiler (no fast math).

namespace wubansia::engine::fx
{
    // sum over m < n of taps[m] x[-m] (x points at the newest sample of a History).
    inline double dotBack (const double* taps, const double* x, int n) noexcept
    {
        double a0 = 0.0, a1 = 0.0, a2 = 0.0, a3 = 0.0;
        int m = 0;
        for (; m + 3 < n; m += 4)
        {
            a0 += taps[m] * x[-m];
            a1 += taps[m + 1] * x[-m - 1];
            a2 += taps[m + 2] * x[-m - 2];
            a3 += taps[m + 3] * x[-m - 3];
        }
        for (; m < n; ++m)
            a0 += taps[m] * x[-m];
        return (a0 + a1) + (a2 + a3);
    }

    // A symmetric FIR of 2 half + 1 taps: sum over k < half of taps[k] (x[-k] + x[-(2 half - k)]) + taps[half] x[-half].
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
}
