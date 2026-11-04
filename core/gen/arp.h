#pragma once

// The arp lane (the sci-fi club brief: 16th arps as a lane that follows the chords; the hook proper is
// S1b2). Per section, from the lane's grid for the section kind, and per variant of that grid:
// - the rhythm: the grid's 16ths, each kept with its probability (100: always), drawn per variant;
// - the cell: one of the grid's cells, picked by weight per variant, every index raised by a shift
//   drawn per variant: indices into the chord's tone ladder (0 the root, 1 the next tone up...),
//   played in turn on the rhythm's steps and started again at every restart step. A 3-index cell
//   restarting every 8 steps gives a club arp's 3-3-2;
// - the pitch: each step takes the chord sounding at its tick (pushed changes included), places the
//   chord's root nearest the lane's center and climbs the ladder from there, in the chord's mode;
//   a key outside the kit's range moves by octaves into it;
// - the velocity from the grid (its accents) plus a spread drawn per bar, the length from the grid,
//   never into the next note.
// Everything but the spread is drawn per (grid, variant), so the arp repeats its 2-bar pattern until a
// change and re-pitches to every chord on the way. Keys: root.with (tag ("lane")).with (tag (lane)).

#include "gen/content.h"
#include "gen/song.h"
#include "rng/rng.h"

#include <cstddef>
#include <cstdint>

namespace wubansia::gen
{
    // A chord root's place: the key with pitch class rootPc nearest center (the lower one on a tie).
    std::int32_t placedRoot (std::int32_t rootPc, std::int32_t center) noexcept;

    // The key of ladder index `index` on a chord: tones[index % n] + 7 * (index / n) scale steps above
    // the placed root, in the chord's mode, plus grid.octave, then moved by octaves into range.
    std::int32_t ladderKey (const ArpGrid& grid, const ChordSpan& chord, std::int32_t center, std::int32_t index, Range range) noexcept;

    // Fills song.lanes[lane].score for an arp lane.
    void scoreArp (Song& song, const Bundle& bundle, std::size_t lane, rng::Key root);
}
