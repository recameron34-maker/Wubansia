#include "sampler/sinc_bank.h"

namespace wubansia::engine
{
    namespace
    {
        // The zeroth modified Bessel function of the first kind, by its series (enough terms for beta 9).
        double besselI0 (double x) noexcept
        {
            double sum = 1.0;
            double term = 1.0;
            const double q = 0.25 * x * x;
            for (int k = 1; k < 60; ++k)
            {
                term *= q / (static_cast<double> (k) * static_cast<double> (k));
                sum += term;
                if (term < 1.0e-17 * sum)
                    break;
            }
            return sum;
        }

        // sin (pi x) / (pi x): 1 at 0 and exactly 0 at the other integers, so bank 0's phase 0 is a copy.
        double sinc (double x) noexcept
        {
            const double r = std::nearbyint (x);
            if (std::fabs (x - r) < 1.0e-12)
                return std::fabs (r) < 0.5 ? 1.0 : 0.0;
            return std::sin (3.14159265358979323846 * x) / (3.14159265358979323846 * x);
        }
    }

    SincBank::SincBank()
    {
        rows.assign (static_cast<std::size_t> (kBanks) * kRows * kTaps, 0.0f);
        const double i0Beta = besselI0 (kBeta);
        const double half = kTaps / 2; // the window spans distances -8..8
        for (int b = 0; b < kBanks; ++b)
        {
            upper[static_cast<std::size_t> (b)] = std::exp2 (b / 12.0);
            const double fc = cutoff (b); // cycles per frame
            float* table = rows.data() + static_cast<std::size_t> (b) * kRows * kTaps;
            for (int p = 0; p < kRows; ++p)
            {
                const double frac = static_cast<double> (p) / kPhases;
                double h[kTaps];
                double sum = 0.0;
                for (int j = 0; j < kTaps; ++j)
                {
                    const double d = frac + kLeft - j; // from tap j to the read point, in frames
                    const double w2 = 1.0 - (d / half) * (d / half);
                    const double window = w2 >= 0.0 ? besselI0 (kBeta * std::sqrt (w2)) / i0Beta : 0.0;
                    h[j] = 2.0 * fc * sinc (2.0 * fc * d) * window;
                    sum += h[j];
                }
                for (int j = 0; j < kTaps; ++j) // unit gain at DC on every phase
                    table[p * kTaps + j] = static_cast<float> (h[j] / sum);
            }
        }
    }

    const SincBank& SincBank::instance()
    {
        static const SincBank bank;
        return bank;
    }
}
