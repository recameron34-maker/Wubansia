#include "fx/oversampler_halfband.h"

#include "fx/generated/halfband.h"

#include <cstddef>

namespace wubansia::engine::fx
{
    namespace
    {
        constexpr int kTaps1 = static_cast<int> (kHalfband1.size()); // 47
        constexpr int kTaps2 = static_cast<int> (kHalfband2.size()); // 15
        constexpr int kCentre1 = (kTaps1 - 1) / 2; // 23
        constexpr int kCentre2 = (kTaps2 - 1) / 2; // 7
        static_assert (kCentre1 % 2 == 1 && kCentre2 % 2 == 1, "half-bands of 4k + 3 taps: the centre is odd");

        constexpr double tap1 (int k) noexcept { return kHalfband1[static_cast<std::size_t> (k)]; }
        constexpr double tap2 (int k) noexcept { return kHalfband2[static_cast<std::size_t> (k)]; }
    }

    // Stage 2 up: y2[m] in, y4[2m] = sum over even k of 2 h2[k] y2[m - k / 2], y4[2m + 1] = y2[m - 3].
    void HalfbandUpsampler4::up2 (double y, double* out) noexcept
    {
        stage2.push (y);
        const double* h = stage2.newest();
        double acc = 0.0;
        for (int k = 0; k < kTaps2; k += 2)
            acc += 2.0 * tap2 (k) * h[-(k / 2)];
        out[0] = acc;
        out[1] = h[-((kCentre2 - 1) / 2)];
    }

    // Stage 1 up: x[n] in, y2[2n] = sum over even k of 2 h1[k] x[n - k / 2], y2[2n + 1] = x[n - 11].
    void HalfbandUpsampler4::push (double x, double* out) noexcept
    {
        stage1.push (x);
        const double* h = stage1.newest();
        double acc = 0.0;
        for (int k = 0; k < kTaps1; k += 2)
            acc += 2.0 * tap1 (k) * h[-(k / 2)];
        const double copy = h[-((kCentre1 - 1) / 2)];
        up2 (acc, out);
        up2 (copy, out + 2);
    }

    // Stage 2 down: w[j] = sum h2[k] z[2j - k] (even k, and the centre), taken after z[2j] arrives; stage 1 down:
    // v[n] = sum h1[k] w[2n - 1 - k] (the one 2x sample of delay that puts it on the grid), taken before this
    // step's two w values join.
    double HalfbandDownsampler4::push (const double* in) noexcept
    {
        const double* w = stage1.newest();
        double v = 0.5 * w[-kCentre1];
        for (int k = 0; k < kTaps1; k += 2)
            v += tap1 (k) * w[-k];

        for (int half = 0; half < 2; ++half)
        {
            stage2.push (in[2 * half]);
            const double* z = stage2.newest();
            double acc = 0.5 * z[-kCentre2];
            for (int k = 0; k < kTaps2; k += 2)
                acc += tap2 (k) * z[-k];
            stage2.push (in[2 * half + 1]);
            stage1.push (acc);
        }
        return v;
    }
}
