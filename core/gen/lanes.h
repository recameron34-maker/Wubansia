#pragma once

// The score of each lane (plan §2.3, §4.1, stages 3-4), on the arrangement and harmony already in
// the song. Every note lies inside one of the lane's active ranges (arrange.h): it starts and ends
// inside its section, so region files tile the song.
//
// Drums: per section and variant a 2-bar pattern from the lane's grid (rhythm.h), repeated bar by
// bar, with per-bar ornaments, skips and velocities, turnaround hits in turnaround bars, Euclidean
// and polymeter cells, the exclusive rule (an open hat replaces a closed one on its step) and the
// choke (an open hat ends where the next closed one starts).
// Bass: on chord degrees from the harmony; on the kicks of its follow lane (each step's choice
// kept per variant) and on its own grid; its seventh the minor one, and its third, seventh and fifth
// agreeing with what the chord lanes voice (so the chords are scored first); held notes run to the
// next note (across a skipped beat);
// a held note crossing a chord change by more than `restrike` is struck again on the change;
// approach notes a 16th or an 8th before chord changes, a scale step from the new root; slides.
// Chords: one voicing per chord span from the section's palette; a pad holds it, stabs play the
// stab grid's onsets plus one on every pushed change; the top voice 4-8 louder.

#include "gen/content.h"
#include "gen/harmony.h"
#include "gen/song.h"
#include "rng/rng.h"

#include <cstddef>

namespace wubansia::gen
{
    void scoreDrums (Song& song, const Bundle& bundle, std::size_t lane, rng::Key root);
    void scoreBass (Song& song, const Bundle& bundle, std::size_t lane, rng::Key root);
    void scoreChords (Song& song, const Bundle& bundle, std::size_t lane, rng::Key root);

    // Sorts notes by (tick, key, voice, length, velocity): a total order on what a track holds.
    void sortNotes (std::vector<Note>& notes);
}
