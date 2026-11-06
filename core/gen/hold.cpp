#include "gen/hold.h"

#include "gen/arrange.h"

#include <algorithm>
#include <cstdint>
#include <map>
#include <vector>

namespace wubansia::gen
{
    void holdPads (Song& song, const Bundle& bundle, std::size_t li)
    {
        const LaneSpec& lane = bundle.style.lanes[li];
        if (lane.kind != LaneKind::chords)
            return;
        // the gap each section's pad leaves before a chord's end, or -1 where the section doesn't hold
        std::vector<std::int64_t> gap (song.sections.size(), -1);
        bool any = false;
        for (const Section& s : song.sections)
        {
            const std::string_view name = lane.gridFor (s.kind);
            for (const ChordGrid& g : lane.chords.grids)
                if (g.name == name && g.pad && g.padHold)
                {
                    gap[s.index] = g.padGap;
                    any = true;
                }
        }
        if (!any)
            return;
        const auto ranges = activeRanges (song, li);
        const auto rangeOf = [&ranges] (std::int64_t tick) -> std::size_t {
            for (std::size_t r = 0; r < ranges.size(); ++r)
                if (tick >= ranges[r].start && tick < ranges[r].end)
                    return r;
            return ranges.size();
        };
        std::vector<Note>& notes = song.lanes[li].score; // in (tick, key, ...) order
        std::vector<bool> held (notes.size(), false);
        std::map<std::uint8_t, std::size_t> last; // key -> the note that sounds it now
        for (std::size_t i = 0; i < notes.size(); ++i)
        {
            const Note& n = notes[i];
            const std::int64_t g = gap[static_cast<std::size_t> (n.section)];
            const auto it = last.find (n.key);
            if (g >= 0 && it != last.end())
            {
                Note& p = notes[it->second];
                if (p.section == n.section && p.tick + p.length + g == n.tick && rangeOf (p.tick) == rangeOf (n.tick))
                {
                    p.length = n.tick + n.length - p.tick; // held over: the same key, struck again straight after itself
                    held[i] = true;
                    continue;
                }
            }
            last[n.key] = i;
        }
        std::size_t k = 0;
        for (std::size_t i = 0; i < notes.size(); ++i)
            if (!held[i])
                notes[k++] = notes[i];
        notes.resize (k);
        // a stack struck again only in part (its common tones held) is a smaller stack: number its voices
        // again from 0, low to high, as the feel's spread reads them
        for (std::size_t i = 0; i < notes.size();)
        {
            std::size_t j = i;
            while (j < notes.size() && notes[j].tick == notes[i].tick)
                ++j;
            for (std::size_t v = i; v < j; ++v)
                notes[v].voice = static_cast<std::uint8_t> (v - i);
            i = j;
        }
    }
}
