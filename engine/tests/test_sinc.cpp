// The sampler's resamplers: the 16-tap polyphase sinc (sinc_bank.h) and 4-point Hermite (hermite.h).
// - Aliasing: a 15 kHz sine read for 44.1, 88.2 and 96 kHz hosts (and 48 kHz) at -1.5, 0 and +1.5
//   semitones keeps everything that isn't the tone at or below -60 dB; at +5 semitones at or below -50 dB.
// - Hermite equals fut01/basslines.hermite on the Python vectors within 1e-6.

#include "sampler_fixtures.h"

#include "sampler/hermite.h"
#include "sampler/sinc_bank.h"

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
