#include "fx/oversampler_exact.h"

#include "fx/dot.h"
#include "fx/generated/os_fir4.h"

#include <cstddef>

namespace wubansia::engine::fx
{
    namespace
    {
        constexpr int kTaps = static_cast<int> (kOsFir4.size()); // 193
        constexpr int kCentre = (kTaps - 1) / 2; // 96
        static_assert (kTaps == 193, "the polyphase pairing below is os_fir(4)'s: 4 x 48 + 1 taps");

        // The up direction's phases, t_j[m] = 4 h[j + 4 m] (49 taps for j = 0, 48 for the rest). h is symmetric
        // (h[k] = h[192 - k]), so:
        // - t0[m] = t0[48 - m]: 24 pairs and a centre;
        // - t2[m] = t2[47 - m]: 24 pairs;
        // - t1[m] = t3[47 - m]: phases 1 and 3 mirror each other, so with a = x[-m], b = x[-(47 - m)] (m < 24),
        //   y1 = sum (s[m] (a + b) + d[m] (a - b)) and y3 = sum (s[m] (a + b) - d[m] (a - b)),
        //   s[m] = (t1[m] + t1[47 - m]) / 2 and d[m] = (t1[m] - t1[47 - m]) / 2.
        // 97 multiplies a sample instead of 193.
        struct Taps
        {
            std::array<double, 25> p0 {}; // t0[0..24], the centre last
            std::array<double, 24> p2 {};
            std::array<double, 24> sum {};
            std::array<double, 24> diff {};
        };

        constexpr double tap (int k) noexcept { return 4.0 * kOsFir4[static_cast<std::size_t> (k)]; }

        constexpr Taps makeTaps() noexcept
        {
            Taps t {};
            for (int m = 0; m <= 24; ++m)
                t.p0[static_cast<std::size_t> (m)] = tap (4 * m);
            for (int m = 0; m < 24; ++m)
            {
                t.p2[static_cast<std::size_t> (m)] = tap (2 + 4 * m);
                const double a = tap (1 + 4 * m), b = tap (1 + 4 * (47 - m));
                t.sum[static_cast<std::size_t> (m)] = 0.5 * (a + b);
                t.diff[static_cast<std::size_t> (m)] = 0.5 * (a - b);
            }
            return t;
        }

        constexpr Taps kTapsUp = makeTaps();
    }

    void ExactUpsampler4::push (double x, double* out) noexcept
    {
        history.push (x);
        const double* h = history.newest();
        out[0] = dotSymmetric (kTapsUp.p0.data(), h, 24); // phase 0: 24 pairs about x[-24], and the centre
        // phase 2, and phases 1 and 3 together, on the pairs (x[-m], x[-(47 - m)]), two partial sums each
        double e0 = 0.0, e1 = 0.0, s0 = 0.0, s1 = 0.0, d0 = 0.0, d1 = 0.0;
        for (int m = 0; m < 24; m += 2)
        {
            const auto k = static_cast<std::size_t> (m);
            const double a0 = h[-m], b0 = h[-(47 - m)];
            const double a1 = h[-m - 1], b1 = h[-(46 - m)];
            e0 += kTapsUp.p2[k] * (a0 + b0);
            e1 += kTapsUp.p2[k + 1] * (a1 + b1);
            s0 += kTapsUp.sum[k] * (a0 + b0);
            s1 += kTapsUp.sum[k + 1] * (a1 + b1);
            d0 += kTapsUp.diff[k] * (a0 - b0);
            d1 += kTapsUp.diff[k + 1] * (a1 - b1);
        }
        const double s = s0 + s1, d = d0 + d1;
        out[1] = s + d;
        out[2] = e0 + e1;
        out[3] = s - d;
    }

    double ExactDownsampler4::push (const double* in) noexcept
    {
        history.push (in[0]);
        const double y = dotSymmetric (kOsFir4.data(), history.newest(), kCentre); // the taps are symmetric about the centre
        history.push (in[1]);
        history.push (in[2]);
        history.push (in[3]);
        return y;
    }
}
