// The sampler's resamplers: the 16-tap polyphase sinc (sinc_bank.h) and 4-point Hermite (hermite.h).
// - Aliasing: a 15 kHz sine read for 44.1, 88.2 and 96 kHz hosts (and 48 kHz) at -1.5, 0 and +1.5
//   semitones keeps everything that isn't the tone at or below -60 dB; at +5 semitones at or below -50 dB.
// - A 1 kHz sine played through a voice stays in tune within 1 cent.
// - Ratio 1 is a bit-exact copy; at a 96 kHz host every other sample is one.
// - Reads never leave the file: guard frames of NaN on both sides never reach the output.
// - Hermite equals fut01/basslines.hermite on the Python vectors within 1e-6.

#include "sampler_fixtures.h"

#include "sampler/hermite.h"
#include "sampler/sinc_bank.h"
#include "sampler/voice.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <array>
#include <cmath>
#include <limits>
#include <vector>

using namespace sampler_test;

namespace
{
    // What isn't the tone: y least-squares fitted with a cos (w n) + b sin (w n) + c over [from, size),
    // and the residual's RMS against the tone's RMS, in dB.
    double nonToneDb (const std::vector<double>& y, double w, std::size_t from)
    {
        double m[3][3] = {}, v[3] = {};
        for (std::size_t n = from; n < y.size(); ++n)
        {
            const double basis[3] { std::cos (w * static_cast<double> (n)), std::sin (w * static_cast<double> (n)), 1.0 };
            for (int i = 0; i < 3; ++i)
            {
                v[i] += basis[i] * y[n];
                for (int j = 0; j < 3; ++j)
                    m[i][j] += basis[i] * basis[j];
            }
        }
        // Gaussian elimination on the 3 x 3 normal equations.
        for (int c = 0; c < 3; ++c)
            for (int r = c + 1; r < 3; ++r)
            {
                const double f = m[r][c] / m[c][c];
                for (int k = c; k < 3; ++k)
                    m[r][k] -= f * m[c][k];
                v[r] -= f * v[c];
            }
        double x[3];
        for (int r = 2; r >= 0; --r)
        {
            double s = v[r];
            for (int k = r + 1; k < 3; ++k)
                s -= m[r][k] * x[k];
            x[r] = s / m[r][r];
        }
        double residual = 0.0;
        for (std::size_t n = from; n < y.size(); ++n)
        {
            const double fit = x[0] * std::cos (w * static_cast<double> (n)) + x[1] * std::sin (w * static_cast<double> (n)) + x[2];
            residual += (y[n] - fit) * (y[n] - fit);
        }
        const double count = static_cast<double> (y.size() - from);
        const double toneRms = std::sqrt (0.5 * (x[0] * x[0] + x[1] * x[1]));
        return 20.0 * std::log10 (std::sqrt (residual / count) / toneRms);
    }

    // The frequency of a steady tone, from its upward zero crossings (interpolated), in Hz at rate.
    double frequencyOf (const std::vector<float>& y, std::size_t from, double rate)
    {
        double first = -1.0, last = -1.0;
        int crossings = 0;
        for (std::size_t n = std::max<std::size_t> (from, 1); n < y.size(); ++n)
        {
            const double a = y[n - 1], b = y[n];
            if (a < 0.0 && b >= 0.0)
            {
                const double t = static_cast<double> (n - 1) + a / (a - b);
                if (first < 0.0)
                    first = t;
                last = t;
                ++crossings;
            }
        }
        REQUIRE (crossings > 10);
        return (crossings - 1) / (last - first) * rate;
    }

    // Renders a Voice on its own into left and right (fresh zeros), in pieces of 32.
    struct VoiceRun
    {
        std::vector<float> left, right, space, echo, lane;

        void run (Voice& v, int samples)
        {
            left.assign (static_cast<std::size_t> (samples), 0.0f);
            right = left;
            space = left;
            echo = left;
            lane.assign (static_cast<std::size_t> (samples), 1.0f);
            VoiceTarget t { left.data(), right.data(), space.data(), echo.data(), lane.data() };
            for (int i = 0; i < samples; i += 32)
                v.render (t, i, std::min (i + 32, samples));
        }
    };
}

TEST_CASE ("the sinc bank: unit gain, a copy at phase 0 of bank 0, cutoffs that follow the ratio", "[sampler][sinc]")
{
    const auto& bank = SincBank::instance();
    for (int b = 0; b < SincBank::kBanks; ++b)
    {
        const float* t = bank.table (b);
        for (int p = 0; p < SincBank::kRows; ++p)
        {
            double sum = 0.0;
            for (int j = 0; j < SincBank::kTaps; ++j)
                sum += t[p * SincBank::kTaps + j];
            CHECK (std::fabs (sum - 1.0) < 1.0e-6);
        }
    }
    const float* zero = bank.table (0);
    for (int j = 0; j < SincBank::kTaps; ++j)
        CHECK (same (zero[j], j == SincBank::kLeft ? 1.0f : 0.0f));

    CHECK (bank.bankFor (0.25) == 0);
    CHECK (bank.bankFor (1.0) == 0);
    CHECK (bank.bankFor (1.0001) == 1);
    CHECK (bank.bankFor (48000.0 / 44100.0) == 2); // 1.47 semitones: the cutoff of 2 semitones, just under 0.5 / ratio
    CHECK (bank.bankFor (std::exp2 (5.0 / 12.0)) == 5);
    CHECK (bank.bankFor (100.0) == SincBank::kBanks - 1);
    CHECK (same (SincBank::cutoff (12), 0.25));
    for (const double r : { 0.5, 1.0, 1.09, 1.3, 2.0, 7.9 })
    {
        const int b = bank.bankFor (r);
        CHECK (SincBank::cutoff (b) <= 0.5 / std::max (1.0, r) + 1.0e-12);
        CHECK (bank.follow (0, r) == b);
        CHECK (bank.follow (SincBank::kBanks - 1, r) == b);
    }
}

TEST_CASE ("a 15 kHz sine keeps its aliases at or below -60 dB at 44.1, 88.2 and 96 kHz and +-1.5 semitones", "[sampler][sinc]")
{
    const double host = GENERATE (44100.0, 48000.0, 88200.0, 96000.0);
    const double semitones = GENERATE (-1.5, 0.0, 1.5, 5.0);
    if (same (host, 48000.0) && same (semitones, 0.0))
        return; // a copy
    CAPTURE (host, semitones);

    const auto source = sine (40000, 15000.0, 0.5);
    const double ratio = 48000.0 / host * std::exp2 (semitones / 12.0);
    const auto& bank = SincBank::instance();
    const float* table = bank.table (bank.bankFor (ratio));
    std::vector<double> y (16384);
    double pos = 0.0;
    for (auto& s : y)
    {
        s = sincRead (table, source.data(), static_cast<std::int64_t> (source.size()), pos);
        pos += ratio;
    }
    const double w = 2.0 * kPi * 15000.0 / 48000.0 * ratio; // the tone, in radians per output sample
    const double db = nonToneDb (y, w, 64);
    CAPTURE (db);
    CHECK (db <= (semitones > 2.0 ? -50.0 : -60.0));
}

TEST_CASE ("a 1 kHz sine played through a voice stays in tune within 1 cent", "[sampler][sinc]")
{
    const double host = GENERATE (44100.0, 48000.0, 88200.0, 96000.0);
    const int key = GENERATE (60, 61, 67, 55);
    const auto interp = GENERATE (Interp::sinc, Interp::hermite);
    CAPTURE (host, key, interp);

    TestKit t;
    const int file = t.addFile (sine (48000, 1000.0, 0.5));
    auto inst = instrument (SampleMode::oneShot);
    inst.interp = interp;
    const int i = t.addSimple (inst, file, 60);

    Rig rig (host);
    rig.at (0, [&] (Sampler& s, int offset) { s.noteOn (t.kit, note (i, key, 127, offset)); });
    rig.runUntil (static_cast<std::int64_t> (host * 0.4));

    const double expected = 1000.0 * std::exp2 ((key - 60) / 12.0);
    const double measured = frequencyOf (rig.left[0], 100, host);
    const double cents = 1200.0 * std::log2 (measured / expected);
    CAPTURE (measured, cents);
    CHECK (std::fabs (cents) <= 1.0);
}

TEST_CASE ("ratio 1 is a bit-exact copy, and so is every other sample at a 96 kHz host", "[sampler][sinc]")
{
    std::vector<float> l (6000), r (6000);
    for (std::size_t k = 0; k < l.size(); ++k)
    {
        l[k] = static_cast<float> (std::sin (0.013 * static_cast<double> (k)) * 0.7 + 0.1 * std::sin (2.1 * static_cast<double> (k)));
        r[k] = static_cast<float> (std::cos (0.029 * static_cast<double> (k)) * 0.6);
    }
    TestKit t;
    const int file = t.addFile (l, r);
    const int sincInst = t.addSimple (instrument (SampleMode::oneShot), file, 60);
    auto h = instrument (SampleMode::oneShot, 1);
    h.interp = Interp::hermite;
    const int hermiteInst = t.addSimple (h, file, 60);

    SECTION ("48 kHz, the root key")
    {
        Rig rig (48000.0);
        rig.at (5, [&] (Sampler& s, int offset) {
            s.noteOn (t.kit, note (sincInst, 60, 127, offset));
            s.noteOn (t.kit, note (hermiteInst, 60, 127, offset));
        });
        rig.runUntil (6100);
        for (const int c : { 0, 1 })
        {
            CAPTURE (c);
            const std::vector<float> gotL (rig.left[c].begin() + 5, rig.left[c].begin() + 6005);
            const std::vector<float> gotR (rig.right[c].begin() + 5, rig.right[c].begin() + 6005);
            CHECK (sameBits (gotL, l));
            CHECK (sameBits (gotR, r));
            CHECK (silent (rig.left[c], 0, 5));
            CHECK (silent (rig.left[c], 6005)); // and it stops at the file's end
        }
    }

    SECTION ("96 kHz: the sinc's integer positions are the frames")
    {
        Rig rig (96000.0);
        rig.at (0, [&] (Sampler& s, int offset) { s.noteOn (t.kit, note (sincInst, 60, 127, offset)); });
        rig.runUntil (12000);
        for (std::size_t k = 0; k < l.size(); ++k)
        {
            if (! same (rig.left[0][2 * k], l[k]) || ! same (rig.right[0][2 * k], r[k]))
            {
                FAIL ("frame " << k << " isn't copied");
                break;
            }
        }
    }
}

TEST_CASE ("reads never leave the file", "[sampler][sinc]")
{
    // The file sits between NaN guards: a single read outside it would put a NaN in the output.
    constexpr std::size_t guard = 64;
    constexpr std::int64_t frames = 3000;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    std::vector<float> storage (guard + static_cast<std::size_t> (frames) + guard, nan);
    const auto body = sine (static_cast<std::size_t> (frames), 4321.0, 0.8);
    std::copy (body.begin(), body.end(), storage.begin() + guard);

    SampleView view;
    view.left = storage.data() + guard;
    view.right = view.left;
    view.frames = frames;
    view.channels = 1;
    view.loopStart = 2000;
    view.loopEnd = frames;

    const auto interp = GENERATE (Interp::sinc, Interp::hermite);
    const auto mode = GENERATE (SampleMode::oneShot, SampleMode::sustainLoop);
    const double base = GENERATE (48000.0 / 44100.0, 0.25, 4.0);
    const int semitones = GENERATE (-12, 0, 7, 19);
    const double start = GENERATE (0.0, 0.5, 2999.0, 2999.75);
    CAPTURE (interp, mode, base, semitones, start);

    VoiceStart s;
    s.sample = view;
    s.interp = interp;
    s.mode = mode;
    s.baseRatio = base;
    s.unitBase = false;
    s.semitones = semitones;
    s.startPosition = start;
    s.releaseSamples = 100.0;
    Voice v;
    v.start (s);
    v.glide (semitones + 5, 1.0, GlideCurve::raisedCosine, 300.0); // and a glide across banks
    VoiceRun out;
    out.run (v, 8000);
    bool finite = true;
    for (std::size_t n = 0; n < out.left.size(); ++n)
        finite = finite && std::isfinite (out.left[n]) && std::isfinite (out.right[n]);
    CHECK (finite);
}

TEST_CASE ("Hermite equals fut01/basslines.hermite on the Python vectors within 1e-6", "[sampler][hermite][vectors]")
{
    const auto a = readVector ("source_a.f32");
    const auto b = readVector ("source_b.f32");
    REQUIRE (a.size() == 4096);
    REQUIRE (b.size() == 4096);

    struct Case
    {
        const char* file;
        const std::vector<float>* source;
        std::int64_t loopStart, loopEnd;
        int n;
        bool sweep;
        double ratio;
    };
    const Case cases[] {
        { "hermite_up4.f32", &a, 1024, 3584, 3000, false, std::pow (2.0, 4.0 / 12.0) },
        { "hermite_down.f32", &a, 1024, 3584, 2000, false, 0.5 * std::pow (2.0, -3.0 / 12.0) },
        { "hermite_sweep.f32", &b, 512, 3000, 3000, true, 0.0 },
    };
    for (const auto& c : cases)
    {
        CAPTURE (c.file);
        const auto expected = readVector (c.file);
        REQUIRE (expected.size() == static_cast<std::size_t> (c.n));
        FrameMap map;
        map.frames = 4096;
        map.loopStart = c.loopStart;
        map.loopEnd = c.loopEnd;
        map.clampStart = true;
        double pos = 0.0, worst = 0.0;
        for (int j = 0; j < c.n; ++j)
        {
            const double got = hermiteRead (c.source->data(), map, pos);
            worst = std::max (worst, std::fabs (got - static_cast<double> (expected[static_cast<std::size_t> (j)])));
            const double ratio = c.sweep ? std::pow (2.0, 7.0 * j / (c.n - 1) / 12.0) : c.ratio;
            pos += ratio;
        }
        CAPTURE (worst);
        CHECK (worst <= 1.0e-6);
    }
}
