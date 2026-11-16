#pragma once

// A --bars cut (groovecore gen --bars N, Options::bars): the whole song's head at bar N, by the
// score: the notes that start in its first N bars in the score, each with its performance. The
// generator always makes the whole form, and cutSong cuts the finished song, so nothing in the
// head depends on where it is cut and every stage keeps its whole-song rules:
// - each score note whose onset is before the cut stays, with its performed note (perf[i] plays
//   score[i]); both end by the cut, and a performed onset at or after it (only at the feel's
//   extremes, a late note on the last 16th before an 8-bar line) plays on the cut's last tick, as
//   it does on a section's last 16th. A note on the cut's line goes, so a performance of one that
//   plays a few ticks early, inside the first N bars by time, isn't in the cut. Flags stay the
//   whole song's.
// - the controllers before the cut stay.
// - the hook (hook.h) keeps the whole song's material, its lines that start before the cut (their
//   masks as they are) and its notes that start before it, ended by it.
// - the sections that start before the cut stay, the last one shortened to the cut, its blocks
//   and its one-bar dropouts too, so the export writes the whole song's regions with the last one
//   shortened (its file name, its marker and its regions.json entry give the new length); one
//   harmony per section stays.
// - the turnaround bars, the salient changes and the chord spans before the cut stay, a span that
//   crosses it ended by it.
// Integer work only, and linear in the song's size.

#include "gen/song.h"
#include "util/result.h"

#include <cstdint>

namespace wubansia::gen
{
    // song cut at bar `bars`, which must be a multiple of 8 from 8 to song.totalBars (the whole
    // song, unchanged). Fails, saying why, on any other value.
    [[nodiscard]] util::Result<Song> cutSong (Song song, std::int32_t bars);
}
