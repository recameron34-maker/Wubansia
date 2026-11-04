// The metric suite (core/metrics) on the generator's own MIDI, as plan §5.1 asks: 64 seeds per
// style, both versions, through the same code that scores the legacy baseline. The invariants must
// hold; the gates are pinned at what part 1 passes, so a regression fails here, and each gate part 1
// can't pass yet is listed with its reason, so S1b2 has to update this list when it fixes one.
// The stand-ins elsewhere (test_gen_feel's and test_gen_styles_feel's hat microtiming, for example)
// are proxies for single properties, not the gates.

#include "gen/content.h"
#include "gen/export.h"
#include "gen/song.h"
#include "metrics/model.h"
#include "metrics/suite.h"
#include "metrics/value.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <filesystem>
#include <set>
#include <string>
#include <string_view>
#include <vector>

using namespace wubansia;

namespace
{
    struct Expected
    {
        const char* kit;
        std::set<std::string_view> failing; // every other gate must pass
    };

    // Part 1's verdicts on seeds 0-63, and why each failing gate fails:
    // - G4, G5: no lead lane yet (the hook, S1b2); FUT02's arp counts as lead. FUT04's G5 also has no
    //   ghost windows: its sparse snare plays no ghosts.
    // - G7 on FUT01 and FUT03: the metric's hats group merges the hats track with the shaker (GM 54),
    //   two independent 1/f series: a decision for the S1b2 gating ADR, not a feature.
    // - G8 on FUT01: 63 of 64 seeds; in seed 23 the sub, locked to the 2-step kick, lands within 2 ticks
    //   of the snare and the stabs, and the perc of the hats: the swing tiers versus round 1's "every
    //   lane uses the kit's one swing" is the same decision.
    // - G1, G2 on FUT04: a one-chord pedal by design (an exemption or another measure, the same ADR).
    // - G9, G10: no fills or transition gestures yet, one form per style (S1b2's fills and form grammar).
    const Expected expected[] = {
        { "FUT01", { "G4", "G5", "G7", "G8", "G9", "G10" } },
        { "FUT02", { "G9", "G10" } },
        { "FUT03", { "G4", "G5", "G7", "G9", "G10" } },
        { "FUT04", { "G1", "G2", "G4", "G5", "G9", "G10" } },
    };
}

TEST_CASE ("gates: the metric suite on 64 seeds per style, invariants and gates", "[gen][metrics][gates]")
{
    for (const Expected& e : expected)
    {
        INFO (e.kit);
        auto bundle = gen::loadBundle (std::filesystem::path (WUBANSIA_SOURCE_DIR) / "content", e.kit);
        INFO (bundle.error);
        REQUIRE (bundle.ok());
        std::vector<metrics::Song> songs;
        songs.reserve (128);
        for (std::uint64_t seed = 0; seed < 64; ++seed)
        {
            gen::Options o;
            o.seed = seed;
            const auto song = gen::generate (bundle.value, o);
            REQUIRE (song.ok());
            for (const gen::Version v : { gen::Version::quantized, gen::Version::feel })
            {
                const auto files = gen::renderFiles (song.value, v);
                REQUIRE (files.ok());
                const auto& full = files.value.front(); // the full arrangement comes first
                auto loaded = metrics::loadSong (full.bytes, "seed" + std::to_string (seed) + "/" + std::string (gen::versionName (v)) + "/" + full.path);
                INFO (loaded.error);
                REQUIRE (loaded.ok());
                songs.push_back (std::move (loaded.value));
            }
        }
        std::vector<std::string> paths;
        for (const auto& s : songs)
            paths.push_back (s.path);
        std::vector<metrics::Seed> seeds;
        for (const auto& p : metrics::pairVersions (paths))
            seeds.push_back ({ &songs[p.quantized], &songs[p.feel] });
        REQUIRE (seeds.size() == 64);
        const metrics::Value r = metrics::scoreSeeds (seeds);

        // the invariants: always
        const metrics::Value& inv = r["invariants"];
        CHECK (inv["I2_risers_and_rolls_on_swung_grid"]["build_snare"].num ("straight_only") == 0);
        CHECK (inv["I2_risers_and_rolls_on_swung_grid"]["build_snare"].num ("notes") > 0);
        CHECK (inv["I3_kick_bass_within_2_ticks"].num ("within_2_ticks_pm") == 1000);
        CHECK (inv["I4_sections_on_8_bar_lines"].num ("on_8_bar_lines_pm") == 1000);
        CHECK (inv["I5_kick_anchors_within_2_ticks"].num ("within_2_ticks_pm") == 1000);

        // the gates: part 1's verdicts, exactly
        for (const auto& g : metrics::gateDefs())
        {
            INFO (g.id << ": " << metrics::gateSummary (g.id, r["gating"][g.id]["result"]));
            CHECK (r["gating"][g.id].num ("pass") == (e.failing.contains (g.id) ? 0 : 1));
        }
    }
}
