#pragma once

// The club master's causal DC blocker (ADR 0021), in place of master.dc_clean's zero-phase one: a 2nd-order
// Butterworth high-pass at a few Hz (rt_ref.dc_block, DC_ORDER 2), stereo. It takes the sub-sonic ripple the clipper
// leaves before the limiter; the limiter, which comes after it, keeps the peaks under the ceiling.

#include "fx/biquad.h"

namespace wubansia::engine::fx
{
    class DcBlocker
    {
    public:
        static constexpr int kOrder = 2;

        void setup (double hz, double sampleRate) noexcept { filter.setButterworth (PassKind::highpass, kOrder, hz, 0.0, sampleRate); }
        void reset() noexcept { filter.reset(); }

        void process (float* l, float* r, int n) noexcept
        {
            filter.process (0, l, n);
            filter.process (1, r, n);
        }

    private:
        BiquadCascade<1, 2> filter;
    };
}
