#include "gen/rhythm.h"

#include "util/ints.h"

#include <algorithm>
#include <bit>
#include <cstdint>

namespace wubansia::gen
{
    namespace
    {
        constexpr bool bit (StepMask m, std::int32_t s) noexcept { return ((m >> s) & 1u) != 0; }
        constexpr StepMask one (std::int32_t s) noexcept { return StepMask (1) << s; }

        // Drops the latest non-anchor hit of the combo in each bar that holds all of it.
        void repair (StepMask& hits, StepMask anchors, const std::vector<std::vector<std::int32_t>>& never)
        {
            for (const auto& combo : never)
                for (const std::int32_t bar : { 0, 16 })
                {
                    // a bounded loop: each pass removes one hit of the combo
                    for (std::size_t guard = 0; guard < combo.size() && holdsAll (hits, bar, combo); ++guard)
                    {
                        std::int32_t drop = -1;
                        for (const auto s : combo)
                            if (!bit (anchors, bar + s) && s > drop)
                                drop = s;
                        if (drop < 0)
                            break; // all anchors: refused by the loader
                        hits &= ~one (bar + drop);
                    }
                }
        }
    }

    std::int32_t syncopation (StepMask onsets) noexcept
    {
        if (onsets == 0)
            return 0;
        std::int32_t total = 0;
        for (std::int32_t i = 0; i < 32; ++i)
        {
            if (!bit (onsets, i))
                continue;
            std::int32_t strongest = -100;
            for (std::int32_t k = 1; k < 32; ++k) // the rests up to the next onset, wrapping round
            {
                const std::int32_t s = (i + k) % 32;
                if (bit (onsets, s))
                    break;
                strongest = std::max (strongest, metricWeight (s));
            }
            const std::int32_t w = metricWeight (i);
            if (strongest > w)
                total += strongest - w;
        }
        return total;
    }

    std::vector<bool> euclid (std::int32_t k, std::int32_t n, std::int32_t rotation)
    {
        if (n <= 0)
            return {};
        k = std::clamp (k, 0, n);
        std::vector<bool> out (static_cast<std::size_t> (n), false);
        for (std::int32_t i = 0; i < n; ++i)
            if ((i * k) % n < k)
                out[static_cast<std::size_t> (util::floorMod (i + rotation, n))] = true;
        return out;
    }

    bool holdsAll (StepMask mask, std::int32_t barStart, const std::vector<std::int32_t>& combo) noexcept
    {
        if (combo.empty())
            return false;
        for (const auto s : combo)
            if (!bit (mask, barStart + s))
                return false;
        return true;
    }

    Pattern drawAttempt (const DrumGrid& grid, rng::Key key, std::int32_t attempt)
    {
        const rng::Key ka = key.with (rng::tag ("attempt")).with (attempt);
        Pattern p;
        p.attempt = attempt;
        StepMask all = 0;
        for (const auto& v : grid.voices)
        {
            const rng::Key kv = ka.with (rng::tag (v.voice));
            StepMask hits = 0, anchors = 0;
            if (v.hasCells)
            {
                for (std::int32_t s = 0; s < 32; ++s)
                {
                    const std::int32_t pct = v.p[static_cast<std::size_t> (s)];
                    if (pct >= 100)
                    {
                        hits |= one (s);
                        anchors |= one (s);
                    }
                    else if (pct > 0 && rng::chance (kv.with (s), p16FromPercent (pct)))
                        hits |= one (s);
                }
            }
            else if (v.euclid)
            {
                const EuclidCells& e = *v.euclid;
                const std::int32_t rot = rng::between (kv.with (rng::tag ("rotation")), e.rotation.lo, e.rotation.hi);
                const auto cycle = euclid (e.k, e.n, rot);
                for (std::int32_t s = 0; s < 32; ++s)
                    if (cycle[static_cast<std::size_t> (s % e.n)] && (e.p >= 100 || rng::chance (kv.with (rng::tag ("keep")).with (s), p16FromPercent (e.p))))
                        hits |= one (s);
            }
            repair (hits, anchors, grid.never);
            p.hits.push_back (hits);
            p.anchors.push_back (anchors);
            all |= hits;
            if (v.minPerBar > 0)
                for (const StepMask bar : { hits & 0xFFFFu, hits >> 16 })
                    p.missing += std::max (0, v.minPerBar - std::popcount (bar));
        }
        p.sync = syncopation (all);
        p.inBand = p.missing == 0 && (!grid.sync || (p.sync >= grid.sync->lo && p.sync <= grid.sync->hi));
        return p;
    }

    StepMask drawCells (const Grid& p, rng::Key key, std::int32_t attempts, std::int32_t minPerBar)
    {
        StepMask best = 0;
        std::int32_t bestThin = -1;
        for (std::int32_t a = 0; a < std::max (1, attempts); ++a)
        {
            const rng::Key ka = key.with (rng::tag ("attempt")).with (a);
            StepMask m = 0;
            for (std::int32_t s = 0; s < 32; ++s)
            {
                const std::int32_t pct = p[static_cast<std::size_t> (s)];
                if (pct >= 100 || (pct > 0 && rng::chance (ka.with (s), p16FromPercent (pct))))
                    m |= one (s);
            }
            const std::int32_t thin = std::min (std::popcount (m & 0xFFFFu), std::popcount (m >> 16));
            if (thin >= minPerBar)
                return m;
            if (thin > bestThin)
            {
                bestThin = thin;
                best = m;
            }
        }
        return best;
    }

    Pattern drawPattern (const DrumGrid& grid, rng::Key key, std::int32_t attempts)
    {
        const bool constrained = grid.sync || std::any_of (grid.voices.begin(), grid.voices.end(), [] (const DrumVoiceGrid& v) { return v.minPerBar > 0; });
        if (!constrained)
            return drawAttempt (grid, key, 0);
        Pattern best;
        std::int32_t bestDistance = -1;
        for (std::int32_t a = 0; a < std::max (1, attempts); ++a)
        {
            Pattern p = drawAttempt (grid, key, a);
            if (p.inBand)
                return p;
            const std::int32_t syncDistance = !grid.sync ? 0 : p.sync < grid.sync->lo ? grid.sync->lo - p.sync
                                                                                      : std::max (0, p.sync - grid.sync->hi);
            const std::int32_t d = 1000 * p.missing + syncDistance;
            if (bestDistance < 0 || d < bestDistance)
            {
                bestDistance = d;
                best = std::move (p);
            }
        }
        return best;
    }
}
