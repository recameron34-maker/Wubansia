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
//   the groove;
// - the loader refuses grammars that could draw a form breaking a DJ rule, and says where.

#include "gen/content.h"
#include "gen/export.h"
#include "gen/json.h"
#include "gen/song.h"
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
      "drop": { "lanes": ["kick", "snare", "hats", "shaker", "perc", "bass", "chords"], "rest": { "p": 50, "bars": 8, "lanes": [{ "lane": "kick", "w": 1 }] }, "dropout": { "p": 50, "every": 16, "lanes": [{ "lanes": ["kick", "bass"], "w": 1 }] }, "change": true },
      "break": { "lanes": ["hats", "perc", "bass", "chords"], "entries": [{ "lanes": ["bass"], "last": [{ "bars": 8, "w": 1 }] }] },
      "drop2": { "lanes": ["kick", "snare", "hats", "shaker", "perc", "bass", "chords"], "rest": { "p": 40, "bars": 8, "lanes": [{ "lane": "hats", "w": 1 }] }, "change": true },
      "outro": { "choices": [ { "w": 1, "blocks": [["kick", "snare", "hats", "shaker", "perc", "bass", "chords"], ["kick", "snare", "hats", "shaker", "perc", "bass"], ["kick", "snare", "hats", "shaker", "perc"], ["kick", "hats", "shaker"]] } ] }
    }
  })";

    struct Texts
    {
        std::string kit, style, feel;
    };

    // The error the garage2step bundle gives with this form, or "" if it loads.
    std::string formError (const std::string& form)
    {
        const auto content = std::filesystem::path (WUBANSIA_SOURCE_DIR) / "content";
        auto kit = parseKit (readText (content / "kits" / "fut01.json"), "kit.json");
        REQUIRE (kit.ok());
        auto style = parseStyle (withForm (readText (content / "styles" / "garage2step.json"), form), "style.json");
        if (!style.ok())
            return style.error;
        auto feel = parseFeel (readText (content / "feels" / "garage2step.json"), "feel.json");
        REQUIRE (feel.ok());
        auto b = makeBundle (std::move (kit.value), std::move (style.value), std::move (feel.value));
        return b.ok() ? std::string() : b.error;
    }

    std::string formWith (std::string_view from, std::string_view to) { return formError (replaced (baseForm, from, to)); }
}

TEST_CASE ("form: every seed draws a DJ-friendly extended mix of 200 to 240 bars", "[gen][form]")
{
    for (const char* id : kits)
        for (const Song& s : songsOf (id))
        {
            INFO (id << " seed " << s.seed << ": " << formOf (s));
            // sections on 8-bar lines, tiling the song: 200 to 240 bars, 6 to 7.5 minutes at the kits' tempos
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

TEST_CASE ("form: drop 2 adds a layer drop 1 lacks, where the style has one", "[gen][form]")
{
    // FUT02's laser zaps sit out the first drop, and drop 2 brings them in on their 3/16 roll; FUT04's
    // snare and clap sit out the first drop, and drop 2 brings the full backbeat.
    for (const auto& [id, layer] : { std::pair { "FUT02", "perc" }, std::pair { "FUT04", "snare" } })
        for (const Song& s : songsOf (id))
        {
            const std::size_t li = laneIndexOf (s, layer);
            INFO (id << " seed " << s.seed << ": " << formOf (s));
            for (const Section& sec : s.sections)
            {
                if (sec.kind == "drop")
                    for (std::int32_t bar = 0; bar < sec.bars; ++bar)
                        CHECK_FALSE (sec.active (li, bar));
                if (sec.kind == "drop2")
                    for (std::size_t k = 0; k < sec.blocks.size(); ++k)
                        CHECK ((sec.blocks[k] >> li & 1u) == 1u);
            }
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
    CHECK_THAT (formWith ("\"kind\": \"intro\", \"bars\": [{ \"bars\": 32", "\"kind\": \"intro\", \"bars\": [{ \"bars\": 40"), ContainsSubstring ("32 bars long, not 40"));
    CHECK_THAT (formWith ("{ \"kind\": \"outro\", \"bars\": [{ \"bars\": 32, \"w\": 1 }] }",
                    "{ \"kind\": \"outro\", \"bars\": [{ \"bars\": 32, \"w\": 1 }] }, { \"kind\": \"groove\", \"bars\": [{ \"bars\": 16, \"w\": 1 }] }"),
        ContainsSubstring ("comes last"));
    CHECK_THAT (formWith ("\"kind\": \"outro\", \"bars\": [{ \"bars\": 32", "\"kind\": \"outro\", \"bars\": [{ \"bars\": 24"), ContainsSubstring ("32 bars long, not 24"));
    // a build leads into a drop that is never left out
    CHECK_THAT (formWith ("{ \"kind\": \"break\", \"bars\"", "{ \"kind\": \"build\", \"bars\": [{ \"bars\": 8, \"w\": 1 }] }, { \"kind\": \"break\", \"bars\""),
        ContainsSubstring ("a build leads into a drop"));
    CHECK_THAT (formWith ("{ \"kind\": \"drop\", \"bars\": [", "{ \"kind\": \"drop\", \"bars\": [{ \"bars\": 0, \"w\": 1 }, "), ContainsSubstring ("a build leads into a drop"));
    // the mix-in: drums only first, lanes only joining, one at least every 16 bars, 4 blocks
    CHECK_THAT (formWith ("[[\"kick\", \"hats\"], [\"kick\", \"hats\", \"shaker\"]", "[[\"kick\", \"bass\"], [\"kick\", \"hats\", \"shaker\"]"), ContainsSubstring ("drums only"));
    CHECK_THAT (formWith ("[\"kick\", \"hats\", \"shaker\"], [\"kick\", \"snare\", \"hats\", \"shaker\"], [\"kick\", \"snare\", \"hats\", \"shaker\", \"bass\"]",
                    "[\"kick\", \"hats\", \"shaker\"], [\"kick\", \"snare\", \"shaker\"], [\"kick\", \"snare\", \"hats\", \"shaker\", \"bass\"]"),
        ContainsSubstring ("\"hats\" leaves here"));
    CHECK_THAT (formWith ("[\"kick\", \"snare\", \"hats\", \"shaker\"], [\"kick\", \"snare\", \"hats\", \"shaker\", \"bass\"]] } ] },\n      \"groove\"",
                    "[\"kick\", \"hats\", \"shaker\"], [\"kick\", \"hats\", \"shaker\"]] } ] },\n      \"groove\""),
        ContainsSubstring ("third block in a row"));
    CHECK_THAT (formWith (", [\"kick\", \"snare\", \"hats\", \"shaker\", \"bass\"]] } ] },\n      \"groove\"", "] } ] },\n      \"groove\""), ContainsSubstring ("not 24"));
    // the mix-out: lanes only leaving, down to drums only
    CHECK_THAT (formWith ("[\"kick\", \"snare\", \"hats\", \"shaker\", \"perc\"], [\"kick\", \"hats\", \"shaker\"]]",
                    "[\"kick\", \"snare\", \"hats\", \"shaker\", \"perc\", \"chords\"], [\"kick\", \"hats\", \"shaker\"]]"),
        ContainsSubstring ("\"chords\" joins here"));
    CHECK_THAT (formWith ("[\"kick\", \"hats\", \"shaker\"]] } ] }\n    }", "[\"kick\", \"hats\", \"perc\", \"bass\"]] } ] }\n    }"), ContainsSubstring ("drums only"));
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
