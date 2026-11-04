// The reported metrics the audit's fixtures don't cover (R1, R2, R6, R11's aggregate, G5's part
// levels), on small files built here with answers worked out by hand in the comments. On the legacy
// baseline test_metrics_legacy.cpp checks them against legacy_expected.json; these keep them checked
// without the baseline too.

#include "metrics/metrics.h"
#include "midi/writer.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <string>
#include <vector>

using namespace wubansia;
using namespace wubansia::metrics;

namespace
{
    constexpr std::int64_t bar = 1920, step = 120;

    midi::Note note (std::int64_t tick, int channel, int key, int velocity = 100, std::int64_t length = 60)
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

    Song build (std::vector<midi::TrackSpec> tracks, std::int64_t endTick, std::string path = "built.mid")
    {
        midi::SongSpec spec;
        spec.endTick = endTick;
        spec.tracks = std::move (tracks);
        const auto bytes = midi::writeSmf (spec);
        INFO (bytes.error);
        REQUIRE (bytes.ok());
        auto song = loadSong (bytes.value, std::move (path));
        INFO (song.error);
        REQUIRE (song.ok());
        return song.value;
    }

    // 12 bars: a kick on step 0 of every bar but bar 6, which has it on step 4; a bass note on
    // step 0 of bars 0, 4 and 9.
    Song lagFile()
    {
        std::vector<midi::Note> kick;
        for (std::int64_t b = 0; b < 12; ++b)
            kick.push_back (note (b * bar + (b == 6 ? 4 * step : 0), 9, 36));
        return build ({ track ("Kick", kick), track ("Bass", { note (0, 1, 30), note (4 * bar, 1, 30), note (9 * bar, 1, 30) }) }, 12 * bar, "a.mid");
    }
}

TEST_CASE ("R1 lag-4 repetition per role", "[metrics][reported]")
{
    const Song s = lagFile();
    const Song* const list[] = { &s };
    const Value r = rRepetition (list);
    // kick: bars 4-11 each pair with bar i - 4; bars 6 and 10 differ from 2 and 6: 6 of 8 equal
    CHECK (r["kick"].num ("pairs") == 8);
    CHECK (r["kick"].num ("equal") == 6);
    CHECK (r["kick"].num ("equal_pm") == 750);
    // bass: only bar 4 has a non-empty bar 4 before it (bar 0), and they are equal
    CHECK (r["bass"].num ("pairs") == 1);
    CHECK (r["bass"].num ("equal") == 1);
    CHECK (r["bass"].num ("equal_pm") == 1000);
    CHECK (r["chords"].num ("pairs") == 0);
    CHECK (r["chords"].num ("equal_pm") == -1);
    CHECK (r["lead"].num ("pairs") == 0);
}

TEST_CASE ("R2 the longest run of identical 4-bar blocks", "[metrics][reported]")
{
    const Song a = lagFile();
    std::vector<midi::Note> steady;
    for (std::int64_t b = 0; b < 12; ++b)
        steady.push_back (note (b * bar, 9, 36));
    const Song b = build ({ track ("Kick", steady) }, 12 * bar, "b.mid");
    const Song* const list[] = { &a, &b };
    const Value r = rLoopTiling (list);
    // a's kick: blocks 0-3, 4-7 (bar 6 differs), 8-11: no two neighbours equal, longest 4.
    // b's kick: three equal blocks, 12. Median of [4, 12] (lower) 4, maximum 12.
    CHECK (r["kick"].num ("files") == 2);
    CHECK (r["kick"].num ("median_bars") == 4);
    CHECK (r["kick"].num ("max_bars") == 12);
    // a's bass: block 0 (bar 0) and block 1 (bar 4) are equal, block 2 (bar 9) isn't: 8. b has no bass.
    CHECK (r["bass"].num ("files") == 1);
    CHECK (r["bass"].num ("median_bars") == 8);
    CHECK (r["bass"].num ("max_bars") == 8);
    CHECK (r["chords"].num ("files") == 0);
    CHECK (r["chords"].num ("median_bars") == -1);
    CHECK (r["chords"].num ("max_bars") == -1);
}

TEST_CASE ("R6 breath and lead/vox complementarity", "[metrics][reported]")
{
    // 8 bars, so two 4-bar windows. The lead plays only in the first: a note at 0 sounding 960 ticks
    // (it covers the starts of steps 0-7) and one at bar 2 for one step: 9 of 64 sixteenths covered,
    // 55 rests: 55/64 = 859.4 thousandths, 859. The second window has no lead onset and isn't counted.
    // Half bars: 0 has the lead and the vox, 2 (bar 1, beat 2) the vox only, 4 (bar 2) the lead only:
    // 3 half bars with either, 2 with only one: 667.
    const Song s = build ({ track ("Lead", { note (0, 2, 72, 100, 960), note (2 * bar, 2, 74, 100, 120) }),
                              track ("Vox", { note (0, 3, 60), note (bar + 480, 3, 62) }) },
        8 * bar);
    // and with another vox-only half bar (6, bar 3): 4 half bars, 3 with only one: 750
    const Song* const list[] = { &s };
    const Value r = rBreath (list);
    CHECK (r.num ("lead_windows") == 1);
    CHECK (r.num ("median_rest_pm") == 859);
    CHECK (r.num ("half_bars_with_lead_or_vox") == 3);
    CHECK (r.num ("only_one_pm") == 667);

    const Song more = build ({ track ("Lead", { note (0, 2, 72, 100, 960), note (2 * bar, 2, 74, 100, 120) }),
                                 track ("Vox", { note (0, 3, 60), note (bar + 480, 3, 62), note (3 * bar, 3, 64) }) },
        8 * bar);
    const Song* const list2[] = { &more };
    const Value r2 = rBreath (list2);
    CHECK (r2.num ("half_bars_with_lead_or_vox") == 4);
    CHECK (r2.num ("only_one_pm") == 750);
}

TEST_CASE ("R11 syncopation means and SDs per lane group", "[metrics][reported]")
{
    // Kick bars with the fixtures' LHL cases: steps {0,4,8,12} 0, {2} 3, {0,6} 2, {3,7,11,15} 11.
    // Sum 16 over 4 bars: mean 4000. SD: n S2 - S1^2 = 4 * 134 - 256 = 280, sqrt 16.733 / 4 = 4.183.
    const std::vector<std::vector<int>> bars = { { 0, 4, 8, 12 }, { 2 }, { 0, 6 }, { 3, 7, 11, 15 } };
    std::vector<midi::Note> kick;
    for (std::size_t b = 0; b < bars.size(); ++b)
        for (const int st : bars[b])
            kick.push_back (note (static_cast<std::int64_t> (b) * bar + st * step, 9, 36));
    const Song s = build ({ track ("Kick", kick) }, 4 * bar);
    const Song* const list[] = { &s };
    const Value r = rSyncopation (list);
    CHECK (r["kick"].num ("bars") == 4);
    CHECK (r["kick"].num ("mean_milli") == 4000);
    CHECK (r["kick"].num ("sd_milli") == 4183);
    CHECK (r["kick"].num ("distinct") == 4);
    // Three bars, 0 + 3 + 2 = 5: 1666.7 thousandths, rounded half up to 1667. SD: 3 * 13 - 25 = 14,
    // sqrt 3.7417 / 3 = 1.2472.
    std::vector<midi::Note> three;
    for (std::size_t b = 0; b < 3; ++b)
        for (const int st : bars[b])
            three.push_back (note (static_cast<std::int64_t> (b) * bar + st * step, 9, 36));
    const Song t = build ({ track ("Kick", three) }, 3 * bar);
    const Song* const list3[] = { &t };
    const Value r3 = rSyncopation (list3);
    CHECK (r3["kick"].num ("mean_milli") == 1667);
    CHECK (r3["kick"].num ("sd_milli") == 1247);
    for (const char* lane : { "snare", "hats", "bass", "chords", "lead" })
    {
        INFO (lane);
        CHECK (r[lane].num ("bars") == 0);
        CHECK (r[lane].num ("mean_milli") == -1);
        CHECK (r[lane].num ("sd_milli") == -1);
        CHECK (r[lane].num ("distinct") == 0);
    }
}

TEST_CASE ("G5 part levels: the velocity values each lane group uses", "[metrics][reported]")
{
    // A drum part (closed hat 80 and 100, open hat 100, kick 110, clap 90, crash 70, conga 50) and a
    // melodic file (bass 64 and 100, chords 90, lead 118).
    const Song drums = build ({ track ("Drums",
                                  { note (0, 9, 42, 80), note (240, 9, 42, 100), note (480, 9, 46, 100), note (0, 9, 36, 110), note (480, 9, 39, 90), note (960, 9, 49, 70), note (960, 9, 63, 50) }) },
        bar,
        "d.mid");
    const Song melodic = build ({ track ("Bass", { note (0, 1, 30, 64), note (480, 1, 30, 100) }), track ("Chords", { note (0, 2, 60, 90), note (0, 2, 64, 90) }), track ("Lead", { note (0, 3, 72, 118) }) },
        bar,
        "m.mid");
    const Song* const list[] = { &drums, &melodic };
    const Value r = partLevels (list);
    CHECK (r.num ("most_levels_in_one_drum_file") == 6); // 50, 70, 80, 90, 100, 110
    CHECK (r["hats"].string() == "80,100");
    CHECK (r["perc"].string() == "50");
    CHECK (r["snareFamily"].string() == "90");
    CHECK (r["kick"].string() == "110");
    CHECK (r["bass"].string() == "64,100");
    CHECK (r["chords"].string() == "90");
    CHECK (r["lead"].string() == "118");
}

TEST_CASE ("R3 counts a closed-hat accent period of exactly 4 as short", "[metrics][reported]")
{
    // 16 closed hats with velocities 100 60 80 60, repeating: period 4 (not 2: step 2 is 80, not 100).
    std::vector<midi::Note> h;
    const int v[4] = { 100, 60, 80, 60 };
    for (int st = 0; st < 16; ++st)
        h.push_back (note (st * step, 9, 42, v[st % 4]));
    const Song s = build ({ track ("Hats", h) }, bar);
    const Song* const list[] = { &s };
    const Value r = rHatAccent (list);
    CHECK (r.num ("bars") == 1);
    CHECK (r.num ("period_4_or_less") == 1);
    CHECK (r["period_histogram"][0].num ("value") == 4);
}

TEST_CASE ("R9 a 2-bar pattern is not an odd cycle", "[metrics][reported]")
{
    // A rim on step 0 of even bars and step 4 of odd bars, over 8 bars: period 32, a multiple of 16.
    std::vector<midi::Note> rim;
    for (std::int64_t b = 0; b < 8; ++b)
        rim.push_back (note (b * bar + (b % 2 == 0 ? 0 : 4 * step), 9, 37));
    const Song s = build ({ track ("Rim", rim) }, 8 * bar);
    const Song* const list[] = { &s };
    const Value r = rOddCycles (list);
    CHECK (r.num ("lane_windows") == 1);
    CHECK (r.num ("odd_cycles") == 0);
}

TEST_CASE ("R10 a motif repeated inside one file is not a cross-seed duplicate", "[metrics][reported]")
{
    // File a plays one 2-bar lead motif twice (bars 0-1 and 2-3); file b plays another. 3 windows,
    // none duplicated across files.
    const auto motif = [] (std::int64_t at, int key) { return std::vector<midi::Note> { note (at, 2, key), note (at + 3 * step, 2, key + 3), note (at + bar, 2, key + 7) }; };
    std::vector<midi::Note> a = motif (0, 60);
    const auto again = motif (2 * bar, 62); // the same shape, transposed
    a.insert (a.end(), again.begin(), again.end());
    const Song sa = build ({ track ("Lead", a) }, 4 * bar, "a.mid");
    const Song sb = build ({ track ("Lead", { note (0, 2, 60), note (5 * step, 2, 61) }) }, 2 * bar, "b.mid");
    const Song* const list[] = { &sa, &sb };
    const Value r = rDuplicates (list, Role::lead);
    CHECK (r.num ("windows") == 3);
    CHECK (r.num ("duplicated") == 0);
}
