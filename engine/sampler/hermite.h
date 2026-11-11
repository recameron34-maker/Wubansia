#pragma once

// 4-point cubic Hermite (Catmull-Rom), as fut01/basslines.hermite computes it, where an instrument asks for
// it (interp hermite: FUT01's warp bass and organ lead). Same operations in the same order, in double, so
// with the same frames and position it gives the recipe's value (tests/vectors/sampler/hermite_*.f32).
//
// The recipe reads its four frames through at (k): an index under 0 reads frame 0, and one at or past a
// loop's end wraps into [loopStart, loopEnd). Past the end of a file that doesn't loop, the voice reads
// silence (the recipes never read there).

#include <cstdint>

namespace wubansia::engine
{
    // The value at u in [0, 1) between x0 and x1, with xm before them and x2 after.
    inline double hermite4 (double xm, double x0, double x1, double x2, double u) noexcept
    {
        const double c1 = 0.5 * (x1 - xm);
        const double c2 = xm - 2.5 * x0 + 2.0 * x1 - 0.5 * x2;
        const double c3 = 0.5 * (x2 - xm) + 1.5 * (x0 - x1);
        return x0 + u * (c1 + u * (c2 + u * c3));
    }

    // Where a voice reads frame k of a file: basslines.hermite's at (k) for a sustain loop, the frame
    // itself for a file played straight through. -1: silence (past the end, or before 0 for the sinc).
    struct FrameMap
    {
        std::int64_t frames = 0;
        std::int64_t loopStart = 0;
        std::int64_t loopEnd = 0; // 0: plays straight through
        bool clampStart = false; // Hermite: an index under 0 reads frame 0

        bool loops() const noexcept { return loopEnd > loopStart; }

        // The highest index + 1 that maps to itself: reads under it need no mapping.
        std::int64_t direct() const noexcept { return loops() ? loopEnd : frames; }

        std::int64_t operator() (std::int64_t k) const noexcept
        {
            if (k < 0)
                return clampStart && frames > 0 ? 0 : -1;
            if (loops())
                return k < loopEnd ? k : loopStart + (k - loopStart) % (loopEnd - loopStart);
            return k < frames ? k : -1;
        }
    };

    // The Hermite read of one channel at pos (pos >= 0), through map.
    inline double hermiteRead (const float* data, const FrameMap& map, double pos) noexcept
    {
        const auto i = static_cast<std::int64_t> (pos); // pos >= 0: the floor
        const double u = pos - static_cast<double> (i);
        auto at = [&] (std::int64_t k) noexcept {
            const std::int64_t m = map (k);
            return m < 0 ? 0.0 : static_cast<double> (data[m]);
        };
        if (i >= 1 && i + 2 < map.direct())
        {
            const float* x = data + (i - 1);
            return hermite4 (x[0], x[1], x[2], x[3], u);
        }
        return hermite4 (at (i - 1), at (i), at (i + 1), at (i + 2), u);
    }
}
