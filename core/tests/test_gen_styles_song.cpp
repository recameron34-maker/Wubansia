// Song-level tests for the sci-fi club (FUT02) and techno (FUT04) styles: the invariants every style
// keeps (the S1b1 brief and plan §5.1, as gen-core's tests check them for FUT01 and FUT03), then the
// rules of each direction's brief.

#include "gen/arrange.h"
#include "gen/content.h"
#include "gen/export.h"
#include "gen/harmony.h"
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
#include <tuple>
#include <vector>

using namespace wubansia;
using namespace wubansia::gen;
using Catch::Matchers::ContainsSubstring;

#if defined(__has_feature)
    #if __has_feature(address_sanitizer) || __has_feature(undefined_behavior_sanitizer)
        #define WUBANSIA_STYLES_NO_TIMING 1
    #endif
#endif
#if defined(__SANITIZE_ADDRESS__)
    #define WUBANSIA_STYLES_NO_TIMING 1
#endif

namespace
{
    constexpr std::array<const char*, 2> kits = { "FUT02", "FUT04" };

    const Bundle& bundleFor (std::string_view id)
    {
        static std::map<std::string, Bundle, std::less<>> cache;
        auto it = cache.find (id);
        if (it == cache.end())
        {
            auto b = loadBundle (std::filesystem::path (WUBANSIA_SOURCE_DIR) / "content", id);
            INFO (b.error);
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
        INFO (s.error);
        REQUIRE (s.ok());
        return std::move (s.value);
    }

    Song songFor (std::string_view id, std::uint64_t seed) { return songOf (bundleFor (id), seed); }

    std::size_t laneIndexOf (const Song& s, std::string_view id)
    {
        for (std::size_t i = 0; i < s.lanes.size(); ++i)
            if (s.lanes[i].id == id)
                return i;
        FAIL ("no lane " << id);
        return 0;
    }

    const LaneOut& laneOf (const Song& s, std::string_view id) { return s.lanes[laneIndexOf (s, id)]; }

    const LaneSpec& specOf (const Bundle& b, std::string_view id) { return b.style.lanes[*b.style.laneIndex (id)]; }

    // A drum voice's index in its lane (the order the style lists the voices in).
    std::uint8_t voiceOf (const Bundle& b, std::string_view lane, std::string_view voice)
    {
        const auto& voices = specOf (b, lane).drums.voices;
        for (std::size_t i = 0; i < voices.size(); ++i)
            if (voices[i].id == voice)
                return static_cast<std::uint8_t> (i);
        FAIL ("no voice " << voice << " in " << lane);
        return 0;
    }

    std::vector<std::uint8_t> fullBytes (const Song& s, Version v)
    {
        const auto r = midi::writeSmf (fullSpec (s, v));
        REQUIRE (r.ok());
        return r.value;
    }

    midi::File fullFile (const Song& s, Version v)
    {
        const auto f = midi::readSmf (fullBytes (s, v));
        REQUIRE (f.ok());
        return f.value;
    }

    std::set<std::int64_t> stepsOf (const LaneOut& lane, int voice = -1)
    {
        std::set<std::int64_t> out;
        for (const Note& n : lane.score)
            if (voice < 0 || n.voice == voice)
                out.insert (n.step);
        return out;
    }

    bool isDrop (const std::string& kind) { return kind == "drop" || kind == "drop2"; }

    // Perturbs one lane's own parameters (gen-core's test, extended to the new fields): every
    // non-anchor probability p becomes 100 - p, velocities rise by 3, a CC lane's ramp rises by 5, and
    // the feel's 1/f spread doubles. Nothing else in the style changes.
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
                flip (g.slide);
                louder (g.vel);
            }
            lane.bass.approach = 100 - lane.bass.approach;
            for (auto& g : lane.chords.grids)
            {
                flip (g.p);
                louder (g.vel);
                g.padVel = std::min (127, g.padVel + 9);
            }
            for (auto& g : lane.arp.grids)
            {
                flip (g.p);
                louder (g.vel);
            }
            for (auto& cc : lane.cc)
                for (auto& row : cc.rows)
                {
                    row.from = Range { std::min (127, row.from.lo + 5), std::min (127, row.from.hi + 5) };
                    row.to = Range { std::min (127, row.to.lo + 5), std::min (127, row.to.hi + 5) };
                }
        }
        for (auto& [laneId, fl] : b.feel.lanes)
            if (laneId == id && fl.sd)
                fl.sd = Range { fl.sd->lo * 2 + 2, fl.sd->hi * 2 + 2 };
    }

    // The chord tones an arp grid may play over a chord: its ladder's pitch classes.
    std::set<int> arpTones (const ArpGrid& g, const ChordSpan& c)
    {
        std::set<int> out;
        for (const auto t : g.tones)
            out.insert (pitchClass (c.rootPc + chordInterval (c.mode, c.degree, t)));
        return out;
    }

    const ArpGrid& arpGridFor (const Bundle& b, std::string_view kind)
    {
        const LaneSpec& arp = specOf (b, "arp");
        const std::string_view name = arp.gridFor (kind);
        for (const auto& g : arp.arp.grids)
            if (g.name == name)
                return g;
        FAIL ("no arp grid for " << kind);
        return arp.arp.grids.front();
    }

    std::string describe (const Note& n)
    {
        return "tick " + std::to_string (n.tick) + " length " + std::to_string (n.length) + " key " + std::to_string (n.key) + " velocity "
               + std::to_string (n.velocity) + " step " + std::to_string (n.step) + " flags " + std::to_string (n.flags);
    }

    // The first difference between two note lists, or "" where they are the same.
    std::string firstDifference (const std::vector<Note>& got, const std::vector<Note>& want)
    {
        for (std::size_t i = 0; i < std::min (got.size(), want.size()); ++i)
            if (!(got[i] == want[i]))
                return "note " + std::to_string (i) + " is " + describe (got[i]) + ", the whole song's " + describe (want[i]);
        if (got.size() != want.size())
            return std::to_string (got.size()) + " notes, the whole song's head " + std::to_string (want.size());
        return {};
    }

    // A --bars cut's definition, written out on its own: how `cut` differs from the head of `whole`
    // at bar `bars` (nothing, if it is that head). The head holds the sections that start before the
    // cut, the last one shortened with its blocks; the turnarounds, salient changes and chord spans
    // before it (a span ended by it), and one harmony per section; and per lane every score note that
    // starts before the cut with its performed note (perf[i] plays score[i]), both ended by the cut
    // (a performed onset at or after it plays on its last tick, as at a section's end), and every
    // controller before it. `crossing` counts the score notes the cut ends.
    std::vector<std::string> headDifferences (const Song& whole, const Song& cut, std::int32_t bars, std::size_t& crossing)
    {
        std::vector<std::string> out;
        const std::int64_t end = static_cast<std::int64_t> (bars) * ticksPerBar;
        if (cut.totalBars != bars)
            out.push_back (std::to_string (cut.totalBars) + " bars");
        std::vector<Section> sections;
        for (const Section& s : whole.sections)
            if (s.startBar < bars)
            {
                Section h = s;
                h.bars = std::min (s.bars, bars - s.startBar);
                h.blocks.resize (static_cast<std::size_t> (h.bars / barsPerBlock));
                sections.push_back (h);
            }
        const auto sameSection = [&whole] (const Section& a, const Section& b) {
            bool same = a.index == b.index && a.kind == b.kind && a.startBar == b.startBar && a.bars == b.bars && a.blocks == b.blocks;
            for (std::size_t l = 0; l < whole.lanes.size() && same; ++l) // the bars each lane plays, dropouts included
                for (std::int32_t bar = 0; bar < a.bars && same; ++bar)
                    same = a.active (l, bar) == b.active (l, bar);
            return same;
        };
        if (cut.sections.size() != sections.size() || !std::equal (sections.begin(), sections.end(), cut.sections.begin(), sameSection))
            out.push_back (std::to_string (cut.sections.size()) + " sections, or not the whole song's (the last one shortened)");
        std::vector<std::int32_t> turns;
        for (const std::int32_t b : whole.turnBars)
            if (b < bars)
                turns.push_back (b);
        if (cut.turnBars != turns)
            out.push_back ("the turnaround bars");
        const auto changeTuple = [] (const Change& c) { return std::make_tuple (c.section, c.bar, c.lane); };
        std::vector<std::tuple<std::size_t, std::int32_t, std::size_t>> changes, gotChanges;
        for (const Change& c : whole.changes)
            if (c.section < sections.size() && c.bar < sections[c.section].bars)
                changes.push_back (changeTuple (c));
        for (const Change& c : cut.changes)
            gotChanges.push_back (changeTuple (c));
        if (gotChanges != changes)
            out.push_back ("the salient changes");
        const auto chordTuple = [] (const ChordSpan& c) { return std::make_tuple (c.start, c.end, c.degree, c.mode, c.rootPc, c.pushedBy, c.section, c.index); };
        std::vector<decltype (chordTuple (ChordSpan {}))> chords, gotChords;
        for (ChordSpan c : whole.chords)
            if (c.start < end)
            {
                c.end = std::min (c.end, end);
                chords.push_back (chordTuple (c));
            }
        for (const ChordSpan& c : cut.chords)
            gotChords.push_back (chordTuple (c));
        if (gotChords != chords)
            out.push_back ("the chord spans: " + std::to_string (gotChords.size()) + ", the whole song's head " + std::to_string (chords.size()));
        const auto harmonyTuple = [] (const SectionHarmony& h) { return std::make_tuple (h.slot, h.mode, h.degrees, h.units, h.loopBars, h.pushPermille, h.pushTicks, h.voicing); };
        if (cut.harmony.size() != sections.size())
            out.push_back (std::to_string (cut.harmony.size()) + " section harmonies");
        else
            for (std::size_t i = 0; i < sections.size(); ++i)
                if (harmonyTuple (cut.harmony[i]) != harmonyTuple (whole.harmony[i]))
                    out.push_back ("section " + std::to_string (i + 1) + "'s harmony");
        if (cut.lanes.size() != whole.lanes.size())
            out.push_back (std::to_string (cut.lanes.size()) + " lanes");
        for (std::size_t l = 0; l < std::min (cut.lanes.size(), whole.lanes.size()); ++l)
        {
            const LaneOut& w = whole.lanes[l];
            const LaneOut& c = cut.lanes[l];
            const std::string lane = "lane " + w.id + ": ";
            if (c.id != w.id || c.track != w.track || c.channel != w.channel || c.kind != w.kind || c.swing != w.swing)
                out.push_back (lane + "not the whole song's lane");
            std::vector<Note> score, perf;
            for (std::size_t i = 0; i < w.score.size(); ++i)
            {
                Note s = w.score[i];
                if (s.tick >= end)
                    continue;
                crossing += s.tick + s.length > end ? 1u : 0u;
                s.length = std::min (s.length, end - s.tick);
                Note p = w.perf[i];
                const std::int64_t pend = std::min (p.tick + p.length, end);
                p.tick = std::min (p.tick, end - 1);
                p.length = std::max<std::int64_t> (1, pend - p.tick);
                score.push_back (s);
                perf.push_back (p);
            }
            if (const auto d = firstDifference (c.score, score); !d.empty())
                out.push_back (lane + "score " + d);
            if (const auto d = firstDifference (c.perf, perf); !d.empty())
                out.push_back (lane + "performance " + d);
            std::vector<ControlPoint> cc;
            for (const ControlPoint& p : w.cc)
                if (p.tick < end)
                    cc.push_back (p);
            if (c.cc != cc)
                out.push_back (lane + std::to_string (c.cc.size()) + " controller points, the whole song's head " + std::to_string (cc.size()));
        }
        return out;
    }
}

// ------------------------------------------------------------------ invariants every style keeps

TEST_CASE ("styles: the same seed gives the same bytes and other seeds other songs", "[gen][styles][song]")
{
    for (const char* id : kits)
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

TEST_CASE ("styles: changing one lane's parameters leaves the other lanes' bytes alone", "[gen][styles][song]")
{
    // The bass and the rumble follow, lock to and duck under the kick, so they may change with it;
    // the bass's third, seventh and fifth agree with what the chords voice, so it may change with
    // them too; nothing else depends on another lane (the arp follows the harmony, not a lane).
    const std::map<std::string, std::set<std::string>> dependents = { { "kick", { "bass", "rumble" } }, { "chords", { "bass" } } };
    for (const char* id : kits)
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

TEST_CASE ("styles: an extended mix generates in under 50 ms per 128 bars", "[gen][styles][song][timing]")
{
    for (const char* id : kits)
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
#if defined(NDEBUG) && !defined(__SANITIZE_ADDRESS__) && !defined(WUBANSIA_STYLES_NO_TIMING)
        CHECK (per128 < 50.0);
        CHECK (bestCut < 50.0);
#endif
    }
}

TEST_CASE ("styles: sections start on 8-bar lines, tile the song and make a 200 to 240 bar mix", "[gen][styles][song]")
{
    for (const char* id : kits)
        for (std::uint64_t seed = 0; seed < 8; ++seed)
        {
            INFO (id << " seed " << seed);
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
            CHECK (s.totalBars >= 200); // each seed draws its form (arrange.h)
            CHECK (s.totalBars <= 240);
            CHECK (s.sections.front().kind == "intro");
            CHECK (s.sections.front().bars == 32);
            CHECK (s.sections.back().kind == "outro");
            CHECK (s.sections.back().bars == 32);
        }
}

TEST_CASE ("styles: lanes play only in their active blocks and the mix edges are drums only", "[gen][styles][song]")
{
    for (const char* id : kits)
        for (std::uint64_t seed = 0; seed < 8; ++seed)
        {
            INFO (id << " seed " << seed);
            const Song s = songFor (id, seed);
            for (std::size_t li = 0; li < s.lanes.size(); ++li)
            {
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
                if (s.lanes[li].kind == LaneKind::drums)
                    continue;
                CHECK_FALSE (s.sections.front().active (li, 0)); // the mix-in starts with drums only...
                CHECK_FALSE (s.sections.back().active (li, s.sections.back().bars - 1)); // ...and the mix-out ends with them
            }
        }
}

TEST_CASE ("styles: a lane active in a block sounds in it", "[gen][styles][song]")
{
    // regions.json lists the lanes of every 8-bar block, and the arrangement adds and takes lanes out
    // on those lines: a lane listed there must sound there (a held drone counts), in every seed, both
    // versions. A break's sparse lanes have an anchor to make sure of it.
    for (const char* id : kits)
    {
        std::size_t blocks = 0;
        for (std::uint64_t seed = 0; seed < 64; ++seed)
        {
            const Song s = songFor (id, seed);
            for (const Section& sec : s.sections)
                for (std::size_t k = 0; k < sec.blocks.size(); ++k)
                {
                    const std::int64_t from = (sec.startBar + static_cast<std::int32_t> (k) * barsPerBlock) * ticksPerBar;
                    const std::int64_t to = from + barsPerBlock * ticksPerBar;
                    for (std::size_t li = 0; li < s.lanes.size(); ++li)
                    {
                        if ((sec.blocks[k] & (std::uint64_t (1) << li)) == 0)
                            continue;
                        ++blocks;
                        for (const auto* notes : { &s.lanes[li].score, &s.lanes[li].perf })
                        {
                            const bool sounds = std::any_of (notes->begin(), notes->end(), [&] (const Note& n) { return n.tick < to && n.tick + n.length > from; });
                            INFO (id << " seed " << seed << ": " << s.lanes[li].id << " in block " << k << " of the " << sec.kind << " at bar " << sec.startBar);
                            CHECK (sounds);
                        }
                    }
                }
        }
        CHECK (blocks > 1000);
    }
}

TEST_CASE ("styles: kick and bass onsets that coincide are within 2 ticks in every file", "[gen][styles][song]")
{
    for (const char* id : kits)
    {
        std::size_t near = 0;
        for (std::uint64_t seed = 0; seed < 16; ++seed)
            for (const Version v : { Version::feel, Version::quantized })
            {
                INFO (id << " seed " << seed << " " << versionName (v));
                const Song s = songFor (id, seed);
                const midi::File f = fullFile (s, v);
                std::vector<std::int64_t> kicks;
                for (const auto& n : midi::notesOf (f.tracks[laneIndexOf (s, "kick") + 1]))
                    kicks.push_back (n.tick);
                for (const auto& lane : s.lanes)
                {
                    if (lane.kind != LaneKind::bass)
                        continue; // the bass, and the rumble
                    for (const auto& n : midi::notesOf (f.tracks[laneIndexOf (s, lane.id) + 1]))
                    {
                        const auto it = std::lower_bound (kicks.begin(), kicks.end(), n.tick - 30);
                        if (it != kicks.end() && *it <= n.tick + 30)
                        {
                            ++near;
                            INFO (lane.id << " at " << n.tick);
                            CHECK (std::abs (*it - n.tick) <= 2);
                        }
                    }
                }
            }
        INFO (id << ": " << near << " bass onsets within 30 ticks of a kick");
        if (std::string (id) == "FUT04")
            CHECK (near > 1000); // the rumble on every kick, the acid's squelch ticks (FUT02's bass rests on the kicks)
    }
}

TEST_CASE ("styles: drum notes stay inside 35-59 on channel 10", "[gen][styles][song]")
{
    for (const char* id : kits)
    {
        const Song s = songFor (id, 5);
        for (const auto& lane : s.lanes)
        {
            if (lane.kind != LaneKind::drums)
            {
                CHECK (lane.channel != 9);
                continue;
            }
            CHECK (lane.channel == 9);
            for (const Note& n : lane.score)
            {
                CHECK (n.key >= 35);
                CHECK (n.key <= 59);
            }
        }
    }
}

TEST_CASE ("styles: a bars cut gives exactly the whole song's first bars", "[gen][styles][song]")
{
    for (const char* id : kits)
    {
        INFO (id);
        const Song full = songFor (id, 21);
        const std::int32_t cutBars = full.sections[0].bars + full.sections[1].bars;
        const Song cut = songOf (bundleFor (id), 21, cutBars);
        REQUIRE (cut.totalBars == cutBars);
        for (std::size_t l = 0; l < full.lanes.size(); ++l)
        {
            INFO (full.lanes[l].id);
            for (const bool performed : { false, true })
            {
                std::vector<Note> head;
                for (const Note& n : performed ? full.lanes[l].perf : full.lanes[l].score)
                    if (n.tick < cutBars * ticksPerBar)
                        head.push_back (n);
                CHECK ((performed ? cut.lanes[l].perf : cut.lanes[l].score) == head);
            }
            std::vector<ControlPoint> ccHead;
            for (const ControlPoint& c : full.lanes[l].cc)
                if (c.tick < cutBars * ticksPerBar)
                    ccHead.push_back (c);
            CHECK (cut.lanes[l].cc == ccHead);
        }
        Options o;
        o.seed = 21;
        o.bars = 20;
        CHECK_THAT (generate (bundleFor (id), o).error, ContainsSubstring ("8"));
    }
}

TEST_CASE ("styles: a cut at every section line is exactly the head of the whole song", "[gen][styles][song]")
{
    // The cut's promise (cut.h) at every section line of all four kits, not one: every score and
    // performed note and every controller before the cut. A section line is where a cut ends no
    // note, so these notes are the whole song's as they are, lengths included.
    for (const char* id : { "FUT01", "FUT02", "FUT03", "FUT04" })
        for (std::uint64_t seed = 0; seed < 4; ++seed)
        {
            const Song full = songFor (id, seed);
            std::int32_t cut = 0;
            for (std::size_t si = 0; si + 1 < full.sections.size(); ++si)
            {
                cut += full.sections[si].bars;
                const Song head = songOf (bundleFor (id), seed, cut);
                const std::int64_t end = static_cast<std::int64_t> (cut) * ticksPerBar;
                for (std::size_t l = 0; l < full.lanes.size(); ++l)
                {
                    INFO (id << " seed " << seed << " cut at bar " << cut << ", lane " << full.lanes[l].id);
                    for (const bool performed : { false, true })
                    {
                        std::vector<Note> want;
                        for (const Note& n : performed ? full.lanes[l].perf : full.lanes[l].score)
                            if (n.tick < end)
                                want.push_back (n);
                        CHECK ((performed ? head.lanes[l].perf : head.lanes[l].score) == want);
                    }
                    std::vector<ControlPoint> cc;
                    for (const ControlPoint& c : full.lanes[l].cc)
                        if (c.tick < end)
                            cc.push_back (c);
                    CHECK (head.lanes[l].cc == cc);
                }
            }
        }
}

TEST_CASE ("styles: a cut at any 8-bar line is exactly the head of the whole song", "[gen][styles][song]")
{
    // A --bars cut is the whole song, generated and then cut (cut.h): at every 8-bar line of 16 seeds
    // of every kit, inside a section or on its line, it is the whole song's head (headDifferences):
    // every score and performed note and every controller before the cut, notes that cross it ended
    // at it, and the sections, chords, turnarounds and changes before it.
    std::size_t cuts = 0, inside = 0, crossing = 0, failed = 0;
    std::vector<std::string> examples;
    for (const char* id : { "FUT01", "FUT02", "FUT03", "FUT04" })
        for (std::uint64_t seed = 0; seed < 16; ++seed)
        {
            const Song whole = songFor (id, seed);
            for (std::int32_t bars = barsPerBlock; bars < whole.totalBars; bars += barsPerBlock)
            {
                const Song cut = songOf (bundleFor (id), seed, bars);
                ++cuts;
                inside += std::none_of (whole.sections.begin(), whole.sections.end(), [&] (const Section& s) { return s.startBar == bars; }) ? 1u : 0u;
                const auto differences = headDifferences (whole, cut, bars, crossing);
                if (differences.empty())
                    continue;
                ++failed;
                for (const auto& d : differences)
                    if (examples.size() < 12)
                        examples.push_back (std::string (id) + " seed " + std::to_string (seed) + " cut at bar " + std::to_string (bars) + ": " + d);
            }
        }
    std::string shown;
    for (const auto& e : examples)
        shown += "\n  " + e;
    INFO (failed << " of " << cuts << " cuts are not the whole song's head; the first differences:" << shown);
    CHECK (failed == 0);
    CHECK (cuts > 1500);
    CHECK (inside * 2 > cuts); // most 8-bar lines are inside a section
    CHECK (crossing > 1000); // pads, held bass and long stabs cross the lines, and the cut ends them
}

TEST_CASE ("styles: the bass, chords and arp change on phrase lines", "[gen][styles][song]")
{
    // G4's definition (tools/legacy_audit/README.md): a bar's fingerprint is its set of (16th, key)
    // onsets; inside a section, bar i >= 8 differs if it isn't bar i - 8's; a macro change is a
    // differing bar after one that didn't; it is on a phrase bar if i mod 8 is 0 or 7.
    for (const char* id : kits)
        for (const auto& spec : bundleFor (id).style.lanes)
        {
            if (spec.kind == LaneKind::drums || spec.id == "rumble")
                continue;
            std::size_t changes = 0, onPhrase = 0;
            for (std::uint64_t seed = 0; seed < 32; ++seed)
            {
                const Song s = songFor (id, seed);
                const LaneOut& lane = laneOf (s, spec.id);
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
            INFO (id << " " << spec.id << ": " << onPhrase << " of " << changes << " macro changes on phrase bars");
            CHECK (changes > 0);
            CHECK (onPhrase * 100 >= changes * 60);
        }
}

TEST_CASE ("styles: every section opens on a chord stack", "[gen][styles][song]")
{
    for (const char* id : kits)
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
                    continue;
                INFO (id << " seed " << seed << " " << sec.kind << " at bar " << sec.startBar);
                CHECK (count >= 2);
            }
        }
}

TEST_CASE ("styles: one seed gives unrelated songs in different kits", "[gen][styles][song]")
{
    std::size_t sameIntro = 0, sameCells = 0;
    for (std::uint64_t seed = 0; seed < 32; ++seed)
    {
        const Song a = songFor ("FUT02", seed);
        const Song b = songFor ("FUT04", seed);
        sameIntro += a.harmony.front().pushPermille == b.harmony.front().pushPermille ? 1u : 0u;
        // the hats' first drop bar, as steps: drawn from unrelated keys
        const auto firstDropHats = [] (const Song& s) {
            std::set<std::int64_t> out;
            for (const Section& sec : s.sections)
                if (sec.kind == "drop")
                {
                    for (const Note& n : laneOf (s, "hats").score)
                        if (n.step / stepsPerBar == sec.startBar)
                            out.insert (n.step % stepsPerBar * 8 + n.voice);
                    break;
                }
            return out;
        };
        sameCells += firstDropHats (a) == firstDropHats (b) ? 1u : 0u;
    }
    CHECK (sameIntro <= 2);
    CHECK (sameCells <= 2);
}

TEST_CASE ("styles: every pitched note is in key unless tagged borrowed or planed", "[gen][styles][song]")
{
    for (const char* id : kits)
        for (std::uint64_t seed = 0; seed < 32; ++seed)
        {
            INFO (id << " seed " << seed);
            const Song s = songFor (id, seed);
            for (const auto& lane : s.lanes)
            {
                if (lane.kind == LaneKind::drums)
                    continue;
                for (const auto* notes : { &lane.score, &lane.perf })
                    for (const Note& n : *notes)
                        if (!inMinorKey (s.tonic, pitchClass (n.key)))
                        {
                            INFO (lane.id << " key " << int (n.key) << " tick " << n.tick);
                            CHECK ((n.tags & (noteTag::borrowed | noteTag::planed)) != 0);
                        }
            }
        }
}

TEST_CASE ("styles: slides overlap the next note and approach notes lead into chord changes", "[gen][styles][song]")
{
    for (const char* id : kits)
    {
        std::size_t slides = 0, approaches = 0;
        for (std::uint64_t seed = 0; seed < 16; ++seed)
        {
            INFO (id << " seed " << seed);
            const Song s = songFor (id, seed);
            std::set<std::int64_t> changes;
            for (const auto& c : s.chords)
            {
                changes.insert (c.start);
                changes.insert (c.start + c.pushedBy);
            }
            const LaneOut& bass = laneOf (s, "bass");
            for (std::size_t i = 0; i < bass.score.size(); ++i)
            {
                const Note& n = bass.score[i];
                if (n.flags & noteFlag::slide)
                {
                    ++slides;
                    REQUIRE (i + 1 < bass.score.size());
                    const Note& next = bass.score[i + 1];
                    CHECK (n.tick + n.length > next.tick); // legato: it overlaps the next note
                    CHECK (n.key != next.key);
                    CHECK (std::abs (int (next.key) - int (n.key)) <= 12);
                }
                if (n.flags & noteFlag::approach)
                {
                    ++approaches;
                    REQUIRE (i + 1 < bass.score.size());
                    CHECK (changes.contains (bass.score[i + 1].tick));
                }
            }
        }
        INFO (id << ": " << slides << " slides, " << approaches << " approach notes");
        CHECK (slides > 20);
        if (std::string (id) == "FUT02")
            CHECK (approaches > 20); // the sci-fi bass leads into its chord changes
        else
            CHECK (approaches == 0); // a pedal has no changes to lead into
    }
}

// ------------------------------------------------------------------ sci-fi club (FUT02)

TEST_CASE ("sci-fi club: four on the floor, claps on 2 and 4, open hats on the offbeats", "[gen][styles][sciclub]")
{
    const Bundle& b = bundleFor ("FUT02");
    std::size_t dropBars = 0;
    for (std::uint64_t seed = 0; seed < 32; ++seed)
    {
        const Song s = songFor ("FUT02", seed);
        const std::size_t kickLane = laneIndexOf (s, "kick");
        const auto kicks = stepsOf (s.lanes[kickLane]);
        const auto claps = stepsOf (laneOf (s, "snare"), voiceOf (b, "snare", "clap"));
        const auto opens = stepsOf (laneOf (s, "hats"), voiceOf (b, "hats", "open"));
        for (const Section& sec : s.sections)
            for (std::int32_t bar = 0; bar < sec.bars; ++bar)
            {
                const std::int64_t s0 = (sec.startBar + bar) * stepsPerBar;
                INFO ("seed " << seed << " bar " << sec.startBar + bar << " " << sec.kind);
                if (sec.active (kickLane, bar))
                    for (const std::int64_t beat : { 0, 4, 8, 12 })
                        CHECK (kicks.contains (s0 + beat)); // four on the floor wherever the kick plays
                if (!isDrop (sec.kind))
                    continue;
                ++dropBars;
                if (sec.active (laneIndexOf (s, "snare"), bar)) // where the lane plays: it can sit out a block or a bar of a drop
                {
                    CHECK (claps.contains (s0 + 4));
                    CHECK (claps.contains (s0 + 12));
                }
                if (sec.active (laneIndexOf (s, "hats"), bar))
                    for (const std::int64_t off : { 2, 6, 10, 14 })
                        CHECK (opens.contains (s0 + off));
            }
    }
    CHECK (dropBars > 32 * 50);
}

TEST_CASE ("sci-fi club: the closed hats run 16ths around the open ones", "[gen][styles][sciclub]")
{
    const Bundle& b = bundleFor ("FUT02");
    std::size_t bars = 0, covered = 0;
    for (std::uint64_t seed = 0; seed < 32; ++seed)
    {
        const Song s = songFor ("FUT02", seed);
        const LaneOut& hats = laneOf (s, "hats");
        const auto closed = stepsOf (hats, voiceOf (b, "hats", "closed"));
        const auto open = stepsOf (hats, voiceOf (b, "hats", "open"));
        for (const auto st : open)
            CHECK_FALSE (closed.contains (st)); // an open hat takes its step
        for (const Section& sec : s.sections)
        {
            if (!isDrop (sec.kind))
                continue;
            for (std::int32_t bar = 0; bar < sec.bars; ++bar)
            {
                const std::int64_t s0 = (sec.startBar + bar) * stepsPerBar;
                ++bars;
                for (std::int64_t k = 0; k < 16; ++k)
                    covered += (closed.contains (s0 + k) || open.contains (s0 + k)) ? 1u : 0u;
            }
        }
    }
    INFO (covered << " hat steps in " << bars << " drop bars");
    CHECK (covered >= bars * 12); // 16ths: 12 or more of 16 steps a bar on average
}

TEST_CASE ("sci-fi club: laser perc accents punctuate off the beat", "[gen][styles][sciclub]")
{
    std::size_t bars = 0, hits = 0, offBeat = 0, windows = 0, empty = 0;
    for (std::uint64_t seed = 0; seed < 32; ++seed)
    {
        const Song s = songFor ("FUT02", seed);
        const std::size_t li = laneIndexOf (s, "perc");
        const auto perc = stepsOf (s.lanes[li]);
        for (const Section& sec : s.sections)
        {
            if (!isDrop (sec.kind))
                continue;
            for (std::int32_t bar = 0; bar < sec.bars; ++bar)
            {
                if (!sec.active (li, bar))
                    continue;
                const std::int64_t s0 = (sec.startBar + bar) * stepsPerBar;
                ++bars;
                std::size_t here = 0;
                for (std::int64_t k = 0; k < 16; ++k)
                    if (perc.contains (s0 + k))
                    {
                        ++here;
                        offBeat += k % 4 != 0 ? 1u : 0u;
                    }
                hits += here;
                if (bar % 2 == 1)
                {
                    ++windows;
                    std::size_t inWindow = here;
                    for (std::int64_t k = -16; k < 0; ++k)
                        inWindow += perc.contains (s0 + k) ? 1u : 0u;
                    empty += inWindow == 0 ? 1u : 0u;
                }
            }
        }
    }
    INFO (hits << " perc hits in " << bars << " drop bars, " << offBeat << " off the beat, " << empty << " of " << windows << " 2-bar windows empty");
    CHECK (hits >= bars); // at least one accent a bar on average...
    CHECK (hits <= bars * 5); // ...but accents, not a carpet
    CHECK (offBeat * 100 >= hits * 70);
    CHECK (empty * 100 <= windows * 20);
}

TEST_CASE ("sci-fi club: the bass rolls on the offbeats with octave jumps and rests on the kicks", "[gen][styles][sciclub]")
{
    std::size_t onsets = 0, onKick = 0, beats = 0, bars = 0, octaveBars = 0;
    std::int64_t velE = 0, velAnd = 0, velA = 0, nE = 0, nAnd = 0, nA = 0;
    for (std::uint64_t seed = 0; seed < 32; ++seed)
    {
        const Song s = songFor ("FUT02", seed);
        const std::size_t li = laneIndexOf (s, "bass");
        const LaneOut& bass = s.lanes[li];
        std::map<std::int64_t, std::vector<const Note*>> byBar;
        for (const Note& n : bass.score)
        {
            const Section& sec = s.sections[static_cast<std::size_t> (n.section)];
            if (!isDrop (sec.kind))
                continue;
            ++onsets;
            const auto pos = n.step % 4;
            onKick += pos == 0 ? 1u : 0u;
            if (pos == 1)
            {
                velE += n.velocity;
                ++nE;
            }
            if (pos == 2)
            {
                velAnd += n.velocity;
                ++nAnd;
            }
            if (pos == 3)
            {
                velA += n.velocity;
                ++nA;
            }
            byBar[n.step / 16].push_back (&n);
        }
        for (const Section& sec : s.sections)
            if (isDrop (sec.kind))
                for (std::int32_t bar = 0; bar < sec.bars; ++bar)
                    beats += sec.active (li, bar) ? 4u : 0u;
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
    INFO (onsets << " onsets, " << onKick << " on a kick, " << beats << " beats, octave bars " << octaveBars << " of " << bars);
    CHECK (onKick * 100 <= onsets * 5); // it rests on the kick (an approach note may land there)
    CHECK (onsets * 10 >= beats * 20); // rolling: two or more notes a beat
    CHECK (octaveBars * 100 >= bars * 60);
    REQUIRE (nE > 0);
    REQUIRE (nAnd > 0);
    REQUIRE (nA > 0);
    INFO ("mean velocity: e " << velE / nE << ", and " << velAnd / nAnd << ", a " << velA / nA);
    CHECK (velAnd * nE > velE * nAnd); // the offbeat 8th is the accent...
    CHECK (velE * nA > velA * nE); // ...and the a before the kick the ghost
}

TEST_CASE ("sci-fi club: the arp runs 16ths on the chord tones with 3-3-2 accents", "[gen][styles][sciclub]")
{
    const Bundle& b = bundleFor ("FUT02");
    const Range reg = b.kit.lane ("arp")->range;
    std::size_t bars = 0, onsets = 0;
    std::int64_t accentVel = 0, otherVel = 0, nAccent = 0, nOther = 0;
    for (std::uint64_t seed = 0; seed < 32; ++seed)
    {
        const Song s = songFor ("FUT02", seed);
        const std::size_t li = laneIndexOf (s, "arp");
        const LaneOut& arp = s.lanes[li];
        REQUIRE_FALSE (arp.score.empty());
        for (const Note& n : arp.score)
        {
            const Section& sec = s.sections[static_cast<std::size_t> (n.section)];
            INFO ("seed " << seed << " tick " << n.tick << " key " << int (n.key));
            CHECK (n.key >= reg.lo);
            CHECK (n.key <= reg.hi);
            CHECK (n.tick % 120 == 0);
            const ChordSpan* c = chordAt (s, n.tick);
            REQUIRE (c != nullptr);
            CHECK (arpTones (arpGridFor (b, sec.kind), *c).contains (pitchClass (n.key))); // a chord tone of the chord sounding
            if (!isDrop (sec.kind))
                continue;
            ++onsets;
            const auto pos = n.step % 8;
            if (pos == 0 || pos == 3 || pos == 6)
            {
                accentVel += n.velocity;
                ++nAccent;
            }
            else
            {
                otherVel += n.velocity;
                ++nOther;
            }
        }
        for (const Section& sec : s.sections)
            if (isDrop (sec.kind))
                for (std::int32_t bar = 0; bar < sec.bars; ++bar)
                    bars += sec.active (li, bar) ? 1u : 0u;
    }
    INFO (onsets << " arp notes in " << bars << " drop bars");
    CHECK (onsets >= bars * 12); // a 16th motor: 12 or more of 16 steps a bar
    REQUIRE (nAccent > 0);
    REQUIRE (nOther > 0);
    INFO ("accent mean " << accentVel / nAccent << ", others " << otherVel / nOther);
    CHECK (accentVel * nOther >= (otherVel + 10 * nOther) * nAccent); // the 3-3-2 accents stand 10 or more over the rest
}

// ------------------------------------------------------------------ techno (FUT04)

TEST_CASE ("techno: four on the floor, with only a turnaround pickup on the a of 4", "[gen][styles][techno]")
{
    std::size_t pickups = 0;
    for (std::uint64_t seed = 0; seed < 32; ++seed)
    {
        const Song s = songFor ("FUT04", seed);
        const std::size_t li = laneIndexOf (s, "kick");
        const auto kicks = stepsOf (s.lanes[li]);
        for (const Note& n : s.lanes[li].score)
        {
            const auto pos = n.step % 16;
            if (pos % 4 == 0)
                continue;
            INFO ("seed " << seed << " step " << n.step);
            CHECK (pos == 15);
            CHECK ((n.flags & noteFlag::turn) != 0);
            CHECK (isTurnBar (s, static_cast<std::int32_t> (n.step / 16)));
            ++pickups;
        }
        for (const Section& sec : s.sections)
            for (std::int32_t bar = 0; bar < sec.bars; ++bar)
            {
                if (!sec.active (li, bar))
                    continue;
                const std::int64_t s0 = (sec.startBar + bar) * stepsPerBar;
                for (const std::int64_t beat : { 0, 4, 8, 12 })
                    CHECK (kicks.contains (s0 + beat));
            }
    }
    CHECK (pickups > 10);
}

TEST_CASE ("techno: 16th hats with open hats or rides on the offbeats", "[gen][styles][techno]")
{
    const Bundle& b = bundleFor ("FUT04");
    std::size_t bars = 0, covered = 0, drop2Bars = 0, rideBars = 0;
    for (std::uint64_t seed = 0; seed < 32; ++seed)
    {
        const Song s = songFor ("FUT04", seed);
        const LaneOut& hats = laneOf (s, "hats");
        const auto all = stepsOf (hats);
        const auto open = stepsOf (hats, voiceOf (b, "hats", "open"));
        const auto ride = stepsOf (hats, voiceOf (b, "hats", "ride"));
        for (const Section& sec : s.sections)
        {
            if (!isDrop (sec.kind))
                continue;
            for (std::int32_t bar = 0; bar < sec.bars; ++bar)
            {
                if (!sec.active (laneIndexOf (s, "hats"), bar))
                    continue; // where the hats play: they can sit out a block of a drop
                const std::int64_t s0 = (sec.startBar + bar) * stepsPerBar;
                INFO ("seed " << seed << " bar " << sec.startBar + bar << " " << sec.kind);
                ++bars;
                for (std::int64_t k = 0; k < 16; ++k)
                    covered += all.contains (s0 + k) ? 1u : 0u;
                for (const std::int64_t off : { 2, 6, 10, 14 })
                    CHECK ((open.contains (s0 + off) || ride.contains (s0 + off)));
                if (sec.kind == "drop2")
                {
                    ++drop2Bars;
                    bool any = false;
                    for (std::int64_t k = 0; k < 16; ++k)
                        any = any || ride.contains (s0 + k);
                    rideBars += any ? 1u : 0u;
                }
            }
        }
    }
    INFO (covered << " hat steps in " << bars << " drop bars; rides in " << rideBars << " of " << drop2Bars << " drop 2 bars");
    CHECK (covered >= bars * 13);
    CHECK (rideBars * 100 >= drop2Bars * 90);
}

TEST_CASE ("techno: a sparse snare and clap in the groove, none in the first drop, the full backbeat in the second", "[gen][styles][techno]")
{
    // Machine-hard techno: in the groove the metal snare hits beat 4 of every second bar, a clap
    // sometimes doubles beat 4, and nothing else plays but a pickup in some turnaround bars; no
    // swung ghost snares (that is house and garage programming). The snare and clap are drop 2's
    // layer (the form grammar): the first drop runs on the kick, rumble, hats, anvil and acid alone,
    // and the second drop lifts to the full backbeat on 2 and 4.
    std::size_t grooveBars = 0, grooveSteps = 0, grooveTwos = 0, grooveFours = 0, dropNotes = 0, drop2Bars = 0, drop2Backbeats = 0, ghosts = 0;
    for (std::uint64_t seed = 0; seed < 32; ++seed)
    {
        const Song s = songFor ("FUT04", seed);
        const std::size_t li = laneIndexOf (s, "snare");
        std::map<std::int64_t, std::vector<const Note*>> byStep;
        for (const Note& n : s.lanes[li].score)
        {
            byStep[n.step].push_back (&n);
            const std::string& kind = s.sections[static_cast<std::size_t> (n.section)].kind;
            // outside turnaround pickups, nothing quieter than the backbeat: no ghosts
            if (!(n.flags & noteFlag::turn) && (isDrop (kind) || kind == "groove"))
                ghosts += n.velocity < 64 ? 1u : 0u;
            dropNotes += kind == "drop" ? 1u : 0u;
        }
        for (const Section& sec : s.sections)
            for (std::int32_t bar = 0; bar < sec.bars; ++bar)
            {
                if (!sec.active (li, bar))
                    continue; // a dropout, or a block the snare sits out
                const std::int64_t s0 = (sec.startBar + bar) * stepsPerBar;
                const auto snareAt = [&] (std::int64_t k) {
                    const auto it = byStep.find (s0 + k);
                    return it != byStep.end() && std::any_of (it->second.begin(), it->second.end(), [] (const Note* n) { return n->voice == 0; });
                };
                if (sec.kind == "groove")
                {
                    ++grooveBars;
                    grooveTwos += snareAt (4) ? 1u : 0u;
                    grooveFours += snareAt (12) ? 1u : 0u;
                    for (std::int64_t k = 0; k < 16; ++k)
                        grooveSteps += byStep.contains (s0 + k) ? 1u : 0u;
                    if (bar % 2 == 1)
                        CHECK (snareAt (12)); // beat 4 of every second bar
                }
                else if (sec.kind == "drop2")
                {
                    ++drop2Bars;
                    drop2Backbeats += (snareAt (4) && snareAt (12)) ? 1u : 0u;
                }
            }
    }
    INFO (grooveSteps << " snare-lane steps in " << grooveBars << " groove bars, the snare on beat 2 in " << grooveTwos << " and on beat 4 in " << grooveFours
                      << "; the backbeat in " << drop2Backbeats << " of " << drop2Bars << " drop 2 bars; " << ghosts << " ghosts; " << dropNotes << " notes in the first drops");
    REQUIRE (grooveBars > 500);
    CHECK (grooveTwos == 0);
    CHECK (grooveFours * 2 == grooveBars); // every second bar, no more
    CHECK (grooveSteps * 10 <= grooveBars * 12); // sparse: at most 1.2 hits a bar, pickups included
    CHECK (dropNotes == 0);
    REQUIRE (drop2Bars > 500);
    CHECK (drop2Backbeats == drop2Bars);
    CHECK (ghosts == 0);
}

TEST_CASE ("techno: the acid line rolls 16ths with accents, ghosts, slides and octave jumps", "[gen][styles][techno]")
{
    std::size_t onsets = 0, beats = 0, accents = 0, normals = 0, ghosts = 0, slides = 0, bars = 0, octaveBars = 0, onKick = 0;
    for (std::uint64_t seed = 0; seed < 32; ++seed)
    {
        const Song s = songFor ("FUT04", seed);
        const std::size_t li = laneIndexOf (s, "bass");
        const LaneOut& bass = s.lanes[li];
        std::map<std::int64_t, std::vector<const Note*>> byBar;
        for (const Note& n : bass.score)
        {
            const Section& sec = s.sections[static_cast<std::size_t> (n.section)];
            if (!isDrop (sec.kind))
                continue;
            ++onsets;
            accents += n.velocity >= 100 ? 1u : 0u;
            normals += (n.velocity >= 64 && n.velocity < 100) ? 1u : 0u;
            ghosts += n.velocity < 64 ? 1u : 0u;
            slides += (n.flags & noteFlag::slide) ? 1u : 0u;
            if (n.step % 4 == 0)
            {
                ++onKick;
                INFO ("seed " << seed << " tick " << n.tick);
                CHECK (n.length <= 90); // on a kick: a short squelch tick
            }
            byBar[n.step / 16].push_back (&n);
        }
        for (const Section& sec : s.sections)
            if (isDrop (sec.kind))
                for (std::int32_t bar = 0; bar < sec.bars; ++bar)
                    beats += sec.active (li, bar) ? 4u : 0u;
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
    INFO (onsets << " onsets in " << beats << " beats: " << accents << " accents, " << normals << " normal, " << ghosts << " ghosts, " << slides
                 << " slides, " << onKick << " on a kick; octave bars " << octaveBars << " of " << bars);
    CHECK (onsets * 10 >= beats * 20); // two or more 16ths a beat
    CHECK (accents * 10 >= onsets);
    CHECK (normals * 10 >= onsets);
    CHECK (ghosts * 10 >= onsets);
    CHECK (slides * 4 >= bars); // a slide every 4 bars or so
    CHECK (octaveBars * 100 >= bars * 50);
    CHECK (onKick * 100 <= onsets * 12); // it rests on the kick, but for squelch ticks
}

TEST_CASE ("styles: a Euclidean or polymeter hit keeps its velocity within a variant", "[gen][styles][song]")
{
    // Like a cell's fixed velocity, a cycle's accent shape repeats: the score velocity of a Euclid
    // hit is its cell's, of a polymeter hit its position's in the reset cycle, for the whole of a
    // pattern variant (the feel's 1/f velocity adds the life on top). FUT04's anvil 3/16 among them.
    for (const char* id : { "FUT01", "FUT02", "FUT03", "FUT04" })
    {
        const Bundle& b = bundleFor (id);
        std::size_t hits = 0, repeats = 0;
        for (std::uint64_t seed = 0; seed < 8; ++seed)
        {
            const Song s = songFor (id, seed);
            for (std::size_t li = 0; li < s.lanes.size(); ++li)
            {
                const LaneSpec& lane = b.style.lanes[li];
                if (lane.kind != LaneKind::drums)
                    continue;
                // (section, variant, voice, cell or cycle position) -> the velocity first seen there
                std::map<std::tuple<std::int16_t, std::int32_t, std::uint8_t, std::int64_t>, std::uint8_t> seen;
                for (const Note& n : s.lanes[li].score)
                {
                    const Section& sec = s.sections[static_cast<std::size_t> (n.section)];
                    const std::string_view gridName = lane.gridFor (sec.kind);
                    const auto g = std::find_if (lane.drums.grids.begin(), lane.drums.grids.end(), [&] (const DrumGrid& x) { return x.name == gridName; });
                    if (g == lane.drums.grids.end() || !g->stages.empty() || g->ramp)
                        continue;
                    const std::string& voice = lane.drums.voices[n.voice].id;
                    const auto v = std::find_if (g->voices.begin(), g->voices.end(), [&] (const DrumVoiceGrid& x) { return x.voice == voice; });
                    if (v == g->voices.end() || !(v->euclid || v->poly) || (n.flags & (noteFlag::euclid | noteFlag::poly)) == 0)
                        continue;
                    const auto bar = static_cast<std::int32_t> (n.tick / ticksPerBar - sec.startBar);
                    const std::int32_t variant = variantAt (s, b.style, li, sec.index, bar);
                    const std::int64_t at = v->poly ? n.step % (static_cast<std::int64_t> (v->poly->resetBars) * stepsPerBar) : (bar % 2) * 16 + n.step % 16;
                    ++hits;
                    const auto [it, fresh] = seen.emplace (std::make_tuple (n.section, variant, n.voice, at), n.velocity);
                    if (fresh)
                        continue;
                    ++repeats;
                    INFO (id << " seed " << seed << " " << lane.id << "/" << voice << " section " << sec.index << " variant " << variant << " at " << at);
                    CHECK (int (n.velocity) == int (it->second));
                }
            }
        }
        INFO (id << ": " << hits << " Euclid and polymeter hits, " << repeats << " of them a repeat of a place");
        CHECK (repeats > 500);
    }
}

TEST_CASE ("techno: the rumble follows every kick and never rings over one", "[gen][styles][techno]")
{
    const Bundle& b = bundleFor ("FUT04");
    const std::int32_t duck = specOf (b, "rumble").bass.duck;
    std::size_t rumbles = 0;
    for (std::uint64_t seed = 0; seed < 16; ++seed)
    {
        INFO ("seed " << seed);
        const Song s = songFor ("FUT04", seed);
        const std::size_t ri = laneIndexOf (s, "rumble");
        const LaneOut& kick = laneOf (s, "kick");
        const LaneOut& rumble = s.lanes[ri];
        std::map<std::int64_t, const Note*> structural; // step -> kick (not an ornament or a turnaround)
        for (const Note& k : kick.score)
            if (!(k.flags & (noteFlag::ornament | noteFlag::turn)))
                structural.emplace (k.step, &k);
        // one rumble note on every structural kick where the rumble plays, and nowhere else
        std::size_t expected = 0;
        for (const auto& [step, k] : structural)
        {
            const Section& sec = s.sections[static_cast<std::size_t> (k->section)];
            expected += sec.active (ri, static_cast<std::int32_t> (step / 16) - sec.startBar) ? 1u : 0u;
        }
        CHECK (rumble.score.size() == expected);
        for (const Note& n : rumble.score)
            CHECK (structural.contains (n.step));
        rumbles += rumble.score.size();
        // in both versions' files, read back: a rumble note starts on its kick exactly and is over
        // before the next kick starts, with the duck's gap, pickups included
        for (const Version v : { Version::feel, Version::quantized })
        {
            INFO (versionName (v));
            const midi::File f = fullFile (s, v);
            const auto kicks = midi::notesOf (f.tracks[laneIndexOf (s, "kick") + 1]);
            const auto notes = midi::notesOf (f.tracks[ri + 1]);
            REQUIRE (notes.size() == rumble.score.size());
            std::vector<std::int64_t> onsets;
            for (const auto& k : kicks)
                onsets.push_back (k.tick);
            for (const auto& n : notes)
            {
                INFO ("rumble at " << n.tick << " for " << n.length);
                CHECK (std::binary_search (onsets.begin(), onsets.end(), n.tick)); // locked to its kick
                const auto next = std::upper_bound (onsets.begin(), onsets.end(), n.tick);
                if (next != onsets.end())
                    CHECK (n.tick + n.length <= *next - duck);
            }
        }
    }
    CHECK (rumbles > 16 * 400);
}

TEST_CASE ("techno: the bass repeats its 2 bars and a change mutates a note or two", "[gen][styles][techno]")
{
    const Bundle& b = bundleFor ("FUT04");
    const Style& style = b.style;
    const LaneSpec& spec = specOf (b, "bass");
    std::size_t pairs = 0, equal = 0, changes = 0, audible = 0;
    for (std::uint64_t seed = 0; seed < 32; ++seed)
    {
        const Song s = songFor ("FUT04", seed);
        const std::size_t li = laneIndexOf (s, "bass");
        const LaneOut& bass = s.lanes[li];
        for (const Section& sec : s.sections)
        {
            if (!isDrop (sec.kind))
                continue;
            const std::string_view gridName = spec.gridFor (sec.kind);
            const BassGrid* grid = nullptr;
            for (const auto& g : spec.bass.grids)
                if (g.name == gridName)
                    grid = &g;
            REQUIRE (grid != nullptr);
            REQUIRE (grid->mutate > 0);
            // each bar's notes as (cell, key): the cell is the 16th in the 2-bar pattern
            std::vector<std::map<std::int64_t, int>> cells (static_cast<std::size_t> (sec.bars));
            for (const Note& n : bass.score)
            {
                const std::int64_t bar = n.step / 16 - sec.startBar;
                if (bar >= 0 && bar < sec.bars)
                    cells[static_cast<std::size_t> (bar)][(bar % 2) * 16 + n.step % 16] = n.key;
            }
            const auto ornamentBar = [&] (std::int32_t bar) { return grid->ornBars > 1 && (bar + 1) % grid->ornBars == 0; };
            const auto usable = [&] (std::int32_t bar) { return bar >= 0 && bar < sec.bars && sec.active (li, bar) && !ornamentBar (bar); };
            for (std::int32_t bar = 0; bar + 2 < sec.bars; ++bar)
            {
                if (!usable (bar) || !usable (bar + 2))
                    continue;
                if (variantAt (s, style, li, sec.index, bar) != variantAt (s, style, li, sec.index, bar + 2))
                    continue;
                ++pairs;
                equal += cells[static_cast<std::size_t> (bar)] == cells[static_cast<std::size_t> (bar + 2)] ? 1u : 0u;
            }
            for (std::int32_t c = 4; c + 1 < sec.bars; ++c)
            {
                if (variantAt (s, style, li, sec.index, c) == variantAt (s, style, li, sec.index, c - 1))
                    continue;
                if (!usable (c) || !usable (c + 1) || !usable (c - 4) || !usable (c - 3))
                    continue;
                ++changes;
                // the 2 bars before the change against the 2 after, cell by cell
                std::map<std::int64_t, int> before = cells[static_cast<std::size_t> (c - 4)], after = cells[static_cast<std::size_t> (c)];
                for (const auto& [k, v] : cells[static_cast<std::size_t> (c - 3)])
                    before[k] = v;
                for (const auto& [k, v] : cells[static_cast<std::size_t> (c + 1)])
                    after[k] = v;
                std::set<std::int64_t> keys;
                for (const auto& [k, v] : before)
                    keys.insert (k);
                for (const auto& [k, v] : after)
                    keys.insert (k);
                std::set<std::int64_t> steps; // the 16ths of the bar that changed, in either bar
                for (const auto k : keys)
                    if (before.contains (k) != after.contains (k) || (before.contains (k) && before[k] != after[k]))
                        steps.insert (k % 16);
                INFO ("seed " << seed << " " << sec.kind << " change at bar " << c);
                CHECK (steps.size() <= static_cast<std::size_t> (grid->mutate)); // a mutation moves that many steps of the line
                audible += steps.empty() ? 0u : 1u;
            }
        }
    }
    INFO (equal << " of " << pairs << " bar pairs repeat; " << audible << " of " << changes << " changes are heard");
    CHECK (pairs > 500);
    CHECK (equal == pairs); // a 2-bar loop, exactly, until a change
    CHECK (changes > 20);
    CHECK (audible * 100 >= changes * 50); // and a change is a change
}

TEST_CASE ("techno: CC74 arcs move the acid's filter instead of chord changes", "[gen][styles][techno]")
{
    std::size_t builds = 0, outros = 0, drops = 0;
    for (std::uint64_t seed = 0; seed < 16; ++seed)
    {
        INFO ("seed " << seed);
        const Song s = songFor ("FUT04", seed);
        const LaneOut& bass = laneOf (s, "bass");
        REQUIRE_FALSE (bass.cc.empty());
        for (const Section& sec : s.sections)
        {
            std::vector<const ControlPoint*> pts;
            for (const ControlPoint& c : bass.cc)
                if (static_cast<std::size_t> (c.section) == sec.index && c.number == 74)
                    pts.push_back (&c);
            if (pts.empty())
                continue;
            INFO (sec.kind << " at bar " << sec.startBar);
            if (sec.kind == "intro")
                CHECK (pts.front()->value <= 40); // the acid comes in filtered
            if (sec.kind == "build")
            {
                ++builds;
                CHECK (pts.back()->value >= pts.front()->value + 20); // and opens through the build
            }
            if (sec.kind == "outro")
            {
                ++outros;
                CHECK (pts.front()->value >= pts.back()->value + 15); // and closes as it mixes out
            }
            if (isDrop (sec.kind))
            {
                ++drops;
                std::set<int> values;
                for (const auto* p : pts)
                    values.insert (p->value);
                CHECK (values.size() >= 8); // the knob moves through the drop
            }
        }
    }
    CHECK (builds >= 16);
    CHECK (outros >= 16);
    CHECK (drops >= 32);
}
