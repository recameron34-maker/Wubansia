#include "mix/inserts.h"

#include <algorithm>
#include <cmath>

namespace wubansia::engine::mix
{
    namespace
    {
        constexpr double kDbToLog = 0.11512925464970228; // ln (10) / 20

        double mdbToLinear (std::int32_t mdb) noexcept { return std::pow (10.0, static_cast<double> (mdb) / 20000.0); }
        double us (std::int32_t micros) noexcept { return static_cast<double> (micros) * 1e-6; }
        double hz (std::int32_t h) noexcept { return static_cast<double> (h); }

        // The due events of a ring (time + delay before the block's end), from `next` on, as sample offsets into
        // the block; returns how many (at most `capacity`). Events not yet due wait; a ring that overran skips ahead.
        template <class Event, class Accept>
        int dueEvents (const Event* ring, std::size_t size, std::uint64_t written, std::uint64_t& next, std::int64_t start, int n,
            int delay, Accept&& accept, int* offsets, const Event** events, int capacity) noexcept
        {
            if (ring == nullptr)
                return 0;
            if (written - next > size)
                next = written - size;
            int count = 0;
            for (; next < written; ++next)
            {
                const Event& e = ring[next % size];
                const std::int64_t at = e.time + delay - start;
                if (at >= n)
                    break;
                if (! accept (e) || count == capacity)
                    continue;
                offsets[count] = static_cast<int> (std::max<std::int64_t> (at, 0));
                events[count] = &e;
                ++count;
            }
            return count;
        }
    }

    bool whenHolds (const When& when, const Controls& controls) noexcept
    {
        if (when.variant >= 0 && static_cast<int> (controls.variant) != when.variant)
            return false;
        if (when.ab >= 0 && when.ab < kAbSwitches && controls.ab[static_cast<std::size_t> (when.ab)] != static_cast<std::uint8_t> (when.is))
            return false;
        return true;
    }

    // ------------------------------------------------------------------ gain, pan, width, EQ

    void GainInsert::setup (const GainParams& p, double) noexcept { gain = mdbToLinear (p.gainMdb); }

    void GainInsert::process (float* l, float* r, int n, const BlockContext&) noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            l[i] = static_cast<float> (static_cast<double> (l[i]) * gain);
            r[i] = static_cast<float> (static_cast<double> (r[i]) * gain);
        }
    }

    void PanInsert::setup (const PanParams& p, double) noexcept { gains = fx::PanGains::of (static_cast<double> (p.panPm) / 1000.0); }

    void PanInsert::process (float* l, float* r, int n, const BlockContext&) noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            l[i] = static_cast<float> (static_cast<double> (l[i]) * gains.l);
            r[i] = static_cast<float> (static_cast<double> (r[i]) * gains.r);
        }
    }

    void WidthInsert::setup (const WidthParams& p, double fs) noexcept
    {
        side.setup (fx::SideFilter::Mode::width, hz (p.freqHz), std::clamp (p.order, 1, 8), static_cast<double> (p.widthPm) / 1000.0, fs);
    }

    void EqInsert::setup (InsertType type, const EqParams& p, double fs) noexcept
    {
        auto kind = fx::RbjKind::peak;
        if (type == InsertType::lowShelf)
            kind = fx::RbjKind::lowShelf;
        else if (type == InsertType::highShelf)
            kind = fx::RbjKind::highShelf;
        const auto c = fx::rbj (kind, hz (p.freqHz), static_cast<double> (p.gainMdb) / 1000.0, static_cast<double> (p.qPm) / 1000.0, fs);
        filter.setSections (&c, 1);
        filter.reset();
    }

    void EqInsert::process (float* l, float* r, int n, const BlockContext&) noexcept
    {
        filter.process (0, l, n);
        filter.process (1, r, n);
    }

    // ------------------------------------------------------------------ highpass, lowpass

    void PassInsert::setup (InsertType type, const PassParams& p, double fs) noexcept
    {
        const auto kind = type == InsertType::highpass ? fx::PassKind::highpass : fx::PassKind::lowpass;
        std::array<fx::BiquadCoeffs, 8> stage {};
        int count = 0;
        if (p.qPm > 0)
        {
            stage[0] = fx::rbj (kind == fx::PassKind::highpass ? fx::RbjKind::highpass : fx::RbjKind::lowpass, hz (p.freqHz), 0.0,
                static_cast<double> (p.qPm) / 1000.0, fs);
            count = 1;
        }
        else
            count = fx::butterworth (kind, std::clamp (p.order, 1, 8), hz (p.freqHz), 0.0, fs, stage.data(), 4);
        const int stages = std::clamp (p.stages, 1, 2);
        std::array<fx::BiquadCoeffs, 8> all {};
        for (int s = 0; s < stages; ++s)
            for (int c = 0; c < count; ++c)
                all[static_cast<std::size_t> (s * count + c)] = stage[static_cast<std::size_t> (c)];
        filter.setSections (all.data(), stages * count);
        band.setup (hz (p.band.loHz), hz (p.band.hiHz), fs);
        reset();
    }

    void PassInsert::reset() noexcept
    {
        filter.reset();
        band.reset();
    }

    void PassInsert::process (float* l, float* r, int n, const BlockContext&) noexcept
    {
        if (band.whole())
        {
            filter.process (0, l, n);
            filter.process (1, r, n);
            return;
        }
        float* ch[2] { l, r };
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i)
            {
                double rest, b;
                band.split (c, static_cast<double> (ch[c][i]), rest, b);
                ch[c][i] = static_cast<float> (rest + filter.tick (c, b));
            }
    }

    // ------------------------------------------------------------------ cc_filter

    void CcFilterInsert::setup (const CcFilterParams& p, double sampleRate) noexcept
    {
        params = p;
        fs = sampleRate;
        band.setup (hz (p.band.loHz), hz (p.band.hiHz), fs);
        reset();
    }

    void CcFilterInsert::reset() noexcept
    {
        v74 = static_cast<double> (std::clamp (params.cc74Default, 0, 127));
        v71 = static_cast<double> (std::clamp (params.cc71Default, 0, 127));
        r74 = {};
        r71 = {};
        dirty = true;
        for (auto& s : svf)
            s.reset();
        band.reset();
        nextEvent = 0;
    }

    void CcFilterInsert::apply (const CcEvent& e) noexcept
    {
        Ramp* ramp = e.controller == 74 ? &r74 : e.controller == 71 ? &r71 : nullptr;
        if (ramp == nullptr)
            return;
        double& v = e.controller == 74 ? v74 : v71;
        const double target = static_cast<double> (std::min<int> (e.value, 127));
        if (e.ramp > 0)
        {
            ramp->target = target;
            ramp->left = e.ramp;
            ramp->step = (target - v) / static_cast<double> (e.ramp);
        }
        else
        {
            *ramp = {};
            v = target;
        }
        dirty = true;
    }

    void CcFilterInsert::process (float* l, float* r, int n, const BlockContext& ctx) noexcept
    {
        constexpr int kMaxDue = 32;
        std::array<int, kMaxDue> at {};
        std::array<const CcEvent*, kMaxDue> due {};
        const int count = dueEvents (ctx.cc, static_cast<std::size_t> (kCcEvents), ctx.ccWritten, nextEvent, ctx.start, n, delay,
            [] (const CcEvent&) { return true; }, at.data(), due.data(), kMaxDue);

        const double lo = std::max (hz (params.cc74LoHz), 1.0);
        const double ratio = std::max (hz (params.cc74HiHz), lo) / lo;
        const double q0 = static_cast<double> (params.cc71LoPm);
        const double q1 = static_cast<double> (params.cc71HiPm);
        const bool whole = band.whole();
        int e = 0;
        for (int i = 0; i < n; ++i)
        {
            // a ramp started at an earlier sample moves on; one starting here holds its value this sample and
            // reaches its target `ramp` samples on; a step lands here
            for (auto* rp : { &r74, &r71 })
                if (rp->left > 0)
                {
                    double& v = rp == &r74 ? v74 : v71;
                    v = --rp->left == 0 ? rp->target : v + rp->step;
                    dirty = true;
                }
            while (e < count && at[static_cast<std::size_t> (e)] <= i)
                apply (*due[static_cast<std::size_t> (e++)]);
            if (dirty)
            {
                const double cutoff = lo * std::pow (ratio, v74 / 127.0);
                const double q = (q0 + (q1 - q0) * v71 / 127.0) / 1000.0;
                coeffs = fx::SvfCoeffs::make (cutoff, q, fs);
                dirty = false;
            }
            float* ch[2] { l, r };
            for (std::size_t c = 0; c < 2; ++c)
            {
                double rest = 0.0, b = static_cast<double> (ch[c][i]);
                if (! whole)
                    band.split (static_cast<int> (c), static_cast<double> (ch[c][i]), rest, b);
                const auto o = svf[c].tick (coeffs, b);
                const double y = params.response == FilterResponse::lowpass ? o.lp : params.response == FilterResponse::bandpass ? o.bp : o.hp;
                ch[c][i] = static_cast<float> (rest + y);
            }
        }
    }

    // ------------------------------------------------------------------ mono_below, clip, drive

    void MonoBelowInsert::setup (const MonoBelowParams& p, double fs) noexcept
    {
        side.setup (fx::SideFilter::Mode::monoBelow, hz (p.freqHz), std::clamp (p.order, 1, 8), 1.0, fs);
    }

    void ClipInsert::setup (const ClipParams& p, double) noexcept
    {
        clip.setCurve (static_cast<double> (p.ceilingMdb) / 1000.0, static_cast<double> (p.kneeMdb) / 1000.0);
        clip.reset();
    }

    void DriveInsert::setup (const DriveParams& p, double) noexcept
    {
        drive.setCurve (static_cast<double> (p.drivePm) / 1000.0, static_cast<double> (p.biasPm) / 1000.0);
        drive.setOutputGain (mdbToLinear (p.outputMdb));
        drive.reset();
    }

    // ------------------------------------------------------------------ duck

    void DuckInsert::setup (const DuckParams& p, double fs) noexcept
    {
        params = p;
        depthDb = static_cast<double> (p.depthMdb) / 1000.0;
        band.setup (hz (p.band.loHz), hz (p.band.hiHz), fs);
        envelope.setShape (us (p.rampUs), us (p.holdUs), us (p.releaseUs), fs);
        detector.setup (static_cast<double> (p.rangeMdb) / 1000.0, static_cast<double> (p.topMdb) / 1000.0, us (p.attackUs), us (p.releaseUs), fs);
        keyChan = p.mode == DuckMode::level ? p.keyChannel : -1;
        keyDelay = 0;
        preDelay.setDelay (0);
        reset();
    }

    void DuckInsert::setLevelKey (int channel, int keyDelaySamples, int preDelaySamples) noexcept
    {
        keyChan = channel;
        keyDelay = std::max (keyDelaySamples, 0);
        preDelay.setDelay (preDelaySamples);
    }

    void DuckInsert::reset() noexcept
    {
        band.reset();
        envelope.reset();
        detector.reset();
        preDelay.reset();
        nextKey = 0;
    }

    void DuckInsert::process (float* l, float* r, int n, const BlockContext& ctx) noexcept
    {
        preDelay.process (l, r, n);

        constexpr int kMaxDue = 64;
        std::array<int, kMaxDue> at {};
        std::array<const KeyEvent*, kMaxDue> due {};
        int count = 0;
        const bool onset = params.mode == DuckMode::onset;
        if (onset)
        {
            const auto lanes = params.keyLanes;
            const auto minVelocity = params.minVelocity;
            count = dueEvents (ctx.keys, static_cast<std::size_t> (kKeyEvents), ctx.keysWritten, nextKey, ctx.start, n, keyDelay,
                [lanes, minVelocity] (const KeyEvent& e) {
                    return e.lane < 16 && (lanes & (1u << e.lane)) != 0 && static_cast<int> (e.velocity) >= minVelocity;
                },
                at.data(), due.data(), kMaxDue);
        }
        const float* key = ! onset && keyChan >= 0 && ctx.keyRings != nullptr ? ctx.keyRings[keyChan] : nullptr;
        if (! onset && key == nullptr)
            return; // a level duck without its key (a cycle): bypassed

        const bool whole = band.whole();
        if (onset && whole && count == 0 && envelope.idle())
            return; // at rest the whole band passes as it is: 10^0 x = x

        int e = 0;
        for (int i = 0; i < n; ++i)
        {
            double share;
            if (onset)
            {
                while (e < count && at[static_cast<std::size_t> (e)] <= i)
                {
                    envelope.trigger();
                    ++e;
                }
                share = envelope.tick();
            }
            else
            {
                const std::int64_t t = ctx.start + i - keyDelay;
                const double k = t >= 0 ? static_cast<double> (key[static_cast<std::size_t> (t) & (kKeyRing - 1)]) : 0.0;
                share = detector.tick (k);
            }
            if (whole && ! (share > 0.0))
                continue; // at rest: exactly the input
            const double depth = params.pump && ctx.pump != nullptr ? depthDb * static_cast<double> (ctx.pump[i]) : depthDb;
            const double g = std::exp (-kDbToLog * depth * share);
            if (whole)
            {
                l[i] = static_cast<float> (static_cast<double> (l[i]) * g);
                r[i] = static_cast<float> (static_cast<double> (r[i]) * g);
            }
            else
            {
                const double xl = static_cast<double> (l[i]);
                const double xr = static_cast<double> (r[i]);
                l[i] = static_cast<float> (xl + (g - 1.0) * band.tick (0, xl));
                r[i] = static_cast<float> (xr + (g - 1.0) * band.tick (1, xr));
            }
        }
    }

    // ------------------------------------------------------------------ comp, parallel

    void CompInsert::setup (const CompParams& p, double fs) noexcept
    {
        fx::GlueSettings s;
        s.thresholdDb = static_cast<double> (p.thresholdMdb) / 1000.0;
        s.ratio = static_cast<double> (p.ratioPm) / 1000.0;
        s.kneeDb = static_cast<double> (p.kneeMdb) / 1000.0;
        s.attackS = us (p.attackUs);
        s.releaseFastS = us (p.releaseFastUs);
        s.releaseSlowS = us (p.releaseSlowUs);
        s.makeupDb = static_cast<double> (p.makeupMdb) / 1000.0;
        glue.setup (s, fs);
    }

    void ParallelInsert::setup (const ParallelParams& p, double) noexcept
    {
        level = mdbToLinear (p.levelMdb);
        chain = nullptr;
        chainCount = 0;
        dry.setDelay (0);
        dry.reset();
    }

    void ParallelInsert::setChain (InsertSlot* first, int count, int chainLatency) noexcept
    {
        chain = first;
        chainCount = count;
        dry.setDelay (chainLatency);
    }

    void ParallelInsert::process (float* l, float* r, int n, const BlockContext& ctx) noexcept
    {
        float* wl = ctx.scratch.parallelL;
        float* wr = ctx.scratch.parallelR;
        if (wl == nullptr || wr == nullptr)
            return;
        std::copy_n (l, n, wl);
        std::copy_n (r, n, wr);
        dry.process (l, r, n);
        processChain (chain, chainCount, wl, wr, n, ctx);
        for (int i = 0; i < n; ++i)
        {
            l[i] = static_cast<float> (static_cast<double> (l[i]) + level * static_cast<double> (wl[i]));
            r[i] = static_cast<float> (static_cast<double> (r[i]) + level * static_cast<double> (wr[i]));
        }
    }

    // ------------------------------------------------------------------ the slot

    void InsertSlot::setup (const Insert& insert, int nesting, double fs, const Controls& controls) noexcept
    {
        type = insert.type;
        when = insert.when;
        conditional = ! insert.when.always();
        depth = std::clamp (nesting, 0, 1);
        span = 1;
        inputLatency = 0;
        fadeStep = 1.0 / (0.010 * fs);

        const auto& p = insert.params;
        bool ok = true;
        switch (insert.type)
        {
            case InsertType::gain:
                if ((ok = std::holds_alternative<GainParams> (p)))
                    proc.emplace<GainInsert>().setup (std::get<GainParams> (p), fs);
                break;
            case InsertType::pan:
                if ((ok = std::holds_alternative<PanParams> (p)))
                    proc.emplace<PanInsert>().setup (std::get<PanParams> (p), fs);
                break;
            case InsertType::width:
                if ((ok = std::holds_alternative<WidthParams> (p)))
                    proc.emplace<WidthInsert>().setup (std::get<WidthParams> (p), fs);
                break;
            case InsertType::peak:
            case InsertType::lowShelf:
            case InsertType::highShelf:
                if ((ok = std::holds_alternative<EqParams> (p)))
                    proc.emplace<EqInsert>().setup (insert.type, std::get<EqParams> (p), fs);
                break;
            case InsertType::highpass:
            case InsertType::lowpass:
                if ((ok = std::holds_alternative<PassParams> (p)))
                    proc.emplace<PassInsert>().setup (insert.type, std::get<PassParams> (p), fs);
                break;
            case InsertType::ccFilter:
                if ((ok = std::holds_alternative<CcFilterParams> (p)))
                    proc.emplace<CcFilterInsert>().setup (std::get<CcFilterParams> (p), fs);
                break;
            case InsertType::monoBelow:
                if ((ok = std::holds_alternative<MonoBelowParams> (p)))
                    proc.emplace<MonoBelowInsert>().setup (std::get<MonoBelowParams> (p), fs);
                break;
            case InsertType::clip:
                if ((ok = std::holds_alternative<ClipParams> (p)))
                    proc.emplace<ClipInsert>().setup (std::get<ClipParams> (p), fs);
                break;
            case InsertType::drive:
                if ((ok = std::holds_alternative<DriveParams> (p)))
                    proc.emplace<DriveInsert>().setup (std::get<DriveParams> (p), fs);
                break;
            case InsertType::duck:
                if ((ok = std::holds_alternative<DuckParams> (p)))
                    proc.emplace<DuckInsert>().setup (std::get<DuckParams> (p), fs);
                break;
            case InsertType::comp:
                if ((ok = std::holds_alternative<CompParams> (p)))
                    proc.emplace<CompInsert>().setup (std::get<CompParams> (p), fs);
                break;
            case InsertType::parallel:
                if ((ok = std::holds_alternative<ParallelParams> (p)))
                    proc.emplace<ParallelInsert>().setup (std::get<ParallelParams> (p), fs);
                break;
        }
        if (! ok) // the parser never lets a type and its params disagree; unity gain if they do
            proc.emplace<GainInsert>().setup (GainParams {}, fs);
        bypassDelay.setDelay (latency());
        reset (controls);
    }

    int InsertSlot::latency() const noexcept
    {
        return std::visit ([] (const auto& p) { return p.latency(); }, proc);
    }

    bool InsertSlot::activeFor (const Controls& controls) const noexcept
    {
        return ! conditional || whenHolds (when, controls);
    }

    void InsertSlot::reset (const Controls& controls) noexcept
    {
        std::visit ([] (auto& p) { p.reset(); }, proc);
        bypassDelay.setDelay (latency());
        bypassDelay.reset();
        fade = activeFor (controls) ? 1.0 : 0.0;
    }

    void InsertSlot::process (float* l, float* r, int n, const BlockContext& ctx) noexcept
    {
        if (! conditional)
        {
            std::visit ([&] (auto& p) { p.process (l, r, n, ctx); }, proc);
            return;
        }
        const bool on = activeFor (*ctx.controls);
        float* bl = ctx.scratch.bypassL[depth];
        float* br = ctx.scratch.bypassR[depth];
        std::copy_n (l, n, bl);
        std::copy_n (r, n, br);
        bypassDelay.process (bl, br, n);
        std::visit ([&] (auto& p) { p.process (l, r, n, ctx); }, proc);
        if (on && fade >= 1.0)
            return;
        if (! on && fade <= 0.0)
        {
            std::copy_n (bl, n, l);
            std::copy_n (br, n, r);
            return;
        }
        for (int i = 0; i < n; ++i)
        {
            fade = std::clamp (fade + (on ? fadeStep : -fadeStep), 0.0, 1.0);
            l[i] = static_cast<float> (static_cast<double> (bl[i]) + fade * (static_cast<double> (l[i]) - static_cast<double> (bl[i])));
            r[i] = static_cast<float> (static_cast<double> (br[i]) + fade * (static_cast<double> (r[i]) - static_cast<double> (br[i])));
        }
    }

    void processChain (InsertSlot* slots, int total, float* l, float* r, int n, const BlockContext& ctx) noexcept
    {
        for (int i = 0; i < total; i += std::max (slots[i].span, 1))
            slots[i].process (l, r, n, ctx);
    }
}
