// P8's effect blocks (engine/fx) against the Python they port, at 48 kHz, through the vectors
// tools/sound/export_engine_vectors.py writes (ADR 0021): within 1e-6 for the linear blocks and 1e-4 for the
// nonlinear ones, as the largest absolute error on inputs peaking at -6 dBFS. Then their properties at other rates.

#include "fx_vectors.h"

#include "fx/band_split.h"
#include "fx/biquad.h"
#include "fx/duck.h"
#include "fx/echo.h"
#include "fx/level_detector.h"
#include "fx/oversampled.h"
#include "fx/resonator.h"
#include "fx/space.h"
#include "fx/stereo.h"
#include "fx/svf.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <numbers>
#include <vector>

using namespace wubansia::engine::fx;
using fxtest::channel;
using fxtest::load;
using fxtest::maxAbsDiff;

namespace
{
    constexpr double kFs = 48000.0;
    constexpr double kLinear = 1e-6;
    constexpr double kNonlinear = 1e-4;

    // Every k-th of x from `from` (a vector's decimation).
    std::vector<float> every (const std::vector<float>& x, std::size_t k, std::size_t from = 0, std::size_t channels = 1)
    {
        std::vector<float> out;
        for (std::size_t i = from; i * channels < x.size(); i += k)
            for (std::size_t c = 0; c < channels; ++c)
                out.push_back (x[i * channels + c]);
        return out;
    }

    std::vector<float> interleave (const std::vector<float>& l, const std::vector<float>& r)
    {
        std::vector<float> out (2 * l.size());
        for (std::size_t i = 0; i < l.size(); ++i)
        {
            out[2 * i] = l[i];
            out[2 * i + 1] = r[i];
        }
        return out;
    }

    template <class Filter>
    std::vector<float> runMono (Filter& f, std::vector<float> x)
    {
        f.process (0, x.data(), static_cast<int> (x.size()));
        return x;
    }
}

TEST_CASE ("fx vectors: Butterworth sections are scipy's butter through sosfilt", "[fx][vectors]")
{
    const auto a0 = channel (fxtest::inputA(), 2, 0);
    struct Case
    {
        const char* name;
        PassKind kind;
        int order;
        double f1, f2;
    };
    const Case cases[] {
        { "bw_hp4_25", PassKind::highpass, 4, 25.0, 0.0 },
        { "bw_hp2_5", PassKind::highpass, 2, 5.0, 0.0 },
        { "bw_lp2_2600", PassKind::lowpass, 2, 2600.0, 0.0 },
        { "bw_hp8_150", PassKind::highpass, 8, 150.0, 0.0 },
        { "bw_hp3_450", PassKind::highpass, 3, 450.0, 0.0 },
        { "bw_lp1_9000", PassKind::lowpass, 1, 9000.0, 0.0 },
        { "bw_bp4_300_7000", PassKind::bandpass, 4, 300.0, 7000.0 },
    };
    for (const auto& c : cases)
    {
        CAPTURE (c.name);
        BiquadCascade<8, 1> f;
        f.setButterworth (c.kind, c.order, c.f1, c.f2, kFs);
        CHECK (f.sections() == (c.kind == PassKind::bandpass ? c.order : (c.order + 1) / 2));
        CHECK (maxAbsDiff (runMono (f, a0), load (c.name)) <= kLinear);
    }
}

TEST_CASE ("fx vectors: RBJ biquads are the cookbook's (dsp48.peaking for the peak)", "[fx][vectors]")
{
    const auto a0 = channel (fxtest::inputA(), 2, 0);
    struct Case
    {
        const char* name;
        RbjKind kind;
        double fc, gainDb, q;
    };
    const Case cases[] {
        { "rbj_peak_1000", RbjKind::peak, 1000.0, -3.0, 1.2 },
        { "rbj_peak_16000", RbjKind::peak, 16000.0, -2.5, 0.8 },
        { "rbj_lowshelf_200", RbjKind::lowShelf, 200.0, 4.0, 0.707 },
        { "rbj_highshelf_8000", RbjKind::highShelf, 8000.0, -3.0, 0.707 },
        { "rbj_lp_1200", RbjKind::lowpass, 1200.0, 0.0, 2.0 },
        { "rbj_hp_300", RbjKind::highpass, 300.0, 0.0, 0.9 },
    };
    for (const auto& c : cases)
    {
        CAPTURE (c.name);
        BiquadCascade<1, 1> f;
        const auto coeffs = rbj (c.kind, c.fc, c.gainDb, c.q, kFs);
        f.setSections (&coeffs, 1);
        CHECK (maxAbsDiff (runMono (f, a0), load (c.name)) <= kLinear);
    }
}

TEST_CASE ("fx vectors: LR4 crossovers and the insert band split", "[fx][vectors]")
{
    const auto a0 = channel (fxtest::inputA(), 2, 0);
    for (const auto kind : { PassKind::lowpass, PassKind::highpass })
    {
        BiquadCascade<2, 1> f;
        f.setRepeated (lr4Section (kind, 1200.0, kFs), 2);
        CHECK (maxAbsDiff (runMono (f, a0), load (kind == PassKind::lowpass ? "lr4_lo_1200" : "lr4_hi_1200")) <= kLinear);
    }

    BandSplit split;
    split.setup (1200.0, 6500.0, kFs);
    std::vector<float> rest (a0.size()), band (a0.size());
    for (std::size_t i = 0; i < a0.size(); ++i)
    {
        double r, b;
        split.split (0, static_cast<double> (a0[i]), r, b);
        rest[i] = static_cast<float> (r);
        band[i] = static_cast<float> (b);
    }
    CHECK (maxAbsDiff (rest, load ("band_rest_1200_6500")) <= kLinear);
    CHECK (maxAbsDiff (band, load ("band_band_1200_6500")) <= kLinear);
}

TEST_CASE ("fx vectors: a duck on a band adds (g - 1) of the band (rt_ref.band_duck)", "[fx][vectors]")
{
    const auto a = fxtest::inputA();
    BandPart part;
    part.setup (120.0, 2500.0, kFs);
    OnsetEnvelope env;
    env.setShape (0.002, 0.060, 0.080, kFs);
    std::vector<float> out (a.size());
    for (std::size_t i = 0; i < a.size() / 2; ++i)
    {
        if (i == 100 || i == 600)
            env.trigger();
        const double g = std::pow (10.0, -6.0 * env.tick() / 20.0);
        for (std::size_t c = 0; c < 2; ++c)
        {
            const double x = static_cast<double> (a[2 * i + c]);
            out[2 * i + c] = static_cast<float> (x + (g - 1.0) * part.tick (static_cast<int> (c), x));
        }
    }
    CHECK (maxAbsDiff (out, load ("band_duck_120_2500")) <= kLinear);
}

TEST_CASE ("fx vectors: mono_below and the width insert's side filter", "[fx][vectors]")
{
    const auto a = fxtest::inputA();
    auto l = channel (a, 2, 0);
    auto r = channel (a, 2, 1);
    {
        SideFilter mono;
        mono.setup (SideFilter::Mode::monoBelow, 150.0, 8, 1.0, kFs);
        auto ml = l, mr = r;
        mono.process (ml.data(), mr.data(), static_cast<int> (ml.size()));
        CHECK (maxAbsDiff (interleave (ml, mr), load ("mono_below_150")) <= kLinear);
    }
    {
        SideFilter width;
        width.setup (SideFilter::Mode::width, 300.0, 4, 1.6, kFs);
        width.process (l.data(), r.data(), static_cast<int> (l.size()));
        CHECK (maxAbsDiff (interleave (l, r), load ("width_300_1600")) <= kLinear);
    }
}

TEST_CASE ("fx vectors: the TPT SVF with a cutoff every sample", "[fx][vectors]")
{
    const auto b0 = channel (fxtest::inputB(), 2, 0);
    const auto n = b0.size();
    Svf svf;
    std::vector<float> lp (n), bp (n), hp (n);
    for (std::size_t i = 0; i < n; ++i)
    {
        const double fc = 100.0 * std::pow (100.0, static_cast<double> (i) / static_cast<double> (n - 1));
        const auto o = svf.tick (SvfCoeffs::make (fc, 2.0, kFs), static_cast<double> (b0[i]));
        lp[i] = static_cast<float> (o.lp);
        bp[i] = static_cast<float> (o.bp);
        hp[i] = static_cast<float> (o.hp);
    }
    CHECK (maxAbsDiff (lp, load ("svf_lp")) <= kLinear);
    CHECK (maxAbsDiff (bp, load ("svf_bp")) <= kLinear);
    CHECK (maxAbsDiff (hp, load ("svf_hp")) <= kLinear);
}

TEST_CASE ("fx vectors: os_fir(4) up and down, each 24 samples late", "[fx][vectors]")
{
    const auto a = fxtest::inputA();
    const auto a0 = channel (a, 2, 0);
    {
        ExactUpsampler4 up;
        std::vector<float> out;
        for (std::size_t i = 0; i < 256 + 24; ++i)
        {
            std::array<double, 4> u {};
            up.push (i < 256 ? static_cast<double> (a0[i]) : 0.0, u.data());
            for (const double v : u)
                out.push_back (static_cast<float> (v));
        }
        const std::vector<float> aligned (out.begin() + kExactUpLatency4x, out.begin() + kExactUpLatency4x + 1024);
        CHECK (maxAbsDiff (aligned, load ("os_up")) <= kLinear);
    }
    {
        const auto z = channel (a, 2, 1); // as a 4x stream
        ExactDownsampler4 down;
        std::vector<float> out;
        for (std::size_t i = 0; i < 1024 + 96; i += 4)
        {
            std::array<double, 4> in {};
            for (std::size_t j = 0; j < 4; ++j)
                in[j] = i + j < z.size() ? static_cast<double> (z[i + j]) : 0.0;
            out.push_back (static_cast<float> (down.push (in.data())));
        }
        const std::vector<float> aligned (out.begin() + 24, out.begin() + 24 + 256);
        CHECK (maxAbsDiff (aligned, load ("os_down")) <= kLinear);
    }
}

namespace
{
    // Runs a stereo stage of latency `delay` over x (interleaved), then `delay` samples of silence; the aligned output.
    template <class Stage>
    std::vector<float> runAligned (Stage& stage, const std::vector<float>& x, int delay)
    {
        auto l = channel (x, 2, 0);
        auto r = channel (x, 2, 1);
        const std::size_t n = l.size();
        l.resize (n + static_cast<std::size_t> (delay), 0.0f);
        r.resize (n + static_cast<std::size_t> (delay), 0.0f);
        stage.process (l.data(), r.data(), static_cast<int> (l.size()));
        l.erase (l.begin(), l.begin() + delay);
        r.erase (r.begin(), r.begin() + delay);
        return interleave (l, r);
    }

    std::vector<float> scaled (std::vector<float> x, float k)
    {
        for (auto& v : x)
            v *= k;
        return x;
    }
}

TEST_CASE ("fx vectors: master.soft_clip on os_fir(4), 48 samples late", "[fx][vectors]")
{
    MasterClip clip;
    clip.setCurve (-1.0, 1.5);
    clip.reset();
    const auto out = runAligned (clip, scaled (fxtest::inputA(), 3.0f), MasterClip::kLatency);
    CHECK (MasterClip::kLatency == 48);
    CHECK (maxAbsDiff (out, load ("soft_clip_master")) <= kNonlinear);
}

TEST_CASE ("fx vectors: tanh_adaa at 1x", "[fx][vectors]")
{
    const auto a0 = channel (fxtest::inputA(), 2, 0);
    TanhDrive drive;
    drive.setCurve (2.5, 0.2);
    std::vector<float> out (a0.size());
    for (std::size_t i = 0; i < a0.size(); ++i)
        out[i] = static_cast<float> (drive.tick (static_cast<double> (a0[i]) * 2.0));
    CHECK (maxAbsDiff (out, load ("tanh_adaa")) <= kNonlinear);
}

TEST_CASE ("fx vectors: the clip and drive inserts on the half-band cascade, 27 samples late", "[fx][vectors]")
{
    CHECK (LaneClip::kLatency == 27);
    CHECK (LaneDrive::kLatency == 27);
    {
        LaneClip clip;
        clip.setCurve (-1.0, 1.5);
        clip.reset();
        CHECK (maxAbsDiff (runAligned (clip, scaled (fxtest::inputA(), 3.0f), LaneClip::kLatency), load ("lane_clip")) <= kNonlinear);
    }
    {
        LaneDrive drive;
        drive.setCurve (2.5, 0.2);
        drive.setOutputGain (std::pow (10.0, -1.5 / 20.0));
        const auto a0 = scaled (channel (fxtest::inputA(), 2, 0), 1.5f);
        const auto out = channel (runAligned (drive, interleave (a0, a0), LaneDrive::kLatency), 2, 0);
        CHECK (maxAbsDiff (out, load ("lane_drive")) <= kNonlinear);
    }
}

TEST_CASE ("fx vectors: phrase.pump_db as the onset envelope, and a lift", "[fx][vectors]")
{
    {
        OnsetEnvelope env;
        env.setShape (0.002, 0.060, 0.080, kFs);
        const std::array<int, 4> onsets { 1000, 9000, 9500, 17000 };
        std::vector<float> db;
        std::size_t next = 0;
        for (int t = 0; t < 24000; ++t)
        {
            if (next < onsets.size() && onsets[next] == t)
            {
                env.trigger();
                ++next;
            }
            db.push_back (static_cast<float> (-6.0 * env.tick()));
        }
        CHECK (maxAbsDiff (every (db, 8), load ("pump_db")) <= 1e-5); // dB, stored as float32
    }
    {
        OnsetEnvelope env;
        env.setShape (0.0, 0.012, 0.013, kFs);
        std::vector<float> db;
        for (int t = 0; t < 6000; ++t)
        {
            if (t == 500 || t == 3000)
                env.trigger();
            db.push_back (static_cast<float> (3.0 * env.tick()));
        }
        const auto want = load ("pump_lift_db");
        CHECK (maxAbsDiff (every (db, 4), want) <= 1e-5);
        CHECK (*std::max_element (want.begin(), want.end()) > 2.99f); // it lifts
    }
}

TEST_CASE ("fx vectors: sends.duck with a fixed top, 96 samples later", "[fx][vectors]")
{
    // export_engine_vectors.level_key: input E scaled so its loudest 64-sample block is -12 dB
    const auto k = fxtest::music (28800, 6, { { 3000, 2000, 2.0 }, { 14000, 4000, 1.0 } }, 0.5, 1.0, 2, 192);
    std::vector<double> mono (k.size() / 2);
    for (std::size_t i = 0; i < mono.size(); ++i)
        mono[i] = (static_cast<double> (k[2 * i]) + static_cast<double> (k[2 * i + 1])) / 2.0;
    double top = 0.0;
    for (std::size_t b = 0; b < mono.size(); b += 64)
    {
        double p = 0.0;
        for (std::size_t i = b; i < std::min (b + 64, mono.size()); ++i)
            p += mono[i] * mono[i];
        top = std::max (top, p / 64.0);
    }
    const double scale = std::pow (10.0, (-12.0 - 10.0 * std::log10 (top + 1e-20)) / 20.0);

    LevelDetector det;
    det.setup (18.0, -12.0, 0.010, 0.300, kFs);
    CHECK (det.lagSamples() == 96);
    std::vector<float> db (mono.size());
    for (std::size_t i = 0; i < mono.size(); ++i)
        db[i] = static_cast<float> (-6.0 * det.tick (mono[i] * scale));
    const auto want = load ("duck_level_db");
    std::vector<float> got;
    for (std::size_t t = 0; t < want.size(); ++t)
    {
        const std::size_t at = 16 * t + 96;
        if (at < db.size())
            got.push_back (db[at]);
    }
    REQUIRE (got.size() > want.size() / 2);
    CHECK (maxAbsDiff (got, want) <= kNonlinear);
    CHECK (*std::min_element (want.begin(), want.end()) < -3.0f); // it ducks
}

namespace
{
    // export_engine_vectors.SPACE, with the delays and signs sends.space drew from rng ('P8', 'space') at size 0.45
    SpaceSettings vectorSpace()
    {
        SpaceSettings s;
        s.delaysFrames = { 512, 585, 694, 767, 895, 1057, 1199, 1348 };
        s.signs = { 1, -1, 1, -1, -1, -1, -1, -1 };
        s.t60S = 2.6;
        s.t60LowS = 0.4;
        s.lowHz = 500.0;
        s.dampHz = 8500.0;
        s.predelayS = 0.002;
        s.sendHpHz = 500.0;
        s.sendOrder = 4;
        s.shimmer = 0.06;
        s.shimmerLoHz = 300.0;
        s.shimmerHiHz = 7000.0;
        s.windowFrames = 1024;
        s.monoHz = 150.0;
        return s;
    }

    template <class Return>
    std::vector<float> runReturn (Return& ret, const std::vector<float>& in)
    {
        std::vector<float> l (in.size()), r (in.size());
        ret.process (in.data(), l.data(), r.data(), static_cast<int> (in.size()));
        return interleave (l, r);
    }
}

TEST_CASE ("fx vectors: sends.space", "[fx][vectors]")
{
    const auto e = fxtest::music (8192, 7, { { 0, 400, 1.0 } }, 0.5, 0.0, 1);
    Space space;
    space.prepare (kFs);
    space.setup (vectorSpace());
    const auto out = runReturn (space, e);
    CHECK (maxAbsDiff (every (out, 4, 0, 2), load ("space")) <= kLinear);
}

TEST_CASE ("fx vectors: sends.echo", "[fx][vectors]")
{
    const auto f = fxtest::music (7200, 8, { { 0, 300, 1.0 } }, 0.5, 0.0, 1);
    EchoSettings s;
    s.steps = 3;
    s.wowPhase = 1.234;
    s.flutterPhase = 5.678;
    Echo echo;
    echo.prepare (kFs);
    echo.setup (s);
    echo.setTempo (1440.0); // 3 16ths: 0.03125 s, 1500 samples
    const auto out = runReturn (echo, f);
    CHECK (maxAbsDiff (every (out, 4, 0, 2), load ("echo")) <= kNonlinear);
}

TEST_CASE ("fx vectors: sends.resonator", "[fx][vectors]")
{
    auto x = channel (fxtest::inputA(), 2, 0);
    x.resize (4096, 0.0f);
    ResonatorSettings s;
    const std::array<int, 5> mhz { 739989, 1108731, 1318510, 1661219, 1760000 };
    for (std::size_t i = 0; i < mhz.size(); ++i)
        s.tonesHz[i] = mhz[i] / 1000.0;
    s.tones = 5;
    s.tauS = 0.4;
    s.sendHpHz = 600.0;
    Resonator res;
    res.prepare (kFs);
    res.setup (s);
    const auto out = runReturn (res, x);
    CHECK (maxAbsDiff (every (out, 2, 0, 2), load ("resonator")) <= kLinear);
}
