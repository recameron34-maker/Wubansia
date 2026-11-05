#pragma once

// Echo, the second send's return (ADR 0021): sends.echo on a stream. A tape ping-pong: two lines of `steps` 16ths at
// the host's tempo, cross-fed; the input enters the left line; each repeat reads its line (the delay wobbling by wow
// and flutter, fractions of it, at absolute-time phases), soft-saturates (tanh (drive u) / drive), passes a 2nd-order
// Butterworth low-pass and high-pass, sounds on its side and feeds the other line at the feedback. Repeat k sounds on
// the left for odd k, the right for even k.
// The tempo is read per block. When it changes the delay glides to the new time (a tape's speed change) instead of
// jumping; at a steady tempo it is exactly sends.echo's.

#include "fx/biquad.h"
#include "fx/delay_line.h"

#include <cstdint>

namespace wubansia::engine::fx
{
    struct EchoSettings
    {
        int steps = 3; // 16ths
        double feedback = 0.38;
        double lpHz = 2600.0;
        double hpHz = 300.0;
        double drive = 1.5;
        double wow = 0.0004; // the wobble, a fraction of the delay
        double wowHz = 0.5;
        double flutter = 0.00015;
        double flutterHz = 4.7;
        double wowPhase = 0.0; // radians
        double flutterPhase = 0.0;
    };

    class Echo
    {
    public:
        static constexpr int kMaxSteps = 16; // the manifest's largest
        static constexpr double kMinBpm = 40.0; // the lines hold kMaxSteps 16ths at this tempo; slower ones clamp

        void prepare (double sampleRate);
        void setup (const EchoSettings& settings) noexcept; // also resets
        void reset() noexcept;

        // The tempo for the following samples (BPM > 0).
        void setTempo (double bpm) noexcept;

        // The delay in seconds directly (tests; sends.echo's delay_s).
        void setDelaySeconds (double seconds) noexcept;

        // in: the mono send; out: the stereo return.
        void process (const float* in, float* outL, float* outR, int n) noexcept;

    private:
        double fs = 48000.0;
        EchoSettings s;
        double target = 0.0; // the delay, samples
        double current = 0.0; // what the tape runs at now (glides to target)
        double glide = 0.0; // per-sample factor of the glide
        bool primed = false; // a delay was set since the last reset
        DelayLine<float> a, b;
        BiquadCascade<2, 2> loop; // LP then HP, per side (0: the left line's repeats, 1: the right's)
        std::int64_t t = 0;
    };
}
