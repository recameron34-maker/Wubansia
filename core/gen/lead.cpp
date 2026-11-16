// The lead lane (lead.h): the hook's realized notes inside the lane's active ranges, scaled by its
// grid and its section; then each hook line's half-bar mask, from these final notes.

#include "gen/lead.h"

#include "gen/arrange.h"
#include "gen/lanes.h"
#include "util/ints.h"

#include <algorithm>
#include <cstdint>
#include <string_view>
#include <vector>

namespace wubansia::gen
{
    namespace
    {
        std::uint8_t clampVelocity (std::int64_t v) noexcept { return static_cast<std::uint8_t> (std::clamp<std::int64_t> (v, 1, 127)); }
        std::int64_t scaled (std::int64_t v, std::int32_t permille) noexcept { return util::roundDiv (v * permille, std::int64_t (1000)); }

        constexpr std::int64_t ticksPerLine = barsPerBlock * ticksPerBar;
        constexpr std::int64_t ticksPerHalf = ticksPerBar / 2;
    }

    void scoreLead (Song& song, const Bundle& bundle, std::size_t li)
    {
        LaneOut& out = song.lanes[li];
        out.score.clear();
        if (!song.hook || song.hook->lane != li)
            return;
        const Style& style = bundle.style;
        const LaneSpec& lane = style.lanes[li];
        const auto ranges = activeRanges (song, li); // the blocks planHook left it, less its pulls

        // the hook's notes inside the ranges (song.hook's are in time order, as the ranges are)
        std::vector<Note> notes;
        std::vector<std::int64_t> limits; // where each note must end: its range's end or its line's, whichever is first
        std::size_t r = 0;
        for (const Note& n : song.hook->notes)
        {
            while (r < ranges.size() && ranges[r].end <= n.tick)
                ++r;
            if (r >= ranges.size() || n.tick < ranges[r].start)
                continue; // a bar a pull takes the lane out of
            const Section& sec = song.sections[static_cast<std::size_t> (n.section)];
            const std::string_view gridName = lane.gridFor (sec.kind);
            std::int32_t vel = 1000, len = 1000;
            for (const LeadGrid& g : lane.lead.grids)
                if (g.name == gridName)
                {
                    vel = g.vel;
                    len = g.len;
                }
            const SectionSpec* kind = style.section (sec.kind);
            Note x = n;
            x.velocity = clampVelocity (scaled (scaled (n.velocity, vel), kind != nullptr ? kind->vel : 1000));
            x.length = std::max<std::int64_t> (1, scaled (n.length, len));
            notes.push_back (x);
            const std::int64_t lineEnd = sec.startTick() + (util::floorDiv (n.tick - sec.startTick(), ticksPerLine) + 1) * ticksPerLine;
            limits.push_back (std::min (ranges[r].end, lineEnd));
        }
        // lengths: never into the next note (a fresh pluck, 10 ticks short), never past the range or the line
        for (std::size_t i = 0; i < notes.size(); ++i)
        {
            std::int64_t limit = limits[i];
            if (i + 1 < notes.size() && notes[i + 1].tick < limit)
                limit = notes[i + 1].tick - 10;
            notes[i].length = std::max<std::int64_t> (1, std::min (notes[i].length, limit - notes[i].tick));
        }
        sortNotes (notes);
        out.score = std::move (notes);

        // the half-bar masks: bit 2 * bar + half where a note of the lane starts in the half or sounds into it
        for (HookLine& l : song.hook->lines)
        {
            l.mask = 0;
            const Section& sec = song.sections[l.section];
            const std::int64_t from = sec.startTick() + static_cast<std::int64_t> (l.bar) * ticksPerBar;
            const auto first = std::lower_bound (out.score.begin(), out.score.end(), from, [] (const Note& n, std::int64_t t) { return n.tick < t; });
            for (auto it = first; it != out.score.end() && it->tick < from + ticksPerLine; ++it)
            {
                const std::int64_t a = (it->tick - from) / ticksPerHalf;
                const std::int64_t b = std::min<std::int64_t> ((it->tick + it->length - 1 - from) / ticksPerHalf, 2 * barsPerBlock - 1);
                for (std::int64_t half = a; half <= b; ++half)
                    l.mask = static_cast<std::uint16_t> (l.mask | (1u << half));
            }
        }
    }
}
