// buildTimeline (sched/timeline_builder.h): an SMF and the kit's manifests in, a Timeline out. Swing
// snap, pre-roll, the loop wrap, round robins and pans by position, CC ramps, throw latches, duck keys
// and the chase list, on P7's fixtures and on every embedded MIDI file.

#include "sched_fixtures.h"

#include "midi/reader.h"
#include "midi/writer.h"
#include "sched/timeline_builder.h"
#include "testing/synthetic_kit.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <tuple>
#include <vector>

using namespace p7;

namespace
{
    bool sortedByPosition (const Timeline& t)
    {
        return std::is_sorted (t.notes.begin(), t.notes.end(), [] (const Note& a, const Note& b) {
            return std::tie (a.pos, a.channel, a.key) < std::tie (b.pos, b.channel, b.key);
        });
    }

    const Note* findNote (const Timeline& t, int channel, int key, std::int64_t pos)
    {
        for (const auto& n : t.notes)
            if (n.channel == channel - 1 && n.key == key && n.pos == pos)
                return &n;
        return nullptr;
    }

    // The one note on that channel and key.
    const Note* findKey (const Timeline& t, int channel, int key)
    {
        const Note* found = nullptr;
        for (const auto& n : t.notes)
            if (n.channel == channel - 1 && n.key == key)
            {
                if (found != nullptr)
                    return nullptr;
                found = &n;
            }
        return found;
    }

    std::vector<const Note*> notesOn (const Timeline& t, int channel, int key)
    {
        std::vector<const Note*> out;
        for (const auto& n : t.notes)
            if (n.channel == channel - 1 && n.key == key)
                out.push_back (&n);
        return out;
    }

    // A file from raw events, for what core's writer refuses on purpose (a note-on at a loop's end).
    std::vector<std::uint8_t> rawFile (std::vector<midi::Event> events, std::int64_t endTick)
    {
        midi::File f;
        f.format = 1;
        f.ppq = 960;
        midi::Track conductor;
        conductor.endTick = endTick;
        midi::Track track;
        track.events = std::move (events);
        track.endTick = endTick;
        f.tracks = { conductor, track };
        auto bytes = midi::encode (f);
        return bytes.ok() ? bytes.value : std::vector<std::uint8_t> {};
    }

    midi::Event channelEvent (std::int64_t tick, std::uint8_t status, int data1, int data2)
    {
        midi::Event e;
        e.tick = tick;
        e.status = status;
        e.data1 = static_cast<std::uint8_t> (data1);
        e.data2 = static_cast<std::uint8_t> (data2);
        return e;
    }

    // The four kits' tempo and swing (tools/sound/kits/fut0N.py), with the swing the files write in
    // whole ticks and the exact one in milliticks.
    struct KitGrid
    {
        const char* id;
        int bpm;
        int swingPm;
        int written;
        std::int64_t exact;
    };
    constexpr KitGrid kKits[] {
        { "FUT01", 134, 590, 43, 43200 },
        { "FUT02", 130, 520, 10, 9600 },
        { "FUT03", 128, 560, 29, 28800 },
        { "FUT04", 136, 540, 19, 19200 },
    };

    // A manifest that plays every key on every channel but 3 and 9, one instrument per channel, at a
    // kit's tempo and swing: for the real files, before P2's kit manifests land.
    KitManifest catchAll (int bpm, int swingPm)
    {
        KitManifest m;
        m.kit.id = "ALL01";
        m.kit.bpm = bpm;
        m.kit.swingPm = swingPm;
        m.files = { fileEntry ("all.wav", 4800) };
        for (int ch = 1; ch <= 16; ++ch)
        {
            if (ch == 3 || ch == 9)
                continue;
            m.instruments.push_back (instrument ("ch" + std::to_string (ch), Lane::bass, "all", SampleMode::gated,
                { zone (60, 0, 127, { layer (127, { 0 }) }) }));
            m.keymap.push_back (keys (ch, 0, 127, static_cast<int> (m.instruments.size()) - 1));
        }
        return m;
    }

    std::vector<std::uint8_t> readFile (const std::filesystem::path& path)
    {
        std::ifstream in (path, std::ios::binary);
        return { std::istreambuf_iterator<char> (in), std::istreambuf_iterator<char>() };
    }
}

TEST_CASE ("the timeline builder keeps every mapped note, sorted by position, with its length", "[timeline]")
{
    const auto t = build (synLoopSmf(), synManifest(), synMix(), 15360);
    CHECK (t.loops);
    CHECK (t.length == kLoopLength);
    REQUIRE (t.notes.size() == 16u + 8u + 32u + 32u + 1u + 1u + 1u);
    CHECK (sortedByPosition (t));
    for (const auto& n : t.notes)
    {
        CHECK (n.len >= kMilliticksPerTick);
        CHECK (n.pos >= 0);
        CHECK (n.pos < t.length);
        CHECK (n.velocity > 0);
    }

    const Note* kick = findNote (t, 10, 36, 1920000);
    REQUIRE (kick != nullptr);
    CHECK (kick->len == 120000);
    CHECK (kick->velocity == 127);
    CHECK (kick->phrase == -1);
    CHECK (kick->slice == -1);
    CHECK (kick->throwSpace == 0);
    CHECK (kick->throwEcho == 0);

    // A second build gives the same timeline.
    CHECK (build (synLoopSmf(), synManifest(), synMix(), 15360) == t);
}

TEST_CASE ("notes nothing plays are left out: channel 3, channel 9 and unmapped keys", "[timeline]")
{
    const auto bytes = smf ({ track ("Chords", { note (0, 480, 4, 60) }), track ("Chord voicing", { note (0, 480, 3, 60), note (0, 480, 3, 64) }),
                                track ("Reserved", { note (0, 480, 9, 60) }), track ("Perc", { note (0, 120, 10, 37), note (240, 120, 10, 60) }) },
        3840);
    const auto t = build (bytes, synManifest(), synMix(), 0);
    REQUIRE (t.notes.size() == 1u);
    CHECK (t.notes[0].channel == 3);
    CHECK (t.notes[0].key == 60);
}

TEST_CASE ("a song keeps its length, its sections from the markers and its cue points", "[timeline]")
{
    midi::TrackSpec vox = track ("Vox", { note (1920, 480, 5, 64) });
    vox.texts = { { 1920, midi::meta::cuePoint, "Vox" } };
    midi::TrackSpec fx = track ("FX", { note (2880, 960, 7, 48) });
    fx.texts = { { 2880, midi::meta::cuePoint, "Riser" } };
    const auto bytes = smf ({ vox, fx }, 7680, kSynBpm, { { 0, "Intro" }, { 3840, "Drop" } });
    const auto t = build (bytes, synManifest(), synMix(), 0);
    CHECK_FALSE (t.loops);
    CHECK (t.length == 7680 * kMilliticksPerTick);
    REQUIRE (t.sections.size() == 2u);
    CHECK (t.sections[0] == Section { 0, "Intro" });
    CHECK (t.sections[1] == Section { 3840000, "Drop" });
    REQUIRE (t.cues.size() == 2u);
    CHECK (t.cues[0] == Section { 1920000, "Vox" });
    CHECK (t.cues[1] == Section { 2880000, "Riser" });
}

TEST_CASE ("swing snap moves the written odd-16th offsets to the exact swing and leaves the rest", "[timeline][swing]")
{
    for (const auto& k : kKits)
    {
        CAPTURE (k.id);
        CHECK (swingSnap (240 + k.written, k.swingPm) == 240000 + k.exact);
        CHECK (swingSnap (3 * 240 + k.written, k.swingPm) == 3 * 240000 + k.exact);
        CHECK (swingSnap (15360 - 240 + k.written, k.swingPm) == (15360 - 240) * 1000 + k.exact);
        CHECK (swingSnap (240, k.swingPm) == 240000); // an odd 16th written straight stays
        CHECK (swingSnap (240 + k.written + 1, k.swingPm) == (240 + k.written + 1) * 1000);
        CHECK (swingSnap (240 + k.written - 1, k.swingPm) == (240 + k.written - 1) * 1000);
        CHECK (swingSnap (480 + k.written, k.swingPm) == (480 + k.written) * 1000); // an even 16th plus the offset stays
        CHECK (swingSnap (0, k.swingPm) == 0);
        CHECK (swingSnap (-240 + k.written, k.swingPm) == -240000 + k.exact); // a pre-roll's negative tick follows the same rule

        // Through the builder: starts and ends both snap; a note's length follows.
        auto m = synManifest (k.bpm, k.swingPm);
        const auto bytes = smf ({ track ("Bass", { note (0, 240 + k.written, 1, 36), note (240 + k.written, 240 - k.written, 1, 38),
                                                     note (480, 240, 1, 40), note (720 + k.written + 2, 100, 1, 41) }) },
            3840, k.bpm);
        const auto t = build (bytes, m, synMix(), 0);
        REQUIRE (t.notes.size() == 4u);
        CHECK (t.notes[0].pos == 0);
        CHECK (t.notes[0].len == 240000 + k.exact); // its end, on the swung 16th, snaps
        CHECK (t.notes[1].pos == 240000 + k.exact);
        CHECK (t.notes[1].len == 480000 - (240000 + k.exact)); // it ends on the even 16th
        CHECK (t.notes[2].pos == 480000);
        CHECK (t.notes[3].pos == (720 + k.written + 2) * 1000); // off the written offset: as written
    }
}

TEST_CASE ("pre-roll starts a note its sample's pre-roll early at the kit's BPM, and wraps in loops", "[timeline][preroll]")
{
    // 2400 frames (50 ms) at 120 BPM: 2400 x 120 / 3 = 96000 milliticks (96 ticks).
    CHECK (framesToMilliticks (2400, 120) == 96000);
    CHECK (framesToMilliticks (2400, 134) == 107200);
    CHECK (framesToMilliticks (1000, 130) == 43333); // 43333.3
    CHECK (framesToMilliticks (1001, 130) == 43377); // 43376.7
    // The design's formula, round (ms x bpm x 960 / 60000) ticks, here in milliticks.
    for (const int bpm : { 40, 120, 128, 134, 480 })
        for (const int frames : { 1, 480, 2400, 48000 })
        {
            const double ms = frames / 48.0;
            CHECK (framesToMilliticks (frames, bpm) == static_cast<std::int64_t> (std::floor (ms * bpm * 960000.0 / 60000.0 + 0.5)));
        }

    const auto m = synManifest();
    const std::int64_t shift = 96000;

    SECTION ("in a song")
    {
        const auto bytes = smf ({ track ("Vox", { note (0, 480, 5, 60), note (7680, 1440, 5, 60) }) }, 15360);
        const auto t = build (bytes, m, synMix(), 0);
        REQUIRE (t.notes.size() == 2u);
        CHECK (t.notes[0].pos == -shift); // before the song's start: its onset is on tick 0
        CHECK (t.notes[0].len == 480000 + shift); // the end stays
        CHECK (t.notes[1].pos == 7680000 - shift);
        CHECK (t.notes[1].len == 1440000 + shift);
    }

    SECTION ("at the note's pitch: an octave up halves it, a fifth down lengthens it")
    {
        const auto bytes = smf ({ track ("Vox", { note (3840, 480, 5, 72), note (7680, 480, 5, 53) }) }, 15360);
        const auto t = build (bytes, m, synMix(), 0);
        REQUIRE (t.notes.size() == 2u);
        CHECK (t.notes[0].pos == 3840000 - shift / 2);
        CHECK (std::bit_cast<std::uint64_t> (semitoneRatio (12)) == std::bit_cast<std::uint64_t> (2.0));
        CHECK (std::bit_cast<std::uint64_t> (semitoneRatio (-12)) == std::bit_cast<std::uint64_t> (0.5));
        CHECK (semitoneRatio (-7) == Catch::Approx (std::pow (2.0, -7.0 / 12.0)).epsilon (1e-15));
        CHECK (semitoneRatio (19) == Catch::Approx (std::pow (2.0, 19.0 / 12.0)).epsilon (1e-15));
        CHECK (t.notes[1].pos == 7680000 - static_cast<std::int64_t> (std::floor (2400.0 / semitoneRatio (-7) * 120.0 / 3.0 + 0.5)));
        CHECK (t.notes[1].pos < 7680000 - shift);
    }

    SECTION ("in a loop, a note on tick 0 wraps to the end and crosses it")
    {
        const auto bytes = smf ({ track ("Vox", { note (0, 480, 5, 60), note (7680, 1440, 5, 60) }) }, 15360);
        const auto t = build (bytes, m, synMix(), 15360);
        REQUIRE (t.notes.size() == 2u);
        CHECK (t.notes[0].pos == 7680000 - shift);
        CHECK (t.notes[1].pos == kLoopLength - shift);
        CHECK (t.notes[1].len == 480000 + shift);
        CHECK (t.notes[1].pos + t.notes[1].len > t.length);
    }

    SECTION ("an A/B pair shifts by the smaller pre-roll of its two instruments")
    {
        const auto bytes = smf ({ track ("Vox chops", { note (3840, 240, 8, 48) }) }, 15360);
        const auto t = build (bytes, m, synMix(), 0);
        REQUIRE (t.notes.size() == 1u);
        CHECK (t.notes[0].pos == 3840000 - framesToMilliticks (480, kSynBpm)); // chop_b's 480, not chop_a's 960
    }

    SECTION ("instruments without pre-roll stay on their ticks")
    {
        const auto t = build (synLoopSmf(), m, synMix(), 15360);
        CHECK (findNote (t, 10, 36, 0) != nullptr);
        CHECK (findNote (t, 7, 48, 11520000) != nullptr);
    }
}

TEST_CASE ("a loop's events at its end move to its start", "[timeline]")
{
    // A note-on and a controller on tick 15360 of a 4-bar loop: core's writer refuses such a file, so it is
    // built from raw events, as another writer might leave it.
    const auto bytes = rawFile ({ channelEvent (0, 0xB0, 74, 10), channelEvent (0, 0x90, 40, 100), channelEvent (480, 0x80, 40, 0),
                                    channelEvent (15360, 0xB0, 74, 99), channelEvent (15360, 0x90, 41, 100) },
        15360);
    REQUIRE_FALSE (bytes.empty());
    const auto t = build (bytes, synManifest(), synMix(), 15360);
    REQUIRE (t.notes.size() == 2u);
    CHECK (t.notes[0].pos == 0);
    CHECK (t.notes[1].pos == 0);
    CHECK (t.notes[1].key == 41);
    for (const auto& n : t.notes)
        CHECK (n.pos + n.len <= t.length + n.len);
    // The controller from the end comes first at tick 0, so the loop's own value at 0 wins.
    REQUIRE (t.ccs.size() == 2u);
    CHECK (t.ccs[0].pos == 0);
    CHECK (t.ccs[0].value == 99);
    CHECK (t.ccs[1].pos == 0);
    CHECK (t.ccs[1].value == 10);
}

TEST_CASE ("round robins and pan indices count each instrument's hits within its rr block", "[timeline][rr]")
{
    const auto t = build (synLoopSmf(), synManifest(), synMix(), 15360);

    // Kicks: a 64-16th block, so 0..15 across the loop.
    const auto kicks = notesOn (t, 10, 36);
    REQUIRE (kicks.size() == 16u);
    for (std::size_t i = 0; i < kicks.size(); ++i)
    {
        CHECK (kicks[i]->rr == i);
        CHECK (kicks[i]->panIndex == i);
    }

    // Closed hats: a 16-16th block, so 0..7 in every bar; the open hat counts on its own.
    const auto hats = notesOn (t, 10, 42);
    REQUIRE (hats.size() == 31u);
    for (std::size_t i = 0; i < hats.size(); ++i)
    {
        CAPTURE (i);
        CHECK (hats[i]->rr == i % 8);
        CHECK (hats[i]->panIndex == i % 8);
    }
    const auto open = notesOn (t, 10, 46);
    REQUIRE (open.size() == 1u);
    CHECK (open[0]->rr == 0);

    // A song counts by position too: bar 5 counts as bar 1 did.
    std::vector<midi::Note> songHats;
    for (int e = 0; e < 64; ++e)
        songHats.push_back (note (e * 480, 120, 10, 42, 90));
    const auto song = build (smf ({ track ("Hats", songHats) }, 30720), synManifest(), synMix(), 0);
    const auto sh = notesOn (song, 10, 42);
    REQUIRE (sh.size() == 64u);
    for (std::size_t i = 0; i < sh.size(); ++i)
        CHECK (sh[i]->rr == i % 8);

    // A pre-rolled note keeps the count of its written tick: the vox on tick 0 wraps to the loop's end but
    // is still the vox's first hit.
    const auto loop = build (smf ({ track ("Vox", { note (0, 480, 5, 60), note (3840, 480, 5, 62) }) }, 15360), synManifest(), synMix(), 15360);
    REQUIRE (loop.notes.size() == 2u);
    CHECK (loop.notes[0].key == 62);
    CHECK (loop.notes[0].rr == 1);
    CHECK (loop.notes[1].key == 60);
    CHECK (loop.notes[1].rr == 0);
}

TEST_CASE ("sequenced CC74 and CC71 ramp to their next point within a 16th; other points step", "[timeline][cc]")
{
    const auto bytes = smf ({ track ("Bass", { note (0, 240, 1, 36) },
                                {
                                    cc (0, 1, 74, 10),
                                    cc (60, 1, 74, 20),
                                    cc (300, 1, 74, 30), // 240 ticks after: still within a 16th
                                    cc (600, 1, 74, 40), // 300 after: a step
                                    cc (600, 1, 71, 5),
                                    cc (660, 1, 71, 6),
                                    cc (700, 1, 7, 100), // a volume: dropped
                                    cc (15300, 1, 74, 50), // a loop's last: ramps to the first, in the next cycle
                                }) },
        15360);

    SECTION ("in a loop")
    {
        const auto t = build (bytes, synManifest(), synMix(), 15360);
        REQUIRE (t.ccs.size() == 7u);
        auto at = [&t] (std::int64_t tick, int controller) -> const Cc* {
            for (const auto& c : t.ccs)
                if (c.pos == tick * 1000 && c.controller == controller)
                    return &c;
            return nullptr;
        };
        REQUIRE (at (0, 74) != nullptr);
        CHECK (at (0, 74)->rampTo == 60000);
        CHECK (at (0, 74)->nextValue == 20);
        CHECK (at (60, 74)->rampTo == 300000);
        CHECK (at (60, 74)->nextValue == 30);
        CHECK (at (300, 74)->rampTo == 300000); // the next point is 300 ticks away: a step
        CHECK (at (300, 74)->nextValue == 30);
        CHECK (at (600, 74)->rampTo == 600000);
        CHECK (at (600, 71)->rampTo == 660000);
        CHECK (at (600, 71)->nextValue == 6);
        CHECK (at (660, 71)->rampTo == 660000);
        CHECK (at (15300, 74)->rampTo == kLoopLength); // the first point, one cycle on
        CHECK (at (15300, 74)->nextValue == 10);
        CHECK (at (700, 7) == nullptr);
        for (const auto& c : t.ccs)
            CHECK (c.channel == 0);
        CHECK (std::is_sorted (t.ccs.begin(), t.ccs.end(), [] (const Cc& a, const Cc& b) { return a.pos < b.pos; }));
    }

    SECTION ("in a song the last point steps")
    {
        const auto t = build (bytes, synManifest(), synMix(), 0);
        REQUIRE (t.ccs.size() == 7u);
        CHECK (t.ccs.back().pos == 15300000);
        CHECK (t.ccs.back().rampTo == 15300000);
        CHECK (t.ccs.back().nextValue == 50);
    }
}

TEST_CASE ("CC91 and CC94 latch per note at its note-on and leave the controller list", "[timeline][throws]")
{
    const auto bytes = smf ({ track ("Vox",
                                { note (960, 480, 5, 60), note (1440, 480, 5, 62), note (1920, 480, 5, 64), note (14400, 480, 5, 65) },
                                { cc (960, 5, 91, 127), cc (960, 5, 94, 64), cc (1440, 5, 91, 0), cc (1440, 5, 94, 0), cc (1920, 5, 94, 127),
                                    cc (14880, 5, 94, 0), cc (15000, 5, 91, 32) }),
                              track ("Bass", { note (960, 480, 1, 36) }) },
        15360);

    SECTION ("in a song, from 0")
    {
        const auto t = build (bytes, synManifest(), synMix(), 0);
        CHECK (t.ccs.empty());
        const Note* thrown = findKey (t, 5, 60);
        const Note* after = findKey (t, 5, 62);
        const Note* echo = findKey (t, 5, 64);
        const Note* bass = findKey (t, 1, 36);
        REQUIRE (thrown != nullptr);
        REQUIRE (after != nullptr);
        REQUIRE (echo != nullptr);
        REQUIRE (bass != nullptr);
        CHECK (thrown->throwSpace == 127);
        CHECK (thrown->throwEcho == 64);
        CHECK (after->throwSpace == 0); // the controller on its own tick comes first
        CHECK (after->throwEcho == 0);
        CHECK (echo->throwSpace == 0);
        CHECK (echo->throwEcho == 127);
        CHECK (bass->throwSpace == 0); // another channel's
        CHECK (bass->throwEcho == 0);
    }

    SECTION ("in a loop the end's values carry into the start")
    {
        const auto loopBytes = smf ({ track ("Vox", { note (0, 480, 5, 60) }, { cc (15000, 5, 91, 32) }) }, 15360);
        const auto t = build (loopBytes, synManifest(), synMix(), 15360);
        REQUIRE (t.notes.size() == 1u);
        CHECK (t.notes[0].throwSpace == 32);
        CHECK (t.notes[0].throwEcho == 0);
    }
}

TEST_CASE ("duck keys sit on duck-key onsets, pre_us early where an onset duck asks", "[timeline][duck]")
{
    const auto t = build (synLoopSmf(), synManifest(), synMix (5000), 15360);
    // 5000 us at 120 BPM: 5000 x 120 x 2 / 125 = 9600 milliticks.
    CHECK (microsToMilliticks (5000, 120) == 9600);
    CHECK (microsToMilliticks (5000, 134) == 10720);

    std::vector<DuckKey> kicks, claps;
    for (const auto& k : t.keys)
        (k.keyLane == static_cast<std::uint8_t> (Lane::kick) ? kicks : claps).push_back (k);
    REQUIRE (kicks.size() == 16u);
    REQUIRE (claps.size() == 8u);
    CHECK (std::is_sorted (t.keys.begin(), t.keys.end(), [] (const DuckKey& a, const DuckKey& b) { return a.pos < b.pos; }));
    // The first kick's key wraps to the loop's end; the others sit 9600 before their beats.
    CHECK (kicks.back().pos == kLoopLength - 9600);
    for (std::size_t i = 0; i + 1 < kicks.size(); ++i)
        CHECK (kicks[i].pos == static_cast<std::int64_t> (i + 1) * 960000 - 9600);
    for (const auto& k : kicks)
        CHECK (k.velocity == 127);
    for (std::size_t i = 0; i < claps.size(); ++i)
    {
        CHECK (claps[i].keyLane == static_cast<std::uint8_t> (Lane::snare));
        CHECK (claps[i].pos == static_cast<std::int64_t> (2 * i + 1) * 960000); // no onset duck keys on the snare: no pre_us
        CHECK (claps[i].velocity == 100);
    }

    // Without pre_us every key sits on its onset.
    const auto plain = build (synLoopSmf(), synManifest(), synMix (0), 15360);
    CHECK (plain.keys.front().pos == 0);
}

TEST_CASE ("the chase list holds long notes, pre-rolled notes and nothing else", "[timeline][chase]")
{
    const auto t = build (synLoopSmf(), synManifest(), synMix(), 15360);
    std::vector<std::uint32_t> expected;
    for (std::size_t i = 0; i < t.notes.size(); ++i)
    {
        const auto& n = t.notes[i];
        const bool long_ = n.len > kMilliticksPerQuarter;
        const bool prerolled = n.channel == 4; // the vox
        CAPTURE (i, n.channel, n.key, n.len);
        CHECK (((n.flags & NoteFlag::chase) != 0) == (long_ || prerolled));
        if (long_ || prerolled)
            expected.push_back (static_cast<std::uint32_t> (i));
    }
    CHECK (t.chase == expected);
    CHECK (t.chase.size() == 3u); // the pad, the vox and the riser

    for (const auto& n : t.notes)
        CHECK (((n.flags & NoteFlag::endAligned) != 0) == (n.channel == 6)); // the riser
}

TEST_CASE ("a mono instrument's dips are flagged on both notes of the pair", "[timeline][dip]")
{
    // On the mono bass: 38 starts while 36 is held (legato) and the next legato note returns to 36, so 38
    // is a dip and the 36 after it the note returning from it. 43 is legato too, but 45 doesn't return
    // to 40: a slide. The pad (not mono) plays the same shape without dips.
    const std::vector<midi::Note> shape { note (0, 250, 1, 36), note (240, 250, 1, 38), note (480, 200, 1, 36), note (960, 250, 1, 40),
        note (1200, 250, 1, 43), note (1440, 100, 1, 45) };
    std::vector<midi::Note> pad;
    for (auto n : shape)
    {
        n.channel = 3;
        n.key = static_cast<std::uint8_t> (n.key + 24);
        pad.push_back (n);
    }

    for (const std::int64_t loopTicks : { std::int64_t { 0 }, std::int64_t { 3840 } })
    {
        CAPTURE (loopTicks);
        const auto t = build (smf ({ track ("Bass", shape), track ("Chords", pad) }, 3840), synManifest(), synMix(), loopTicks);
        REQUIRE (t.notes.size() == 12u);
        std::vector<int> dips;
        for (const auto& n : t.notes)
            if ((n.flags & NoteFlag::dip) != 0)
            {
                CHECK (n.channel == 0);
                dips.push_back (static_cast<int> (n.pos / 1000));
            }
        CHECK (dips == std::vector<int> { 240, 480 });
    }

    // Across a loop's seam: 38 (the last note) is held 10 ticks past the end, into the 36 on tick 0 of the
    // next cycle, which returns to the pitch before 38. In a song nothing follows 38.
    const auto seamFile = smf ({ track ("Bass", { note (0, 200, 1, 36), note (3360, 250, 1, 36), note (3600, 240, 1, 38) }) }, 3840);
    const auto seam = build (seamFile, synManifest(), synMix(), 3840);
    REQUIRE (seam.notes.size() == 3u);
    for (const auto& n : seam.notes)
        CHECK ((n.flags & NoteFlag::dip) == 0); // 38 ends exactly on the seam: not held into the next 36

    // The same notes as a loop of 3830 ticks: 38 is now held 10 ticks into the next cycle (a loop file
    // can't write that, since its notes can't end past it).
    const auto held = build (seamFile, synManifest(), synMix(), 3830);
    REQUIRE (held.notes.size() == 3u);
    CHECK (held.notes[0].key == 36);
    CHECK ((held.notes[0].flags & NoteFlag::dip) != 0); // the return, one cycle on
    CHECK ((held.notes[1].flags & NoteFlag::dip) == 0);
    CHECK ((held.notes[2].flags & NoteFlag::dip) != 0); // the dip
    const auto song = build (seamFile, synManifest(), synMix(), 0);
    for (const auto& n : song.notes)
        CHECK ((n.flags & NoteFlag::dip) == 0);
}

TEST_CASE ("a file that isn't one, or isn't at 960 ticks per quarter, is refused", "[timeline]")
{
    TimelineOptions loop;
    loop.loopTicks = 15360;
    const auto m = synManifest();
    const auto mix = synMix();

    auto bytes = synLoopSmf();
    bytes.resize (bytes.size() / 2);
    const auto cut = buildTimeline (bytes, m, mix, loop);
    REQUIRE_FALSE (cut.ok());
    CHECK (cut.error.rfind ("smf: ", 0) == 0);

    const std::vector<std::uint8_t> none;
    CHECK_FALSE (buildTimeline (none, m, mix, loop).ok());

    midi::SongSpec spec;
    spec.ppq = 480;
    spec.endTick = 7680;
    spec.tracks = { track ("Bass", { note (0, 120, 1, 36) }) };
    const auto bytes480 = midi::writeSmf (spec);
    REQUIRE (bytes480.ok());
    const auto r = buildTimeline (bytes480.value, m, mix, loop);
    REQUIRE_FALSE (r.ok());
    CHECK (r.error.find ("480") != std::string::npos);
}

TEST_CASE ("every embedded MIDI file builds into a timeline", "[timeline][content]")
{
    // The content's MIDI: content/midi/<kit>/, or WUBANSIA_MIDI_DIR (another checkout's, e.g. the MIDI v2
    // branch before it merges).
    std::filesystem::path root = std::filesystem::path (WUBANSIA_SOURCE_DIR) / "content" / "midi";
    if (const char* dir = std::getenv ("WUBANSIA_MIDI_DIR"))
        root = dir;
    REQUIRE (std::filesystem::is_directory (root));

    int files = 0;
    for (const auto& k : kKits)
    {
        std::string folder = k.id;
        std::transform (folder.begin(), folder.end(), folder.begin(), [] (unsigned char c) { return static_cast<char> (std::tolower (c)); });
        std::vector<std::filesystem::path> paths;
        for (const auto& entry : std::filesystem::recursive_directory_iterator (root / folder))
            if (entry.is_regular_file() && entry.path().extension() == ".mid" && entry.path().filename().string().rfind (k.id, 0) == 0)
                paths.push_back (entry.path());
        std::sort (paths.begin(), paths.end());
        REQUIRE_FALSE (paths.empty());

        const auto m = catchAll (k.bpm, k.swingPm);
        for (const auto& path : paths)
        {
            CAPTURE (path.string());
            ++files;
            const auto bytes = readFile (path);
            const auto file = midi::readSmf (bytes);
            REQUIRE (file.ok());

            // What the file holds on the channels the manifest plays.
            std::size_t mapped = 0;
            std::size_t rides = 0;
            std::int64_t end = 0;
            for (const auto& track : file.value.tracks)
            {
                end = std::max (end, track.endTick);
                for (const auto& n : midi::notesOf (track))
                    mapped += (n.channel != 2 && n.channel != 8) ? 1 : 0;
                for (const auto& c : midi::controllersOf (track))
                    rides += (c.number == 74 || c.number == 71) ? 1 : 0;
            }
            const auto markers = midi::markersOf (file.value);

            for (const bool loop : { false, true })
            {
                CAPTURE (loop);
                TimelineOptions options;
                options.loopTicks = loop ? end : 0;
                const auto t = buildTimeline (bytes, m, synMix(), options);
                REQUIRE (t.ok());
                const auto& tl = t.value;
                CHECK (tl.loops == loop);
                CHECK (tl.length == end * kMilliticksPerTick);
                CHECK (tl.notes.size() == mapped);
                CHECK (tl.ccs.size() == rides);
                CHECK (sortedByPosition (tl));
                CHECK (std::is_sorted (tl.ccs.begin(), tl.ccs.end(), [] (const Cc& a, const Cc& b) { return a.pos < b.pos; }));
                REQUIRE (tl.sections.size() == markers.size());
                for (std::size_t i = 0; i < markers.size(); ++i)
                {
                    CHECK (tl.sections[i].pos == markers[i].tick * kMilliticksPerTick);
                    CHECK (tl.sections[i].name == markers[i].text);
                }
                for (const auto& n : tl.notes)
                {
                    CHECK (n.len >= kMilliticksPerTick);
                    if (loop)
                    {
                        CHECK (n.pos >= 0);
                        CHECK (n.pos < tl.length);
                    }
                    // No onset is left on the written swing: it moved to the exact one.
                    const auto inEighth = ((n.pos % 480000) + 480000) % 480000;
                    CHECK (inEighth != 240000 + k.written * 1000);
                }
                for (const auto& c : tl.ccs)
                    CHECK ((c.controller == 74 || c.controller == 71));
            }
        }
    }
    CHECK (files >= 4 * 4);
}

TEST_CASE ("MIDI v2's loops latch their throws onto the thrown vox notes", "[timeline][content][throws]")
{
    // FUT02's loop throws its last vox note (CC91 and CC94 at 127 on tick 10560, back to 0 at 11520). The
    // MIDI v1 files have no throws: then there is nothing to check.
    std::filesystem::path root = std::filesystem::path (WUBANSIA_SOURCE_DIR) / "content" / "midi";
    if (const char* dir = std::getenv ("WUBANSIA_MIDI_DIR"))
        root = dir;
    const auto path = root / "fut02" / "FUT02_Loop_Vox_4bars.mid";
    if (! std::filesystem::exists (path))
        SKIP ("no MIDI v2 loop for FUT02 in " << root.string());

    const auto bytes = readFile (path);
    TimelineOptions options;
    options.loopTicks = 15360;
    const auto t = buildTimeline (bytes, catchAll (130, 520), synMix(), options);
    REQUIRE (t.ok());
    int thrown = 0;
    for (const auto& n : t.value.notes)
    {
        if (n.channel != 4)
        {
            CHECK (n.throwSpace == 0);
            continue;
        }
        if (n.throwSpace > 0 || n.throwEcho > 0)
        {
            ++thrown;
            CHECK (n.pos == 10560000);
            CHECK (n.throwSpace == 127);
            CHECK (n.throwEcho == 127);
        }
    }
    CHECK (thrown == 1);
}

TEST_CASE ("MIDI v2's FUT01 loop flags its bass line's dips: 16ths 6-7 and 21-22 of each 2-bar line", "[timeline][content][dip]")
{
    std::filesystem::path root = std::filesystem::path (WUBANSIA_SOURCE_DIR) / "content" / "midi";
    if (const char* dir = std::getenv ("WUBANSIA_MIDI_DIR"))
        root = dir;
    const auto path = root / "fut01" / "FUT01_Loop_Vox_4bars.mid";
    if (! std::filesystem::exists (path))
        SKIP ("no MIDI v2 loop for FUT01 in " << root.string());

    auto m = catchAll (134, 590);
    m.instruments[0].mono = true; // channel 1's: the bass
    TimelineOptions options;
    options.loopTicks = 15360;
    const auto t = buildTimeline (readFile (path), m, synMix(), options);
    REQUIRE (t.ok());
    std::vector<std::int64_t> sixteenths;
    for (const auto& n : t.value.notes)
        if ((n.flags & NoteFlag::dip) != 0)
        {
            CHECK (n.channel == 0);
            sixteenths.push_back (n.pos / kMilliticksPerSixteenth);
        }
    CHECK (sixteenths == std::vector<std::int64_t> { 6, 7, 21, 22, 38, 39, 53, 54 });
}

TEST_CASE ("P0's synthetic kit's four files build: sorted notes, lengths, sections, rides, the loop's wrap", "[timeline][synthetic]")
{
    const auto& content = wubansia::engine::testing::syntheticContent();
    for (std::size_t i = 0; i < 4; ++i)
    {
        CAPTURE (i);
        TimelineOptions options;
        options.loopTicks = i < 2 ? wubansia::engine::testing::kSyntheticLoopTicks : 0;
        const auto built = buildTimeline (content.smfs[i], content.kit, content.mix, options);
        REQUIRE (built.ok());
        const auto& t = built.value;
        CHECK (t.loops == (i < 2));
        CHECK (t.length == (i < 2 ? wubansia::engine::testing::kSyntheticLoopTicks : wubansia::engine::testing::kSyntheticSongTicks) * kMilliticksPerTick);
        REQUIRE_FALSE (t.notes.empty());
        CHECK (sortedByPosition (t));
        for (const auto& n : t.notes)
        {
            CHECK (n.len >= kMilliticksPerTick);
            if (t.loops)
            {
                CHECK (n.pos >= 0);
                CHECK (n.pos < t.length);
            }
        }
        const auto file = midi::readSmf (content.smfs[i]);
        REQUIRE (file.ok());
        const auto markers = midi::markersOf (file.value);
        REQUIRE (t.sections.size() == markers.size());
        for (std::size_t s = 0; s < markers.size(); ++s)
            CHECK (t.sections[s] == Section { markers[s].tick * kMilliticksPerTick, markers[s].text });
        if (! t.loops)
            CHECK (t.sections.size() >= 3u); // Intro, Drop, Outro
        CHECK (std::is_sorted (t.chase.begin(), t.chase.end()));
        // The swung 16ths (written 29 ticks late) sit on the exact 28.8.
        for (const auto& n : t.notes)
            CHECK (((n.pos % 480000) + 480000) % 480000 != 240000 + 29000);
    }
}
