#pragma once

// The nonlinear curves, with first-order antiderivative antialiasing (dsp48._adaa): each sample's output is
// (F (u) - F (u1)) / (u - u1) for the curve's antiderivative F and the previous input u1, or f ((u + u1) / 2) when
// the step is 1e-6 or less. Both run at 4x (the oversamplers): the clip insert and the club master's clip, and the
// drive insert.
// - SoftClip: master.soft_clip. Linear up to T = C 10^(-knee / 20) under the ceiling C, then T + k tanh ((|u| - T) / k)
//   toward it (k = C - T), C1-smooth at the knee; the ADAA runs on the excess over the line only, so what stays under
//   the knee passes untouched.
// - TanhDrive: dsp48.tanh_adaa, (tanh (d x + b) - tanh (b)) / tanh (d).

#include <algorithm>
#include <cmath>
#include <numbers>

namespace wubansia::engine::fx
{
    inline constexpr double kAdaaEps = 1e-6;

    // log cosh (u), stable for any u.
    inline double logCosh (double u) noexcept
    {
        const double a = std::abs (u);
        return a + std::log1p (std::exp (-2.0 * a)) - std::numbers::ln2;
    }

    class SoftClip
    {
    public:
        // ceilingDb: the absolute ceiling (dBFS); kneeDb: the curve is linear up to this far under it.
        void setCurve (double ceilingDb, double kneeDb) noexcept
        {
            const double c = std::pow (10.0, ceilingDb / 20.0);
            threshold = c * std::pow (10.0, -kneeDb / 20.0);
            k = c - threshold;
        }

        void reset() noexcept
        {
            u1 = 0.0;
            f1 = 0.0;
        }

        // master.soft_clip's excess over the line, and its antiderivative.
        double excess (double u) const noexcept
        {
            const double a = std::abs (u);
            if (a <= threshold)
                return 0.0;
            const double s = u < 0.0 ? -1.0 : 1.0;
            return s * (threshold + k * std::tanh ((a - threshold) / k)) - u;
        }

        double excessIntegral (double u) const noexcept
        {
            const double a = std::abs (u);
            if (a <= threshold)
                return 0.0;
            const double v = std::max ((a - threshold) / k, 0.0);
            const double lc = v + std::log1p (std::exp (-2.0 * v)) - std::numbers::ln2;
            return threshold * a + k * k * lc - 0.5 * a * a - 0.5 * threshold * threshold;
        }

        // One sample at the oversampled rate: u plus the antialiased excess.
        double tick (double u) noexcept
        {
            const double F = excessIntegral (u);
            double e;
            if (std::abs (u) <= threshold && std::abs (u1) <= threshold)
                e = 0.0; // both under the knee: F (u) - F (u1) = 0, and so is f at their midpoint
            else
            {
                const double d = u - u1;
                e = std::abs (d) > kAdaaEps ? (F - f1) / d : excess (0.5 * (u + u1));
            }
            u1 = u;
            f1 = F;
            return u + e;
        }

        double knee() const noexcept { return threshold; }

    private:
        double threshold = 1.0;
        double k = 0.0;
        double u1 = 0.0;
        double f1 = 0.0;
    };

    class TanhDrive
    {
    public:
        // Also resets: the previous input is 0 (its antiderivative log cosh (b)), as at the start of a stream.
        void setCurve (double drive, double bias) noexcept
        {
            d = drive;
            b = bias;
            tb = std::tanh (bias);
            norm = 1.0 / std::tanh (std::max (drive, 1e-9));
            reset();
        }

        void reset() noexcept
        {
            u1 = 0.0;
            f1 = antiderivative (0.0);
        }

        double tick (double x) noexcept
        {
            const double u = d * x;
            const double F = antiderivative (u);
            const double du = u - u1;
            const double y = std::abs (du) > kAdaaEps ? (F - f1) / du : curve (0.5 * (u + u1));
            u1 = u;
            f1 = F;
            return y * norm;
        }

    private:
        double curve (double v) const noexcept { return std::tanh (v + b) - tb; }
        double antiderivative (double v) const noexcept { return logCosh (v + b) - v * tb; }

        double d = 1.0;
        double b = 0.0;
        double tb = 0.0;
        double norm = 1.0;
        double u1 = 0.0;
        double f1 = 0.0;
    };
}
