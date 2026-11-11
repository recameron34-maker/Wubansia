#pragma once

// The club master's soft clip at any rate, on a fixed internal rate (ADR 0021): 4x at 44.1 and 48 kHz (MasterClip,
// master.soft_clip's os_fir(4) round trip, unchanged), 2x at 88.2 and 96 kHz and 1x at 176.4 and 192 kHz, each
// filtering with os_fir(4)'s taps at that internal rate, so it is master.py's clip in absolute time at every rate.
// Plain 4x at 96 kHz differed in two ways, and the limiter after it took about 0.13 dB less (the mix up to 0.2 LU
// louder):
// - the ADAA applies the curve's excess half an oversampled sample late (2.6 us at 4x 48 kHz, 1.3 us at 4x 96 kHz),
//   and that lag puts a small overshoot on every rising edge;
// - os_fir(4) is designed relative to the rate, so the clip would keep its harmonics up to about 44 kHz. Resampled to
//   48 kHz, their band-limited overshoot came back as true peak over the ceiling.
// The round trip is 192 internal samples late (96 each way): 48, 96 and 192 base samples, 1 ms at 48, 96 and 192 kHz.

#include "fx/delay_line.h"
#include "fx/dot.h"
#include "fx/generated/os_fir4.h"
#include "fx/oversampled.h"
#include "fx/soft_clip.h"

#include <array>

namespace wubansia::engine::fx
{
    class ClubClip
    {
    public:
        static int factorFor (double sampleRate) noexcept { return sampleRate < 64000.0 ? 4 : sampleRate < 128000.0 ? 2 : 1; }

        static int latencyFor (double sampleRate) noexcept { return 2 * kHalf / factorFor (sampleRate); }

        void setup (double sampleRate) noexcept
        {
            factor = factorFor (sampleRate);
            reset();
        }

        void setCurve (double ceilingDb, double kneeDb) noexcept
        {
            exact.setCurve (ceilingDb, kneeDb);
            for (auto& c : curves)
                c.setCurve (ceilingDb, kneeDb);
        }

        void reset() noexcept
        {
            exact.reset();
            for (auto& h : ups)
                h.reset();
            for (auto& h : downs)
                h.reset();
            for (auto& c : curves)
                c.reset();
        }

        int latency() const noexcept { return 2 * kHalf / factor; }

        void process (float* l, float* r, int n) noexcept
        {
            if (factor == 4)
            {
                exact.process (l, r, n);
                return;
            }
            float* ch[2] = { l, r };
            for (std::size_t c = 0; c < 2; ++c)
                for (int i = 0; i < n; ++i)
                    ch[c][i] = static_cast<float> (tick (c, static_cast<double> (ch[c][i])));
        }

    private:
        static_assert (MasterClip::kLatency == 2 * static_cast<int> (kOsFir4.size() / 2) / 4, "the 4x path is os_fir(4)'s");
        static_assert (symmetricTaps (kOsFir4), "os_fir(4)'s taps are symmetric");
        static constexpr int kHalf = static_cast<int> (kOsFir4.size() / 2); // 96

        // One base sample: factor internal samples, the input zero-stuffed (times factor, os_fir(4)'s unit DC gain)
        // and interpolated, clipped, then low-passed; the output is read on the input's grid, 192 internal samples late.
        double tick (std::size_t c, double x) noexcept
        {
            double y = 0.0;
            for (int j = 0; j < factor; ++j)
            {
                ups[c].push (j == 0 ? x * static_cast<double> (factor) : 0.0);
                const double u = dotSymmetric (kOsFir4.data(), ups[c].newest(), kHalf);
                downs[c].push (curves[c].tick (u));
                if (j == 0)
                    y = dotSymmetric (kOsFir4.data(), downs[c].newest(), kHalf);
            }
            return y;
        }

        int factor = 4;
        MasterClip exact;
        std::array<History<double, 256>, 2> ups {}, downs {};
        std::array<SoftClip, 2> curves {};
    };
}
