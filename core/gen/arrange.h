#pragma once

// The arrangement (plan §2.3): sections on 8-bar lines, each with a kind and the lanes that play in
// each of its 8-bar blocks, from the style's default form, a DJ-friendly extended mix. Lane
// entries and exits therefore fall on 8-bar lines. Variation by hypermeter tier:
// - every 4-bar line: a turnaround in its last bar, with the style's probability (about 0.5)
// - every 8-bar line inside a section: one salient change (about 0.6-0.8): one lane of the roster
//   moves to its next pattern variant. Otherwise the change is held to the next 8-bar line or placed
//   4 bars early there (a pre-change at bar 12 of 16).
// Both are drawn from song-level keys, never from a lane's own parameters, so they don't tie lanes
// together.

#include "gen/content.h"
#include "gen/song.h"
#include "rng/rng.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace wubansia::gen
{
    // The sections of the style's whole form (a --bars cut comes after the whole song: cut.h).
    std::vector<Section> arrange (const Style& style);

    // Fills song.changes and song.turnBars. Keys: root.with (tag ("arrange")).
    void planChanges (Song& song, const Style& style, rng::Key root);

    // The pattern variant lane plays at a bar of a section: the section kind's base variant plus the
    // changes for that lane in the section up to and including that bar.
    std::int32_t variantAt (const Song& song, const Style& style, std::size_t lane, std::size_t section, std::int32_t barInSection) noexcept;

    // True if the absolute bar carries a turnaround.
    bool isTurnBar (const Song& song, std::int32_t bar) noexcept;

    // The tick ranges [start, end) where a lane plays: its active blocks, merged within a section
    // (never across a section boundary), in order.
    struct TickRange
    {
        std::int64_t start = 0;
        std::int64_t end = 0;
        std::size_t section = 0;
    };
    std::vector<TickRange> activeRanges (const Song& song, std::size_t lane);
}
