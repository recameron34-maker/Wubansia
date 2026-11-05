#pragma once

// The band an insert works in (ADR 0021).
// - BandSplit (rt_ref.band_split), for the highpass, lowpass and cc_filter inserts: split off with LR4 crossovers
//   (dsp48.lr4, a 2nd-order Butterworth squared), the rest passing by, so rest + band is an all-pass of the input:
// - {lo, 0}: the band is everything over lo, the rest under it;
// - {0, hi}: the band is everything under hi, the rest over it;
// - {lo, hi}: the band between; the rest is what is under lo, through the upper crossover's all-pass, plus what is
//   over hi;
// - {0, 0}: the whole signal is the band (no split).
// - BandPart (rt_ref.band_part), for the ducks: an LR4 high-pass at lo, then an LR4 low-pass at hi (each only if set).
//   A duck adds (g - 1) of it to the signal, x + (g - 1) band (x) (rt_ref.band_duck), so where it rests the signal
//   passes untouched; {0, 0} is x g.
// Stereo, state in double.

#include "fx/biquad.h"

namespace wubansia::engine::fx
{
    class BandSplit
    {
    public:
        // Frequencies in Hz (0: open), the rate in Hz. Also resets.
        void setup (double loHz, double hiHz, double fs) noexcept
        {
            lo = loHz > 0.0 ? loHz : 0.0;
            hi = hiHz > 0.0 ? hiHz : 0.0;
            if (lo > 0.0 && hi > 0.0 && ! (lo < hi))
                hi = 0.0;
            if (lo > 0.0)
            {
                loLow.setRepeated (lr4Section (PassKind::lowpass, lo, fs), 2);
                loHigh.setRepeated (lr4Section (PassKind::highpass, lo, fs), 2);
            }
            if (hi > 0.0)
            {
                const auto lp = lr4Section (PassKind::lowpass, hi, fs);
                const auto hp = lr4Section (PassKind::highpass, hi, fs);
                hiLow.setRepeated (lp, 2);
                hiHigh.setRepeated (hp, 2);
                apLow.setRepeated (lp, 2);
                apHigh.setRepeated (hp, 2);
            }
            reset();
        }

        void reset() noexcept
        {
            loLow.reset();
            loHigh.reset();
            hiLow.reset();
            hiHigh.reset();
            apLow.reset();
            apHigh.reset();
        }

        bool whole() const noexcept { return ! (lo > 0.0) && ! (hi > 0.0); }

        // x into (rest, band), one sample of channel c.
        void split (int c, double x, double& rest, double& band) noexcept
        {
            if (lo > 0.0 && hi > 0.0)
            {
                const double low = loLow.tick (c, x);
                const double up = loHigh.tick (c, x);
                band = hiLow.tick (c, up);
                const double top = hiHigh.tick (c, up);
                rest = apLow.tick (c, low) + apHigh.tick (c, low) + top;
            }
            else if (lo > 0.0)
            {
                rest = loLow.tick (c, x);
                band = loHigh.tick (c, x);
            }
            else if (hi > 0.0)
            {
                rest = hiHigh.tick (c, x);
                band = hiLow.tick (c, x);
            }
            else
            {
                rest = 0.0;
                band = x;
            }
        }

    private:
        double lo = 0.0, hi = 0.0;
        BiquadCascade<2, 2> loLow, loHigh; // at lo
        BiquadCascade<2, 2> hiLow, hiHigh; // at hi, on what is over lo (or on the input)
        BiquadCascade<2, 2> apLow, apHigh; // at hi, on what is under lo: its all-pass
    };

    class BandPart
    {
    public:
        // Frequencies in Hz (0: open), the rate in Hz. Also resets.
        void setup (double loHz, double hiHz, double fs) noexcept
        {
            lo = loHz > 0.0;
            hi = hiHz > 0.0;
            if (lo)
                high.setRepeated (lr4Section (PassKind::highpass, loHz, fs), 2);
            if (hi)
                low.setRepeated (lr4Section (PassKind::lowpass, hiHz, fs), 2);
            reset();
        }

        void reset() noexcept
        {
            high.reset();
            low.reset();
        }

        bool whole() const noexcept { return ! lo && ! hi; }

        double tick (int c, double x) noexcept
        {
            if (lo)
                x = high.tick (c, x);
            if (hi)
                x = low.tick (c, x);
            return x;
        }

    private:
        bool lo = false, hi = false;
        BiquadCascade<2, 2> high, low;
    };
}
