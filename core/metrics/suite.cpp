#include "metrics/suite.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <map>

namespace wubansia::metrics
{
    namespace
    {
        // The file sets each gate is measured on in the audit (tools/legacy_audit/audit.js, GATES);
        // the first is its primary set.
        constexpr std::array<std::string_view, 2> fullSongAndExtended = { "FullSong", "ExtendedMix" };
        constexpr std::array<std::string_view, 3> g2Sets = { "FullSong", "Chords-A", "ExtendedMix" };
        constexpr std::array<std::string_view, 2> g3Sets = { "parts", "FullSong" };
        constexpr std::array<std::string_view, 2> extendedAndFullSong = { "ExtendedMix", "FullSong" };
        constexpr std::array<std::string_view, 3> g6Sets = { "chord_parts", "FullSong", "ExtendedMix" };

        // The bands in the audit's words (audit.js, BANDS): legacy_expected.json carries the same text.
        const std::array<GateDef, 10> gates = { {
            { "G1", "progression shapes of the first break and the first drop: distinct >= 50% of seeds, top shape <= 10%, each", Version::quantized, &g1, fullSongAndExtended },
            { "G2", "harmonic rhythm of the first 4 drop bars: \"4 x 1-bar slots\" (2,2,2,2 half-bars) <= 30% of seeds, >= 4 patterns", Version::quantized, &g2, g2Sets },
            { "G3", "one-bar repetition in 4-bar windows: drum lanes <= 40% (four-on-the-floor kick windows exempt), bass <= 30%", Version::quantized, &g3, g3Sets },
            { "G4", "macro changes on phrase-start or phrase-end bars (section bar mod 8 in {0, 7}): >= 50% for bass, chords and lead", Version::quantized, &g4, extendedAndFullSong },
            { "G5", "median per 8-bar window: hats, perc, ghosts >= 10 velocity values with SD 6-22; bass and lead >= 6 values with SD >= 8", Version::feel, &g5, fullSongAndExtended },
            { "G6", "chord stacks with >= 2 velocities >= 70%; stack onset spread 0-4 ticks", Version::feel, &g6, g6Sets },
            { "G7", "feel lanes (hats, perc), median over seeds: residual SD 1.5-6 ticks, lag-1 autocorrelation >= 0.2, DFA alpha 0.65-1.1", Version::feel, &g7, extendedAndFullSong },
            { "G8", ">= 3 distinct swings (2-tick tolerance) among the non-anchor lanes of the first drop, in every seed", Version::feel, &g8, fullSongAndExtended },
            { "G9", "100% of section boundaries carry a gesture; no fill position > 60%; median distinct fills per song >= 70%", Version::quantized, &g9, fullSongAndExtended },
            { "G10", ">= 8 distinct form shapes, top form <= 25%, intervals between salient changes take >= 2 values in every seed", Version::quantized, &g10, fullSongAndExtended },
        } };

        constexpr std::array<Tolerance, 2> tolerances = { { { "alpha_milli", 1 }, { "median_alpha_milli", 1 } } };

        constexpr std::array<std::string_view, 10> setNames = { "parts", "chord_parts", "drum_parts", "lead_parts", "bass_parts", "all_parts", "Chords-A", "FullArrangement", "FullSong", "ExtendedMix" };

        std::vector<std::string_view> folders (std::string_view path)
        {
            std::vector<std::string_view> parts;
            std::size_t start = 0;
            for (std::size_t slash = path.find ('/'); slash != std::string_view::npos; slash = path.find ('/', start))
            {
                parts.push_back (path.substr (start, slash - start));
                start = slash + 1;
            }
            return parts; // every component but the file name
        }

        // ---- text ----

        // A permille as a percentage with one decimal: 279 -> "27.9%". "n/a" for no value.
        std::string pct (std::int64_t pm)
        {
            if (pm < 0)
                return "n/a";
            return std::to_string (pm / 10) + "." + std::to_string (pm % 10) + "%";
        }

        // Thousandths as a decimal: 2376 -> "2.376", -29 -> "-0.029". "undefined" for -9999.
        std::string milli (std::int64_t v)
        {
            if (v == -9999)
                return "undefined";
            const std::int64_t a = std::llabs (v);
            std::string frac = std::to_string (a % 1000);
            frac.insert (0, 3 - frac.size(), '0');
            return (v < 0 ? "-" : "") + std::to_string (a / 1000) + "." + frac;
        }

        std::string ratio (const Value& r, std::string_view part, std::string_view whole) { return std::to_string (r.num (part)) + "/" + std::to_string (r.num (whole)); }

        std::string n (const Value& r, std::string_view key) { return std::to_string (r.num (key)); }

        std::string g5Group (const Value& r, std::string_view g)
        {
            const Value& v = r[g];
            if (v.num ("windows") <= 0)
                return std::string (g) + " no windows";
            return std::string (g) + " " + n (v, "median_distinct") + " values, SD " + milli (v.num ("median_sd_milli"));
        }

        std::string g7Lane (const Value& r, std::string_view lane)
        {
            const Value& v = r[lane];
            if (v.num ("files") <= 0)
                return std::string (lane) + ": no file with 64 residuals";
            return std::string (lane) + ": SD " + milli (v.num ("median_sd_milli")) + ", lag-1 " + milli (v.num ("median_lag1_pm")) + ", alpha "
                   + milli (v.num ("median_alpha_milli")) + " (" + n (v, "files") + " files)";
        }
    }

    std::string_view versionName (Version version) noexcept
    {
        return version == Version::feel ? "feel" : "quantized";
    }

    std::span<const GateDef> gateDefs() noexcept { return gates; }

    const GateDef* findGate (std::string_view id) noexcept
    {
        for (const auto& g : gates)
            if (g.id == id)
                return &g;
        return nullptr;
    }

    std::span<const Tolerance> portTolerances() noexcept { return tolerances; }

    std::span<const std::string_view> legacySetNames() noexcept { return setNames; }

    bool inLegacySet (std::string_view set, std::string_view slot) noexcept
    {
        // audit.js SET_SLOTS
        if (set == "parts")
            return slot == "Drums-Main" || slot == "Drums-Variation" || slot == "Bass-A" || slot == "Bass-B";
        if (set == "chord_parts")
            return slot == "Chords-A" || slot == "Chords-B";
        if (set == "drum_parts")
            return slot == "Drums-Main" || slot == "Drums-Variation";
        if (set == "lead_parts")
            return slot == "Melody-A" || slot == "Melody-B";
        if (set == "bass_parts")
            return slot == "Bass-A" || slot == "Bass-B";
        if (set == "all_parts")
            return slot.starts_with ("Drums-") || slot.starts_with ("Bass-") || slot.starts_with ("Chords-") || slot.starts_with ("Melody-");
        if (set == "Chords-A" || set == "FullArrangement" || set == "FullSong" || set == "ExtendedMix")
            return slot == set;
        return false;
    }

    Value auditLegacy (std::span<const Song> songs)
    {
        // By path in byte order, so nothing depends on the order the files came in.
        std::vector<const Song*> sorted;
        for (const auto& s : songs)
            sorted.push_back (&s);
        std::stable_sort (sorted.begin(), sorted.end(), [] (const Song* a, const Song* b) { return a->path < b->path; });
        const auto set = [&sorted] (std::string_view name) {
            std::vector<const Song*> out;
            for (const Song* s : sorted)
                if (inLegacySet (name, s->slot))
                    out.push_back (s);
            return out;
        };

        std::int64_t notes = 0;
        for (const Song* s : sorted)
            notes += s->noteCount();

        Value result = Value::object();
        result.set ("about", "The metric suite (core/metrics) on the legacy MIDI baseline, laid out as tools/legacy_audit/audit.js lays it out: "
                             "core/tests/legacy_expected.json holds the audit's own result. Integers and strings only.");
        result.set ("format", 1);
        result.set ("files", sorted.size());
        result.set ("notes", notes);
        Value sets = Value::object();
        for (const auto name : setNames)
            sets.set (name, set (name).size());
        result.set ("sets", std::move (sets));
        Value kinds = Value::object();
        for (const auto& e : legacySections())
            kinds.set (e.text, e.kind);
        result.set ("section_kinds", std::move (kinds));

        Value gating = Value::object();
        for (const GateDef& g : gates)
        {
            Value out = Value::object();
            out.set ("band", g.band);
            out.set ("primary", g.legacySets[0]);
            for (const auto name : g.legacySets)
                out.set (name, g.run (set (name)));
            out.set ("legacy_fails", out[g.legacySets[0]].num ("pass") == 0);
            if (g.id == "G5")
                out.set ("part_levels", partLevels (set ("all_parts")));
            gating.set (g.id, std::move (out));
        }
        result.set ("gating", std::move (gating));

        const auto fs = set ("FullSong");
        const auto em = set ("ExtendedMix");
        const auto two = [&] (Value (*run) (Songs), std::string_view band = {}) {
            Value v = Value::object();
            if (!band.empty())
                v.set ("band", band);
            v.set ("FullSong", run (fs));
            v.set ("ExtendedMix", run (em));
            return v;
        };
        Value reported = Value::object();
        reported.set ("R1_repetition_lag4", two (&rRepetition, "kick 0.70-0.95, chords 0.50-0.85, bass 0.40-0.80, lead 0.30-0.70"));
        reported.set ("R2_loop_tiling", two (&rLoopTiling));
        {
            Value v = Value::object();
            v.set ("band", "period <= 4 steps in <= 40% of bars");
            v.set ("drum_parts", rHatAccent (set ("drum_parts")));
            v.set ("FullSong", rHatAccent (fs));
            v.set ("ExtendedMix", rHatAccent (em));
            reported.set ("R3_hat_accent_period", std::move (v));
        }
        {
            Value v = Value::object();
            v.set ("band", "<= 50% of melodic notes");
            v.set ("FullArrangement", rGateRule (set ("FullArrangement")));
            v.set ("FullSong", rGateRule (fs));
            v.set ("ExtendedMix", rGateRule (em));
            reported.set ("R4_gate_rule", std::move (v));
        }
        {
            Value v = Value::object();
            v.set ("FullArrangement", rChordTones (set ("FullArrangement")));
            v.set ("FullSong", rChordTones (fs));
            v.set ("ExtendedMix", rChordTones (em));
            reported.set ("R5_chord_tones", std::move (v));
        }
        {
            Value v = Value::object();
            v.set ("lead_parts", rBreath (set ("lead_parts")));
            v.set ("ExtendedMix", rBreath (em));
            reported.set ("R6_breath_complementarity", std::move (v));
        }
        {
            Value v = Value::object();
            v.set ("band", "RIFF: step ratio 0.2-0.6, bar 1 = bar 3 rhythm 0.6-0.9; SENTENCE: step ratio 0.5-0.8, bar 1 = bar 3 <= 0.6");
            v.set ("lead_parts", rLeadShape (set ("lead_parts")));
            v.set ("ExtendedMix", rLeadShape (em));
            reported.set ("R7_lead_shape", std::move (v));
        }
        reported.set ("R8_voicing_texture", two (&rVoicing, ">= 3 types per song, 4+-note voicings <= 60% of stacks, >= 25% of recurring chords re-voiced"));
        reported.set ("R9_odd_cycles", two (&rOddCycles));
        {
            Value v = Value::object();
            v.set ("band", "<= 3% of 2-bar motifs");
            v.set ("lead_parts", rDuplicates (set ("lead_parts"), Role::lead));
            v.set ("bass_parts", rDuplicates (set ("bass_parts"), Role::bass));
            v.set ("ExtendedMix_lead", rDuplicates (em, Role::lead));
            reported.set ("R10_cross_seed_duplicate_motifs", std::move (v));
        }
        reported.set ("R11_syncopation_lhl", two (&rSyncopation));
        result.set ("reported", std::move (reported));

        Value invariants = Value::object();
        invariants.set ("I1_in_key", two (&iInKey));
        invariants.set ("I2_risers_and_rolls_on_swung_grid", two (&iSwungGrid));
        invariants.set ("I3_kick_bass_within_2_ticks", two (&iKickBass));
        invariants.set ("I4_sections_on_8_bar_lines", two (&iSections));
        invariants.set ("I5_kick_anchors_within_2_ticks", two (&iAnchors));
        result.set ("invariants", std::move (invariants));
        return result;
    }

    Value scoreSeeds (std::span<const Seed> seeds)
    {
        std::vector<const Song*> quantized, feel;
        std::vector<const Song*> files; // each file once, in seed order
        const auto addFile = [&files] (const Song* s) {
            if (std::find (files.begin(), files.end(), s) == files.end())
                files.push_back (s);
        };
        for (const Seed& s : seeds)
        {
            const Song* q = s.quantized != nullptr ? s.quantized : s.feel;
            const Song* f = s.feel != nullptr ? s.feel : s.quantized;
            if (q == nullptr)
                continue; // a seed without files
            quantized.push_back (q);
            feel.push_back (f);
            addFile (q);
            addFile (f);
        }
        std::int64_t notes = 0;
        for (const Song* s : files)
            notes += s->noteCount();

        Value result = Value::object();
        result.set ("seeds", quantized.size());
        result.set ("files", files.size());
        result.set ("notes", notes);
        Value gating = Value::object();
        std::string failed;
        for (const GateDef& g : gates)
        {
            Value r = g.run (g.reads == Version::feel ? feel : quantized);
            const bool pass = r.num ("pass") == 1;
            if (!pass)
                failed += (failed.empty() ? "" : " ") + std::string (g.id);
            Value entry = Value::object();
            entry.set ("band", g.band);
            entry.set ("reads", versionName (g.reads));
            entry.set ("pass", pass);
            entry.set ("result", std::move (r));
            gating.set (g.id, std::move (entry));
        }
        result.set ("gating", std::move (gating));
        result.set ("pass", failed.empty());
        result.set ("failed", failed);

        // README, "With feel or quantized": R4 reads the performance, the other reported metrics the score.
        Value reported = Value::object();
        reported.set ("R1_repetition_lag4", rRepetition (quantized));
        reported.set ("R2_loop_tiling", rLoopTiling (quantized));
        reported.set ("R3_hat_accent_period", rHatAccent (quantized));
        reported.set ("R4_gate_rule", rGateRule (feel));
        reported.set ("R5_chord_tones", rChordTones (quantized));
        reported.set ("R6_breath_complementarity", rBreath (quantized));
        reported.set ("R7_lead_shape", rLeadShape (quantized));
        reported.set ("R8_voicing_texture", rVoicing (quantized));
        reported.set ("R9_odd_cycles", rOddCycles (quantized));
        Value dup = Value::object();
        dup.set ("lead", rDuplicates (quantized, Role::lead));
        dup.set ("bass", rDuplicates (quantized, Role::bass));
        reported.set ("R10_cross_seed_duplicate_motifs", std::move (dup));
        reported.set ("R11_syncopation_lhl", rSyncopation (quantized));
        result.set ("reported", std::move (reported));

        // I2, I3 and I5 read the performance (the low-end lock and the anchors are feel), I1 and I4 the score.
        Value invariants = Value::object();
        invariants.set ("I1_in_key", iInKey (quantized));
        invariants.set ("I2_risers_and_rolls_on_swung_grid", iSwungGrid (feel));
        invariants.set ("I3_kick_bass_within_2_ticks", iKickBass (feel));
        invariants.set ("I4_sections_on_8_bar_lines", iSections (quantized));
        invariants.set ("I5_kick_anchors_within_2_ticks", iAnchors (feel));
        result.set ("invariants", std::move (invariants));
        return result;
    }

    std::vector<SeedFiles> pairVersions (std::span<const std::string> paths)
    {
        std::map<std::string_view, std::size_t> index; // a path -> its first index
        for (std::size_t i = 0; i < paths.size(); ++i)
            index.try_emplace (paths[i], i);
        std::vector<bool> used (paths.size(), false);
        std::vector<SeedFiles> out;
        for (std::size_t i = 0; i < paths.size(); ++i)
        {
            if (used[i])
                continue;
            used[i] = true;
            // the version folder nearest the file: <...>/feel/<name> or <...>/quantized/<name>
            const auto dirs = folders (paths[i]);
            std::size_t start = 0, at = dirs.size();
            for (std::size_t d = 0; d < dirs.size(); ++d)
            {
                if (dirs[d] == "feel" || dirs[d] == "quantized")
                    at = d;
            }
            if (at < dirs.size())
            {
                for (std::size_t d = 0; d < at; ++d)
                    start += dirs[d].size() + 1;
                const bool isFeel = dirs[at] == "feel";
                const std::string twin = paths[i].substr (0, start) + (isFeel ? "quantized" : "feel") + paths[i].substr (start + dirs[at].size());
                if (const auto it = index.find (twin); it != index.end() && !used[it->second])
                {
                    used[it->second] = true;
                    out.push_back (isFeel ? SeedFiles { it->second, i } : SeedFiles { i, it->second });
                    continue;
                }
            }
            out.push_back ({ i, i });
        }
        return out;
    }

    bool isRegionPath (std::string_view path) noexcept
    {
        std::size_t start = 0;
        for (std::size_t slash = path.find ('/'); slash != std::string_view::npos; slash = path.find ('/', start))
        {
            if (path.substr (start, slash - start) == "regions")
                return true;
            start = slash + 1;
        }
        return false;
    }

    std::string gateSummary (std::string_view id, const Value& r)
    {
        if (id == "G1")
        {
            const auto window = [] (const Value& w, std::string_view name) {
                std::string s = std::string (name) + ": " + n (w, "distinct") + " distinct of " + n (w, "measured") + " (" + pct (w.num ("distinct_pm")) + "), top "
                                + n (w, "top") + " (" + pct (w.num ("top_pm")) + ")";
                if (w.num ("measured") != w.num ("seeds"))
                    s += ", " + std::to_string (w.num ("seeds") - w.num ("measured")) + " without one";
                return s;
            };
            return window (r["break"], "break") + "; " + window (r["drop"], "drop");
        }
        if (id == "G2")
            return "2,2,2,2 in " + ratio (r, "four_by_one_bar", "measured") + " (" + pct (r.num ("four_by_one_bar_pm")) + "), " + n (r, "patterns")
                   + " patterns, changes only on bar lines in " + n (r, "changes_on_bar_lines_only") + " of " + n (r, "seeds");
        if (id == "G3")
            return "drums " + ratio (r, "drum_repeated", "drum_windows") + " repeated (" + pct (r.num ("drum_repeated_pm")) + "), " + n (r, "kick_4x4_exempt")
                   + " four-on-the-floor kick windows exempt; bass " + ratio (r, "bass_repeated", "bass_windows") + " repeated (" + pct (r.num ("bass_repeated_pm")) + ")";
        if (id == "G4")
        {
            std::string s = "on phrase:";
            for (const char* role : { "bass", "chords", "lead" })
                s += std::string (role == std::string_view ("bass") ? " " : ", ") + role + " " + ratio (r[role], "on_phrase", "changes") + " ("
                     + pct (r[role].num ("on_phrase_pm")) + ")";
            return s;
        }
        if (id == "G5")
            return "median per window: " + g5Group (r, "hats") + "; " + g5Group (r, "perc") + "; " + g5Group (r, "ghosts") + "; " + g5Group (r, "bass") + "; "
                   + g5Group (r, "lead");
        if (id == "G6")
            return ratio (r, "two_velocities", "stacks") + " stacks (" + pct (r.num ("two_velocities_pm")) + ") with 2 or more velocities, onset spread up to "
                   + n (r, "max_spread_ticks") + " ticks";
        if (id == "G7")
            return g7Lane (r, "hats") + "; " + g7Lane (r, "perc");
        if (id == "G8")
        {
            std::string s = ratio (r, "with_3_or_more", "measured") + " seeds with 3 or more distinct swings";
            std::string hist;
            for (const Value& h : r["distinct_histogram"].items())
                hist += (hist.empty() ? "" : ", ") + n (h, "value") + " in " + n (h, "count");
            if (!hist.empty())
                s += " (distinct swings: " + hist + ")";
            if (r.num ("measured") != r.num ("seeds"))
                s += ", " + std::to_string (r.num ("seeds") - r.num ("measured")) + " without a drop";
            return s;
        }
        if (id == "G9")
            return "gestures at " + ratio (r, "with_gesture", "boundaries") + " boundaries, fill position " + n (r, "top_position") + " ("
                   + pct (r.num ("top_position_pm")) + ") of " + n (r, "fills") + " fills, median distinct fills " + pct (r.num ("median_distinct_pm"));
        if (id == "G10")
            return n (r, "distinct_forms") + " forms of " + n (r, "measured") + ", top " + n (r, "top_form_count") + " (" + pct (r.num ("top_form_pm")) + "), "
                   + (r.num ("every_song_two_interval_values") == 1 ? "change intervals vary in every song" : "some song changes at one interval only");
        return {};
    }

    std::string legacyTable (const Value& audit)
    {
        std::string out = "legacy baseline: " + n (audit, "files") + " files, " + n (audit, "notes") + " notes\n";
        for (const GateDef& g : gates)
        {
            const Value& gate = audit["gating"][g.id];
            std::string id (g.id);
            id.resize (4, ' ');
            out += id + (gate.num ("legacy_fails") == 1 ? "legacy fails  " : "legacy PASSES ") + "on " + std::string (g.legacySets[0]) + ": "
                   + gateSummary (g.id, gate[g.legacySets[0]]) + "\n";
            for (std::size_t i = 1; i < g.legacySets.size(); ++i)
                out += "      " + std::string (g.legacySets[i]) + (gate[g.legacySets[i]].num ("pass") == 1 ? " passes: " : " fails: ")
                       + gateSummary (g.id, gate[g.legacySets[i]]) + "\n";
        }
        return out;
    }

    std::string seedsTable (const Value& score)
    {
        std::string out = n (score, "seeds") + " seeds, " + n (score, "files") + " files, " + n (score, "notes") + " notes\n";
        for (const GateDef& g : gates)
        {
            const Value& gate = score["gating"][g.id];
            std::string id (g.id);
            id.resize (4, ' ');
            std::string reads (versionName (g.reads));
            reads.resize (10, ' ');
            out += id + reads + (gate.num ("pass") == 1 ? "pass  " : "FAIL  ") + gateSummary (g.id, gate["result"]) + "\n";
        }
        out += score.num ("pass") == 1 ? std::string ("every gate passes\n") : "failed: " + score["failed"].string() + "\n";
        return out;
    }
}
