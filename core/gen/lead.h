#pragma once

// The lead lane (S1b2b): the hook's carrier (hook.h). Its score is song.hook's notes that lie inside
// the lane's active ranges (arrange.h), so a bar a one-bar pull takes it out of has none; each
// velocity is scaled by the lane grid's vel and the section's vel, each length by the grid's len
// (never into the next note, 10 ticks short, nor past its range or its 8-bar line). Each hook line's
// half-bar mask comes from these final notes. FUT02's arp carries the hook the same way, as kind lead.

#include "gen/content.h"
#include "gen/song.h"

#include <cstddef>

namespace wubansia::gen
{
    // Fills song.lanes[lane].score for a lead lane from song.hook (nothing without one), and the masks
    // of song.hook's lines.
    void scoreLead (Song& song, const Bundle& bundle, std::size_t lane);
}
