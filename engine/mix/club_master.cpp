#include "mix/club_master.h"

#include <algorithm>
#include <cmath>

namespace wubansia::engine::mix
{
    namespace
    {
        fx::LimiterSettings limiterSettings (const MasterConfig& m) noexcept
        {
            fx::LimiterSettings s;
            s.ceilingDb = static_cast<double> (m.limiterCeilingMdb) / 1000.0;
            s.lookaheadS = std::min (static_cast<double> (m.limiterLookaheadUs) * 1e-6, ClubMaster::kLookaheadS);
            s.holdS = static_cast<double> (m.limiterHoldUs) * 1e-6;
            s.releaseS = static_cast<double> (m.limiterReleaseUs) * 1e-6;
            return s;
        }

        double driveGain (const MasterConfig& m) noexcept { return std::pow (10.0, static_cast<double> (m.driveMdb) / 20000.0); }

        int limiterBudget (double fs) noexcept
        {
            fx::LimiterSettings s;
            s.lookaheadS = ClubMaster::kLookaheadS;
            return fx::Limiter::latencyFor (s, fx::Limiter::Hold::lookBack, fs);
        }
    }

    int ClubMaster::latencyFor (double sampleRate) noexcept
    {
        return fx::Glue::kLatency + fx::MasterClip::kLatency + limiterBudget (sampleRate);
    }

    void ClubMaster::prepare (double sampleRate, int maxBlock)
    {
        fs = sampleRate;
        fixedLatency = latencyFor (fs);
        limiter.prepare (fs);
        const auto padCapacity = static_cast<std::size_t> (limiterBudget (fs) + 2);
        padL.prepare (padCapacity);
        padR.prepare (padCapacity);
        bypassL.prepare (static_cast<std::size_t> (fixedLatency + 2));
        bypassR.prepare (static_cast<std::size_t> (fixedLatency + 2));
        dryL.assign (static_cast<std::size_t> (std::max (maxBlock, 1)), 0.0f);
        dryR.assign (static_cast<std::size_t> (std::max (maxBlock, 1)), 0.0f);
        drive.setup (fs);
        wet.setup (fs);
        setup (cfg, Controls {}, origin);
    }

    void ClubMaster::setup (const std::array<MasterConfig, 2>& config, const Controls& controls, int originSamples) noexcept
    {
        cfg = config;
        origin = originSamples;
        variant = controls.variant == Variant::novox ? 1 : 0;
        const auto& m = cfg[static_cast<std::size_t> (variant)];

        lowcut.setButterworth (fx::PassKind::highpass, std::clamp (m.lowcutOrder, 1, 8), static_cast<double> (m.lowcutHz), 0.0, fs);
        mono.setup (fx::SideFilter::Mode::monoBelow, static_cast<double> (std::max (m.monoBelowHz, 0)), 8, 1.0, fs);

        fx::GlueSettings g;
        g.thresholdDb = static_cast<double> (m.glueThresholdMdb) / 1000.0;
        g.ratio = static_cast<double> (m.glueRatioPm) / 1000.0;
        g.kneeDb = static_cast<double> (m.glueKneeMdb) / 1000.0;
        g.attackS = static_cast<double> (m.glueAttackUs) * 1e-6;
        g.releaseFastS = static_cast<double> (m.glueReleaseFastUs) * 1e-6;
        g.releaseSlowS = static_cast<double> (m.glueReleaseSlowUs) * 1e-6;
        glue.setOrigin (origin);
        glue.setup (g, fs);

        clip.setCurve (static_cast<double> (m.clipCeilingMdb) / 1000.0, static_cast<double> (m.clipKneeMdb) / 1000.0);
        dc.setup (static_cast<double> (std::max (m.dcHz, 1)), fs);
        limiter.setup (limiterSettings (m), fx::Limiter::Hold::lookBack);
        pad = std::max (limiterBudget (fs) - limiter.latency(), 0);
        reset (controls);
    }

    void ClubMaster::reset (const Controls& controls) noexcept
    {
        variant = controls.variant == Variant::novox ? 1 : 0;
        const auto& m = cfg[static_cast<std::size_t> (variant)];
        lowcut.reset();
        mono.reset();
        glue.setThreshold (static_cast<double> (m.glueThresholdMdb) / 1000.0);
        glue.reset();
        drive.jump (driveGain (m));
        clip.reset();
        dc.reset();
        limiter.reset();
        padL.reset();
        padR.reset();
        bypassL.reset();
        bypassR.reset();
        wet.jump (controls.club ? 1.0 : 0.0);
        gr = 0.0;
    }

    void ClubMaster::process (float* l, float* r, int n, const Controls& controls) noexcept
    {
        const int v = controls.variant == Variant::novox ? 1 : 0;
        if (v != variant)
        {
            variant = v;
            const auto& m = cfg[static_cast<std::size_t> (v)];
            glue.setThreshold (static_cast<double> (m.glueThresholdMdb) / 1000.0);
            drive.moveTo (driveGain (m));
        }
        wet.moveTo (controls.club ? 1.0 : 0.0);

        // the bypass: the bus as it comes, as late as the chain
        for (int i = 0; i < n; ++i)
        {
            bypassL.push (l[i]);
            bypassR.push (r[i]);
            dryL[static_cast<std::size_t> (i)] = bypassL.delayed (fixedLatency);
            dryR[static_cast<std::size_t> (i)] = bypassR.delayed (fixedLatency);
        }

        lowcut.process (0, l, n);
        lowcut.process (1, r, n);
        mono.process (l, r, n);
        glue.process (l, r, n);
        for (int i = 0; i < n; ++i)
        {
            const double g = drive.next();
            l[i] = static_cast<float> (static_cast<double> (l[i]) * g);
            r[i] = static_cast<float> (static_cast<double> (r[i]) * g);
        }
        clip.process (l, r, n);
        dc.process (l, r, n);
        limiter.process (l, r, n);
        if (pad > 0)
            for (int i = 0; i < n; ++i)
            {
                padL.push (l[i]);
                padR.push (r[i]);
                l[i] = padL.delayed (pad);
                r[i] = padR.delayed (pad);
            }

        if (! wet.moving() && wet.current() >= 1.0)
        {
            gr = glue.lastReductionDb() + limiter.lastReductionDb();
            return;
        }
        for (int i = 0; i < n; ++i)
        {
            const double w = wet.next();
            const auto k = static_cast<std::size_t> (i);
            l[i] = static_cast<float> (static_cast<double> (dryL[k]) + w * (static_cast<double> (l[i]) - static_cast<double> (dryL[k])));
            r[i] = static_cast<float> (static_cast<double> (dryR[k]) + w * (static_cast<double> (r[i]) - static_cast<double> (dryR[k])));
        }
        gr = wet.current() > 0.0 ? glue.lastReductionDb() + limiter.lastReductionDb() : 0.0;
    }
}
