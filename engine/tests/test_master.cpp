// The club master's blocks (ADR 0021): the glue and the true-peak limiter against master.py and rt_ref.py's
// vectors at 48 kHz, the limiter's ceiling on a +12 dB burst and its latencies at 44.1, 48 and 96 kHz.

#include "fx_vectors.h"

#include "fx/glue.h"
#include "fx/limiter.h"
#include "fx/oversampler_exact.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <vector>

using namespace wubansia::engine::fx;
using fxtest::channel;
using fxtest::load;
using fxtest::maxAbsDiff;

namespace
{
    constexpr double kFs = 48000.0;
    constexpr double kNonlinear = 1e-4;

    // The 4x true peak (os_fir(4)) of a stereo pair, linear.
    double truePeak (const std::vector<float>& l, const std::vector<float>& r)
    {
        double peak = 0.0;
        for (const auto* ch : { &l, &r })
        {
            ExactUpsampler4 up;
            for (std::size_t i = 0; i < ch->size() + 25; ++i)
            {
                std::array<double, 4> u {};
                up.push (i < ch->size() ? static_cast<double> ((*ch)[i]) : 0.0, u.data());
                for (const double v : u)
                    peak = std::max (peak, std::abs (v));
            }
        }
        return peak;
    }
}

TEST_CASE ("master vectors: the glue at a fixed threshold is master.glue's detector and envelopes", "[master][vectors]")
{
    const auto c = fxtest::music (48000, 3, { { 2400, 2400, 2.0 }, { 12000, 6000, 1.5 }, { 30000, 1200, 2.5 } }, 0.9);
    auto l = channel (c, 2, 0);
    auto r = channel (c, 2, 1);
    const auto n = l.size();
    l.resize (n + Glue::kLatency, 0.0f);
    r.resize (n + Glue::kLatency, 0.0f);

    Glue glue;
    GlueSettings s;
    s.thresholdDb = -20.0;
    glue.setup (s, kFs);
    std::vector<double> gr (l.size());
    glue.process (l.data(), r.data(), static_cast<int> (l.size()), gr.data());

    CHECK (Glue::kLatency == 23);
    const auto want = load ("glue_gr"); // the reduction at 16 i + 8
    std::vector<float> got;
    for (std::size_t i = 0; i < want.size(); ++i)
        got.push_back (static_cast<float> (gr[16 * i + 8 + Glue::kLatency]));
    CHECK (maxAbsDiff (got, want) <= kNonlinear);
    CHECK (*std::max_element (want.begin(), want.end()) > 2.0f); // it compresses

    std::vector<float> out;
    for (std::size_t i = 24000; i < 24512; ++i)
    {
        out.push_back (l[i + Glue::kLatency]);
        out.push_back (r[i + Glue::kLatency]);
    }
    CHECK (maxAbsDiff (out, load ("glue_out")) <= kNonlinear);
}

TEST_CASE ("master vectors: the limiter, two-sided (master.limit) and looking back (rt_ref)", "[master][vectors]")
{
    const auto d = fxtest::music (24000, 4, { { 1000, 800, 3.0 }, { 6000, 300, 4.0 }, { 15000, 3000, 2.0 } }, 2.0);
    for (const auto hold : { Limiter::Hold::twoSided, Limiter::Hold::lookBack })
    {
        const bool two = hold == Limiter::Hold::twoSided;
        CAPTURE (two);
        Limiter lim;
        lim.prepare (kFs);
        lim.setup (LimiterSettings {}, hold);
        const int latency = lim.latency();
        CHECK (latency == (two ? 3145 : 289));

        auto l = channel (d, 2, 0);
        auto r = channel (d, 2, 1);
        const auto n = l.size();
        l.resize (n + static_cast<std::size_t> (latency), 0.0f);
        r.resize (n + static_cast<std::size_t> (latency), 0.0f);
        std::vector<double> gr (l.size());
        lim.process (l.data(), r.data(), static_cast<int> (l.size()), gr.data());

        const auto want = load (two ? "limit2_gr" : "limitlb_gr"); // every 16th sample
        std::vector<float> got;
        for (std::size_t i = 0; i < want.size(); ++i)
            got.push_back (static_cast<float> (gr[16 * i + static_cast<std::size_t> (latency)]));
        CHECK (maxAbsDiff (got, want) <= kNonlinear);
        CHECK (*std::max_element (want.begin(), want.end()) > 6.0f);

        std::vector<float> out;
        for (std::size_t i = 5800; i < 6312; ++i)
        {
            out.push_back (l[i + static_cast<std::size_t> (latency)]);
            out.push_back (r[i + static_cast<std::size_t> (latency)]);
        }
        CHECK (maxAbsDiff (out, load (two ? "limit2_out" : "limitlb_out")) <= kNonlinear);
    }
}

TEST_CASE ("master: the limiter holds a +12 dB burst at or under -1.02 dBTP in both holds", "[master]")
{
    const double rate = GENERATE (44100.0, 48000.0, 96000.0);
    const auto hold = GENERATE (Limiter::Hold::lookBack, Limiter::Hold::twoSided);
    CAPTURE (rate, hold == Limiter::Hold::twoSided);

    // a loop at 0 dBFS with a burst 12 dB over it, and a hot tone after it
    const int n = static_cast<int> (rate);
    auto x = fxtest::music (n, 9, { { n / 4, n / 8, 16.0 } }, 1.0);
    for (int i = n / 2; i < n * 3 / 4; ++i)
        for (int c = 0; c < 2; ++c)
            x[static_cast<std::size_t> (2 * i + c)] += static_cast<float> (3.0 * std::sin (2.0 * 3.141592653589793 * 9973.0 * i / rate + c));
    auto l = channel (x, 2, 0);
    auto r = channel (x, 2, 1);

    Limiter lim;
    lim.prepare (rate);
    lim.setup (LimiterSettings {}, hold);
    const auto latency = static_cast<std::size_t> (lim.latency());
    l.resize (l.size() + latency, 0.0f);
    r.resize (r.size() + latency, 0.0f);
    lim.process (l.data(), r.data(), static_cast<int> (l.size()));

    // at the ceiling up to the 4x grid's residual (master.limit's own, 0.0002 dB at 44.1 kHz) and float rounding
    const double tp = 20.0 * std::log10 (truePeak (l, r));
    INFO ("true peak " << tp << " dBTP");
    CHECK (tp <= -1.02 + 0.001);
    CHECK (tp > -2.0); // and it isn't simply quiet
}

TEST_CASE ("master: the limiter's latency is master.limit_latency's at 48 kHz and scales with the rate", "[master]")
{
    LimiterSettings s;
    CHECK (Limiter::latencyFor (s, Limiter::Hold::twoSided, 48000.0) == 3145);
    CHECK (Limiter::latencyFor (s, Limiter::Hold::lookBack, 48000.0) == 289); // 240 + 24 + 1 + 24
    CHECK (Limiter::latencyFor (s, Limiter::Hold::lookBack, 44100.0) == 220 + 49);
    CHECK (Limiter::latencyFor (s, Limiter::Hold::lookBack, 96000.0) == 480 + 49);

    // an impulse under the ceiling comes out exactly that late, untouched
    for (const double rate : { 44100.0, 48000.0, 96000.0 })
        for (const auto hold : { Limiter::Hold::lookBack, Limiter::Hold::twoSided })
        {
            Limiter lim;
            lim.prepare (rate);
            lim.setup (s, hold);
            std::vector<float> l (8192, 0.0f), r (8192, 0.0f);
            l[100] = 0.5f;
            r[100] = -0.25f;
            lim.process (l.data(), r.data(), 8192);
            const auto at = static_cast<std::size_t> (std::max_element (l.begin(), l.end()) - l.begin());
            CHECK (static_cast<int> (at) == 100 + lim.latency());
            CHECK (std::equal_to<float> {}(l[at], 0.5f));
            CHECK (std::equal_to<float> {}(r[at], -0.25f));
        }
}
