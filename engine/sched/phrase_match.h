#pragma once

// Phrase matching (P6): ties a timeline's notes on phrase lanes to the recorded slices of their
// stems, on the service thread, after buildTimeline's other steps.
//
// One cycle (cycle_16ths, aligned to the song grid) at a time, a cycle's notes on the phrase's
// channel are matched as a set, by (16th in the cycle, key), against each stem's slice pattern. A
// subset matches, so a 'call' line that plays only part of the phrase still plays its slices; of
// several phrases on one channel the one with the most matches wins. Matched notes get Note::phrase,
// Note::slice and NoteFlag::continues (the next slice follows contiguously); every other note stays
// on the multisample.
//
// P0's stub (phrase_match.cpp) leaves every note on the multisample.

#include "kit/manifest.h"
#include "sched/timeline.h"

namespace wubansia::engine
{
    void annotatePhrases (Timeline& timeline, const KitManifest& kit);
}
