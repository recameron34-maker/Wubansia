#include "mix/graph.h"

#include <algorithm>
#include <cmath>

namespace wubansia::engine::mix
{
    namespace
    {
        double mdbToLinear (std::int32_t mdb) noexcept { return std::pow (10.0, static_cast<double> (mdb) / 20000.0); }

        // The level ducks' key channels in a chain (a parallel insert's included).
        template <class F>
        void forEachLevelKey (const std::vector<Insert>& inserts, F&& f)
        {
            for (const auto& insert : inserts)
            {
                if (const auto* d = std::get_if<DuckParams> (&insert.params); d != nullptr && d->mode == DuckMode::level)
                    f (d->keyChannel);
                if (const auto* p = std::get_if<ParallelParams> (&insert.params))
                    forEachLevelKey (p->chain, f);
            }
        }

        // The channels in an order where each level duck's key channel comes first. A key that would close a cycle
        // stays unplanned when its duck is planned, and the duck is bypassed (the parser refuses cycles anyway).
        int keyOrder (const MixConfig& cfg, std::array<int, kMaxMixChannels>& order) noexcept
        {
            const int n = std::min (static_cast<int> (cfg.channels.size()), kMaxMixChannels);
            std::array<std::uint8_t, kMaxMixChannels> state {}; // 0 new, 1 on the stack, 2 placed
            int count = 0;
            // iterative depth-first search with an explicit stack of (channel, next key index)
            for (int root = 0; root < n; ++root)
            {
                if (state[static_cast<std::size_t> (root)] != 0)
                    continue;
                std::array<int, kMaxMixChannels> stack {};
                std::array<int, kMaxMixChannels> cursor {};
                int depth = 0;
                stack[0] = root;
                cursor[0] = 0;
                state[static_cast<std::size_t> (root)] = 1;
                while (depth >= 0)
                {
                    const int c = stack[static_cast<std::size_t> (depth)];
                    int k = 0, wanted = cursor[static_cast<std::size_t> (depth)], next = -1;
                    forEachLevelKey (cfg.channels[static_cast<std::size_t> (c)].inserts, [&] (int key) {
                        if (next < 0 && k++ >= wanted && key >= 0 && key < n && state[static_cast<std::size_t> (key)] == 0)
                            next = key;
                    });
                    if (next >= 0 && depth + 1 < kMaxMixChannels)
                    {
                        cursor[static_cast<std::size_t> (depth)] = k;
                        stack[static_cast<std::size_t> (++depth)] = next;
                        cursor[static_cast<std::size_t> (depth)] = 0;
                        state[static_cast<std::size_t> (next)] = 1;
                        continue;
                    }
                    state[static_cast<std::size_t> (c)] = 2;
                    order[static_cast<std::size_t> (count++)] = c;
                    --depth;
                }
            }
            return count;
        }

        // A chain's latency from an input latency, as setConfig plans it (clip and drive 27, comp 23, a parallel
        // insert its chain's, a level duck the wait for a later key).
        int chainLatency (const std::vector<Insert>& inserts, int l, const std::array<int, kMaxMixChannels>& known) noexcept
        {
            for (const auto& insert : inserts)
            {
                switch (insert.type)
                {
                    case InsertType::clip:
                        l += fx::LaneClip::kLatency;
                        break;
                    case InsertType::drive:
                        l += fx::LaneDrive::kLatency;
                        break;
                    case InsertType::comp:
                        l += fx::Glue::kLatency;
                        break;
                    case InsertType::parallel:
                        if (const auto* p = std::get_if<ParallelParams> (&insert.params))
                            l = chainLatency (p->chain, l, known);
                        break;
                    case InsertType::duck:
                        if (const auto* d = std::get_if<DuckParams> (&insert.params); d != nullptr && d->mode == DuckMode::level
                                                                                      && d->keyChannel >= 0 && d->keyChannel < kMaxMixChannels)
                        {
                            const int key = known[static_cast<std::size_t> (d->keyChannel)];
                            if (key > l)
                                l = key;
                        }
                        break;
                    case InsertType::gain:
                    case InsertType::pan:
                    case InsertType::width:
                    case InsertType::peak:
                    case InsertType::lowShelf:
                    case InsertType::highShelf:
                    case InsertType::highpass:
                    case InsertType::lowpass:
                    case InsertType::ccFilter:
                    case InsertType::monoBelow:
                        break;
                }
            }
            return l;
        }

        // `n` samples of a BlockDelay's mono use: delay x in place.
        void delayMono (AlignDelay& d, float* x, int n) noexcept { d.process (x, nullptr, n); }
    }

    int MixGraph::requiredLatency (const MixConfig& cfg) noexcept
    {
        std::array<int, kMaxMixChannels> order {};
        const int n = keyOrder (cfg, order);
        std::array<int, kMaxMixChannels> known;
        known.fill (-1);
        int a1v = 0;
        for (int i = 0; i < n; ++i)
        {
            const int c = order[static_cast<std::size_t> (i)];
            known[static_cast<std::size_t> (c)] = chainLatency (cfg.channels[static_cast<std::size_t> (c)].inserts, 0, known);
            a1v = std::max (a1v, known[static_cast<std::size_t> (c)]);
        }
        int a2v = a1v;
        for (const auto* chain : { &cfg.drums.inserts, &cfg.music.inserts, &cfg.space.inserts, &cfg.echo.inserts, &cfg.resonator.inserts })
            a2v = std::max (a2v, chainLatency (*chain, a1v, known));
        int a3v = a2v;
        for (const auto& eq : cfg.busEq)
            a3v = std::max (a3v, chainLatency (eq, a2v, known));
        return a3v;
    }

    void MixGraph::prepare (double sampleRate, int maxBlockSize)
    {
        fs = sampleRate;
        maxBlock = std::max (1, maxBlockSize);
        slots.assign (static_cast<std::size_t> (kMaxSlots), InsertSlot {});
        ccEvents.assign (static_cast<std::size_t> (kMaxMixChannels * kCcEvents), CcEvent {});
        keyRingData.assign (static_cast<std::size_t> (kMaxMixChannels) * kKeyRing, 0.0f);
        for (std::size_t c = 0; c < static_cast<std::size_t> (kMaxMixChannels); ++c)
            keyRings[c] = keyRingData.data() + c * kKeyRing;

        // bypass x2 x2, parallel x2, groups x2 x2, none x2, sends x3, throw, return x2, mix x2, alt x2, pump, macros x2
        constexpr int kBuffers = 4 + 2 + 4 + 2 + kSends + 1 + 2 + 2 + 2 + 1 + 2;
        const auto stride = static_cast<std::size_t> (maxBlock);
        buffers.assign (stride * kBuffers, 0.0f);
        int next = 0;
        auto take = [&] { return buffers.data() + stride * static_cast<std::size_t> (next++); };
        for (auto& level : bypass)
            for (auto& b : level)
                b = take();
        for (auto& p : parallel)
            p = take();
        for (auto& g : groupIn)
            for (auto& b : g)
                b = take();
        for (auto& b : noneIn)
            b = take();
        for (auto& s : sendIn)
            s = take();
        throwBuf = take();
        for (auto& b : ret)
            b = take();
        mixL = take();
        mixR = take();
        altL = take();
        altR = take();
        pump = take();
        for (auto& m : macro)
            m = take();

        space.prepare (fs);
        echo.prepare (fs);
        resonator.prepare (fs);
        master.prepare (fs, maxBlock);
        plainL.prepare (static_cast<std::size_t> (latency() + 2));
        plainR.prepare (static_cast<std::size_t> (latency() + 2));
        for (auto* r : { &spaceMacro, &echoMacro, &pumpMacro, &busMix })
            r->setup (fs);
        for (auto& ch : channels)
            ch.level.setup (fs);

        const auto* cfg = config;
        config = nullptr; // replan at this rate
        setConfig (cfg, Controls {});
    }

    int MixGraph::addChain (const std::vector<Insert>& inserts, int nesting, const Controls& controls) noexcept
    {
        const int first = slotsUsed;
        for (const auto& insert : inserts)
        {
            if (slotsUsed >= kMaxSlots)
            {
                tooMany = true;
                break;
            }
            auto& slot = slots[static_cast<std::size_t> (slotsUsed++)];
            slot.setup (insert, nesting, fs, controls);
            if (const auto* p = std::get_if<ParallelParams> (&insert.params); p != nullptr && nesting == 0 && insert.type == InsertType::parallel)
                slot.span = 1 + addChain (p->chain, 1, controls);
        }
        return slotsUsed - first;
    }

    // Each slot's input latency, its keys' and controllers' waits and origins, its bypass; the chain's output latency.
    int MixGraph::planChain (int firstSlot, int count, int l, const std::array<int, kMaxMixChannels>& channelLatency) noexcept
    {
        for (int i = firstSlot; i < firstSlot + count; i += std::max (slots[static_cast<std::size_t> (i)].span, 1))
        {
            auto& slot = slots[static_cast<std::size_t> (i)];
            slot.inputLatency = l;
            if (auto* cc = std::get_if<CcFilterInsert> (&slot.proc))
                cc->setDelay (l);
            else if (auto* comp = std::get_if<CompInsert> (&slot.proc))
                comp->setOrigin (l);
            else if (auto* duck = std::get_if<DuckInsert> (&slot.proc))
            {
                if (duck->mode() == DuckMode::onset)
                    duck->setOnsetDelay (l);
                else
                {
                    const int key = duck->keyChannel();
                    const int keyLatency = key >= 0 && key < kMaxMixChannels ? channelLatency[static_cast<std::size_t> (key)] : -1;
                    if (keyLatency < 0)
                        duck->setLevelKey (-1, 0, 0); // a cycle: bypassed
                    else
                    {
                        const int pre = std::max (keyLatency - l, 0);
                        duck->setLevelKey (key, l + pre - keyLatency, pre);
                        duck->setOrigin (l + pre);
                    }
                }
            }
            else if (auto* par = std::get_if<ParallelInsert> (&slot.proc))
            {
                const int inner = slot.span - 1;
                const int out = planChain (i + 1, inner, l, channelLatency);
                par->setChain (inner > 0 ? &slots[static_cast<std::size_t> (i + 1)] : nullptr, inner, out - l);
            }
            slot.bypassDelay.setDelay (slot.latency());
            slot.bypassDelay.reset();
            l += slot.latency();
        }
        return l;
    }

    void MixGraph::setConfig (const MixConfig* cfg, const Controls& controls) noexcept
    {
        config = cfg;
        slotsUsed = 0;
        numChannels = 0;
        tooMany = tooLate = false;
        a1 = a2 = a3 = 0;
        for (auto& g : groups)
            g = Strip {};
        for (auto& r : returns)
            r = Strip {};
        for (auto& b : busEq)
            b = Strip {};
        returnOn.fill (false);

        if (cfg != nullptr)
        {
            numChannels = std::min (static_cast<int> (cfg->channels.size()), kMaxMixChannels);
            for (int c = 0; c < numChannels; ++c)
            {
                const auto& src = cfg->channels[static_cast<std::size_t> (c)];
                auto& ch = channels[static_cast<std::size_t> (c)];
                ch.config = c;
                ch.lane = src.lane;
                ch.group = src.group;
                for (std::size_t s = 0; s < static_cast<std::size_t> (kSends); ++s)
                    ch.sends[s] = static_cast<double> (std::clamp (src.sendsPm[s], 0, 1000)) / 1000.0;
                ch.firstSlot = slotsUsed;
                ch.slotCount = addChain (src.inserts, 0, controls);
                ch.key = false;
            }
            // every channel a level duck listens to keeps its post-insert signal for it, whatever chain the duck is in
            const auto markKey = [&] (int key) {
                if (key >= 0 && key < numChannels)
                    channels[static_cast<std::size_t> (key)].key = true;
            };
            for (int c = 0; c < numChannels; ++c)
                forEachLevelKey (cfg->channels[static_cast<std::size_t> (c)].inserts, markKey);
            for (const auto* chain : { &cfg->drums.inserts, &cfg->music.inserts, &cfg->space.inserts, &cfg->echo.inserts,
                     &cfg->resonator.inserts, &cfg->busEq[0], &cfg->busEq[1] })
                forEachLevelKey (*chain, markKey);
            const std::vector<Insert>* stripChains[2] { &cfg->drums.inserts, &cfg->music.inserts };
            for (std::size_t g = 0; g < 2; ++g)
            {
                groups[g].firstSlot = slotsUsed;
                groups[g].slotCount = addChain (*stripChains[g], 0, controls);
            }
            const std::vector<Insert>* returnChains[kSends] { &cfg->space.inserts, &cfg->echo.inserts, &cfg->resonator.inserts };
            for (std::size_t r = 0; r < static_cast<std::size_t> (kSends); ++r)
            {
                returns[r].firstSlot = slotsUsed;
                returns[r].slotCount = addChain (*returnChains[r], 0, controls);
            }
            for (std::size_t v = 0; v < 2; ++v)
            {
                busEq[v].firstSlot = slotsUsed;
                busEq[v].slotCount = addChain (cfg->busEq[v], 0, controls);
            }

            // latencies, in key order
            const int planned = keyOrder (*cfg, order);
            std::array<int, kMaxMixChannels> known;
            known.fill (-1);
            for (int i = 0; i < planned; ++i)
            {
                const int c = order[static_cast<std::size_t> (i)];
                auto& ch = channels[static_cast<std::size_t> (c)];
                ch.latency = planChain (ch.firstSlot, ch.slotCount, 0, known);
                known[static_cast<std::size_t> (c)] = ch.latency;
                a1 = std::max (a1, ch.latency);
            }
            int late = 0;
            for (auto& g : groups)
            {
                g.latency = planChain (g.firstSlot, g.slotCount, a1, known) - a1;
                late = std::max (late, g.latency);
            }
            for (auto& r : returns)
            {
                r.latency = planChain (r.firstSlot, r.slotCount, a1, known) - a1;
                late = std::max (late, r.latency);
            }
            a2 = a1 + late;
            int eqLate = 0;
            for (auto& b : busEq)
            {
                b.latency = planChain (b.firstSlot, b.slotCount, a2, known) - a2;
                eqLate = std::max (eqLate, b.latency);
            }
            a3 = a2 + eqLate;
            tooLate = a3 > kMixLatency;

            for (int c = 0; c < numChannels; ++c)
            {
                auto& ch = channels[static_cast<std::size_t> (c)];
                ch.align.setDelay (a1 - ch.latency);
            }
            for (auto& g : groups)
                g.align.setDelay (late - g.latency);
            for (auto& r : returns)
                r.align.setDelay (late - r.latency);
            noneAlign.setDelay (late);
            for (auto& b : busEq)
                b.align.setDelay (eqLate - b.latency);
            for (auto& t : throwAlign)
                t.setDelay (a1);
            preMaster.setDelay (std::max (kMixLatency - a3, 0));

            // the returns
            fx::SpaceSettings s;
            const auto& sc = cfg->space;
            for (std::size_t i = 0; i < 8; ++i)
            {
                s.delaysFrames[i] = sc.delaysFrames[i];
                s.signs[i] = sc.signs[i];
            }
            s.t60S = static_cast<double> (sc.t60Ms) / 1000.0;
            s.t60LowS = static_cast<double> (sc.t60LowMs) / 1000.0;
            s.lowHz = static_cast<double> (sc.lowHz);
            s.dampHz = static_cast<double> (sc.dampHz);
            s.predelayS = static_cast<double> (sc.predelayUs) * 1e-6;
            s.sendHpHz = static_cast<double> (sc.sendHpHz);
            s.sendOrder = sc.sendOrder;
            s.shimmer = static_cast<double> (sc.shimmerPm) / 1000.0;
            s.shimmerLoHz = static_cast<double> (sc.shimmerLoHz);
            s.shimmerHiHz = static_cast<double> (sc.shimmerHiHz);
            s.windowFrames = sc.windowFrames;
            s.monoHz = static_cast<double> (sc.monoHz);
            space.setOrigin (a1);
            space.setup (s);
            returnOn[0] = std::any_of (sc.delaysFrames.begin(), sc.delaysFrames.end(), [] (std::int32_t d) { return d > 0; });
            returnGain[0] = mdbToLinear (sc.returnMdb);

            fx::EchoSettings e;
            const auto& ec = cfg->echo;
            e.steps = std::clamp (ec.steps, 1, fx::Echo::kMaxSteps);
            e.feedback = static_cast<double> (ec.feedbackPm) / 1000.0;
            e.lpHz = static_cast<double> (ec.lpHz);
            e.hpHz = static_cast<double> (ec.hpHz);
            e.drive = static_cast<double> (ec.drivePm) / 1000.0;
            e.wow = static_cast<double> (ec.wowPpm) * 1e-6;
            e.wowHz = static_cast<double> (ec.wowMhz) / 1000.0;
            e.flutter = static_cast<double> (ec.flutterPpm) * 1e-6;
            e.flutterHz = static_cast<double> (ec.flutterMhz) / 1000.0;
            e.wowPhase = static_cast<double> (ec.phasesMrad[0]) / 1000.0;
            e.flutterPhase = static_cast<double> (ec.phasesMrad[1]) / 1000.0;
            echo.setOrigin (a1);
            echo.setup (e);
            returnOn[1] = ec.lpHz > 0 && ec.hpHz > 0;
            returnGain[1] = mdbToLinear (ec.returnMdb);

            fx::ResonatorSettings rs;
            const auto& rc = cfg->resonator;
            rs.tones = std::min (static_cast<int> (rc.tonesMhz.size()), fx::kMaxResonatorTones);
            for (int i = 0; i < rs.tones; ++i)
                rs.tonesHz[static_cast<std::size_t> (i)] = static_cast<double> (rc.tonesMhz[static_cast<std::size_t> (i)]) / 1000.0;
            rs.tauS = static_cast<double> (rc.tauMs) / 1000.0;
            rs.sendHpHz = static_cast<double> (rc.sendHpHz);
            resonator.setup (rs);
            returnOn[2] = resonator.active();
            returnGain[2] = mdbToLinear (rc.returnMdb);

            master.setup (cfg->master, controls, kMixLatency);
        }
        reset (controls);
    }

    void MixGraph::reset (const Controls& controls) noexcept
    {
        for (int i = 0; i < slotsUsed; ++i)
            slots[static_cast<std::size_t> (i)].reset (controls);
        // reset () undoes nothing planned: the processors keep their delays, keys and origins
        for (int c = 0; c < numChannels; ++c)
        {
            auto& ch = channels[static_cast<std::size_t> (c)];
            ch.align.reset();
            ch.level.jump (static_cast<double> (ch.lane < kLanes ? controls.laneGain[ch.lane] : 0.0f));
        }
        for (auto* strips : { &groups, &busEq })
            for (auto& s : *strips)
                s.align.reset();
        for (auto& r : returns)
            r.align.reset();
        for (auto& t : throwAlign)
            t.reset();
        noneAlign.reset();
        preMaster.reset();
        space.reset();
        echo.reset();
        resonator.reset();
        if (config != nullptr)
            master.reset (controls);
        spaceMacro.jump (static_cast<double> (controls.space));
        echoMacro.jump (static_cast<double> (controls.echo));
        pumpMacro.jump (static_cast<double> (controls.pump));
        busMix.jump (controls.variant == Variant::novox ? 1.0 : 0.0);
        keysWritten = 0;
        ccWritten.fill (0);
        std::fill (keyRingData.begin(), keyRingData.end(), 0.0f);
        plainL.reset();
        plainR.reset();
        clock = 0;
        meterValues = {};
    }

    void MixGraph::keyHit (std::uint8_t lane, int velocity, int offset) noexcept
    {
        auto& e = keys[keysWritten % static_cast<std::uint64_t> (kKeyEvents)];
        e.time = clock + std::max (offset, 0);
        e.lane = lane;
        e.velocity = static_cast<std::uint8_t> (std::clamp (velocity, 0, 127));
        ++keysWritten;
    }

    void MixGraph::cc (int channel, int controller, int value, int offset, int rampSamples) noexcept
    {
        if (channel < 0 || channel >= kMaxMixChannels)
            return;
        auto& written = ccWritten[static_cast<std::size_t> (channel)];
        auto& e = ccEvents[static_cast<std::size_t> (channel * kCcEvents) + written % static_cast<std::uint64_t> (kCcEvents)];
        e.time = clock + std::max (offset, 0);
        e.controller = static_cast<std::uint8_t> (std::clamp (controller, 0, 127));
        e.value = static_cast<std::uint8_t> (std::clamp (value, 0, 127));
        e.ramp = std::max (rampSamples, 0);
        ++written;
    }

    BlockContext MixGraph::context (int n, const Controls& controls) noexcept
    {
        BlockContext ctx;
        ctx.start = clock;
        ctx.n = n;
        ctx.fs = fs;
        ctx.controls = &controls;
        ctx.pump = pump;
        ctx.keys = keys.data();
        ctx.keysWritten = keysWritten;
        ctx.keyRings = keyRings.data();
        ctx.scratch.bypassL[0] = bypass[0][0];
        ctx.scratch.bypassR[0] = bypass[0][1];
        ctx.scratch.bypassL[1] = bypass[1][0];
        ctx.scratch.bypassR[1] = bypass[1][1];
        ctx.scratch.parallelL = parallel[0];
        ctx.scratch.parallelR = parallel[1];
        return ctx;
    }

    void MixGraph::mixInto (float* dl, float* dr, const float* sl, const float* sr, int n) noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            dl[i] += sl[i];
            dr[i] += sr[i];
        }
    }

    // A channel after its inserts: the key ring, the alignment, the lane's level, the meters, the routing.
    void MixGraph::routeChannel (int c, float* l, float* r, int n, const Controls& controls) noexcept
    {
        auto& ch = channels[static_cast<std::size_t> (c)];
        if (ch.key)
        {
            float* ring = keyRingData.data() + static_cast<std::size_t> (c) * kKeyRing;
            for (int i = 0; i < n; ++i)
                ring[static_cast<std::size_t> (clock + i) & (kKeyRing - 1)] = 0.5f * (l[i] + r[i]);
        }
        ch.align.process (l, r, n);
        ch.level.moveTo (static_cast<double> (ch.lane < kLanes ? controls.laneGain[ch.lane] : 0.0f));

        float* dl = ch.group == Group::drums ? groupIn[0][0] : ch.group == Group::music ? groupIn[1][0] : noneIn[0];
        float* dr = ch.group == Group::drums ? groupIn[0][1] : ch.group == Group::music ? groupIn[1][1] : noneIn[1];
        float peak = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const double g = ch.level.next();
            const auto yl = static_cast<float> (static_cast<double> (l[i]) * g);
            const auto yr = static_cast<float> (static_cast<double> (r[i]) * g);
            dl[i] += yl;
            dr[i] += yr;
            peak = std::max (peak, std::max (std::abs (yl), std::abs (yr)));
            const double mono = 0.5 * (static_cast<double> (yl) + static_cast<double> (yr));
            for (std::size_t s = 0; s < static_cast<std::size_t> (kSends); ++s)
                if (ch.sends[s] > 0.0)
                    sendIn[s][i] += static_cast<float> (mono * ch.sends[s]);
        }
        if (ch.lane < kLanes)
            meterValues.lanePeak[ch.lane] = std::max (meterValues.lanePeak[ch.lane], peak);
    }

    void MixGraph::process (MixBus& bus, SendBus& sends, float* outL, float* outR, int n, const Controls& controls, double bpm) noexcept
    {
        n = std::clamp (n, 0, maxBlock);
        if (n == 0)
            return;
        meterValues.lanePeak.fill (0.0f);

        if (config == nullptr)
        {
            // every bus at unity, as late as a mix would be
            for (int i = 0; i < n; ++i)
            {
                double l = 0.0, r = 0.0;
                for (int c = 0; c < bus.numChannels(); ++c)
                {
                    l += static_cast<double> (bus.left (c)[i]);
                    r += static_cast<double> (bus.right (c)[i]);
                }
                plainL.push (static_cast<float> (l));
                plainR.push (static_cast<float> (r));
                mixL[i] = plainL.delayed (latency());
                mixR[i] = plainR.delayed (latency());
            }
        }
        else
        {
            const auto count = static_cast<std::size_t> (n);
            for (float* b : { groupIn[0][0], groupIn[0][1], groupIn[1][0], groupIn[1][1], noneIn[0], noneIn[1], mixL, mixR })
                std::fill_n (b, count, 0.0f);
            for (float* s : sendIn)
                std::fill_n (s, count, 0.0f);
            pumpMacro.moveTo (static_cast<double> (controls.pump));
            for (int i = 0; i < n; ++i)
                pump[i] = static_cast<float> (pumpMacro.next());

            auto ctx = context (n, controls);

            // the channels, keys first
            for (int i = 0; i < numChannels; ++i)
            {
                const int c = order[static_cast<std::size_t> (i)];
                auto& ch = channels[static_cast<std::size_t> (c)];
                ctx.cc = ccEvents.data() + static_cast<std::size_t> (c * kCcEvents);
                ctx.ccWritten = ccWritten[static_cast<std::size_t> (c)];
                float* l = bus.left (c);
                float* r = bus.right (c);
                processChain (slots.data() + ch.firstSlot, ch.slotCount, l, r, n, ctx);
                routeChannel (c, l, r, n, controls);
            }
            ctx.cc = nullptr;
            ctx.ccWritten = 0;

            // the throws join the sends as late as the channels
            for (std::size_t s = 0; s < static_cast<std::size_t> (kSends); ++s)
            {
                std::copy_n (sends.send (static_cast<Send> (s)), count, throwBuf);
                delayMono (throwAlign[s], throwBuf, n);
                for (int i = 0; i < n; ++i)
                    sendIn[s][i] += throwBuf[i];
            }

            // the groups, and the channels that go straight to the bus
            for (std::size_t g = 0; g < 2; ++g)
            {
                auto& strip = groups[g];
                processChain (slots.data() + strip.firstSlot, strip.slotCount, groupIn[g][0], groupIn[g][1], n, ctx);
                strip.align.process (groupIn[g][0], groupIn[g][1], n);
                mixInto (mixL, mixR, groupIn[g][0], groupIn[g][1], n);
            }
            noneAlign.process (noneIn[0], noneIn[1], n);
            mixInto (mixL, mixR, noneIn[0], noneIn[1], n);

            // the returns: each at its level times its macro (Space scales the Space and the Resonator, Echo the Echo)
            echo.setTempo (bpm);
            spaceMacro.moveTo (static_cast<double> (controls.space));
            echoMacro.moveTo (static_cast<double> (controls.echo));
            for (int i = 0; i < n; ++i)
            {
                macro[0][i] = static_cast<float> (spaceMacro.next());
                macro[1][i] = static_cast<float> (echoMacro.next());
            }
            for (std::size_t s = 0; s < static_cast<std::size_t> (kSends); ++s)
            {
                if (! returnOn[s])
                    continue;
                if (s == 0)
                    space.process (sendIn[0], ret[0], ret[1], n);
                else if (s == 1)
                    echo.process (sendIn[1], ret[0], ret[1], n);
                else
                    resonator.process (sendIn[2], ret[0], ret[1], n);
                auto& strip = returns[s];
                processChain (slots.data() + strip.firstSlot, strip.slotCount, ret[0], ret[1], n, ctx);
                const float* m = macro[s == 1 ? 1 : 0];
                for (int i = 0; i < n; ++i)
                {
                    const double g = returnGain[s] * static_cast<double> (m[i]);
                    ret[0][i] = static_cast<float> (static_cast<double> (ret[0][i]) * g);
                    ret[1][i] = static_cast<float> (static_cast<double> (ret[1][i]) * g);
                }
                strip.align.process (ret[0], ret[1], n);
                mixInto (mixL, mixR, ret[0], ret[1], n);
            }

            // the bus EQ of each variant, crossfaded on a change
            std::copy_n (mixL, count, altL);
            std::copy_n (mixR, count, altR);
            processChain (slots.data() + busEq[0].firstSlot, busEq[0].slotCount, mixL, mixR, n, ctx);
            busEq[0].align.process (mixL, mixR, n);
            processChain (slots.data() + busEq[1].firstSlot, busEq[1].slotCount, altL, altR, n, ctx);
            busEq[1].align.process (altL, altR, n);
            busMix.moveTo (controls.variant == Variant::novox ? 1.0 : 0.0);
            for (int i = 0; i < n; ++i)
            {
                const double m = busMix.next();
                mixL[i] = static_cast<float> (static_cast<double> (mixL[i]) + m * (static_cast<double> (altL[i]) - static_cast<double> (mixL[i])));
                mixR[i] = static_cast<float> (static_cast<double> (mixR[i]) + m * (static_cast<double> (altR[i]) - static_cast<double> (mixR[i])));
            }

            preMaster.process (mixL, mixR, n);
            master.process (mixL, mixR, n, controls);
            meterValues.gainReduction = static_cast<float> (master.gainReductionDb());
        }

        if (outR != nullptr)
        {
            std::copy_n (mixL, static_cast<std::size_t> (n), outL);
            std::copy_n (mixR, static_cast<std::size_t> (n), outR);
        }
        else
            for (int i = 0; i < n; ++i)
                outL[i] = 0.5f * (mixL[i] + mixR[i]);
        clock += n;
    }
}
