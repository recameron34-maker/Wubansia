#pragma once

// The kick duck: a sidechain written into the MIDI. A bass lane with "duck": d leaves d ticks free
// before every onset of the drum lane it follows, so none of its notes rings into a kick (the techno
// rumble, the rolling basses that rest on the kick), and before its section's end, so none rings into
// the next section either. It counts every onset of the follow lane, ornaments and turnaround kicks
// too (the bass follows only the structural kicks, so it can't know of those otherwise), in the
// note's own section: a later section's onsets start on or after its end, which the gap already
// keeps clear, so a section's notes never depend on the next section's (ADR 0013 §3).
//
// duckScore runs on the score after scoreBass, against the follow lane's score; duckPerformance runs
// on the performance after the feel, against the follow lane's performed onsets, so the gap holds in
// the with-feel files too. A note is only ever shortened, to no less than 1 tick: the loader keeps d at
// 90 or below, so a score note on the 16th grid keeps 30 ticks or more, and the feels keep their
// ghosts' delays small enough that performed notes do too (the tests check both). A slide that no
// longer reaches the next note loses its slide flag.

#include "gen/content.h"
#include "gen/song.h"

#include <cstddef>

namespace wubansia::gen
{
    void duckScore (Song& song, const Bundle& bundle, std::size_t lane);
    void duckPerformance (Song& song, const Bundle& bundle, std::size_t lane);
}
