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
#if defined(NDEBUG) && !defined(__SANITIZE_ADDRESS__) && !defined(WUBANSIA_NO_TIMING)
        CHECK (per128 < 50.0);
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
            // a DJ-friendly extended mix: 6 to 8 minutes, a 32-bar mix-in and mix-out
            const std::int32_t seconds = s.totalBars * 240 / s.bpm;
            CHECK (seconds >= 360);
            CHECK (seconds <= 480);
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
                    const std::int64_t blockEnd = (sec.startBar + (bar / barsPerBlock + 1) * barsPerBlock) * ticksPerBar;
                    if (!sec.active (li, bar + barsPerBlock) || bar / barsPerBlock + 1 >= static_cast<std::int32_t> (sec.blocks.size()))
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
    for (std::uint64_t seed = 0; seed < 64; ++seed)
    {
        const Song s = songFor ("FUT03", seed);
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
                for (const std::int64_t beat : { 0, 4, 8, 12 })
                    CHECK (kicks.contains (s0 + beat));
                CHECK (claps.contains (s0 + 4)); // the clap on 2 and 4
                CHECK (claps.contains (s0 + 12));
                for (const std::int64_t off : { 2, 6, 10, 14 })
                    CHECK (opens.contains (s0 + off)); // offbeat open hats
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
                if (std::string (id) == "FUT01")
                    CHECK (near > 50);
            }
}

TEST_CASE ("the 2-step sub follows the kick and holds across the skipped beat", "[gen][song][style]")
{
    std::size_t bars = 0, held = 0, kicksOnOne = 0, followed = 0;
    for (std::uint64_t seed = 0; seed < 32; ++seed)
    {
        const Song s = songFor ("FUT01", seed);
        const LaneOut& bass = laneOf (s, "bass");
        const auto kicks = stepsOf (laneOf (s, "kick"));
        const auto bassSteps = stepsOf (bass);
        for (const Section& sec : s.sections)
        {
            if (!isDrop (sec.kind))
                continue;
            for (std::int32_t b = 0; b < sec.bars; ++b)
            {
                const std::int64_t s0 = (sec.startBar + b) * stepsPerBar;
                ++bars;
                if (kicks.contains (s0))
                {
                    ++kicksOnOne;
                    followed += bassSteps.contains (s0) ? 1u : 0u;
                }
                const std::int64_t skipped = (s0 + 8) * ticksPerStep;
                for (const Note& n : bass.score)
                    if (n.tick < skipped && n.tick + n.length > skipped && n.tick >= s0 * ticksPerStep)
                    {
                        ++held;
                        break;
                    }
            }
        }
    }
    INFO ("held " << held << " of " << bars << ", followed " << followed << " of " << kicksOnOne);
    CHECK (followed * 100 >= kicksOnOne * 85);
    CHECK (held * 100 >= bars * 50);
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
    const Song cut = songOf (b, 9, 72); // the intro (32), the groove (32) and the first build (8)
    CHECK (cut.totalBars == 72);
    std::int32_t bars = 0;
    for (const auto& sec : cut.sections)
        bars += sec.bars;
    CHECK (bars == 72);
    const Song mid = songOf (b, 9, 48); // the groove cut after 16 of its 32 bars
    CHECK (mid.totalBars == 48);
    CHECK (mid.sections.back().bars == 16);
    CHECK (mid.sections.back().plannedBars == 32);

    Options o;
    o.seed = 9;
    o.bars = 20;
    CHECK_THAT (generate (b, o).error, ContainsSubstring ("8"));
    o.bars = full.totalBars + 8;
    CHECK_THAT (generate (b, o).error, ContainsSubstring (std::to_string (full.totalBars)));
    o.bars = 0;
    o.feelAmount = 2001;
    CHECK_FALSE (generate (b, o).ok());
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
            for (const Section& sec : s.sections)
            {
                if (!isDrop (sec.kind))
                    continue;
                for (std::int32_t b = 0; b < sec.bars; ++b)
                {
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
