#pragma once

// The Resonator, the third send's return (ADR 0021): sends.resonator on a stream. The send high-passed (2nd-order
// Butterworth), into one 2-pole resonator per tone (the amplitude falls by e in tau), each scaled so white noise comes
// out at the level it went in, the tones in ascending order alternating 0.3 left and right of centre (equal power,
// x sqrt 2). Tones at or over 0.49 of the rate are left out (they'd fold back).

#include "fx/biquad.h"

#include <array>

namespace wubansia::engine::fx
{
    inline constexpr int kMaxResonatorTones = 24;

    struct ResonatorSettings
    {
        std::array<double, kMaxResonatorTones> tonesHz {};
        int tones = 0;
        double tauS = 0.4;
        double sendHpHz = 600.0; // 0: none
    };

    class Resonator
    {
    public:
        void prepare (double sampleRate) noexcept { fs = sampleRate; }
        void setup (const ResonatorSettings& settings) noexcept; // also resets
        void reset() noexcept;

        bool active() const noexcept { return count > 0; }

        // in: the mono send; out: the stereo return.
        void process (const float* in, float* outL, float* outR, int n) noexcept;

    private:
        struct Mode
        {
            double a1 = 0.0, a2 = 0.0; // y = x - a1 y1 - a2 y2
            double norm = 0.0;
            double gl = 0.0, gr = 0.0; // pan, times sqrt 2
            double y1 = 0.0, y2 = 0.0;
        };

        double fs = 48000.0;
        BiquadCascade<1, 1> sendHp;
        std::array<Mode, kMaxResonatorTones> modes {};
        int count = 0;
    };
}
