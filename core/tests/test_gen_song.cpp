// Song-level invariants of the generator (plan §5.1 and the S1b1 brief): determinism, lane
// independence, speed, the arrangement's 8-bar lines, the style rules of 2-step and house, and the
// bass's behaviour against its kick.

#include "gen/content.h"
#include "gen/export.h"
#include "gen/feel.h"
#include "gen/rhythm.h"
#include "gen/song.h"
#include "midi/reader.h"
#include "midi/writer.h"
#include "util/file_io.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

using namespace wubansia;
using namespace wubansia::gen;
using Catch::Matchers::ContainsSubstring;

#if defined(__has_feature)
    #if __has_feature(address_sanitizer) || __has_feature(undefined_behavior_sanitizer)
        #define WUBANSIA_NO_TIMING 1
    #endif
#endif
#if defined(__SANITIZE_ADDRESS__)
    #define WUBANSIA_NO_TIMING 1
#endif

namespace
{
    const Bundle& bundleFor (std::string_view id)
    {
        static std::map<std::string, Bundle, std::less<>> cache;
        auto it = cache.find (id);
        if (it == cache.end())
        {
            auto b = loadBundle (std::filesystem::path (WUBANSIA_SOURCE_DIR) / "content", id);
            REQUIRE (b.ok());
            it = cache.emplace (std::string (id), std::move (b.value)).first;
        }
        return it->second;
    }

    Song songOf (const Bundle& b, std::uint64_t seed, std::int32_t bars = 0)
    {
        Options o;
        o.seed = seed;
        o.bars = bars;
        auto s = generate (b, o);
        REQUIRE (s.ok());
        return std::move (s.value);
    }

    Song songFor (const std::string& id, std::uint64_t seed) { return songOf (bundleFor (id), seed); }

    const LaneOut& laneOf (const Song& s, std::string_view id)
    {
        for (const auto& l : s.lanes)
            if (l.id == id)
                return l;
        FAIL ("no lane " << id);
        return s.lanes.front();
    }

    std::vector<std::uint8_t> fullBytes (const Song& s, Version v)
    {
        const auto r = midi::writeSmf (fullSpec (s, v));
        REQUIRE (r.ok());
        return r.value;
    }

    midi::File fullFile (const Song& s, Version v)
    {
        const auto bytes = fullBytes (s, v);
        const auto f = midi::readSmf (bytes);
        REQUIRE (f.ok());
        return f.value;
    }

    // The score steps of a lane's onsets of one voice (all voices if voice < 0).
    std::set<std::int64_t> stepsOf (const LaneOut& lane, int voice = -1)
    {
        std::set<std::int64_t> out;
        for (const Note& n : lane.score)
            if (voice < 0 || n.voice == voice)
                out.insert (n.step);
        return out;
    }

    bool isDrop (const std::string& kind) { return kind == "drop" || kind == "drop2"; }

    // Perturbs one lane's own parameters: every non-anchor probability p becomes 100 - p, the
    // velocities rise by 3 and the feel's 1/f spread doubles. Nothing else in the style changes.
    void perturbLane (Bundle& b, const std::string& id)
    {
        const auto flip = [] (Grid& g) {
            for (auto& v : g)
                if (v > 0 && v < 100)
                    v = 100 - v;
        };
        const auto louder = [] (Grid& g) {
            for (auto& v : g)
                if (v > 0)
                    v = std::min (127, v + 3);
        };
        for (auto& lane : b.style.lanes)
        {
            if (lane.id != id)
                continue;
            for (auto& g : lane.drums.grids)
                for (auto& v : g.voices)
                {
                    flip (v.p);
                    flip (v.orn);
                    louder (v.vel);
                    if (v.euclid)
                    {
                        v.euclid->p = 100 - v.euclid->p / 2;
                        v.euclid->vel = Range { v.euclid->vel.lo + 3, v.euclid->vel.hi + 3 };
                    }
                    if (v.poly)
                        v.poly->vel = Range { v.poly->vel.lo + 3, v.poly->vel.hi + 3 };
                }
            for (auto& g : lane.bass.grids)
            {
                flip (g.p);
                flip (g.follow);
                louder (g.vel);
            }
            lane.bass.approach = 100 - lane.bass.approach;
            for (auto& g : lane.chords.grids)
            {
                flip (g.p);
                louder (g.vel);
                g.padVel = std::min (127, g.padVel + 9);
            }
        }
        for (auto& [laneId, fl] : b.feel.lanes)
            if (laneId == id && fl.sd)
                fl.sd = Range { fl.sd->lo * 2, fl.sd->hi * 2 };
    }
}

TEST_CASE ("the same seed gives the same bytes and other seeds other songs", "[gen][song]")
{
    for (const char* id : { "FUT01", "FUT03" })
        for (std::uint64_t seed : { 0ull, 1ull, 977ull, 0xFFFFFFFFFFFFFFFFull })
        {
            INFO (id << " seed " << seed);
            const Song a = songFor (id, seed);
            const Song b = songFor (id, seed);
            for (const Version v : { Version::feel, Version::quantized })
            {
                const auto fa = renderFiles (a, v);
                const auto fb = renderFiles (b, v);
                REQUIRE (fa.ok());
                REQUIRE (fb.ok());
                REQUIRE (fa.value.size() == fb.value.size());
                for (std::size_t i = 0; i < fa.value.size(); ++i)
                {
                    CHECK (fa.value[i].path == fb.value[i].path);
                    CHECK (fa.value[i].bytes == fb.value[i].bytes);
                }
            }
            const Song c = songFor (id, seed + 1);
            CHECK (fullBytes (a, Version::feel) != fullBytes (c, Version::feel));
            CHECK (fullBytes (a, Version::quantized) != fullBytes (c, Version::quantized));
            CHECK (fullBytes (a, Version::feel) != fullBytes (a, Version::quantized));
        }
}

TEST_CASE ("changing one lane's parameters leaves the other lanes' bytes alone", "[gen][song]")
{
    // The bass follows and locks to the kick, so it may change with the kick, and its third, seventh
    // and fifth agree with what the chords voice, so it may change with them; nothing else depends
    // on another lane.
    const std::map<std::string, std::set<std::string>> dependents = { { "kick", { "bass" } }, { "chords", { "bass" } } };
    for (const char* id : { "FUT01", "FUT03" })
        for (const auto& lane : bundleFor (id).style.lanes)
        {
            INFO (id << " lane " << lane.id);
            Bundle changed = bundleFor (id);
            perturbLane (changed, lane.id);
            for (std::uint64_t seed : { 3ull, 4ull })
            {
                const Song a = songOf (bundleFor (id), seed);
                const Song b = songOf (changed, seed);
                for (const Version v : { Version::feel, Version::quantized })
                {
                    const midi::File fa = fullFile (a, v);
                    const midi::File fb = fullFile (b, v);
                    REQUIRE (fa.tracks.size() == fb.tracks.size());
                    CHECK (fa.tracks[0] == fb.tracks[0]); // the conductor
                    bool changedItself = false;
                    for (std::size_t t = 1; t < fa.tracks.size(); ++t)
                    {
                        const std::string& other = a.lanes[t - 1].id;
                        if (other == lane.id)
                        {
                            changedItself = changedItself || fa.tracks[t] != fb.tracks[t];
                            continue;
                        }
                        const auto dep = dependents.find (lane.id);
                        if (dep != dependents.end() && dep->second.contains (other))
                            continue;
                        INFO ("track of " << other);
                        CHECK (fa.tracks[t] == fb.tracks[t]);
                    }
                    CHECK (changedItself);
                }
            }
        }
}

TEST_CASE ("an extended mix generates in under 50 ms per 128 bars", "[gen][song][timing]")
{
    for (const char* id : { "FUT01", "FUT03" })
    {
        const Bundle& b = bundleFor (id);
        double best = 1e9;
        std::int32_t bars = 0;
        for (int run = 0; run < 5; ++run)
        {
            const auto t0 = std::chrono::steady_clock::now();
            const Song s = songOf (b, static_cast<std::uint64_t> (100 + run));
            const auto feel = renderFiles (s, Version::feel);
            const auto quant = renderFiles (s, Version::quantized);
            const auto t1 = std::chrono::steady_clock::now();
            REQUIRE (feel.ok());
            REQUIRE (quant.ok());
            bars = s.totalBars;
            best = std::min (best, std::chrono::duration<double, std::milli> (t1 - t0).count());
        }
        const double per128 = best * 128.0 / static_cast<double> (bars);
        WARN (id << ": " << bars << " bars with both versions' files in " << best << " ms, " << per128 << " ms per 128 bars");
        // and a 128-bar cut, which generates the whole song and then cuts it (cut.h)
        double bestCut = 1e9;
        for (int run = 0; run < 5; ++run)
        {
            const auto t0 = std::chrono::steady_clock::now();
            const Song s = songOf (b, static_cast<std::uint64_t> (100 + run), 128);
            const auto feel = renderFiles (s, Version::feel);
            const auto quant = renderFiles (s, Version::quantized);
            const auto t1 = std::chrono::steady_clock::now();
            REQUIRE (s.totalBars == 128);
            REQUIRE (feel.ok());
            REQUIRE (quant.ok());
            bestCut = std::min (bestCut, std::chrono::duration<double, std::milli> (t1 - t0).count());
        }
        WARN (id << ": a 128-bar cut with both versions' files in " << bestCut << " ms");
#if defined(NDEBUG) && !defined(__SANITIZE_ADDRESS__) && !defined(WUBANSIA_NO_TIMING)
        CHECK (per128 < 50.0);
        CHECK (bestCut < 50.0);
#endif
    }
}

TEST_CASE ("sections start on 8-bar lines and tile the song", "[gen][song]")
{
    for (const char* id : { "FUT01", "FUT03" })
        for (std::uint64_t seed = 0; seed < 8; ++seed)
        {
            const Song s = songFor (id, seed);
            std::int32_t bar = 0;
            for (const Section& sec : s.sections)
            {
                CHECK (sec.startBar % barsPerBlock == 0);
                CHECK (sec.bars % barsPerBlock == 0);
                CHECK (sec.startBar == bar);
                CHECK (sec.blocks.size() == static_cast<std::size_t> (sec.bars / barsPerBlock));
                bar += sec.bars;
            }
            CHECK (bar == s.totalBars);
            // a DJ-friendly extended mix: 200 to 240 bars (each seed draws its form), a 32-bar mix-in and mix-out
            CHECK (s.totalBars >= 200);
            CHECK (s.totalBars <= 240);
            CHECK (s.sections.front().kind == "intro");
            CHECK (s.sections.front().bars == 32);
            CHECK (s.sections.back().kind == "outro");
            CHECK (s.sections.back().bars == 32);
        }
}

TEST_CASE ("lanes play only in their active blocks so entries and exits fall on 8-bar lines", "[gen][song]")
{
    for (const char* id : { "FUT01", "FUT03" })
        for (std::uint64_t seed = 0; seed < 8; ++seed)
        {
            INFO (id << " seed " << seed);
            const Song s = songFor (id, seed);
            for (std::size_t li = 0; li < s.lanes.size(); ++li)
                for (const Note& n : s.lanes[li].perf)
                {
                    const Section& sec = s.sections[static_cast<std::size_t> (n.section)];
                    const auto bar = static_cast<std::int32_t> (n.step / stepsPerBar) - sec.startBar;
                    CHECK (sec.active (li, bar));
                    const std::int32_t nextBlock = (bar / barsPerBlock + 1) * barsPerBlock;
                    const std::int64_t blockEnd = (sec.startBar + nextBlock) * ticksPerBar;
                    if (!sec.active (li, nextBlock) || bar / barsPerBlock + 1 >= static_cast<std::int32_t> (sec.blocks.size()))
                        CHECK (n.tick + n.length <= blockEnd); // nothing rings into a block the lane sits out
                }
            // the mix-in starts with drums only and the mix-out ends with drums only
            const std::set<std::string> drums = { "kick", "snare", "hats", "shaker", "perc" };
            for (std::size_t li = 0; li < s.lanes.size(); ++li)
            {
                if (drums.contains (s.lanes[li].id))
                    continue;
                CHECK_FALSE (s.sections.front().active (li, 0));
                CHECK_FALSE (s.sections.back().active (li, s.sections.back().bars - 1));
            }
        }
}

TEST_CASE ("2-step is never four on the floor and keeps its skip kick", "[gen][song][style]")
{
    std::size_t bars = 0, skipBars = 0;
    for (std::uint64_t seed = 0; seed < 64; ++seed)
    {
        const Song s = songFor ("FUT01", seed);
        const auto kicks = stepsOf (laneOf (s, "kick"));
        for (const Section& sec : s.sections)
            for (std::int32_t b = 0; b < sec.bars; ++b)
            {
                const std::size_t kickLane = 0;
                if (!sec.active (kickLane, b))
                    continue;
                const std::int64_t s0 = (sec.startBar + b) * stepsPerBar;
                INFO ("seed " << seed << " bar " << sec.startBar + b);
                const bool four = kicks.contains (s0) && kicks.contains (s0 + 4) && kicks.contains (s0 + 8) && kicks.contains (s0 + 12);
                CHECK_FALSE (four);
                CHECK (kicks.contains (s0)); // beat 1 is an anchor
                CHECK_FALSE (kicks.contains (s0 + 8)); // the skipped beat
                ++bars;
                for (std::int64_t k = 6; k <= 11; ++k)
                    if (kicks.contains (s0 + k))
                    {
                        ++skipBars;
                        break;
                    }
            }
    }
    CHECK (bars > 64 * 100);
    CHECK (skipBars * 100 >= bars * 90); // the skip kick around the and of 3 in nearly every bar (turnarounds may drop it)
}

TEST_CASE ("house keeps the kick on every beat of every drop bar", "[gen][song][style]")
{
    // every drop bar where the lane plays: a lane can sit out a block or a bar of a drop (the form
    // grammar's lane moves)
    const auto lane = [] (const Song& s, std::string_view id) {
        for (std::size_t i = 0; i < s.lanes.size(); ++i)
            if (s.lanes[i].id == id)
                return i;
        FAIL ("no lane " << id);
        return std::size_t (0);
    };
    for (std::uint64_t seed = 0; seed < 64; ++seed)
    {
        const Song s = songFor ("FUT03", seed);
        const std::size_t kickLane = lane (s, "kick"), snareLane = lane (s, "snare"), hatsLane = lane (s, "hats"), shakerLane = lane (s, "shaker");
        const auto kicks = stepsOf (laneOf (s, "kick"));
        const auto claps = stepsOf (laneOf (s, "snare"), 0); // voice 0: the clap
        std::multiset<std::int64_t> opens;
        for (const Note& n : laneOf (s, "hats").score)
            if (n.voice == 1)
                opens.insert (n.step);
        std::size_t shakerHits = 0, shakerBars = 0;
        const auto shaker = stepsOf (laneOf (s, "shaker"));
        for (const Section& sec : s.sections)
        {
            if (!isDrop (sec.kind))
                continue;
            for (std::int32_t b = 0; b < sec.bars; ++b)
            {
                const std::int64_t s0 = (sec.startBar + b) * stepsPerBar;
                INFO ("seed " << seed << " bar " << sec.startBar + b);
                if (sec.active (kickLane, b))
                    for (const std::int64_t beat : { 0, 4, 8, 12 })
                        CHECK (kicks.contains (s0 + beat));
                if (sec.active (snareLane, b))
                {
                    CHECK (claps.contains (s0 + 4)); // the clap on 2 and 4
                    CHECK (claps.contains (s0 + 12));
                }
                if (sec.active (hatsLane, b))
                    for (const std::int64_t off : { 2, 6, 10, 14 })
                        CHECK (opens.contains (s0 + off)); // offbeat open hats
                if (!sec.active (shakerLane, b))
                    continue;
                for (std::int64_t k = 0; k < 16; ++k)
                    shakerHits += shaker.contains (s0 + k) ? 1u : 0u;
                ++shakerBars;
            }
        }
        CHECK (shakerHits >= shakerBars * 13); // 16th shakers: most 16ths of every drop bar
    }
}

TEST_CASE ("kick-bass coincident onsets are within 2 ticks in every file", "[gen][song]")
{
    for (const char* id : { "FUT01", "FUT03" })
    {
        // both basslines groove against the kick, so few of their notes land on one (the 2-step
        // line's first note under a rare skip kick, approach notes): each that does is locked to it
        std::size_t nearAll = 0;
        for (std::uint64_t seed = 0; seed < 16; ++seed)
            for (const Version v : { Version::feel, Version::quantized })
            {
                INFO (id << " seed " << seed << " " << versionName (v));
                const Song s = songFor (id, seed);
                const midi::File f = fullFile (s, v);
                std::size_t kickTrack = 0, bassTrack = 0;
                for (std::size_t t = 0; t < s.lanes.size(); ++t)
                {
                    if (s.lanes[t].id == "kick")
                        kickTrack = t + 1;
                    if (s.lanes[t].id == "bass")
                        bassTrack = t + 1;
                }
                std::vector<std::int64_t> kicks;
                for (const auto& n : midi::notesOf (f.tracks[kickTrack]))
                    kicks.push_back (n.tick);
                std::size_t near = 0;
                for (const auto& n : midi::notesOf (f.tracks[bassTrack]))
                {
                    const auto it = std::lower_bound (kicks.begin(), kicks.end(), n.tick - 30);
                    if (it != kicks.end() && *it <= n.tick + 30)
                    {
                        ++near;
                        CHECK (std::abs (*it - n.tick) <= 2);
                    }
                }
                nearAll += near;
            }
        INFO (id << ": " << nearAll << " bass onsets near a kick in 16 seeds, both versions");
        CHECK (nearAll > 0);
    }
}

TEST_CASE ("the 2-step hats loop: no loud hat sits out a bar at random", "[gen][song][style]")
{
    // The hats' skeleton loops with its 2-bar pattern; only ghost-level hats vary bar by bar (plan
    // §2.3's "every bar: micro changes"). So in a groove or drop bar, a step where a pattern hat
    // sounds at velocity 85 or more both 2 bars before and 2 bars after holds a hat too.
    std::size_t checked = 0;
    for (std::uint64_t seed = 0; seed < 64; ++seed)
    {
        const Song s = songFor ("FUT01", seed);
        const LaneOut& hats = laneOf (s, "hats");
        const std::size_t li = static_cast<std::size_t> (&hats - s.lanes.data());
        std::map<std::int64_t, std::int32_t> loud; // step -> the loudest pattern hat there
        std::set<std::int64_t> any; // steps with any hat
        for (const Note& n : hats.score)
        {
            any.insert (n.step);
            if (!(n.flags & (noteFlag::ornament | noteFlag::turn)))
                loud[n.step] = std::max (loud[n.step], std::int32_t (n.velocity));
        }
        const auto loudAt = [&] (std::int64_t step) { const auto it = loud.find (step); return it != loud.end() && it->second >= 85; };
        for (const Section& sec : s.sections)
        {
            if (sec.kind != "groove" && !isDrop (sec.kind))
                continue;
            for (std::int32_t b = 2; b + 2 < sec.bars; ++b)
            {
                if (!sec.active (li, b - 2) || !sec.active (li, b) || !sec.active (li, b + 2))
                    continue;
                for (std::int64_t j = 0; j < stepsPerBar; ++j)
                {
                    const std::int64_t at = (sec.startBar + b) * stepsPerBar + j;
                    if (!loudAt (at - 2 * stepsPerBar) || !loudAt (at + 2 * stepsPerBar))
                        continue;
                    ++checked;
                    INFO ("seed " << seed << " " << sec.kind << " at bar " << sec.startBar << ", bar " << b << " step " << j);
                    CHECK (any.contains (at));
                }
            }
        }
    }
    CHECK (checked > 10000);
}

TEST_CASE ("the 2-step sub grooves against the kick: room on its steps, answers in the gaps, beat 2 held", "[gen][song][style]")
{
    // Round 1's binding rule (docs/plans/S1-four-kits.md, ADR 0010): a bassline that grooves against
    // the kick instead of following it, leaving room on the kick and answering it in the gaps, with
    // short and held notes. The sub still sounds through beat 2 of every drop bar (round 1's hold).
    std::size_t onsets = 0, onKick = 0, bars = 0, silent = 0, held = 0, shortNotes = 0;
    for (std::uint64_t seed = 0; seed < 64; ++seed)
    {
        const Song s = songFor ("FUT01", seed);
        const LaneOut& bass = laneOf (s, "bass");
        const std::size_t li = static_cast<std::size_t> (&bass - s.lanes.data());
        const auto kicks = stepsOf (laneOf (s, "kick"));
        for (const Section& sec : s.sections)
        {
            if (!isDrop (sec.kind))
                continue;
            for (const Note& n : bass.score)
                if (n.section == static_cast<std::int16_t> (sec.index))
                {
                    ++onsets;
                    onKick += kicks.contains (n.step) ? 1u : 0u;
                    (n.length >= 240 ? held : shortNotes) += 1;
                }
            for (std::int32_t b = 0; b < sec.bars; ++b)
            {
                if (!sec.active (li, b))
                    continue;
                ++bars;
                const std::int64_t beat2 = (sec.startBar + b) * ticksPerBar + 480; // steps 4 to 7
                const bool sounds = std::any_of (bass.score.begin(), bass.score.end(), [&] (const Note& n) { return n.tick < beat2 + 480 && n.tick + n.length > beat2; });
                silent += sounds ? 0u : 1u;
            }
        }
    }
    INFO (onsets << " drop bass onsets, " << onKick << " on a kick step; " << held << " held and " << shortNotes << " short; beat 2 silent in " << silent << " of " << bars << " bars");
    CHECK (onKick * 100 <= onsets * 10);
    CHECK (silent == 0);
    CHECK (held * 100 >= onsets * 20);
    CHECK (shortNotes * 100 >= onsets * 20);
}

TEST_CASE ("the house bass bounces on the offbeats with octave jumps", "[gen][song][style]")
{
    std::size_t onsets = 0, offbeat = 0, onKick = 0, bars = 0, octaveBars = 0;
    for (std::uint64_t seed = 0; seed < 32; ++seed)
    {
        const Song s = songFor ("FUT03", seed);
        const LaneOut& bass = laneOf (s, "bass");
        const auto kicks = stepsOf (laneOf (s, "kick"));
        std::map<std::int64_t, std::vector<const Note*>> byBar;
        for (const Note& n : bass.score)
        {
            const Section& sec = s.sections[static_cast<std::size_t> (n.section)];
            if (!isDrop (sec.kind))
                continue;
            ++onsets;
            const auto pos = n.step % 16;
            offbeat += (pos % 4 == 2 || pos % 4 == 3) ? 1u : 0u;
            onKick += kicks.contains (n.step) ? 1u : 0u;
            byBar[n.step / 16].push_back (&n);
        }
        for (const auto& [bar, notes] : byBar)
        {
            ++bars;
            for (std::size_t i = 1; i < notes.size(); ++i)
                if (std::abs (int (notes[i]->key) - int (notes[i - 1]->key)) == 12)
                {
                    ++octaveBars;
                    break;
                }
        }
    }
    INFO ("offbeat " << offbeat << " of " << onsets << ", on a kick " << onKick << ", octave bars " << octaveBars << " of " << bars);
    CHECK (offbeat * 100 >= onsets * 80);
    CHECK (onKick * 100 <= onsets * 10);
    CHECK (octaveBars * 100 >= bars * 40);
}

TEST_CASE ("approach notes lead into chord changes and slides overlap the next note", "[gen][song]")
{
    for (const char* id : { "FUT01", "FUT03" })
    {
        std::size_t approaches = 0, slides = 0;
        for (std::uint64_t seed = 0; seed < 16; ++seed)
        {
            INFO (id << " seed " << seed);
            const Song s = songFor (id, seed);
            const LaneOut& bass = laneOf (s, "bass");
            std::set<std::int64_t> changes; // where chords change: pushed starts and the lines they were pushed from
            for (const auto& c : s.chords)
            {
                changes.insert (c.start);
                changes.insert (c.start + c.pushedBy);
            }
            for (std::size_t i = 0; i < bass.score.size(); ++i)
            {
                const Note& n = bass.score[i];
                if (n.flags & noteFlag::approach)
                {
                    ++approaches;
                    REQUIRE (i + 1 < bass.score.size());
                    const Note& next = bass.score[i + 1];
                    const std::int64_t lead = next.tick - n.tick;
                    CHECK ((lead == 120 || lead == 240));
                    CHECK (changes.contains (next.tick)); // it leads into the note on the change
                    const int step = std::abs (int (next.key) - int (n.key));
                    CHECK (step >= 1);
                    CHECK (step <= 2); // a scale step from the note it leads into
                }
                if (n.flags & noteFlag::slide)
                {
                    ++slides;
                    REQUIRE (i + 1 < bass.score.size());
                    const Note& next = bass.score[i + 1];
                    CHECK (n.tick + n.length > next.tick); // legato: it overlaps the next note
                    CHECK (n.key != next.key);
                    CHECK (std::abs (int (next.key) - int (n.key)) <= 12);
                }
            }
        }
        INFO (id);
        CHECK (approaches > 20);
        CHECK (slides > 10);
    }
}

TEST_CASE ("drum notes stay inside 35-59 on channel 10", "[gen][song]")
{
    for (const char* id : { "FUT01", "FUT03" })
    {
        const Song s = songFor (id, 5);
        for (const auto& lane : s.lanes)
        {
            if (lane.kind != LaneKind::drums)
                continue;
            CHECK (lane.channel == 9);
            for (const Note& n : lane.score)
            {
                CHECK (n.key >= 35);
                CHECK (n.key <= 59);
            }
        }
    }
}

TEST_CASE ("bars cuts the arrangement on an 8-bar line and refuses other lengths", "[gen][song]")
{
    const Bundle& b = bundleFor ("FUT01");
    const Song full = songOf (b, 9);
    const std::int32_t three = full.sections[0].bars + full.sections[1].bars + full.sections[2].bars;
    const Song cut = songOf (b, 9, three); // the intro (32), the groove and the section after it
    CHECK (cut.totalBars == three);
    CHECK (cut.sections.size() == 3);
    std::int32_t bars = 0;
    for (const auto& sec : cut.sections)
        bars += sec.bars;
    CHECK (bars == three);
    REQUIRE (full.sections[1].bars >= 16);
    const Song mid = songOf (b, 9, 40); // the groove (16 bars or more) cut after its first 8
    CHECK (mid.totalBars == 40);
    REQUIRE (mid.sections.size() == 2);
    CHECK (mid.sections.back().bars == 8);
    CHECK (mid.sections.back().blocks == std::vector<std::uint64_t> (full.sections[1].blocks.begin(), full.sections[1].blocks.begin() + 1)); // its first block
    // a cut at the song's end is the whole song
    const Song all = songOf (b, 9, full.totalBars);
    for (const Version v : { Version::feel, Version::quantized })
        CHECK (fullBytes (all, v) == fullBytes (full, v));

    Options o;
    o.seed = 9;
    o.bars = 20;
    CHECK_THAT (generate (b, o).error, ContainsSubstring ("8"));
    o.bars = -8;
    CHECK_FALSE (generate (b, o).ok());
    o.bars = full.totalBars + 8;
    CHECK_THAT (generate (b, o).error, ContainsSubstring (std::to_string (full.totalBars)));
    o.bars = 0;
    o.feelAmount = 2001;
    CHECK_FALSE (generate (b, o).ok());
}

TEST_CASE ("a section ends the same whatever the next one starts with", "[gen][song]")
{
    // Rules that look at a neighbouring note stay inside its section (ADR 0013 §3), so a region is
    // the same whatever region follows it. The bass's hold gap is one of them: a held note runs to
    // the next note less the gap, but only to a next note inside its own active range (lanes.cpp),
    // so a section's last held note ends the same whether the next section starts with a bass note
    // on that line or not. Only a bass lane without a duck shows it (a duck ends every note its gap,
    // more than the hold gap, before its section's end), and no shipped one starts a section on its
    // line: FUT04's rumble has a note on six section lines of every song, but its duck ends the note
    // before at least 60 ticks short of the line either way. So this edits garage2step: the bass
    // plays all through the break (not only its last 8 bars), whose grid plays the 1 (a 100% cell), so the bass starts the
    // break on the line where the section before it ends. Every section but the breaks stays the
    // same, note for note and controller for controller, in both versions.
    const auto read = [] (const std::filesystem::path& p) {
        const auto r = util::readBinaryFile (p);
        REQUIRE (r.ok());
        return std::string (r.value.begin(), r.value.end());
    };
    const auto content = std::filesystem::path (WUBANSIA_SOURCE_DIR) / "content";
    std::string style = read (content / "styles" / "garage2step.json");
    const std::string bassLast = ",\n          { \"lanes\": [\"bass\"], \"last\": [ { \"bars\": 8, \"w\": 1 } ] }";
    const auto at = style.find (bassLast);
    REQUIRE (at != std::string::npos);
    style.erase (at, bassLast.size());
    auto kit = parseKit (read (content / "kits" / "fut01.json"), "fut01.json");
    auto sty = parseStyle (style, "garage2step.json");
    auto feel = parseFeel (read (content / "feels" / "garage2step.json"), "garage2step.json");
    REQUIRE (kit.ok());
    INFO (sty.error);
    REQUIRE (sty.ok());
    REQUIRE (feel.ok());
    auto edited = makeBundle (std::move (kit.value), std::move (sty.value), std::move (feel.value));
    INFO (edited.error);
    REQUIRE (edited.ok());

    std::size_t breaks = 0, onTheLine = 0, toTheLine = 0, compared = 0;
    std::vector<std::string> differences;
    for (std::uint64_t seed = 0; seed < 16; ++seed)
    {
        const Song shipped = songOf (bundleFor ("FUT01"), seed);
        const Song changed = songOf (edited.value, seed);
        REQUIRE (changed.sections.size() == shipped.sections.size());
        for (const Section& sec : shipped.sections)
        {
            if (sec.kind != "break")
                continue;
            ++breaks;
            const Section& before = shipped.sections[sec.index - 1]; // a drop, or a groove before an early break
            const auto& bass = laneOf (changed, "bass").score;
            onTheLine += std::any_of (bass.begin(), bass.end(), [&] (const Note& n) { return n.tick == sec.startTick(); }) ? 1u : 0u;
            // the case the rule is for: the last note before the break runs to the line
            const auto& shippedBass = laneOf (shipped, "bass").score;
            const auto last = std::find_if (shippedBass.rbegin(), shippedBass.rend(), [&] (const Note& n) { return n.section == static_cast<std::int16_t> (before.index); });
            if (last != shippedBass.rend())
                toTheLine += last->tick + last->length == before.endTick() ? 1u : 0u;
        }
        for (std::size_t l = 0; l < shipped.lanes.size(); ++l)
            for (const Section& sec : shipped.sections)
            {
                if (sec.kind == "break")
                    continue;
                const auto in = [&] (const std::vector<Note>& notes) {
                    std::vector<Note> out;
                    for (const Note& n : notes)
                        if (n.section == static_cast<std::int16_t> (sec.index))
                            out.push_back (n);
                    return out;
                };
                const auto ccIn = [&] (const std::vector<ControlPoint>& points) {
                    std::vector<ControlPoint> out;
                    for (const ControlPoint& c : points)
                        if (c.section == static_cast<std::int16_t> (sec.index))
                            out.push_back (c);
                    return out;
                };
                const LaneOut& a = shipped.lanes[l];
                const LaneOut& b = changed.lanes[l];
                for (const bool performed : { false, true })
                {
                    const auto want = in (performed ? a.perf : a.score);
                    const auto got = in (performed ? b.perf : b.score);
                    ++compared;
                    if (got == want)
                        continue;
                    std::size_t i = 0;
                    while (i < std::min (got.size(), want.size()) && got[i] == want[i])
                        ++i;
                    const auto shown = [] (const std::vector<Note>& v, std::size_t k) {
                        return k < v.size() ? "tick " + std::to_string (v[k].tick) + " length " + std::to_string (v[k].length) + " key " + std::to_string (v[k].key) : std::string ("none");
                    };
                    differences.push_back ("seed " + std::to_string (seed) + ", the " + sec.kind + " at bar " + std::to_string (sec.startBar) + ", " + a.id + (performed ? " performed" : " score")
                                           + ": note " + std::to_string (i) + " is " + shown (got, i) + " with the edit, " + shown (want, i) + " without");
                }
                if (ccIn (b.cc) != ccIn (a.cc))
                    differences.push_back ("seed " + std::to_string (seed) + ", the " + sec.kind + " at bar " + std::to_string (sec.startBar) + ", " + a.id + ": the controllers");
            }
    }
    std::string shown;
    for (std::size_t i = 0; i < std::min<std::size_t> (differences.size(), 8); ++i)
        shown += "\n  " + differences[i];
    INFO (differences.size() << " of " << compared << " section lanes differ; the first:" << shown);
    CHECK (differences.empty());
    CHECK (breaks >= 16);
    CHECK (onTheLine == breaks); // the edit starts every break with a bass note on its line
    CHECK (toTheLine >= 3); // and in some songs the drop's last bass note runs to that line
}

TEST_CASE ("stab patterns keep their minimum of hits in every bar of a drop", "[gen][song][style]")
{
    for (const char* id : { "FUT01", "FUT03" })
    {
        std::size_t bars = 0, thin = 0;
        for (std::uint64_t seed = 0; seed < 32; ++seed)
        {
            const Song s = songFor (id, seed);
            std::set<std::int64_t> onsets;
            for (const Note& n : laneOf (s, "chords").score)
                onsets.insert (n.step);
            const auto chords = static_cast<std::size_t> (&laneOf (s, "chords") - s.lanes.data());
            for (const Section& sec : s.sections)
            {
                if (!isDrop (sec.kind))
                    continue;
                for (std::int32_t b = 0; b < sec.bars; ++b)
                {
                    if (!sec.active (chords, b))
                        continue; // the stabs sit out a block or a bar of the drop (the form's lane moves)
                    const std::int64_t s0 = (sec.startBar + b) * stepsPerBar;
                    std::size_t n = 0;
                    for (std::int64_t k = 0; k < 16; ++k)
                        n += onsets.contains (s0 + k) ? 1u : 0u;
                    ++bars;
                    thin += n < 2 ? 1u : 0u;
                }
            }
        }
        INFO (id << ": " << thin << " of " << bars << " drop bars with fewer than 2 stabs");
        CHECK (thin * 100 <= bars * 2);
    }
}

TEST_CASE ("the bass and chords change on phrase lines", "[gen][song]")
{
    // G4's definition (tools/legacy_audit/README.md): a bar's fingerprint is its set of (16th, key)
    // onsets; inside a section, bar i >= 8 differs if it isn't bar i - 8's; a macro change is a
    // differing bar after one that didn't; it is on a phrase bar if i mod 8 is 0 or 7.
    for (const char* id : { "FUT01", "FUT03" })
        for (const char* laneId : { "bass", "chords" })
        {
            std::size_t changes = 0, onPhrase = 0;
            for (std::uint64_t seed = 0; seed < 32; ++seed)
            {
                const Song s = songFor (id, seed);
                const LaneOut& lane = laneOf (s, laneId);
                for (const Section& sec : s.sections)
                {
                    if (sec.bars < 9)
                        continue;
                    std::vector<std::set<std::int64_t>> bars (static_cast<std::size_t> (sec.bars));
                    for (const Note& n : lane.score)
                    {
                        const std::int64_t b = n.step / stepsPerBar - sec.startBar;
                        if (b >= 0 && b < sec.bars)
                            bars[static_cast<std::size_t> (b)].insert ((n.step % stepsPerBar) * 128 + n.key);
                    }
                    bool prev = false;
                    for (std::size_t i = 8; i < bars.size(); ++i)
                    {
                        const bool differs = bars[i] != bars[i - 8];
                        if (differs && !prev)
                        {
                            ++changes;
                            onPhrase += (i % 8 == 0 || i % 8 == 7) ? 1u : 0u;
                        }
                        prev = differs;
                    }
                }
            }
            INFO (id << " " << laneId << ": " << onPhrase << " of " << changes << " macro changes on phrase bars");
            CHECK (changes > 0);
            CHECK (onPhrase * 100 >= changes * 60); // G4's gate is 50%; random placement gives 25%
        }
}

TEST_CASE ("every section opens on a chord stack", "[gen][song]")
{
    // a section's first chord onset has 2 or more notes, so the harmony reads from its first bar
    // (single-note riffs come later, as a development)
    for (const char* id : { "FUT01", "FUT03" })
        for (std::uint64_t seed = 0; seed < 32; ++seed)
        {
            const Song s = songFor (id, seed);
            const LaneOut& ch = laneOf (s, "chords");
            for (const Section& sec : s.sections)
            {
                std::int64_t first = -1;
                std::size_t count = 0;
                for (const Note& n : ch.score)
                {
                    if (static_cast<std::size_t> (n.section) != sec.index)
                        continue;
                    if (first < 0)
                        first = n.tick;
                    if (n.tick == first)
                        ++count;
                }
                if (first < 0)
                    continue; // the lane sits the section out
                INFO (id << " seed " << seed << " " << sec.kind << " at bar " << sec.startBar);
                CHECK (count >= 2);
            }
        }
}

TEST_CASE ("one seed gives unrelated songs in different kits", "[gen][song]")
{
    // The key tree is rooted at (seed, kit id): seed N of one kit says nothing about seed N of another.
    std::size_t sameHarmony = 0, samePushes = 0;
    for (std::uint64_t seed = 0; seed < 32; ++seed)
    {
        const Song a = songFor ("FUT01", seed);
        const Song b = songFor ("FUT03", seed);
        const auto mainOf = [] (const Song& s) {
            for (const auto& h : s.harmony)
                if (h.slot == "main")
                    return std::make_tuple (h.degrees, h.units, h.loopBars, h.mode);
            return std::make_tuple (std::vector<std::int32_t> {}, std::vector<std::int32_t> {}, 0, Mode::aeolian);
        };
        sameHarmony += mainOf (a) == mainOf (b) ? 1u : 0u;
        samePushes += a.harmony.front().pushPermille == b.harmony.front().pushPermille ? 1u : 0u;
    }
    INFO (sameHarmony << " of 32 seeds share the main progression, " << samePushes << " the intro's push share");
    CHECK (sameHarmony <= 4); // chance agreement only
    CHECK (samePushes <= 2); // a share is drawn from 201 values
}

TEST_CASE ("the hat grooves keep their syncopation band", "[gen][song][rhythm]")
{
    // Plan §4.1: a pattern's LHL syncopation stays inside a style band. Both styles band their hats'
    // main grid (the groove of every section but the build and break); every banded grid reaches its
    // band within the style's attempts, and the band is not decorative: some first attempts fall
    // outside it and are drawn again.
    for (const char* id : { "FUT01", "FUT03" })
    {
        const Bundle& b = bundleFor (id);
        std::size_t banded = 0;
        for (const LaneSpec& lane : b.style.lanes)
        {
            if (lane.kind != LaneKind::drums)
                continue;
            for (const DrumGrid& g : lane.drums.grids)
            {
                INFO (id << " " << lane.id << " " << g.name);
                if (lane.id == "hats" && g.name == "main")
                    CHECK (g.sync.has_value());
                if (!g.sync)
                    continue;
                ++banded;
                INFO ("band " << g.sync->lo << "-" << g.sync->hi);
                std::size_t missed = 0, redrawn = 0;
                for (std::uint64_t i = 0; i < 1024; ++i)
                {
                    const rng::Key k = rng::seedKey (i);
                    const Pattern p = drawPattern (g, k, b.style.attempts);
                    missed += (p.inBand && p.sync >= g.sync->lo && p.sync <= g.sync->hi) ? 0u : 1u;
                    redrawn += drawAttempt (g, k, 0).inBand ? 0u : 1u;
                }
                CHECK (missed == 0);
                CHECK (redrawn > 0);
            }
        }
        CHECK (banded >= 1);
    }
}
