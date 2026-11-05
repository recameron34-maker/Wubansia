#pragma once

// The level duck's detector (ADR 0021): sends.duck on a stream with a fixed top, as rt_ref.level_duck_db runs it.
// The key (mono) is measured in RMS blocks of 64 samples at 48 kHz (the same time at other rates), its level in dB
// smoothed per block (attack while rising, release while falling, from silence, -200 dB), then mapped from
// (top - range) .. top onto 0 .. 1 of the duck's depth. sends.duck interpolates between block centres, which needs
// the next block; here the value follows one and a half blocks later (96 samples at 48 kHz), from finished blocks
// only, so the detector adds no latency. Before the first centre it is 0.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace wubansia::engine::fx
{
    class LevelDetector
    {
    public:
        static constexpr double kSilenceDb = -200.0; // 10 log10 (1e-20)

        void setup (double rangeDb, double topDb, double attackS, double releaseS, double sampleRate) noexcept
        {
            block = std::max (1, static_cast<int> (std::nearbyint (64.0 * sampleRate / 48000.0)));
            lag = block + block / 2;
            range = std::max (rangeDb, 1e-9);
            low = topDb - rangeDb;
            const double b = static_cast<double> (block);
            attack = std::exp (-b / (std::max (attackS, 1e-9) * sampleRate));
            release = std::exp (-b / (std::max (releaseS, 1e-9) * sampleRate));
            reset();
        }

        void reset() noexcept
        {
            t = 0;
            acc = 0.0;
            level = kSilenceDb;
            values.fill (0.0);
            blocksDone = 0;
        }

        int blockSize() const noexcept { return block; }
        int lagSamples() const noexcept { return lag; }

        // One key sample in; the share of the duck's depth (0..1) at this sample out.
        double tick (double key) noexcept
        {
            acc += key * key;
            if ((t + 1) % block == 0)
            {
                const double p = acc / static_cast<double> (block);
                acc = 0.0;
                const double v = 10.0 * std::log10 (p + 1e-20);
                const double c = v > level ? attack : release;
                level = c * level + (1.0 - c) * v;
                values[static_cast<std::size_t> (blocksDone % kRing)] = std::clamp ((level - low) / range, 0.0, 1.0);
                ++blocksDone;
            }

            // np.interp (t - lag, centres (i + 0.5) block - 0.5, values, left = 0)
            const double b = static_cast<double> (block);
            const double x = static_cast<double> (t - lag);
            const double c0 = 0.5 * b - 0.5;
            double out = 0.0;
            if (x >= c0)
            {
                const auto i = static_cast<std::int64_t> (std::floor ((x - c0) / b));
                const double ci = (static_cast<double> (i) + 0.5) * b - 0.5;
                const double ri = valueAt (i);
                if (i + 1 < blocksDone)
                {
                    const double slope = (valueAt (i + 1) - ri) / b;
                    out = slope * (x - ci) + ri;
                }
                else
                    out = ri;
            }
            ++t;
            return out;
        }

    private:
        static constexpr int kRing = 4;

        double valueAt (std::int64_t i) const noexcept
        {
            if (i < 0 || i >= blocksDone || blocksDone - i > kRing)
                return 0.0;
            return values[static_cast<std::size_t> (i % kRing)];
        }

        int block = 64;
        int lag = 96;
        double range = 18.0;
        double low = -18.0;
        double attack = 0.0;
        double release = 0.0;
        std::int64_t t = 0;
        double acc = 0.0;
        double level = kSilenceDb;
        std::array<double, kRing> values {};
        std::int64_t blocksDone = 0;
    };
}
