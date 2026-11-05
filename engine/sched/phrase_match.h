#pragma once

// Phrase matching (P6): ties a timeline's notes on phrase lanes to the recorded slices of their stems,
// on the service thread, as buildTimeline's last step before the chase list.
// tools/sound/phrase_tables.py's resolve () is its model, and play ()'s continues () its rule for
// NoteFlag::continues.
//
// Per MIDI channel that has phrases (they share one cycle_16ths), one cycle at a time: cycles start every
// cycle_16ths from song 16th 0. A note's cell is (the 16th it sounds in, from the cycle's start; its key),
// from its onset: pos + its pre-roll (Note::preroll), so the 16th it is written on. Odd 16ths start
// (swing_pm - 500) x 480 milliticks late, as the timeline's swing snap puts them.
// - Each phrase scores the cycle: the number of distinct cells of the cycle in its slice pattern, or -1
//   when its ride doesn't fit the cycle's controllers (below). The highest score wins, the first phrase
//   on a tie; a best score of 0 or less leaves the cycle's notes on the multisample. So a subset of a
//   phrase ('call' lines) plays its slices, and of two lines on one channel the one with more matches
//   plays.
// - In the winner, in order of onset (then key): a note on a cell of the pattern not yet taken plays
//   that slice. After it, the notes its slice holds (PhraseSlice::holds, by key, in order) are the
//   slice's own, each on the slice's own cell or a cell that is no slice's. Every other note stays on
//   the multisample.
// - Ride: a phrase with a ride (PhraseEntry::ride74, ride71) fits a cycle only where each of its values
//   is within kRideSlack of the cycle's: on the kRideTicks grid from the cycle's start, the last CC on
//   the channel at or before each point (Timeline::ccs), 0 before any. An empty array constrains
//   nothing.
//
// What it writes (the encoding the engine selects by, per note-on, with playsSlices ()):
// - A matched note stays where it is, with phrase = its phrase and slice = -1: it plays its
//   multisample, only while that phrase doesn't play slices. So does a note a slice holds.
// - Beside it, a slice note: phrase and slice (Kit::slices' index: the phrase's firstSlice + its own),
//   at the slice's first frame: pos = the slice's 16th on the grid (the cycle's start + step, swung)
//   less its lead at the kit's BPM, preroll = that lead, len = the slice's frames (end - start) at the
//   kit's BPM (so a chase finds a slice that plays on past its note). It plays the slice, only while the
//   phrase plays slices. NoteFlag::continues when the phrase's next slice note is the slice that follows
//   it in the stem: the next slice in the same cycle, or a looped stem's first slice in the next cycle
//   (round the loop in a loop). Its other fields are its note's; it has no endAligned or dip.
// - Everything else is as it was (phrase = -1). The notes are sorted again by (pos, channel, key),
//   stably; in a loop, a slice note that starts before 0 starts at the loop's end.
// Frames become milliticks at the kit's BPM: frames x bpm / 3, rounded half up (as the pre-roll step).

#include "kit/manifest.h"
#include "sched/timeline.h"

#include <cstdint>

namespace wubansia::engine
{
    void annotatePhrases (Timeline& timeline, const KitManifest& kit);

    // The 16th a position (milliticks) sounds in: odd 16ths start (swingPm - 500) x 480 milliticks late.
    std::int64_t sixteenthOf (std::int64_t pos, std::int32_t swingPm) noexcept;

    // Where a 16th starts, in milliticks.
    std::int64_t sixteenthStart (std::int64_t sixteenth, std::int32_t swingPm) noexcept;
}
