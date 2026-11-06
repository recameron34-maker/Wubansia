// The edges of the metric rules (tools/legacy_audit/README.md), on small files built here: each
// test puts notes on both sides of one boundary. A mutation run (one rule changed at a time) found
// these rules were reached by neither the audit's fixtures nor the legacy baseline, so without
// these tests a change to any of them would pass unnoticed.
//
// One mutation can't be caught by any test, and isn't: G4 counts sections of 9 bars or more, but an
// 8-bar section has no bar 8 to compare with bar 0, so the floor can never change a result.

#include "metrics/histogram.h"
#include "metrics/metrics.h"
#include "midi/writer.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

using namespace wubansia;
using namespace wubansia::metrics;

namespace
{
    constexpr std::int64_t bar = 1920;

    midi::Note note (std::int64_t tick, int channel, int key, int velocity = 100, std::int64_t length = 1)
    {
        midi::Note n;
        n.tick = tick;
        n.length = length;
        n.channel = static_cast<std::uint8_t> (channel);
        n.key = static_cast<std::uint8_t> (key);
        n.velocity = static_cast<std::uint8_t> (velocity);
        return n;
    }

    midi::TrackSpec track (std::string name, std::vector<midi::Note> notes) { return { std::move (name), std::move (notes), {}, {}, {} }; }

    Song build (std::vector<midi::TrackSpec> tracks, std::int64_t endTick, std::vector<midi::Marker> markers = {})
    {
        midi::SongSpec spec;
        spec.endTick = endTick;
        spec.markers = std::move (markers);
        spec.tracks = std::move (tracks);
        const auto bytes = midi::writeSmf (spec);
        INFO (bytes.error);
        REQUIRE (bytes.ok());
        auto song = loadSong (bytes.value, "built.mid");
        INFO (song.error);
        REQUIRE (song.ok());
        return song.value;
    }

    Value run (Value (*metric) (Songs), const Song& s)
    {
        const Song* const list[] = { &s };
        return metric (Songs (list)); // explicit: GCC 13 won't convert an array through a function pointer
    }
}

TEST_CASE ("G3 counts a 4-bar window only when it lies wholly inside the file", "[metrics][edges]")
{
    // Hats on 0 and 480 of bars 0-3. Without markers the file is one section of ceil (end / 1920)
    // bars, so a file ending halfway through bar 3 still has a 4-bar section, but its window would
    // run past the file's end.
    std::vector<midi::Note> hats;
    for (std::int64_t b = 0; b < 4; ++b)
        for (const std::int64_t at : { 0, 480 })
            hats.push_back (note (b * bar + at, 9, 42));
    const Song short_ = build ({ track ("Hats", hats) }, 3 * bar + 960);
    const Song whole = build ({ track ("Hats", hats) }, 4 * bar);
    REQUIRE (sectionsOf (short_)[0].bars == 4);
    CHECK (run (&g3, short_).num ("drum_windows") == 0);
    CHECK (run (&g3, whole).num ("drum_windows") == 1);
    CHECK (run (&g3, whole).num ("drum_repeated") == 1);
}

TEST_CASE ("G3 exempts a four-on-the-floor kick window within 2 ticks of every beat", "[metrics][edges]")
{
    // Four bars of kicks on the beats, one of them pushed off its beat by `off` ticks.
    const auto kicks = [] (std::int64_t off) {
        std::vector<midi::Note> k;
        for (std::int64_t b = 0; b < 4; ++b)
            for (const std::int64_t beat : { 0, 480, 960, 1440 })
                k.push_back (note (b * bar + beat + (b == 2 && beat == 960 ? off : 0), 9, 36));
        return k;
    };
    for (const std::int64_t off : { -2, 2 })
    {
        INFO ("off by " << off);
        const Value r = run (&g3, build ({ track ("Kick", kicks (off)) }, 4 * bar));
        CHECK (r.num ("kick_4x4_exempt") == 1);
        CHECK (r.num ("drum_windows") == 0);
    }
    for (const std::int64_t off : { -3, 3 })
    {
        INFO ("off by " << off);
        const Value r = run (&g3, build ({ track ("Kick", kicks (off)) }, 4 * bar));
        CHECK (r.num ("kick_4x4_exempt") == 0);
        CHECK (r.num ("drum_windows") == 1);
        CHECK (r.num ("drum_repeated") == 0); // bar 2 differs
    }
    // Only the kick group is exempt: the same pattern on a clap counts.
    std::vector<midi::Note> claps;
    for (std::int64_t b = 0; b < 4; ++b)
        for (const std::int64_t beat : { 0, 480, 960, 1440 })
            claps.push_back (note (b * bar + beat, 9, 39));
    const Value c = run (&g3, build ({ track ("Clap", claps) }, 4 * bar));
    CHECK (c.num ("kick_4x4_exempt") == 0);
    CHECK (c.num ("drum_repeated") == 1);
}

TEST_CASE ("G5 ghosts leave out the backbeat windows [450, 510] and [1410, 1470] exactly", "[metrics][edges]")
{
    // Bar 0: snares just inside and just outside both backbeat windows. The ones outside (449, 511,
    // 1409, 1471) are ghosts, each with its own velocity; the ones inside (450, 510, 1410, 1470)
    // aren't. Bars 1-7: one ghost each at velocity 30, so the window has 11 ghosts and 5 values.
    std::vector<midi::Note> snare = { note (449, 9, 38, 11), note (450, 9, 38, 21), note (510, 9, 38, 22), note (511, 9, 38, 12), note (1409, 9, 38, 13), note (1410, 9, 38, 23), note (1470, 9, 38, 24), note (1471, 9, 38, 14) };
    for (std::int64_t b = 1; b < 8; ++b)
        snare.push_back (note (b * bar + 240, 9, 38, 30));
    const Value ghosts = run (&g5, build ({ track ("Snare", snare) }, 8 * bar))["ghosts"];
    CHECK (ghosts.num ("windows") == 1);
    CHECK (ghosts.num ("median_distinct") == 5);
}

TEST_CASE ("G5 needs 16 hats in an 8-bar window", "[metrics][edges]")
{
    const auto hats = [] (int count) {
        std::vector<midi::Note> h;
        for (int i = 0; i < count; ++i)
            h.push_back (note (i * 480, 9, 42, 60 + i));
        return h;
    };
    const Value fifteen = run (&g5, build ({ track ("Hats", hats (15)) }, 8 * bar))["hats"];
    const Value sixteen = run (&g5, build ({ track ("Hats", hats (16)) }, 8 * bar))["hats"];
    CHECK (fifteen.num ("windows") == 0);
    CHECK (fifteen.num ("median_distinct") == -1);
    CHECK (sixteen.num ("windows") == 1);
    CHECK (sixteen.num ("median_distinct") == 16);
}

TEST_CASE ("G9 takes an fx note in [s - 2 bars, s + 480) as the boundary's gesture", "[metrics][edges]")
{
    // Two 8-bar sections, the boundary s at bar 8, and nothing else that could count as a gesture:
    // no drums (so no fill, cymbal or gap), only one riser note.
    constexpr std::int64_t s = 8 * bar;
    const auto withRiserAt = [] (std::int64_t tick) {
        return build ({ track ("Riser", { note (tick, 0, 60) }) }, 16 * bar, { { 0, "groove" }, { s, "drop" } });
    };
    const auto gestures = [] (const Song& song) {
        const Value r = run (&g9, song);
        REQUIRE (r.num ("boundaries") == 1);
        return r.num ("with_gesture");
    };
    CHECK (gestures (withRiserAt (s - 2 * bar)) == 1);
    CHECK (gestures (withRiserAt (s - 2 * bar - 1)) == 0);
    CHECK (gestures (withRiserAt (s + 479)) == 1);
    CHECK (gestures (withRiserAt (s + 480)) == 0);
}

TEST_CASE ("G10 counts bar 0 among the salient bars even when the first section starts later", "[metrics][edges]")
{
    // Sections from bar 8 and bar 24: the salient bars are 0, 8 and 24, so the intervals are 8 and 16.
    const Song s = build ({}, 32 * bar, { { 8 * bar, "drop" }, { 24 * bar, "outro" } });
    CHECK (run (&g10, s)["per_file"]["built.mid"].num ("interval_values") == 2);
}

TEST_CASE ("I3 pairs a bass onset with a kick up to 30 ticks either side", "[metrics][edges]")
{
    // One kick at 960; bass onsets 30 and 31 ticks before and after it.
    const Song s = build ({ track ("Kick", { note (960, 9, 36) }),
                              track ("Bass", { note (929, 1, 40), note (930, 1, 41), note (990, 1, 42), note (991, 1, 43) }) },
        bar);
    const Value r = run (&iKickBass, s);
    CHECK (r.num ("pairs") == 2); // 930 and 990
    CHECK (r.num ("within_2_ticks") == 0);
}

TEST_CASE ("G8 measures a lane with 16 odd-16th onsets in the drop, and never the kick", "[metrics][edges]")
{
    // One 8-bar drop. Hats with 15 or 16 onsets on odd 16ths, 10 ticks late; a kick with 32 of
    // them, 20 late, which must not count: the kick is the anchor.
    const auto song = [] (int hats) {
        std::vector<midi::Note> h, k;
        for (int i = 0; i < hats; ++i)
            h.push_back (note (i * 240 + 130, 9, 42));
        for (int i = 0; i < 32; ++i)
            k.push_back (note (i * 240 + 140, 9, 36));
        return build ({ track ("Hats", h), track ("Kick", k) }, 8 * bar, { { 0, "drop" } });
    };
    const Value fifteen = run (&g8, song (15))["per_file"]["built.mid"];
    const Value sixteen = run (&g8, song (16))["per_file"]["built.mid"];
    CHECK (fifteen["lanes"].string() == "");
    CHECK (fifteen.num ("distinct") == 0);
    CHECK (sixteen["lanes"].string() == "hats=10");
    CHECK (sixteen.num ("distinct") == 1);
}

TEST_CASE ("G9 counts a kick step as common when it is in three quarters of the bars", "[metrics][edges]")
{
    // A 4-bar section: kicks on steps 0 and 8 in bars 0-2, none in bar 3. Each step is in 3 of 4
    // bars, so both are common (4 * 3 >= 3 * 4), and bar 3 misses two: a fill (a dropout).
    std::vector<midi::Note> k;
    for (std::int64_t b = 0; b < 3; ++b)
        for (const std::int64_t st : { 0, 8 })
            k.push_back (note (b * bar + st * 120, 9, 36));
    const Value r = run (&g9, build ({ track ("Kick", k) }, 4 * bar, { { 0, "groove" } }));
    CHECK (r.num ("fills") == 1);
    CHECK (r["per_file"]["built.mid"]["fill_positions"].string() == "3");
}

TEST_CASE ("the most common value: ties go to the smallest key compared as text", "[metrics][edges]")
{
    using detail::topOf;
    const std::map<std::string, std::int64_t> shapes = { { "b", 2 }, { "a", 2 }, { "c", 1 } };
    CHECK (topOf (shapes)->first == "a");
    CHECK (topOf (shapes)->second == 2);
    const std::map<std::int64_t, std::int64_t> positions = { { 7, 3 }, { 3, 3 }, { 5, 1 } };
    CHECK (topOf (positions)->first == 3);
    const std::map<std::int64_t, std::int64_t> asText = { { 9, 2 }, { 10, 2 } };
    CHECK (topOf (asText)->first == 10); // "10" sorts before "9", as the audit's String (k) does
    CHECK_FALSE (topOf (std::map<std::int64_t, std::int64_t> {}).has_value());

    // On the G1 fixtures' drop windows all three shapes occur once: the top is the smallest.
    const auto load = [] (const char* name) {
        auto r = loadSongFile (std::filesystem::path (WUBANSIA_SOURCE_DIR) / "tools" / "legacy_audit" / "fixtures" / name, name);
        REQUIRE (r.ok());
        return r.value;
    };
    const Song a = load ("harmony_a.mid"), b = load ("harmony_b.mid"), c = load ("harmony_c.mid");
    const Song* const list[] = { &a, &b, &c };
    const Value drop = g1 (Songs (list))["drop"];
    REQUIRE (drop.num ("top") == 1);
    CHECK (drop["top_shape"].string() == *progressionShape (b, "drop"));
    CHECK (*progressionShape (b, "drop") < *progressionShape (c, "drop"));
    CHECK (*progressionShape (c, "drop") < *progressionShape (a, "drop"));
}
