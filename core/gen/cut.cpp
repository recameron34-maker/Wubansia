#include "gen/cut.h"

#include <algorithm>
#include <string>
#include <utility>

namespace wubansia::gen
{
    util::Result<Song> cutSong (Song song, std::int32_t bars)
    {
        using R = util::Result<Song>;
        if (bars <= 0 || bars % barsPerBlock != 0)
            return R::failure ("bars must be a multiple of 8 (sections and lane changes sit on 8-bar lines), not " + std::to_string (bars));
        if (bars > song.totalBars)
            return R::failure ("the form has only " + std::to_string (song.totalBars) + " bars, so it can't be cut at " + std::to_string (bars));
        if (bars == song.totalBars)
            return R::success (std::move (song));
        const std::int64_t end = static_cast<std::int64_t> (bars) * ticksPerBar;

        // the arrangement: the sections that start before the cut, the last one (its blocks and dropouts) shortened
        std::erase_if (song.sections, [&] (const Section& s) { return s.startBar >= bars; });
        Section& last = song.sections.back();
        last.bars = std::min (last.bars, bars - last.startBar);
        last.blocks.resize (static_cast<std::size_t> (last.bars / barsPerBlock));
        std::erase_if (last.dropouts, [&] (const Dropout& d) { return d.bar >= last.bars; });
        song.harmony.resize (song.sections.size());
        std::erase_if (song.changes, [&] (const Change& c) { return c.section >= song.sections.size() || c.bar >= song.sections[c.section].bars; });
        std::erase_if (song.turnBars, [&] (std::int32_t b) { return b >= bars; });
        std::erase_if (song.chords, [&] (const ChordSpan& c) { return c.start >= end; });
        for (ChordSpan& c : song.chords)
            c.end = std::min (c.end, end);
        song.totalBars = bars;

        // the lanes: the notes whose score onsets are before the cut, both versions ended by it
        for (LaneOut& lane : song.lanes)
        {
            std::size_t kept = 0;
            for (std::size_t i = 0; i < lane.score.size(); ++i)
            {
                Note s = lane.score[i];
                if (s.tick >= end)
                    continue;
                Note p = lane.perf[i];
                s.length = std::min (s.length, end - s.tick);
                const std::int64_t stop = std::min (p.tick + p.length, end);
                p.tick = std::min (p.tick, end - 1);
                p.length = std::max<std::int64_t> (1, stop - p.tick);
                lane.score[kept] = s;
                lane.perf[kept] = p;
                ++kept;
            }
            lane.score.resize (kept);
            lane.perf.resize (kept);
            std::erase_if (lane.cc, [&] (const ControlPoint& c) { return c.tick >= end; });
        }
        return R::success (std::move (song));
    }
}
