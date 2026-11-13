#pragma once

// The arrangement (plan §2.3): sections on 8-bar lines, each with a kind and the lanes that play in
// each of its 8-bar blocks. Every seed draws its own form from the style's grammar (FormSpec,
// content.h), a DJ-friendly extended mix, the way club extended mixes are put together:
// - the lines: each drop, drop2 and break starts on a 16-bar line with the grammar's chance (about
//   70%, as measured on annotated four-on-the-floor tracks), else on 8 mod 16;
// - the lengths: every slot's length drawn by its weights, the form kept if it keeps those lines and
//   ends inside its length range (200 to 240 bars in the shipped styles, techno 208 to 240); a
//   length of 0 leaves an optional slot out;
// - the lanes: the 32-bar mix-in and mix-out take a whole block plan (drums only at the edges, lanes
//   only joining in the mix-in and only leaving in the mix-out, from the section before it on: the
//   mix-out never takes a choice that brings back a non-drum lane that section ends without). Every
//   other section plays its kind's lanes:
//   - some joining late or leaving early (the entries);
//   - one lane sitting out 8 or 16 bars (the rest; a drum lane always comes back inside the section);
//   - lane groups sitting out the bar before the section's own 16-bar lines, landing back on the line
//     (the dropouts, at least 32 bars apart);
//   - drop 2's layer, drawn per song, in drop 2 and never in drop 1.
//   A kind with "change" has a lane change inside every section. A slot may start its section on its
//   own pattern variant (the groove after drop 1: new patterns on its grids).
// Lane entries and exits therefore fall on 8-bar lines, and dropouts on the last bar of a phrase.
// Variation by hypermeter tier, on top of the form:
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
#include <span>
#include <string_view>
#include <vector>

namespace wubansia::gen
{
    // A section of this kind starts on a drawn line: on a 16-bar line or on 8 mod 16 (drop, drop2, break).
    bool startsOnLine (std::string_view kind) noexcept;

    // The lines asked of each slot: -1 none (a slot whose kind doesn't start on a line), 0 on 8 mod
    // 16, 1 on a 16-bar line.
    using Lines = std::span<const std::int8_t>;

    // True if some lengths of the slots keep those lines and end inside the form's length range.
    bool formFits (const FormSpec& form, Lines lines);

    // The slots' lengths in bars (0: left out): the whole form drawn slot by slot by the weights, kept
    // if it keeps the lines and the length range, up to 1024 times (the grammar's weights given the
    // lines); then, if none was kept, each slot in order by weight among the lengths that leave the
    // rest of the form a way to keep them. Empty if no lengths fit (the loader refuses a grammar where
    // any lines it can draw don't fit).
    std::vector<std::int32_t> formLengths (const FormSpec& form, Lines lines, rng::Key key);

    // The sections of the seed's whole form (a --bars cut comes after the whole song: cut.h). Keys:
    // root.with (tag ("form")).
    std::vector<Section> arrange (const Style& style, rng::Key root);

    // Fills song.changes and song.turnBars. Keys: root.with (tag ("arrange")).
    void planChanges (Song& song, const Style& style, rng::Key root);

    // The pattern variant lane plays at a bar of a section: the section kind's base variant plus the
    // changes for that lane in the section up to and including that bar.
    std::int32_t variantAt (const Song& song, const Style& style, std::size_t lane, std::size_t section, std::int32_t barInSection) noexcept;

    // True if the absolute bar carries a turnaround.
    bool isTurnBar (const Song& song, std::int32_t bar) noexcept;

    // The tick ranges [start, end) where a lane plays: its active bars (its blocks, less its
    // dropouts), merged within a section (never across a section boundary), in order.
    struct TickRange
    {
        std::int64_t start = 0;
        std::int64_t end = 0;
        std::size_t section = 0;
    };
    std::vector<TickRange> activeRanges (const Song& song, std::size_t lane);
}
