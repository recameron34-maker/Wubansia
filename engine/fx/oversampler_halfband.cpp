#include "fx/oversampler_halfband.h"

#include "fx/dot.h"
#include "fx/generated/halfband.h"

#include <array>
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
        constexpr int kHalf1 = (kTaps1 + 1) / 2; // 24 nonzero taps beside the centre, at the even places
        constexpr int kHalf2 = (kTaps2 + 1) / 2; // 8

        // A half-band's taps at its even places (the rest are 0 but the centre), times `gain`.
        template <std::size_t N, std::size_t M>
        constexpr std::array<double, M> evenTaps (const std::array<double, N>& h, double gain) noexcept
        {
            std::array<double, M> out {};
            for (std::size_t j = 0; j < M; ++j)
                out[j] = gain * h[2 * j];
            return out;
        }

        constexpr auto kUp1 = evenTaps<47, 24> (kHalfband1, 2.0);
        constexpr auto kUp2 = evenTaps<15, 8> (kHalfband2, 2.0);
        constexpr auto kDown1 = evenTaps<47, 24> (kHalfband1, 1.0);
        constexpr auto kDown2 = evenTaps<15, 8> (kHalfband2, 1.0);
    }

    // Stage 2 up: y2[m] in, y4[2m] = sum 2 h2[2j] y2[m - j], y4[2m + 1] = y2[m - 3] (the centre's).
    void HalfbandUpsampler4::up2 (double y, double* out) noexcept
    {
        stage2.push (y);
        const double* h = stage2.newest();
        out[0] = dotBack (kUp2.data(), h, kHalf2);
        out[1] = h[-((kCentre2 - 1) / 2)];
    }

    // Stage 1 up: x[n] in, y2[2n] = sum 2 h1[2j] x[n - j], y2[2n + 1] = x[n - 11].
    void HalfbandUpsampler4::push (double x, double* out) noexcept
    {
        stage1.push (x);
        const double* h = stage1.newest();
        const double filtered = dotBack (kUp1.data(), h, kHalf1);
        const double copy = h[-((kCentre1 - 1) / 2)];
        up2 (filtered, out);
        up2 (copy, out + 2);
    }

    // Stage 2 down: w[j] = sum h2[2i] z[2j - 2i] + 0.5 z[2j - 7], taken when z[2j] arrives. Stage 1 down:
    // v[n] = sum h1[2i] w[2n - 1 - 2i] + 0.5 w[2n - 24] (the one 2x sample of delay that puts it on the grid), taken
    // before this step's two w join.
    double HalfbandDownsampler4::push (const double* in) noexcept
    {
        const double v = dotBack (kDown1.data(), odds2.newest(), kHalf1) + 0.5 * evens2.newest()[-((kCentre1 + 1) / 2 - 1)];

        evens4.push (in[0]);
        const double w0 = dotBack (kDown2.data(), evens4.newest(), kHalf2) + 0.5 * odds4.newest()[-((kCentre2 - 1) / 2)];
        odds4.push (in[1]);
        evens4.push (in[2]);
        const double w1 = dotBack (kDown2.data(), evens4.newest(), kHalf2) + 0.5 * odds4.newest()[-((kCentre2 - 1) / 2)];
        odds4.push (in[3]);

        evens2.push (w0);
        odds2.push (w1);
        return v;
    }
}
