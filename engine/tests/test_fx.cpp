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

// ------------------------------------------------------------------ properties at other rates

namespace
{
    constexpr double kPi = 3.141592653589793;

    // |X| at f Hz of x sampled at fs (a single DFT bin, any frequency), normalised so a sine of amplitude a gives a.
    double dftMagnitude (const std::vector<double>& x, double f, double fs, std::size_t from = 0)
    {
        double re = 0.0, im = 0.0;
        const double w = 2.0 * kPi * f / fs;
        for (std::size_t i = from; i < x.size(); ++i)
        {
            re += x[i] * std::cos (w * static_cast<double> (i - from));
            im -= x[i] * std::sin (w * static_cast<double> (i - from));
        }
        return 2.0 * std::sqrt (re * re + im * im) / static_cast<double> (x.size() - from);
    }

    // The energy decay curve (Schroeder's backward integral) of x, in dB; and the T60 its slope gives between -5 and
    // -35 dB.
    double t60Of (const std::vector<double>& x, double fs)
    {
        std::vector<double> edc (x.size());
        double acc = 0.0;
        for (std::size_t i = x.size(); i-- > 0;)
        {
            acc += x[i] * x[i];
            edc[i] = acc;
        }
        const double total = edc[0];
        double t5 = -1.0, t35 = -1.0;
        for (std::size_t i = 0; i < edc.size(); ++i)
        {
            const double db = 10.0 * std::log10 (edc[i] / total + 1e-300);
            if (t5 < 0.0 && db <= -5.0)
                t5 = static_cast<double> (i) / fs;
            if (t35 < 0.0 && db <= -35.0)
            {
                t35 = static_cast<double> (i) / fs;
                break;
            }
        }
        return 2.0 * (t35 - t5); // 30 dB in (t35 - t5): 60 dB in twice that
    }

    std::vector<double> bandOf (const std::vector<double>& x, double lo, double hi, double fs)
    {
        BiquadCascade<8, 1> f;
        f.setButterworth (PassKind::bandpass, 4, lo, hi, fs);
        std::vector<double> y (x.size());
        for (std::size_t i = 0; i < x.size(); ++i)
            y[i] = f.tick (0, x[i]);
        return y;
    }

    // The Space's impulse response, mid (L + R) / 2 and side (L - R) / 2.
    struct Response
    {
        std::vector<double> mid, side;
    };

    Response spaceResponse (const SpaceSettings& s, double fs, double seconds)
    {
        Space space;
        space.prepare (fs);
        space.setup (s);
        const auto n = static_cast<std::size_t> (seconds * fs);
        std::vector<float> in (n, 0.0f), l (n), r (n);
        in[0] = 1.0f;
        space.process (in.data(), l.data(), r.data(), static_cast<int> (n));
        Response out { std::vector<double> (n), std::vector<double> (n) };
        for (std::size_t i = 0; i < n; ++i)
        {
            out.mid[i] = 0.5 * (static_cast<double> (l[i]) + r[i]);
            out.side[i] = 0.5 * (static_cast<double> (l[i]) - r[i]);
        }
        return out;
    }

    double energy (const std::vector<double>& x)
    {
        double e = 0.0;
        for (const double v : x)
            e += v * v;
        return e;
    }
}

TEST_CASE ("fx: the Space's T60: its lines decay in t60 at every rate, its low band in t60_low", "[fx][rates]")
{
    // the core, undamped: the line gains give t60 (1-3 kHz) and t60_low (60-150 Hz) at any rate
    const double fs = GENERATE (44100.0, 48000.0, 96000.0);
    CAPTURE (fs);
    auto s = vectorSpace();
    s.dampHz = 24000.0;
    s.shimmer = 0.0;
    s.sendHpHz = 0.0;
    s.monoHz = 0.0;
    s.t60S = 2.6;
    s.t60LowS = 0.0;
    const double mid = t60Of (bandOf (spaceResponse (s, fs, 6.0).mid, 1000.0, 3000.0, fs), fs);
    INFO ("T60 1-3 kHz " << mid << " s for 2.6 s");
    CHECK (std::abs (mid / 2.6 - 1.0) <= 0.10);
    s.t60LowS = 0.6;
    const double low = t60Of (bandOf (spaceResponse (s, fs, 6.0).mid, 60.0, 150.0, fs), fs);
    INFO ("T60 60-150 Hz " << low << " s for t60_low 0.6 s");
    CHECK (std::abs (low / 0.6 - 1.0) <= 0.15); // the split is a one-pole at 500 Hz: a little of the long decay stays
}

TEST_CASE ("fx: the Space decays the same at 44.1 and 96 kHz as at 48 kHz, damping and low band included", "[fx][rates]")
{
    // sends.space's damping shortens the decay above its low band (0.2 dB a pass at 2 kHz for 8.5 kHz): at 48 kHz the
    // vectors prove it is sends.space's; at other rates the delays and coefficients scale so the decays stay put
    auto s = vectorSpace();
    s.shimmer = 0.0;
    s.sendHpHz = 0.0;
    s.monoHz = 0.0;
    auto bands = [&] (double fs) {
        const auto r = spaceResponse (s, fs, 6.0);
        return std::array<double, 2> { t60Of (bandOf (r.mid, 1000.0, 3000.0, fs), fs), t60Of (bandOf (r.mid, 60.0, 150.0, fs), fs) };
    };
    const auto ref = bands (48000.0);
    for (const double fs : { 44100.0, 96000.0 })
    {
        CAPTURE (fs);
        const auto t = bands (fs);
        INFO ("T60 1-3 kHz " << t[0] << " s (48 kHz: " << ref[0] << "), 60-150 Hz " << t[1] << " s (" << ref[1] << ")");
        CHECK (std::abs (t[0] / ref[0] - 1.0) <= 0.10);
        CHECK (std::abs (t[1] / ref[1] - 1.0) <= 0.10);
    }
}

TEST_CASE ("fx: the Space return is mono under 150 Hz", "[fx][rates]")
{
    const double fs = GENERATE (44100.0, 48000.0, 96000.0);
    CAPTURE (fs);
    auto s = vectorSpace();
    s.sendHpHz = 0.0;
    s.shimmer = 0.0;
    const auto r = spaceResponse (s, fs, 2.0);
    const double mid = energy (bandOf (r.mid, 40.0, 100.0, fs));
    const double side = energy (bandOf (r.side, 40.0, 100.0, fs));
    INFO ("side under the mid, 40-100 Hz: " << 10.0 * std::log10 (side / mid) << " dB");
    CHECK (side < mid * 1e-2); // 20 dB under, the samples' own rule (dsp48.mono_below, QA)
    // and with no fold the side is there
    s.monoHz = 0.0;
    const auto open = spaceResponse (s, fs, 2.0);
    CHECK (energy (bandOf (open.side, 40.0, 100.0, fs)) > 0.1 * energy (bandOf (open.mid, 40.0, 100.0, fs)));
}

TEST_CASE ("fx: the Space stays stable with the most shimmer", "[fx][rates]")
{
    const double fs = GENERATE (44100.0, 96000.0);
    CAPTURE (fs);
    auto s = vectorSpace();
    s.shimmer = 0.5; // the manifest's most
    s.t60S = 10.0;
    s.t60LowS = 0.0;
    Space space;
    space.prepare (fs);
    space.setup (s);
    const auto n = static_cast<std::size_t> (fs);
    std::vector<float> in (n), l (n), r (n);
    fxtest::SplitMix64 rng { 11 };
    std::vector<double> perSecond;
    for (int second = 0; second < 16; ++second)
    {
        for (auto& v : in)
            v = second == 0 ? static_cast<float> (rng.uniform() - 0.5) : 0.0f;
        space.process (in.data(), l.data(), r.data(), static_cast<int> (n));
        double e = 0.0;
        for (std::size_t i = 0; i < n; ++i)
            e += static_cast<double> (l[i]) * l[i] + static_cast<double> (r[i]) * r[i];
        perSecond.push_back (e);
    }
    for (std::size_t k = 2; k < perSecond.size(); ++k)
    {
        CAPTURE (k);
        CHECK (std::isfinite (perSecond[k]));
        CHECK (perSecond[k] < perSecond[k - 1]); // decaying, never growing
    }
    CHECK (perSecond.back() < perSecond[1] * 1e-3); // 15 s of a 10 s T60: well under 30 dB down
}

TEST_CASE ("fx: the echo's first repeat is 3 16ths late, the sides alternate, each repeat one feedback step quieter", "[fx][rates]")
{
    const double fs = GENERATE (44100.0, 96000.0);
    const double bpm = GENERATE (128.0, 134.0);
    CAPTURE (fs, bpm);
    EchoSettings s;
    s.wow = s.flutter = 0.0; // the delay exactly 3 16ths
    Echo echo;
    echo.prepare (fs);
    echo.setup (s);
    echo.setTempo (bpm);
    const double delay = 3.0 * 60.0 / (4.0 * bpm) * fs;
    const auto n = static_cast<std::size_t> (std::ceil (4.5 * delay));
    // a 20 ms 1 kHz burst (in the loop's pass band), raised-cosine edges
    std::vector<float> in (n, 0.0f), l (n), r (n);
    const auto burst = static_cast<std::size_t> (0.020 * fs);
    for (std::size_t i = 0; i < burst; ++i)
    {
        const double e = 0.5 - 0.5 * std::cos (2.0 * kPi * static_cast<double> (i) / static_cast<double> (burst));
        in[i] = static_cast<float> (0.1 * e * std::sin (2.0 * kPi * 1000.0 * static_cast<double> (i) / fs));
    }
    echo.process (in.data(), l.data(), r.data(), static_cast<int> (n));

    // the burst's first nonzero sample is its second (the raised cosine starts at 0): its first repeat starts 3 16ths
    // later, at 1 + delay (the tape reads it from the first output sample whose position passes 1 - 1)
    std::size_t first = 0;
    while (first < n && std::abs (l[first]) < 1e-12f)
        ++first;
    CHECK (first == static_cast<std::size_t> (std::floor (delay)) + 1);
    // repeat k sounds on the left for odd k, the right for even k: its energy in a window round k delays
    auto windowEnergy = [&] (const std::vector<float>& x, int k) {
        const auto from = static_cast<std::size_t> (k * delay);
        double e = 0.0;
        for (std::size_t i = from; i < std::min (n, from + static_cast<std::size_t> (0.6 * delay)); ++i)
            e += static_cast<double> (x[i]) * x[i];
        return e;
    };
    CHECK (windowEnergy (r, 1) < windowEnergy (l, 1) * 1e-6);
    CHECK (windowEnergy (l, 2) < windowEnergy (r, 2) * 1e-6);
    CHECK (windowEnergy (r, 3) < windowEnergy (l, 3) * 1e-6);
    const double step1 = 10.0 * std::log10 (windowEnergy (r, 2) / windowEnergy (l, 1));
    const double step2 = 10.0 * std::log10 (windowEnergy (l, 3) / windowEnergy (r, 2));
    const double feedbackDb = 20.0 * std::log10 (s.feedback);
    INFO ("steps " << step1 << " and " << step2 << " dB, feedback " << feedbackDb << " dB");
    CHECK (std::abs (step1 - feedbackDb) <= 0.5);
    CHECK (std::abs (step2 - feedbackDb) <= 0.5);
}

TEST_CASE ("fx: the resonator's peaks are within 5 cents of its tones", "[fx][rates]")
{
    const double fs = GENERATE (44100.0, 48000.0, 96000.0);
    CAPTURE (fs);
    ResonatorSettings s;
    const std::array<double, 5> tones { 739.989, 1108.731, 1318.51, 1661.219, 1760.0 };
    for (std::size_t i = 0; i < tones.size(); ++i)
        s.tonesHz[i] = tones[i];
    s.tones = 5;
    s.tauS = 0.4;
    Resonator res;
    res.prepare (fs);
    res.setup (s);
    const auto n = static_cast<std::size_t> (1.5 * fs);
    std::vector<float> in (n, 0.0f), l (n), r (n);
    in[0] = 1.0f;
    res.process (in.data(), l.data(), r.data(), static_cast<int> (n));
    std::vector<double> mono (n);
    for (std::size_t i = 0; i < n; ++i)
        mono[i] = static_cast<double> (l[i]) + r[i];
    for (const double tone : tones)
    {
        CAPTURE (tone);
        double best = 0.0, bestCents = 0.0;
        for (double cents = -15.0; cents <= 15.0; cents += 0.25)
        {
            const double m = dftMagnitude (mono, tone * std::pow (2.0, cents / 1200.0), fs);
            if (m > best)
            {
                best = m;
                bestCents = cents;
            }
        }
        CHECK (std::abs (bestCents) <= 5.0);
    }
}

namespace
{
    // |H (e^jw)| of a cascade's sections at f.
    double digitalGain (const BiquadCoeffs* c, int count, double f, double fs)
    {
        const std::complex<double> z1 = std::polar (1.0, -2.0 * kPi * f / fs);
        const std::complex<double> z2 = z1 * z1;
        std::complex<double> h (1.0, 0.0);
        for (int i = 0; i < count; ++i)
            h *= (c[i].b0 + c[i].b1 * z1 + c[i].b2 * z2) / (1.0 + c[i].a1 * z1 + c[i].a2 * z2);
        return std::abs (h);
    }

    // tan (pi f / fs): the bilinear transform's warped frequency, so a prototype at w / w0 is the digital response.
    double warped (double f, double fs) { return std::tan (kPi * f / fs); }

    double db (double g) { return 20.0 * std::log10 (g); }
}

TEST_CASE ("fx: biquads and the SVF are their analog prototypes through the bilinear transform at every rate", "[fx][rates]")
{
    // within 0.1 dB of the analog response at the warped frequency, up to 0.8 Nyquist: each rate gets the same
    // design, prewarped at its own cutoff (so the 48 kHz vectors' designs carry to 44.1 and 96 kHz)
    const double fs = GENERATE (44100.0, 96000.0);
    CAPTURE (fs);
    using C = std::complex<double>;
    for (double f = 20.0; f <= 0.8 * fs / 2.0; f *= 1.05)
    {
        CAPTURE (f);
        // Butterworth: |H|^2 = 1 / (1 + (w / wc)^2N), high-pass (wc / w)^2N
        struct Bw
        {
            PassKind kind;
            int order;
            double fc;
        };
        for (const Bw b : { Bw { PassKind::highpass, 4, 25.0 }, Bw { PassKind::lowpass, 2, 2600.0 }, Bw { PassKind::highpass, 8, 150.0 },
                 Bw { PassKind::highpass, 3, 450.0 } })
        {
            std::array<BiquadCoeffs, 4> c {};
            const int n = butterworth (b.kind, b.order, b.fc, 0.0, fs, c.data(), 4);
            double ratio = warped (f, fs) / warped (b.fc, fs);
            if (b.kind == PassKind::highpass)
                ratio = 1.0 / ratio;
            const double want = -10.0 * std::log10 (1.0 + std::pow (ratio, 2 * b.order));
            if (want > -80.0)
                CHECK (std::abs (db (digitalGain (c.data(), n, f, fs)) - want) <= 0.1);
        }
        // RBJ peak and shelves: the cookbook's analog prototypes at s = j w / w0
        struct Eq
        {
            RbjKind kind;
            double fc, gainDb, q;
        };
        for (const Eq e : { Eq { RbjKind::peak, 1000.0, -3.0, 1.2 }, Eq { RbjKind::peak, 16000.0, -2.5, 0.8 },
                 Eq { RbjKind::lowShelf, 200.0, 4.0, 0.707 }, Eq { RbjKind::highShelf, 8000.0, -3.0, 0.707 } })
        {
            const auto c = rbj (e.kind, e.fc, e.gainDb, e.q, fs);
            const C s (0.0, warped (f, fs) / warped (e.fc, fs));
            const double A = std::pow (10.0, e.gainDb / 40.0);
            C h;
            if (e.kind == RbjKind::peak)
                h = (s * s + s * (A / e.q) + 1.0) / (s * s + s / (A * e.q) + 1.0);
            else if (e.kind == RbjKind::lowShelf)
                h = A * (s * s + std::sqrt (A) / e.q * s + A) / (A * s * s + std::sqrt (A) / e.q * s + 1.0);
            else
                h = A * (A * s * s + std::sqrt (A) / e.q * s + 1.0) / (s * s + std::sqrt (A) / e.q * s + A);
            CHECK (std::abs (db (digitalGain (&c, 1, f, fs)) - db (std::abs (h))) <= 0.1);
        }
        // the SVF's low-pass: 1 / (s^2 + s / Q + 1); measured by running a sine through it
    }
    for (const double f : { 200.0, 1000.0, 5000.0, 0.4 * fs })
    {
        CAPTURE (f);
        const double fc = 1500.0, q = 1.6;
        Svf svf;
        const auto c = SvfCoeffs::make (fc, q, fs);
        std::vector<double> y;
        const auto n = static_cast<std::size_t> (fs / 2.0);
        for (std::size_t i = 0; i < n; ++i)
            y.push_back (svf.tick (c, std::sin (2.0 * kPi * f * static_cast<double> (i) / fs)).lp);
        const C s (0.0, warped (f, fs) / warped (fc, fs));
        const double want = db (std::abs (1.0 / (s * s + s / q + 1.0)));
        CHECK (std::abs (db (dftMagnitude (y, f, fs, n / 2)) - want) <= 0.1);
    }
}

TEST_CASE ("fx: the soft clip's curve never goes over its ceiling", "[fx]")
{
    SoftClip curve;
    curve.setCurve (-1.0, 1.5);
    const double ceiling = std::pow (10.0, -1.0 / 20.0);

    // the static curve: u plus its excess is at most the ceiling, however hot u is
    double worst = 0.0;
    for (double u = -50.0; u <= 50.0; u += 0.001)
        worst = std::max (worst, std::abs (u + curve.excess (u)));
    CHECK (worst <= ceiling * (1.0 + 1e-12)); // T + k tanh (...) reaches T + k = the ceiling, to rounding
    CHECK (worst > 0.999 * ceiling);

    // master.soft_clip runs the ADAA on the excess only, u + its mean over [u1, u], so a step at 4x can carry the
    // output past the ceiling by at most half the step: on a 4x stream from the upsampler that is nothing for bass and
    // kicks (0.02 dB for 100 Hz at +6 dBFS), more for hot highs (1.5 dB for 5 kHz), as in master.soft_clip itself
    fxtest::SplitMix64 rng { 5 };
    double prev = 0.0, overStep = 0.0;
    curve.reset();
    for (int i = 0; i < 100000; ++i)
    {
        const double u = (rng.uniform() - 0.5) * (i % 3 == 0 ? 20.0 : 2.5);
        const double y = curve.tick (u);
        overStep = std::max (overStep, std::abs (y) - ceiling - 0.5 * std::abs (u - prev));
        prev = u;
    }
    CHECK (overStep <= 1e-12);
    for (const double f : { 100.0, 1000.0, 5000.0 })
    {
        CAPTURE (f);
        ExactUpsampler4 up;
        curve.reset();
        double peak = 0.0, step = 0.0, last = 0.0;
        for (int i = 0; i < 48000; ++i)
        {
            std::array<double, 4> u {};
            up.push (2.0 * std::sin (2.0 * kPi * f * i / 48000.0), u.data());
            for (const double v : u)
            {
                peak = std::max (peak, std::abs (curve.tick (v)));
                step = std::max (step, std::abs (v - last));
                last = v;
            }
        }
        INFO ("the 4x curve's peak over the ceiling " << db (peak / ceiling) << " dB, half its largest step " << 0.5 * step);
        CHECK (peak <= ceiling + 0.5 * step);
        if (f <= 100.0)
            CHECK (db (peak / ceiling) <= 0.05);
    }
}

TEST_CASE ("fx: decimating a hard-clipped tone overshoots the ceiling the way master.soft_clip does", "[fx]")
{
    // band-limiting a clipped tone brings back its lost harmonics' ripple (a clipped 5 kHz tone keeps its fundamental,
    // 4 / pi of a square's height, and its 15 kHz): the limiter after the club master's clip holds the ceiling. The
    // lane and master clips overshoot alike, as master.soft_clip does (the 48 kHz vectors).
    const double ceiling = std::pow (10.0, -1.0 / 20.0);
    for (const double f : { 100.0, 1000.0, 5000.0 })
    {
        CAPTURE (f);
        LaneClip lane;
        MasterClip master;
        lane.setCurve (-1.0, 1.5);
        master.setCurve (-1.0, 1.5);
        lane.reset();
        master.reset();
        std::vector<float> a (48000), b (48000), c2 (48000), d (48000);
        for (std::size_t i = 0; i < a.size(); ++i)
            a[i] = b[i] = c2[i] = d[i] = static_cast<float> (2.0 * std::sin (2.0 * kPi * f * static_cast<double> (i) / 48000.0));
        lane.process (a.data(), b.data(), 48000);
        master.process (c2.data(), d.data(), 48000);
        float lanePeak = 0.0f, masterPeak = 0.0f;
        for (std::size_t i = 1000; i < a.size(); ++i)
        {
            lanePeak = std::max (lanePeak, std::abs (a[i]));
            masterPeak = std::max (masterPeak, std::abs (c2[i]));
        }
        INFO ("over the ceiling: lane " << db (lanePeak / ceiling) << " dB, master " << db (masterPeak / ceiling) << " dB");
        CHECK (db (lanePeak / ceiling) <= 2.1); // 4 / pi (2.1 dB) at most
        CHECK (std::abs (db (lanePeak / ceiling) - db (masterPeak / ceiling)) <= 0.1);
    }
}

TEST_CASE ("fx: the half-band lane clip on a 5 kHz sine 6 dB over the knee leaves aliases at or under -60 dB", "[fx]")
{
    LaneClip clip;
    clip.setCurve (0.0, 1.5);
    clip.reset();
    const double knee = std::pow (10.0, -1.5 / 20.0);
    const double amp = knee * std::pow (10.0, 6.0 / 20.0);
    const std::size_t n = 48000 + 4800;
    std::vector<float> l (n), r (n);
    for (std::size_t i = 0; i < n; ++i)
        l[i] = r[i] = static_cast<float> (amp * std::sin (2.0 * kPi * 5000.0 * static_cast<double> (i) / 48000.0));
    clip.process (l.data(), r.data(), static_cast<int> (n));
    // a second of output, whole periods of 5 kHz and of every alias: the aliases of the odd harmonics land on odd
    // kHz (25 kHz on 23, 35 on 13, 45 on 3...), the harmonics themselves on 5 and 15
    std::vector<double> y (l.begin() + 4800, l.end());
    double worst = -300.0, worstAt = 0.0;
    for (int k = 1; k <= 20; ++k)
    {
        if (k == 5 || k == 15)
            continue;
        const double m = db (dftMagnitude (y, 1000.0 * k, 48000.0) + 1e-300);
        if (m > worst)
        {
            worst = m;
            worstAt = 1000.0 * k;
        }
    }
    INFO ("the loudest alias under 20 kHz: " << worst << " dBFS at " << worstAt << " Hz");
    CHECK (worst <= -60.0);
    CHECK (db (dftMagnitude (y, 5000.0, 48000.0)) > -2.0); // the tone is there
}
