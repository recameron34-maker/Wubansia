#pragma once

// The metric suite as a whole: the ten gates with their bands, which version of a seed each metric
// reads, the legacy baseline laid out as tools/legacy_audit/audit.js lays it out, a set of
// generator v2 seeds, and the text table groovecore metrics prints.

#include "metrics/metrics.h"
#include "metrics/value.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace wubansia::metrics
{
    // A generator v2 seed comes as two files (plan §2.3): the score, "quantized", and the
    // performance, "with feel". Each metric reads one of them (README, "With feel or quantized");
    // a legacy file is its own score and performance.
    enum class Version : std::uint8_t {
        quantized,
        feel
    };
    std::string_view versionName (Version version) noexcept; // "quantized", "feel"

    struct GateDef
    {
        std::string_view id; // "G1"
        std::string_view band; // the plan's band, in the audit's words
        Version reads;
        Value (*run) (Songs);
        // The legacy file sets the audit measures the gate on. The first is its primary set: the
        // one whose result is the plan's "Legacy (audit)" column, which legacy_fails reports on.
        std::span<const std::string_view> legacySets;
    };

    // G1 to G10, in order.
    std::span<const GateDef> gateDefs() noexcept;
    const GateDef* findGate (std::string_view id) noexcept;

    // The tolerances of the port (README, "Tolerances for the C++ port"): every value exact except
    // DFA alpha, +-1 in thousandths.
    std::span<const Tolerance> portTolerances() noexcept;

    // ---- the legacy baseline ----

    // The legacy file sets, by slot (README, "legacy_expected.json"): parts, chord_parts,
    // drum_parts, lead_parts, bass_parts, all_parts, Chords-A, FullArrangement, FullSong,
    // ExtendedMix.
    std::span<const std::string_view> legacySetNames() noexcept;
    bool inLegacySet (std::string_view set, std::string_view slot) noexcept;

    // What tools/legacy_audit/audit.js computes on the 516-file baseline: files, notes, sets,
    // section_kinds, gating (every gate on each of its sets, with legacy_fails from the primary
    // set, and G5's part_levels), reported and invariants. core/tests/legacy_expected.json holds the
    // same, plus a header only the audit can write (its Node version, the manifest's SHA-256...).
    // songs may come in any order: they are measured by path, in byte order.
    Value auditLegacy (std::span<const Song> songs);

    // ---- a set of generator v2 seeds ----

    struct Seed
    {
        const Song* quantized = nullptr; // the score
        const Song* feel = nullptr; // the performance; the same Song as quantized for a legacy file
    };

    // Every gate, reported metric and invariant on the seeds (the 64 seeds of plan §5.1, or any
    // number), each on the version it reads:
    //   { "seeds": n, "files": distinct files, "notes": their notes,
    //     "gating": { "G1": { "band", "reads", "pass", "result": { ...g1 () } }, ... },
    //     "pass": 1 if every gate passes, "failed": "G1 G2 ..." (or ""),
    //     "reported": { "R1_repetition_lag4": ..., ... }, "invariants": { ... } }
    Value scoreSeeds (std::span<const Seed> seeds);

    // Pairs files into seeds by path: a file whose path has a "feel" folder and the file at the same
    // path with "quantized" in that place are one seed; any other file is a seed of its own, read as
    // both. In paths order, by the first file of each seed. Paths use '/'.
    struct SeedFiles
    {
        std::size_t quantized = 0; // indices into paths
        std::size_t feel = 0;
    };
    std::vector<SeedFiles> pairVersions (std::span<const std::string> paths);

    // True for a path inside a "regions" folder: a generator v2 region file, one section of a song.
    // Scoring it as a song of its own would count each section twice.
    bool isRegionPath (std::string_view path) noexcept;

    // ---- text ----

    // One line on a gate's result: the numbers its band judges, e.g. for G3
    // "drums 426/508 repeated (83.9%), 70 four-on-the-floor kick windows exempt; bass 60/86 (69.8%)".
    std::string gateSummary (std::string_view id, const Value& result);

    // The table groovecore metrics prints for an auditLegacy () result: per gate, its primary set,
    // whether legacy fails it, and gateSummary ().
    std::string legacyTable (const Value& audit);

    // The same for a scoreSeeds () result: per gate, the version it reads, PASS or FAIL, gateSummary ().
    std::string seedsTable (const Value& score);
}
