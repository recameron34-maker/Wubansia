#pragma once

// dsp48.svf_tpt: Zavalishin's topology-preserving state-variable filter, its cutoff and Q settable every sample (the
// cc_filter insert's CC74 and CC71 rides). One channel per Svf; the coefficients (SvfCoeffs) can be shared.

#include <algorithm>
#include <cmath>
#include <numbers>

namespace wubansia::engine::fx
{
    struct SvfCoeffs
    {
        double g = 0.0; // tan (pi fc / fs), fc clamped to 10 Hz .. 0.49 fs
        double k = 1.0 / 0.707; // 1 / Q
        double a1 = 1.0; // 1 / (1 + g (g + k))

        static SvfCoeffs make (double fc, double q, double fs) noexcept
        {
            SvfCoeffs c;
            c.g = std::tan (std::numbers::pi * std::clamp (fc, 10.0, 0.49 * fs) / fs);
            c.k = 1.0 / std::max (q, 1e-6);
            c.a1 = 1.0 / (1.0 + c.g * (c.g + c.k));
            return c;
        }
    };

    struct SvfOut
    {
        double lp;
        double bp;
        double hp;
    };

    class Svf
    {
    public:
        void reset() noexcept { s1 = s2 = 0.0; }

        SvfOut tick (const SvfCoeffs& c, double x) noexcept
        {
            const double h = (x - (c.k + c.g) * s1 - s2) * c.a1;
            const double v1 = c.g * h;
            const double b = v1 + s1;
            s1 = b + v1;
            const double v2 = c.g * b;
            const double lo = v2 + s2;
            s2 = lo + v2;
            return { lo, b, h };
        }

    private:
        double s1 = 0.0;
        double s2 = 0.0;
    };
}
