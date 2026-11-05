#pragma once

// The club master's true-peak limiter (ADR 0021): master.limit on a stream, as rt_ref.limit_db runs it.
// Stereo-linked:
// - need[n] = min (1, ceiling / the peak of the 4x signal within a sample of n) (os_fir(4), ExactUpsampler4);
// - the lowest need held from `hold` before n to lookahead + 24 after it (Hold::lookBack, the real-time master), or
//   to lookahead + hold after it (Hold::twoSided, master.py's, for groovecore render's verification renders);
// - ramped down by a moving average over the lookahead (never over the need), released exponentially in dB.
// The look-back hold reaches 24 samples (the true-peak FIR's reach) past the lookahead, so the gain is already flat
// across the interpolation kernel round each peak and the 4x peaks stay at the ceiling; without it they overshot it
// by up to 0.013 dB (ADR 0021). The two-sided hold reaches further anyway.
// The audio is late by the lookahead (2 round (lookahead fs / 2)), the hold's forward reach, the sample the detector
// reads ahead and the FIR's 24: 289 samples at 48 kHz looking back, 3145 two-sided (master.limit_latency).

#include "fx/delay_line.h"
#include "fx/oversampler_exact.h"

#include <array>
#include <cstdint>
#include <vector>

namespace wubansia::engine::fx
{
    struct LimiterSettings
    {
        double ceilingDb = -1.02;
        double lookaheadS = 0.005;
        double holdS = 0.060;
        double releaseS = 0.080;
    };

    class Limiter
    {
    public:
        enum class Hold : std::uint8_t {
            lookBack,
            twoSided
        };

        // Allocates for settings up to maxLookaheadS and maxHoldS at sampleRate (either hold).
        void prepare (double sampleRate, double maxLookaheadS = 0.020, double maxHoldS = 0.200);

        // Also resets. The latency follows: call latency () after.
        void setup (const LimiterSettings& settings, Hold hold) noexcept;
        void reset() noexcept;

        int latency() const noexcept { return after + 1 + 24; }

        // Stereo in place, latency () samples late. grDb, if given, receives each output sample's reduction (dB).
        void process (float* l, float* r, int n, double* grDb = nullptr) noexcept;

        double lastReductionDb() const noexcept { return lastR; }

        // The latency for these settings at this rate, without preparing one.
        static int latencyFor (const LimiterSettings& settings, Hold hold, double sampleRate) noexcept;

    private:
        void pushNeed (double need) noexcept;

        double fs = 48000.0;
        LimiterSettings s;
        Hold mode = Hold::lookBack;
        int lookahead = 240; // w
        int holdSamples = 2880; // h
        int after = 240; // how far the hold's window reaches forward: w, or w + h
        double ceiling = 1.0;
        double releaseCoeff = 0.0;

        std::array<ExactUpsampler4, 2> up {};
        History<double, 16> peaks4x; // the 4x peak (both channels), the last 9 at least

        // The sliding minimum over need[k - h .. k + after]: a monotonic deque of (index, value).
        std::vector<std::int64_t> dqIndex;
        std::vector<double> dqValue;
        std::size_t dqMask = 0;
        std::size_t dqHead = 0, dqTail = 0; // [head, tail)
        std::int64_t needCount = 0; // need values pushed (need[0..needCount))

        // The moving average of the minimum over w + 1 values.
        std::vector<double> meanRing; // allocated for the longest lookahead
        std::size_t meanSize = 241; // w + 1
        std::size_t meanPos = 0;
        double meanSum = 0.0;

        double r = 0.0; // the released reduction, dB
        double lastR = 0.0;
        std::int64_t t = 0; // samples taken
        DelayLine<float> audioL, audioR;
    };
}
