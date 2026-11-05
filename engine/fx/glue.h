#pragma once

// The glue (ADR 0021): master.glue at a fixed threshold, on a stream, as rt_ref.glue_db runs it. The club master's
// glue and the comp insert.
// - The detector: the mean power of L and R (stereo-linked) per 16-sample block, smoothed by a one-pole over 5 ms,
//   in dB.
// - The static curve: (1 - 1 / ratio) of the level over the threshold, with a soft knee.
// - Two envelopes per block on the reduction: a fast one (attack, release[0]) and a slow one following it (a 0.5 s
//   attack, release[1]); the larger applies, so sustained compression releases slowly and a lone hit quickly.
// - The gain per sample, interpolated between block centres, which needs the next block: the audio is 23 samples late
//   (a block and a half, less one).
// Blocks count from the stream's first sample, whatever the host's block size. 16 samples and the 0.5 s slow attack
// are master.py's constants; the times are seconds at any rate.

#include "fx/delay_line.h"

#include <array>
#include <cstdint>

namespace wubansia::engine::fx
{
    struct GlueSettings
    {
        double thresholdDb = 0.0;
        double ratio = 2.0;
        double kneeDb = 6.0;
        double attackS = 0.030;
        double releaseFastS = 0.15;
        double releaseSlowS = 1.5;
        double makeupDb = 0.0;
    };

    class Glue
    {
    public:
        static constexpr int kBlock = 16; // master.BLOCK
        static constexpr int kLatency = kBlock + kBlock / 2 - 1; // 23
        static constexpr double kRmsS = 0.005; // master.GLUE_RMS_S
        static constexpr double kSlowAttackS = 0.5; // master.SLOW_ATTACK_S

        // Also resets. The threshold can move later with setThreshold () (the club master's variant).
        void setup (const GlueSettings& settings, double sampleRate) noexcept;
        void setThreshold (double thresholdDb) noexcept { s.thresholdDb = thresholdDb; }
        void reset() noexcept;

        // The stream starts this many samples after the first process (): the input before it is silence, and the
        // blocks count from it (where aligned time 0 reaches the glue in the mix graph). Takes effect at reset ().
        void setOrigin (std::int64_t samplesBefore) noexcept { origin = samplesBefore; }

        // Stereo in place (r may equal l for mono), 23 samples late. grDb, if given, receives each output sample's
        // gain reduction in dB (>= 0, before the makeup).
        void process (float* l, float* r, int n, double* grDb = nullptr) noexcept;

        // The reduction (dB) the last process () ended on.
        double lastReductionDb() const noexcept { return lastGr; }

    private:
        double target (double levelDb) const noexcept;
        void finishBlock() noexcept;
        double gainAt (std::int64_t t) const noexcept; // the reduction (dB) at aligned sample t

        GlueSettings s;
        double fs = 48000.0;
        double rmsCoeff = 0.0;
        double attackCoeff = 0.0, fastCoeff = 0.0, slowAttackCoeff = 0.0, slowCoeff = 0.0;

        std::int64_t origin = 0;
        std::int64_t t = 0; // samples taken, from the stream's start (negative before it)
        double blockSum = 0.0;
        double power = 0.0; // the smoothed power
        double fast = 0.0, slow = 0.0;
        std::array<double, 4> out {}; // the last block reductions, by block index % 4
        std::int64_t blocks = 0;
        std::array<History<float, 32>, 2> delay {}; // the audio, 23 samples back
        double lastGr = 0.0;
    };
}
