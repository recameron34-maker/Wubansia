#pragma once

// The onset duck's envelope (ADR 0021): phrase.pump_db on a stream, as rt_ref.onset_envelope runs it. At each onset
// it rises over the ramp (linearly; at once for a ramp of 0), holds, then falls back over the release on a raised
// cosine; overlapping onsets take the larger value. The duck's gain is -depth x envelope dB (a negative depth
// lifts). Times are seconds of the host's rate, so the shape is the same at every rate.
//
// Only a few onsets matter at once: once an onset's ramp is done its envelope is at least every older one's (they
// started earlier, so they fall earlier), so the envelope is the latest such onset's, or a newer one's still
// ramping, whichever is larger. Older onsets are dropped.

#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace wubansia::engine::fx
{
    class OnsetEnvelope
    {
    public:
        // Times in seconds, the rate in Hz.
        void setShape (double rampS, double holdS, double releaseS, double sampleRate) noexcept
        {
            fs = sampleRate;
            ramp = rampS;
            hold = holdS;
            release = releaseS > 0.0 ? releaseS : 1e-9;
        }

        void reset() noexcept
        {
            count = 0;
            now = 0;
        }

        // An onset at the next sample (before tick ()).
        void trigger() noexcept
        {
            if (count == kSlots) // more onsets inside one ramp than slots: the oldest goes
            {
                for (int i = 1; i < kSlots; ++i)
                    onsets[static_cast<std::size_t> (i - 1)] = onsets[static_cast<std::size_t> (i)];
                --count;
            }
            onsets[static_cast<std::size_t> (count++)] = now;
        }

        // The envelope at this sample, 0..1; then the next sample.
        double tick() noexcept
        {
            double e = 0.0;
            // From the newest: onsets still ramping, then the first whose ramp is done, which hides all older ones.
            int keepFrom = 0;
            for (int i = count - 1; i >= 0; --i)
            {
                const double tau = static_cast<double> (now - onsets[static_cast<std::size_t> (i)]) / fs;
                if (tau < ramp)
                {
                    const double v = tau / ramp;
                    e = v > e ? v : e;
                    continue;
                }
                const double u = (tau - ramp - hold) / release;
                double v = 0.0;
                if (u < 1.0)
                    v = u <= 0.0 ? 1.0 : 0.5 + 0.5 * std::cos (std::numbers::pi * u);
                e = v > e ? v : e;
                keepFrom = u < 1.0 ? i : i + 1;
                break;
            }
            if (keepFrom > 0)
            {
                for (int i = keepFrom; i < count; ++i)
                    onsets[static_cast<std::size_t> (i - keepFrom)] = onsets[static_cast<std::size_t> (i)];
                count -= keepFrom;
            }
            ++now;
            return e;
        }

        bool idle() const noexcept { return count == 0; }

    private:
        static constexpr int kSlots = 16;
        std::array<std::int64_t, kSlots> onsets {};
        int count = 0;
        std::int64_t now = 0;
        double fs = 48000.0;
        double ramp = 0.0;
        double hold = 0.0;
        double release = 1.0;
    };
}
