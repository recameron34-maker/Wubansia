#pragma once

// Held pads, for drones (the techno brief: a pedal that evolves by filter moves, not chord
// changes). A pad grid strikes its voicing again on every chord span; with the pad's "hold", a key
// the pad strikes again straight after itself (the next span starts where this note's gap ends:
// a pedal's next loop, a recurring chord with the same voicing) is held over instead, as one note.
// So a span that keeps its voicing strikes nothing, and a re-voiced span (voice leading moving an
// inner voice, a variant change on a phrase line) strikes only the keys that move while the common
// tones hold. Only within a section and an active range, so region files and --bars cuts are
// untouched by it. Runs on the score after scoreChords; the feel then plays the held notes.

#include "gen/content.h"
#include "gen/song.h"

#include <cstddef>

namespace wubansia::gen
{
    void holdPads (Song& song, const Bundle& bundle, std::size_t lane);
}
