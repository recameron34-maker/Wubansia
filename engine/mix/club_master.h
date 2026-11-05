#pragma once

// The club master (ADR 0021; master.py's chain, played live), per kit and variant:
//  1. a Butterworth high-pass (25 Hz, 4th order);
//  2. mono under 150 Hz (the side high-passed, 8th order);
//  3. the glue at the variant's solved threshold (2:1, 6 dB knee, 30 ms attack, auto release; 23 samples late);
//  4. the variant's solved drive;
//  5. the soft clip at 4x on os_fir(4)'s taps (48 samples late);
//  6. a causal 5 Hz DC blocker, in place of master.py's zero-phase clean;
//  7. the true-peak limiter at -1.02 dBTP, its 60 ms hold looking back (2 round (0.0025 fs) + 49 samples late).
// Its latency is fixed per rate whatever the kit: a limiter lookahead shorter than 5 ms is padded, a longer one
// clamped. Off, the bus passes through a delay as long, and switching crossfades over 10 ms; the chain keeps running
// either way, so it comes back warm. A variant change moves the glue's threshold at once (its envelopes glide) and the
// drive over 10 ms.

#include "engine.h"
#include "fx/biquad.h"
#include "fx/dc_blocker.h"
#include "fx/delay_line.h"
#include "fx/glue.h"
#include "fx/limiter.h"
#include "fx/oversampled.h"
#include "fx/stereo.h"
#include "mix/channel.h"
#include "mix/mix_config.h"

#include <array>
#include <vector>

namespace wubansia::engine::mix
{
    class ClubMaster
    {
    public:
        static constexpr double kLookaheadS = 0.005; // the latency budget's lookahead

        // The fixed latency at this rate: glue, clip and the look-back limiter at a 5 ms lookahead.
        static int latencyFor (double sampleRate) noexcept;

        void prepare (double sampleRate, int maxBlock); // allocates

        // The kit's two masters (by Variant); also resets. origin: how late aligned time 0 reaches the master.
        void setup (const std::array<MasterConfig, 2>& config, const Controls& controls, int origin) noexcept;
        void reset (const Controls& controls) noexcept;

        int latency() const noexcept { return fixedLatency; }

        // Stereo in place, latency () samples late.
        void process (float* l, float* r, int n, const Controls& controls) noexcept;

        // The glue's and the limiter's gain reduction at the end of the last block, dB.
        double gainReductionDb() const noexcept { return gr; }

    private:
        double fs = 48000.0;
        int fixedLatency = 0;
        int pad = 0; // the limiter's latency short of the budget
        std::array<MasterConfig, 2> cfg {};
        int variant = 0;

        fx::BiquadCascade<4, 2> lowcut;
        fx::SideFilter mono;
        fx::Glue glue;
        Ramp drive;
        fx::MasterClip clip;
        fx::DcBlocker dc;
        fx::Limiter limiter;
        fx::DelayLine<float> padL, padR, bypassL, bypassR;
        Ramp wet; // the club master's share, 0 (off) .. 1 (on)
        std::vector<float> dryL, dryR;
        double gr = 0.0;
        int origin = 0;
    };
}
