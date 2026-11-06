#pragma once

// Mutation-based variants for bass grids (the techno brief: hypnotic 1-2 bar loops that evolve by
// small mutations, not by new patterns). Without it, a salient change (arrange.h) moves a lane to
// its next variant, and every cell of its 2-bar grid is drawn again. With a grid's "mutate": m:
// - the line is drawn per step of the bar, like a step sequencer's: the second bar's cell at a
//   16th takes the first bar's draws (drawCell), so the 2-bar line is one bar's line, and its second
//   bar differs only where the grid does (its own probabilities and degree sets there);
// - variant 0 is drawn whole, and each later variant u draws only m steps of it again, in both bars:
//   a cell at variant v takes its draws from the last u <= v that mutated its step, or from variant 0.
// So the line a section starts on stays, and each change moves a note or two of it, heard in both
// bars of the loop from the change on (G4 sees the change where it happens, on its phrase line). A
// section whose kind starts on a later variant (drop 2's "variant": 2) starts on the line as those
// mutations left it.

#include "gen/content.h"
#include "rng/rng.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace wubansia::gen
{
    // The cell whose draws a cell takes: itself, or with "mutate" its 16th in the first bar.
    constexpr std::size_t drawCell (const BassGrid& grid, std::size_t cell) noexcept { return grid.mutate > 0 ? cell % barSteps : cell; }

    // The cells a new draw can change: a cell that can sound (p or follow above 0) and has something
    // to draw: p or follow strictly between 0 and 100, a degree set with two or more weights above 0,
    // or (with the grid's slide) a slide strictly between 0 and 100. In cell order.
    std::vector<std::size_t> variableCells (const BassGrid& grid, const BassLane& lane);

    // The steps of the bar (0-15) whose cells are variable in both bars, in order: what a mutation picks,
    // so a mutation is heard in both bars of the loop.
    std::vector<std::size_t> variableSteps (const BassGrid& grid, const BassLane& lane);

    // The variant each of the grid's 32 cells takes its draws from at `variant`. key is the grid's own
    // key (the lane's key with the grid's tag). Mutation u (1, 2, ...) takes the first grid.mutate steps
    // of variableSteps shuffled with key.with (tag ("mutate")).with (u), and both cells of each (step and
    // step + 16) take u. With grid.mutate == 0, or at variant 0 or below, every cell takes `variant`
    // itself, so a grid without "mutate" draws exactly as it always did.
    std::array<std::int32_t, gridSteps> cellVariants (const BassGrid& grid, const BassLane& lane, rng::Key key, std::int32_t variant);
}
