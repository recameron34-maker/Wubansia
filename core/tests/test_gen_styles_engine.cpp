// Unit tests for the engine features the sci-fi club and techno styles added to core/gen: mutation
// variants (mutate.h), one-chord progressions (harmony.h), the arp's ladder (arp.h), CC arcs (cc.h),
// the kick duck (duck.h), the feel's tight rule (feel.h), held pads (hold.h), fresh voicings and
// alike pushes, each on its own and on whole songs.

#include "gen/arp.h"
#include "gen/arrange.h"
#include "gen/cc.h"
#include "gen/content.h"
#include "gen/feel.h"
#include "gen/harmony.h"
#include "gen/hold.h"
#include "gen/mutate.h"
#include "gen/song.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
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

namespace
{
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

    Song songFor (std::string_view id, std::uint64_t seed, std::int32_t amount = -1)
    {
        Options o;
        o.seed = seed;
        o.feelAmount = amount;
        auto s = generate (bundleFor (id), o);
        INFO (s.error);
        REQUIRE (s.ok());
        return std::move (s.value);
    }

    const LaneOut& laneOf (const Song& s, std::string_view id)
    {
        for (const auto& l : s.lanes)
            if (l.id == id)
                return l;
        FAIL ("no lane " << id);
        return s.lanes.front();
    }

    const LaneSpec& specOf (const Bundle& b, std::string_view id) { return b.style.lanes[*b.style.laneIndex (id)]; }

    BassGrid gridWith (std::int32_t pct)
    {
        BassGrid g;
        g.name = "test";
        for (std::size_t s = 0; s < gridSteps; ++s)
        {
            g.p[s] = s % 4 == 0 ? 0 : pct; // the kick steps rest
            g.vel[s] = 90;
        }
        return g;
    }

    BassLane laneWith (std::vector<DegreeWeights> sets)
    {
        BassLane l;
        l.degreeSets = std::move (sets);
        return l;
    }

    std::size_t differing (const std::array<std::int32_t, gridSteps>& a, const std::array<std::int32_t, gridSteps>& b)
    {
        std::size_t n = 0;
        for (std::size_t s = 0; s < gridSteps; ++s)
            n += a[s] != b[s] ? 1u : 0u;
        return n;
    }

    // The section holding a tick.
    const Section& sectionAt (const Song& s, std::int64_t tick)
    {
        for (const Section& sec : s.sections)
            if (tick >= sec.startTick() && tick < sec.endTick())
                return sec;
        FAIL ("no section at " << tick);
        return s.sections.front();
    }
}

// ------------------------------------------------------------------ mutations

TEST_CASE ("styles: without mutate every cell draws from the variant itself", "[gen][styles][mutate]")
{
    const BassGrid g = gridWith (60);
    const BassLane lane = laneWith ({ { 100, 0, 0, 0, 0 } });
    const rng::Key k = rng::seedKey (5);
    for (std::int32_t v = 0; v < 6; ++v)
    {
        const auto cells = cellVariants (g, lane, k, v);
        for (const auto c : cells)
            CHECK (c == v);
    }
}

TEST_CASE ("styles: variableCells lists the cells a new draw can change", "[gen][styles][mutate]")
{
    BassGrid g;
    g.p[0] = 100; // sure, one degree: nothing to draw
    g.deg[0] = 0;
    g.p[1] = 100; // sure, but two degrees to choose from
    g.deg[1] = 1;
    g.p[2] = 50; // maybe
    g.follow[3] = 30; // a kick it may follow
    g.p[4] = 100; // sure and one degree, but a slide to draw
    g.hasSlide = true;
    g.slide[4] = 40;
    g.p[5] = 100; // a sure slide is nothing to draw either
    g.slide[5] = 100;
    // 6: silent
    const BassLane lane = laneWith ({ { 100, 0, 0, 0, 0 }, { 50, 50, 0, 0, 0 } });
    CHECK (variableCells (g, lane) == std::vector<std::size_t> { 1, 2, 3, 4 });
    g.hasSlide = false; // without the grid's slides, slides don't count
    CHECK (variableCells (g, lane) == std::vector<std::size_t> { 1, 2, 3 });
}

TEST_CASE ("styles: with mutate a change draws only that many steps again, in both bars", "[gen][styles][mutate]")
{
    BassGrid g = gridWith (60);
    g.mutate = 2;
    g.p[9] = 100; // sure in bar 1 with one degree: step 9 is variable in bar 2 only, so it never mutates
    const BassLane lane = laneWith ({ { 100, 0, 0, 0, 0 } });
    CHECK (variableCells (g, lane).size() == 23); // 24 non-kick cells, less the sure one
    const auto steps = variableSteps (g, lane);
    CHECK (steps == std::vector<std::size_t> { 1, 2, 3, 5, 6, 7, 10, 11, 13, 14, 15 }); // variable in both bars
    const rng::Key k = rng::seedKey (9).with (rng::tag ("bass")).with (rng::tag ("test"));
    auto prev = cellVariants (g, lane, k, 0);
    for (const auto c : prev)
        CHECK (c == 0); // variant 0 is drawn whole
    std::set<std::size_t> touched;
    for (std::int32_t v = 1; v <= 12; ++v)
    {
        INFO ("variant " << v);
        const auto cur = cellVariants (g, lane, k, v);
        CHECK (differing (prev, cur) == 4); // each change redraws exactly 2 steps, each in both bars...
        for (std::size_t s = 0; s < gridSteps; ++s)
            if (cur[s] != prev[s])
            {
                CHECK (cur[s] == v); // ...which take the new variant's draws
                CHECK (cur[s % barSteps] == cur[s % barSteps + barSteps]); // a step at a time
                CHECK (std::find (steps.begin(), steps.end(), s % barSteps) != steps.end()); // and only variable steps
                touched.insert (s % barSteps);
            }
        CHECK (cur[9] == 0);
        CHECK (cur[25] == 0);
        CHECK (cur[8] == 0); // a kick step with no follow: silent, never drawn
        CHECK (cur == cellVariants (g, lane, k, v)); // a pure function of its key
        prev = cur;
    }
    CHECK (touched.size() >= 8); // the changes wander over the line
    // another key picks other steps
    const rng::Key other = rng::seedKey (10).with (rng::tag ("bass")).with (rng::tag ("test"));
    std::size_t same = 0;
    for (std::int32_t v = 1; v <= 12; ++v)
        same += cellVariants (g, lane, k, v) == cellVariants (g, lane, other, v) ? 1u : 0u;
    CHECK (same < 3);
    // a grid with fewer variable steps than mutate redraws all of them
    BassGrid tiny;
    tiny.p[3] = 50;
    tiny.p[19] = 50;
    tiny.vel[3] = 90;
    tiny.vel[19] = 90;
    tiny.mutate = 4;
    const auto t1 = cellVariants (tiny, lane, k, 1);
    CHECK (t1[3] == 1);
    CHECK (t1[19] == 1);
    CHECK (std::count (t1.begin(), t1.end(), 1) == 2);
}

TEST_CASE ("styles: drawCell gives a mutating grid one bar's draws", "[gen][styles][mutate]")
{
    BassGrid g;
    CHECK (drawCell (g, 5) == 5);
    CHECK (drawCell (g, 21) == 21); // without mutate: every cell its own draws, as before
    g.mutate = 1;
    CHECK (drawCell (g, 5) == 5);
    CHECK (drawCell (g, 21) == 5); // the second bar takes the first bar's draws at its 16th
}

// ------------------------------------------------------------------ pedal harmony

TEST_CASE ("styles: one-chord progressions make a pedal on the tonic", "[gen][styles][harmony]")
{
    const Style& style = bundleFor ("FUT04").style;
    const HarmonySpec& spec = style.harmony;
    REQUIRE_FALSE (spec.slots.empty());
    const ProgressionSlot& main = spec.slots.front();
    const auto cands = progressionCandidates (spec, main, Mode::aeolian, 1, {});
    REQUIRE (cands.size() == 7); // every degree, one chord each
    for (const auto& c : cands)
    {
        REQUIRE (c.degrees.size() == 1);
        INFO ("degree " << c.degrees.front());
        CHECK ((c.weight > 0) == (c.degrees.front() == 0)); // "present": the tonic
    }
    for (std::uint64_t seed = 0; seed < 64; ++seed)
    {
        const HarmonyPlan plan = planHarmony (style, rng::seedKey (seed).with (rng::tag ("FUT04")));
        for (const auto& [name, p] : plan.slots)
        {
            INFO ("seed " << seed << " slot " << name);
            CHECK (p.degrees == std::vector<std::int32_t> { 0 });
            CHECK (p.units == std::vector<std::int32_t> { 4 });
            CHECK (p.mode == Mode::aeolian);
        }
    }
    // the two-chord candidates of the other styles are unchanged: never a repeated neighbour
    const HarmonySpec& fh = bundleFor ("FUT03").style.harmony;
    for (const auto& c : progressionCandidates (fh, fh.slots.front(), Mode::aeolian, 2, {}))
        CHECK (c.degrees[0] != c.degrees[1]);
}

TEST_CASE ("styles: a pedal lays out one span per loop and pushes none", "[gen][styles][harmony]")
{
    for (std::uint64_t seed = 1; seed <= 16; ++seed)
    {
        const Song s = songFor ("FUT04", seed);
        INFO ("seed " << seed);
        for (const Section& sec : s.sections)
        {
            std::vector<const ChordSpan*> spans;
            for (const auto& c : s.chords)
                if (static_cast<std::size_t> (c.section) == sec.index)
                    spans.push_back (&c);
            REQUIRE_FALSE (spans.empty());
            const std::int32_t loop = s.harmony[sec.index].loopBars;
            CHECK (spans.size() == static_cast<std::size_t> ((sec.bars + loop - 1) / loop));
            for (const ChordSpan* c : spans)
            {
                CHECK (c->degree == 0);
                CHECK (c->rootPc == 4); // E
                CHECK (c->pushedBy == 0);
                CHECK ((c->start - sec.startTick()) % (loop * ticksPerBar) == 0);
            }
        }
    }
}

// ------------------------------------------------------------------ the arp's ladder

TEST_CASE ("styles: placedRoot puts a chord root nearest the center, the lower on a tie", "[gen][styles][arp]")
{
    CHECK (placedRoot (2, 69) == 74); // D: D5 is 5 above A4, D4 7 below
    CHECK (placedRoot (9, 69) == 69);
    CHECK (placedRoot (10, 69) == 70); // B flat
    CHECK (placedRoot (4, 69) == 64); // E4 5 below, E5 7 above
    CHECK (placedRoot (0, 66) == 60); // C: 6 either way, the lower
    CHECK (placedRoot (3, 69) == 63); // E flat: 6 either way
    for (std::int32_t pc = 0; pc < 12; ++pc)
        for (const std::int32_t center : { 60, 66, 69, 71 })
        {
            const std::int32_t k = placedRoot (pc, center);
            CHECK (pitchClass (k) == pc);
            CHECK (k - center >= -6);
            CHECK (k - center < 6);
        }
}

TEST_CASE ("styles: ladderKey climbs the chord's tones in its mode and stays in range", "[gen][styles][arp]")
{
    ArpGrid g;
    g.tones = { 0, 2, 4 }; // a triad
    const Range range { 62, 86 };
    ChordSpan i;
    i.degree = 0;
    i.mode = Mode::aeolian;
    i.rootPc = 2; // D minor
    CHECK (ladderKey (g, i, 69, 0, range) == 74); // D5
    CHECK (ladderKey (g, i, 69, 1, range) == 77); // F5, the minor third
    CHECK (ladderKey (g, i, 69, 2, range) == 81); // A5
    CHECK (ladderKey (g, i, 69, 3, range) == 86); // D6, the octave
    CHECK (ladderKey (g, i, 69, 4, range) == 77); // F6 is out of range: an octave down
    ChordSpan vi;
    vi.degree = 5;
    vi.mode = Mode::aeolian;
    vi.rootPc = 10; // B flat major in D minor
    CHECK (ladderKey (g, vi, 69, 0, range) == 70);
    CHECK (ladderKey (g, vi, 69, 1, range) == 74); // the major third
    CHECK (ladderKey (g, vi, 69, 2, range) == 77);
    ChordSpan iv;
    iv.degree = 3;
    iv.mode = Mode::dorian;
    iv.rootPc = 7; // G major in D Dorian: its third is B natural
    CHECK (ladderKey (g, iv, 69, 1, range) == 71);
    g.octave = -12;
    CHECK (ladderKey (g, i, 69, 0, range) == 62); // D4
    CHECK (ladderKey (g, i, 69, 0, Range { 64, 86 }) == 74); // D4 is under this range: back up
    g.octave = 0;
    g.tones = { 0, 1, 2, 4 }; // with the second: a ninth in the upper octave
    CHECK (ladderKey (g, i, 62, 4, Range { 48, 96 }) == 74);
    CHECK (ladderKey (g, i, 62, 5, Range { 48, 96 }) == 76); // E5, the ninth
}

TEST_CASE ("styles: ladderKey folds any ladder index the loader allows into the range", "[gen][styles][arp]")
{
    // The loader allows a cell index of 15 plus a shift of 15 and an octave of +-24. On a one-tone
    // ladder index 30 is 30 octaves up: the fold must bring any distance into the range, with the
    // pitch class kept (a uint8_t cast of a key above 255 would wrap to a wrong note instead).
    ArpGrid g;
    g.tones = { 0 };
    ChordSpan i;
    i.degree = 0;
    i.mode = Mode::aeolian;
    i.rootPc = 2; // D minor
    for (const std::int32_t octave : { -24, 0, 24 })
        for (const std::int32_t index : { 0, 1, 15, 29, 30 })
            for (const Range range : { Range { 62, 86 }, Range { 100, 127 }, Range { 0, 12 } })
                for (const std::int32_t center : { range.lo, range.hi })
                {
                    g.octave = octave;
                    INFO ("octave " << octave << " index " << index << " range " << range.lo << "-" << range.hi << " center " << center);
                    const std::int32_t k = ladderKey (g, i, center, index, range);
                    CHECK (k >= range.lo);
                    CHECK (k <= range.hi);
                    CHECK (pitchClass (k) == 2);
                }
}

// ------------------------------------------------------------------ CC arcs

TEST_CASE ("styles: arcValue ramps from its start to its end and adds arches", "[gen][styles][cc]")
{
    const std::int64_t bar = ticksPerBar;
    CHECK (arcValue (20, 100, 16 * bar, 0, 0, 0) == 20);
    CHECK (arcValue (20, 100, 16 * bar, 0, 0, 16 * bar) == 100);
    CHECK (arcValue (20, 100, 16 * bar, 0, 0, 8 * bar) == 60);
    CHECK (arcValue (0, 1, 2, 0, 0, 1) == 1); // a half rounds up
    CHECK (arcValue (1, 0, 2, 0, 0, 1) == 1);
    std::int32_t prev = -1;
    for (std::int64_t t = 0; t <= 16 * bar; t += 120)
    {
        const std::int32_t v = arcValue (20, 100, 16 * bar, 0, 0, t);
        CHECK (v >= prev); // a rising ramp never falls
        prev = v;
    }
    // an arch every 8 bars: nothing at its ends, its depth at its middle
    CHECK (arcValue (60, 60, 32 * bar, 20, 8 * bar, 0) == 60);
    CHECK (arcValue (60, 60, 32 * bar, 20, 8 * bar, 4 * bar) == 80);
    CHECK (arcValue (60, 60, 32 * bar, 20, 8 * bar, 8 * bar) == 60);
    CHECK (arcValue (60, 60, 32 * bar, 20, 8 * bar, 12 * bar) == 80);
    CHECK (arcValue (60, 60, 32 * bar, 20, 8 * bar, 2 * bar) == 75); // 20 * 4 * 2 * 6 / 64
    CHECK (arcValue (60, 60, 32 * bar, -20, 8 * bar, 4 * bar) == 40); // a dip
    CHECK (arcValue (120, 120, 32 * bar, 20, 8 * bar, 4 * bar) == 127); // clamped
    CHECK (arcValue (5, 5, 32 * bar, -20, 8 * bar, 4 * bar) == 0);
}

TEST_CASE ("styles: CC points sit in the lane's active ranges, one per tick, opening every range", "[gen][styles][cc]")
{
    for (const char* id : { "FUT02", "FUT04" })
        for (std::uint64_t seed = 1; seed <= 8; ++seed)
        {
            INFO (id << " seed " << seed);
            const Bundle& b = bundleFor (id);
            const Song s = songFor (id, seed);
            std::size_t lanesWithCc = 0;
            for (std::size_t li = 0; li < s.lanes.size(); ++li)
            {
                const LaneOut& lane = s.lanes[li];
                const LaneSpec& spec = b.style.lanes[li];
                INFO (lane.id);
                if (spec.cc.empty())
                {
                    CHECK (lane.cc.empty());
                    continue;
                }
                ++lanesWithCc;
                REQUIRE_FALSE (lane.cc.empty());
                const auto ranges = activeRanges (s, li);
                for (std::size_t i = 0; i < lane.cc.size(); ++i)
                {
                    const ControlPoint& c = lane.cc[i];
                    if (i > 0)
                        CHECK (std::make_pair (lane.cc[i - 1].tick, lane.cc[i - 1].number) < std::make_pair (c.tick, c.number));
                    CHECK (c.value <= 127);
                    const bool inside = std::any_of (ranges.begin(), ranges.end(), [&] (const TickRange& r) { return c.tick >= r.start && c.tick < r.end; });
                    CHECK (inside);
                    const Section& sec = sectionAt (s, c.tick);
                    CHECK (static_cast<std::size_t> (c.section) == sec.index);
                    // inside the row's envelope: the ramp's ends plus the arch
                    const CcSpec* cs = nullptr;
                    for (const auto& x : spec.cc)
                        if (x.number == c.number)
                            cs = &x;
                    REQUIRE (cs != nullptr);
                    const CcRow* row = cs->row (sec.kind);
                    REQUIRE (row != nullptr);
                    const std::int32_t lo = std::max (0, std::min (row->from.lo, row->to.lo) + std::min (0, row->depth.lo));
                    const std::int32_t hi = std::min (127, std::max (row->from.hi, row->to.hi) + std::max (0, row->depth.hi));
                    CHECK (c.value >= lo);
                    CHECK (c.value <= hi);
                }
                for (const auto& cs : spec.cc)
                    for (const TickRange& r : ranges)
                    {
                        const bool opened = std::any_of (lane.cc.begin(), lane.cc.end(), [&] (const ControlPoint& c) { return c.tick == r.start && c.number == cs.number; });
                        CHECK (opened); // a region file opens on the value in force
                    }
            }
            CHECK (lanesWithCc >= 1);
        }
    // a lane without "cc" writes none: FUT01's and FUT03's only controllers are their chords' filter
    for (const char* id : { "FUT01", "FUT03" })
        for (const auto& lane : songFor (id, 3).lanes)
        {
            INFO (id << " " << lane.id);
            CHECK (lane.cc.empty() == (lane.id != "chords"));
            for (const ControlPoint& c : lane.cc)
                CHECK (c.number == 74);
        }
}

// ------------------------------------------------------------------ the duck

TEST_CASE ("styles: the duck leaves its gap before every kick of the section and before its end", "[gen][styles][duck]")
{
    struct Case
    {
        const char* kit;
        const char* lane;
    };
    for (const Case c : { Case { "FUT02", "bass" }, Case { "FUT04", "bass" }, Case { "FUT04", "rumble" } })
    {
        const Bundle& b = bundleFor (c.kit);
        const std::int32_t duck = specOf (b, c.lane).bass.duck;
        REQUIRE (duck > 0);
        std::size_t notes = 0, tight = 0;
        for (std::uint64_t seed = 1; seed <= 16; ++seed)
        {
            INFO (c.kit << " " << c.lane << " seed " << seed);
            const Song s = songFor (c.kit, seed);
            const LaneOut& kick = laneOf (s, "kick");
            const LaneOut& lane = laneOf (s, c.lane);
            for (const bool performed : { false, true })
            {
                const auto& kicks = performed ? kick.perf : kick.score;
                const auto& mine = performed ? lane.perf : lane.score;
                std::vector<std::pair<std::int16_t, std::int64_t>> onsets; // (section, tick), sorted
                for (const Note& k : kicks)
                    onsets.emplace_back (k.section, k.tick);
                std::sort (onsets.begin(), onsets.end());
                for (const Note& n : mine)
                {
                    INFO ((performed ? "perf" : "score") << " tick " << n.tick << " length " << n.length);
                    const Section& sec = s.sections[static_cast<std::size_t> (n.section)];
                    std::int64_t limit = sec.endTick() - duck;
                    const auto it = std::upper_bound (onsets.begin(), onsets.end(), std::make_pair (n.section, n.tick));
                    if (it != onsets.end() && it->first == n.section)
                        limit = std::min (limit, it->second - duck);
                    CHECK (n.tick + n.length <= limit);
                    CHECK (n.length >= 30); // still a note, not a click
                    ++notes;
                    tight += n.tick + n.length == limit ? 1u : 0u;
                }
            }
        }
        INFO (c.kit << " " << c.lane);
        CHECK (notes > 1000);
        CHECK (tight > 0); // the duck is what ends some notes
    }
}

// ------------------------------------------------------------------ tight

TEST_CASE ("styles: tight notes play as written and only the ghosts take the feel", "[gen][styles][feel]")
{
    std::size_t tightNotes = 0, locked = 0, ghosts = 0, movedGhosts = 0;
    for (std::uint64_t seed = 1; seed <= 16; ++seed)
    {
        INFO ("seed " << seed);
        const Bundle& b = bundleFor ("FUT04");
        const Song s = songFor ("FUT04", seed);
        std::map<std::int64_t, std::int64_t> kickAt; // step -> performed tick
        for (const Note& k : laneOf (s, "kick").perf)
            kickAt.emplace (k.step, k.tick);
        for (const char* id : { "bass", "rumble" })
        {
            const LaneOut& lane = laneOf (s, id);
            REQUIRE (lane.score.size() == lane.perf.size());
            for (std::size_t i = 0; i < lane.score.size(); ++i)
            {
                const Note& sc = lane.score[i];
                const Note& pf = lane.perf[i];
                const Section& sec = s.sections[static_cast<std::size_t> (sc.section)];
                const FeelLane f = b.feel.resolve (id, sec.kind);
                REQUIRE (f.tight.has_value());
                INFO (id << " tick " << sc.tick << " velocity " << int (sc.velocity));
                const auto kick = kickAt.find (sc.step);
                if (kick != kickAt.end() && f.lock)
                {
                    CHECK (pf.tick == kick->second); // the lock comes first
                    ++locked;
                }
                if (sc.velocity >= *f.tight)
                {
                    ++tightNotes;
                    if (kick == kickAt.end() || !f.lock)
                        CHECK (pf.tick == sc.tick); // on the grid
                    CHECK (pf.velocity == sc.velocity);
                    if (sc.flags & noteFlag::slide)
                    {
                        REQUIRE (i + 1 < lane.perf.size());
                        CHECK (pf.tick + pf.length > lane.perf[i + 1].tick); // a slide keeps its legato into the next note, which may be a swung ghost
                    }
                    else
                        CHECK (pf.length <= sc.length); // as written, or shortened to keep the line clear
                }
                else
                {
                    // a ghost stays a ghost: its velocity feel keeps it below the tight velocity, so the
                    // file shows the same split as the score (everything at or above it on the grid)
                    CHECK (pf.velocity < *f.tight);
                    if (kick == kickAt.end())
                    {
                        ++ghosts;
                        movedGhosts += (pf.tick != sc.tick || pf.velocity != sc.velocity) ? 1u : 0u;
                    }
                }
            }
        }
        // every rumble note is on a kick, so the rumble is the kick's exactly
        for (const Note& n : laneOf (s, "rumble").perf)
        {
            const auto kick = kickAt.find (n.step);
            REQUIRE (kick != kickAt.end());
            CHECK (n.tick == kick->second);
        }
    }
    INFO ("tight " << tightNotes << ", locked " << locked << ", ghosts " << ghosts << " (moved " << movedGhosts << ")");
    CHECK (tightNotes > 5000);
    CHECK (locked > 1000);
    CHECK (ghosts > 500);
    CHECK (movedGhosts * 100 >= ghosts * 70); // the ghosts do take the feel
}

// ------------------------------------------------------------------ held pads

TEST_CASE ("styles: holdPads holds a key struck again straight after itself, inside one section and range", "[gen][styles][hold]")
{
    const Bundle& b = bundleFor ("FUT04"); // its pad grid holds with a 60-tick gap; its drop plays stabs
    const std::size_t li = *b.style.laneIndex ("chords");
    REQUIRE (b.style.lanes[li].gridFor ("groove") == "pad");
    REQUIRE (b.style.lanes[li].gridFor ("drop") == "stab");
    const std::int64_t bar = ticksPerBar, gap = 60;
    const std::uint64_t on = std::uint64_t (1) << li;
    Song s;
    s.lanes.resize (b.style.lanes.size());
    s.lanes[li].kind = LaneKind::chords;
    // a held section with two active ranges (bars 0-15 and 24-31), a stab section, a held section again
    s.sections.push_back ({ 0, "groove", 0, 32, 32, { on, on, 0, on } });
    s.sections.push_back ({ 1, "drop", 32, 8, 8, { on } });
    s.sections.push_back ({ 2, "groove", 40, 8, 8, { on } });
    const auto note = [] (std::int64_t tick, std::int64_t length, int key, int voice, int section) {
        Note n;
        n.tick = tick;
        n.length = length;
        n.step = tick / ticksPerStep;
        n.key = static_cast<std::uint8_t> (key);
        n.velocity = 62;
        n.voice = static_cast<std::uint8_t> (voice);
        n.section = static_cast<std::int16_t> (section);
        return n;
    };
    // clang-format off: one stack a line
    s.lanes[li].score = {
        // three spans of the first range: 40 and 47 are common to all three, 52 moves to 55
        note (0, 4 * bar - gap, 40, 0, 0), note (0, 4 * bar - gap, 47, 1, 0), note (0, 4 * bar - gap, 52, 2, 0),
        note (4 * bar, 4 * bar - gap, 40, 0, 0), note (4 * bar, 4 * bar - gap, 47, 1, 0), note (4 * bar, 4 * bar - gap, 55, 2, 0),
        note (8 * bar, 8 * bar - gap, 40, 0, 0), note (8 * bar, 8 * bar - gap, 47, 1, 0), note (8 * bar, 8 * bar - gap, 55, 2, 0),
        // the second range: 52 ends one tick early, so its next strike is no hold
        note (24 * bar, 8 * bar - gap, 40, 0, 0), note (24 * bar, 2 * bar - gap - 1, 52, 1, 0),
        note (26 * bar, 6 * bar - gap, 52, 0, 0),
        // the stab section doesn't hold, and the held section after it doesn't reach back into it
        note (32 * bar, 4 * bar - gap, 40, 0, 1),
        note (36 * bar, 4 * bar - gap, 40, 0, 1),
        note (40 * bar, 8 * bar - gap, 40, 0, 2),
    };
    const auto before = s.lanes[li].score;
    holdPads (s, b, *b.style.laneIndex ("bass")); // not a chords lane: nothing to do
    CHECK (s.lanes[li].score == before);
    holdPads (s, b, li);
    const std::vector<Note> expected = {
        note (0, 16 * bar - gap, 40, 0, 0), note (0, 16 * bar - gap, 47, 1, 0), note (0, 4 * bar - gap, 52, 2, 0),
        note (4 * bar, 12 * bar - gap, 55, 0, 0), // the one key the second span strikes: voice 0 of its stack
        note (24 * bar, 8 * bar - gap, 40, 0, 0), note (24 * bar, 2 * bar - gap - 1, 52, 1, 0),
        note (26 * bar, 6 * bar - gap, 52, 0, 0),
        note (32 * bar, 4 * bar - gap, 40, 0, 1),
        note (36 * bar, 4 * bar - gap, 40, 0, 1),
        note (40 * bar, 8 * bar - gap, 40, 0, 2),
    };
    // clang-format on
    REQUIRE (s.lanes[li].score.size() == expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i)
    {
        INFO ("note " << i);
        CHECK (s.lanes[li].score[i] == expected[i]);
    }
}

TEST_CASE ("styles: the techno drone holds its common tones over every chord span", "[gen][styles][hold]")
{
    // The pedal plays the tonic chord in every 4-bar span. Voice leading may move an inner voice from
    // one span to the next, and a variant change on a phrase line re-voices the stack, but every key
    // the next span strikes again is held over: in a held section no key is ever struck straight after
    // itself, and a span that keeps its whole voicing strikes nothing.
    const Bundle& b = bundleFor ("FUT04");
    const std::size_t li = *b.style.laneIndex ("chords");
    std::int64_t gap = -1;
    for (const auto& g : b.style.lanes[li].chords.grids)
        if (g.name == "pad")
            gap = g.padGap;
    REQUIRE (gap > 0);
    std::size_t spans = 0, silent = 0, notes = 0, heldOver = 0;
    for (std::uint64_t seed = 0; seed < 16; ++seed)
    {
        const Song s = songFor ("FUT04", seed);
        const LaneOut& ch = laneOf (s, "chords");
        for (const auto& r : activeRanges (s, li))
        {
            const Section& sec = s.sections[r.section];
            if (b.style.lanes[li].gridFor (sec.kind) != "pad")
                continue;
            INFO ("seed " << seed << " " << sec.kind << " range at bar " << r.start / ticksPerBar);
            std::map<int, const Note*> last; // key -> its latest note in the range
            std::set<std::int64_t> onsets;
            for (const Note& n : ch.score)
            {
                if (n.tick < r.start || n.tick >= r.end)
                    continue;
                ++notes;
                onsets.insert (n.tick);
                CHECK (n.tick + n.length <= r.end - gap);
                if (const auto it = last.find (n.key); it != last.end())
                    CHECK (it->second->tick + it->second->length + gap != n.tick); // struck straight after itself: it would be held
                last[n.key] = &n;
                heldOver += n.length > 4 * ticksPerBar ? 1u : 0u;
            }
            CHECK (onsets.count (r.start) == 1); // the range opens on a stack
            for (const auto& c : s.chords)
                if (c.start >= r.start && c.start < r.end)
                {
                    ++spans;
                    silent += onsets.count (c.start) == 0 ? 1u : 0u;
                }
        }
        // every stack's voices count up from 0, low to high, as the feel's spread reads them
        for (std::size_t i = 0; i < ch.score.size();)
        {
            std::size_t j = i;
            while (j < ch.score.size() && ch.score[j].tick == ch.score[i].tick)
            {
                CHECK (ch.score[j].voice == j - i);
                if (j > i)
                    CHECK (ch.score[j].key > ch.score[j - 1].key);
                ++j;
            }
            i = j;
        }
    }
    INFO (silent << " of " << spans << " spans strike nothing; " << heldOver << " of " << notes << " notes held over a span");
    CHECK (spans > 200);
    CHECK (silent * 100 >= spans * 30); // a drone: a third of its spans or more just ring on
    CHECK (heldOver * 100 >= notes * 40); // and most notes ring on over the next span
}

// ------------------------------------------------------------------ fresh voicings, alike pushes

namespace
{
    // A section's chords phrase by phrase, by where each is written (the start a push moves it from).
    struct WrittenChord
    {
        std::int64_t at = 0; // ticks from the phrase's start
        std::int32_t degree = 0;
        std::int32_t pushedBy = 0;
        std::int64_t start = 0; // where it sounds from
    };

    std::vector<std::vector<WrittenChord>> phrasesOf (const Song& s, const Section& sec)
    {
        const std::int64_t phrase = std::int64_t (barsPerBlock) * ticksPerBar;
        std::vector<std::vector<WrittenChord>> out (static_cast<std::size_t> ((sec.bars + barsPerBlock - 1) / barsPerBlock));
        for (const auto& c : s.chords)
        {
            if (static_cast<std::size_t> (c.section) != sec.index)
                continue;
            const std::int64_t written = c.start + c.pushedBy - sec.startTick();
            const auto k = static_cast<std::size_t> (written / phrase);
            out[k].push_back ({ written - static_cast<std::int64_t> (k) * phrase, c.degree, c.pushedBy, c.start });
        }
        return out;
    }
}

TEST_CASE ("styles: alike pushes lay every phrase out the same, the section's first included", "[gen][styles][harmony]")
{
    // The sci-fi club pushes 40-60% of its changes. Without "alike" a section's first phrase counts one
    // change fewer (its first chord, which is never pushed), so its pushes fall elsewhere than in the
    // phrases after it.
    std::size_t compared = 0, pushed = 0;
    for (std::uint64_t seed = 0; seed < 16; ++seed)
    {
        const Song s = songFor ("FUT02", seed);
        for (const Section& sec : s.sections)
        {
            const auto phrases = phrasesOf (s, sec);
            REQUIRE_FALSE (phrases.empty());
            REQUIRE_FALSE (phrases[0].empty());
            CHECK (phrases[0][0].pushedBy == 0);
            for (std::size_t k = 1; k < phrases.size(); ++k)
            {
                INFO ("seed " << seed << " section " << sec.index << " (" << sec.kind << ") phrase " << k);
                REQUIRE (phrases[k].size() == phrases[0].size()); // the progression loops inside the phrase
                for (std::size_t i = 0; i < phrases[k].size(); ++i)
                {
                    CHECK (phrases[k][i].at == phrases[0][i].at);
                    CHECK (phrases[k][i].degree == phrases[0][i].degree);
                    if (i == 0)
                        continue; // the section's own first chord stays unpushed; a later phrase's may push
                    CHECK (phrases[k][i].pushedBy == phrases[0][i].pushedBy);
                    ++compared;
                    pushed += phrases[k][i].pushedBy > 0 ? 1u : 0u;
                }
            }
        }
    }
    INFO (pushed << " of " << compared << " compared changes pushed");
    CHECK (compared > 500);
    CHECK (pushed * 100 >= compared * 25);
}

TEST_CASE ("styles: fresh voicings repeat phrase by phrase", "[gen][styles][chords]")
{
    // With the chords lane's "fresh", voice leading starts again at every 8-bar phrase, so every phrase
    // of a pad section plays the voicings of the section's first phrase, key for key.
    const Bundle& b = bundleFor ("FUT02");
    const std::size_t li = *b.style.laneIndex ("chords");
    REQUIRE (b.style.lanes[li].chords.fresh);
    std::size_t compared = 0, sections = 0;
    for (std::uint64_t seed = 0; seed < 16; ++seed)
    {
        const Song s = songFor ("FUT02", seed);
        const LaneOut& ch = laneOf (s, "chords");
        // the keys struck at a tick
        const auto stackAt = [&ch] (std::int64_t tick) {
            std::vector<int> keys;
            for (const Note& n : ch.score)
                if (n.tick == tick)
                    keys.push_back (n.key);
            return keys;
        };
        for (const Section& sec : s.sections)
        {
            if (b.style.lanes[li].gridFor (sec.kind) != "pad" || !sec.active (li, 0))
                continue;
            ++sections;
            const auto phrases = phrasesOf (s, sec);
            for (std::size_t k = 1; k < phrases.size(); ++k)
            {
                if (!sec.active (li, static_cast<std::int32_t> (k) * barsPerBlock))
                    continue;
                REQUIRE (phrases[k].size() == phrases[0].size());
                for (std::size_t i = 0; i < phrases[k].size(); ++i)
                {
                    INFO ("seed " << seed << " " << sec.kind << " phrase " << k << " chord " << i);
                    const auto first = stackAt (phrases[0][i].start);
                    REQUIRE (first.size() >= 3);
                    CHECK (stackAt (phrases[k][i].start) == first);
                    ++compared;
                }
            }
        }
    }
    INFO (compared << " chords compared in " << sections << " pad sections");
    CHECK (sections > 40);
    CHECK (compared > 500);
}
