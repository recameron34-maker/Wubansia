#include "fx/resonator.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace wubansia::engine::fx
{
    void Resonator::setup (const ResonatorSettings& settings) noexcept
    {
        std::array<double, kMaxResonatorTones> tones = settings.tonesHz;
        const int n = std::clamp (settings.tones, 0, kMaxResonatorTones);
        std::sort (tones.begin(), tones.begin() + n);

        const double rad = std::exp (-1.0 / (std::max (settings.tauS, 1e-6) * fs));
        count = 0;
        for (int i = 0; i < n; ++i)
        {
            const double f = tones[static_cast<std::size_t> (i)];
            if (! (f > 0.0) || f >= 0.49 * fs)
                continue;
            const double w = 2.0 * std::numbers::pi * f / fs;
            const double p = i % 2 != 0 ? 0.3 : -0.3; // by place among all the tones, at every rate
            Mode m;
            m.a1 = -2.0 * rad * std::cos (w);
            m.a2 = rad * rad;
            m.norm = std::sqrt (2.0 * (1.0 - rad * rad)) * std::sin (w);
            m.gl = std::sqrt (0.5 - 0.5 * p) * std::numbers::sqrt2;
            m.gr = std::sqrt (0.5 + 0.5 * p) * std::numbers::sqrt2;
            modes[static_cast<std::size_t> (count++)] = m;
        }
        if (settings.sendHpHz > 0.0)
            sendHp.setButterworth (PassKind::highpass, 2, settings.sendHpHz, 0.0, fs);
        else
            sendHp.setSections (nullptr, 0);
        reset();
    }

    void Resonator::reset() noexcept
    {
        sendHp.reset();
        for (auto& m : modes)
            m.y1 = m.y2 = 0.0;
    }

    void Resonator::process (const float* in, float* outL, float* outR, int n) noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            const double x = sendHp.tick (0, static_cast<double> (in[i]));
            double l = 0.0, r = 0.0;
            for (int k = 0; k < count; ++k)
            {
                auto& m = modes[static_cast<std::size_t> (k)];
                const double y = x - m.a1 * m.y1 - m.a2 * m.y2;
                m.y2 = m.y1;
                m.y1 = y;
                const double v = y * m.norm;
                l += v * m.gl;
                r += v * m.gr;
            }
            outL[i] = static_cast<float> (l);
            outR[i] = static_cast<float> (r);
        }
    }
}
