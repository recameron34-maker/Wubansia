#include "fx/glue.h"

#include <algorithm>
#include <cmath>

namespace wubansia::engine::fx
{
    namespace
    {
        constexpr double kDbToLog = 0.11512925464970228; // ln (10) / 20: 10^(dB / 20) = exp (kDbToLog dB)
    }

    void Glue::setup (const GlueSettings& settings, double sampleRate) noexcept
    {
        s = settings;
        fs = sampleRate;
        const double b = static_cast<double> (kBlock);
        const auto coeff = [&] (double tau) { return std::exp (-b / (std::max (tau, 1e-9) * fs)); };
        rmsCoeff = coeff (kRmsS);
        attackCoeff = coeff (s.attackS);
        fastCoeff = coeff (s.releaseFastS);
        slowAttackCoeff = coeff (kSlowAttackS);
        slowCoeff = coeff (s.releaseSlowS);
        reset();
    }

    void Glue::reset() noexcept
    {
        t = 0;
        blockSum = 0.0;
        power = 0.0;
        fast = slow = 0.0;
        out.fill (0.0);
        blocks = 0;
        for (auto& d : delay)
            d.reset();
        lastGr = 0.0;
    }

    // master._reduction_db's static curve: (1 - 1 / ratio) x the level over the threshold, soft-kneed.
    double Glue::target (double levelDb) const noexcept
    {
        const double over = levelDb - s.thresholdDb;
        const double knee = s.kneeDb;
        double v;
        if (over <= -knee / 2.0)
            v = 0.0;
        else if (over >= knee / 2.0)
            v = over;
        else
            v = knee > 0.0 ? (over + knee / 2.0) * (over + knee / 2.0) / (2.0 * knee) : over;
        return (1.0 - 1.0 / s.ratio) * v;
    }

    void Glue::finishBlock() noexcept
    {
        const double p = blockSum / static_cast<double> (kBlock);
        blockSum = 0.0;
        power = (1.0 - rmsCoeff) * p + rmsCoeff * power;
        const double ti = target (10.0 * std::log10 (power + 1e-20));
        fast = ti + (ti > fast ? attackCoeff : fastCoeff) * (fast - ti);
        slow = fast + (fast > slow ? slowAttackCoeff : slowCoeff) * (slow - fast);
        out[static_cast<std::size_t> (blocks % 4)] = fast > slow ? fast : slow;
        ++blocks;
    }

    // np.interp (t, centres 16 i + 7.5, out): before the first centre, block 0's value.
    double Glue::gainAt (std::int64_t ta) const noexcept
    {
        if (ta < kBlock / 2)
            return out[0];
        const std::int64_t i = (ta - kBlock / 2) / kBlock;
        const double ci = static_cast<double> (kBlock * i) + 0.5 * kBlock - 0.5;
        const double oi = out[static_cast<std::size_t> (i % 4)];
        const double on = out[static_cast<std::size_t> ((i + 1) % 4)];
        const double slope = (on - oi) / static_cast<double> (kBlock);
        return slope * (static_cast<double> (ta) - ci) + oi;
    }

    void Glue::process (float* l, float* r, int n, double* grDb) noexcept
    {
        const bool stereo = r != nullptr && r != l;
        for (int i = 0; i < n; ++i)
        {
            const double xl = static_cast<double> (l[i]);
            const double xr = stereo ? static_cast<double> (r[i]) : xl;
            blockSum += (xl * xl + xr * xr) / 2.0;
            delay[0].push (l[i]);
            if (stereo)
                delay[1].push (r[i]);
            ++t;
            if (t % kBlock == 0)
                finishBlock();

            const std::int64_t ta = t - 1 - kLatency;
            double g = 0.0;
            if (ta >= 0)
                g = gainAt (ta);
            const double gain = std::exp (kDbToLog * (s.makeupDb - g));
            l[i] = static_cast<float> (static_cast<double> (delay[0].newest()[-kLatency]) * gain);
            if (stereo)
                r[i] = static_cast<float> (static_cast<double> (delay[1].newest()[-kLatency]) * gain);
            if (grDb != nullptr)
                grDb[i] = g;
            lastGr = g;
        }
    }
}
