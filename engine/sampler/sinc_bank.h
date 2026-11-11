#pragma once

// The polyphase windowed sinc the voices resample with: 16 taps, 256 phases (with linear interpolation
// between neighbouring phases), a Kaiser window (beta 9), and a cutoff that follows the voice's ratio
// (frames read per output sample): one bank of phases per semitone of ratio, from 1 (cutoff at the
// files' Nyquist, 24 kHz) up to 8 (three octaves up). A voice reading faster than 1 takes the bank whose
// cutoff is at or just under 0.5 / ratio of the file's rate, so nothing folds back under the host's
// Nyquist. At ratio 1 or below the first bank's phase 0 is exactly a copy.
//
// The banks don't depend on the host's rate: they are built once per process (instance (), the first
// call allocates; the sampler's prepare () makes it) and shared, read-only, by every voice.
//
// Read at a position p = i + f (i = floor (p)): out = sum over j of c_j (f) x[i - 7 + j], j = 0..15.

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

namespace wubansia::engine
{
    class SincBank
    {
    public:
        static constexpr int kTaps = 16;
        static constexpr int kPhases = 256;
        static constexpr int kRows = kPhases + 1; // phase 256 too, for the interpolation
        static constexpr int kBanks = 37; // ratios 2 ^ (b / 12), b = 0..36
        static constexpr int kLeft = 7; // taps before i
        static constexpr double kBeta = 9.0;

        // The process's bank, built on the first call (which allocates about 600 KB).
        static const SincBank& instance();

        // The bank for a ratio: the first whose ratio 2 ^ (b / 12) is at or above it (ratios at or under 1
        // take bank 0; over 8, the last).
        int bankFor (double ratio) const noexcept
        {
            int b = 0;
            while (b + 1 < kBanks && ratio > upper[static_cast<std::size_t> (b)])
                ++b;
            return b;
        }

        // From a bank already chosen, the bank for a new ratio (a glide moves a semitone at a time).
        int follow (int bank, double ratio) const noexcept
        {
            while (bank + 1 < kBanks && ratio > upper[static_cast<std::size_t> (bank)])
                ++bank;
            while (bank > 0 && ratio <= upper[static_cast<std::size_t> (bank - 1)])
                --bank;
            return bank;
        }

        // Bank b's rows: kRows x kTaps floats, row p for the fraction p / kPhases.
        const float* table (int bank) const noexcept
        {
            return rows.data() + static_cast<std::size_t> (bank) * kRows * kTaps;
        }

        // The cutoff of bank b, as a fraction of the file's rate.
        static double cutoff (int bank) noexcept { return 0.5 / std::exp2 (bank / 12.0); }

    private:
        SincBank();

        std::vector<float> rows;
        std::array<double, kBanks> upper {}; // bank b serves ratios up to upper[b] = 2 ^ (b / 12)
    };

    // Four floats side by side, where the compiler has them (GCC, Clang, clang-cl): the same IEEE operations
    // in the same order as the scalar code below, four at a time, so either gives the same bits.
#if defined(__GNUC__) || defined(__clang__)
    #define WUBANSIA_SINC_FLOAT4 1
    typedef float SincFloat4 __attribute__ ((vector_size (16)));

    inline SincFloat4 sincLoad4 (const float* p) noexcept
    {
        SincFloat4 v;
        std::memcpy (&v, p, sizeof v);
        return v;
    }
#else
    #define WUBANSIA_SINC_FLOAT4 0
#endif

    // The 16 coefficients for the fraction frac in [0, 1), interpolated between its two phases:
    // c_j = r0_j + t (r1_j - r0_j).
    inline void sincCoefficients (const float* table, double frac, float* c) noexcept
    {
        const double pf = frac * SincBank::kPhases;
        int p = static_cast<int> (pf);
        if (p >= SincBank::kPhases)
            p = SincBank::kPhases - 1;
        const auto t = static_cast<float> (pf - p);
        const float* r0 = table + p * SincBank::kTaps;
        const float* r1 = r0 + SincBank::kTaps;
#if WUBANSIA_SINC_FLOAT4
        const SincFloat4 tt = SincFloat4 {} + t;
        for (int j = 0; j < SincBank::kTaps; j += 4)
        {
            const SincFloat4 a = sincLoad4 (r0 + j);
            const SincFloat4 v = a + tt * (sincLoad4 (r1 + j) - a);
            std::memcpy (c + j, &v, sizeof v);
        }
#else
        for (int j = 0; j < SincBank::kTaps; ++j)
            c[j] = r0[j] + t * (r1[j] - r0[j]);
#endif
    }

    // sum c_j x_j over the 16 taps in a fixed order: four running sums, sum k over taps k, k + 4, k + 8,
    // k + 12, then (s0 + s1) + (s2 + s3).
    inline float sincDot (const float* c, const float* x) noexcept
    {
#if WUBANSIA_SINC_FLOAT4
        SincFloat4 a = {};
        for (int j = 0; j < SincBank::kTaps; j += 4)
            a += sincLoad4 (c + j) * sincLoad4 (x + j);
        return (a[0] + a[1]) + (a[2] + a[3]);
#else
        float a0 = 0.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
        for (int j = 0; j < SincBank::kTaps; j += 4)
        {
            a0 += c[j] * x[j];
            a1 += c[j + 1] * x[j + 1];
            a2 += c[j + 2] * x[j + 2];
            a3 += c[j + 3] * x[j + 3];
        }
        return (a0 + a1) + (a2 + a3);
#endif
    }

    // One read of a plain signal (silence outside [0, frames)) at pos >= 0, through a bank: what a voice
    // does for a file it plays straight through. For tests and offline use.
    inline float sincRead (const float* table, const float* data, std::int64_t frames, double pos) noexcept
    {
        const auto i = static_cast<std::int64_t> (pos);
        float c[SincBank::kTaps];
        sincCoefficients (table, pos - static_cast<double> (i), c);
        const std::int64_t first = i - SincBank::kLeft;
        if (first >= 0 && first + SincBank::kTaps <= frames)
            return sincDot (c, data + first);
        float x[SincBank::kTaps];
        for (int j = 0; j < SincBank::kTaps; ++j)
        {
            const std::int64_t k = first + j;
            x[j] = k >= 0 && k < frames ? data[k] : 0.0f;
        }
        return sincDot (c, x);
    }
}
