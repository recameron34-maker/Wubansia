#pragma once

// The hook (S1b2b, docs/plans/S1b2b-plan.md): one per seed, a 2-bar riff drawn rhythm first and
// pitched key-relative, drop 2's new phrase in the kit's written idiom, and ANSWER's response. The
// line plan places it per 8-bar line over the drawn form (the style's menus, then the DJ rules r1,
// r2 and r3, the withheld kinds, drop 1's FULL line, drop 2's new phrase and the cover floor), each
// line one texture of the material, so the lead changes on the line as G4 reads it (onTheLine). The
// lines are realized against the chords (the avoid-note fit, then the voicing guard: no minor 9th
// and no low semitone against a chord-lane note) and scoreLead (lead.h) copies them onto the
// carrier lane. Integers only; every draw hangs below hookKey (root), so the form, harmony, drums,
// chords and bass draw exactly as without a hook.

#include "gen/content.h"
#include "gen/song.h"
#include "rng/rng.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace wubansia::gen
{
    // The root of the hook's key tree: root.with (tag ("hook")), a permanent top-level tag.
    rng::Key hookKey (rng::Key root) noexcept;

    // The material's first draw, A's bar-1 cell: hookKey (root).with (tag ("rhythm")).with (tag ("cell")).
    rng::Key hookCellKey (rng::Key root) noexcept;

    // A line's bar prints as G4 reads a track (tools/legacy_audit/README.md, G4): per bar of the 8-bar
    // line, its onsets as 16th * 128 + key, ascending, each once.
    using BarPrint = std::vector<std::int32_t>;
    using LinePrints = std::array<BarPrint, static_cast<std::size_t> (barsPerBlock)>;

    // The line rule. G4 compares bar i with bar i - 8 inside a section and counts a run of differing
    // bars once, on phrase at bar 0 or 7 of a line; so `after`, the line that follows `before`, may
    // differ from it in every bar or in none, bar 7 aside (VARY, CADENCE and BREATH change bar 7 only).
    bool onTheLine (const LinePrints& before, const LinePrints& after) noexcept;

    // Fills song.hook for a style with a hook block (nothing otherwise): the material, the line plan
    // over the drawn form and the realized notes (scoreLead, lead.h, keeps those inside the lane's
    // ranges and sets the lines' half-bar masks from them); then clears the carrier's block bit on its
    // OUT lines (and in a section with no menu) and the carrier from any one-bar dropout inside one (a
    // dropout that then lists no lane goes). Runs after planChanges, the chords and holdPads (the guard
    // reads the chord lanes) and before the bass. Keys: hookKey (root).
    void planHook (Song& song, const Bundle& bundle, rng::Key root);
}
