#pragma once

// P7's test fixtures: a small kit manifest and mix, SMFs through core's writer, a Kit assembled from
// them (what P4's buildKit will do), and recording sinks for the scheduler and the router. Until
// P0's engine/testing lands, the timeline, scheduler, router and engine tests share these.
//
// The kit, "SYN07" at 120 BPM (a quarter is 24000 samples at 48 kHz, a 16th 6000, a bar 96000):
//   instrument      lane    mode          notes
//   0 kick_a        kick    one_shot      duck key; 2 round robins; ch 10 key 36, A side of kick_ab
//   1 kick_b        kick    one_shot      duck key; B side of kick_ab
//   2 clap          snare   one_shot      duck key; ch 10 key 38
//   3 hat_closed    hats    one_shot      chokes the open hat; ch 10 key 42; a 4-step pan cycle
//   4 hat_open      hats    one_shot      choked; ch 10 key 46
//   5 bass          bass    sustain_loop  mono; zones at 36, 48 and 60; ch 1 keys 24-71
//   6 pad           chords  sustain_loop  restrike-continues; ch 4 keys 48-72
//   7 vox           vox     gated         preroll 2400 frames (50 ms); ch 5 keys 48-84
//   8 riser         fx      one_shot      align end; ch 7 key 48
//   9 chop_a        vox     one_shot      preroll 960 frames; ch 8 key 48, A side of stab_ab
//  10 chop_b        vox     one_shot      preroll 480 frames; B side of stab_ab
// The mix has one channel per lane in use; the bass channel ducks on the kick lane with pre_us 5000
// and has a cc_filter, so CC74 on channel 1 reaches it.

#include "kit/kit.h"
#include "kit/manifest.h"
#include "mix/mix_config.h"
#include "midi/smf.h"
#include "midi/writer.h"
#include "sched/scheduler.h"
#include "sched/timeline.h"
#include "sched/timeline_builder.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <tuple>
#include <span>
#include <string>
#include <vector>

namespace p7
{
    using namespace wubansia;
    using namespace wubansia::engine;

    inline constexpr int kSynBpm = 120;
    inline constexpr int kSynSwingPm = 560; // 28.8 ticks, written 29

    namespace inst
    {
        inline constexpr int kickA = 0, kickB = 1, clap = 2, hatClosed = 3, hatOpen = 4, bass = 5, pad = 6, vox = 7,
                             riser = 8, chopA = 9, chopB = 10;
    }

    // ---- the manifests -------------------------------------------------------------------------

    inline FileEntry fileEntry (std::string path, std::int64_t frames, std::int64_t loopStart = 0, std::int64_t loopEnd = 0,
        std::int32_t preroll = 0)
    {
        FileEntry f;
        f.path = std::move (path);
        f.frames = frames;
        f.channels = 1;
        f.sha256 = std::string (64, '0');
        f.loopStart = loopStart;
        f.loopEnd = loopEnd;
        f.prerollFrames = preroll;
        return f;
    }

    inline InstrumentEntry instrument (std::string id, Lane lane, std::string mixChannel, SampleMode mode,
        std::vector<ZoneEntry> zones)
    {
        InstrumentEntry i;
        i.name = id;
        i.id = std::move (id);
        i.lane = lane;
        i.mixChannel = std::move (mixChannel);
        i.mode = mode;
        i.zones = std::move (zones);
        return i;
    }

    inline ZoneEntry zone (int root, int lo, int hi, std::vector<LayerEntry> layers)
    {
        ZoneEntry z;
        z.root = root;
        z.lo = lo;
        z.hi = hi;
        z.layers = std::move (layers);
        return z;
    }

    inline LayerEntry layer (int vel, std::vector<std::int32_t> rr)
    {
        LayerEntry l;
        l.vel = vel;
        l.rr = std::move (rr);
        return l;
    }

    inline KeymapEntry keys (int ch, int lo, int hi, int a, int b = -1, int ab = -1)
    {
        KeymapEntry k;
        k.ch = ch;
        k.lo = lo;
        k.hi = hi;
        k.a = a;
        k.b = b;
        k.ab = ab;
        return k;
    }

    inline KitManifest synManifest (int bpm = kSynBpm, int swingPm = kSynSwingPm)
    {
        KitManifest m;
        m.kit.id = "SYN07";
        m.kit.title = "Scheduler Test";
        m.kit.direction = "test";
        m.kit.bpm = bpm;
        m.kit.key = "A minor";
        m.kit.camelot = "8A";
        m.kit.swingPm = swingPm;
        m.kit.contentVersion = 1;
        m.kit.lanes = { "Kick", "Clap", "Hats", "", "Bass", "", "Pad", "Vox", "", "FX" };

        m.files = {
            fileEntry ("Drums/kick_a_rr1.wav", 4800), // 0
            fileEntry ("Drums/kick_a_rr2.wav", 4800), // 1
            fileEntry ("Drums/kick_b.wav", 4800), // 2
            fileEntry ("Drums/clap.wav", 3600), // 3
            fileEntry ("Drums/hat_closed.wav", 1200), // 4
            fileEntry ("Drums/hat_open.wav", 9600), // 5
            fileEntry ("Bass/bass_36.wav", 24000, 4800, 24000), // 6
            fileEntry ("Bass/bass_48.wav", 24000, 4800, 24000), // 7
            fileEntry ("Bass/bass_60.wav", 24000, 4800, 24000), // 8
            fileEntry ("Pad/pad.wav", 96000, 48000, 96000), // 9
            fileEntry ("Vox/vox.wav", 24000, 0, 0, 2400), // 10
            fileEntry ("FX/riser.wav", 48000), // 11
            fileEntry ("Vox/chop_a.wav", 9600, 0, 0, 960), // 12
            fileEntry ("Vox/chop_b.wav", 9600, 0, 0, 480), // 13
        };

        auto kickA = instrument ("kick_a", Lane::kick, "kick", SampleMode::oneShot, { zone (36, 36, 36, { layer (127, { 0, 1 }) }) });
        kickA.duckKey = true;
        auto kickB = instrument ("kick_b", Lane::kick, "kick", SampleMode::oneShot, { zone (36, 36, 36, { layer (127, { 2 }) }) });
        kickB.duckKey = true;
        auto clap = instrument ("clap", Lane::snare, "snare", SampleMode::oneShot, { zone (38, 38, 38, { layer (100, { 3 }) }) });
        clap.duckKey = true;
        auto hatClosed = instrument ("hat_closed", Lane::hats, "hats", SampleMode::oneShot, { zone (42, 42, 42, { layer (127, { 4 }) }) });
        hatClosed.choke = { "hats", ChokeRole::chokes, 4000, ChokeCurve::cos2 };
        hatClosed.panCyclePm = { -300, 300, -150, 150 };
        hatClosed.rrBlock16ths = 16; // round robins and pans restart every bar
        auto hatOpen = instrument ("hat_open", Lane::hats, "hats", SampleMode::oneShot, { zone (46, 46, 46, { layer (127, { 5 }) }) });
        hatOpen.choke = { "hats", ChokeRole::choked, 4000, ChokeCurve::cos2 };
        auto bass = instrument ("bass", Lane::bass, "bass", SampleMode::sustainLoop,
            { zone (36, 24, 47, { layer (127, { 6 }) }), zone (48, 48, 59, { layer (127, { 7 }) }), zone (60, 60, 71, { layer (127, { 8 }) }) });
        bass.mono = true;
        bass.releaseUs = 5000;
        bass.velocityLaw = VelocityLaw::pow15;
        auto pad = instrument ("pad", Lane::chords, "chords", SampleMode::sustainLoop, { zone (60, 48, 72, { layer (127, { 9 }) }) });
        pad.restrikeContinues = true;
        pad.releaseUs = 20000;
        auto vox = instrument ("vox", Lane::vox, "vox", SampleMode::gated, { zone (60, 48, 84, { layer (127, { 10 }) }) });
        vox.releaseUs = 10000;
        auto riser = instrument ("riser", Lane::fx, "fx", SampleMode::oneShot, { zone (48, 48, 48, { layer (127, { 11 }) }) });
        riser.align = Align::end;
        auto chopA = instrument ("chop_a", Lane::vox, "vox", SampleMode::oneShot, { zone (48, 48, 48, { layer (127, { 12 }) }) });
        auto chopB = instrument ("chop_b", Lane::vox, "vox", SampleMode::oneShot, { zone (48, 48, 48, { layer (127, { 13 }) }) });
        m.instruments = { kickA, kickB, clap, hatClosed, hatOpen, bass, pad, vox, riser, chopA, chopB };

        m.keymap = {
            keys (1, 24, 71, inst::bass),
            keys (4, 48, 72, inst::pad),
            keys (5, 48, 84, inst::vox),
            keys (7, 48, 48, inst::riser),
            keys (8, 48, 48, inst::chopA, inst::chopB, static_cast<int> (AbSwitch::stab)),
            keys (10, 36, 36, inst::kickA, inst::kickB, static_cast<int> (AbSwitch::kick)),
            keys (10, 38, 38, inst::clap),
            keys (10, 42, 42, inst::hatClosed),
            keys (10, 46, 46, inst::hatOpen),
        };
        return m;
    }

    inline Insert onsetDuck (std::uint16_t keyLanes, std::int32_t preUs)
    {
        DuckParams d;
        d.mode = DuckMode::onset;
        d.keyLanes = keyLanes;
        d.minVelocity = 1;
        d.depthMdb = 6000;
        d.rampUs = 2000;
        d.holdUs = 10000;
        d.releaseUs = 100000;
        d.preUs = preUs;
        Insert i;
        i.type = InsertType::duck;
        i.params = d;
        return i;
    }

    inline MixConfig synMix (std::int32_t kickPreUs = 5000)
    {
        MixConfig mix;
        mix.kit = "SYN07";
        mix.contentVersion = 1;
        auto channel = [] (std::string id, Lane lane) {
            Channel c;
            c.id = std::move (id);
            c.lane = static_cast<std::uint8_t> (lane);
            return c;
        };
        mix.channels = { channel ("kick", Lane::kick), channel ("snare", Lane::snare), channel ("hats", Lane::hats),
            channel ("bass", Lane::bass), channel ("chords", Lane::chords), channel ("vox", Lane::vox), channel ("fx", Lane::fx) };
        mix.channels[3].inserts.push_back (onsetDuck (static_cast<std::uint16_t> (laneBit (Lane::kick)), kickPreUs));
        CcFilterParams f;
        f.cc74LoHz = 200;
        f.cc74HiHz = 8000;
        Insert cc;
        cc.type = InsertType::ccFilter;
        cc.params = f;
        mix.channels[3].inserts.push_back (cc);
        return mix;
    }

    // ---- SMFs ----------------------------------------------------------------------------------

    // A file in the content's shape: a conductor (name, tempo, 4/4, markers) and the given tracks, every
    // track ending on endTick.
    inline std::vector<std::uint8_t> smf (std::vector<midi::TrackSpec> tracks, std::int64_t endTick, int bpm = kSynBpm,
        std::vector<midi::Marker> markers = {})
    {
        midi::SongSpec spec;
        spec.ppq = 960;
        spec.endTick = endTick;
        spec.name = "SYN07 test";
        spec.tempos = { { 0, midi::microsPerQuarter (bpm) } };
        spec.timeSignatures = { { 0, 4, 2, 24, 8 } };
        spec.markers = std::move (markers);
        spec.tracks = std::move (tracks);
        auto bytes = midi::writeSmf (spec);
        return bytes.ok() ? std::move (bytes.value) : std::vector<std::uint8_t> {};
    }

    // channel is 1-16, as the files' maps count it.
    inline midi::Note note (std::int64_t tick, std::int64_t length, int channel, int key, int velocity = 100)
    {
        midi::Note n;
        n.tick = tick;
        n.length = length;
        n.channel = static_cast<std::uint8_t> (channel - 1);
        n.key = static_cast<std::uint8_t> (key);
        n.velocity = static_cast<std::uint8_t> (velocity);
        n.releaseVelocity = 0;
        return n;
    }

    inline midi::Controller cc (std::int64_t tick, int channel, int number, int value)
    {
        return { tick, static_cast<std::uint8_t> (channel - 1), static_cast<std::uint8_t> (number), static_cast<std::uint8_t> (value) };
    }

    inline midi::TrackSpec track (std::string name, std::vector<midi::Note> notes, std::vector<midi::Controller> controllers = {})
    {
        midi::TrackSpec t;
        t.name = std::move (name);
        t.notes = std::move (notes);
        t.controllers = std::move (controllers);
        return t;
    }

    inline Timeline build (const std::vector<std::uint8_t>& bytes, const KitManifest& m, const MixConfig& mix, std::int64_t loopTicks)
    {
        TimelineOptions options;
        options.loopTicks = loopTicks;
        auto t = buildTimeline (bytes, m, mix, options);
        return t.ok() ? std::move (t.value) : Timeline {};
    }

    // The default 4-bar loop: a kick on every beat, the clap on 2 and 4, closed hats on every 8th with an
    // open hat on the last, a bass line (the swung 16ths written 29 ticks late), a 4-bar pad, a vox note,
    // a riser ending on the loop's end, and a CC74 ride on the bass channel.
    inline std::vector<std::uint8_t> synLoopSmf()
    {
        std::vector<midi::Note> kick, clap, hats, bass, pad, vox, fx;
        for (int beat = 0; beat < 16; ++beat)
            kick.push_back (note (beat * 960, 120, 10, 36, 127));
        for (int beat = 1; beat < 16; beat += 2)
            clap.push_back (note (beat * 960, 120, 10, 38, 100));
        for (int e = 0; e < 31; ++e)
            hats.push_back (note (e * 480, 120, 10, 42, 90));
        hats.push_back (note (31 * 480, 240, 10, 46, 90));
        const int line[] = { 36, 36, 48, 43, 41, 48, 36, 38 };
        for (int bar = 0; bar < 4; ++bar)
            for (int i = 0; i < 8; ++i)
            {
                const std::int64_t tick = bar * 3840 + i * 480 + (i % 2 == 1 ? 240 + 29 : 0);
                bass.push_back (note (tick, 200, 1, line[i], 100 + i));
            }
        pad.push_back (note (0, 15360, 4, 57, 100));
        vox.push_back (note (7680, 1440, 5, 64, 110));
        fx.push_back (note (11520, 3840, 7, 48, 127));
        std::vector<midi::Controller> ride;
        for (int t = 0; t < 15360; t += 60)
            ride.push_back (cc (t, 1, 74, (t / 60) % 128));
        return smf ({ track ("Kick", kick), track ("Clap", clap), track ("Hats", hats), track ("Bass", bass, ride),
                        track ("Chords", pad), track ("Vox", vox), track ("FX", fx) },
            15360);
    }

    // ---- an assembled Kit (P4's buildKit, by hand) -----------------------------------------------

    // Deterministic audio for file i: a decaying sine with a pitch of its own, so voices differ.
    inline SampleData audioFor (std::size_t i, std::int64_t frames)
    {
        SampleData d;
        d.frames = frames;
        d.channels = 1;
        d.audio.resize (static_cast<std::size_t> (frames));
        const double f = 110.0 * (1.0 + 0.25 * static_cast<double> (i));
        for (std::int64_t n = 0; n < frames; ++n)
        {
            const double t = static_cast<double> (n) / 48000.0;
            d.audio[static_cast<std::size_t> (n)] = static_cast<float> (0.25 * std::sin (6.283185307179586 * f * t) * std::exp (-3.0 * t));
        }
        return d;
    }

    inline Kit assemble (const KitManifest& m, MixConfig mix, std::array<Timeline, 4> timelines, int index = 0)
    {
        Kit kit;
        kit.info.id = m.kit.id;
        kit.info.title = m.kit.title;
        kit.info.direction = m.kit.direction;
        kit.info.key = m.kit.key;
        kit.info.camelot = m.kit.camelot;
        kit.info.bpm = m.kit.bpm;
        kit.info.swingPm = m.kit.swingPm;
        kit.info.contentVersion = m.kit.contentVersion;
        kit.info.index = index;
        kit.info.laneLabels = m.kit.lanes;
        for (int lane = 0; lane < kLanes; ++lane)
            if (! m.kit.lanes[static_cast<std::size_t> (lane)].empty())
                kit.info.lanes |= 1u << lane;

        for (std::size_t i = 0; i < m.files.size(); ++i)
        {
            const auto& f = m.files[i];
            kit.files.push_back ({ f.path, f.frames, f.channels, f.loopStart, f.loopEnd, f.prerollFrames });
            kit.audio.push_back (audioFor (i, f.frames));
        }

        std::vector<std::string> chokeGroups;
        for (const auto& e : m.instruments)
        {
            Instrument in;
            in.id = e.id;
            in.name = e.name;
            in.lane = e.lane;
            in.mixChannel = static_cast<std::uint8_t> (std::max (0, mix.channelIndex (e.mixChannel)));
            in.mode = e.mode;
            in.align = e.align;
            in.attackUs = e.attackUs;
            in.releaseUs = e.releaseUs;
            in.gainMdb = e.gainMdb;
            in.startFrames = e.startFrames;
            in.velocityLaw = e.velocityLaw;
            in.interp = e.interp;
            in.mono = e.mono;
            in.glide = e.glide;
            in.restrikeContinues = e.restrikeContinues;
            if (e.choke.role != ChokeRole::none)
            {
                auto it = std::find (chokeGroups.begin(), chokeGroups.end(), e.choke.group);
                if (it == chokeGroups.end())
                {
                    chokeGroups.push_back (e.choke.group);
                    it = chokeGroups.end() - 1;
                }
                in.choke.group = static_cast<std::uint8_t> (1 + (it - chokeGroups.begin()));
            }
            in.choke.role = e.choke.role;
            in.choke.fadeUs = e.choke.fadeUs;
            in.choke.curve = e.choke.curve;
            in.duckKey = e.duckKey;
            in.firstPan = static_cast<std::uint16_t> (kit.pans.size());
            in.panCount = static_cast<std::uint8_t> (e.panCyclePm.size());
            for (const auto p : e.panCyclePm)
                kit.pans.push_back (static_cast<std::int16_t> (p));
            in.rrBlock16ths = e.rrBlock16ths;
            in.layeredWith = static_cast<std::int16_t> (e.layeredWith);
            in.firstZone = static_cast<std::uint16_t> (kit.zones.size());
            in.zoneCount = static_cast<std::uint8_t> (e.zones.size());
            for (const auto& z : e.zones)
            {
                Zone zone;
                zone.root = static_cast<std::uint8_t> (z.root);
                zone.lo = static_cast<std::uint8_t> (z.lo);
                zone.hi = static_cast<std::uint8_t> (z.hi);
                zone.firstLayer = static_cast<std::uint16_t> (kit.layers.size());
                zone.layerCount = static_cast<std::uint8_t> (z.layers.size());
                for (const auto& l : z.layers)
                {
                    Layer layer;
                    layer.velocity = static_cast<std::uint8_t> (l.vel);
                    layer.firstRr = static_cast<std::uint16_t> (kit.rr.size());
                    layer.rrCount = static_cast<std::uint8_t> (l.rr.size());
                    for (const auto f : l.rr)
                        kit.rr.push_back (static_cast<std::uint16_t> (f));
                    kit.layers.push_back (layer);
                }
                kit.zones.push_back (zone);
            }
            kit.instruments.push_back (in);
        }

        for (const auto& k : m.keymap)
            for (int key = k.lo; key <= k.hi; ++key)
            {
                auto& t = kit.keys[static_cast<std::size_t> (k.ch - 1)][static_cast<std::size_t> (key)];
                t.a = static_cast<std::int16_t> (k.a);
                t.b = static_cast<std::int16_t> (k.b);
                t.ab = static_cast<std::int8_t> (k.ab);
                for (const int i : { k.a, k.b })
                    if (i >= 0)
                        kit.ccMixChannels[static_cast<std::size_t> (k.ch - 1)] |= 1u << kit.instruments[static_cast<std::size_t> (i)].mixChannel;
            }

        kit.mix = std::move (mix);
        kit.timelines = std::move (timelines);
        return kit;
    }

    // The synthetic kit with its loop as all four timelines, or the given ones.
    inline Kit synKit (int index = 0)
    {
        const auto m = synManifest();
        const auto mix = synMix();
        const auto loop = build (synLoopSmf(), m, mix, 15360);
        return assemble (m, mix, { loop, loop, loop, loop }, index);
    }

    // ---- a recording Sink for the scheduler ------------------------------------------------------

    struct Event
    {
        enum class Kind : std::uint8_t {
            stop,
            off,
            cc,
            key,
            on
        };

        Kind kind = Kind::on;
        std::int64_t sample = 0; // absolute: the scheduler's sample index
        std::uint8_t channel = 0;
        std::uint8_t key = 0; // the key; a cc's controller; a duck key's lane
        std::uint8_t value = 0; // a note's velocity; a cc's value; a duck key's velocity
        std::int64_t length = 0; // a note-on's lengthSamples; a cc's ramp
        std::int64_t chase = 0; // a note-on's chaseSamples
        std::int64_t pos = 0; // a note-on's timeline position
        int tag = -1;

        friend bool operator== (const Event&, const Event&) = default;
    };

    // Records what the scheduler tells a Sink, at absolute samples. Set `base` to the scheduler's sample
    // index before each sync () and advance ().
    struct RecordingSink
    {
        std::int64_t base = 0;
        std::vector<Event> events;
        int nextTag = 0;
        int sounding = 0; // note-ons less note-offs since the last stopSequenced
        bool playNothing = false; // noteOn returns -1

        int noteOn (const Note& n, int offset, std::int64_t lengthSamples, std::int64_t chaseSamples) noexcept
        {
            Event e;
            e.kind = Event::Kind::on;
            e.sample = base + offset;
            e.channel = n.channel;
            e.key = n.key;
            e.value = n.velocity;
            e.length = lengthSamples;
            e.chase = chaseSamples;
            e.pos = n.pos;
            if (playNothing)
            {
                events.push_back (e);
                return -1;
            }
            e.tag = nextTag++;
            events.push_back (e);
            ++sounding;
            return e.tag;
        }

        void noteOff (std::uint8_t channel, std::uint8_t key, int tag, int offset) noexcept
        {
            Event e;
            e.kind = Event::Kind::off;
            e.sample = base + offset;
            e.channel = channel;
            e.key = key;
            e.tag = tag;
            events.push_back (e);
            --sounding;
        }

        void cc (std::uint8_t channel, std::uint8_t controller, std::uint8_t value, int offset, int rampSamples) noexcept
        {
            Event e;
            e.kind = Event::Kind::cc;
            e.sample = base + offset;
            e.channel = channel;
            e.key = controller;
            e.value = value;
            e.length = rampSamples;
            events.push_back (e);
        }

        void keyHit (std::uint8_t lane, std::uint8_t velocity, int offset) noexcept
        {
            Event e;
            e.kind = Event::Kind::key;
            e.sample = base + offset;
            e.key = lane;
            e.value = velocity;
            events.push_back (e);
        }

        void stopSequenced (int offset) noexcept
        {
            Event e;
            e.kind = Event::Kind::stop;
            e.sample = base + offset;
            events.push_back (e);
            sounding = 0;
        }

        std::vector<Event> only (Event::Kind kind) const
        {
            std::vector<Event> out;
            for (const auto& e : events)
                if (e.kind == kind)
                    out.push_back (e);
            return out;
        }
    };

    // The scheduler as an engine-like type for P0's FakeHost: each block syncs, then advances in pieces
    // of at most `piece` samples on the absolute 32-sample grid (0: the whole block), as the engine does.
    struct SchedulerHarness
    {
        Scheduler scheduler;
        RecordingSink sink;
        const Timeline* timeline = nullptr;
        double defaultBpm = kSynBpm;
        int piece = 32;
        std::vector<SyncEvent> syncs;

        void prepare (double rate, int, int) { scheduler.prepare (rate); }

        void process (float* const* out, int numOutputs, int n, const TransportInfo& transport, const Controls&, const MidiEvent*, int)
        {
            for (int c = 0; c < numOutputs; ++c)
                if (out[c] != nullptr)
                    std::fill_n (out[c], n, 0.0f);
            sink.base = scheduler.sampleIndex();
            syncs.push_back (scheduler.sync (timeline, transport, defaultBpm, sink));
            int done = 0;
            while (done < n)
            {
                int k = n - done;
                if (piece > 0)
                {
                    const auto toGrid = piece - static_cast<int> (scheduler.sampleIndex() % piece);
                    k = std::min (k, toGrid);
                }
                sink.base = scheduler.sampleIndex();
                scheduler.advance (timeline, k, sink);
                done += k;
            }
        }
    };
}
