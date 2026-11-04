#include "gen/arp.h"

#include "gen/arrange.h"
#include "gen/harmony.h"
#include "gen/lanes.h"
#include "gen/rhythm.h"
#include "util/ints.h"

#include <algorithm>
#include <array>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace wubansia::gen
{
    std::int32_t placedRoot (std::int32_t rootPc, std::int32_t center) noexcept
    {
        const std::int32_t below = center - util::floorMod (center - rootPc, 12); // at or below center
        const std::int32_t above = below + 12;
        return center - below <= above - center ? below : above;
    }

    std::int32_t ladderKey (const ArpGrid& grid, const ChordSpan& chord, std::int32_t center, std::int32_t index, Range range) noexcept
    {
        std::int32_t key = placedRoot (chord.rootPc, center) + grid.octave;
        const auto n = static_cast<std::int32_t> (grid.tones.size());
        if (n > 0)
        {
            const std::int32_t steps = grid.tones[static_cast<std::size_t> (util::floorMod (index, n))] + 7 * util::floorDiv (index, n);
            key += chordInterval (chord.mode, chord.degree, steps);
        }
        // into the range by octaves, however far outside (the loader allows ladder indices up to 30
        // and an octave of +-24): a range spans an octave or more (makeBundle), so one direction does
        if (key > range.hi)
            key -= 12 * util::ceilDiv (key - range.hi, 12);
        if (key < range.lo)
            key += 12 * util::ceilDiv (range.lo - key, 12);
        return key;
    }

    namespace
    {
        std::uint8_t clampVelocity (std::int64_t v) noexcept { return static_cast<std::uint8_t> (std::clamp<std::int64_t> (v, 1, 127)); }
        std::int64_t scaled (std::int64_t v, std::int32_t permille) noexcept { return util::roundDiv (v * permille, std::int64_t (1000)); }

        const TickRange* rangeAt (const std::vector<TickRange>& ranges, std::int64_t tick) noexcept
        {
            for (const TickRange& r : ranges)
                if (tick >= r.start && tick < r.end)
                    return &r;
            return nullptr;
        }

        // One variant of an arp grid: the 16ths it plays, and the ladder index each one takes.
        struct ArpPattern
        {
            StepMask hits = 0;
            std::array<std::int32_t, gridSteps> index {};
        };

        ArpPattern drawArp (const ArpGrid& g, rng::Key kv)
        {
            ArpPattern p;
            p.hits = drawCells (g.p, kv.with (rng::tag ("rhythm")), 1, 0);
            std::vector<std::int32_t> weights;
            for (const auto& c : g.cells)
                weights.push_back (c.weight);
            const std::size_t pick = rng::pickWeighted (kv.with (rng::tag ("cell")), weights);
            const ArpCell& cell = g.cells[pick < g.cells.size() ? pick : 0];
            const std::int32_t shift = rng::between (kv.with (rng::tag ("shift")), g.shift.lo, g.shift.hi);
            // the cell runs over the steps that play, from the 2-bar pattern's start and again at every restart step
            std::size_t pos = 0;
            for (std::size_t s = 0; s < gridSteps; ++s)
            {
                const auto barStep = static_cast<std::int32_t> (s % barSteps);
                if (s == 0 || std::find (g.restart.begin(), g.restart.end(), barStep) != g.restart.end())
                    pos = 0;
                if (((p.hits >> s) & 1u) == 0)
                    continue;
                p.index[s] = cell.idx[pos % cell.idx.size()] + shift;
                ++pos;
            }
            return p;
        }
    }

    void scoreArp (Song& song, const Bundle& bundle, std::size_t li, rng::Key root)
    {
        const Style& style = bundle.style;
        const LaneSpec& lane = style.lanes[li];
        const ArpLane& arp = lane.arp;
        const KitLane& kit = *bundle.kit.lane (lane.id);
        LaneOut& out = song.lanes[li];
        out.score.clear();
        const rng::Key ka = root.with (rng::tag ("lane")).with (rng::tag (lane.id));
        const auto ranges = activeRanges (song, li);

        std::map<std::pair<std::string, std::int32_t>, ArpPattern> patterns; // (grid, variant) -> its pattern
        std::vector<Note> notes;
        for (const Section& s : song.sections)
        {
            const std::string_view gridName = lane.gridFor (s.kind);
            if (gridName.empty())
                continue;
            const ArpGrid& g = *std::find_if (arp.grids.begin(), arp.grids.end(), [&] (const ArpGrid& x) { return x.name == gridName; });
            const std::int32_t velScale = style.section (s.kind)->vel;
            for (std::int32_t bar = 0; bar < s.bars; ++bar)
            {
                if (!s.active (li, bar))
                    continue;
                const std::int32_t variant = variantAt (song, style, li, s.index, bar);
                const auto pk = std::make_pair (std::string (gridName), variant);
                auto it = patterns.find (pk);
                if (it == patterns.end())
                    it = patterns.emplace (pk, drawArp (g, ka.with (rng::tag (gridName)).with (variant))).first;
                const ArpPattern& pat = it->second;
                const std::int64_t barStep = static_cast<std::int64_t> (s.startBar + bar) * stepsPerBar;
                for (std::int32_t j = 0; j < 16; ++j)
                {
                    const auto cell = static_cast<std::size_t> ((bar % 2) * 16 + j);
                    if (((pat.hits >> cell) & 1u) == 0)
                        continue;
                    const std::int64_t step = barStep + j;
                    const std::int64_t tick = step * ticksPerStep;
                    const ChordSpan* chord = chordAt (song, tick); // pushed changes included
                    if (chord == nullptr)
                        continue;
                    const std::int32_t key = ladderKey (g, *chord, arp.center, pat.index[cell], kit.range);
                    const std::int32_t vel = g.vel[cell] + rng::between (ka.with (rng::tag ("vel")).with (s.index).with (bar).with (j), -g.spread, g.spread);
                    Note n;
                    n.tick = tick;
                    n.step = step;
                    n.length = g.len[cell];
                    n.key = static_cast<std::uint8_t> (key);
                    n.velocity = clampVelocity (scaled (vel, velScale));
                    n.tags = inMinorKey (song.tonic, pitchClass (key)) ? 0 : noteTag::borrowed;
                    n.section = static_cast<std::int16_t> (s.index);
                    notes.push_back (n);
                }
            }
        }
        // lengths: never into the next note (a fresh pluck), never out of the lane's range
        for (std::size_t i = 0; i < notes.size(); ++i)
        {
            const TickRange* r = rangeAt (ranges, notes[i].tick);
            std::int64_t limit = r != nullptr ? r->end : notes[i].tick + notes[i].length;
            if (i + 1 < notes.size() && notes[i + 1].tick < limit)
                limit = notes[i + 1].tick - 10;
            notes[i].length = std::max<std::int64_t> (1, std::min (notes[i].length, limit - notes[i].tick));
        }
        sortNotes (notes);
        out.score = std::move (notes);
    }
}
