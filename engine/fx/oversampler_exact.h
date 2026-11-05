#pragma once

// The club master's exact 4x oversampling (ADR 0021): dsp48.os_fir(4)'s 193 taps (generated/os_fir4.h), as
// scipy.signal.resample_poly runs them for dsp48.up and dsp48.down, made causal. resample_poly centres its filter, so
// its output is aligned with its input; here each direction is 24 samples (96 at 4x) late:
// - ExactUpsampler4: x[n] in, four samples out, y[4n + j] = sum 4 h[k] xup[4n + j - k]: dsp48.up (x, 4) 96 4x
//   samples late. It is also the limiter's true-peak detector.
// - ExactDownsampler4: four samples in, one out, y[n] = sum h[k] z[4n - k]: dsp48.down (z, 4) 24 samples late.
// Up, a 4x stage, then down: 48 samples late, the club master clip's latency. One channel per object; state in
// double.

#include "fx/delay_line.h"

#include <array>

namespace wubansia::engine::fx
{
    inline constexpr int kExactOversamplerLatency = 48; // up and down, in base samples
    inline constexpr int kExactUpLatency4x = 96; // up alone, in 4x samples

    class ExactUpsampler4
    {
    public:
        void reset() noexcept { history.reset(); }

        // One input sample to four (out[0..3] at 4x).
        void push (double x, double* out) noexcept;

    private:
        History<double, 64> history; // the last 49 inputs
    };

    class ExactDownsampler4
    {
    public:
        void reset() noexcept { history.reset(); }

        // Four samples at 4x (the first is the one aligned with the output) to one.
        double push (const double* in) noexcept;

    private:
        History<double, 256> history; // the last 193 4x samples
    };
}
