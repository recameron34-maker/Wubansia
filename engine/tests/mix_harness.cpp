#include "mix_harness.h"

#include <algorithm>
#include <cmath>

namespace mixtest
{
    namespace
    {
        std::int32_t mdb (double db) { return static_cast<std::int32_t> (std::lround (db * 1000.0)); }
        std::int32_t micros (double s) { return static_cast<std::int32_t> (std::lround (s * 1e6)); }
    }

    Insert gain (double db, When when)
    {
        Insert i;
        i.type = InsertType::gain;
        i.when = when;
        i.params = GainParams { mdb (db) };
        return i;
    }

    Insert clip (double ceilingDb, double kneeDb, When when)
    {
        Insert i;
        i.type = InsertType::clip;
        i.when = when;
        i.params = ClipParams { mdb (ceilingDb), mdb (kneeDb) };
        return i;
    }

    Insert comp (double thresholdDb, double makeupDb)
    {
        Insert i;
        i.type = InsertType::comp;
        CompParams p;
        p.thresholdMdb = mdb (thresholdDb);
        p.ratioPm = 2000;
        p.kneeMdb = 6000;
        p.makeupMdb = mdb (makeupDb);
        i.params = p;
        return i;
    }

    Insert onsetDuck (std::uint16_t keyLanes, double depthDb, double rampS, double holdS, double releaseS, bool pump, Band band)
    {
        Insert i;
        i.type = InsertType::duck;
        DuckParams p;
        p.mode = DuckMode::onset;
        p.keyLanes = keyLanes;
        p.minVelocity = 1;
        p.depthMdb = mdb (depthDb);
        p.rampUs = micros (rampS);
        p.holdUs = micros (holdS);
        p.releaseUs = micros (releaseS);
        p.pump = pump;
        p.band = band;
        i.params = p;
        return i;
    }

    Insert levelDuck (int keyChannel, double depthDb, double rangeDb, double topDb, double attackS, double releaseS, Band band)
    {
        Insert i;
        i.type = InsertType::duck;
        DuckParams p;
        p.mode = DuckMode::level;
        p.keyChannel = keyChannel;
        p.depthMdb = mdb (depthDb);
        p.rangeMdb = mdb (rangeDb);
        p.topMdb = mdb (topDb);
        p.attackUs = micros (attackS);
        p.releaseUs = micros (releaseS);
        p.band = band;
        i.params = p;
        return i;
    }

    Insert ccFilter (int loHz, int hiHz, int qPm, int cc74Default)
    {
        Insert i;
        i.type = InsertType::ccFilter;
        CcFilterParams p;
        p.response = FilterResponse::lowpass;
        p.cc74LoHz = loHz;
        p.cc74HiHz = hiHz;
        p.cc74Default = cc74Default;
        p.cc71LoPm = qPm;
        p.cc71HiPm = qPm;
        p.cc71Default = 0;
        i.params = p;
        return i;
    }

    Insert parallel (double levelDb, std::vector<Insert> chain)
    {
        Insert i;
        i.type = InsertType::parallel;
        ParallelParams p;
        p.levelMdb = mdb (levelDb);
        p.chain = std::move (chain);
        i.params = std::move (p);
        return i;
    }

    Channel channel (const char* id, Lane lane, std::vector<Insert> inserts, Group group, std::array<std::int32_t, kSends> sendsPm)
    {
        Channel c;
        c.id = id;
        c.lane = static_cast<std::uint8_t> (lane);
        c.group = group;
        c.inserts = std::move (inserts);
        c.sendsPm = sendsPm;
        return c;
    }

    MixConfig makeMix (std::vector<Channel> channels)
    {
        MixConfig m;
        m.kit = "SYN01";
        m.contentVersion = 1;
        m.channels = std::move (channels);
        return m;
    }

    MixConfig busyMix()
    {
        auto m = makeMix ({
            channel ("kick", Lane::kick, { clip (-3.0), gain (-1.0) }, Group::drums, { 0, 0, 0 }),
            channel ("clap", Lane::snare, { gain (-2.0) }, Group::drums, { 300, 0, 0 }),
            channel ("bass", Lane::bass, { ccFilter (650, 2210, 1600, 64), onsetDuck (1u << 0, 6.0, 0.002, 0.04, 0.08, true) }, Group::music, { 0, 0, 0 }),
            channel ("vox", Lane::vox, { comp (-24.0, 2.0) }, Group::music, { 300, 150, 100 }),
            channel ("chords", Lane::chords, { levelDuck (3, 6.0, 18.0, -12.0, 0.01, 0.3, { 1200, 6500 }) }, Group::music, { 250, 0, 200 }),
        });
        m.drums.inserts = { clip (-2.0), parallel (-6.0, { gain (6.0), clip (-6.0) }) };
        auto& s = m.space;
        s.delaysFrames = { 520, 597, 685, 783, 899, 1033, 1167, 1329 };
        s.signs = { 1, -1, 1, -1, -1, 1, 1, -1 };
        s.t60Ms = 1100;
        s.t60LowMs = 400;
        s.lowHz = 500;
        s.dampHz = 8500;
        s.predelayUs = 27985;
        s.sendHpHz = 500;
        s.sendOrder = 4;
        s.shimmerPm = 30;
        s.shimmerLoHz = 300;
        s.shimmerHiHz = 7000;
        s.windowFrames = 2048;
        s.monoHz = 150;
        s.returnMdb = 3000;
        s.inserts = { onsetDuck (1u << 0, 3.0, 0.002, 0.06, 0.12, true), levelDuck (3, 6.0, 18.0, -12.0, 0.01, 0.3) };
        auto& e = m.echo;
        e.steps = 3;
        e.feedbackPm = 380;
        e.lpHz = 2600;
        e.hpHz = 600;
        e.drivePm = 1500;
        e.wowPpm = 400;
        e.wowMhz = 500;
        e.flutterPpm = 150;
        e.flutterMhz = 4700;
        e.phasesMrad = { 1234, 5678 };
        e.returnMdb = 3000;
        auto& r = m.resonator;
        r.tonesMhz = { 739989, 1108731, 1318510, 1661219, 1760000 };
        r.tauMs = 400;
        r.sendHpHz = 600;
        r.returnMdb = -21000;
        Insert eq;
        eq.type = InsertType::peak;
        eq.params = EqParams { 16000, -1500, 800 };
        m.busEq[0] = { eq };
        eq.params = EqParams { 3000, 3000, 1000 };
        m.busEq[1] = { eq };
        for (auto& v : m.master)
        {
            v.glueThresholdMdb = -18000;
            v.driveMdb = 3000;
        }
        m.master[1].glueThresholdMdb = -16000;
        return m;
    }

    std::vector<float> run (Mixer& mixer, int total, int block, int numOutputs, const std::function<float (int, std::int64_t, int)>& input,
        int channels, const std::vector<Event>& events, const std::function<Controls (std::int64_t)>& controls, double bpm)
    {
        std::vector<float> out (static_cast<std::size_t> (total * numOutputs));
        std::vector<float> l (static_cast<std::size_t> (block)), r (static_cast<std::size_t> (block));
        float* outs[2] { l.data(), r.data() };
        std::size_t next = 0;
        for (std::int64_t start = 0; start < total; start += block)
        {
            const int n = static_cast<int> (std::min<std::int64_t> (block, total - start));
            auto& bus = mixer.bus();
            bus.clear (n);
            mixer.sends().clear (n);
            for (int c = 0; c < channels; ++c)
                for (int i = 0; i < n; ++i)
                {
                    bus.left (c)[i] = input (c, start + i, 0);
                    bus.right (c)[i] = input (c, start + i, 1);
                }
            while (next < events.size() && events[next].time < start + n)
            {
                const auto& e = events[next++];
                const int offset = static_cast<int> (std::max<std::int64_t> (e.time - start, 0));
                if (e.kind == Event::Kind::key)
                    mixer.keyHit (static_cast<std::uint8_t> (e.a), e.b, offset);
                else
                    mixer.cc (e.a, e.b, e.c, offset, e.d);
            }
            const Controls c = controls ? controls (start) : Controls {};
            mixer.process (outs, numOutputs, n, c, bpm);
            for (int i = 0; i < n; ++i)
                for (int k = 0; k < numOutputs; ++k)
                    out[static_cast<std::size_t> ((start + i) * numOutputs + k)] = outs[k][i];
        }
        return out;
    }
}
