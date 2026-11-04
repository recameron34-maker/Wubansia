#pragma once

// CC lanes (plan §2.3: "CC74 filter arcs"; the techno brief: loops that evolve by filter moves rather
// than chord changes). A lane's "cc" lists controllers, and each has a row per section kind the lane
// plays in: a straight ramp from `from` at the section's start to `to` at its planned end (both drawn
// per section), plus, if the row has a wave, one arch every `waveBars` bars (a length drawn per
// section) whose height at its middle is `depth` (drawn per section; below 0 it dips), clamped to
// 0-127. Integer arithmetic only.
//
// Points go every `step` ticks inside the lane's active ranges, only where the value changes, plus
// one at the start of every range: a region file opens on the value in force, and the controller is
// set before the lane's first note there. The ramp runs over the section's planned length, so a
// --bars cut keeps the whole song's values. Keys: root.with (tag ("cc")).with (tag (lane)).with
// (number).with (section): no other lane's draws move them. Automation, not feel: both versions of
// every file carry the same points.

#include "gen/content.h"
#include "gen/song.h"
#include "rng/rng.h"

#include <cstddef>
#include <cstdint>

namespace wubansia::gen
{
    // The controller's value pos ticks into a section `length` ticks long: from + (to - from) * pos /
    // length (rounded half up), plus the arch of the period holding pos, depth * 4 k (period - k) /
    // period^2 with k = pos mod period (rounded half up; period 0: no arch), clamped to 0-127.
    std::int32_t arcValue (std::int32_t from, std::int32_t to, std::int64_t length, std::int32_t depth, std::int64_t period, std::int64_t pos) noexcept;

    // Fills song.lanes[lane].cc from the lane's cc specs (nothing without any).
    void scoreControllers (Song& song, const Bundle& bundle, std::size_t lane, rng::Key root);
}
