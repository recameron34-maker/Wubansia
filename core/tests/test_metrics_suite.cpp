// Tests for core/metrics/suite.h: the gates' definitions, which version of a seed each metric reads,
// the legacy file sets, seeds from paths, and the text groovecore metrics prints. Nothing here needs
// the legacy baseline: the committed legacy_expected.json and the audit's fixtures are enough.

#include "metrics/suite.h"
#include "metrics/value.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <filesystem>
#include <set>
#include <string>
#include <vector>

using namespace wubansia::metrics;
using Catch::Matchers::ContainsSubstring;

namespace
{
    const std::filesystem::path sourceDir = WUBANSIA_SOURCE_DIR;
    const std::filesystem::path fixtureDir = sourceDir / "tools" / "legacy_audit" / "fixtures";

    const Value& expectedAudit()
    {
        static const Value v = [] {
            auto r = readJsonFile (sourceDir / "core" / "tests" / "legacy_expected.json");
            INFO (r.error);
            REQUIRE (r.ok());
            return r.value;
        }();
        return v;
    }

    Song fixture (const std::string& name)
    {
        auto r = loadSongFile (fixtureDir / name, "fixtures/" + name);
        INFO (r.error);
        REQUIRE (r.ok());
        return r.value;
    }
}

TEST_CASE ("the ten gates: ids, bands, file sets and versions", "[metrics][suite]")
{
    const auto gates = gateDefs();
    REQUIRE (gates.size() == 10);
    const Value& gating = expectedAudit()["gating"];
    REQUIRE (gating.size() == 10);
    for (std::size_t i = 0; i < gates.size(); ++i)
    {
        const GateDef& g = gates[i];
        INFO (g.id);
        CHECK (g.id == "G" + std::to_string (i + 1));
        CHECK (findGate (g.id) == &g);
        CHECK (g.run != nullptr);
        const Value& e = gating[g.id];
        // the audit's band text and sets, in its order
        CHECK (g.band == e["band"].string());
        REQUIRE_FALSE (g.legacySets.empty());
        CHECK (g.legacySets[0] == e["primary"].string());
        std::vector<std::string> sets;
        for (const auto& [k, v] : e.members())
            if (k != "band" && k != "primary" && k != "legacy_fails" && k != "part_levels")
                sets.push_back (k);
        REQUIRE (sets.size() == g.legacySets.size());
        for (std::size_t s = 0; s < sets.size(); ++s)
            CHECK (sets[s] == g.legacySets[s]);
    }
    CHECK (findGate ("G11") == nullptr);

    // README, "With feel or quantized": G5-G8 read the performance, the rest the score.
    for (const GateDef& g : gates)
    {
        INFO (g.id);
        const bool feel = g.id == "G5" || g.id == "G6" || g.id == "G7" || g.id == "G8";
        CHECK (g.reads == (feel ? Version::feel : Version::quantized));
    }
    CHECK (versionName (Version::feel) == "feel");
    CHECK (versionName (Version::quantized) == "quantized");

    const auto tol = portTolerances();
    REQUIRE (tol.size() == 2);
    for (const auto& t : tol)
    {
        CHECK ((t.key == "alpha_milli" || t.key == "median_alpha_milli"));
        CHECK (t.plusMinus == 1);
    }
}

TEST_CASE ("the legacy file sets by slot", "[metrics][suite]")
{
    const auto names = legacySetNames();
    std::vector<std::string> expectedNames;
    for (const auto& [k, v] : expectedAudit()["sets"].members())
        expectedNames.push_back (k);
    REQUIRE (names.size() == expectedNames.size());
    for (std::size_t i = 0; i < names.size(); ++i)
        CHECK (names[i] == expectedNames[i]);

    const std::vector<std::string> slots = { "Drums-Main", "Drums-Variation", "Drums-Fill", "Bass-A", "Bass-B", "Chords-A", "Chords-B", "Melody-A", "Melody-B", "FullArrangement", "FullSong", "ExtendedMix" };
    const auto members = [&] (std::string_view set) {
        std::set<std::string> out;
        for (const auto& s : slots)
            if (inLegacySet (set, s))
                out.insert (s);
        return out;
    };
    CHECK (members ("parts") == std::set<std::string> { "Drums-Main", "Drums-Variation", "Bass-A", "Bass-B" });
    CHECK (members ("chord_parts") == std::set<std::string> { "Chords-A", "Chords-B" });
    CHECK (members ("drum_parts") == std::set<std::string> { "Drums-Main", "Drums-Variation" });
    CHECK (members ("lead_parts") == std::set<std::string> { "Melody-A", "Melody-B" });
    CHECK (members ("bass_parts") == std::set<std::string> { "Bass-A", "Bass-B" });
    CHECK (members ("all_parts").size() == 9);
    CHECK_FALSE (inLegacySet ("all_parts", "FullSong"));
    CHECK_FALSE (inLegacySet ("all_parts", "Drums"));
    CHECK (members ("Chords-A") == std::set<std::string> { "Chords-A" });
    CHECK (members ("FullArrangement") == std::set<std::string> { "FullArrangement" });
    CHECK (members ("FullSong") == std::set<std::string> { "FullSong" });
    CHECK (members ("ExtendedMix") == std::set<std::string> { "ExtendedMix" });
    CHECK_FALSE (inLegacySet ("no such set", "FullSong"));
    CHECK_FALSE (inLegacySet ("chord_parts", "Chords-C"));
    CHECK_FALSE (inLegacySet ("FullSong", "FullSong2"));

    // The set counts on the baseline follow from the slots: 43 kits.
    const Value& counts = expectedAudit()["sets"];
    for (const auto set : names)
    {
        INFO (set);
        CHECK (counts.num (set) == 43 * static_cast<std::int64_t> (members (set).size()));
    }
}

TEST_CASE ("seeds from paths: a feel file and its quantized twin are one seed", "[metrics][suite]")
{
    const std::vector<std::string> paths = {
        "a/feel/FUT01_seed1_134bpm_11A_full.mid", // 0
        "a/quantized/FUT01_seed1_134bpm_11A_full.mid", // 1
        "UKG01/UKG01_FullSong.mid", // 2: legacy, both versions
        "b/feel/FUT02_seed2_130bpm_7A_full.mid", // 3: no quantized twin
        "c/quantized/FUT03_seed3_128bpm_6A_full.mid", // 4: no feel twin
        "feel/x.mid", // 5
        "quantized/x.mid", // 6
    };
    const auto seeds = pairVersions (paths);
    REQUIRE (seeds.size() == 5);
    CHECK ((seeds[0].quantized == 1 && seeds[0].feel == 0));
    CHECK ((seeds[1].quantized == 2 && seeds[1].feel == 2));
    CHECK ((seeds[2].quantized == 3 && seeds[2].feel == 3));
    CHECK ((seeds[3].quantized == 4 && seeds[3].feel == 4));
    CHECK ((seeds[4].quantized == 6 && seeds[4].feel == 5));
    CHECK (pairVersions ({}).empty());

    CHECK (isRegionPath ("out/feel/regions/03_drop_32bars.mid"));
    CHECK (isRegionPath ("regions/01_intro_32bars.mid"));
    CHECK_FALSE (isRegionPath ("out/feel/FUT01_seed1_134bpm_11A_full.mid"));
    CHECK_FALSE (isRegionPath ("out/myregions/x.mid"));
    CHECK_FALSE (isRegionPath ("regions.mid"));
}

TEST_CASE ("scoreSeeds runs each metric on the version it reads", "[metrics][suite]")
{
    const Song a = fixture ("harmony_a.mid");
    const Song b = fixture ("harmony_b.mid");
    const Song v = fixture ("velocity.mid");
    const Song m = fixture ("microtiming.mid");
    // Two seeds whose score and performance are different files, so each gate shows which it read.
    const std::vector<Seed> seeds = { { &a, &v }, { &b, &m } };
    const Value score = scoreSeeds (seeds);

    CHECK (score.num ("seeds") == 2);
    CHECK (score.num ("files") == 4);
    CHECK (score.num ("notes") == a.noteCount() + b.noteCount() + v.noteCount() + m.noteCount());
    const std::vector<const Song*> quantized = { &a, &b };
    const std::vector<const Song*> feel = { &v, &m };
    for (const GateDef& g : gateDefs())
    {
        INFO (g.id);
        const Value& gate = score["gating"][g.id];
        CHECK (gate["band"].string() == g.band);
        CHECK (gate["reads"].string() == versionName (g.reads));
        const Value direct = g.run (g.reads == Version::feel ? std::span<const Song* const> (feel) : std::span<const Song* const> (quantized));
        CHECK (gate["result"] == direct);
        CHECK (gate.num ("pass") == direct.num ("pass"));
    }
    std::string failed;
    for (const GateDef& g : gateDefs())
        if (score["gating"][g.id].num ("pass") == 0)
            failed += (failed.empty() ? "" : " ") + std::string (g.id);
    CHECK (score["failed"].string() == failed);
    CHECK (score.num ("pass") == (failed.empty() ? 1 : 0));

    // reported metrics and invariants: R4, I2, I3 and I5 read the performance
    CHECK (score["reported"]["R4_gate_rule"] == rGateRule (feel));
    CHECK (score["reported"]["R1_repetition_lag4"] == rRepetition (quantized));
    CHECK (score["reported"]["R3_hat_accent_period"] == rHatAccent (quantized));
    CHECK (score["reported"]["R10_cross_seed_duplicate_motifs"]["lead"] == rDuplicates (quantized, Role::lead));
    CHECK (score["reported"]["R10_cross_seed_duplicate_motifs"]["bass"] == rDuplicates (quantized, Role::bass));
    CHECK (score["invariants"]["I1_in_key"] == iInKey (quantized));
    CHECK (score["invariants"]["I2_risers_and_rolls_on_swung_grid"] == iSwungGrid (feel));
    CHECK (score["invariants"]["I3_kick_bass_within_2_ticks"] == iKickBass (feel));
    CHECK (score["invariants"]["I4_sections_on_8_bar_lines"] == iSections (quantized));
    CHECK (score["invariants"]["I5_kick_anchors_within_2_ticks"] == iAnchors (feel));

    // A legacy-style seed is one file for both versions, counted once.
    const std::vector<Seed> same = { { &a, &a } };
    CHECK (scoreSeeds (same).num ("files") == 1);
    CHECK (scoreSeeds ({}).num ("seeds") == 0);
}

TEST_CASE ("one-line gate summaries carry the numbers the band judges", "[metrics][suite]")
{
    const Value& g = expectedAudit()["gating"];
    CHECK_THAT (gateSummary ("G1", g["G1"]["FullSong"]), ContainsSubstring ("break: 21 distinct of 43 (48.8%), top 12 (27.9%)"));
    CHECK_THAT (gateSummary ("G2", g["G2"]["FullSong"]), ContainsSubstring ("26/43 (60.5%)"));
    CHECK_THAT (gateSummary ("G2", g["G2"]["FullSong"]), ContainsSubstring ("12 patterns"));
    CHECK_THAT (gateSummary ("G3", g["G3"]["parts"]), ContainsSubstring ("drums 426/508 repeated (83.9%)"));
    CHECK_THAT (gateSummary ("G3", g["G3"]["parts"]), ContainsSubstring ("70 four-on-the-floor kick windows exempt"));
    CHECK_THAT (gateSummary ("G3", g["G3"]["parts"]), ContainsSubstring ("bass 60/86 repeated (69.8%)"));
    CHECK_THAT (gateSummary ("G4", g["G4"]["ExtendedMix"]), ContainsSubstring ("lead 272/787 (34.6%)"));
    CHECK_THAT (gateSummary ("G5", g["G5"]["FullSong"]), ContainsSubstring ("hats 4 values, SD 16.386"));
    CHECK_THAT (gateSummary ("G6", g["G6"]["chord_parts"]), ContainsSubstring ("0/1018 stacks (0.0%)"));
    CHECK_THAT (gateSummary ("G7", g["G7"]["ExtendedMix"]), ContainsSubstring ("hats: SD 2.376, lag-1 -0.029, alpha 0.524"));
    CHECK_THAT (gateSummary ("G7", g["G7"]["FullSong"]), ContainsSubstring ("lag-1 undefined"));
    CHECK_THAT (gateSummary ("G8", g["G8"]["FullSong"]), ContainsSubstring ("0/43 seeds with 3 or more"));
    CHECK_THAT (gateSummary ("G9", g["G9"]["FullSong"]), ContainsSubstring ("gestures at 344/352 boundaries"));
    CHECK_THAT (gateSummary ("G9", g["G9"]["FullSong"]), ContainsSubstring ("position 7 (99.0%)"));
    CHECK_THAT (gateSummary ("G10", g["G10"]["FullSong"]), ContainsSubstring ("7 forms of 43, top 8 (18.6%)"));
    CHECK (gateSummary ("G99", g["G1"]["FullSong"]).empty());

    // legacy_expected.json has the shape auditLegacy () returns, so the table reads it too
    const std::string table = legacyTable (expectedAudit());
    for (int i = 1; i <= 10; ++i)
        CHECK_THAT (table, ContainsSubstring ("G" + std::to_string (i) + " "));
    CHECK_THAT (table, ContainsSubstring ("legacy fails"));
    CHECK_THAT (table, ContainsSubstring ("ExtendedMix"));
}
