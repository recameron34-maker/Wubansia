#pragma once

// The metric suite of docs/plans/S1-futuristic-slice.md §5.1: the ten gating metrics G1-G10, the
// reported metrics R1-R11 and the invariants I1-I5 that MIDI can show, exactly as
// tools/legacy_audit/README.md defines them (the C++ port of tools/legacy_audit/metrics.js).
//
// Each metric takes the set of files it is measured on (for generator v2 the seeds, for legacy
// the kits that stand in for seeds) and returns a Value with the same keys and numbers as the
// audit's result: the aggregate, a "pass" flag for the gates (1 or 0) and "per_file" values by
// path. A set of one file gives that file's values. core/tests/legacy_expected.json holds the
// audit's results on the legacy baseline, and the C++ must reproduce them (README, "Tolerances").
//
// Which version of a generator v2 seed each metric reads (score or performance) is in suite.h.

#include "metrics/measures.h"
#include "metrics/model.h"
#include "metrics/value.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace wubansia::metrics
{
    // The files a metric is measured on, in the order they are reported.
    using Songs = std::span<const Song* const>;

    // ---- gating metrics (README, "Gating metrics") ----

    Value g1 (Songs songs); // progression-shape diversity of the first break and the first drop
    Value g2 (Songs songs); // harmonic rhythm of the first 4 drop bars
    Value g3 (Songs songs); // one-bar repetition in 4-bar windows, four-on-the-floor kick windows exempt
    Value g4 (Songs songs); // macro changes on phrase-start or phrase-end bars
    Value g5 (Songs songs); // velocity resolution per 8 bars
    Value g6 (Songs songs); // chord stacks with two or more velocities
    Value g7 (Songs songs); // microtiming on the feel lanes: SD, lag-1 autocorrelation, DFA alpha
    Value g8 (Songs songs); // per-lane swing in the first drop
    Value g9 (Songs songs); // fills and boundary gestures
    Value g10 (Songs songs); // form diversity and the intervals between salient changes

    // The gating bands (plan §5.1) of G3, G5, G6 and G7 as pure functions of each gate's aggregate:
    // the gates decide "pass" with them, and tools/legacy_audit/fixtures/expected.json ("bands")
    // pins every bound at its edge, here and in the audit's metrics.js (its "bands"), alike.
    namespace bands
    {
        // drum lanes repeat in at most 40% of their 4-bar windows, the bass in at most 30%
        bool g3 (std::int64_t drumWindows, std::int64_t drumRepeated, std::int64_t bassWindows, std::int64_t bassRepeated) noexcept;
        // a drum group's median 8-bar window: 10 or more velocities, an SD of 6 to 22 (milli)
        bool g5Drums (std::int64_t windows, std::int64_t medianDistinct, std::int64_t medianSdMilli) noexcept;
        // a melodic group's: 6 or more velocities, an SD of 8 or more
        bool g5Melodic (std::int64_t windows, std::int64_t medianDistinct, std::int64_t medianSdMilli) noexcept;
        // 70% or more of the chord stacks with two velocities or more, none spread over more than 4 ticks
        bool g6 (std::int64_t stacks, std::int64_t twoVelocities, std::int64_t maxSpreadTicks) noexcept;
        // a feel lane's median file: residual SD 1.5 to 6 ticks, lag-1 0.2 or more, DFA alpha 0.65 to 1.1
        bool g7 (std::int64_t files, std::int64_t medianSdMilli, std::int64_t medianLag1Pm, std::int64_t medianAlphaMilli) noexcept;
    }

    // The velocity values each lane group uses across a set of parts (G5's legacy figure).
    Value partLevels (Songs songs);

    // Per-file pieces of the gates, for tests and messages.
    std::optional<std::string> progressionShape (const Song& song, std::string_view kind); // G1
    std::optional<std::string> harmonicRhythm (const Song& song); // G2: "2,2,2,2"
    struct MacroChange
    {
        std::int64_t bar = 0; // absolute
        std::int64_t pos = 0; // the section bar index mod 8
    };
    struct MacroChanges
    {
        std::vector<MacroChange> bass, chords, lead;
    };
    MacroChanges macroChangeBars (const Song& song); // G4, G10
    std::string formShape (const Song& song); // G10: "intro16 drop32 ..."

    // ---- reported metrics (README, "Reported metrics") ----

    Value rRepetition (Songs songs); // R1 lag-4 repetition per role
    Value rLoopTiling (Songs songs); // R2 longest run of identical 4-bar blocks
    Value rHatAccent (Songs songs); // R3 closed-hat accent period
    Value rGateRule (Songs songs); // R4 the legacy gate rule
    Value rChordTones (Songs songs); // R5 chord tones on strong and weak steps
    Value rBreath (Songs songs); // R6 lead rests and lead/vox complementarity
    Value rLeadShape (Songs songs); // R7 step ratio, bar 1 = bar 3
    Value rVoicing (Songs songs); // R8 voicing texture
    Value rOddCycles (Songs songs); // R9 odd cycles in drum lanes
    Value rDuplicates (Songs songs, Role role); // R10 cross-seed duplicate 2-bar motifs of a role
    Value rSyncopation (Songs songs); // R11 Longuet-Higgins and Lee syncopation per lane group

    std::int64_t accentPeriod (std::span<const std::int32_t, 16> velocities) noexcept; // R3
    std::string voicingType (std::vector<int> keys); // R8: "quartal", "octave", "dyad", "triad", "four_plus"

    // ---- invariants measured on MIDI (README, "Invariants measured on legacy") ----

    Value iInKey (Songs songs); // I1 non-drum notes in the key signature's scale, per role
    Value iSwungGrid (Songs songs); // I2 risers and build rolls on the swung grid
    Value iKickBass (Songs songs); // I3 bass onsets near a kick within 2 ticks of it
    Value iSections (Songs songs); // I4 sections on 8-bar lines
    Value iAnchors (Songs songs); // I5 kicks near a beat within 2 ticks of it
}
