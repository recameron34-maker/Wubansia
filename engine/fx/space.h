#pragma once

// Space, the first send's return (ADR 0021): sends.space on a stream, at the host's rate.
// - The send (mono) high-passed (Butterworth, sendOrder) and pre-delayed.
// - An 8-line feedback delay network: Hadamard feedback, the exported delays (48 kHz frames, scaled to the rate)
//   and input signs, a one-pole damping per line, a low band (a one-pole split) decaying in its own T60, line gains
//   for the T60.
// - The shimmer: each line's damped output band-passed (Butterworth order 4) through an octave-up delay-line shifter
//   whose two taps run on absolute time (sends.octave_taps); the line feeds back sqrt (1 - shimmer^2) of itself and
//   `shimmer` of its octave.
// - Out: two orthogonal sign patterns of the lines, mono under monoHz (the side high-passed, 8th order).
// sends.py runs the network in blocks of its shortest delay; per sample it is the same arithmetic. The lines and the
// shifter's history hold float; every filter state is double.

#include "fx/biquad.h"
#include "fx/delay_line.h"

#include <array>
#include <cstdint>
#include <vector>

namespace wubansia::engine::fx
{
    struct SpaceSettings
    {
        std::array<int, 8> delaysFrames {}; // at 48 kHz
        std::array<int, 8> signs {}; // -1 or 1
        double t60S = 2.6;
        double t60LowS = 0.0; // 0: no low band
        double lowHz = 500.0;
        double dampHz = 6500.0;
        double predelayS = 0.020;
        double sendHpHz = 450.0; // 0: none
        int sendOrder = 4;
        double shimmer = 0.0; // 0: none
        double shimmerLoHz = 300.0;
        double shimmerHiHz = 7000.0;
        int windowFrames = 2048; // at 48 kHz
        double monoHz = 150.0; // 0: none
    };

    class Space
    {
    public:
        static constexpr int kLines = 8;
        static constexpr int kMaxDelayFrames = 48000; // at 48 kHz: the manifest's largest
        static constexpr int kMaxWindowFrames = 16384;
        static constexpr double kMaxPredelayS = 0.5;

        // Allocates the lines for the manifest's largest delays, window and pre-delay at this rate.
        void prepare (double sampleRate);
        void setup (const SpaceSettings& settings) noexcept; // also resets
        void reset() noexcept;

        // The stream starts this many samples after the first process (): the shimmer's taps count from it. Takes
        // effect at reset ().
        void setOrigin (std::int64_t samplesBefore) noexcept { origin = samplesBefore; }

        // in: the mono send; out: the stereo return. n samples.
        void process (const float* in, float* outL, float* outR, int n) noexcept;

    private:
        double fs = 48000.0;
        SpaceSettings s;
        std::array<int, kLines> d {};
        std::array<double, kLines> g {};
        std::array<double, kLines> lowMix {}; // g_low / g - 1
        std::array<double, kLines> inj {};
        double dampA = 0.0, lowA = 0.0;
        bool lowBand = false;
        int predelay = 0;
        int window = 2048;
        double shimmerDry = 1.0;

        BiquadCascade<8, 1> sendHp;
        DelayLine<float> pre;
        std::array<DelayLine<float>, kLines> lines;
        std::array<DelayLine<float>, kLines> hist; // the shifter's input (band-passed taps)
        std::array<double, kLines> damp {}, low {};
        // the shimmer's band-pass, its state by section and line, so the 8 lines run side by side
        std::array<BiquadCoeffs, 4> bandCoeffs {};
        int bandSections = 0;
        std::array<std::array<double, kLines>, 4> bandZ1 {}, bandZ2 {};
        BiquadCascade<4, 2> monoSide; // 8th order: 4 sections, on the side (channel 0)
        std::int64_t origin = 0;
        std::int64_t t = 0; // from the stream's start
        double phaseSin = 0.0, phaseCos = 1.0, stepSin = 0.0, stepCos = 1.0; // the shifter's crossfade
        bool phaseSynced = false;
    };
}
