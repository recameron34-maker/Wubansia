#include "gen/cc.h"

#include "gen/arrange.h"
#include "util/ints.h"
#include "util/sort.h"

#include <algorithm>
#include <tuple>

namespace wubansia::gen
{
    std::int32_t arcValue (std::int32_t from, std::int32_t to, std::int64_t length, std::int32_t depth, std::int64_t period, std::int64_t pos) noexcept
    {
        std::int64_t v = from;
        if (length > 0)
            v += util::roundDiv (static_cast<std::int64_t> (to - from) * pos, length);
        if (period > 0 && depth != 0)
        {
            const std::int64_t k = util::floorMod (pos, period);
            v += util::roundDiv (static_cast<std::int64_t> (depth) * 4 * k * (period - k), period * period);
        }
        return static_cast<std::int32_t> (std::clamp<std::int64_t> (v, 0, 127));
    }

    void scoreControllers (Song& song, const Bundle& bundle, std::size_t li, rng::Key root)
    {
        const LaneSpec& lane = bundle.style.lanes[li];
        LaneOut& out = song.lanes[li];
        out.cc.clear();
        if (lane.cc.empty())
            return;
        const auto ranges = activeRanges (song, li);
        const rng::Key kl = root.with (rng::tag ("cc")).with (rng::tag (lane.id));
        for (const CcSpec& spec : lane.cc)
        {
            const rng::Key kn = kl.with (static_cast<std::uint64_t> (spec.number));
            for (const TickRange& r : ranges)
            {
                const Section& s = song.sections[r.section];
                const CcRow* row = spec.row (s.kind);
                if (row == nullptr)
                    continue; // the loader gives every kind the lane plays a row
                // drawn per section, so every range of a section follows one arc
                const rng::Key ks = kn.with (s.index);
                const std::int32_t from = rng::between (ks.with (rng::tag ("from")), row->from.lo, row->from.hi);
                const std::int32_t to = rng::between (ks.with (rng::tag ("to")), row->to.lo, row->to.hi);
                std::int32_t depth = 0;
                std::int64_t period = 0;
                if (!row->waveBars.empty())
                {
                    period = row->waveBars[rng::pickIndex (ks.with (rng::tag ("wave")), row->waveBars.size())] * ticksPerBar;
                    depth = rng::between (ks.with (rng::tag ("depth")), row->depth.lo, row->depth.hi);
                }
                const std::int64_t length = static_cast<std::int64_t> (s.bars) * ticksPerBar;
                std::int32_t last = -1; // the first point of a range is always written
                for (std::int64_t t = r.start; t < r.end; t += spec.step)
                {
                    const std::int32_t v = arcValue (from, to, length, depth, period, t - s.startTick());
                    if (v == last)
                        continue;
                    out.cc.push_back ({ t, static_cast<std::uint8_t> (spec.number), static_cast<std::uint8_t> (v), static_cast<std::int16_t> (s.index) });
                    last = v;
                }
            }
        }
        util::stableSortBy (out.cc, [] (const ControlPoint& c) { return std::make_tuple (c.tick, c.number); });
    }
}
