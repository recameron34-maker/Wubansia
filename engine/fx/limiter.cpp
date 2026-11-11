#include "fx/limiter.h"

#include <algorithm>
#include <cmath>

namespace wubansia::engine::fx
{
    namespace
    {
        constexpr double kDbToLog = 0.11512925464970228; // ln (10) / 20
        constexpr int kTpReach = 24; // os_fir(4)'s reach each way at 1x: master.TP_REACH

        int lookaheadSamples (double lookaheadS, double fs) noexcept
        {
            return 2 * static_cast<int> (std::nearbyint (lookaheadS * fs / 2.0));
        }

        int holdSamplesFor (double holdS, double fs) noexcept
        {
            return static_cast<int> (std::nearbyint (holdS * fs));
        }
    }

    int Limiter::latencyFor (const LimiterSettings& settings, Hold hold, double sampleRate) noexcept
    {
        const int w = lookaheadSamples (settings.lookaheadS, sampleRate);
        const int h = holdSamplesFor (settings.holdS, sampleRate);
        return w + (hold == Hold::twoSided ? h : kTpReach) + 1 + kTpReach;
    }

    void Limiter::prepare (double sampleRate, double maxLookaheadS, double maxHoldS)
    {
        fs = sampleRate;
        const int w = lookaheadSamples (maxLookaheadS, fs);
        const int h = holdSamplesFor (maxHoldS, fs);
        const auto window = static_cast<std::size_t> (2 * h + w + kTpReach + 2); // the longest: look-back, 2h + w + 24 + 1
        const std::size_t cap = ceilPow2 (window + 1);
        dqIndex.assign (cap, 0);
        dqValue.assign (cap, 1.0);
        dqMask = cap - 1;
        meanRing.assign (static_cast<std::size_t> (w + 1), 1.0);
        const auto maxLatency = static_cast<std::size_t> (w + h + 1 + kTpReach);
        audioL.prepare (maxLatency + 1);
        audioR.prepare (maxLatency + 1);
        setup (s, mode);
    }

    void Limiter::setup (const LimiterSettings& settings, Hold hold) noexcept
    {
        s = settings;
        mode = hold;
        const int maxW = std::max (static_cast<int> (meanRing.size()) - 1, 0);
        lookahead = std::clamp (lookaheadSamples (s.lookaheadS, fs), 0, maxW);
        // the deque spans need[k - before .. k + after]: before + after + 1 entries at most
        const int cap = static_cast<int> (dqMask + 1);
        holdSamples = std::clamp (holdSamplesFor (s.holdS, fs), 0, std::max ((cap - lookahead - kTpReach - 2) / 2, 0));
        before = mode == Hold::twoSided ? holdSamples : 2 * holdSamples;
        after = lookahead + (mode == Hold::twoSided ? holdSamples : kTpReach);
        meanSize = static_cast<std::size_t> (lookahead + 1);
        ceiling = std::pow (10.0, s.ceilingDb / 20.0);
        releaseCoeff = std::exp (-1.0 / (std::max (s.releaseS, 1e-9) * fs));
        reset();
    }

    void Limiter::reset() noexcept
    {
        for (auto& u : up)
            u.reset();
        peaks4x.reset();
        dqHead = dqTail = 0;
        needCount = 0;
        std::fill (meanRing.begin(), meanRing.end(), 1.0);
        meanPos = 0;
        meanSum = static_cast<double> (meanSize);
        r = 0.0;
        lastR = 0.0;
        t = 0;
        audioL.reset();
        audioR.reset();
    }

    // need[n] joins the window; the minimum over need[k - before .. k + after], k = n - after, then leaves at the front.
    // The ramp at k averages the minima at k - w .. k, so it starts w samples before the stream: before the stream the
    // need is 1 (silence), but those minima already see what follows, as master.limit's does from silence round its
    // loop.
    void Limiter::pushNeed (double need) noexcept
    {
        const std::int64_t n = needCount++;
        while (dqTail != dqHead && dqValue[(dqTail - 1) & dqMask] >= need)
            --dqTail;
        dqIndex[dqTail & dqMask] = n;
        dqValue[dqTail & dqMask] = need;
        ++dqTail;
        const std::int64_t k = n - after;
        while (dqIndex[dqHead & dqMask] < k - before)
            ++dqHead;
        if (k < -static_cast<std::int64_t> (lookahead))
            return;

        const double m = dqValue[dqHead & dqMask];
        meanSum += m - meanRing[meanPos];
        meanRing[meanPos] = m;
        if (++meanPos == meanSize)
        {
            meanPos = 0;
            double sum = 0.0; // once round the ring, the sum afresh: no drift over hours
            for (std::size_t i = 0; i < meanSize; ++i)
                sum += meanRing[i];
            meanSum = sum;
        }
        if (k < 0)
            return;
        const double g = meanSum / static_cast<double> (meanSize);
        const double r2 = g < 1.0 ? -20.0 * std::log10 (g) : 0.0;
        r = std::max (r2, r * releaseCoeff);
    }

    void Limiter::process (float* l, float* rr, int n, double* grDb) noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            const double xl = static_cast<double> (l[i]);
            const double xr = static_cast<double> (rr[i]);
            audioL.push (l[i]);
            audioR.push (rr[i]);

            std::array<double, 4> ul {}, ur {};
            up[0].push (xl, ul.data());
            up[1].push (xr, ur.data());
            for (std::size_t j = 0; j < 4; ++j)
            {
                // aligned 4x index 4 (t - 24) + j; before the stream rt_ref pads with silence
                const bool preStream = 4 * (t - kTpReach) + static_cast<std::int64_t> (j) < 0;
                peaks4x.push (preStream ? 0.0 : std::max (std::abs (ul[j]), std::abs (ur[j])));
                if (j == 0 && t >= kTpReach + 1)
                {
                    // need[t - 25]: the 4x peak over aligned 4 (t - 25) - 4 .. 4 (t - 25) + 4, the last 9 pushed
                    const double* p = peaks4x.newest();
                    double pk = p[0];
                    for (int d = 1; d < 9; ++d)
                        pk = std::max (pk, p[-d]);
                    pushNeed (std::min (1.0, ceiling / (pk + 1e-30)));
                }
            }

            const std::int64_t k = t - latency();
            double out = 0.0;
            if (k >= 0)
                out = r < 1e-9 ? 0.0 : r;
            const double gain = out > 0.0 ? std::exp (-kDbToLog * out) : 1.0;
            l[i] = static_cast<float> (static_cast<double> (audioL.at (k)) * gain);
            rr[i] = static_cast<float> (static_cast<double> (audioR.at (k)) * gain);
            if (grDb != nullptr)
                grDb[i] = out;
            lastR = out;
            ++t;
        }
    }
}
