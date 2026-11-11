#pragma once

// The 4x stages (ADR 0021): a curve from soft_clip.h between an upsampler and a downsampler, stereo, in place.
// - MasterClip: master.soft_clip on os_fir(4) (the exact oversampler), 48 samples late: the club master's clip.
// - LaneClip, LaneDrive: the clip and drive inserts on the half-band cascade (rt_ref.lane_soft_clip, lane_drive),
//   27 samples late.

#include "fx/oversampler_exact.h"
#include "fx/oversampler_halfband.h"
#include "fx/soft_clip.h"

#include <array>
#include <cmath>

namespace wubansia::engine::fx
{
    template <class Up, class Down, class Curve, int Latency>
    class Oversampled
    {
    public:
        static constexpr int kLatency = Latency;

        Curve& curve (int channel) noexcept { return curves[static_cast<std::size_t> (channel)]; }

        // Applies to both channels (the curve's own setCurve arguments).
        template <class... Args>
        void setCurve (Args... args) noexcept
        {
            for (auto& c : curves)
                c.setCurve (args...);
        }

        void setOutputGain (double linear) noexcept { output = linear; }

        void reset() noexcept
        {
            for (auto& u : ups)
                u.reset();
            for (auto& d : downs)
                d.reset();
            for (auto& c : curves)
                c.reset();
        }

        void process (float* l, float* r, int n) noexcept
        {
            float* ch[2] { l, r };
            for (std::size_t c = 0; c < 2; ++c)
            {
                if (ch[c] == nullptr)
                    continue;
                auto& up = ups[c];
                auto& down = downs[c];
                auto& cv = curves[c];
                for (int i = 0; i < n; ++i)
                {
                    std::array<double, 4> u {};
                    up.push (static_cast<double> (ch[c][i]), u.data());
                    for (auto& v : u)
                        v = cv.tick (v);
                    ch[c][i] = static_cast<float> (down.push (u.data()) * output);
                }
            }
        }

    private:
        std::array<Up, 2> ups {};
        std::array<Down, 2> downs {};
        std::array<Curve, 2> curves {};
        double output = 1.0;
    };

    using MasterClip = Oversampled<ExactUpsampler4, ExactDownsampler4, SoftClip, kExactOversamplerLatency>;
    using LaneClip = Oversampled<HalfbandUpsampler4, HalfbandDownsampler4, SoftClip, kHalfbandLatency>;
    using LaneDrive = Oversampled<HalfbandUpsampler4, HalfbandDownsampler4, TanhDrive, kHalfbandLatency>;
}
