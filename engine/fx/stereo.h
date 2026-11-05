#pragma once

// The stereo stages, on mid = (L + R) / 2 and side = (L - R) / 2 (ADR 0021):
// - SideHighpass: the side through a Butterworth high-pass. With mix 1 - (side) it is dsp48.mono_below (everything
//   under f in the centre); as width it is context4.widen, side + (width - 1) x HP (side), or side x width when f is
//   0.
// - pan: phrase.pan, equal power, unity in the centre.

#include "fx/biquad.h"

#include <cmath>
#include <numbers>

namespace wubansia::engine::fx
{
    class SideFilter
    {
    public:
        enum class Mode : std::uint8_t {
            monoBelow, // side' = HP (side)
            width // side' = side + (width - 1) HP (side); side x width without a filter
        };

        // Also resets. hz 0 (width only): no filter.
        void setup (Mode m, double hz, int order, double width, double fs) noexcept
        {
            mode = m;
            amount = width;
            if (hz > 0.0)
                hp.setButterworth (PassKind::highpass, order, hz, 0.0, fs);
            else
                hp.setSections (nullptr, 0);
            hp.reset();
        }

        void reset() noexcept { hp.reset(); }

        void process (float* l, float* r, int n) noexcept
        {
            const bool filtered = hp.sections() > 0;
            for (int i = 0; i < n; ++i)
            {
                const double xl = static_cast<double> (l[i]);
                const double xr = static_cast<double> (r[i]);
                const double m = 0.5 * (xl + xr);
                double s = 0.5 * (xl - xr);
                if (mode == Mode::monoBelow)
                    s = filtered ? hp.tick (0, s) : s;
                else
                    s = filtered ? s + (amount - 1.0) * hp.tick (0, s) : s * amount;
                l[i] = static_cast<float> (m + s);
                r[i] = static_cast<float> (m - s);
            }
        }

    private:
        Mode mode = Mode::monoBelow;
        double amount = 1.0;
        BiquadCascade<4, 1> hp;
    };

    // phrase.pan's two gains for p in -1..1: sqrt 2 (cos a, sin a), a = (p + 1) pi / 4.
    struct PanGains
    {
        double l = 1.0;
        double r = 1.0;

        static PanGains of (double p) noexcept
        {
            const double a = (p + 1.0) * std::numbers::pi / 4.0;
            return { std::numbers::sqrt2 * std::cos (a), std::numbers::sqrt2 * std::sin (a) };
        }
    };
}
