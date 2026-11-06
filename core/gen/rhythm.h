#pragma once

// Rhythm skeletons (plan §4.1): 2-bar probability grids with style anchors, Euclidean and
// polymeter cells, a syncopation score kept inside a band, and a fixed number of attempts before a
// deterministic best so far. A 2-bar pattern is a 32-bit mask per voice: bit s is a hit on 16th s.

#include "gen/content.h"
#include "rng/rng.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace wubansia::gen
{
    using StepMask = std::uint32_t; // 2 bars of 16ths

    // The metrical weight of a 16th within a 4/4 bar, after Longuet-Higgins & Lee (1984): 0 on the
    // bar line, -1 on beat 3, -2 on beats 2 and 4, -3 on the other 8ths, -4 on the other 16ths.
    constexpr std::int32_t metricWeight (std::int64_t step) noexcept
    {
        const auto s = static_cast<std::int32_t> (((step % 16) + 16) % 16);
        if (s == 0)
            return 0;
        if (s == 8)
            return -1;
        if (s % 4 == 0)
            return -2;
        if (s % 2 == 0)
            return -3;
        return -4;
    }

    // The LHL syncopation of a cyclic 2-bar pattern: for every onset, the strongest step among the
    // rests up to the next onset (wrapping round), if it is stronger than the onset, adds the
    // difference of their weights. 0 for no onsets; a note on every 16th gives 0 too.
    std::int32_t syncopation (StepMask onsets) noexcept;

    // Euclid's E(k, n), rotated: the k hits spread as evenly as n steps allow, as Bresenham draws
    // them (hit i where (i * k) mod n < k), then step i moves to (i + rotation) mod n.
    // E(3, 8) = x..x..x. and E(5, 16) has gaps 3, 3, 3, 3, 4 (in some rotation). k is clamped to [0, n].
    std::vector<bool> euclid (std::int32_t k, std::int32_t n, std::int32_t rotation);

    // A 2-bar pattern for one lane: one mask per grid voice (same order as grid.voices), with the
    // attempt that gave it and its syncopation (of all voices together).
    struct Pattern
    {
        std::vector<StepMask> hits;
        std::vector<StepMask> anchors; // the 100% cells that hit
        std::int32_t attempt = 0;
        std::int32_t sync = 0;
        std::int32_t missing = 0; // hits short of the voices' minPerBar, summed over bars and voices
        bool inBand = true;
    };

    // Draws the pattern a lane plays for one variant of a grid: each cell hits with its probability
    // (anchors always), keyed by (key, attempt, voice, step). Polymeter voices (which aren't 2-bar
    // periodic) give no mask here; lanes.cpp places them per bar. A bar holding every step of one of
    // grid.never is repaired by dropping its last non-anchor hit there. If the grid has constraints
    // (grid.sync, a voice's minPerBar), attempts 0 .. attempts - 1 are tried and the first that keeps
    // them wins; if none does, the nearest (fewest missing hits, then the syncopation distance; the
    // first of equals). Always returns a pattern.
    Pattern drawPattern (const DrumGrid& grid, rng::Key key, std::int32_t attempts);

    // One attempt of drawPattern: cells keyed by key.with (tag ("attempt")).with (attempt), then the
    // repair. inBand says whether it keeps the constraints: grid.sync, and each voice's minPerBar
    // in both bars (true if there are none).
    Pattern drawAttempt (const DrumGrid& grid, rng::Key key, std::int32_t attempt);

    // A 2-bar mask of cells (percent, 100 always), keyed like a drum voice's, with at least
    // minPerBar hits in each bar when one of `attempts` attempts gives that (else the attempt with
    // the most hits in its thinner bar, the first of equals). For stab patterns.
    StepMask drawCells (const Grid& p, rng::Key key, std::int32_t attempts, std::int32_t minPerBar);

    // True if a bar (16 bits of mask, from bit `barStart`) holds every step of combo.
    bool holdsAll (StepMask mask, std::int32_t barStart, const std::vector<std::int32_t>& combo) noexcept;
}
