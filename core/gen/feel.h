#pragma once

// The feel pass, always last (plan §4.1): it turns the score into the performance. Per lane:
// - swing in permille on odd 16ths: ((sw - 500) * 240 + 500) / 1000 ticks late at 480 PPQ
// - a 2-bar offset map and velocity map per lane, one feel row per section kind
// - 1/f fluctuation (Voss-McCartney), never i.i.d. jitter, for timing and velocity
// - anchors (the 100% cells of anchor lanes) within +-anchorClamp ticks of the grid
// - the low-end lock: a bass onset on a step where its kick plays takes the kick's performed tick
// - voices that play a fixed distance after another (the clap 4-8 ticks behind the snare)
// - chord stacks spread over 0-4 ticks, a velocity dip before accents, articulation (lengths)
// - amount (permille) scales every timing and velocity deviation: 0 gives the score back exactly
// - a lane's "tight": notes with a score velocity at or above it play as written (only the lock
//   moves them), so only its ghosts take the feel (the techno acid stays on the grid), and a ghost's
//   velocity feel keeps it below the tight velocity
// Then the hats' choke (an open hat ends where a closed one starts) on performed ticks, the
// section boundaries (no note starts before its section or ends after it) and the overlap trim.

#include "gen/content.h"
#include "gen/song.h"
#include "rng/rng.h"

#include <cstdint>

namespace wubansia::gen
{
    // Ticks an odd 16th is late at swing sw (permille), at 480 PPQ: floor (((sw - 500) * 240 + 500) / 1000).
    // 500 -> 0, 540 -> 10, 570 -> 17, 580 -> 19, 630 -> 31 (plan §2.3).
    constexpr std::int64_t swingDelay (std::int32_t swingPermille) noexcept
    {
        const std::int64_t x = (static_cast<std::int64_t> (swingPermille) - 500) * 240 + 500;
        const std::int64_t q = x / 1000;
        return (x % 1000 != 0 && x < 0) ? q - 1 : q;
    }

    // Voss-McCartney pink noise as a pure function of (key, n): vossRows rows, row r updated on the
    // steps n where n's lowest set bit is bit r (so it holds for 2^(r+1) steps on average), plus a
    // white row updated on every step. Each row is uniform in [-1023, 1023].
    inline constexpr std::int32_t vossRows = 7;
    std::int32_t vossSum (rng::Key key, std::int64_t n) noexcept;

    // The sum's standard deviation times 10: sqrt (8 * 1023 * 1024 / 3) * 10, rounded down.
    inline constexpr std::int32_t vossSdTimes10 = 16713;

    // vossSum scaled to a standard deviation of sdTenths / 10 (ticks or velocity steps), rounded half up.
    std::int32_t vossScaled (rng::Key key, std::int64_t n, std::int32_t sdTenths) noexcept;

    // Fills every lane's perf from its score. song.lanes[i] is bundle.style.lanes[i].
    void perform (Song& song, const Bundle& bundle);
}
