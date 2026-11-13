#include "gen/arrange.h"

#include <algorithm>
#include <optional>

namespace wubansia::gen
{
    bool Section::active (std::size_t lane, std::int32_t barInSection) const noexcept
    {
        return lane < 64 && (lanesAt (barInSection) & (std::uint64_t (1) << lane)) != 0;
    }

    std::uint64_t Section::lanesAt (std::int32_t barInSection) const noexcept
    {
        if (barInSection < 0 || barInSection >= bars)
            return 0;
        const auto block = static_cast<std::size_t> (barInSection / barsPerBlock);
        if (block >= blocks.size())
            return 0;
        std::uint64_t lanes = blocks[block];
        for (const Dropout& d : dropouts)
            if (d.bar == barInSection)
                lanes &= ~d.lanes;
        return lanes;
    }

    std::uint64_t Section::lanesAnywhere() const noexcept
    {
        std::uint64_t all = 0;
        for (const auto b : blocks)
            all |= b;
        return all;
    }

    bool startsOnLine (std::string_view kind) noexcept { return kind == "drop" || kind == "drop2" || kind == "break"; }

    namespace
    {
        // The form's arithmetic runs in units of an 8-bar block: a start on a 16-bar line is an even unit.
        constexpr std::int32_t unitBars = barsPerBlock;
        // Free draws of the whole form before the slot-by-slot fallback (formLengths).
        constexpr std::int32_t formAttempts = 1024;

        bool keepsLine (const FormSpec& form, Lines lines, std::size_t slot, std::int32_t unit, std::int32_t bars) noexcept
        {
            if (bars == 0 || slot >= lines.size() || lines[slot] < 0 || !startsOnLine (form.slots[slot].kind))
                return true; // left out, or a kind without a line
            return (unit % 2 == 0) == (lines[slot] == 1);
        }

        // reach[i][u]: from slot i at unit u, slots i.. can keep their lines and end inside form.bars.
        std::vector<std::vector<std::uint8_t>> reachable (const FormSpec& form, Lines lines)
        {
            const std::int32_t hi = form.bars.hi / unitBars;
            const std::int32_t lo = (form.bars.lo + unitBars - 1) / unitBars;
            const std::size_t n = form.slots.size();
            std::vector<std::vector<std::uint8_t>> reach (n + 1, std::vector<std::uint8_t> (static_cast<std::size_t> (std::max (hi, 0) + 1), 0));
            for (std::int32_t u = std::max (lo, 0); u <= hi; ++u)
                reach[n][static_cast<std::size_t> (u)] = 1;
            for (std::size_t i = n; i-- > 0;)
                for (std::int32_t u = 0; u <= hi; ++u)
                    for (const Weighted& w : form.slots[i].bars)
                    {
                        const std::int32_t next = u + w.value / unitBars;
                        if (w.weight > 0 && next <= hi && keepsLine (form, lines, i, u, w.value) && reach[i + 1][static_cast<std::size_t> (next)] != 0)
                        {
                            reach[i][static_cast<std::size_t> (u)] = 1;
                            break;
                        }
                    }
            return reach;
        }

        std::uint64_t maskOf (const Style& style, const std::vector<std::string>& ids)
        {
            std::uint64_t m = 0;
            for (const auto& id : ids)
                if (const auto li = style.laneIndex (id))
                    m |= std::uint64_t (1) << *li;
            return m;
        }

        std::uint64_t bitOf (const Style& style, const std::string& id)
        {
            const auto li = style.laneIndex (id);
            return li ? std::uint64_t (1) << *li : 0;
        }

        // The style's drum lanes (kick, snare, hats...), as a mask.
        std::uint64_t drumLanes (const Style& style)
        {
            std::uint64_t m = 0;
            for (std::size_t i = 0; i < style.lanes.size() && i < 64; ++i)
                if (style.lanes[i].kind == LaneKind::drums)
                    m |= std::uint64_t (1) << i;
            return m;
        }

        // Pulls (one-bar dropouts) inside one section come at least this many bars apart: a pull
        // before every 16-bar line is the same edit on a loop.
        constexpr std::int32_t pullSpacing = 32;

        // A section's blocks and dropouts from its kind's plan. layer: drop 2's layer lane (0 for none).
        // prevEnd: the lanes the section before it plays in its last bar, if there is one (a mix-out
        // choice that would bring back a non-drum lane it lacks is never drawn).
        void planBlocks (Section& s, const Style& style, const KindPlan& plan, std::uint64_t layer, std::optional<std::uint64_t> prevEnd, rng::Key kf)
        {
            const auto blockCount = static_cast<std::size_t> (s.bars / barsPerBlock);
            const std::uint64_t drums = drumLanes (style);
            if (!plan.choices.empty())
            {
                // lanes only leave from the section before the mix-out on: a choice whose first block
                // brings back a non-drum lane that section ends without weighs 0 (drums may come back,
                // the kick above all: a DJ mixes on it). The loader keeps a choice for every way the
                // section before can end, so the draw never runs out (the plain weights if it did).
                std::vector<std::int32_t> weights, plain;
                for (const BlockChoice& c : plan.choices)
                {
                    const bool back = prevEnd && !c.blocks.empty() && (maskOf (style, c.blocks.front()) & ~*prevEnd & ~drums) != 0;
                    weights.push_back (back ? 0 : c.weight);
                    plain.push_back (c.weight);
                }
                const rng::Key kc = kf.with (rng::tag ("choice")).with (s.index);
                std::size_t pick = rng::pickWeighted (kc, weights);
                if (pick >= plan.choices.size())
                    pick = rng::pickWeighted (kc, plain);
                const BlockChoice& c = plan.choices[pick];
                for (std::size_t k = 0; k < blockCount; ++k)
                    s.blocks.push_back (k < c.blocks.size() ? maskOf (style, c.blocks[k]) : 0);
                return;
            }
            s.blocks.assign (blockCount, maskOf (style, plan.lanes));

            // the entries: lanes joining late, playing only the end, or leaving early
            for (std::size_t e = 0; e < plan.entries.size(); ++e)
            {
                const LaneEntry& entry = plan.entries[e];
                std::vector<std::int32_t> weights;
                for (const Weighted& w : entry.bars)
                    weights.push_back (w.weight);
                const std::int32_t bars = entry.bars[rng::pickWeighted (kf.with (rng::tag ("entry")).with (s.index).with (e), weights)].value;
                const auto n = static_cast<std::size_t> (std::min (bars, s.bars) / barsPerBlock);
                const std::uint64_t lanes = maskOf (style, entry.lanes);
                // the blocks [from, to) the lanes sit out
                std::size_t from = 0, to = n; // after: the first n
                if (entry.entry == Entry::last)
                    to = blockCount - n; // last: all but the last n
                else if (entry.entry == Entry::before)
                {
                    from = blockCount - n; // before: the last n
                    to = blockCount;
                }
                for (std::size_t k = from; k < to; ++k)
                    s.blocks[k] &= ~lanes;
            }

            // drop 2's layer: drop 1 never plays it (drop 2 has it in its lanes: the loader checks)
            if (s.kind == "drop")
                for (auto& b : s.blocks)
                    b &= ~layer;

            // the rest: one lane, drawn by weight, out for rest.bars bars from a multiple of them, never
            // the first; a drum lane never through the section's last block, so the kick (or the hats)
            // comes back inside the section, never as the next one starts
            const auto rest = [&] (bool forced) {
                if (!plan.rest)
                    return false;
                const RestSpec& r = *plan.rest;
                const rng::Key kr = kf.with (rng::tag ("rest")).with (s.index);
                if (!forced && !rng::chance (kr.with (rng::tag ("p")), p16FromPercent (r.p)))
                    return false;
                // the starts a lane can take: it plays all through them (and isn't drop 2's layer there)
                const auto startsOf = [&] (const LaneWeight& lw) {
                    std::vector<std::int32_t> starts;
                    const std::uint64_t bit = bitOf (style, lw.lane);
                    if (bit == 0 || (s.kind == "drop2" && bit == layer))
                        return starts;
                    const bool drum = (bit & drums) != 0;
                    for (std::int32_t at = r.bars; at + r.bars <= s.bars; at += r.bars)
                    {
                        if (drum && at + r.bars == s.bars)
                            continue;
                        bool on = true;
                        for (auto k = static_cast<std::size_t> (at / barsPerBlock); k < static_cast<std::size_t> ((at + r.bars) / barsPerBlock) && on; ++k)
                            on = (s.blocks[k] & bit) != 0;
                        if (on)
                            starts.push_back (at);
                    }
                    return starts;
                };
                std::vector<std::int32_t> weights;
                for (const LaneWeight& lw : r.lanes)
                    weights.push_back (startsOf (lw).empty() ? 0 : lw.weight);
                const std::size_t pick = rng::pickWeighted (kr.with (rng::tag ("lane")), weights);
                if (pick >= r.lanes.size())
                    return false;
                const auto starts = startsOf (r.lanes[pick]);
                const std::int32_t at = starts[rng::pickIndex (kr.with (rng::tag ("at")), starts.size())];
                for (auto k = static_cast<std::size_t> (at / barsPerBlock); k < static_cast<std::size_t> ((at + r.bars) / barsPerBlock); ++k)
                    s.blocks[k] &= ~bitOf (style, r.lanes[pick].lane);
                return true;
            };

            // a dropout in the bar before the section's own line: a lane group drawn among those with a
            // lane playing there and none leaving on the line, so the whole group (its lanes that play
            // the bar: one whose rest ends on the line is out already) sits the bar out and slams back
            // on the line. Before a line a group's lane leaves on (the kick's rest starting there), the
            // rest of the group alone would be a gap with nothing landing after it: not drawn
            const auto dropout = [&] (std::int32_t line, bool forced) {
                const DropoutSpec& d = *plan.dropout;
                const rng::Key kd = kf.with (rng::tag ("dropout")).with (s.index).with (line);
                if (!forced && !rng::chance (kd.with (rng::tag ("p")), p16FromPercent (d.p)))
                    return false;
                const std::int32_t bar = line - 1;
                const std::uint64_t here = s.lanesAt (bar), leaving = here & ~s.lanesAt (line);
                std::vector<std::int32_t> weights;
                for (const LaneGroup& g : d.lanes)
                {
                    const std::uint64_t m = maskOf (style, g.lanes);
                    weights.push_back ((m & here) != 0 && (m & leaving) == 0 ? g.weight : 0);
                }
                const std::size_t pick = rng::pickWeighted (kd.with (rng::tag ("lanes")), weights);
                if (pick >= d.lanes.size())
                    return false;
                s.dropouts.push_back ({ bar, maskOf (style, d.lanes[pick].lanes) & here });
                return true;
            };

            (void) rest (false);
            // a line less than pullSpacing bars after the section's last pull isn't drawn
            if (plan.dropout)
                for (std::int32_t line = plan.dropout->every, last = -pullSpacing; line < s.bars; line += plan.dropout->every)
                    if (line - last >= pullSpacing && dropout (line, false))
                        last = line;

            // a kind with "change": a lane change inside every section, a rest else a dropout if the draws gave none
            const auto changes = [&s] {
                for (std::int32_t bar = 1; bar < s.bars; ++bar)
                    if (s.lanesAt (bar) != s.lanesAt (bar - 1))
                        return true;
                return false;
            };
            if (plan.change && !changes() && !rest (true) && plan.dropout)
                for (std::int32_t line = plan.dropout->every; line < s.bars; line += plan.dropout->every)
                    if (dropout (line, true))
                        break;
        }
    }

    bool formFits (const FormSpec& form, Lines lines)
    {
        if (form.slots.empty())
            return false;
        return reachable (form, lines)[0][0] != 0;
    }

    std::vector<std::int32_t> formLengths (const FormSpec& form, Lines lines, rng::Key key)
    {
        if (form.slots.empty())
            return {};
        const std::int32_t hi = form.bars.hi / unitBars, lo = (form.bars.lo + unitBars - 1) / unitBars;
        const auto weightsOf = [] (const FormSlot& slot) {
            std::vector<std::int32_t> w;
            for (const Weighted& x : slot.bars)
                w.push_back (x.weight);
            return w;
        };
        // Every slot drawn by its own weights, kept if the form keeps its lines and its length: the
        // grammar's weights, given the lines. Up to formAttempts tries...
        for (std::int32_t attempt = 0; attempt < formAttempts; ++attempt)
        {
            const rng::Key ka = key.with (rng::tag ("attempt")).with (attempt);
            std::vector<std::int32_t> out;
            std::int32_t unit = 0;
            bool kept = true;
            for (std::size_t i = 0; i < form.slots.size() && kept; ++i)
            {
                const auto& choices = form.slots[i].bars;
                const std::size_t pick = rng::pickWeighted (ka.with (i), weightsOf (form.slots[i]));
                kept = pick < choices.size() && keepsLine (form, lines, i, unit, choices[pick].value);
                if (kept)
                {
                    out.push_back (choices[pick].value);
                    unit += choices[pick].value / unitBars;
                }
            }
            if (kept && unit >= lo && unit <= hi)
                return out;
        }
        // ...then slot by slot, each by weight among the lengths that leave the rest of the form a way
        // to keep its lines and its length
        const auto reach = reachable (form, lines);
        if (reach[0][0] == 0)
            return {};
        std::vector<std::int32_t> out;
        std::int32_t unit = 0;
        for (std::size_t i = 0; i < form.slots.size(); ++i)
        {
            const auto& choices = form.slots[i].bars;
            std::vector<std::int32_t> weights;
            for (const Weighted& w : choices)
            {
                const std::int32_t next = unit + w.value / unitBars;
                const bool ok = w.weight > 0 && next <= hi && keepsLine (form, lines, i, unit, w.value) && reach[i + 1][static_cast<std::size_t> (next)] != 0;
                weights.push_back (ok ? w.weight : 0);
            }
            const std::size_t pick = rng::pickWeighted (key.with (rng::tag ("fit")).with (i), weights);
            if (pick >= choices.size())
                return {}; // not reached: reach[i][unit] holds
            out.push_back (choices[pick].value);
            unit += choices[pick].value / unitBars;
        }
        return out;
    }

    std::vector<Section> arrange (const Style& style, rng::Key root)
    {
        const rng::Key kf = root.with (rng::tag ("form"));
        const FormSpec& form = style.form;

        // the lines: each drop, drop2 and break on a 16-bar line, or on 8 mod 16
        std::vector<std::int8_t> lines (form.slots.size(), -1);
        for (std::size_t i = 0; i < form.slots.size(); ++i)
            if (startsOnLine (form.slots[i].kind))
                lines[i] = rng::chance (kf.with (rng::tag ("line")).with (i), p16FromPercent (form.sixteen)) ? 1 : 0;
        const auto lengths = formLengths (form, lines, kf.with (rng::tag ("bars")));

        // drop 2's layer
        std::uint64_t layer = 0;
        if (!form.layer.empty())
        {
            std::vector<std::int32_t> weights;
            for (const LaneWeight& lw : form.layer)
                weights.push_back (lw.weight);
            const std::size_t pick = rng::pickWeighted (kf.with (rng::tag ("layer")), weights);
            if (pick < form.layer.size())
                layer = bitOf (style, form.layer[pick].lane);
        }

        std::vector<Section> out;
        std::int32_t start = 0;
        for (std::size_t i = 0; i < lengths.size(); ++i)
        {
            if (lengths[i] == 0)
                continue;
            Section s;
            s.index = out.size();
            s.kind = form.slots[i].kind;
            s.startBar = start;
            s.bars = lengths[i];
            s.variant = form.slots[i].variant;
            const std::optional<std::uint64_t> prevEnd = out.empty() ? std::nullopt : std::optional<std::uint64_t> (out.back().lanesAt (out.back().bars - 1));
            if (const KindPlan* plan = form.plan (s.kind))
                planBlocks (s, style, *plan, layer, prevEnd, kf);
            else
                s.blocks.assign (static_cast<std::size_t> (s.bars / barsPerBlock), 0);
            start += s.bars;
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
        const Section& sec = song.sections[section];
        const SectionSpec* spec = style.section (sec.kind);
        std::int32_t v = sec.variant >= 0 ? sec.variant : spec != nullptr ? spec->variant : 0;
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
            for (std::int32_t bar = 0; bar < s.bars; ++bar)
            {
                const bool on = s.active (lane, bar);
                const std::int64_t at = (s.startBar + bar) * ticksPerBar;
                if (on && !open)
                    out.push_back ({ at, at, s.index });
                open = on;
                if (on)
                    out.back().end = at + ticksPerBar;
            }
        }
        return out;
    }
}
