#include "gen/mutate.h"

#include <algorithm>

namespace wubansia::gen
{
    std::vector<std::size_t> variableCells (const BassGrid& grid, const BassLane& lane)
    {
        std::vector<std::size_t> out;
        for (std::size_t s = 0; s < gridSteps; ++s)
        {
            const std::int32_t p = grid.p[s];
            const std::int32_t f = grid.follow[s];
            if (p <= 0 && f <= 0)
                continue; // never sounds
            bool variable = (p > 0 && p < 100) || (f > 0 && f < 100);
            const auto set = static_cast<std::size_t> (grid.deg[s]);
            if (!variable && set < lane.degreeSets.size())
            {
                const auto& weights = lane.degreeSets[set];
                variable = std::count_if (weights.begin(), weights.end(), [] (std::int32_t w) { return w > 0; }) >= 2;
            }
            if (!variable && grid.hasSlide)
                variable = grid.slide[s] > 0 && grid.slide[s] < 100;
            if (variable)
                out.push_back (s);
        }
        return out;
    }

    std::vector<std::size_t> variableSteps (const BassGrid& grid, const BassLane& lane)
    {
        std::array<std::int32_t, barSteps> count {};
        for (const auto c : variableCells (grid, lane))
            ++count[c % barSteps];
        std::vector<std::size_t> out;
        for (std::size_t s = 0; s < barSteps; ++s)
            if (count[s] == 2) // variable in both bars: a mutation there is heard in both
                out.push_back (s);
        return out;
    }

    std::array<std::int32_t, gridSteps> cellVariants (const BassGrid& grid, const BassLane& lane, rng::Key key, std::int32_t variant)
    {
        std::array<std::int32_t, gridSteps> out {};
        if (grid.mutate <= 0 || variant <= 0)
        {
            out.fill (variant); // a grid without "mutate" draws every cell from the variant, as it always did
            return out;
        }
        out.fill (0);
        const auto steps = variableSteps (grid, lane);
        if (steps.empty())
            return out;
        const std::size_t m = std::min (static_cast<std::size_t> (grid.mutate), steps.size());
        // one shuffle per mutation, a fixed number of them: the variant's own count
        for (std::int32_t u = 1; u <= variant; ++u)
        {
            std::vector<std::size_t> order = steps;
            rng::shuffle (key.with (rng::tag ("mutate")).with (u), order);
            for (std::size_t k = 0; k < m; ++k)
            {
                out[order[k]] = u; // the step, in both bars
                out[order[k] + barSteps] = u;
            }
        }
        return out;
    }
}
