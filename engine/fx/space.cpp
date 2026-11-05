#include "fx/space.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace wubansia::engine::fx
{
    namespace
    {
        constexpr std::array<double, 8> kOutL { 1, -1, 1, 1, -1, 1, -1, -1 }; // sends.OUT_L
        constexpr std::array<double, 8> kOutR { 1, 1, -1, -1, 1, 1, -1, -1 }; // sends.OUT_R
        constexpr double kInvSqrt8 = 0.35355339059327373; // 1 / sqrt (8)

        // v = H v, H the 8 x 8 Sylvester Hadamard matrix (scipy.linalg.hadamard (8)), unscaled.
        void hadamard8 (std::array<double, 8>& v) noexcept
        {
            for (std::size_t h = 1; h < 8; h <<= 1)
                for (std::size_t i = 0; i < 8; i += 2 * h)
                    for (std::size_t j = i; j < i + h; ++j)
                    {
                        const double a = v[j];
                        const double b = v[j + h];
                        v[j] = a + b;
                        v[j + h] = a - b;
                    }
        }

        int scaled (int frames48k, double fs) noexcept
        {
            return static_cast<int> (std::nearbyint (static_cast<double> (frames48k) * fs / 48000.0));
        }
    }

    void Space::prepare (double sampleRate)
    {
        fs = sampleRate;
        pre.prepare (static_cast<std::size_t> (std::ceil (kMaxPredelayS * fs)) + 2);
        for (auto& line : lines)
            line.prepare (static_cast<std::size_t> (scaled (kMaxDelayFrames, fs)) + 2);
        for (auto& h : hist)
            h.prepare (static_cast<std::size_t> (scaled (kMaxWindowFrames, fs)) + 2);
        setup (s);
    }

    void Space::setup (const SpaceSettings& settings) noexcept
    {
        s = settings;
        const int maxD = static_cast<int> (lines[0].capacity());
        for (std::size_t i = 0; i < kLines; ++i)
        {
            d[i] = std::clamp (scaled (s.delaysFrames[i], fs), 1, std::max (maxD, 1));
            const double di = static_cast<double> (d[i]);
            g[i] = std::pow (10.0, -3.0 * di / (std::max (s.t60S, 1e-3) * fs));
            const double gLow = s.t60LowS > 0.0 ? std::pow (10.0, -3.0 * di / (s.t60LowS * fs)) : g[i];
            lowMix[i] = gLow / g[i] - 1.0;
            inj[i] = static_cast<double> (s.signs[i] < 0 ? -1 : 1) * kInvSqrt8;
        }
        lowBand = s.t60LowS > 0.0;
        dampA = std::exp (-2.0 * std::numbers::pi * s.dampHz / fs);
        lowA = std::exp (-2.0 * std::numbers::pi * s.lowHz / fs);
        predelay = std::clamp (static_cast<int> (std::nearbyint (s.predelayS * fs)), 0, std::max (static_cast<int> (pre.capacity()) - 1, 0));
        const int maxW = std::max (static_cast<int> (hist[0].capacity()) - 1, 2);
        window = std::clamp (scaled (s.windowFrames, fs), 2, maxW);
        window -= window % 2;
        shimmerDry = std::sqrt (std::max (1.0 - s.shimmer * s.shimmer, 0.0));

        if (s.sendHpHz > 0.0)
            sendHp.setButterworth (PassKind::highpass, std::clamp (s.sendOrder, 1, 8), s.sendHpHz, 0.0, fs);
        else
            sendHp.setSections (nullptr, 0);
        shimmerBand.setButterworth (PassKind::bandpass, 4, s.shimmerLoHz, s.shimmerHiHz, fs);
        if (s.monoHz > 0.0)
            monoSide.setButterworth (PassKind::highpass, 8, s.monoHz, 0.0, fs);
        else
            monoSide.setSections (nullptr, 0);
        reset();
    }

    void Space::reset() noexcept
    {
        sendHp.reset();
        pre.reset();
        for (auto& line : lines)
            line.reset();
        for (auto& h : hist)
            h.reset();
        damp.fill (0.0);
        low.fill (0.0);
        shimmerBand.reset();
        monoSide.reset();
        t = 0;
    }

    void Space::process (const float* in, float* outL, float* outR, int n) noexcept
    {
        const bool shimmer = s.shimmer > 0.0;
        const bool mono = monoSide.sections() > 0;
        const double w = static_cast<double> (window);
        for (int i = 0; i < n; ++i)
        {
            pre.push (static_cast<float> (sendHp.tick (0, static_cast<double> (in[i]))));
            const double xin = static_cast<double> (pre.delayed (predelay));

            // the shifter's two taps at this absolute time, shared by the lines
            std::array<std::int64_t, 2> dk {};
            std::array<double, 2> wk {};
            if (shimmer)
                for (std::size_t o = 0; o < 2; ++o)
                {
                    const std::int64_t off = o == 0 ? 0 : window / 2;
                    dk[o] = window - (t + off) % window;
                    const double sn = std::sin (std::numbers::pi * static_cast<double> (dk[o]) / w);
                    wk[o] = sn * sn;
                }

            std::array<double, kLines> v {};
            for (std::size_t j = 0; j < kLines; ++j)
            {
                const double tap = static_cast<double> (lines[j].at (t - d[j]));
                damp[j] = (1.0 - dampA) * tap + dampA * damp[j];
                double x = damp[j];
                if (lowBand)
                {
                    low[j] = (1.0 - lowA) * x + lowA * low[j];
                    x = x + lowMix[j] * low[j];
                }
                if (shimmer)
                {
                    const double h = shimmerBand.tick (static_cast<int> (j), x);
                    hist[j].push (static_cast<float> (h));
                    const double upOct = wk[0] * static_cast<double> (hist[j].at (t - dk[0]))
                                         + wk[1] * static_cast<double> (hist[j].at (t - dk[1]));
                    x = shimmerDry * x + s.shimmer * upOct;
                }
                v[j] = x * g[j];
            }

            hadamard8 (v);
            double yl = 0.0, yr = 0.0;
            for (std::size_t j = 0; j < kLines; ++j)
            {
                const double line = v[j] * kInvSqrt8 + inj[j] * xin;
                lines[j].push (static_cast<float> (line));
                yl += kOutL[j] * line;
                yr += kOutR[j] * line;
            }
            yl *= kInvSqrt8;
            yr *= kInvSqrt8;

            if (mono)
            {
                const double m = 0.5 * (yl + yr);
                const double side = monoSide.tick (0, 0.5 * (yl - yr));
                yl = m + side;
                yr = m - side;
            }
            outL[i] = static_cast<float> (yl);
            outR[i] = static_cast<float> (yr);
            ++t;
        }
    }
}
