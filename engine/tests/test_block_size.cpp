// Block-size and seek invariance: event offsets follow the anchor, so the scheduler's events, the voices'
// note-ons and the engine's output are the same for blocks of 1, 17, 128, 512 and 2048 samples and random
// sizes, and a render started at bar 3 equals a render from bar 1 from bar 3 on.

#include "sched_fixtures.h"

#include "engine.h"
#include "engine_loop.h"
#include "testing/fake_host.h"
#include "testing/synthetic_kit.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstring>
#include <memory>
#include <random>
#include <vector>

using namespace p7;
using wubansia::engine::testing::FakeHost;
using Item = RecordingVoices::Item;

namespace
{
    // The block patterns: one size each, and a fixed random sequence of 1-2048.
    std::vector<std::vector<int>> patterns()
    {
        std::vector<std::vector<int>> out { { 1 }, { 17 }, { 128 }, { 512 }, { 2048 } };
        std::mt19937 random (20261105u);
        std::uniform_int_distribution<int> size (1, 2048);
        std::vector<int> mixed;
        for (int i = 0; i < 997; ++i)
            mixed.push_back (size (random));
        out.push_back (mixed);
        return out;
    }

    bool sameBits (const std::vector<float>& a, const std::vector<float>& b)
    {
        return a.size() == b.size() && (a.empty() || std::memcmp (a.data(), b.data(), a.size() * sizeof (float)) == 0);
    }

    const Timeline& loopTimeline()
    {
        static const Timeline t = build (synLoopSmf(), synManifest(), synMix(), 15360);
        return t;
    }

    const Timeline& songTimeline()
    {
        static const Timeline t = [] {
            std::vector<midi::Note> kick, bass;
            for (int beat = 0; beat < 32; ++beat)
                kick.push_back (note (beat * 960, 120, 10, 36, 127));
            for (int i = 0; i < 64; ++i)
                bass.push_back (note (i * 480 + (i % 2 == 1 ? 269 : 0), 200, 1, 36 + i % 12));
            return build (smf ({ track ("Kick", kick), track ("Bass", bass), track ("Chords", { note (0, 15360, 4, 57) }), track ("FX", { note (11520, 3840, 7, 48) }) },
                              30720, kSynBpm, { { 0, "Intro" }, { 15360, "Drop" } }),
                synManifest(), synMix(), 0);
        }();
        return t;
    }

    struct Setup
    {
        const Timeline* timeline;
        double fromPpq;
        bool hostLoop;
        const char* name;
    };

    std::vector<Event> schedule (const Setup& setup, const std::vector<int>& blocks, int piece, std::int64_t samples)
    {
        FakeHost host (44100.0, 2048);
        SchedulerHarness h;
        h.timeline = setup.timeline;
        h.piece = piece;
        host.prepare (h);
        host.setTempo (134.0);
        if (setup.hostLoop)
            host.setLoop (1.5, 9.25);
        host.play (setup.fromPpq);
        FakeHost::Output out;
        host.render (h, Controls {}, samples, out, blocks);
        return h.sink.events;
    }

    // Two kits to switch between, each with the loop for every mode.
    const Kit& kitA()
    {
        static const Kit k = synKit (0);
        return k;
    }

    const Kit& kitB()
    {
        static const Kit k = synKit (1);
        return k;
    }

    struct Render
    {
        FakeHost::Output out;
        std::vector<Item> log;
    };

    // The block loop on recording voices: kit A, MIDI in at fixed samples, kit B published 10000 samples
    // in (adopted on the next bar line).
    Render renderLoop (const std::vector<int>& blocks, std::int64_t samples)
    {
        LoopHarness<RecordingVoices> h;
        FakeHost host (44100.0, 2048);
        host.prepare (h);
        h.loop.voices().drone = 0.125f;
        h.pending.publish (&kitA());
        host.setTempo (134.0);
        host.play (0.0);
        for (std::int64_t s = 3000; s < samples; s += 20011)
        {
            host.addMidi (s, 0x91, 50, 100); // the lead's channel: nothing there in this kit
            host.addMidi (s + 7, 0x94, 64, 90); // the vox
            host.addMidi (s + 5000, 0x84, 64, 0);
            host.addMidi (s + 9, 0xB0, 74, static_cast<std::uint8_t> (s % 128));
        }
        Render r;
        host.render (h, Controls {}, 10000, r.out, blocks);
        h.pending.publish (&kitB());
        host.render (h, Controls {}, samples - 10000, r.out, blocks);
        r.log = h.loop.voices().log;
        return r;
    }
}

TEST_CASE ("the scheduler's events are the same for blocks of 1, 17, 128, 512, 2048 and random sizes", "[block][sched]")
{
    const Setup setups[] {
        { &loopTimeline(), 0.0, false, "loop" },
        { &loopTimeline(), 2.37, false, "loop from 2.37" },
        { &loopTimeline(), 0.0, true, "loop in a host loop region" },
        { &songTimeline(), -1.0, false, "song from -1" },
    };
    const std::int64_t samples = 2 * 315941; // two loops at 134 BPM, 44.1 kHz
    for (const auto& setup : setups)
        for (const int piece : { 32, 0 })
        {
            CAPTURE (setup.name, piece);
            const auto reference = schedule (setup, { 512 }, piece, samples);
            REQUIRE (reference.size() > 100u);
            for (const auto& blocks : patterns())
            {
                CAPTURE (blocks.front(), blocks.size());
                CHECK (schedule (setup, blocks, piece, samples) == reference);
            }
        }
}

TEST_CASE ("the engine's output and its voices' events are bit-identical for every block size, and on a second render", "[block][engine]")
{
    const std::int64_t samples = 2 * 315941;
    const auto reference = renderLoop ({ 512 }, samples);
    REQUIRE (reference.log.size() > 100u);
    CHECK (std::any_of (reference.log.begin(), reference.log.end(), [] (const Item& i) { return i.kind == Item::Kind::fadeKit; }));
    CHECK (std::any_of (reference.log.begin(), reference.log.end(), [] (const Item& i) { return i.note.source == Source::live; }));

    const auto again = renderLoop ({ 512 }, samples);
    CHECK (sameBits (again.out.left, reference.out.left));
    CHECK (sameBits (again.out.right, reference.out.right));

    for (const auto& blocks : patterns())
    {
        CAPTURE (blocks.front(), blocks.size());
        const auto r = renderLoop (blocks, samples);
        CHECK (sameBits (r.out.left, reference.out.left));
        CHECK (sameBits (r.out.right, reference.out.right));
        REQUIRE (r.log.size() == reference.log.size());
        for (std::size_t i = 0; i < r.log.size(); ++i)
        {
            const auto& x = r.log[i];
            const auto& y = reference.log[i];
            CAPTURE (i);
            CHECK (x.kind == y.kind);
            CHECK (x.sample == y.sample);
            CHECK (x.kit == y.kit);
            CHECK (x.note.instrument == y.note.instrument);
            CHECK (x.note.key == y.note.key);
            CHECK (x.note.startFrame == y.note.startFrame);
            CHECK (x.note.lengthSamples == y.note.lengthSamples);
            CHECK (x.instrument == y.instrument);
        }
    }
}

TEST_CASE ("a render started at bar 3 equals a render from bar 1 from bar 3 on", "[block][seek]")
{
    // The loop without the notes that cross bar 3 (the pad, the vox, the riser) and without its ride.
    std::vector<midi::Note> kick, clap, hats, bass;
    for (int beat = 0; beat < 16; ++beat)
        kick.push_back (note (beat * 960, 120, 10, 36, 127));
    for (int beat = 1; beat < 16; beat += 2)
        clap.push_back (note (beat * 960, 120, 10, 38, 100));
    for (int e = 0; e < 32; ++e)
        hats.push_back (note (e * 480, 120, 10, 42, 90));
    for (int i = 0; i < 32; ++i)
        bass.push_back (note (i * 480 + (i % 2 == 1 ? 269 : 0), 200, 1, 36 + i % 7, 100));
    const auto t = build (smf ({ track ("Kick", kick), track ("Clap", clap), track ("Hats", hats), track ("Bass", bass) }, 15360), synManifest(), synMix(), 15360);
    const Kit kit = assemble (synManifest(), synMix(), { t, t, t, t });

    auto render = [&kit] (double fromPpq, std::int64_t samples, int block) {
        LoopHarness<RecordingVoices> h;
        FakeHost host (48000.0, block);
        host.prepare (h);
        h.pending.publish (&kit);
        host.setTempo (120.0);
        host.play (fromPpq);
        Render r;
        host.render (h, Controls {}, samples, r.out);
        r.log = h.loop.voices().log;
        return r;
    };

    // At 120 BPM and 48 kHz bar 3 starts on sample 192000.
    for (const int block : { 512, 77 })
    {
        CAPTURE (block);
        const auto whole = render (0.0, 384000, block);
        const auto fromBar3 = render (8.0, 192000, block);
        const std::vector<float> tailLeft (whole.out.left.begin() + 192000, whole.out.left.end());
        const std::vector<float> tailRight (whole.out.right.begin() + 192000, whole.out.right.end());
        CHECK (sameBits (tailLeft, fromBar3.out.left));
        CHECK (sameBits (tailRight, fromBar3.out.right));

        std::vector<std::pair<std::int64_t, int>> a, b;
        for (const auto& i : whole.log)
            if (i.kind == Item::Kind::on && i.sample >= 192000)
                a.emplace_back (i.sample - 192000, i.note.instrument * 128 + i.note.key);
        for (const auto& i : fromBar3.log)
            if (i.kind == Item::Kind::on)
                b.emplace_back (i.sample, i.note.instrument * 128 + i.note.key);
        CHECK (a == b);
        CHECK (a.size() == 8u + 4u + 16u + 16u); // two bars of kicks, claps, hats and bass
    }
}

TEST_CASE ("the Engine renders P0's synthetic kit identically for every block size, and twice", "[block][engine]")
{
    // P0's SYN01 with P7's timelines. Until P5's voices land the sampler is silent, so this pins the
    // engine's path (and becomes the audio check when they do).
    std::shared_ptr<Kit> kit;
    {
        wubansia::engine::testing::SyntheticSampleSource source;
        const auto& content = wubansia::engine::testing::syntheticContent();
        auto laid = wubansia::engine::testing::layOutKit (content.kit, content.mix, source);
        REQUIRE (laid.ok());
        kit = laid.value;
        for (std::size_t i = 0; i < 4; ++i)
        {
            TimelineOptions options;
            options.loopTicks = i < 2 ? 15360 : 0;
            auto timeline = buildTimeline (content.smfs[i], content.kit, content.mix, options);
            REQUIRE (timeline.ok());
            REQUIRE_FALSE (timeline.value.notes.empty());
            kit->timelines[i] = std::move (timeline.value);
        }
    }

    auto render = [&kit] (const std::vector<int>& blocks) {
        Engine engine;
        FakeHost host (44100.0, 2048);
        host.prepare (engine);
        engine.publish (kit.get());
        host.setTempo (128.0);
        host.play (0.0);
        host.addMidi (1000, 0x99, 36, 100);
        host.addMidi (30000, 0x91, 40, 90);
        host.addMidi (50000, 0x81, 40, 0);
        Controls controls;
        controls.mode = PlayMode::song;
        FakeHost::Output out;
        host.render (engine, controls, 200000, out, blocks);
        return out;
    };
    const auto reference = render ({ 512 });
    const auto again = render ({ 512 });
    CHECK (sameBits (again.left, reference.left));
    CHECK (sameBits (again.right, reference.right));
    for (const auto& blocks : patterns())
    {
        CAPTURE (blocks.front(), blocks.size());
        const auto out = render (blocks);
        CHECK (sameBits (out.left, reference.left));
        CHECK (sameBits (out.right, reference.right));
    }
}
