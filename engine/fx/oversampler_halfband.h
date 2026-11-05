#pragma once

// The clip and drive inserts' 4x oversampling (ADR 0021): two half-band stages (rt_ref.py's HB1, 47 taps, for
// 1x <-> 2x and HB2, 15 taps, for 2x <-> 4x, generated/halfband.h), made causal. rt_ref's hb_up4 and hb_down4 centre
// each stage; here the round trip (up, a 4x stage, down) is 27 samples late: 23 / 2 + 7 / 4 each way, 26.5, and one
// more 2x sample before the last decimation so the output lands on the input's grid.
// A half-band's even taps are 0 but the centre (0.5), so each stage's interpolation copies one phase and filters
// the other with half the taps, and each decimation sums half the taps and the centre.

#include "fx/delay_line.h"

namespace wubansia::engine::fx
{
    inline constexpr int kHalfbandLatency = 27; // up and down, in base samples

    class HalfbandUpsampler4
    {
    public:
        void reset() noexcept
        {
            stage1.reset();
            stage2.reset();
        }

        // One input sample to four (out[0..3] at 4x).
        void push (double x, double* out) noexcept;

    private:
        void up2 (double y, double* out) noexcept;

        History<double, 32> stage1; // 1x inputs: the last 24
        History<double, 8> stage2; // 2x inputs: the last 8
    };

    class HalfbandDownsampler4
    {
    public:
        void reset() noexcept
        {
            stage2.reset();
            stage1.reset();
        }

        // Four samples at 4x to one.
        double push (const double* in) noexcept;

    private:
        History<double, 16> stage2; // 4x inputs: the last 15
        History<double, 64> stage1; // 2x samples after stage 2: the last 47
    };
}
