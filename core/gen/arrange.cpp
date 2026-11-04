#include "gen/arrange.h"

#include <algorithm>

namespace wubansia::gen
{
    bool Section::active (std::size_t lane, std::int32_t barInSection) const noexcept
    {
        if (barInSection < 0 || barInSection >= bars || lane >= 64)
            return false;
        const auto block = static_cast<std::size_t> (barInSection / barsPerBlock);
        return block < blocks.size() && (blocks[block] & (std::uint64_t (1) << lane)) != 0;
    }

    std::uint64_t Section::lanesAnywhere() const noexcept
    {
        std::uint64_t all = 0;
        for (const auto b : blocks)
            all |= b;
        return all;
    }

    std::vector<Section> arrange (const Style& style)
    {
        std::vector<Section> out;
        std::int32_t start = 0;
        for (std::size_t i = 0; i < style.form.size(); ++i)
        {
            const FormSection& f = style.form[i];
            Section s;
            s.index = i;
            s.kind = f.kind;
            s.startBar = start;
            s.bars = f.bars;
            for (std::int32_t b = 0; b < s.bars / barsPerBlock; ++b)
            {
                std::uint64_t mask = 0;
                for (const auto& id : f.blocks[static_cast<std::size_t> (b)])
                    mask |= std::uint64_t (1) << *style.laneIndex (id);
                s.blocks.push_back (mask);
            }
            start += f.bars;
            out.push_back (std::move (s));
        }
        return out;
    }

    void planChanges (Song& song, const Style& style, rng::Key root)
    {
        const rng::Key ka = root.with (rng::tag ("arrange"));
        const ChangeSpec& spec = style.changes;
        std::vector<std::size_t> roster;
        for (const auto& id : spec.roster)
            roster.push_back (*style.laneIndex (id));

        song.changes.clear();
        song.turnBars.clear();
        for (const Section& s : song.sections)
        {
            const auto add = [&] (std::int32_t bar) {
                std::vector<std::size_t> candidates;
                for (const auto lane : roster)
                    if (s.active (lane, bar))
                        candidates.push_back (lane);
                if (candidates.empty())
                    return;
                const auto pick = rng::pickIndex (ka.with (rng::tag ("who")).with (s.index).with (bar), candidates.size());
                song.changes.push_back ({ s.index, bar, candidates[pick] });
            };
            bool pending = false;
            for (std::int32_t line = barsPerBlock; line < s.bars; line += barsPerBlock)
            {
                if (pending)
                {
                    const bool early = rng::chance (ka.with (rng::tag ("early")).with (s.index).with (line), p16FromPercent (spec.early));
                    add (early ? line - barsPerBlock / 2 : line);
                    pending = false;
                }
                else if (rng::chance (ka.with (rng::tag ("change")).with (s.index).with (line), p16FromPercent (spec.change)))
                    add (line);
                else
                    pending = true;
            }
            for (std::int32_t b = 3; b < s.bars; b += 4)
                if (rng::chance (ka.with (rng::tag ("turn")).with (s.index).with (b), p16FromPercent (spec.turn)))
                    song.turnBars.push_back (s.startBar + b);
        }
    }

    std::int32_t variantAt (const Song& song, const Style& style, std::size_t lane, std::size_t section, std::int32_t barInSection) noexcept
    {
        const SectionSpec* spec = style.section (song.sections[section].kind);
        std::int32_t v = spec != nullptr ? spec->variant : 0;
        for (const auto& c : song.changes)
            if (c.section == section && c.lane == lane && c.bar <= barInSection)
                ++v;
        return v;
    }

    bool isTurnBar (const Song& song, std::int32_t bar) noexcept
    {
        return std::binary_search (song.turnBars.begin(), song.turnBars.end(), bar);
    }

    std::vector<TickRange> activeRanges (const Song& song, std::size_t lane)
    {
        std::vector<TickRange> out;
        for (const Section& s : song.sections)
        {
            bool open = false;
            for (std::int32_t k = 0; k < static_cast<std::int32_t> (s.blocks.size()); ++k)
            {
                const bool on = (s.blocks[static_cast<std::size_t> (k)] & (std::uint64_t (1) << lane)) != 0;
                const std::int64_t at = (s.startBar + k * barsPerBlock) * ticksPerBar;
                if (on && !open)
                {
                    out.push_back ({ at, at, s.index });
                    open = true;
                }
                if (!on)
                    open = false;
                if (on)
                    out.back().end = at + barsPerBlock * ticksPerBar;
            }
        }
        return out;
    }
}
