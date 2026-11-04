// Every metric on the audit's fixture files gives the answer worked out by hand in
// tools/legacy_audit/fixtures/make_fixtures.js (fixtures/expected.json). The Node audit's own test
// (tools/legacy_audit/test/audit.test.js) checks the same answers, case for case, so the C++ port
// and the audit are held to one set of answers without the legacy files.

#include "metrics/metrics.h"
#include "metrics/value.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <initializer_list>
#include <set>
#include <string>
#include <string_view>
#include <vector>

using namespace wubansia::metrics;

namespace
{
    const std::filesystem::path fixtureDir = std::filesystem::path (WUBANSIA_SOURCE_DIR) / "tools" / "legacy_audit" / "fixtures";

    const Value& answers()
    {
        static const Value a = [] {
            auto r = readJsonFile (fixtureDir / "expected.json");
            INFO (r.error);
            REQUIRE (r.ok());
            return r.value;
        }();
        return a;
    }

    // Loaded once each, under the path the audit's test uses ("fixtures/<name>").
    const Song& fixture (const std::string& name)
    {
        static std::vector<std::pair<std::string, Song>> cache;
        for (const auto& [n, s] : cache)
            if (n == name)
                return s;
        auto r = loadSongFile (fixtureDir / name, "fixtures/" + name);
        INFO (r.error);
        REQUIRE (r.ok());
        cache.emplace_back (name, std::move (r.value));
        return cache.back().second;
    }

    std::vector<const Song*> fixtures (const Value& names)
    {
        std::vector<const Song*> out;
        for (const Value& n : names.items())
            fixture (n.string());
        for (const Value& n : names.items())
            out.push_back (&fixture (n.string())); // after loading: the cache doesn't move any more
        return out;
    }

    std::vector<const Song*> one (const std::string& name)
    {
        fixture (name);
        return { &fixture (name) };
    }

    // The members of v named in keys, as an object.
    Value pick (const Value& v, std::initializer_list<std::string_view> keys)
    {
        Value out = Value::object();
        for (const auto k : keys)
            out.set (k, v[k]);
        return out;
    }

    void report (const Comparison& c)
    {
        for (const auto& d : c.differences)
            UNSCOPED_INFO (d);
        CHECK (c.ok());
    }

    // actual equals expected (minus the keys skipped), with no other members: the audit test's deepEqual.
    void checkExact (const Value& actual, const Value& expected, std::initializer_list<std::string_view> skip = {})
    {
        Value e = Value::object();
        for (const auto& [k, v] : expected.members())
            if (std::find (skip.begin(), skip.end(), k) == skip.end())
                e.set (k, v);
        report (compare (actual, e));
    }

    // The members of actual that expected names (minus the keys skipped) equal expected's: the audit
    // test's deepEqual (pick (r, keys), ...).
    void checkPicked (const Value& actual, const Value& expected, std::initializer_list<std::string_view> skip = {})
    {
        Value e = Value::object();
        Value a = Value::object();
        for (const auto& [k, v] : expected.members())
        {
            if (std::find (skip.begin(), skip.end(), k) != skip.end())
                continue;
            e.set (k, v);
            a.set (k, actual[k]);
        }
        report (compare (a, e));
    }

    std::string pathOf (const std::string& name) { return "fixtures/" + name; }
}

TEST_CASE ("every fixture file is used by a test", "[metrics][fixtures]")
{
    std::set<std::string> named = { "reader.mid" }; // test_metrics_model.cpp reads it
    const std::function<void (const Value&)> walk = [&] (const Value& v) {
        if (v.isString() && v.string().size() > 4 && v.string().ends_with (".mid"))
            named.insert (v.string());
        for (const auto& item : v.items())
            walk (item);
        for (const auto& [k, m] : v.members())
            walk (m);
    };
    walk (answers());
    std::set<std::string> present;
    for (const auto& e : std::filesystem::directory_iterator (fixtureDir))
        if (e.path().extension() == ".mid")
            present.insert (e.path().filename().string());
    CHECK (present.size() == 28);
    CHECK (present == named);
}

TEST_CASE ("G1 progression shapes on the harmony fixtures", "[metrics][fixtures]")
{
    const Value& e = answers()["g1"];
    const auto songs = fixtures (e["files"]);
    const Value r = g1 (songs);
    for (const Value& f : e["files"].items())
    {
        INFO (f.string());
        const Value& shapes = e["shapes"][f.string()];
        CHECK (r["per_file"][pathOf (f.string())]["break"].string() == shapes["break"].string());
        CHECK (r["per_file"][pathOf (f.string())]["drop"].string() == shapes["drop"].string());
        CHECK (progressionShape (fixture (f.string()), "break") == shapes["break"].string());
    }
    checkPicked (r["break"], e["break"]);
    checkPicked (r["drop"], e["drop"]);
    CHECK (r.num ("pass") == e.num ("pass"));
    CHECK_FALSE (progressionShape (fixture ("lead.mid"), "break").has_value()); // no markers, no break
}

TEST_CASE ("G2 harmonic rhythm on the harmony fixtures", "[metrics][fixtures]")
{
    const Value& e = answers()["g2"];
    const auto songs = fixtures (e["files"]);
    const Value r = g2 (songs);
    for (const Value& f : e["files"].items())
    {
        INFO (f.string());
        CHECK (r["per_file"][pathOf (f.string())].string() == e["patterns"][f.string()].string());
    }
    checkPicked (r, pick (e, { "seeds", "measured", "four_by_one_bar", "four_by_one_bar_pm", "changes_on_bar_lines_only", "pass" }));
    CHECK (r.num ("patterns") == e.num ("patterns_count"));
}

TEST_CASE ("G3 one-bar repetition, with four-on-the-floor kick windows exempt", "[metrics][fixtures]")
{
    const Value& e = answers()["g3"];
    checkPicked (g3 (one (e["file"].string())), e, { "file" });
}

TEST_CASE ("G4 macro changes on phrase bars", "[metrics][fixtures]")
{
    const Value& e = answers()["g4"];
    const Song& s = fixture (e["file"].string());
    const Song* const list[] = { &s };
    const Value r = g4 (list);
    for (const char* role : { "bass", "chords", "lead" })
    {
        INFO (role);
        checkExact (r[role], e[role]);
    }
    const auto bars = macroChangeBars (s);
    const auto barsOf = [] (const std::vector<MacroChange>& v) {
        Value out = Value::array();
        for (const auto& c : v)
            out.push (Value::of (c.bar));
        return out;
    };
    CHECK (compare (barsOf (bars.bass), e["change_bars"]["bass"]).ok());
    CHECK (compare (barsOf (bars.chords), e["change_bars"]["chords"]).ok());
    CHECK (compare (barsOf (bars.lead), e["change_bars"]["lead"]).ok());
    CHECK (r.num ("pass") == e.num ("pass"));
}

TEST_CASE ("G4 does not count an 8-bar loop as changes", "[metrics][fixtures]")
{
    const Value& e = answers()["g4_loop8"];
    const Song& s = fixture (e["file"].string());
    const Song* const list[] = { &s };
    checkPicked (g4 (list)["lead"], e["lead"]);
    const auto bars = macroChangeBars (s);
    REQUIRE (bars.lead.size() == e["lead_change_bars"].size());
    for (std::size_t i = 0; i < bars.lead.size(); ++i)
        CHECK (bars.lead[i].bar == e["lead_change_bars"][i].integer());
}

TEST_CASE ("G5 velocity resolution per 8 bars", "[metrics][fixtures]")
{
    const Value& e = answers()["g5"];
    const Value r = g5 (one (e["file"].string()));
    for (const char* g : { "hats", "perc", "ghosts", "bass", "lead" })
    {
        INFO (g);
        checkExact (r[g], e[g]);
    }
    CHECK (r.num ("pass") == e.num ("pass"));
}

TEST_CASE ("G6 chord stacks with two or more velocities", "[metrics][fixtures]")
{
    const Value& e = answers()["g6"];
    checkPicked (g6 (one (e["file"].string())), e, { "file" });
}

TEST_CASE ("G7 microtiming: SD, lag-1 autocorrelation, DFA alpha", "[metrics][fixtures]")
{
    const Value& e = answers()["g7"];
    const Value r = g7 (one (e["file"].string()));
    Value both = Value::object();
    both.set ("hats", e["hats"]);
    both.set ("perc", e["perc"]);
    checkExact (r["per_file"][pathOf (e["file"].string())], both);
    CHECK (r.num ("pass") == e.num ("pass"));
}

TEST_CASE ("G7 lag-1 with a residual mean that is not 0", "[metrics][fixtures]")
{
    const Value& e = answers()["g7_mean"];
    const Value r = g7 (one (e["file"].string()));
    checkExact (r["per_file"][pathOf (e["file"].string())]["hats"], e["hats"]);
}

TEST_CASE ("G8 per-lane swing in the first drop", "[metrics][fixtures]")
{
    const Value& e = answers()["g8"];
    const Value r = g8 (one (e["file"].string()));
    checkExact (r["per_file"][pathOf (e["file"].string())], pick (e, { "lanes", "distinct" }));
    CHECK (r.num ("pass") == e.num ("pass"));
}

TEST_CASE ("G9 fills and boundary gestures", "[metrics][fixtures]")
{
    const Value& e = answers()["g9"];
    const Value r = g9 (one (e["file"].string()));
    checkPicked (r, e, { "file", "missing_gesture_bars", "fill_positions" });
    const Value& pf = r["per_file"][pathOf (e["file"].string())];
    CHECK (pf["missing_gesture_bars"].string() == e["missing_gesture_bars"].string());
    CHECK (pf["fill_positions"].string() == e["fill_positions"].string());
}

TEST_CASE ("G10 form diversity and change intervals", "[metrics][fixtures]")
{
    const Value& e = answers()["g10"];
    const auto songs = fixtures (e["files"]);
    const Value r = g10 (songs);
    for (const Value& f : e["files"].items())
    {
        INFO (f.string());
        Value pf = Value::object();
        pf.set ("form", e["forms"][f.string()]);
        pf.set ("interval_values", e["interval_values"][f.string()]);
        checkExact (r["per_file"][pathOf (f.string())], pf);
        CHECK (formShape (fixture (f.string())) == e["forms"][f.string()].string());
    }
    checkPicked (r, pick (e, { "distinct_forms", "top_form_count", "top_form_pm", "every_song_two_interval_values", "pass" }));

    const Value& m = answers()["g10_intervals"];
    const Value rm = g10 (one (m["file"].string()));
    CHECK (rm["per_file"][pathOf (m["file"].string())].num ("interval_values") == m.num ("interval_values"));
}

TEST_CASE ("R3 closed-hat accent period", "[metrics][fixtures]")
{
    const Value& e = answers()["r3"];
    checkExact (rHatAccent (one (e["file"].string())), e, { "file" });
}

TEST_CASE ("R4 the legacy gate rule", "[metrics][fixtures]")
{
    const Value& e = answers()["r4"];
    checkExact (rGateRule (one (e["file"].string())), e, { "file" });
}

TEST_CASE ("R5 chord tones on strong and weak steps", "[metrics][fixtures]")
{
    const Value& e = answers()["r5"];
    const Value r = rChordTones (one (e["file"].string()));
    checkPicked (r["lead"], e["lead"]);
    checkPicked (r["bass"], e["bass"]);
}

TEST_CASE ("R5 before the first chord, the harmony is the last one", "[metrics][fixtures]")
{
    const Value& e = answers()["r5_wrap"];
    checkPicked (rChordTones (one (e["file"].string()))["lead"], e["lead"]);
}

TEST_CASE ("R7 lead shape", "[metrics][fixtures]")
{
    const Value& e = answers()["r7"];
    checkExact (rLeadShape (one (e["file"].string())), e, { "file" });
}

TEST_CASE ("R8 voicing texture", "[metrics][fixtures]")
{
    const Value& e = answers()["r8"];
    const Value r = rVoicing (one (e["file"].string()));
    checkPicked (r, e, { "file", "types" });
    CHECK (r.num ("median_types_per_song") == e.num ("types"));
}

TEST_CASE ("R9 odd cycles", "[metrics][fixtures]")
{
    const Value& e = answers()["r9"];
    checkExact (rOddCycles (one (e["file"].string())), e, { "file" });
}

TEST_CASE ("R10 cross-seed duplicate motifs", "[metrics][fixtures]")
{
    const Value& e = answers()["r10"];
    checkExact (rDuplicates (fixtures (e["files"]), Role::lead), e, { "files" });
}

TEST_CASE ("R11 LHL syncopation", "[metrics][fixtures]")
{
    for (const Value& c : answers()["r11"]["cases"].items())
    {
        INFO (c["onsets"].string());
        std::vector<std::int32_t> onsets;
        std::string_view text = c["onsets"].string();
        while (!text.empty())
        {
            const auto comma = text.find (',');
            onsets.push_back (std::stoi (std::string (text.substr (0, comma))));
            text = comma == std::string_view::npos ? std::string_view {} : text.substr (comma + 1);
        }
        CHECK (lhl (onsets) == c.num ("value"));
    }
}

TEST_CASE ("I1 notes in key", "[metrics][fixtures]")
{
    const Value& e = answers()["i1"];
    const Value r = iInKey (one (e["file"].string()));
    for (const char* role : { "lead", "bass", "chords" })
    {
        INFO (role);
        checkPicked (r[role], e[role]);
    }
}

TEST_CASE ("I2 risers and rolls on the swung grid", "[metrics][fixtures]")
{
    const Value& e = answers()["i2"];
    const Value r = iSwungGrid (one (e["file"].string()));
    checkPicked (r["riser"], e["riser"]);
    checkPicked (r["build_snare"], e["build_snare"]);
}

TEST_CASE ("I3 kick and bass within 2 ticks, I5 kick anchors", "[metrics][fixtures]")
{
    const Value& e3 = answers()["i3"];
    checkPicked (iKickBass (one (e3["file"].string())), e3, { "file" });
    const Value& e5 = answers()["i5"];
    checkPicked (iAnchors (one (e5["file"].string())), e5, { "file" });
}

TEST_CASE ("I4 sections on 8-bar lines", "[metrics][fixtures]")
{
    const Value& e = answers()["i4"];
    checkPicked (iSections (one (e["file"].string())), e, { "file" });
}
