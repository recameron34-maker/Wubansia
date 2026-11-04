// Builds that build (the extended mix's risers, before S1b2's fills and transitions): a drum grid's
// "stages", a ladder of grids across the section (quarters, then 8ths, then 16ths), and its "ramp",
// a velocity crescendo from the section's first bar to its last; plus the CC74 filter rising through
// every build and the drop landing at least as open as the build left it.

#include "gen/content.h"
#include "gen/song.h"
#include "util/file_io.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

using namespace wubansia;
using namespace wubansia::gen;
using Catch::Matchers::ContainsSubstring;

namespace
{
    const std::filesystem::path contentDir = std::filesystem::path (WUBANSIA_SOURCE_DIR) / "content";

    const Bundle& bundleFor (std::string_view id)
    {
        static std::map<std::string, Bundle, std::less<>> cache;
        auto it = cache.find (id);
        if (it == cache.end())
        {
            auto b = loadBundle (contentDir, id);
            INFO (b.error);
            REQUIRE (b.ok());
            it = cache.emplace (std::string (id), std::move (b.value)).first;
        }
        return it->second;
    }

    Song songFor (std::string_view id, std::uint64_t seed)
    {
        Options o;
        o.seed = seed;
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

    std::string readText (const std::filesystem::path& p)
    {
        const auto r = util::readBinaryFile (p);
        REQUIRE (r.ok());
        return std::string (r.value.begin(), r.value.end());
    }

    std::string replaced (std::string text, std::string_view from, std::string_view to)
    {
        const auto at = text.find (from);
        INFO ("looking for: " << from);
        REQUIRE (at != std::string::npos);
        text.replace (at, from.size(), to);
        return text;
    }

    const char* const kits[] = { "FUT01", "FUT02", "FUT03", "FUT04" };
}

TEST_CASE ("builds: the loader reads a grid's stages and ramp, and refuses broken ones", "[gen][builds][content]")
{
    // every style's snare lane builds with a ladder of stages and a crescendo
    for (const char* id : kits)
    {
        INFO (id);
        const Style& st = bundleFor (id).style;
        const LaneSpec& snare = st.lanes[*st.laneIndex ("snare")];
        const DrumGrid* build = nullptr;
        for (const auto& g : snare.drums.grids)
            if (g.name == snare.gridFor ("build"))
                build = &g;
        REQUIRE (build != nullptr);
        CHECK (build->stages.size() >= 3);
        CHECK (build->voices.empty());
        REQUIRE (build->ramp.has_value());
        CHECK (build->ramp->lo < build->ramp->hi);
        for (const auto& name : build->stages)
        {
            const auto it = std::find_if (snare.drums.grids.begin(), snare.drums.grids.end(), [&] (const DrumGrid& g) { return g.name == name; });
            REQUIRE (it != snare.drums.grids.end());
            CHECK (it->stages.empty());
            CHECK_FALSE (it->ramp.has_value());
            CHECK_FALSE (it->voices.empty());
        }
    }

    const std::string text = readText (contentDir / "styles" / "garage2step.json");
    const std::string staged = "\"stages\": [\"build4\", \"build4\", \"build8\", \"build16\"]";
    REQUIRE (text.find (staged) != std::string::npos);
    const auto refusal = [&] (std::string_view from, std::string_view to) {
        const auto r = parseStyle (replaced (text, from, to), "style.json");
        CHECK_FALSE (r.ok());
        return r.error;
    };
    CHECK_THAT (refusal (staged, "\"stages\": [\"build4\", \"nope\"]"), ContainsSubstring ("\"nope\""));
    CHECK_THAT (refusal (staged, "\"stages\": []"), ContainsSubstring ("stages"));
    CHECK_THAT (refusal (staged, "\"stages\": [\"build4\", \"build\"]"), ContainsSubstring ("stages of its own"));
    CHECK_THAT (refusal ("\"ramp\": [650, 1100]", "\"ramp\": [650, 2100]"), ContainsSubstring ("2000"));
    CHECK_THAT (refusal ("\"ramp\": [650, 1100]", "\"ramp\": [1100, 650]"), ContainsSubstring ("[low, high]"));
    CHECK_THAT (refusal (staged, staged + std::string (", \"voices\": { \"snare\": { \"p\": [100, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0], \"vel\": [90, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0] } }")),
        ContainsSubstring ("voices"));
    // a stage plays under the staged grid's ramp, so it can't ramp on its own
    CHECK_THAT (refusal ("\"build4\": {", "\"build4\": { \"ramp\": [800, 900],"), ContainsSubstring ("ramp"));
    // and a ladder has no "sync" or "never" of its own either: its stages' patterns hold them
    CHECK_THAT (refusal (staged, staged + std::string (", \"sync\": [11, 24]")), ContainsSubstring ("stages"));
    CHECK_THAT (refusal (staged, staged + std::string (", \"never\": [[0, 4, 8, 12]]")), ContainsSubstring ("stages"));
}

TEST_CASE ("builds: the ramp scales a build's first bar by its low end and its last bar by its high end", "[gen][builds]")
{
    // FUT01's snare build ramps [650, 1100] over the section's bars; with every spread at 0
    // each hit's score velocity is its cell's, scaled by the build's 950 and then exactly by the ramp
    // (rounded half up, as the generator's roundDiv does).
    Bundle b = bundleFor ("FUT01");
    LaneSpec& spec = b.style.lanes[*b.style.laneIndex ("snare")];
    for (auto& g : spec.drums.grids)
        for (auto& v : g.voices)
            v.spread = 0;
    const auto gridNamed = [&] (std::string_view name) -> const DrumGrid& {
        return *std::find_if (spec.drums.grids.begin(), spec.drums.grids.end(), [&] (const DrumGrid& g) { return g.name == name; });
    };
    const DrumGrid& build = gridNamed (spec.gridFor ("build"));
    REQUIRE (build.ramp.has_value());
    REQUIRE (build.ramp->lo == 650);
    REQUIRE (build.ramp->hi == 1100);
    const std::int64_t sectionVel = b.style.section ("build")->vel;
    REQUIRE (sectionVel == 950);
    const auto scaled = [] (std::int64_t v, std::int64_t permille) { return (2 * v * permille + 1000) / 2000; };
    std::size_t firstBars = 0, lastBars = 0;
    for (std::uint64_t seed = 0; seed < 4; ++seed)
    {
        Options o;
        o.seed = seed;
        const auto s = generate (b, o);
        REQUIRE (s.ok());
        const LaneOut& snare = laneOf (s.value, "snare");
        for (const Section& sec : s.value.sections)
        {
            if (sec.kind != "build")
                continue;
            for (const Note& n : snare.score)
            {
                if (n.section != static_cast<std::int16_t> (sec.index) || n.voice != 0)
                    continue;
                const auto bar = static_cast<std::int32_t> (n.tick / ticksPerBar - sec.startBar);
                if (bar != 0 && bar != sec.bars - 1)
                    continue;
                const DrumGrid& g = gridNamed (build.stages[static_cast<std::size_t> (bar) * build.stages.size() / static_cast<std::size_t> (sec.bars)]);
                REQUIRE (g.voices.front().voice == "snare"); // the stages' first voice
                const auto cell = static_cast<std::size_t> ((bar % 2) * 16 + n.step % 16);
                const std::int64_t written = g.voices.front().vel[cell];
                const std::int64_t ramp = bar == 0 ? 650 : 1100;
                INFO ("seed " << seed << " build at bar " << sec.startBar << ", bar " << bar << " step " << n.step % 16 << ": written " << written);
                CHECK (n.velocity == scaled (scaled (written, sectionVel), ramp));
                ++(bar == 0 ? firstBars : lastBars);
            }
        }
    }
    CHECK (firstBars >= 16);
    CHECK (lastBars >= 16);
}

TEST_CASE ("builds: in every style's data a drop starts at least as open as the build before it can end", "[gen][builds][styles]")
{
    // The sampled check below can't see a margin of 0: each CC lane's drop and drop2 rows start no
    // lower than the build row's highest end, and a build row has no arches that could lift it past
    // its "to" (round 1's defect was the filter closing at the drop).
    std::size_t pairs = 0;
    for (const char* id : kits)
        for (const LaneSpec& lane : bundleFor (id).style.lanes)
            for (const CcSpec& cc : lane.cc)
            {
                const CcRow* build = cc.row ("build");
                if (build == nullptr)
                    continue;
                INFO (id << " " << lane.id << " CC" << cc.number);
                CHECK (build->waveBars.empty());
                for (const char* kind : { "drop", "drop2" })
                    if (const CcRow* drop = cc.row (kind); drop != nullptr)
                    {
                        INFO (kind << " from " << drop->from.lo << "-" << drop->from.hi << ", the build to " << build->to.lo << "-" << build->to.hi);
                        CHECK (drop->from.lo >= build->to.hi);
                        ++pairs;
                    }
            }
    CHECK (pairs >= 5);
}

TEST_CASE ("builds: each part of a build plays its stage, under a crescendo", "[gen][builds]")
{
    // FUT01's builds: the snare in quarters, then 8ths, then 16ths (stages build4, build4, build8,
    // build16 over the section), from 65% to 110% of the velocity.
    const Bundle& b = bundleFor ("FUT01");
    const LaneSpec& spec = b.style.lanes[*b.style.laneIndex ("snare")];
    const auto gridNamed = [&] (std::string_view name) -> const DrumGrid& {
        return *std::find_if (spec.drums.grids.begin(), spec.drums.grids.end(), [&] (const DrumGrid& g) { return g.name == name; });
    };
    const DrumGrid& build = gridNamed (spec.gridFor ("build"));
    REQUIRE (build.stages.size() == 4);
    for (std::uint64_t seed = 0; seed < 8; ++seed)
    {
        const Song s = songFor ("FUT01", seed);
        const LaneOut& snare = laneOf (s, "snare");
        for (const Section& sec : s.sections)
        {
            if (sec.kind != "build")
                continue;
            std::vector<std::int64_t> velSum (static_cast<std::size_t> (sec.bars), 0), onBeats (static_cast<std::size_t> (sec.bars), 0), count (static_cast<std::size_t> (sec.bars), 0);
            for (const Note& n : snare.score)
            {
                if (n.section != static_cast<std::int16_t> (sec.index) || n.flags & (noteFlag::ornament | noteFlag::turn))
                    continue;
                const auto bar = static_cast<std::int32_t> (n.tick / ticksPerBar - sec.startBar);
                const auto stage = static_cast<std::size_t> (bar * 4 / sec.bars);
                const DrumGrid& g = gridNamed (build.stages[stage]);
                // the note is on a cell its stage's grid can play, for its voice
                bool playable = false;
                for (const auto& v : g.voices)
                    if (spec.drums.voices[n.voice].id == v.voice && v.hasCells)
                        playable = playable || v.p[static_cast<std::size_t> ((bar % 2) * 16 + n.step % 16)] > 0;
                INFO ("seed " << seed << " bar " << bar << " of the build at " << sec.startBar << ", stage " << build.stages[stage] << ", step " << n.step % 16);
                CHECK (playable);
                if (n.voice == 0)
                    ++count[static_cast<std::size_t> (bar)];
                if (n.voice == 0 && n.step % 4 == 0) // the crescendo, on the beats every stage plays
                {
                    velSum[static_cast<std::size_t> (bar)] += n.velocity;
                    ++onBeats[static_cast<std::size_t> (bar)];
                }
            }
            const auto first = static_cast<std::size_t> (0), last = static_cast<std::size_t> (sec.bars - 1);
            REQUIRE (onBeats[first] > 0);
            REQUIRE (onBeats[last] > 0);
            INFO ("seed " << seed << " build at " << sec.startBar << ": snare hits " << count[first] << " in its first bar, " << count[last] << " in its last; mean velocity on the beat "
                          << velSum[first] / onBeats[first] << " then " << velSum[last] / onBeats[last]);
            CHECK (count[last] >= 3 * count[first]); // quarters to 16ths
            CHECK (velSum[last] * onBeats[first] >= velSum[first] * onBeats[last] + 30 * onBeats[first] * onBeats[last]); // 30 louder, on average
        }
    }
}

TEST_CASE ("builds: every style's builds rise to the drop", "[gen][builds][styles]")
{
    for (const char* id : kits)
    {
        std::size_t builds = 0;
        for (std::uint64_t seed = 0; seed < 16; ++seed)
        {
            const Song s = songFor (id, seed);
            const LaneOut& snare = laneOf (s, "snare");
            const LaneOut& hats = laneOf (s, "hats");
            for (const Section& sec : s.sections)
            {
                if (sec.kind != "build")
                    continue;
                ++builds;
                const std::int32_t quarter = sec.bars / 4;
                const auto inBars = [&] (const LaneOut& lane, std::int32_t from, std::int32_t to, const std::function<bool (const Note&)>& pick) {
                    std::size_t n = 0;
                    for (const Note& x : lane.score)
                        if (x.tick >= (sec.startBar + from) * ticksPerBar && x.tick < (sec.startBar + to) * ticksPerBar && pick (x))
                            ++n;
                    return n;
                };
                const auto any = [] (const Note&) { return true; };
                const std::size_t early = inBars (snare, 0, quarter, any), late = inBars (snare, sec.bars - quarter, sec.bars, any);
                INFO (id << " seed " << seed << " build at bar " << sec.startBar << ": snare-lane notes " << early << " in its first quarter, " << late << " in its last");
                CHECK (late >= 2 * early); // the roll gets denser
                // the hats open up (garage: the open hats ring on the offbeats) or fill in (16ths)
                const auto openHat = bundleFor (id).kit.lane ("hats")->notes[1].note; // closed, open
                const auto open = [&] (const Note& x) { return x.key == openHat; };
                CHECK ((inBars (hats, sec.bars - quarter, sec.bars, open) > inBars (hats, 0, quarter, open)
                        || inBars (hats, sec.bars - quarter, sec.bars, any) > inBars (hats, 0, quarter, any)));
                // every CC lane playing in the build sweeps up through it, and the drop after it
                // starts at least as open as the build ends
                for (const auto& lane : s.lanes)
                {
                    std::vector<const ControlPoint*> here, next;
                    for (const ControlPoint& c : lane.cc)
                    {
                        if (c.section == static_cast<std::int16_t> (sec.index))
                            here.push_back (&c);
                        if (c.section == static_cast<std::int16_t> (sec.index + 1))
                            next.push_back (&c);
                    }
                    if (here.empty())
                        continue;
                    INFO (lane.id << " CC" << int (here.front()->number) << ": " << int (here.front()->value) << " at the build's start, " << int (here.back()->value) << " at its end"
                                  << (next.empty() ? std::string() : ", " + std::to_string (next.front()->value) + " at the next section's start"));
                    CHECK (here.back()->value >= here.front()->value + 30);
                    if (!next.empty() && sec.index + 1 < s.sections.size() && (s.sections[sec.index + 1].kind == "drop" || s.sections[sec.index + 1].kind == "drop2"))
                        CHECK (next.front()->value >= here.back()->value);
                }
            }
        }
        CHECK (builds >= 16);
    }
    // FUT01 and FUT03 have a filter on their chords now, so their builds sweep too
    for (const char* id : { "FUT01", "FUT03" })
    {
        const Song s = songFor (id, 1);
        CHECK_FALSE (laneOf (s, "chords").cc.empty());
    }
}
