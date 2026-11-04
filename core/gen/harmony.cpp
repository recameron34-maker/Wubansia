#include "gen/harmony.h"

#include "util/ints.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>

namespace wubansia::gen
{
    namespace
    {
        std::size_t pickScaled (rng::Key key, const std::vector<Candidate>& cands)
        {
            // Weights are int64 products; scale them into pickWeighted's 32-bit total.
            std::int64_t total = 0;
            for (const auto& c : cands)
                total += std::max<std::int64_t> (0, c.weight);
            if (total <= 0)
                return cands.size();
            const std::int64_t div = total / (std::int64_t (1) << 30) + 1;
            std::vector<std::int32_t> w;
            w.reserve (cands.size());
            for (const auto& c : cands)
                w.push_back (static_cast<std::int32_t> (std::max<std::int64_t> (0, c.weight) / div));
            return rng::pickWeighted (key, w);
        }

        std::size_t pickWeights (rng::Key key, const std::vector<Weighted>& ws)
        {
            std::vector<std::int32_t> w;
            for (const auto& x : ws)
                w.push_back (x.weight);
            const auto i = rng::pickWeighted (key, w);
            return i < ws.size() ? i : 0;
        }
    }

    bool sameShape (std::span<const std::int32_t> a, std::span<const std::int32_t> b) noexcept
    {
        if (a.size() != b.size() || a.empty())
            return false;
        const std::size_t n = a.size();
        for (std::size_t r = 0; r < n; ++r)
        {
            bool all = true;
            for (std::size_t i = 0; i < n && all; ++i)
                all = a[(i + r) % n] == b[i];
            if (all)
                return true;
        }
        return false;
    }

    bool containsRun (std::span<const std::int32_t> shape, std::span<const std::int32_t> run) noexcept
    {
        const std::size_t n = shape.size();
        if (run.empty() || n == 0 || run.size() > n)
            return false;
        for (std::size_t s = 0; s < n; ++s)
        {
            bool all = true;
            for (std::size_t i = 0; i < run.size() && all; ++i)
                all = shape[(s + i) % n] == run[i];
            if (all)
                return true;
        }
        return false;
    }

    std::int32_t clichePenalty (const HarmonySpec& spec, std::span<const std::int32_t> degrees) noexcept
    {
        const std::size_t n = degrees.size();
        if (n == 0)
            return 0;
        for (const auto& m : spec.memory)
            if (sameShape (m.shape, degrees))
                return spec.penaltyWhole;
        if (n >= 3)
            for (std::size_t i = 0; i < n; ++i)
            {
                const std::array<std::int32_t, 3> run = { degrees[i], degrees[(i + 1) % n], degrees[(i + 2) % n] };
                for (const auto& m : spec.memory)
                    if (m.shape.size() >= 3 && containsRun (m.shape, run))
                        return spec.penaltyThree;
            }
        for (std::size_t i = 0; i < n; ++i)
        {
            const std::array<std::int32_t, 2> run = { degrees[i], degrees[(i + 1) % n] };
            for (const auto& m : spec.memory)
                if (containsRun (m.shape, run))
                    return spec.penaltyTwo;
        }
        return 0;
    }

    std::vector<Candidate> progressionCandidates (const HarmonySpec& spec, const ProgressionSlot& slot, Mode mode, std::size_t count, std::span<const std::vector<std::int32_t>> taken)
    {
        // At most 3 chords (the loader's limit): a weight is then at most 100^7 = 1e14, times 1000
        // for the penalty, and the 343 candidates sum to at most 3.43e16, far inside 64 bits. Four
        // chords could reach 1e18 before the penalty and overflow.
        std::vector<Candidate> out;
        if (count < 1 || count > 3)
            return out;
        std::size_t combos = 1;
        for (std::size_t i = 0; i < count; ++i)
            combos *= 7;
        const auto& degW = spec.degrees[static_cast<std::size_t> (mode)];
        const auto& colour = spec.colour[static_cast<std::size_t> (mode)];
        for (std::size_t code = 0; code < combos; ++code)
        {
            std::vector<std::int32_t> d (count);
            std::size_t c = code;
            for (std::size_t i = count; i-- > 0;)
            {
                d[i] = static_cast<std::int32_t> (c % 7);
                c /= 7;
            }
            // one chord is a pedal: no neighbours to differ and no moves (the move table's diagonal)
            bool repeats = false;
            for (std::size_t i = 0; i < count && count > 1; ++i)
                repeats = repeats || d[i] == d[(i + 1) % count];
            if (repeats)
                continue;
            std::int64_t w = slot.start[static_cast<std::size_t> (d[0])];
            for (std::size_t i = 0; i < count && count > 1; ++i)
                w *= spec.move[static_cast<std::size_t> (d[i])][static_cast<std::size_t> (d[(i + 1) % count])];
            for (std::size_t i = 0; i < count; ++i)
                w *= degW[static_cast<std::size_t> (d[i])];
            const bool holdsTonic = std::find (d.begin(), d.end(), 0) != d.end();
            if (slot.tonic == "present" && !holdsTonic)
                w = 0;
            else if (slot.tonic == "emergent" && (!holdsTonic || d[0] == 0))
                w = 0;
            else if (slot.tonic == "absent" && holdsTonic)
                w = 0;
            if (!colour.empty() && std::none_of (d.begin(), d.end(), [&] (std::int32_t x) { return std::find (colour.begin(), colour.end(), x) != colour.end(); }))
                w = 0;
            for (const auto& t : taken)
                if (sameShape (t, d))
                    w = 0;
            w = w * (1000 - clichePenalty (spec, d)) / 1000;
            out.push_back ({ std::move (d), w });
        }
        return out;
    }

    const Progression* HarmonyPlan::slot (std::string_view name) const noexcept
    {
        for (const auto& [n, p] : slots)
            if (n == name)
                return &p;
        return nullptr;
    }

    HarmonyPlan planHarmony (const Style& style, rng::Key root)
    {
        const HarmonySpec& spec = style.harmony;
        const rng::Key kh = root.with (rng::tag ("harmony"));
        HarmonyPlan plan;
        plan.mode = static_cast<Mode> (spec.modes[pickWeights (kh.with (rng::tag ("mode")), spec.modes)].value);
        std::vector<std::vector<std::int32_t>> taken;
        for (const auto& slot : spec.slots)
        {
            const rng::Key ks = kh.with (rng::tag ("slot")).with (rng::tag (slot.name));
            Progression p;
            p.mode = plan.mode;
            if (slot.phrygian > 0 && rng::chance (ks.with (rng::tag ("phrygian")), p16FromPercent (slot.phrygian)))
                p.mode = Mode::phrygian;
            p.loopBars = spec.loops[pickWeights (ks.with (rng::tag ("loop")), spec.loops)].value;
            const auto drawn = rng::pickWeighted (ks.with (rng::tag ("rhythm")), spec.rhythmWeights);
            const std::size_t r0 = drawn < spec.rhythms.size() ? drawn : 0;

            // The drawn harmonic rhythm if some progression of its length fits the slot's rules and
            // differs from the other slots; else the next rhythm in the style's list that does (a slot
            // whose rules leave only another slot's shape, such as a Dorian break on 2 chords, takes
            // 3); else the drawn one, dropping the other slots' shapes.
            std::vector<Candidate> cands;
            std::size_t pick = 0;
            for (std::size_t k = 0; k <= spec.rhythms.size(); ++k)
            {
                const bool last = k == spec.rhythms.size();
                const std::size_t ri = last ? r0 : (r0 + k) % spec.rhythms.size();
                p.units = spec.rhythms[ri];
                cands = progressionCandidates (spec, slot, p.mode, p.units.size(), last ? std::span<const std::vector<std::int32_t>> {} : taken);
                pick = pickScaled (ks.with (rng::tag ("progression")).with (k), cands);
                if (pick < cands.size())
                    break;
            }
            if (pick >= cands.size()) // nothing at all: the tonic and the degree after it (a pedal: the tonic)
                p.degrees = p.units.size() == 1   ? std::vector<std::int32_t> { 0 }
                            : p.units.size() == 2 ? std::vector<std::int32_t> { 0, 6 }
                                                  : std::vector<std::int32_t> { 0, 6, 3 };
            else
                p.degrees = cands[pick].degrees;
            taken.push_back (p.degrees);
            plan.slots.emplace_back (slot.name, std::move (p));
        }
        return plan;
    }

    void layOutHarmony (Song& song, const Style& style, const HarmonyPlan& plan, rng::Key root)
    {
        const HarmonySpec& spec = style.harmony;
        const rng::Key kh = root.with (rng::tag ("harmony")).with (rng::tag ("layout"));
        song.harmony.clear();
        song.chords.clear();
        for (const Section& s : song.sections)
        {
            const std::string_view slotName = spec.slotFor (s.kind);
            const Progression* p = plan.slot (slotName);
            SectionHarmony h;
            h.slot = std::string (slotName);
            h.mode = p->mode;
            h.degrees = p->degrees;
            h.units = p->units;
            h.loopBars = p->loopBars;
            h.pushPermille = rng::between (kh.with (rng::tag ("share")).with (s.index), spec.pushPermille.lo, spec.pushPermille.hi);
            h.pushTicks = spec.pushBy[pickWeights (kh.with (rng::tag ("by")).with (s.index), spec.pushBy)].value;
            const std::int32_t rotation = rng::between (kh.with (rng::tag ("rotation")).with (s.index), 0, 1023);

            const std::int64_t unit = static_cast<std::int64_t> (p->loopBars) * ticksPerBar / 4;
            const std::size_t first = song.chords.size();
            for (std::int64_t loop = s.startTick(); loop < s.endTick(); loop += p->loopBars * ticksPerBar)
            {
                std::int64_t at = loop;
                for (std::size_t i = 0; i < p->degrees.size() && at < s.endTick(); ++i)
                {
                    ChordSpan c;
                    c.start = at;
                    c.end = std::min (s.endTick(), at + p->units[i] * unit);
                    c.degree = p->degrees[i];
                    c.mode = p->mode;
                    c.rootPc = degreePc (song.tonic, p->mode, c.degree);
                    c.section = static_cast<std::int16_t> (s.index);
                    c.index = static_cast<std::int32_t> (i);
                    at += p->units[i] * unit;
                    song.chords.push_back (c);
                }
            }
            // Pushes, laid out per 8-bar phrase so every phrase pushes the same changes: of a phrase's
            // n changes (a section's first chord isn't one), k inside the style's share band are
            // pushed, spread evenly from a rotation drawn per section. With the push's "alike", the
            // section's first chord counts in its phrase like every phrase's first chord, so the first
            // phrase lays out exactly as the others do, but it is still never pushed.
            const std::int64_t phrase = barsPerBlock * ticksPerBar;
            for (std::int64_t from = s.startTick(); from < s.endTick(); from += phrase)
            {
                std::vector<std::size_t> changes;
                for (std::size_t i = first + (spec.pushAlike ? 0 : 1); i < song.chords.size(); ++i)
                    if (song.chords[i].start >= from && song.chords[i].start < from + phrase)
                        changes.push_back (i);
                const auto n = static_cast<std::int64_t> (changes.size());
                if (n == 0)
                    continue;
                const std::int64_t lo = util::ceilDiv (n * spec.pushPermille.lo, std::int64_t (1000));
                const std::int64_t hi = util::floorDiv (n * spec.pushPermille.hi, std::int64_t (1000));
                std::int64_t k = util::roundDiv (n * h.pushPermille, std::int64_t (1000));
                if (lo <= hi)
                    k = std::clamp (k, lo, hi);
                for (std::int64_t j = 0; j < n; ++j)
                {
                    const std::int64_t idx = (j + rotation) % n;
                    if ((idx + 1) * k / n == idx * k / n)
                        continue;
                    if (changes[static_cast<std::size_t> (j)] == first)
                        continue; // only with "alike": no chord crosses a section's start
                    ChordSpan& c = song.chords[changes[static_cast<std::size_t> (j)]];
                    c.pushedBy = h.pushTicks;
                    c.start -= h.pushTicks;
                    song.chords[changes[static_cast<std::size_t> (j)] - 1].end = c.start;
                }
            }
            song.harmony.push_back (std::move (h));
        }
    }

    const ChordSpan* chordAt (const Song& song, std::int64_t tick) noexcept
    {
        const auto it = std::upper_bound (song.chords.begin(), song.chords.end(), tick, [] (std::int64_t t, const ChordSpan& c) { return t < c.start; });
        if (it == song.chords.begin())
            return nullptr;
        const ChordSpan& c = *std::prev (it);
        return tick < c.end ? &c : nullptr;
    }

    std::vector<Voiced> voicingCandidates (const VoicingSpec& spec, std::int32_t tonicPc, Mode mode, std::int32_t degree, Range range, std::int32_t mudBelow, std::int32_t mudGap)
    {
        const std::int32_t rootPc = degreePc (tonicPc, mode, degree);
        // each tone: its pitch class and tag, in the order given
        std::vector<std::int32_t> pcs;
        std::vector<std::uint8_t> tags;
        const bool planed = !spec.shape.empty();
        const auto& offsets = planed ? spec.shape : spec.degrees;
        for (const auto o : offsets)
        {
            // A scale step a minor 9th (or 2nd) over the root moves up two steps, to the 11th (or
            // 4th): no stack puts a b9 on its own root (the Phrygian i's b2, the Aeolian v's, the
            // Dorian ii's), so m9 and add9 become m11 and add11 there, and sus2 sus4.
            const std::int32_t step = !planed && pitchClass (chordInterval (mode, degree, o)) == 1 ? o + 2 : o;
            const std::int32_t rel = planed ? o : chordInterval (mode, degree, step);
            const std::int32_t pc = pitchClass (rootPc + rel);
            pcs.push_back (pc);
            tags.push_back (inMinorKey (tonicPc, pc) ? 0 : (planed ? noteTag::planed : noteTag::borrowed));
        }

        // the tone subsets: all of them, or every `choose` of them (in index order)
        std::vector<std::vector<std::size_t>> subsets;
        const std::size_t n = pcs.size();
        if (spec.choose <= 0 || static_cast<std::size_t> (spec.choose) >= n)
        {
            std::vector<std::size_t> all (n);
            for (std::size_t i = 0; i < n; ++i)
                all[i] = i;
            subsets.push_back (all);
        }
        else
        {
            const auto k = static_cast<std::size_t> (spec.choose);
            std::vector<std::size_t> idx (k);
            for (std::size_t i = 0; i < k; ++i)
                idx[i] = i;
            while (true)
            {
                subsets.push_back (idx);
                std::size_t i = k;
                while (i > 0 && idx[i - 1] == n - k + (i - 1))
                    --i;
                if (i == 0)
                    break;
                ++idx[i - 1];
                for (std::size_t j = i; j < k; ++j)
                    idx[j] = idx[j - 1] + 1;
            }
        }

        std::vector<Voiced> out;
        for (const auto& sub : subsets)
        {
            // per tone, every key with its pitch class inside the range
            std::vector<std::vector<std::int32_t>> options;
            for (const auto t : sub)
            {
                std::vector<std::int32_t> keys;
                for (std::int32_t k = range.lo; k <= range.hi; ++k)
                    if (pitchClass (k) == pcs[t])
                        keys.push_back (k);
                options.push_back (std::move (keys));
            }
            if (std::any_of (options.begin(), options.end(), [] (const auto& o) { return o.empty(); }))
                continue;
            std::vector<std::size_t> at (sub.size(), 0); // an odometer, the first tone most significant
            while (true)
            {
                std::vector<std::pair<std::int32_t, std::uint8_t>> keys;
                for (std::size_t i = 0; i < sub.size(); ++i)
                    keys.emplace_back (options[i][at[i]], tags[sub[i]]);
                bool ok = true;
                if (!spec.inversions && spec.choose <= 0)
                    for (std::size_t i = 1; i < keys.size(); ++i)
                        ok = ok && keys[i].first > keys[0].first;
                std::stable_sort (keys.begin(), keys.end()); // (key, tag): equal keys have equal tags, so this is a total order
                for (std::size_t i = 1; i < keys.size() && ok; ++i)
                    ok = keys[i].first != keys[i - 1].first && (keys[i - 1].first >= mudBelow || keys[i].first - keys[i - 1].first >= mudGap);
                const std::int32_t span = keys.back().first - keys.front().first;
                if (spec.choose <= 0)
                    ok = ok && span >= spec.span.lo && span <= spec.span.hi;
                if (ok)
                {
                    Voiced v;
                    for (const auto& [k, t] : keys)
                    {
                        v.keys.push_back (k);
                        v.tags.push_back (t);
                    }
                    if (std::find (out.begin(), out.end(), v) == out.end())
                        out.push_back (std::move (v));
                }
                // next placement: the last tone moves fastest
                bool done = true;
                for (std::size_t i = sub.size(); i-- > 0;)
                {
                    if (++at[i] < options[i].size())
                    {
                        done = false;
                        break;
                    }
                    at[i] = 0;
                }
                if (done)
                    break;
            }
        }
        return out;
    }

    std::int64_t voicingCost (const Voiced& v, const Voiced* prev, std::int32_t targetTop, std::int32_t center) noexcept
    {
        if (v.keys.empty())
            return std::numeric_limits<std::int64_t>::max() / 4;
        std::int64_t cost = 0;
        if (prev != nullptr && !prev->keys.empty())
        {
            const std::size_t n = v.keys.size(), m = prev->keys.size();
            std::int64_t moves = 0;
            for (std::size_t i = 0; i < n; ++i)
            {
                const std::size_t j = n == 1 ? m - 1 : i * (m - 1) / (n - 1);
                moves += std::abs (v.keys[i] - prev->keys[j]);
            }
            std::int64_t common = 0;
            for (const auto k : v.keys)
                common += std::find (prev->keys.begin(), prev->keys.end(), k) != prev->keys.end() ? 1 : 0;
            cost += 4 * moves - 3 * common;
        }
        cost += 2 * std::abs (v.keys.back() - targetTop);
        std::int64_t sum = 0;
        for (const auto k : v.keys)
            sum += k;
        const std::int64_t centroid = util::roundDiv (sum, static_cast<std::int64_t> (v.keys.size()));
        cost += std::abs (centroid - center);
        return cost;
    }

    std::size_t bestVoicing (const std::vector<Voiced>& cands, const Voiced* prev, std::int32_t targetTop, const VoicingSpec& spec, std::size_t skip)
    {
        const auto inWindow = [&] (const Voiced& v) { return !v.keys.empty() && v.keys.back() >= spec.top.lo && v.keys.back() <= spec.top.hi; };
        bool anyInside = false;
        for (const Voiced& v : cands)
            anyInside = anyInside || inWindow (v);
        std::size_t best = cands.size();
        std::int64_t bestCost = 0;
        for (std::size_t k = 0; k < cands.size(); ++k)
        {
            if (k == skip || (anyInside && !inWindow (cands[k])))
                continue;
            const std::int64_t cost = voicingCost (cands[k], prev, targetTop, spec.center);
            if (best == cands.size() || cost < bestCost)
            {
                best = k;
                bestCost = cost;
            }
        }
        return best < cands.size() ? best : skip; // nothing else inside the window: no other voicing
    }
}
