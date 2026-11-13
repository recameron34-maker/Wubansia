// The form grammar (S1b2a, arrange.h): every seed draws its own extended mix from its style's
// grammar, the way club extended mixes are put together, and the lanes move inside the drops.
// Checked on 64 seeds per style, from the songs alone (sections, blocks, the bars each lane plays):
// - the mix: 200 to 240 bars of sections on 8-bar lines, the 32-bar mix-in from drums only with
//   lanes only joining (one at least every 16 bars) and carrying on into the section after it, the
//   32-bar mix-out with lanes only leaving down to drums only;
// - the lines: drop and break starts on the 8-bar grid, about 70% on 16-bar lines;
// - builds of 8 to 32 bars, each leading into a drop;
// - G10's form part: at least 8 distinct forms, none in more than a quarter of the seeds;
// - the lanes inside the drops: a lane change inside every drop and drop 2, moves only on 8-bar
//   lines and in the bar before a 16-bar line, drop 2's layer, and drop 1 at least 10% denser than
//   the groove; the lanes each drop keeps in every bar as the written mixes do (drop 1's kick,
//   drop 2's hats), and one-bar pulls that take their whole group and land back on the line;
// - the loader refuses grammars that could draw a form breaking a DJ rule, and says where.

#include "gen/arrange.h"
#include "gen/content.h"
#include "gen/export.h"
#include "gen/json.h"
#include "gen/song.h"
#include "rng/rng.h"
#include "util/file_io.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

using namespace wubansia;
using namespace wubansia::gen;
using Catch::Matchers::ContainsSubstring;

namespace
{
    constexpr std::array<const char*, 4> kits = { "FUT01", "FUT02", "FUT03", "FUT04" };
    constexpr std::uint64_t seeds = 64;

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

    // The 64 songs of a kit, generated once.
    const std::vector<Song>& songsOf (std::string_view id)
    {
        static std::map<std::string, std::vector<Song>, std::less<>> cache;
        auto it = cache.find (id);
        if (it == cache.end())
        {
            std::vector<Song> songs;
            for (std::uint64_t seed = 0; seed < seeds; ++seed)
            {
                Options o;
                o.seed = seed;
                auto s = generate (bundleFor (id), o);
                INFO (s.error);
                REQUIRE (s.ok());
                songs.push_back (std::move (s.value));
            }
            it = cache.emplace (std::string (id), std::move (songs)).first;
        }
        return it->second;
    }

    // The lanes playing in a bar of a section, as a mask (bit i: lane i).
    std::uint64_t lanesAt (const Song& s, const Section& sec, std::int32_t bar)
    {
        std::uint64_t m = 0;
        for (std::size_t l = 0; l < s.lanes.size(); ++l)
            if (sec.active (l, bar))
                m |= std::uint64_t (1) << l;
        return m;
    }

    bool isDrums (const Song& s, std::size_t lane) { return s.lanes[lane].kind == LaneKind::drums; }

    std::size_t laneIndexOf (const Song& s, std::string_view id)
    {
        for (std::size_t i = 0; i < s.lanes.size(); ++i)
            if (s.lanes[i].id == id)
                return i;
        FAIL ("no lane " << id);
        return 0;
    }

    std::string formOf (const Song& s)
    {
        std::string out;
        for (const auto& sec : s.sections)
            out += (out.empty() ? "" : " ") + sec.kind + std::to_string (sec.bars);
        return out;
    }

    bool isDrop (const std::string& kind) { return kind == "drop" || kind == "drop2"; }
    bool startsOnLine (const std::string& kind) { return isDrop (kind) || kind == "break"; }

    // ---- the loader's tests: a valid grammar for garage2step's lanes, each plan on its own line

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

    // The style text with its "form" value replaced: found by bracket depth outside strings.
    std::string withForm (const std::string& style, const std::string& form)
    {
        const std::string key = "\"form\": ";
        const auto at = style.find (key);
        REQUIRE (at != std::string::npos);
        const std::size_t value = at + key.size();
        std::size_t i = value;
        int depth = 0;
        bool inString = false;
        for (; i < style.size(); ++i)
        {
            const char c = style[i];
            if (inString)
            {
                if (c == '\\')
                    ++i;
                else if (c == '"')
                    inString = false;
            }
            else if (c == '"')
                inString = true;
            else if (c == '[' || c == '{')
                ++depth;
            else if ((c == ']' || c == '}') && --depth == 0)
                break;
        }
        REQUIRE (i < style.size());
        return style.substr (0, value) + form + style.substr (i + 1);
    }

    const std::string baseForm = R"({
    "bars": [176, 208], "sixteen": 70,
    "sections": [
      { "kind": "intro", "bars": [{ "bars": 32, "w": 1 }] },
      { "kind": "groove", "bars": [{ "bars": 16, "w": 1 }, { "bars": 24, "w": 1 }, { "bars": 32, "w": 1 }] },
      { "kind": "build", "bars": [{ "bars": 8, "w": 1 }, { "bars": 16, "w": 1 }] },
      { "kind": "drop", "bars": [{ "bars": 16, "w": 1 }, { "bars": 24, "w": 2 }, { "bars": 32, "w": 1 }] },
      { "kind": "break", "bars": [{ "bars": 16, "w": 1 }, { "bars": 24, "w": 1 }] },
      { "kind": "build", "bars": [{ "bars": 8, "w": 1 }, { "bars": 16, "w": 2 }] },
      { "kind": "drop2", "bars": [{ "bars": 16, "w": 2 }, { "bars": 24, "w": 1 }, { "bars": 32, "w": 1 }] },
      { "kind": "outro", "bars": [{ "bars": 32, "w": 1 }] }
    ],
    "layer": [{ "lane": "perc", "w": 1 }],
    "plans": {
      "intro": { "choices": [ { "w": 1, "blocks": [["kick", "hats"], ["kick", "hats", "shaker"], ["kick", "snare", "hats", "shaker"], ["kick", "snare", "hats", "shaker", "bass"]] } ] },
      "groove": { "lanes": ["kick", "snare", "hats", "shaker", "perc", "bass", "chords"], "entries": [{ "lanes": ["chords"], "after": [{ "bars": 8, "w": 1 }] }] },
      "build": { "lanes": ["snare", "hats", "shaker", "perc", "chords"] },
      "drop": { "lanes": ["kick", "snare", "hats", "shaker", "perc", "bass", "chords"], "rest": { "p": 50, "bars": 8, "lanes": [{ "lane": "kick", "w": 1 }, { "lane": "chords", "w": 1 }] }, "dropout": { "p": 50, "every": 16, "lanes": [{ "lanes": ["kick", "bass"], "w": 1 }] }, "change": true },
      "break": { "lanes": ["hats", "perc", "bass", "chords"], "entries": [{ "lanes": ["bass"], "last": [{ "bars": 8, "w": 1 }] }] },
      "drop2": { "lanes": ["kick", "snare", "hats", "shaker", "perc", "bass", "chords"], "rest": { "p": 40, "bars": 8, "lanes": [{ "lane": "hats", "w": 1 }, { "lane": "chords", "w": 1 }] }, "change": true },
      "outro": { "choices": [ { "w": 1, "blocks": [["kick", "snare", "hats", "shaker", "perc", "bass"], ["kick", "snare", "hats", "shaker", "perc", "bass"], ["kick", "snare", "hats", "shaker", "perc"], ["kick", "hats", "shaker"]] } ] }
    }
  })";

    struct Texts
    {
        std::string kit, style, feel;
    };

    // The garage2step bundle with this form, or the loader's error.
    util::Result<Bundle> formBundle (const std::string& form)
    {
        const auto content = std::filesystem::path (WUBANSIA_SOURCE_DIR) / "content";
        auto kit = parseKit (readText (content / "kits" / "fut01.json"), "kit.json");
        REQUIRE (kit.ok());
        auto style = parseStyle (withForm (readText (content / "styles" / "garage2step.json"), form), "style.json");
        if (!style.ok())
            return util::Result<Bundle>::failure (style.error);
        auto feel = parseFeel (readText (content / "feels" / "garage2step.json"), "feel.json");
        REQUIRE (feel.ok());
        return makeBundle (std::move (kit.value), std::move (style.value), std::move (feel.value));
    }

    // The error the garage2step bundle gives with this form, or "" if it loads.
    std::string formError (const std::string& form)
    {
        const auto b = formBundle (form);
        return b.ok() ? std::string() : b.error;
    }

    std::string formWith (std::string_view from, std::string_view to) { return formError (replaced (baseForm, from, to)); }

    // The songs of the first n seeds of a bundle made from an edited baseForm.
    std::vector<Song> songsWith (const std::string& form, std::uint64_t n)
    {
        const auto b = formBundle (form);
        INFO (b.error);
        REQUIRE (b.ok());
        std::vector<Song> out;
        for (std::uint64_t seed = 0; seed < n; ++seed)
        {
            Options o;
            o.seed = seed;
            auto s = generate (b.value, o);
            INFO (s.error);
            REQUIRE (s.ok());
            out.push_back (std::move (s.value));
        }
        return out;
    }

    std::uint64_t maskOf (const Song& s, const std::vector<std::string>& ids)
    {
        std::uint64_t m = 0;
        for (const auto& id : ids)
            m |= std::uint64_t (1) << laneIndexOf (s, id);
        return m;
    }
}

TEST_CASE ("form: every seed draws a DJ-friendly extended mix of 200 to 240 bars", "[gen][form]")
{
    for (const char* id : kits)
        for (const Song& s : songsOf (id))
        {
            INFO (id << " seed " << s.seed << ": " << formOf (s));
            // sections on 8-bar lines, tiling the song: 200 to 240 bars (techno 208 to 240), 5:58 to 7:30
            // at the kits' tempos
            std::int32_t bar = 0;
            for (const Section& sec : s.sections)
            {
                CHECK (sec.startBar == bar);
                CHECK (sec.bars % barsPerBlock == 0);
                CHECK (sec.bars >= barsPerBlock);
                CHECK (sec.blocks.size() == static_cast<std::size_t> (sec.bars / barsPerBlock));
                bar += sec.bars;
            }
            CHECK (bar == s.totalBars);
            CHECK (s.totalBars >= 200);
            CHECK (s.totalBars <= 240);
            REQUIRE (s.sections.size() >= 4);

            // the 32-bar mix-in: drums only first, then lanes only join, at least one every 16 bars
            const Section& in = s.sections.front();
            REQUIRE (in.kind == "intro");
            REQUIRE (in.bars == 32);
            for (std::size_t l = 0; l < s.lanes.size(); ++l)
                if (in.active (l, 0))
                    CHECK (isDrums (s, l));
            for (std::size_t k = 1; k < in.blocks.size(); ++k)
            {
                CHECK ((in.blocks[k] & in.blocks[k - 1]) == in.blocks[k - 1]); // lanes only join
                if (k >= 2)
                    CHECK_FALSE ((in.blocks[k] == in.blocks[k - 1] && in.blocks[k - 1] == in.blocks[k - 2]));
            }
            // and the section after it plays every lane the mix-in ends on, from its first bar, and
            // keeps the mix-in's rule across the line, through its first 16 bars: never three blocks in
            // a row with the same lanes (a long groove then holds its lanes while its patterns move)
            CHECK ((lanesAt (s, s.sections[1], 0) & in.blocks.back()) == in.blocks.back());
            std::vector<std::uint64_t> edge = in.blocks;
            edge.insert (edge.end(), s.sections[1].blocks.begin(), s.sections[1].blocks.begin() + std::min<std::ptrdiff_t> (2, std::ssize (s.sections[1].blocks)));
            for (std::size_t k = 2; k < edge.size(); ++k)
            {
                INFO ("block " << k << " from the mix-in's start");
                CHECK_FALSE ((edge[k] == edge[k - 1] && edge[k - 1] == edge[k - 2]));
            }

            // the 32-bar mix-out: lanes only leave, at least one every 16 bars, down to drums only
            const Section& out = s.sections.back();
            REQUIRE (out.kind == "outro");
            REQUIRE (out.bars == 32);
            for (std::size_t k = 1; k < out.blocks.size(); ++k)
            {
                CHECK ((out.blocks[k] & out.blocks[k - 1]) == out.blocks[k]); // lanes only leave
                if (k >= 2)
                    CHECK_FALSE ((out.blocks[k] == out.blocks[k - 1] && out.blocks[k - 1] == out.blocks[k - 2]));
            }
            for (std::size_t l = 0; l < s.lanes.size(); ++l)
                if (out.active (l, out.bars - 1))
                    CHECK (isDrums (s, l));
            // the kick: a DJ mixes on it at both ends
            CHECK (in.active (laneIndexOf (s, "kick"), 0));
            CHECK (out.active (laneIndexOf (s, "kick"), out.bars - 1));

            // the kinds: one drop and one drop 2, at least one break and one build, intro and outro only at the ends
            std::map<std::string, int> count;
            for (const Section& sec : s.sections)
                ++count[sec.kind];
            CHECK (count["intro"] == 1);
            CHECK (count["outro"] == 1);
            CHECK (count["drop"] == 1);
            CHECK (count["drop2"] == 1);
            CHECK (count["break"] >= 1);
            CHECK (count["build"] >= 1);
            CHECK (count["groove"] >= 1);
            CHECK (s.sections[1].kind == "groove");
        }
}

TEST_CASE ("form: drop and break starts sit on the 8-bar grid, about 70% on 16-bar lines", "[gen][form]")
{
    // Each drop, drop 2 and break start is drawn on a 16-bar line with the style's chance, 70%, as on
    // annotated four-on-the-floor club tracks, else on 8 mod 16. Some 210 to 230 starts per style
    // (an optional early break comes and goes), so the share's standard deviation is about 3 points:
    // the band is 60% to 80%, more than 3 of them either way. Both kinds of line must occur.
    for (const char* id : kits)
    {
        std::size_t starts = 0, sixteen = 0;
        for (const Song& s : songsOf (id))
            for (const Section& sec : s.sections)
                if (startsOnLine (sec.kind))
                {
                    INFO (id << " seed " << s.seed << ": " << sec.kind << " at bar " << sec.startBar);
                    CHECK (sec.startBar % 8 == 0);
                    ++starts;
                    sixteen += sec.startBar % 16 == 0 ? 1u : 0u;
                }
        INFO (id << ": " << sixteen << " of " << starts << " drop and break starts on a 16-bar line");
        CHECK (starts >= 3 * seeds);
        CHECK (sixteen * 10 >= starts * 6);
        CHECK (sixteen * 10 <= starts * 8);
    }
}

TEST_CASE ("form: builds are 8 to 32 bars and lead into a drop", "[gen][form]")
{
    for (const char* id : kits)
    {
        std::set<std::int32_t> lengths;
        for (const Song& s : songsOf (id))
            for (const Section& sec : s.sections)
                if (sec.kind == "build")
                {
                    INFO (id << " seed " << s.seed << ": " << formOf (s));
                    CHECK (sec.bars >= 8);
                    CHECK (sec.bars <= 32);
                    REQUIRE (sec.index + 1 < s.sections.size());
                    CHECK (isDrop (s.sections[sec.index + 1].kind));
                    lengths.insert (sec.bars);
                }
        INFO (id);
        CHECK (lengths.size() >= 2); // the length is drawn, not fixed
    }
}

TEST_CASE ("form: at least 8 distinct forms in 64 seeds per style, none in more than a quarter", "[gen][form]")
{
    // G10's form part, on the songs: the kind and length of each section in order.
    for (const char* id : kits)
    {
        std::map<std::string, int> forms;
        for (const Song& s : songsOf (id))
            ++forms[formOf (s)];
        int top = 0;
        for (const auto& [f, n] : forms)
            top = std::max (top, n);
        INFO (id << ": " << forms.size() << " forms, the top one " << top << " times");
        CHECK (forms.size() >= 8);
        CHECK (4 * top <= static_cast<int> (seeds));
    }
}

TEST_CASE ("form: every drop and drop 2 has a lane change inside it, on 8-bar lines or the bar before a 16-bar line", "[gen][form]")
{
    // The drops aren't one block of every lane: a lane out for 8 or 16 bars, a one-bar dropout before
    // the drop's own 16-bar line, or drop 2's strip at its end. A move lands on an 8-bar line (a
    // block's lanes) or takes the bar before a 16-bar line out (a dropout: lanes only leave for it).
    std::map<std::string, std::size_t> rests, dropouts;
    for (const char* id : kits)
        for (const Song& s : songsOf (id))
            for (const Section& sec : s.sections)
            {
                if (!isDrop (sec.kind))
                    continue;
                INFO (id << " seed " << s.seed << ": the " << sec.kind << " at bar " << sec.startBar << " (" << sec.bars << " bars)");
                std::size_t moves = 0;
                for (std::int32_t bar = 0; bar < sec.bars; ++bar)
                {
                    INFO ("bar " << bar << " of the section");
                    const std::uint64_t here = lanesAt (s, sec, bar), block = sec.blocks[static_cast<std::size_t> (bar / barsPerBlock)];
                    if (here != block)
                    {
                        // a dropout: lanes of the block out for the bar before the drop's own 16-bar line
                        CHECK ((here & block) == here);
                        CHECK ((bar + 1) % 16 == 0);
                        CHECK (bar + 1 < sec.bars);
                        ++dropouts[id];
                    }
                    if (bar > 0 && here != lanesAt (s, sec, bar - 1))
                    {
                        ++moves;
                        CHECK ((bar % 8 == 0 || here != block)); // on an 8-bar line, or into a dropout
                    }
                }
                CHECK (moves >= 1);
                // a lane out for a block inside the drop (never its first: the drop lands with every lane)
                for (std::size_t k = 1; k < sec.blocks.size(); ++k)
                    rests[id] += (sec.blocks[k] & sec.blocks[0]) != sec.blocks[0] ? 1u : 0u;
            }
    for (const char* id : kits)
    {
        INFO (id << ": " << rests[id] << " blocks of a drop with a lane out, " << dropouts[id] << " dropout bars");
        CHECK (rests[id] >= seeds / 2); // both kinds of move happen
        CHECK (dropouts[id] >= seeds / 4);
    }
}

TEST_CASE ("form: a lane sits out the bars it doesn't play, dropouts included", "[gen][form]")
{
    // Every note's score onset lies in a bar its lane plays, and nothing rings past the run of bars
    // the lane plays there (a block it sits out, or a one-bar dropout), in either version.
    for (const char* id : kits)
        for (std::uint64_t seed = 0; seed < 16; ++seed)
        {
            const Song& s = songsOf (id)[seed];
            INFO (id << " seed " << seed);
            for (std::size_t li = 0; li < s.lanes.size(); ++li)
            {
                const LaneOut& lane = s.lanes[li];
                for (std::size_t i = 0; i < lane.score.size(); ++i)
                {
                    const Note& n = lane.score[i];
                    const Section& sec = s.sections[static_cast<std::size_t> (n.section)];
                    const auto bar = static_cast<std::int32_t> (n.tick / ticksPerBar) - sec.startBar;
                    INFO (lane.id << " note at tick " << n.tick << ", bar " << bar << " of the " << sec.kind << " at bar " << sec.startBar);
                    REQUIRE (sec.active (li, bar));
                    std::int32_t end = bar;
                    while (end < sec.bars && sec.active (li, end))
                        ++end;
                    const std::int64_t runEnd = (sec.startBar + end) * ticksPerBar;
                    CHECK (n.tick + n.length <= runEnd);
                    CHECK (lane.perf[i].tick + lane.perf[i].length <= runEnd);
                }
            }
        }
}

TEST_CASE ("form: both drops play the zaps (FUT02) and the snare (FUT04) in every block, as their written mixes do", "[gen][form]")
{
    // The kits' own extended mixes keep FUT02's laser zaps and FUT04's metal snare in every line of
    // both drops, so in neither drop does the lane rest, join late or leave early (no pull group
    // takes it either). That is the lanes: the grids are part 1's, so FUT04's first drop plays its
    // sparse snare where the written mix plays the backbeat on 2 and 4 (ADR 0023 §4 lists it).
    for (const auto& [id, lane] : { std::pair { "FUT02", "perc" }, std::pair { "FUT04", "snare" } })
        for (const Song& s : songsOf (id))
        {
            const std::size_t li = laneIndexOf (s, lane);
            std::size_t notes = 0;
            for (const Note& n : s.lanes[li].score)
                notes += s.sections[static_cast<std::size_t> (n.section)].kind == "drop" ? 1u : 0u;
            for (const Section& sec : s.sections)
                if (isDrop (sec.kind))
                    for (std::size_t k = 0; k < sec.blocks.size(); ++k)
                    {
                        INFO (id << " seed " << s.seed << ": the " << sec.kind << " at bar " << sec.startBar << ", its block at bar " << k * barsPerBlock);
                        CHECK ((sec.blocks[k] >> li & 1u) == 1u);
                    }
            INFO (id << " seed " << s.seed << ": " << formOf (s));
            CHECK (notes > 0);
        }
}

TEST_CASE ("form: drop 1 is at least 10% denser than the groove", "[gen][form]")
{
    // Notes a bar over 64 seeds, every lane, the score: the first drop against every groove section
    // (the groove before it, where lanes keep joining, and the interlude after it). Part 1's drops
    // were no denser than their grooves (FUT01: 50.8 against 50.7).
    for (const char* id : kits)
    {
        std::int64_t grooveNotes = 0, grooveBars = 0, dropNotes = 0, dropBars = 0, thinner = 0;
        for (const Song& s : songsOf (id))
        {
            std::vector<std::int64_t> notes (s.sections.size(), 0);
            for (const LaneOut& lane : s.lanes)
                for (const Note& n : lane.score)
                    ++notes[static_cast<std::size_t> (n.section)];
            std::int64_t gn = 0, gb = 0, dn = 0, db = 0;
            for (const Section& sec : s.sections)
            {
                if (sec.kind == "groove")
                {
                    gn += notes[sec.index];
                    gb += sec.bars;
                }
                if (sec.kind == "drop")
                {
                    dn += notes[sec.index];
                    db += sec.bars;
                }
            }
            REQUIRE (gb > 0);
            REQUIRE (db > 0);
            grooveNotes += gn;
            grooveBars += gb;
            dropNotes += dn;
            dropBars += db;
            thinner += dn * gb < gn * db ? 1 : 0;
        }
        INFO (id << ": the groove " << grooveNotes * 10 / grooveBars << " tenths of a note a bar, drop 1 " << dropNotes * 10 / dropBars << "; " << thinner
                 << " of 64 seeds have a drop 1 thinner than their grooves");
        CHECK (dropNotes * grooveBars * 100 >= grooveNotes * dropBars * 110);
    }
}

TEST_CASE ("form: regions.json lists each region's one-bar dropouts", "[gen][form][export]")
{
    // A dropout is a lane of its block that sits one bar out: regions.json gives its start bar (and the
    // DAW's), its one bar and its lanes, so a producer sees it on the timeline.
    std::size_t listed = 0;
    for (const char* id : kits)
        for (std::uint64_t seed = 0; seed < 8; ++seed)
        {
            const Song& s = songsOf (id)[seed];
            const std::string text = regionsJson (s, Version::quantized);
            const auto parsed = json::parse (text);
            INFO (parsed.error);
            REQUIRE (parsed.ok());
            const json::Value* regions = parsed.value.find ("regions");
            REQUIRE (regions != nullptr);
            REQUIRE (regions->items.size() == s.sections.size());
            for (std::size_t r = 0; r < s.sections.size(); ++r)
            {
                const Section& sec = s.sections[r];
                const json::Value* list = regions->items[r].find ("dropouts");
                INFO (id << " seed " << seed << ", region " << r + 1);
                REQUIRE (list != nullptr);
                // the bars where a lane of the block sits out
                std::vector<std::pair<std::int32_t, std::set<std::string>>> want;
                for (std::int32_t bar = 0; bar < sec.bars; ++bar)
                {
                    std::set<std::string> out;
                    for (std::size_t l = 0; l < s.lanes.size(); ++l)
                        if ((sec.blocks[static_cast<std::size_t> (bar / barsPerBlock)] >> l & 1u) != 0 && !sec.active (l, bar))
                            out.insert (s.lanes[l].id);
                    if (!out.empty())
                        want.emplace_back (bar, out);
                }
                REQUIRE (list->items.size() == want.size());
                for (std::size_t k = 0; k < want.size(); ++k)
                {
                    const json::Value& d = list->items[k];
                    REQUIRE (d.find ("startBar") != nullptr);
                    REQUIRE (d.find ("dawBar") != nullptr);
                    REQUIRE (d.find ("bars") != nullptr);
                    REQUIRE (d.find ("lanes") != nullptr);
                    CHECK (d.find ("startBar")->integer == sec.startBar + want[k].first);
                    CHECK (d.find ("dawBar")->integer == sec.startBar + want[k].first + 1);
                    CHECK (d.find ("bars")->integer == 1);
                    std::set<std::string> lanes;
                    for (const auto& l : d.find ("lanes")->items)
                        lanes.insert (l.string);
                    CHECK (lanes == want[k].second);
                    ++listed;
                }
            }
        }
    CHECK (listed > 0);
}

TEST_CASE ("form: the loader refuses grammars that break a DJ rule, and says where", "[gen][form][content]")
{
    REQUIRE (formError (baseForm).empty());
    // lengths on the 8-bar grid
    CHECK_THAT (formWith ("{ \"bars\": 16, \"w\": 1 }, { \"bars\": 24", "{ \"bars\": 12, \"w\": 1 }, { \"bars\": 24"), ContainsSubstring ("form.sections[1].bars[0].bars"));
    CHECK_THAT (formWith ("{ \"bars\": 16, \"w\": 1 }, { \"bars\": 24", "{ \"bars\": 12, \"w\": 1 }, { \"bars\": 24"), ContainsSubstring ("multiple of 8"));
    // a 32-bar mix-in first, a 32-bar mix-out last, and only there
    CHECK_THAT (formWith ("{ \"kind\": \"intro\", \"bars\"", "{ \"kind\": \"groove\", \"bars\""), ContainsSubstring ("starts with an intro"));
    CHECK_THAT (formWith ("{ \"kind\": \"intro\", \"bars\"", "{ \"kind\": \"groove\", \"bars\""), ContainsSubstring ("form.sections[0].kind"));
    CHECK_THAT (formWith ("{ \"kind\": \"break\", \"bars\"", "{ \"kind\": \"intro\", \"bars\": [{ \"bars\": 32, \"w\": 1 }] }, { \"kind\": \"break\", \"bars\""),
        ContainsSubstring ("comes first and only there"));
    CHECK_THAT (formWith ("{ \"kind\": \"break\", \"bars\"", "{ \"kind\": \"intro\", \"bars\": [{ \"bars\": 32, \"w\": 1 }] }, { \"kind\": \"break\", \"bars\""),
        ContainsSubstring ("form.sections[4].kind"));
    CHECK_THAT (formWith ("\"kind\": \"intro\", \"bars\": [{ \"bars\": 32", "\"kind\": \"intro\", \"bars\": [{ \"bars\": 40"), ContainsSubstring ("32 bars long, not 40"));
    CHECK_THAT (formWith ("{ \"kind\": \"outro\", \"bars\": [{ \"bars\": 32, \"w\": 1 }] }",
                    "{ \"kind\": \"outro\", \"bars\": [{ \"bars\": 32, \"w\": 1 }] }, { \"kind\": \"groove\", \"bars\": [{ \"bars\": 16, \"w\": 1 }] }"),
        ContainsSubstring ("comes last"));
    CHECK_THAT (formWith ("{ \"kind\": \"outro\", \"bars\": [{ \"bars\": 32, \"w\": 1 }] }",
                    "{ \"kind\": \"outro\", \"bars\": [{ \"bars\": 32, \"w\": 1 }] }, { \"kind\": \"groove\", \"bars\": [{ \"bars\": 16, \"w\": 1 }] }"),
        ContainsSubstring ("form.sections[7].kind"));
    CHECK_THAT (formWith ("{ \"kind\": \"outro\", \"bars\"", "{ \"kind\": \"groove\", \"bars\""), ContainsSubstring ("ends with an outro"));
    CHECK_THAT (formWith ("{ \"kind\": \"outro\", \"bars\"", "{ \"kind\": \"groove\", \"bars\""), ContainsSubstring ("form.sections[7].kind"));
    CHECK_THAT (formWith ("\"kind\": \"outro\", \"bars\": [{ \"bars\": 32", "\"kind\": \"outro\", \"bars\": [{ \"bars\": 24"), ContainsSubstring ("32 bars long, not 24"));
    // a build leads into a drop that is never left out
    CHECK_THAT (formWith ("{ \"kind\": \"break\", \"bars\"", "{ \"kind\": \"build\", \"bars\": [{ \"bars\": 8, \"w\": 1 }] }, { \"kind\": \"break\", \"bars\""),
        ContainsSubstring ("a build leads into a drop"));
    CHECK_THAT (formWith ("{ \"kind\": \"drop\", \"bars\": [", "{ \"kind\": \"drop\", \"bars\": [{ \"bars\": 0, \"w\": 1 }, "), ContainsSubstring ("a build leads into a drop"));
    // the mix-in: drums only first, lanes only joining, one at least every 16 bars, 4 blocks
    CHECK_THAT (formWith ("[[\"kick\", \"hats\"], [\"kick\", \"hats\", \"shaker\"]", "[[\"kick\", \"bass\"], [\"kick\", \"hats\", \"shaker\"]"), ContainsSubstring ("drums only"));
    CHECK_THAT (formWith ("[[\"kick\", \"hats\"], [\"kick\", \"hats\", \"shaker\"]", "[[\"kick\", \"bass\"], [\"kick\", \"hats\", \"shaker\"]"),
        ContainsSubstring ("form.plans.intro.choices[0].blocks[0]"));
    CHECK_THAT (formWith ("[\"kick\", \"hats\", \"shaker\"], [\"kick\", \"snare\", \"hats\", \"shaker\"], [\"kick\", \"snare\", \"hats\", \"shaker\", \"bass\"]",
                    "[\"kick\", \"hats\", \"shaker\"], [\"kick\", \"snare\", \"shaker\"], [\"kick\", \"snare\", \"hats\", \"shaker\", \"bass\"]"),
        ContainsSubstring ("\"hats\" leaves here"));
    CHECK_THAT (formWith ("[\"kick\", \"hats\", \"shaker\"], [\"kick\", \"snare\", \"hats\", \"shaker\"], [\"kick\", \"snare\", \"hats\", \"shaker\", \"bass\"]",
                    "[\"kick\", \"hats\", \"shaker\"], [\"kick\", \"snare\", \"shaker\"], [\"kick\", \"snare\", \"hats\", \"shaker\", \"bass\"]"),
        ContainsSubstring ("form.plans.intro.choices[0].blocks[2]"));
    CHECK_THAT (formWith ("[\"kick\", \"snare\", \"hats\", \"shaker\"], [\"kick\", \"snare\", \"hats\", \"shaker\", \"bass\"]] } ] },\n      \"groove\"",
                    "[\"kick\", \"hats\", \"shaker\"], [\"kick\", \"hats\", \"shaker\"]] } ] },\n      \"groove\""),
        ContainsSubstring ("third block in a row"));
    CHECK_THAT (formWith ("[\"kick\", \"snare\", \"hats\", \"shaker\"], [\"kick\", \"snare\", \"hats\", \"shaker\", \"bass\"]] } ] },\n      \"groove\"",
                    "[\"kick\", \"hats\", \"shaker\"], [\"kick\", \"hats\", \"shaker\"]] } ] },\n      \"groove\""),
        ContainsSubstring ("form.plans.intro.choices[0].blocks[3]"));
    CHECK_THAT (formWith (", [\"kick\", \"snare\", \"hats\", \"shaker\", \"bass\"]] } ] },\n      \"groove\"", "] } ] },\n      \"groove\""), ContainsSubstring ("not 24"));
    // the mix-out: lanes only leaving, down to drums only
    CHECK_THAT (formWith ("[\"kick\", \"snare\", \"hats\", \"shaker\", \"perc\"], [\"kick\", \"hats\", \"shaker\"]]",
                    "[\"kick\", \"snare\", \"hats\", \"shaker\", \"perc\", \"chords\"], [\"kick\", \"hats\", \"shaker\"]]"),
        ContainsSubstring ("\"chords\" joins here"));
    CHECK_THAT (formWith ("[\"kick\", \"snare\", \"hats\", \"shaker\", \"perc\"], [\"kick\", \"hats\", \"shaker\"]]",
                    "[\"kick\", \"snare\", \"hats\", \"shaker\", \"perc\", \"chords\"], [\"kick\", \"hats\", \"shaker\"]]"),
        ContainsSubstring ("form.plans.outro.choices[0].blocks[2]"));
    CHECK_THAT (formWith ("[\"kick\", \"hats\", \"shaker\"]] } ] }\n    }", "[\"kick\", \"hats\", \"perc\", \"bass\"]] } ] }\n    }"), ContainsSubstring ("drums only"));
    CHECK_THAT (formWith ("[\"kick\", \"hats\", \"shaker\"]] } ] }\n    }", "[\"kick\", \"hats\", \"perc\", \"bass\"]] } ] }\n    }"),
        ContainsSubstring ("form.plans.outro.choices[0].blocks[3]"));
    // the mix-out takes a lane out at least every 16 bars: never three blocks in a row alike
    CHECK_THAT (formWith ("[\"kick\", \"snare\", \"hats\", \"shaker\", \"perc\", \"bass\"], [\"kick\", \"snare\", \"hats\", \"shaker\", \"perc\"], [\"kick\", \"hats\", \"shaker\"]]",
                    "[\"kick\", \"hats\", \"shaker\"], [\"kick\", \"hats\", \"shaker\"], [\"kick\", \"hats\", \"shaker\"]]"),
        ContainsSubstring ("third block in a row"));
    CHECK_THAT (formWith ("[\"kick\", \"snare\", \"hats\", \"shaker\", \"perc\", \"bass\"], [\"kick\", \"snare\", \"hats\", \"shaker\", \"perc\"], [\"kick\", \"hats\", \"shaker\"]]",
                    "[\"kick\", \"hats\", \"shaker\"], [\"kick\", \"hats\", \"shaker\"], [\"kick\", \"hats\", \"shaker\"]]"),
        ContainsSubstring ("form.plans.outro.choices[0].blocks[3]"));
    // the mix-in's lanes carry on into the section after it
    CHECK_THAT (formWith ("{ \"lanes\": [\"chords\"], \"after\"", "{ \"lanes\": [\"bass\"], \"after\""), ContainsSubstring ("the mix-in ends on \"bass\""));
    // plans: known lanes, entries of the kind's own lanes, one entry per lane, one verb per entry
    CHECK_THAT (formWith ("\"groove\": { \"lanes\": [\"kick\", \"snare\", \"hats\", \"shaker\"", "\"groove\": { \"lanes\": [\"kick\", \"snare\", \"hats\", \"shakr\""),
        ContainsSubstring ("unknown lane \"shakr\""));
    CHECK_THAT (formWith ("{ \"lanes\": [\"bass\"], \"last\"", "{ \"lanes\": [\"kick\"], \"last\""), ContainsSubstring ("\"kick\" isn't one of the break's lanes"));
    CHECK_THAT (formWith ("\"after\": [{ \"bars\": 8, \"w\": 1 }] }]", "\"after\": [{ \"bars\": 8, \"w\": 1 }] }, { \"lanes\": [\"chords\"], \"before\": [{ \"bars\": 8, \"w\": 1 }] }]"),
        ContainsSubstring ("in another entry"));
    CHECK_THAT (formWith ("\"after\": [{ \"bars\": 8, \"w\": 1 }] }]", "\"after\": [{ \"bars\": 8, \"w\": 1 }], \"before\": [{ \"bars\": 8, \"w\": 1 }] }]"),
        ContainsSubstring ("one of \"after\""));
    CHECK_THAT (formWith ("\"change\": true },\n      \"break\"", "\"chnage\": true },\n      \"break\""), ContainsSubstring ("unknown key \"chnage\""));
    // rests: 8 or 16 bars, a lane that plays all through the kind; dropouts before 16- or 32-bar lines
    CHECK_THAT (formWith ("\"rest\": { \"p\": 50, \"bars\": 8,", "\"rest\": { \"p\": 50, \"bars\": 12,"), ContainsSubstring ("8 or 16"));
    CHECK_THAT (formWith ("\"rest\": { \"p\": 40, \"bars\": 8, \"lanes\": [{ \"lane\": \"hats\"", "\"rest\": { \"p\": 40, \"bars\": 8, \"lanes\": [{ \"lane\": \"perc\""),
        ContainsSubstring ("\"perc\" can't rest"));
    CHECK_THAT (formWith ("\"every\": 16,", "\"every\": 24,"), ContainsSubstring ("16- or 32-bar line"));
    CHECK_THAT (formWith ("\"every\": 16,", "\"every\": 8,"), ContainsSubstring ("form.plans.drop.dropout.every"));
    CHECK_THAT (formWith ("\"lanes\": [\"kick\", \"bass\"], \"w\": 1", "\"lanes\": [\"kick\", \"bass\", \"organ\"], \"w\": 1"), ContainsSubstring ("unknown lane \"organ\""));
    // a kind with a lane change inside has room for one at every length it takes
    CHECK_THAT (formWith ("{ \"kind\": \"drop\", \"bars\": [", "{ \"kind\": \"drop\", \"bars\": [{ \"bars\": 8, \"w\": 1 }, "), ContainsSubstring ("no room for a rest or a dropout"));
    // drop 2's layer plays all through drop 2
    CHECK_THAT (formWith ("\"drop2\": { \"lanes\": [\"kick\", \"snare\", \"hats\", \"shaker\", \"perc\",", "\"drop2\": { \"lanes\": [\"kick\", \"snare\", \"hats\", \"shaker\","),
        ContainsSubstring ("\"perc\" isn't one of the drop2's lanes"));
    // every kind has a plan, every plan a kind in the form
    CHECK_THAT (formWith ("\"build\": { \"lanes\": [\"snare\", \"hats\", \"shaker\", \"perc\", \"chords\"] },", ""), ContainsSubstring ("gives it no plan"));
    CHECK_THAT (formWith ("\"build\": { \"lanes\"", "\"bridge\": { \"lanes\": [\"kick\"] }, \"build\": { \"lanes\""), ContainsSubstring ("unknown section kind \"bridge\""));
    // the lengths can always keep the lines the form draws, inside its length
    CHECK_THAT (formWith ("\"bars\": [176, 208]", "\"bars\": [144, 144]"), ContainsSubstring ("no lengths fit 144 to 144 bars"));
    CHECK_THAT (formWith ("\"bars\": [176, 208]", "\"bars\": [176, 210]"), ContainsSubstring ("multiple of 8"));
    // a lane the form plays in a kind its lane has no grid for: the kick in the break
    CHECK_THAT (formWith ("\"break\": { \"lanes\": [\"hats\"", "\"break\": { \"lanes\": [\"kick\", \"hats\""), ContainsSubstring ("no grid for break"));
    // and the shipped grammars keep every rule
    for (const auto& [kit, style] : { std::pair { "fut01", "garage2step" }, std::pair { "fut02", "sciclub" }, std::pair { "fut03", "futurehouse" }, std::pair { "fut04", "techno" } })
    {
        INFO (style);
        CHECK (loadBundle (std::filesystem::path (WUBANSIA_SOURCE_DIR) / "content", std::string (kit)).ok());
    }
}

TEST_CASE ("form: the slot-by-slot fallback keeps the lines and the length range", "[gen][form]")
{
    // formLengths draws the whole form by the weights up to 1024 times, then falls back to drawing
    // slot by slot among the lengths that still fit (arrange.h). The shipped grammars almost never
    // reach the fallback, so these grammars force it: a 128-bar length that never fits outweighs the
    // ones that do a million to one, so every free draw fails.
    SECTION ("one way to fit: ten 8-bar slots in 80 bars, each drop and break on its line")
    {
        FormSpec form;
        form.bars = { 80, 80 };
        for (const char* kind : { "intro", "groove", "break", "build", "drop", "groove", "break", "build", "drop2", "outro" })
            form.slots.push_back ({ kind, { { 8, 1 }, { 16, 1000 } } });
        // slot i starts on bar 8i: on a 16-bar line when i is even
        std::vector<std::int8_t> lines (form.slots.size(), -1);
        for (std::size_t i = 0; i < form.slots.size(); ++i)
            if (startsOnLine (form.slots[i].kind))
                lines[i] = i % 2 == 0 ? 1 : 0;
        CHECK (formFits (form, lines));
        for (std::uint64_t k = 0; k < 64; ++k)
            CHECK (formLengths (form, lines, rng::seedKey (k)) == std::vector<std::int32_t> (10, 8));
        lines[4] = 0; // the drop off its line: no lengths keep it
        CHECK_FALSE (formFits (form, lines));
        CHECK (formLengths (form, lines, rng::seedKey (0)).empty());
    }
    SECTION ("several ways to fit: each length by weight among those that fit")
    {
        FormSpec form;
        form.bars = { 64, 96 };
        form.slots.push_back ({ "intro", { { 32, 1 } } });
        form.slots.push_back ({ "groove", { { 8, 1 }, { 16, 1 }, { 128, 1000000 } } });
        form.slots.push_back ({ "drop", { { 16, 1 }, { 24, 1 }, { 128, 1000000 } } });
        form.slots.push_back ({ "outro", { { 8, 1 }, { 16, 1 }, { 128, 1000000 } } });
        for (const std::int8_t line : { std::int8_t (0), std::int8_t (1) })
        {
            INFO ("the drop " << (line == 1 ? "on a 16-bar line" : "on 8 mod 16"));
            const std::vector<std::int8_t> lines { -1, -1, line, -1 };
            REQUIRE (formFits (form, lines));
            std::set<std::vector<std::int32_t>> forms;
            for (std::uint64_t k = 0; k < 200; ++k)
            {
                const auto lengths = formLengths (form, lines, rng::seedKey (k));
                REQUIRE (lengths.size() == form.slots.size());
                std::int32_t total = 0;
                for (std::size_t i = 0; i < lengths.size(); ++i)
                {
                    CHECK (lengths[i] != 128);
                    CHECK (std::any_of (form.slots[i].bars.begin(), form.slots[i].bars.end(), [&] (const Weighted& w) { return w.value == lengths[i] && w.weight > 0; }));
                    total += lengths[i];
                }
                CHECK (total >= form.bars.lo);
                CHECK (total <= form.bars.hi);
                const std::int32_t dropStart = lengths[0] + lengths[1];
                CHECK ((dropStart % 16 == 0) == (line == 1));
                forms.insert (lengths);
            }
            CHECK (forms.size() == 4); // the drop 16 or 24 bars, the outro 8 or 16: all four fit
        }
        // the fallback's draws are part of every seed's key path (tag "fit"): pinned
        CHECK (formLengths (form, std::vector<std::int8_t> { -1, -1, 0, -1 }, rng::seedKey (0)) == std::vector<std::int32_t> { 32, 8, 24, 8 });
        CHECK (formLengths (form, std::vector<std::int8_t> { -1, -1, 0, -1 }, rng::seedKey (1)) == std::vector<std::int32_t> { 32, 8, 16, 16 });
        CHECK (formLengths (form, std::vector<std::int8_t> { -1, -1, 1, -1 }, rng::seedKey (0)) == std::vector<std::int32_t> { 32, 16, 24, 8 });
        CHECK (formLengths (form, std::vector<std::int8_t> { -1, -1, 1, -1 }, rng::seedKey (1)) == std::vector<std::int32_t> { 32, 16, 16, 16 });
    }
    SECTION ("a line slot that can be left out: left out, or on its line")
    {
        // The slot's own line check decides here, where the slot before can't: a break of 0 or 16
        // bars after a groove of 8 or 16, in 64 to 96 bars, fits either way, but only one groove puts
        // it on its line (a slot that always plays is kept on its line by the slots before it).
        // FUT04's break 2 (0, 16 or 24 bars) is this case on a shipped grammar.
        FormSpec form;
        form.bars = { 64, 96 };
        form.slots.push_back ({ "intro", { { 32, 1 } } });
        form.slots.push_back ({ "groove", { { 8, 1 }, { 16, 1 }, { 128, 1000000 } } });
        form.slots.push_back ({ "break", { { 0, 1 }, { 16, 1 }, { 128, 1000000 } } });
        form.slots.push_back ({ "outro", { { 32, 1 } } });
        for (const std::int8_t line : { std::int8_t (0), std::int8_t (1) })
        {
            INFO ("the break " << (line == 1 ? "on a 16-bar line" : "on 8 mod 16"));
            const std::vector<std::int8_t> lines { -1, -1, line, -1 };
            REQUIRE (formFits (form, lines));
            std::set<std::vector<std::int32_t>> forms;
            for (std::uint64_t k = 0; k < 200; ++k)
            {
                const auto lengths = formLengths (form, lines, rng::seedKey (k));
                REQUIRE (lengths.size() == form.slots.size());
                INFO ("key " << k << ": the groove " << lengths[1] << " bars, the break " << lengths[2]);
                CHECK (lengths[1] != 128);
                CHECK (lengths[2] != 128);
                if (lengths[2] != 0)
                    CHECK (((lengths[0] + lengths[1]) % 16 == 0) == (line == 1));
                const std::int32_t total = lengths[0] + lengths[1] + lengths[2] + lengths[3];
                CHECK (total >= form.bars.lo);
                CHECK (total <= form.bars.hi);
                forms.insert (lengths);
            }
            // the break left out after either groove, or 16 bars after the groove that puts it on its line
            CHECK (forms.size() == 3);
        }
    }
}

TEST_CASE ("form: a shipped grammar reaches the fallback on a real seed, FUT04 seed 304, and keeps its lines and range", "[gen][form]")
{
    // Techno's grammar needs the slot-by-slot fallback in 453 of seeds 0 to 99,999 (the other styles
    // in none), the first of them seed 304. Its 1,024 free draws are replayed here as formLengths
    // makes them (arrange.cpp: each slot by its weights, kept if the drawn lines hold and the total is
    // in range), to show none is kept; the song then plays the fallback's lengths, on its lines and
    // inside techno's 208 to 240 bars. The form is pinned, so a change to the fallback's draws on a
    // shipped grammar shows here.
    const Bundle& b = bundleFor ("FUT04");
    const FormSpec& form = b.style.form;
    const rng::Key kf = songRoot ("FUT04", 304).with (rng::tag ("form"));
    std::vector<std::int8_t> lines (form.slots.size(), -1);
    for (std::size_t i = 0; i < form.slots.size(); ++i)
        if (startsOnLine (form.slots[i].kind))
            lines[i] = rng::chance (kf.with (rng::tag ("line")).with (i), p16FromPercent (form.sixteen)) ? 1 : 0;
    // intro, groove, break, build, drop, break, build, drop2, outro: break 1, break 2 and drop 2 on 8
    // mod 16, the drop on a 16-bar line
    CHECK (lines == std::vector<std::int8_t> { -1, -1, 0, -1, 1, 0, -1, 0, -1 });

    const rng::Key kb = kf.with (rng::tag ("bars"));
    std::int32_t kept = 0;
    for (std::int32_t attempt = 0; attempt < 1024; ++attempt)
    {
        const rng::Key ka = kb.with (rng::tag ("attempt")).with (attempt);
        std::int32_t bars = 0;
        bool onLines = true;
        for (std::size_t i = 0; i < form.slots.size() && onLines; ++i)
        {
            std::vector<std::int32_t> weights;
            for (const Weighted& w : form.slots[i].bars)
                weights.push_back (w.weight);
            const std::int32_t n = form.slots[i].bars[rng::pickWeighted (ka.with (i), weights)].value;
            if (n != 0 && lines[i] >= 0)
                onLines = (bars % 16 == 0) == (lines[i] == 1);
            bars += n;
        }
        kept += onLines && bars >= form.bars.lo && bars <= form.bars.hi ? 1 : 0;
    }
    CHECK (kept == 0);
    const auto lengths = formLengths (form, lines, kb);
    CHECK (lengths == std::vector<std::int32_t> { 32, 40, 24, 32, 32, 0, 8, 32, 32 }); // break 2 left out

    Options o;
    o.seed = 304;
    const auto song = generate (b, o);
    REQUIRE (song.ok());
    const auto& sections = song.value.sections;
    CHECK (song.value.totalBars == 232);
    CHECK (song.value.totalBars >= form.bars.lo);
    CHECK (song.value.totalBars <= form.bars.hi);
    std::size_t at = 0;
    for (std::size_t i = 0; i < lengths.size(); ++i)
    {
        if (lengths[i] == 0)
            continue;
        REQUIRE (at < sections.size());
        INFO ("slot " << i << ", " << sections[at].kind << " at bar " << sections[at].startBar);
        CHECK (sections[at].kind == form.slots[i].kind);
        CHECK (sections[at].bars == lengths[i]);
        if (lines[i] >= 0)
            CHECK ((sections[at].startBar % 16 == 0) == (lines[i] == 1));
        ++at;
    }
    CHECK (at == sections.size());
}

TEST_CASE ("form: dropouts before every 32-bar line, and the dropout a drop with no rest takes for its lane change", "[gen][form]")
{
    // grammar features no shipped style uses
    SECTION ("every 32: a pull before the drop's own 32-bar line only")
    {
        // the rest takes the chords only: no lane of the kick and bass group leaves on the line, so
        // the pull is always drawn (before a line its kick leaves on, none would be)
        std::string form = replaced (baseForm, "\"dropout\": { \"p\": 50, \"every\": 16,", "\"dropout\": { \"p\": 100, \"every\": 32,");
        form = replaced (form, "\"lanes\": [{ \"lane\": \"kick\", \"w\": 1 }, { \"lane\": \"chords\", \"w\": 1 }] }, \"dropout\"", "\"lanes\": [{ \"lane\": \"chords\", \"w\": 1 }] }, \"dropout\"");
        form = replaced (form, "{ \"kind\": \"drop\", \"bars\": [{ \"bars\": 16, \"w\": 1 }, { \"bars\": 24, \"w\": 2 }, { \"bars\": 32, \"w\": 1 }] }",
            "{ \"kind\": \"drop\", \"bars\": [{ \"bars\": 40, \"w\": 1 }, { \"bars\": 48, \"w\": 1 }, { \"bars\": 56, \"w\": 1 }] }");
        form = replaced (form, "\"bars\": [176, 208]", "\"bars\": [176, 240]");
        std::size_t drops = 0;
        for (const Song& s : songsWith (form, 64))
            for (const Section& sec : s.sections)
                if (sec.kind == "drop")
                {
                    INFO ("seed " << s.seed << ": the " << sec.bars << "-bar drop");
                    ++drops;
                    REQUIRE (sec.dropouts.size() == 1);
                    CHECK (sec.dropouts.front().bar == 31);
                }
        CHECK (drops == 64);
    }
    SECTION ("no rest: a drop that draws no change takes a dropout before its first 16-bar line")
    {
        std::string form = replaced (baseForm, "\"rest\": { \"p\": 50, \"bars\": 8, \"lanes\": [{ \"lane\": \"kick\", \"w\": 1 }, { \"lane\": \"chords\", \"w\": 1 }] }, ", "");
        form = replaced (form, "\"dropout\": { \"p\": 50, \"every\": 16,", "\"dropout\": { \"p\": 0, \"every\": 16,");
        form = replaced (form, "{ \"kind\": \"drop\", \"bars\": [{ \"bars\": 16, \"w\": 1 }, { \"bars\": 24, \"w\": 2 }, { \"bars\": 32, \"w\": 1 }] }",
            "{ \"kind\": \"drop\", \"bars\": [{ \"bars\": 24, \"w\": 2 }, { \"bars\": 32, \"w\": 1 }] }");
        std::size_t drops = 0;
        for (const Song& s : songsWith (form, 64))
            for (const Section& sec : s.sections)
                if (sec.kind == "drop")
                {
                    INFO ("seed " << s.seed << ": the " << sec.bars << "-bar drop");
                    ++drops;
                    REQUIRE (sec.dropouts.size() == 1);
                    CHECK (sec.dropouts.front().bar == 15);
                    CHECK (sec.dropouts.front().lanes != 0);
                }
        CHECK (drops == 64);
    }
}

TEST_CASE ("form: drop 2's layer sits out drop 1 and plays all through drop 2", "[gen][form]")
{
    // on a grammar with a layer (baseForm's perc): no shipped style has one
    std::size_t drops = 0, drop2s = 0;
    for (const Song& s : songsWith (baseForm, 32))
    {
        const std::size_t perc = laneIndexOf (s, "perc");
        INFO ("seed " << s.seed << ": " << formOf (s));
        for (const Section& sec : s.sections)
        {
            if (sec.kind == "drop")
            {
                ++drops;
                for (std::int32_t bar = 0; bar < sec.bars; ++bar)
                    CHECK_FALSE (sec.active (perc, bar));
            }
            if (sec.kind == "drop2")
            {
                ++drop2s;
                for (const auto block : sec.blocks)
                    CHECK ((block >> perc & 1u) == 1u);
            }
        }
    }
    CHECK (drops == 32);
    CHECK (drop2s == 32);
}

TEST_CASE ("form: every drop plays its plan's lanes from its first block, and a lane sits out at most once, for its rest", "[gen][form]")
{
    // Pinned on the blocks, so a lane move that runs too long or happens twice fails here (a dropout
    // next to a rest is the dropout's bar, checked on its own: test "every drop and drop 2 has a lane
    // change inside it").
    for (const char* id : kits)
    {
        const FormSpec& form = bundleFor (id).style.form;
        for (const Song& s : songsOf (id))
            for (const Section& sec : s.sections)
            {
                if (!isDrop (sec.kind))
                    continue;
                INFO (id << " seed " << s.seed << ": the " << sec.kind << " at bar " << sec.startBar << " (" << sec.bars << " bars)");
                const KindPlan* plan = form.plan (sec.kind);
                REQUIRE (plan != nullptr);
                std::uint64_t lanes = maskOf (s, plan->lanes), late = 0, entered = 0, layer = 0;
                for (const LaneEntry& e : plan->entries)
                {
                    entered |= maskOf (s, e.lanes);
                    if (e.entry != Entry::before)
                        late |= maskOf (s, e.lanes);
                }
                for (const LaneWeight& lw : form.layer)
                    layer |= std::uint64_t (1) << laneIndexOf (s, lw.lane);
                if (sec.kind == "drop")
                    lanes &= ~layer;
                const std::size_t kick = laneIndexOf (s, "kick");
                // the drop lands with every lane that plays all through it, the kick above all
                CHECK ((sec.blocks.front() & (lanes & ~late)) == (lanes & ~late));
                CHECK ((sec.blocks.front() >> kick & 1u) == 1u);
                CHECK ((sec.lanesAnywhere() & ~lanes) == 0); // nothing but the plan's lanes
                // a lane of its own (no entry) sits out one run of blocks at most: its rest, never the first
                std::size_t rested = 0;
                bool kickRested = false;
                for (std::size_t l = 0; l < s.lanes.size(); ++l)
                {
                    const std::uint64_t bit = std::uint64_t (1) << l;
                    if ((lanes & ~entered & bit) == 0)
                        continue;
                    std::vector<std::size_t> out;
                    for (std::size_t k = 0; k < sec.blocks.size(); ++k)
                        if ((sec.blocks[k] & bit) == 0)
                            out.push_back (k);
                    if (out.empty())
                        continue;
                    INFO (s.lanes[l].id << " sits out blocks " << out.front() << " to " << out.back());
                    ++rested;
                    kickRested = kickRested || l == kick;
                    REQUIRE (plan->rest);
                    CHECK (out.back() - out.front() + 1 == out.size()); // one run
                    CHECK (static_cast<std::int32_t> (out.size()) * barsPerBlock == plan->rest->bars);
                    CHECK (out.front() > 0);
                    CHECK (static_cast<std::int32_t> (out.front()) * barsPerBlock % plan->rest->bars == 0);
                    CHECK (std::any_of (plan->rest->lanes.begin(), plan->rest->lanes.end(), [&] (const LaneWeight& lw) { return lw.lane == s.lanes[l].id; }));
                }
                CHECK (rested <= 1);
                // each dropout takes lanes of one of the plan's groups
                for (const Dropout& d : sec.dropouts)
                {
                    CHECK (d.lanes != 0);
                    REQUIRE (plan->dropout);
                    CHECK (std::any_of (plan->dropout->lanes.begin(), plan->dropout->lanes.end(), [&] (const LaneGroup& g) { return (d.lanes & ~maskOf (s, g.lanes)) == 0; }));
                }
                // so the kick sits out its rest and the bars of the dropouts that take it, nothing more
                std::int32_t quiet = 0, allowed = kickRested ? plan->rest->bars : 0;
                for (std::int32_t bar = 0; bar < sec.bars; ++bar)
                    quiet += sec.active (kick, bar) ? 0 : 1;
                for (const Dropout& d : sec.dropouts)
                    allowed += (d.lanes >> kick & 1u) != 0 ? 1 : 0;
                CHECK (quiet <= allowed);
            }
    }
}

// ---- the drops and the lines between sections, as the kits' written extended mixes play them

TEST_CASE ("form: a drum lane that sits out part of a drop is back for its last 8 bars", "[gen][form]")
{
    // The kick (or the hats) coming back is the cue a dancefloor hears as the drop: it comes back
    // inside the drop, never as the next, quieter section starts. Melodic lanes may strip at the
    // end (FUT01's and FUT03's drop 2, FUT04's drone, as their written mixes do).
    for (const char* id : kits)
        for (const Song& s : songsOf (id))
        {
            std::uint64_t drums = 0;
            for (std::size_t l = 0; l < s.lanes.size(); ++l)
                drums |= isDrums (s, l) ? std::uint64_t (1) << l : 0;
            for (const Section& sec : s.sections)
                if (isDrop (sec.kind))
                {
                    INFO (id << " seed " << s.seed << ": the " << sec.kind << " at bar " << sec.startBar << " (" << sec.bars << " bars)");
                    CHECK ((sec.blocks.front() & drums & ~sec.blocks.back()) == 0);
                }
        }
}

TEST_CASE ("form: every lane a dropout takes out lands back on the line after it", "[gen][form]")
{
    // the one-bar pull's payoff: the lanes slam back on the 16-bar line (a lane whose rest starts on
    // the line plays through the bar and leaves on the line)
    for (const char* id : kits)
        for (const Song& s : songsOf (id))
            for (const Section& sec : s.sections)
                for (const Dropout& d : sec.dropouts)
                {
                    INFO (id << " seed " << s.seed << ": the " << sec.kind << " at bar " << sec.startBar << ", the dropout at its bar " << d.bar);
                    CHECK ((d.lanes & ~sec.lanesAt (d.bar + 1)) == 0);
                }
}

TEST_CASE ("form: one-bar pulls inside a section come at least 32 bars apart", "[gen][form]")
{
    // the same pull before every 16-bar line is an edit on a loop (FUT04's written mix pulls its
    // drums once in drop 1)
    for (const char* id : kits)
        for (const Song& s : songsOf (id))
            for (const Section& sec : s.sections)
                for (std::size_t k = 1; k < sec.dropouts.size(); ++k)
                {
                    INFO (id << " seed " << s.seed << ": the " << sec.kind << " at bar " << sec.startBar);
                    CHECK (sec.dropouts[k].bar - sec.dropouts[k - 1].bar >= 32);
                }
}

TEST_CASE ("form: lanes only leave from the section before the mix-out on, and the mix-out plays drums and the bass only", "[gen][form]")
{
    // A DJ blends the next record over the mix-out: nothing the music let go of comes back there
    // (drums may: the kick or hats back on its downbeat after a rest), and it carries no chords,
    // as in the four written mixes (whose mix-outs are drums only; here the sub leaves in its first
    // 8 or 16 bars).
    for (const char* id : kits)
        for (const Song& s : songsOf (id))
        {
            INFO (id << " seed " << s.seed << ": " << formOf (s));
            std::uint64_t drums = 0, low = 0;
            for (std::size_t l = 0; l < s.lanes.size(); ++l)
            {
                drums |= isDrums (s, l) ? std::uint64_t (1) << l : 0;
                low |= s.lanes[l].kind == LaneKind::bass ? std::uint64_t (1) << l : 0;
            }
            REQUIRE (s.sections.size() >= 2);
            const Section& out = s.sections.back();
            const Section& before = s.sections[s.sections.size() - 2];
            CHECK ((out.blocks.front() & ~before.lanesAt (before.bars - 1) & ~drums) == 0);
            for (const auto block : out.blocks)
                CHECK ((block & ~(drums | low)) == 0);
        }
}

namespace
{
    // baseForm's mix-out choice, up to its first block's lanes, and the same choice opening on the chords
    const std::string outroOpens = "\"outro\": { \"choices\": [ { \"w\": 1, \"blocks\": [[\"kick\", \"snare\", \"hats\", \"shaker\", \"perc\", \"bass\"],";
    const std::string outroOpensOnChords = "\"outro\": { \"choices\": [ { \"w\": 1, \"blocks\": [[\"kick\", \"snare\", \"hats\", \"shaker\", \"perc\", \"bass\", \"chords\"],";
    // two mix-out choices of weight 1: one opening on the chords, then baseForm's
    const std::string twoOutros = "\"outro\": { \"choices\": [ { \"w\": 1, \"blocks\": [[\"kick\", \"snare\", \"hats\", \"shaker\", \"perc\", \"bass\", \"chords\"], "
                                  "[\"kick\", \"snare\", \"hats\", \"shaker\", \"perc\", \"bass\"], [\"kick\", \"snare\", \"hats\", \"shaker\", \"perc\"], [\"kick\", \"hats\", \"shaker\"]] }, "
                                  "{ \"w\": 1, \"blocks\": [[\"kick\", \"snare\", \"hats\", \"shaker\", \"perc\", \"bass\"],";
    const std::string drop2Lengths = "{ \"kind\": \"drop2\", \"bars\": [{ \"bars\": 16, \"w\": 2 }, { \"bars\": 24, \"w\": 1 }, { \"bars\": 32, \"w\": 1 }] }";
    const std::string drop2Long = "{ \"kind\": \"drop2\", \"bars\": [{ \"bars\": 24, \"w\": 1 }, { \"bars\": 32, \"w\": 1 }] }";
    const std::string drop2Rest = "\"rest\": { \"p\": 40, \"bars\": 8, \"lanes\": [{ \"lane\": \"hats\", \"w\": 1 }, { \"lane\": \"chords\", \"w\": 1 }] }";

    // baseForm with drop 2's chords leaving 8 bars before its end in about half the songs (a "before"
    // entry) and resting the hats only, 24 or 32 bars long (room for the hats' rest inside it)
    std::string chordsStripForm()
    {
        return replaced (replaced (baseForm, drop2Lengths, drop2Long), drop2Rest,
            "\"entries\": [{ \"lanes\": [\"chords\"], \"before\": [{ \"bars\": 0, \"w\": 1 }, { \"bars\": 8, \"w\": 1 }] }], \"rest\": { \"p\": 40, \"bars\": 8, \"lanes\": [{ \"lane\": \"hats\", \"w\": 1 }] }");
    }

    // Over 64 seeds of a form whose mix-out can open on the chords: never after a drop 2 that ends
    // without them, and at least once after one that ends with them (the draw isn't just avoiding it).
    void checkMixOutChoice (const std::string& form)
    {
        INFO (formError (form));
        REQUIRE (formError (form).empty());
        std::size_t without = 0, with = 0, opened = 0;
        for (const Song& s : songsWith (form, 64))
        {
            INFO ("seed " << s.seed << ": " << formOf (s));
            const std::uint64_t chords = std::uint64_t (1) << laneIndexOf (s, "chords");
            const Section& out = s.sections.back();
            const Section& before = s.sections[s.sections.size() - 2];
            REQUIRE (before.kind == "drop2");
            const bool opens = (out.blocks.front() & chords) != 0;
            if ((before.lanesAt (before.bars - 1) & chords) == 0)
            {
                ++without;
                CHECK_FALSE (opens);
            }
            else
            {
                ++with;
                opened += opens ? 1u : 0u;
            }
        }
        INFO (without << " drop 2s end without the chords; the mix-out opens on them after " << opened << " of the " << with << " that end with them");
        CHECK (without > 0);
        CHECK (opened > 0);
    }
}

TEST_CASE ("form: the mix-out never opens on a non-drum lane the drop 2 before it ended without", "[gen][form]")
{
    // The draw itself (arrange.cpp's choice weights), on a grammar where a mix-out choice opens on the
    // chords: no shipped mix-out carries a non-drum lane its drop 2 can end without, so only a fixture
    // shows the draw at work.
    SECTION ("drop 2's chords rest through its last block") { checkMixOutChoice (replaced (baseForm, outroOpens, twoOutros)); }
    SECTION ("drop 2's chords leave 8 bars before its end") { checkMixOutChoice (replaced (chordsStripForm(), outroOpens, twoOutros)); }
}

TEST_CASE ("form: the kick never sits out more than 32 bars in a row", "[gen][form]")
{
    // The written mixes never go past 16 kickless bars, part 1's forms never past 32: a break of up
    // to 32 bars, but no kickless build or drop's end stacked onto it.
    for (const char* id : kits)
        for (const Song& s : songsOf (id))
        {
            const std::size_t kick = laneIndexOf (s, "kick");
            std::int32_t run = 0, longest = 0, at = 0, from = 0, longestFrom = 0;
            for (const Section& sec : s.sections)
                for (std::int32_t bar = 0; bar < sec.bars; ++bar, ++at)
                    if (sec.active (kick, bar))
                        run = 0;
                    else
                    {
                        if (run++ == 0)
                            from = at;
                        if (run > longest)
                        {
                            longest = run;
                            longestFrom = from;
                        }
                    }
            INFO (id << " seed " << s.seed << ": " << formOf (s) << "; no kick for " << longest << " bars from bar " << longestFrom + 1);
            CHECK (longest <= 32);
        }
}

TEST_CASE ("form: drop 1 keeps its kick in every bar but its one-bar pulls", "[gen][form]")
{
    // None of the four written mixes rests the first drop's kick: it is the payoff of the build.
    for (const char* id : kits)
        for (const Song& s : songsOf (id))
        {
            const std::size_t kick = laneIndexOf (s, "kick");
            for (const Section& sec : s.sections)
                if (sec.kind == "drop")
                    for (std::int32_t bar = 0; bar < sec.bars; ++bar)
                    {
                        const bool pulled = std::any_of (sec.dropouts.begin(), sec.dropouts.end(), [&] (const Dropout& d) { return d.bar == bar && (d.lanes >> kick & 1u) != 0; });
                        INFO (id << " seed " << s.seed << ": the drop at bar " << sec.startBar << ", its bar " << bar);
                        CHECK ((sec.active (kick, bar) || pulled));
                    }
        }
}

TEST_CASE ("form: drop 2 keeps its hats and the four-on-the-floor kick (FUT03 its shaker), and FUT04's drop 1 its acid, in every bar but their one-bar pulls", "[gen][form]")
{
    // Drop 2 is the peak a DJ lets play before the mix-out: none of the written drop 2s rests its hats
    // (FUT01's and FUT03's are busier than their drop 1's), and FUT02's and FUT03's keep the four on
    // the floor in every bar (FUT01's takes its 2-step kick out for 8 bars, its written move), FUT03's
    // with the shaker's 16ths (its written drops thin only their bongos and rim on alternate lines).
    // FUT04's written drop 1 never rests its acid line, the track's hook. A one-bar pull may take a
    // lane for its bar: it lands back on the line.
    struct Kept
    {
        const char* kit;
        const char* kind;
        const char* lane;
    };
    for (const Kept& k : { Kept { "FUT01", "drop2", "hats" }, Kept { "FUT02", "drop2", "hats" }, Kept { "FUT03", "drop2", "hats" }, Kept { "FUT02", "drop2", "kick" },
             Kept { "FUT03", "drop2", "kick" }, Kept { "FUT03", "drop2", "shaker" }, Kept { "FUT04", "drop", "bass" } })
    {
        std::size_t songs = 0;
        for (const Song& s : songsOf (k.kit))
        {
            const std::size_t li = laneIndexOf (s, k.lane);
            bool kept = true;
            for (const Section& sec : s.sections)
                if (sec.kind == k.kind)
                    for (std::int32_t bar = 0; bar < sec.bars; ++bar)
                    {
                        const bool pulled = std::any_of (sec.dropouts.begin(), sec.dropouts.end(), [&] (const Dropout& d) { return d.bar == bar && (d.lanes >> li & 1u) != 0; });
                        kept = kept && (sec.active (li, bar) || pulled);
                    }
            INFO (k.kit << " seed " << s.seed << ": " << formOf (s) << "; the " << k.lane << " all through the " << k.kind << " but its pulls");
            CHECK (kept);
            songs += kept ? 1u : 0u;
        }
        INFO (k.kit << ": the " << k.lane << " all through the " << k.kind << " in " << songs << " of " << seeds << " songs");
        CHECK (songs == seeds);
    }
}

TEST_CASE ("form: a one-bar pull takes its whole group, so it never sits before a line one of its lanes leaves on", "[gen][form]")
{
    // A pull is its lane group dropping out for a bar and slamming back on the line. Before a line
    // where a lane of its group leaves (the kick's rest starting there) it would take the rest of the
    // group alone, a sub gap that resolves into the kick going: an editing seam. So a pull takes every
    // lane of one of its plan's groups that plays its bar (lanes already out, a rest that ends on the
    // line, aside), and each of them lands back on the line.
    SECTION ("the shipped styles")
    {
        for (const char* id : kits)
        {
            const FormSpec& form = bundleFor (id).style.form;
            for (const Song& s : songsOf (id))
                for (const Section& sec : s.sections)
                    for (const Dropout& d : sec.dropouts)
                    {
                        INFO (id << " seed " << s.seed << ": the " << sec.kind << " at bar " << sec.startBar << ", the pull at its bar " << d.bar);
                        const KindPlan* plan = form.plan (sec.kind);
                        REQUIRE (plan != nullptr);
                        REQUIRE (plan->dropout);
                        const std::uint64_t block = sec.blocks[static_cast<std::size_t> (d.bar / barsPerBlock)];
                        CHECK (std::any_of (plan->dropout->lanes.begin(), plan->dropout->lanes.end(), [&] (const LaneGroup& g) { return d.lanes == (maskOf (s, g.lanes) & block); }));
                        CHECK ((d.lanes & ~sec.lanesAt (d.bar + 1)) == 0);
                    }
        }
    }
    SECTION ("a drop whose kick rests from its 16-bar line has no pull before it")
    {
        // every drop is 32 bars on a 16-bar line, rests its kick for 8 bars, from bar 8 or bar 16, and
        // draws a kick and bass pull before its 16-bar line whenever its group can land back there
        std::string form = replaced (baseForm, "\"rest\": { \"p\": 50, \"bars\": 8, \"lanes\": [{ \"lane\": \"kick\", \"w\": 1 }, { \"lane\": \"chords\", \"w\": 1 }] }, \"dropout\": { \"p\": 50,",
            "\"rest\": { \"p\": 100, \"bars\": 8, \"lanes\": [{ \"lane\": \"kick\", \"w\": 1 }] }, \"dropout\": { \"p\": 100,");
        form = replaced (form, "{ \"kind\": \"drop\", \"bars\": [{ \"bars\": 16, \"w\": 1 }, { \"bars\": 24, \"w\": 2 }, { \"bars\": 32, \"w\": 1 }] }", "{ \"kind\": \"drop\", \"bars\": [{ \"bars\": 32, \"w\": 1 }] }");
        form = replaced (form, "\"sixteen\": 70", "\"sixteen\": 100");
        std::size_t leaving = 0, back = 0;
        for (const Song& s : songsWith (form, 64))
        {
            const std::uint64_t kick = std::uint64_t (1) << laneIndexOf (s, "kick"), bass = std::uint64_t (1) << laneIndexOf (s, "bass");
            for (const Section& sec : s.sections)
                if (sec.kind == "drop")
                {
                    INFO ("seed " << s.seed << ": the drop at bar " << sec.startBar);
                    REQUIRE (sec.bars == 32);
                    if ((sec.blocks[2] & kick) == 0)
                    {
                        ++leaving; // the kick plays bar 15 and leaves on the line: no pull there
                        CHECK (sec.dropouts.empty());
                    }
                    else
                    {
                        REQUIRE ((sec.blocks[1] & kick) == 0); // its rest, bars 8 to 15
                        ++back; // the kick sits bar 15 out and comes back on the line, with the bass
                        REQUIRE (sec.dropouts.size() == 1);
                        CHECK (sec.dropouts.front().bar == 15);
                        CHECK (sec.dropouts.front().lanes == bass);
                    }
                }
        }
        INFO (leaving << " drops rest the kick from the line, " << back << " up to it");
        CHECK (leaving > 0);
        CHECK (back > 0);
    }
}

TEST_CASE ("form: drops run 24 bars or more, drop 2 32 or more, and in FUT01-FUT03 never shorter than drop 1", "[gen][form]")
{
    // The lengths only (what drop 2 keeps is pinned by "drop 2 keeps its hats and the four-on-the-
    // floor kick ..."): in all four written mixes the last drop is at least as long as the first
    // (40/32, 32/32, 32/32, 32/32). A 16-bar drop spends half itself on its lane change. Techno's
    // long first drops (48, 64) can outrun its drop 2.
    for (const char* id : kits)
        for (const Song& s : songsOf (id))
        {
            INFO (id << " seed " << s.seed << ": " << formOf (s));
            std::int32_t drop = 0, drop2 = 0;
            for (const Section& sec : s.sections)
            {
                drop = sec.kind == "drop" ? sec.bars : drop;
                drop2 = sec.kind == "drop2" ? sec.bars : drop2;
            }
            CHECK (drop >= 24);
            CHECK (drop2 >= 32);
            if (std::string_view (id) != "FUT04")
                CHECK (drop2 >= drop);
        }
}

TEST_CASE ("form: no bass or melodic lane plays a break's last bar, sits out the build after it and comes back with the drop", "[gen][form]")
{
    // The drop's payoff (the sub above all) never arrives early and is then taken away: the written
    // mixes keep the bass out of their garage, club and house breaks. (A drum lane may: FUT03's
    // plings ring in the break's second half and come back with the drop.)
    for (const char* id : kits)
        for (const Song& s : songsOf (id))
        {
            std::uint64_t drums = 0;
            for (std::size_t l = 0; l < s.lanes.size(); ++l)
                drums |= isDrums (s, l) ? std::uint64_t (1) << l : 0;
            for (std::size_t i = 0; i + 2 < s.sections.size(); ++i)
            {
                const Section& brk = s.sections[i];
                const Section& build = s.sections[i + 1];
                const Section& drop = s.sections[i + 2];
                if (brk.kind != "break" || build.kind != "build" || !isDrop (drop.kind))
                    continue;
                std::uint64_t inBuild = 0;
                for (std::int32_t bar = 0; bar < build.bars; ++bar)
                    inBuild |= build.lanesAt (bar);
                INFO (id << " seed " << s.seed << ": the break at bar " << brk.startBar);
                CHECK ((brk.lanesAt (brk.bars - 1) & ~inBuild & drop.lanesAt (0) & ~drums) == 0);
            }
        }
}

TEST_CASE ("form: a drop opens with every non-drum lane that played into it", "[gen][form]")
{
    // a hook that plays through the break and the build is there on the drop's downbeat
    for (const char* id : kits)
        for (const Song& s : songsOf (id))
            for (std::size_t i = 1; i < s.sections.size(); ++i)
            {
                const Section& sec = s.sections[i];
                if (!isDrop (sec.kind))
                    continue;
                const Section& before = s.sections[i - 1];
                std::uint64_t drums = 0;
                for (std::size_t l = 0; l < s.lanes.size(); ++l)
                    drums |= isDrums (s, l) ? std::uint64_t (1) << l : 0;
                INFO (id << " seed " << s.seed << ": the " << sec.kind << " at bar " << sec.startBar);
                CHECK ((before.lanesAt (before.bars - 1) & ~sec.lanesAt (0) & ~drums) == 0);
            }
}

TEST_CASE ("form: garage and house builds keep the kick through, and house brings its bass back for the first half of a 16-bar build", "[gen][form]")
{
    // FUT01's written build keeps the 2-step kick on (beat 1 and the and of 3 on the build grid);
    // FUT03's keeps the four on the floor and brings the bass line back for its first 8 bars.
    for (const char* id : { "FUT01", "FUT03" })
        for (const Song& s : songsOf (id))
        {
            const std::size_t kick = laneIndexOf (s, "kick"), bass = laneIndexOf (s, "bass");
            for (const Section& sec : s.sections)
            {
                if (sec.kind != "build")
                    continue;
                INFO (id << " seed " << s.seed << ": the " << sec.bars << "-bar build at bar " << sec.startBar);
                for (std::int32_t bar = 0; bar < sec.bars; ++bar)
                    CHECK (sec.active (kick, bar));
                if (std::string_view (id) == "FUT03")
                    for (std::int32_t bar = 0; bar < sec.bars; ++bar)
                        CHECK (sec.active (bass, bar) == (sec.bars == 16 && bar < 8));
            }
        }
}

TEST_CASE ("form: the groove after drop 1 plays new patterns, not groove 1's opening bars", "[gen][form]")
{
    // A lane's pattern is drawn per grid and variant: the groove after drop 1 starts on a variant of
    // its own, so it doesn't replay groove 1 bar for bar. Measured on the bass (the kick may be four
    // on the floor, the same in every pattern). And FUT03's stab plays in some of them, as its
    // written switch-up holds the stab under its riff.
    for (const char* id : { "FUT01", "FUT02", "FUT03" })
    {
        std::size_t compared = 0, same = 0, stabs = 0;
        for (const Song& s : songsOf (id))
        {
            const Section* first = nullptr;
            const Section* after = nullptr;
            for (std::size_t i = 0; i < s.sections.size(); ++i)
            {
                if (s.sections[i].kind == "groove" && first == nullptr)
                    first = &s.sections[i];
                if (s.sections[i].kind == "groove" && i > 0 && s.sections[i - 1].kind == "drop")
                    after = &s.sections[i];
            }
            if (first == nullptr || after == nullptr)
                continue;
            const std::size_t bass = laneIndexOf (s, "bass");
            const auto barNotes = [&] (const Section& sec, std::int32_t bar) {
                std::set<std::pair<std::int64_t, int>> out;
                for (const Note& n : s.lanes[bass].score)
                    if (n.tick >= (sec.startBar + bar) * ticksPerBar && n.tick < (sec.startBar + bar + 1) * ticksPerBar)
                        out.insert ({ n.step % 16, n.key });
                return out;
            };
            for (std::int32_t bar = 0; bar < std::min ({ 8, first->bars, after->bars }); ++bar)
                if (first->active (bass, bar) && after->active (bass, bar))
                {
                    ++compared;
                    same += barNotes (*first, bar) == barNotes (*after, bar) ? 1u : 0u;
                }
            stabs += (after->lanesAnywhere() >> laneIndexOf (s, "chords") & 1u) != 0 ? 1u : 0u;
        }
        INFO (id << ": " << same << " of " << compared << " bass bars the same as groove 1's");
        REQUIRE (compared > 100);
        CHECK (same * 4 <= compared);
        if (std::string_view (id) == "FUT03")
            CHECK (stabs > 0);
    }
}

TEST_CASE ("form: the loader keeps lanes carrying on across the mix-in and mix-out, the kick at both edges, and room for every drop's change", "[gen][form][content]")
{
    REQUIRE (formError (baseForm).empty());
    // a "before" entry long enough to keep a lane the mix-in ends on out of the section after it
    CHECK_THAT (formWith ("{ \"lanes\": [\"chords\"], \"after\": [{ \"bars\": 8, \"w\": 1 }] }]",
                    "{ \"lanes\": [\"chords\"], \"after\": [{ \"bars\": 8, \"w\": 1 }] }, { \"lanes\": [\"bass\"], \"before\": [{ \"bars\": 16, \"w\": 1 }] }]"),
        ContainsSubstring ("the mix-in ends on \"bass\""));
    CHECK (formWith ("{ \"lanes\": [\"chords\"], \"after\": [{ \"bars\": 8, \"w\": 1 }] }]",
               "{ \"lanes\": [\"chords\"], \"after\": [{ \"bars\": 8, \"w\": 1 }] }, { \"lanes\": [\"bass\"], \"before\": [{ \"bars\": 8, \"w\": 1 }] }]")
               .empty()); // it plays the groove's first 8 bars at least
    // drop 2's layer, which drop 1 never plays, right after the mix-in
    const std::string layerAfterIntro = R"({
    "bars": [128, 176], "sixteen": 100,
    "sections": [
      { "kind": "intro", "bars": [{ "bars": 32, "w": 1 }] },
      { "kind": "drop", "bars": [{ "bars": 24, "w": 1 }, { "bars": 32, "w": 1 }] },
      { "kind": "break", "bars": [{ "bars": 16, "w": 1 }, { "bars": 24, "w": 1 }] },
      { "kind": "build", "bars": [{ "bars": 8, "w": 1 }, { "bars": 16, "w": 1 }] },
      { "kind": "drop2", "bars": [{ "bars": 24, "w": 1 }, { "bars": 32, "w": 1 }] },
      { "kind": "outro", "bars": [{ "bars": 32, "w": 1 }] }
    ],
    "layer": [{ "lane": "perc", "w": 1 }],
    "plans": {
      "intro": { "choices": [ { "w": 1, "blocks": [["kick", "hats"], ["kick", "hats", "shaker"], ["kick", "snare", "hats", "shaker"], ["kick", "snare", "hats", "shaker", "perc"]] } ] },
      "drop": { "lanes": ["kick", "snare", "hats", "shaker", "perc", "bass", "chords"], "rest": { "p": 50, "bars": 8, "lanes": [{ "lane": "chords", "w": 1 }] }, "change": true },
      "break": { "lanes": ["hats", "chords"] },
      "build": { "lanes": ["snare", "hats", "shaker", "perc", "chords"] },
      "drop2": { "lanes": ["kick", "snare", "hats", "shaker", "perc", "bass", "chords"], "rest": { "p": 40, "bars": 8, "lanes": [{ "lane": "chords", "w": 1 }] }, "change": true },
      "outro": { "choices": [ { "w": 1, "blocks": [["kick", "snare", "hats", "shaker", "perc", "bass"], ["kick", "snare", "hats", "shaker", "perc"], ["kick", "hats", "shaker", "perc"], ["kick", "hats"]] } ] }
    }
  })";
    CHECK_THAT (formError (layerAfterIntro), ContainsSubstring ("drop 2's layer, which drop 1 never plays"));
    CHECK_THAT (formError (layerAfterIntro), ContainsSubstring ("form.layer"));
    CHECK (formError (replaced (layerAfterIntro, "\"layer\": [{ \"lane\": \"perc\", \"w\": 1 }],", "")).empty());
    // lanes only leave from the section before the mix-out on: drop 2 can end without its chords (a
    // rest that takes its last block), so a mix-out that opens on them can't be the only choice
    CHECK_THAT (formWith ("\"outro\": { \"choices\": [ { \"w\": 1, \"blocks\": [[\"kick\", \"snare\", \"hats\", \"shaker\", \"perc\", \"bass\"],",
                    "\"outro\": { \"choices\": [ { \"w\": 1, \"blocks\": [[\"kick\", \"snare\", \"hats\", \"shaker\", \"perc\", \"bass\", \"chords\"],"),
        ContainsSubstring ("lanes only leave from the section before the mix-out on"));
    CHECK_THAT (formWith ("\"outro\": { \"choices\": [ { \"w\": 1, \"blocks\": [[\"kick\", \"snare\", \"hats\", \"shaker\", \"perc\", \"bass\"],",
                    "\"outro\": { \"choices\": [ { \"w\": 1, \"blocks\": [[\"kick\", \"snare\", \"hats\", \"shaker\", \"perc\", \"bass\", \"chords\"],"),
        ContainsSubstring ("form.plans.outro.choices"));
    // ...and every other way a section before the mix-out can end without a lane: a "before" entry,
    // an "after" entry long enough to sit the section out, a lane the section doesn't play, and a
    // section the mix-out can come straight after because the one between can be left out
    const std::string strip = chordsStripForm();
    const std::string lateChords = replaced (replaced (baseForm, drop2Lengths, drop2Long), drop2Rest,
        "\"entries\": [{ \"lanes\": [\"chords\"], \"after\": [{ \"bars\": 8, \"w\": 1 }, { \"bars\": 24, \"w\": 1 }] }], \"rest\": { \"p\": 40, \"bars\": 8, \"lanes\": [{ \"lane\": \"hats\", \"w\": 1 }] }");
    const std::string noChords = replaced (replaced (replaced (baseForm, drop2Lengths, drop2Long), drop2Rest, "\"rest\": { \"p\": 40, \"bars\": 8, \"lanes\": [{ \"lane\": \"hats\", \"w\": 1 }] }"),
        "\"drop2\": { \"lanes\": [\"kick\", \"snare\", \"hats\", \"shaker\", \"perc\", \"bass\", \"chords\"]", "\"drop2\": { \"lanes\": [\"kick\", \"snare\", \"hats\", \"shaker\", \"perc\", \"bass\"]");
    const std::string optionalGroove = replaced (baseForm, "{ \"kind\": \"outro\", \"bars\"", "{ \"kind\": \"groove\", \"bars\": [{ \"bars\": 0, \"w\": 1 }, { \"bars\": 16, \"w\": 1 }] }, { \"kind\": \"outro\", \"bars\"");
    for (const auto& [what, form] : { std::pair { "a before entry", strip }, std::pair { "an after entry", lateChords }, std::pair { "a lane drop 2 doesn't play", noChords },
             std::pair { "an optional groove before the mix-out", optionalGroove } })
    {
        INFO (what);
        CHECK (formError (form).empty());
        CHECK_THAT (formError (replaced (form, outroOpens, outroOpensOnChords)), ContainsSubstring ("lanes only leave from the section before the mix-out on"));
        CHECK_THAT (formError (replaced (form, outroOpens, outroOpensOnChords)), ContainsSubstring ("form.plans.outro.choices"));
        CHECK (formError (replaced (form, outroOpens, twoOutros)).empty()); // a choice without them is enough
    }
    // the mix-in starts and the mix-out ends on the kick a DJ mixes on, never on silence
    CHECK_THAT (formWith ("[[\"kick\", \"hats\"], [\"kick\", \"hats\", \"shaker\"]", "[[], [\"kick\", \"hats\", \"shaker\"]"), ContainsSubstring ("first block is empty"));
    CHECK_THAT (formWith ("[[\"kick\", \"hats\"], [\"kick\", \"hats\", \"shaker\"]", "[[], [\"kick\", \"hats\", \"shaker\"]"), ContainsSubstring ("form.plans.intro.choices[0].blocks[0]"));
    CHECK_THAT (formWith ("[[\"kick\", \"hats\"], [\"kick\", \"hats\", \"shaker\"]", "[[\"hats\"], [\"kick\", \"hats\", \"shaker\"]"), ContainsSubstring ("a DJ mixes on, \"kick\""));
    CHECK_THAT (formWith ("[\"kick\", \"hats\", \"shaker\"]] } ] }\n    }", "[]] } ] }\n    }"), ContainsSubstring ("last block is empty"));
    CHECK_THAT (formWith ("[\"kick\", \"hats\", \"shaker\"]] } ] }\n    }", "[\"hats\", \"shaker\"]] } ] }\n    }"), ContainsSubstring ("a DJ mixes on, \"kick\""));
    CHECK_THAT (formWith ("[\"kick\", \"hats\", \"shaker\"]] } ] }\n    }", "[\"hats\", \"shaker\"]] } ] }\n    }"), ContainsSubstring ("form.plans.outro.choices[0].blocks[3]"));
    // a drum lane rests inside its section, never through its last block: a 16-bar drop 2 whose only
    // rest lane is the hats, and no dropout, has no room for its change
    CHECK_THAT (formWith ("\"rest\": { \"p\": 40, \"bars\": 8, \"lanes\": [{ \"lane\": \"hats\", \"w\": 1 }, { \"lane\": \"chords\", \"w\": 1 }] }",
                    "\"rest\": { \"p\": 40, \"bars\": 8, \"lanes\": [{ \"lane\": \"hats\", \"w\": 1 }] }"),
        ContainsSubstring ("a 16-bar one has no room for a rest or a dropout"));
    CHECK_THAT (formWith ("\"rest\": { \"p\": 40, \"bars\": 8, \"lanes\": [{ \"lane\": \"hats\", \"w\": 1 }, { \"lane\": \"chords\", \"w\": 1 }] }",
                    "\"rest\": { \"p\": 40, \"bars\": 8, \"lanes\": [{ \"lane\": \"hats\", \"w\": 1 }] }"),
        ContainsSubstring ("form.sections[6].bars[0].bars"));
    // a slot can start its section on its own pattern variant, 0 to 15
    CHECK (formWith ("{ \"kind\": \"break\", \"bars\"", "{ \"kind\": \"break\", \"variant\": 12, \"bars\"").empty());
    CHECK_THAT (formWith ("{ \"kind\": \"break\", \"bars\"", "{ \"kind\": \"break\", \"variant\": 16, \"bars\""), ContainsSubstring ("form.sections[4].variant"));
}
