// The scheduler (sched/scheduler.h) against P0's fake host: anchored offsets, loops, songs, jumps, host
// loop regions, tempo changes, odd tempos and metres, negative positions, starts and stops, switches,
// the internal clock, duck keys and ramps.

#include "sched_fixtures.h"

#include "sched/scheduler.h"
#include "testing/fake_host.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <set>
#include <vector>

using namespace p7;
using wubansia::engine::testing::FakeHost;

namespace
{
    const Timeline& synLoop()
    {
        static const Timeline t = build (synLoopSmf(), synManifest(), synMix(), 15360);
        return t;
    }

    // An 8-bar song: a kick on every beat, a pad over bars 1-4, a riser ending on bar 5, sections.
    const Timeline& synSong()
    {
        static const Timeline t = [] {
            std::vector<midi::Note> kick;
            for (int beat = 0; beat < 32; ++beat)
                kick.push_back (note (beat * 960, 120, 10, 36, 127));
            return build (smf ({ track ("Kick", kick), track ("Chords", { note (0, 15360, 4, 57) }), track ("FX", { note (11520, 3840, 7, 48) }) },
                              30720, kSynBpm, { { 0, "Intro" }, { 15360, "Drop" } }),
                synManifest(), synMix(), 0);
        }();
        return t;
    }

    struct Run
    {
        std::vector<Event> events;
        std::vector<SyncEvent> syncs;
    };

    // Renders `samples` from fromPpq at bpm with the given block pattern.
    Run play (const Timeline* timeline, double rate, double bpm, double fromPpq, std::int64_t samples, std::vector<int> blocks = { 512 },
        int piece = 32)
    {
        const int maxBlock = *std::max_element (blocks.begin(), blocks.end());
        FakeHost host (rate, maxBlock);
        SchedulerHarness h;
        h.timeline = timeline;
        h.piece = piece;
        host.prepare (h);
        host.setTempo (bpm);
        host.play (fromPpq);
        FakeHost::Output out;
        host.render (h, Controls {}, samples, out, blocks);
        return { h.sink.events, h.syncs };
    }

    std::int64_t nearest (double h, double mps) { return static_cast<std::int64_t> (std::floor (h / mps + 0.5)); }

    double mpsOf (double bpm, double rate) { return bpm * 960000.0 / (60.0 * rate); }

    // Every note-off answers an earlier note-on's tag, once.
    void checkPairs (const std::vector<Event>& events)
    {
        std::set<int> open;
        for (const auto& e : events)
        {
            if (e.kind == Event::Kind::on && e.tag >= 0)
                CHECK (open.insert (e.tag).second);
            if (e.kind == Event::Kind::off)
                CHECK (open.erase (e.tag) == 1u);
            if (e.kind == Event::Kind::stop)
                open.clear();
        }
    }
}

TEST_CASE ("an event lands on the sample nearest its position, counted from the anchor", "[sched]")
{
    for (const auto& [rate, bpm] : { std::pair { 48000.0, 120.0 }, std::pair { 44100.0, 134.0 }, std::pair { 96000.0, 128.0 } })
    {
        CAPTURE (rate, bpm);
        const auto run = play (&synLoop(), rate, bpm, 0.0, static_cast<std::int64_t> (rate * 8.0));
        const double mps = mpsOf (bpm, rate);
        const double cycle = 16.0 * 960000.0 / mps; // samples
        int checked = 0;
        for (const auto& e : run.events)
        {
            if (e.kind != Event::Kind::on || e.chase != 0)
                continue;
            // In the first cycle the timeline position is the host position; in the second, one loop on.
            const double h = static_cast<double> (e.pos) + (static_cast<double> (e.sample) >= cycle - 1.0 ? 16.0 * 960000.0 : 0.0);
            CHECK (e.sample == nearest (h, mps));
            ++checked;
        }
        CHECK (checked > 50);
    }

    // At 120 BPM and 48 kHz everything is whole samples: a kick on each beat, 24000 apart; the swung bass
    // 16th at 28.8 ticks past the 16th: 18000 + 720.
    const auto run = play (&synLoop(), 48000.0, 120.0, 0.0, 96000);
    std::vector<std::int64_t> kicks;
    for (const auto& e : run.events)
        if (e.kind == Event::Kind::on && e.channel == 9 && e.key == 36)
            kicks.push_back (e.sample);
    CHECK (kicks == std::vector<std::int64_t> { 0, 24000, 48000, 72000 });
    bool swung = false;
    for (const auto& e : run.events)
        if (e.kind == Event::Kind::on && e.channel == 0 && e.sample == 18720)
            swung = true;
    CHECK (swung);
}

TEST_CASE ("swing snap: at the kit's tempo and 48 kHz every snapped onset lands within 1 sample of phrase.Grid.at", "[sched][swing]")
{
    struct KitGrid
    {
        int bpm;
        int swingPm;
        int written;
        std::array<std::int64_t, 4> python; // phrase.Grid (kit, 64).at (1), at (3), at (62), at (63), from tools/sound
    };
    const KitGrid kits[] {
        { 134, 590, 43, { 6340, 17087, 333135, 339475 } },
        { 130, 520, 10, { 5760, 16837, 343385, 349145 } },
        { 128, 560, 29, { 6300, 17550, 348750, 355050 } },
        { 136, 540, 19, { 5718, 16306, 328236, 333953 } },
    };
    for (const auto& k : kits)
    {
        CAPTURE (k.bpm);
        // phrase.Grid.at, as tools/sound/phrase.py computes it (Python's round: half to even).
        const double step = 60.0 / k.bpm / 4.0;
        const double length = std::nearbyint (64 * step * 48000);
        const double s16 = length / 64.0;
        const double swing = k.swingPm / 1000.0;
        auto gridAt = [&] (int i) {
            const double s = i + (i % 2 != 0 ? 2.0 * swing - 1.0 : 0.0);
            return static_cast<std::int64_t> (std::fmod (std::floor (s * s16 + 0.5), length));
        };
        CHECK (gridAt (1) == k.python[0]);
        CHECK (gridAt (3) == k.python[1]);
        CHECK (gridAt (62) == k.python[2]);
        CHECK (gridAt (63) == k.python[3]);

        // A note on every 16th of a 4-bar loop, the odd ones written late by the file's whole ticks.
        std::vector<midi::Note> line;
        for (int i = 0; i < 64; ++i)
            line.push_back (note (i * 240 + (i % 2 != 0 ? k.written : 0), 100, 1, 36));
        const auto t = build (smf ({ track ("Bass", line) }, 15360, k.bpm), synManifest (k.bpm, k.swingPm), synMix(), 15360);
        REQUIRE (t.notes.size() == 64u);
        const auto run = play (&t, 48000.0, k.bpm, 0.0, static_cast<std::int64_t> (length));
        std::vector<std::int64_t> onsets;
        for (const auto& e : run.events)
            if (e.kind == Event::Kind::on)
                onsets.push_back (e.sample);
        REQUIRE (onsets.size() == 64u);
        int writtenOff = 0;
        const double mps = mpsOf (k.bpm, 48000.0);
        for (int i = 0; i < 64; ++i)
        {
            CAPTURE (i);
            CHECK (std::abs (onsets[static_cast<std::size_t> (i)] - gridAt (i)) <= 1);
            // As written (whole ticks), the odd 16ths would land up to 9 samples off.
            const auto written = nearest ((i * 240.0 + (i % 2 != 0 ? k.written : 0)) * 1000.0, mps);
            writtenOff = std::max (writtenOff, static_cast<int> (std::abs (written - gridAt (i))));
        }
        CHECK (writtenOff >= 2);
    }
}

TEST_CASE ("the loop wraps at 15360 from ppq 0, its note-offs first", "[sched][loop]")
{
    const auto run = play (&synLoop(), 48000.0, 120.0, 0.0, 3 * 384000 + 1000);
    // The third cycle repeats the second, 384000 samples on (the first has no key from the cycle before).
    std::vector<Event> first, second;
    for (const auto& e : run.events)
    {
        if (e.kind != Event::Kind::on && e.kind != Event::Kind::key && e.kind != Event::Kind::cc)
            continue;
        if (e.sample >= 384000 && e.sample < 2 * 384000)
            first.push_back (e);
        else if (e.sample >= 2 * 384000 && e.sample < 3 * 384000)
            second.push_back (e);
    }
    REQUIRE (first.size() == second.size());
    for (std::size_t i = 0; i < first.size(); ++i)
    {
        auto shifted = first[i];
        shifted.sample += 384000;
        if (shifted.kind == Event::Kind::on)
            shifted.tag = second[i].tag;
        CHECK (shifted == second[i]);
    }

    // On the wrap's sample: the pad's and the riser's note-offs come before the new cycle's note-ons.
    std::vector<Event> atWrap;
    for (const auto& e : run.events)
        if (e.sample == 384000 && (e.kind == Event::Kind::on || e.kind == Event::Kind::off))
            atWrap.push_back (e);
    REQUIRE (atWrap.size() >= 4u);
    const auto firstOn = std::find_if (atWrap.begin(), atWrap.end(), [] (const Event& e) { return e.kind == Event::Kind::on; });
    const auto offs = std::count_if (atWrap.begin(), firstOn, [] (const Event& e) { return e.kind == Event::Kind::off; });
    CHECK (offs >= 2); // the pad and the riser end on 15360
    CHECK (std::none_of (firstOn, atWrap.end(), [] (const Event& e) { return e.kind == Event::Kind::off; }));
    checkPairs (run.events);
}

TEST_CASE ("a song is silent before 0 and past its end; Off plays nothing", "[sched][song]")
{
    // Two bars of count-in (ppq -8 at 120 BPM: 192000 samples), then the 8-bar song, then 2 bars more.
    const auto run = play (&synSong(), 48000.0, 120.0, -8.0, 192000 + 768000 + 192000);
    for (const auto& e : run.events)
        CHECK (e.sample >= 192000);
    const auto ons = [&] {
        std::vector<Event> out;
        for (const auto& e : run.events)
            if (e.kind == Event::Kind::on)
                out.push_back (e);
        return out;
    }();
    REQUIRE_FALSE (ons.empty());
    CHECK (ons.front().sample == 192000);
    CHECK (ons.front().chase == 0);
    CHECK (ons.size() == synSong().notes.size());
    for (const auto& e : run.events)
        if (e.kind == Event::Kind::on)
            CHECK (e.sample < 192000 + 768000);
    // The last note-offs come on the song's end; nothing after.
    CHECK (run.events.back().sample <= 192000 + 768000);
    checkPairs (run.events);

    const auto off = play (nullptr, 48000.0, 120.0, 0.0, 96000);
    CHECK (off.events.empty());
}

TEST_CASE ("a jump fades sequenced voices at the jump and chases pads, risers and phrase notes part-way in", "[sched][jump]")
{
    // The loop with its first bass note of bar 3 made a phrase note (as annotatePhrases would): it joins
    // the chase list although it is short.
    Timeline t = synLoop();
    std::size_t phraseNote = t.notes.size();
    for (std::size_t i = 0; i < t.notes.size(); ++i)
        if (t.notes[i].channel == 0 && t.notes[i].pos == 7680000)
            phraseNote = i;
    REQUIRE (phraseNote < t.notes.size());
    t.notes[phraseNote].phrase = 0;
    t.notes[phraseNote].slice = 0;
    t.notes[phraseNote].flags |= NoteFlag::chase;
    t.chase.push_back (static_cast<std::uint32_t> (phraseNote));
    std::sort (t.chase.begin(), t.chase.end());

    for (const int which : { 0, 1 })
    {
        const double target = which == 0 ? 13.0 : 8.1;
        CAPTURE (target);
        FakeHost host (48000.0, 512);
        SchedulerHarness h;
        h.timeline = &t;
        host.prepare (h);
        host.setTempo (120.0);
        host.play (0.0);
        FakeHost::Output out;
        host.render (h, Controls {}, 48128, out); // 94 blocks of 512
        const auto before = h.sink.events.size();
        CHECK (h.scheduler.sounding() > 0);
        host.seek (target);
        host.render (h, Controls {}, 512, out);
        CHECK (h.syncs.back() == SyncEvent::jumped);

        const std::vector<Event> after (h.sink.events.begin() + static_cast<std::ptrdiff_t> (before), h.sink.events.end());
        REQUIRE_FALSE (after.empty());
        CHECK (after.front().kind == Event::Kind::stop);
        CHECK (after.front().sample == 48128);

        std::map<std::int64_t, Event> chased; // by timeline position
        for (const auto& e : after)
            if (e.kind == Event::Kind::on && e.chase > 0)
                chased[e.pos] = e;
        if (which == 0)
        {
            // The pad (0-16 quarters) 13 quarters in; the riser (12-16) 1 quarter in, 3 to go.
            REQUIRE (chased.count (0) == 1u);
            CHECK (chased[0].chase == 13 * 24000);
            CHECK (chased[0].length == 3 * 24000);
            CHECK (chased[0].sample == 48128);
            REQUIRE (chased.count (11520000) == 1u);
            CHECK (chased[11520000].chase == 24000);
            CHECK (chased[11520000].length == 3 * 24000);
            CHECK (chased.size() == 2u);
        }
        else
        {
            // 8.1 quarters: the pad, and the phrase note at 8 quarters, 0.1 quarter (2400 samples) in.
            REQUIRE (chased.count (7680000) == 1u);
            CHECK (chased[7680000].chase == 2400);
            CHECK (chased[7680000].length == 200 * 25 - 2400);
            CHECK (chased.count (0) == 1u);
            // The vox (bar 3, pre-rolled) starts just before 8 quarters and is still sounding: chased too.
            CHECK (chased.size() == 3u);
        }
        checkPairs (h.sink.events);
    }
}

TEST_CASE ("a host loop region wraps inside a block, with no stuck note", "[sched][loop]")
{
    for (const int block : { 512, 1000, 37 })
    {
        CAPTURE (block);
        FakeHost host (48000.0, block);
        SchedulerHarness h;
        h.timeline = &synLoop();
        host.prepare (h);
        host.setTempo (120.0);
        host.setLoop (2.0, 6.0); // quarters: samples 48000-144000 at 120 BPM
        host.play (0.0);
        FakeHost::Output out;
        host.render (h, Controls {}, 480000, out);

        // Never a jump: the scheduler wraps where the host does.
        for (const auto s : h.syncs)
            CHECK ((s == SyncEvent::none || s == SyncEvent::started || s == SyncEvent::wrapped));

        // At each wrap (144000, 240000, 336000, 432000) every sounding note ends, and the notes from 2
        // quarters on play again: the kick on beat 2 first.
        for (const std::int64_t wrap : { 144000, 240000, 336000, 432000 })
        {
            CAPTURE (wrap);
            int ons = 0;
            bool kick = false;
            for (const auto& e : h.sink.events)
            {
                if (e.sample != wrap)
                    continue;
                if (e.kind == Event::Kind::on)
                {
                    ++ons;
                    kick = kick || (e.channel == 9 && e.key == 36 && e.pos == 1920000);
                }
            }
            CHECK (kick);
            CHECK (ons > 0);
        }
        // Between wraps, only notes from 2 to 6 quarters start (or chased ones that sound across 2).
        for (const auto& e : h.sink.events)
            if (e.kind == Event::Kind::on && e.sample >= 144000 && e.chase == 0)
            {
                CHECK (e.pos >= 1920000 - 96000); // the vox's pre-roll may start it a little before
                CHECK (e.pos < 5760000);
            }
        checkPairs (h.sink.events);

        // Stopping leaves nothing sounding.
        host.stop();
        host.render (h, Controls {}, block, out);
        CHECK (h.scheduler.sounding() == 0);
        CHECK (h.sink.events.back().kind == Event::Kind::stop);
    }
}

TEST_CASE ("a host loop region ending on a block's edge wraps there", "[sched][loop]")
{
    // Blocks of 1000 samples: the region's end (4 quarters, 96000 samples) is a block edge.
    FakeHost host (48000.0, 1000);
    SchedulerHarness h;
    h.timeline = &synLoop();
    host.prepare (h);
    host.setTempo (120.0);
    host.setLoop (0.0, 4.0);
    host.play (0.0);
    FakeHost::Output out;
    host.render (h, Controls {}, 4 * 96000, out);
    for (const auto s : h.syncs)
        CHECK (s != SyncEvent::jumped);
    for (const std::int64_t wrap : { 96000, 192000, 288000 })
    {
        bool kick = false;
        for (const auto& e : h.sink.events)
            kick = kick || (e.kind == Event::Kind::on && e.sample == wrap && e.channel == 9 && e.key == 36 && e.pos == 0);
        CHECK (kick);
    }
    checkPairs (h.sink.events);
}

TEST_CASE ("a tempo change re-anchors where the host changed it", "[sched][tempo]")
{
    FakeHost host (48000.0, 500);
    SchedulerHarness h;
    h.timeline = &synLoop();
    host.prepare (h);
    host.setTempo (120.0);
    host.play (0.0);
    FakeHost::Output out;
    host.render (h, Controls {}, 48000, out); // 2 quarters
    host.setTempo (150.0);
    host.render (h, Controls {}, 96000, out);
    CHECK (std::count (h.syncs.begin(), h.syncs.end(), SyncEvent::retimed) == 1);
    CHECK (std::count (h.syncs.begin(), h.syncs.end(), SyncEvent::jumped) == 0);
    // At 150 BPM a quarter is 19200 samples: beat 3 lands 19200 after the change, beat 4 38400.
    std::vector<std::int64_t> kicks;
    for (const auto& e : h.sink.events)
        if (e.kind == Event::Kind::on && e.channel == 9 && e.key == 36)
            kicks.push_back (e.sample);
    REQUIRE (kicks.size() >= 5u);
    CHECK (kicks[0] == 0);
    CHECK (kicks[1] == 24000);
    CHECK (kicks[2] == 48000);
    CHECK (kicks[3] == 48000 + 19200);
    CHECK (kicks[4] == 48000 + 38400);
    checkPairs (h.sink.events);
}

TEST_CASE ("40 and 480 BPM, and hosts in 3/4, 7/8 and 4/3, break neither the loop nor the song", "[sched][tempo]")
{
    for (const double bpm : { 40.0, 480.0 })
        for (const bool song : { false, true })
        {
            CAPTURE (bpm, song);
            const Timeline* t = song ? &synSong() : &synLoop();
            const double rate = 44100.0;
            const double quarter = 60.0 / bpm * rate;
            const auto cycle = static_cast<std::int64_t> (std::ceil (static_cast<double> (t->length) / 960000.0 * quarter));
            FakeHost host (rate, 512);
            SchedulerHarness h;
            h.timeline = t;
            host.prepare (h);
            host.setTempo (bpm);
            host.play (0.0);
            FakeHost::Output out;
            host.render (h, Controls {}, 2 * cycle, out);
            std::size_t ons = 0;
            for (const auto& e : h.sink.events)
                ons += e.kind == Event::Kind::on ? 1 : 0;
            // A loop plays twice (its pre-rolled vox wraps in, once per cycle); a song once.
            CHECK (ons == (song ? t->notes.size() : 2 * t->notes.size()));
            checkPairs (h.sink.events);
            CHECK (std::is_sorted (h.sink.events.begin(), h.sink.events.end(), [] (const Event& a, const Event& b) { return a.sample < b.sample; }));
            host.stop();
            host.render (h, Controls {}, 64, out);
            CHECK (h.scheduler.sounding() == 0);
        }

    // A host in another metre still counts quarters: its bar lines are just other ppq values. The loop
    // plays ppq mod 16 from ppq 0 whatever they are: a start on 7/8's bar 2 (3.5 quarters), 3/4's bar 3
    // (6 quarters) or 4/3's bar 2 (16/3 quarters) starts that far into the loop.
    for (const double start : { 3.5, 6.0, 16.0 / 3.0, 3.5 + 16.0 })
    {
        CAPTURE (start);
        const auto run = play (&synLoop(), 48000.0, 120.0, start, 48000);
        const double startH = std::fmod (start, 16.0) * 960000.0;
        bool any = false;
        for (const auto& e : run.events)
            if (e.kind == Event::Kind::on && e.chase == 0)
            {
                any = true;
                CHECK (static_cast<double> (e.pos) >= startH - 20.0);
                CHECK (e.sample == nearest ((static_cast<double> (e.pos) - startH), 40.0));
            }
        CHECK (any);
    }
}

TEST_CASE ("starting mid-bar, stopping and starting, and toggling play every block leave nothing sounding after each stop", "[sched][transport]")
{
    FakeHost host (48000.0, 256);
    SchedulerHarness h;
    h.timeline = &synLoop();
    host.prepare (h);
    host.setTempo (120.0);
    FakeHost::Output out;

    host.play (1.37);
    host.render (h, Controls {}, 48000, out);
    CHECK (h.syncs.front() == SyncEvent::started);
    CHECK (h.scheduler.sounding() > 0); // the pad, chased
    host.stop();
    host.render (h, Controls {}, 256, out);
    CHECK (h.syncs.back() == SyncEvent::stopped);
    CHECK (h.scheduler.sounding() == 0);
    CHECK (h.sink.events.back().kind == Event::Kind::stop);

    for (int block = 0; block < 400; ++block)
    {
        if (block % 2 == 0)
            host.play (host.ppq());
        else
            host.stop();
        host.render (h, Controls {}, 256, out);
        if (block % 2 == 1)
            CHECK (h.scheduler.sounding() == 0);
    }
    checkPairs (h.sink.events);
}

TEST_CASE ("a timeline switch mid-note ends the old notes at their own note-offs; a kit restart forgets them", "[sched][switch]")
{
    // Loop -> song mid-pad: the loop's pad still ends on its own 4-bar note-off, after the switch.
    FakeHost host (48000.0, 512);
    SchedulerHarness h;
    h.timeline = &synLoop();
    host.prepare (h);
    host.setTempo (120.0);
    host.play (0.0);
    FakeHost::Output out;
    host.render (h, Controls {}, 52736, out);
    h.timeline = &synSong();
    host.render (h, Controls {}, 384000, out);
    int padTag = -1;
    for (const auto& e : h.sink.events)
        if (e.kind == Event::Kind::on && e.channel == 3 && e.sample == 0)
            padTag = e.tag;
    REQUIRE (padTag >= 0);
    bool ended = false;
    for (const auto& e : h.sink.events)
        if (e.kind == Event::Kind::off && e.tag == padTag)
        {
            ended = true;
            CHECK (e.sample == 384000);
        }
    CHECK (ended);
    // After the switch the song's notes play from where the song is (no chase: its pad doesn't restart).
    for (const auto& e : h.sink.events)
        if (e.kind == Event::Kind::on && e.sample > 52736)
            CHECK (e.chase == 0);
    checkPairs (h.sink.events);

    // A kit change: restart () forgets what sounds (their voices fade with their kit) and chases the new
    // timeline at once.
    FakeHost host2 (48000.0, 512);
    SchedulerHarness k;
    k.timeline = &synLoop();
    host2.prepare (k);
    host2.setTempo (120.0);
    host2.play (0.0);
    host2.render (k, Controls {}, 52736, out);
    REQUIRE (k.scheduler.sounding() > 0);
    const auto mark = k.sink.events.size();
    k.sink.base = k.scheduler.sampleIndex();
    k.scheduler.restart (&synSong(), k.sink);
    int chased = 0;
    for (auto i = mark; i < k.sink.events.size(); ++i)
    {
        const auto& e = k.sink.events[i];
        CHECK (e.kind != Event::Kind::off); // nothing ends: the old kit's voices fade
        chased += (e.kind == Event::Kind::on && e.chase > 0 && e.sample == 52736) ? 1 : 0;
    }
    CHECK (chased == 1); // the song's pad
    CHECK (k.scheduler.sounding() == 1);
}

TEST_CASE ("with no playhead, or a Standalone one (no ppq, no tempo), the scheduler runs its own clock at the kit's BPM", "[sched][transport]")
{
    for (const double hostBpm : { 0.0, 140.0 })
    {
        CAPTURE (hostBpm);
        FakeHost host (48000.0, 333);
        SchedulerHarness h;
        h.timeline = &synLoop();
        h.defaultBpm = 128.0; // the kit's
        host.prepare (h);
        host.setHasPosition (false);
        host.setTempo (hostBpm);
        FakeHost::Output out;
        host.render (h, Controls {}, 4800, out); // not playing: silent
        CHECK (h.sink.events.empty());
        host.play();
        host.render (h, Controls {}, 96000, out);
        const double bpm = hostBpm > 0.0 ? hostBpm : 128.0;
        const auto quarter = static_cast<std::int64_t> (std::floor (60.0 / bpm * 48000.0 + 0.5));
        std::vector<std::int64_t> kicks;
        for (const auto& e : h.sink.events)
            if (e.kind == Event::Kind::on && e.channel == 9 && e.key == 36)
                kicks.push_back (e.sample - 4800);
        REQUIRE (kicks.size() >= 3u);
        CHECK (kicks[0] == 0);
        CHECK (kicks[1] == quarter);
        CHECK (std::abs (kicks[2] - 2 * quarter) <= 1);
        CHECK (h.scheduler.position().ppq > 0.0);

        // Stop and start again: the clock starts from 0 again.
        host.stop();
        host.render (h, Controls {}, 333, out);
        const auto mark = h.sink.events.size();
        host.play();
        host.render (h, Controls {}, 333, out);
        bool restarted = false;
        for (auto i = mark; i < h.sink.events.size(); ++i)
            restarted = restarted || (h.sink.events[i].kind == Event::Kind::on && h.sink.events[i].pos == 0);
        CHECK (restarted);
    }
}

TEST_CASE ("duck keys arrive pre_us early, and sequenced CCs ramp linearly to their next point", "[sched][duck][cc]")
{
    const auto run = play (&synLoop(), 48000.0, 120.0, 0.0, 384000);
    std::vector<std::int64_t> kickKeys;
    for (const auto& e : run.events)
        if (e.kind == Event::Kind::key && e.key == static_cast<std::uint8_t> (Lane::kick))
            kickKeys.push_back (e.sample);
    // 5 ms at 48 kHz: 240 samples before each beat; beat 0's key belongs to the cycle before (silent).
    REQUIRE (kickKeys.size() == 16u);
    for (std::size_t i = 0; i + 1 < kickKeys.size(); ++i)
        CHECK (kickKeys[i] == static_cast<std::int64_t> (i + 1) * 24000 - 240);
    CHECK (kickKeys.back() == 384000 - 240);

    // The ride: every 60 ticks (1500 samples) a point, then a ramp to the next over 1500 samples. The
    // start's chase comes first: the loop's last point (127 at 15300) ramps into its first (0 at 0).
    std::vector<Event> rides;
    for (const auto& e : run.events)
        if (e.kind == Event::Kind::cc)
            rides.push_back (e);
    REQUIRE (rides.size() == 1 + 2 * 256u);
    CHECK (rides[0].sample == 0);
    CHECK (rides[0].key == 74);
    CHECK (rides[0].value == 0);
    CHECK (rides[0].length == 0);
    for (std::size_t point = 0; point < 256; ++point)
    {
        CAPTURE (point);
        const auto& at = rides[1 + 2 * point];
        const auto& ramp = rides[2 + 2 * point];
        CHECK (at.sample == static_cast<std::int64_t> (point) * 1500);
        CHECK (at.value == point % 128);
        CHECK (at.length == 0);
        CHECK (ramp.sample == at.sample);
        CHECK (ramp.value == (point + 1) % 128);
        CHECK (ramp.length == 1500);
    }

    // A start in the middle of a ramp: at 510 ticks, halfway from 8 (480 ticks) to 9 (540), the chase
    // sets 8.5 rounded half up, 9, then ramps on to 9 over the 30 ticks (750 samples) left.
    const auto mid = play (&synLoop(), 48000.0, 120.0, 0.5 + 30.0 / 960.0, 4800);
    std::vector<Event> chased;
    for (const auto& e : mid.events)
        if (e.kind == Event::Kind::cc && e.sample == 0)
            chased.push_back (e);
    REQUIRE (chased.size() == 2u);
    CHECK (chased[0].value == 9);
    CHECK (chased[0].length == 0);
    CHECK (chased[1].value == 9);
    CHECK (chased[1].length == 750);
}

TEST_CASE ("the sounding table never strands a note: when it is full, the note due to end first ends", "[sched]")
{
    // 300 long notes on distinct channels and keys, one tick apart: more than the 256 slots.
    KitManifest m = synManifest();
    m.keymap.clear();
    for (int ch = 1; ch <= 16; ++ch)
        if (ch != 3 && ch != 9)
            m.keymap.push_back (keys (ch, 0, 127, inst::pad));
    std::vector<midi::TrackSpec> tracks;
    std::vector<midi::Note> notes;
    for (int i = 0; i < 300; ++i)
    {
        const int ch = 1 + (i / 100) * 3 + (i / 100 == 0 ? 0 : 1); // 1, 5, 8
        notes.push_back (note (i, 15000 - i, ch, i % 100 + 10));
    }
    const auto t = build (smf ({ track ("Chords", notes) }, 15360), m, synMix(), 0);
    REQUIRE (t.notes.size() == 300u);
    const auto run = play (&t, 48000.0, 120.0, 0.0, 384000 + 512);
    std::size_t ons = 0, offs = 0;
    for (const auto& e : run.events)
    {
        ons += e.kind == Event::Kind::on ? 1 : 0;
        offs += e.kind == Event::Kind::off ? 1 : 0;
    }
    CHECK (ons == 300u);
    CHECK (offs == 300u);
    // The 257th note-on (tick 256) first ends the earliest-ending note, at its own sample.
    const auto on257 = std::find_if (run.events.begin(), run.events.end(), [] (const Event& e) { return e.kind == Event::Kind::on && e.pos == 256000; });
    REQUIRE (on257 != run.events.end());
    const auto early = std::find_if (run.events.begin(), run.events.end(), [] (const Event& e) { return e.kind == Event::Kind::off; });
    REQUIRE (early != run.events.end());
    CHECK (early->sample == on257->sample);
    checkPairs (run.events);
}
