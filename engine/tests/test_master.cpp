// The club master's blocks (ADR 0021): the glue and the true-peak limiter against master.py and rt_ref.py's
// vectors at 48 kHz, the limiter's ceiling on a +12 dB burst and its latencies at 44.1, 48 and 96 kHz.

#include "fx_vectors.h"

#include "fx/glue.h"
#include "fx/limiter.h"
#include "fx/oversampler_exact.h"
#include "mix/club_master.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

using namespace wubansia::engine;
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

// ------------------------------------------------------------------ the club master's transparency

namespace
{
    void fft (std::vector<std::complex<double>>& a)
    {
        const std::size_t n = a.size();
        for (std::size_t i = 1, j = 0; i < n; ++i)
        {
            std::size_t bit = n >> 1;
            for (; j & bit; bit >>= 1)
                j ^= bit;
            j ^= bit;
            if (i < j)
                std::swap (a[i], a[j]);
        }
        for (std::size_t len = 2; len <= n; len <<= 1)
        {
            const double angle = -2.0 * 3.141592653589793 / static_cast<double> (len);
            const std::complex<double> wl (std::cos (angle), std::sin (angle));
            for (std::size_t i = 0; i < n; i += len)
            {
                std::complex<double> w (1.0, 0.0);
                for (std::size_t k = 0; k < len / 2; ++k)
                {
                    const auto u = a[i + k];
                    const auto v = a[i + k + len / 2] * w;
                    a[i + k] = u + v;
                    a[i + k + len / 2] = u - v;
                    w *= wl;
                }
            }
        }
    }

    // qa.harmonic_asr: the energy between the harmonics of f0 against the energy on them, dB (a Blackman-Harris
    // window; a bin is harmonic within max (8 bins, 0.5 % of k f0) of k f0, at most 0.45 f0; 0.5 f0 to 20 kHz).
    double harmonicAsr (const std::vector<double>& x, double f0, double fs)
    {
        const std::size_t n = x.size();
        std::vector<std::complex<double>> a (n);
        constexpr double c0 = 0.35875, c1 = 0.48829, c2 = 0.14128, c3 = 0.01168;
        for (std::size_t i = 0; i < n; ++i)
        {
            const double p = 2.0 * 3.141592653589793 * static_cast<double> (i) / static_cast<double> (n);
            const double w = c0 - c1 * std::cos (p) + c2 * std::cos (2.0 * p) - c3 * std::cos (3.0 * p);
            a[i] = x[i] * w;
        }
        fft (a);
        double on = 0.0, off = 0.0;
        const double binHz = fs / static_cast<double> (n);
        for (std::size_t b = 1; b < n / 2; ++b)
        {
            const double f = static_cast<double> (b) * binHz;
            if (! (f > 0.5 * f0) || ! (f < 20000.0))
                continue;
            bool harmonic = false;
            for (int k = 1; k <= static_cast<int> (24000.0 / f0) && ! harmonic; ++k)
                harmonic = std::abs (f - k * f0) <= std::min (std::max (8.0 * binHz, 0.005 * k * f0), 0.45 * f0);
            (harmonic ? on : off) += std::norm (a[b]);
        }
        return 10.0 * std::log10 (off / (on + 1e-30) + 1e-30);
    }
}

TEST_CASE ("master: a driven low saw through the club master adds no aliases (master.py's transparency fixture)", "[master]")
{
    // test_master.py's test_no_aliasing_on_a_driven_tone, live: an additive saw at 0.5, glued 2.5 dB and driven so its
    // true peak sits 3 or 6 dB over the limiter's ceiling, through the whole club master. With a 60 ms look-back
    // window the limiter's gain stepped every few cycles of a low note (55 Hz at 6 dB: -54.8 dB; 110 Hz: -57.2, -58.4
    // in master.py's fixture); twice the hold back (ADR 0021) keeps the lowest need in view, as master.py's two-sided
    // hold does
    const double f0 = GENERATE (55.0, 110.0, 220.0);
    const double over = GENERATE (3.0, 6.0);
    CAPTURE (f0, over);
    const double fs = 48000.0;
    const auto n = static_cast<std::size_t> (2.5 * fs);
    std::vector<float> l (n), r (n);
    const int harmonics = static_cast<int> (20000.0 / f0);
    for (std::size_t i = 0; i < n; ++i)
    {
        const double ph = 2.0 * 3.141592653589793 * f0 * static_cast<double> (i) / fs;
        double s = 0.0;
        for (int h = 1; h <= harmonics; ++h)
            s += std::sin (h * ph) / h;
        l[i] = r[i] = static_cast<float> (0.5 * s * 2.0 / 3.141592653589793);
    }
    const double inPeak = 20.0 * std::log10 (truePeak (l, r));
    double power = 0.0;
    for (std::size_t i = 0; i < n; ++i)
        power += static_cast<double> (l[i]) * l[i];
    const double level = 10.0 * std::log10 (power / static_cast<double> (n));

    // as master.py's fixture: the glue takes 2.5 dB off the steady tone (5 dB over its threshold at 2:1, past the
    // 6 dB knee's half), and the drive puts the glued true peak `over` dB above the limiter's ceiling. (Without the
    // glue, 6 dB on a bare 55 Hz saw leaves -57 dB between the harmonics in master.py's two-sided limiter too.)
    MasterConfig m;
    m.glueThresholdMdb = static_cast<std::int32_t> (std::lround ((level - 5.0) * 1000.0));
    m.driveMdb = static_cast<std::int32_t> (std::lround ((-1.02 + over - (inPeak - 2.5)) * 1000.0));
    mix::ClubMaster master;
    master.prepare (fs, 512);
    Controls controls;
    master.setup ({ m, m }, controls, 0);
    for (std::size_t s = 0; s < n; s += 512)
        master.process (l.data() + s, r.data() + s, static_cast<int> (std::min<std::size_t> (512, n - s)), controls);

    const std::size_t from = static_cast<std::size_t> (fs) / 2, len = 65536; // after the glue and limiter settle
    std::vector<double> mono (len);
    for (std::size_t i = 0; i < len; ++i)
        mono[i] = 0.5 * (static_cast<double> (l[from + i]) + r[from + i]);
    const double asr = harmonicAsr (mono, f0, fs);
    INFO ("between the harmonics " << asr << " dB; drive " << m.driveMdb / 1000.0 << " dB");
    CHECK (asr <= -60.0);
    CHECK (master.gainReductionDb() > 2.5); // the glue's 2.5 dB and the limiter's share of the rest
}
