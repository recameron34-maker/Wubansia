// Tests for core/metrics/measures.h: the building blocks of the metrics, one rule at a time
// (tools/legacy_audit/README.md, "Building blocks"). Where a number is the audit's own (Node
// 22.22.0, tools/legacy_audit/metrics.js on the same input), the comment says so: those check
// that the C++ computes in the same order, not only that it is close.

#include "metrics/int128.h"
#include "metrics/measures.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <random>
#include <vector>

using namespace wubansia::metrics;

namespace
{
#if defined(__SIZEOF_INT128__)
    // The compilers' own 128-bit types, an oracle for Int128 (__extension__: they aren't ISO C++).
    __extension__ typedef unsigned __int128 NativeU128;
    __extension__ typedef __int128 NativeI128;
#endif

    Note note (std::int64_t tick, int key = 42, int velocity = 80, std::int64_t length = 10)
    {
        Note n;
        n.tick = tick;
        n.length = length;
        n.channel = 9;
        n.key = static_cast<std::uint8_t> (key);
        n.velocity = static_cast<std::uint8_t> (velocity);
        return n;
    }

    // The audit test's integer series: an LCG, values -4 to 4 (tools/legacy_audit/test/audit.test.js).
    struct Lcg
    {
        std::uint32_t seed = 12345;
        std::int64_t next()
        {
            seed = seed * 1103515245u + 12345u;
            return static_cast<std::int64_t> ((seed >> 16) % 9) - 4;
        }
    };

    std::vector<std::int64_t> whiteNoise()
    {
        Lcg g;
        std::vector<std::int64_t> x;
        for (int i = 0; i < 4096; ++i)
            x.push_back (g.next());
        return x;
    }
}

TEST_CASE ("permille rounds half up and has no value for nothing", "[metrics][measures]")
{
    CHECK (permille (2, 3) == 667);
    CHECK (permille (1, 3) == 333);
    CHECK (permille (1, 8) == 125); // 125.0 exactly
    CHECK (permille (1, 16) == 63); // 62.5 rounds up
    CHECK (permille (0, 5) == 0);
    CHECK (permille (5, 5) == 1000);
    CHECK (permille (1, 0) == -1);
    CHECK (permille (0, 0) == -1);
}

TEST_CASE ("lower medians, standard deviations in thousandths and distinct counts", "[metrics][measures]")
{
    CHECK (lowerMedian ({ 5, 1, 3, 2 }) == 2);
    CHECK (lowerMedian ({ 7 }) == 7);
    CHECK (lowerMedian ({ 4, -9999, 4, 4 }) == 4);
    CHECK_FALSE (lowerMedian ({}).has_value());

    const std::vector<std::int64_t> pair = { 70, 90 };
    const std::vector<std::int64_t> four = { 1, 2, 3, 4 };
    const std::vector<std::int64_t> same = { 3, 3, 3 };
    CHECK (sdMilli (pair) == 10000);
    CHECK (sdMilli (four) == 1118); // sqrt (1.25) = 1.1180
    CHECK (sdMilli (same) == 0);
    CHECK_FALSE (sdMilli ({}).has_value());
    CHECK (sdMilli (whiteNoise()) == 2588); // the audit's value

    CHECK (countDistinct ({ 3, 1, 3, 2, 1 }) == 3);
    CHECK (countDistinct ({}) == 0);
}

TEST_CASE ("Int128 multiplies, adds and compares like the native 128-bit type", "[metrics][measures]")
{
    using detail::Int128;
    CHECK (Int128::from (-5) < Int128::from (3));
    CHECK (Int128::from (3) * Int128::from (-7) == Int128::from (-21));
    CHECK (Int128::from (-3) * Int128::from (-7) == Int128::from (21));
    const Int128 big = Int128::from (INT64_MAX) * Int128::from (INT64_MAX); // 2^126 - 2^64 + 1
    CHECK (Int128::from (0) < big);
    CHECK (big - big == Int128::from (0));
    CHECK ((Int128::from (0) - big) < Int128::from (INT64_MIN));

    // floor ((2000 num + den) / (2 den)): -63/64 is -984.375; -1/16 is -62.5, rounded up to -62
    CHECK (detail::permilleHalfUp (Int128::from (-63), Int128::from (64)) == -984);
    CHECK (detail::permilleHalfUp (Int128::from (-1), Int128::from (16)) == -62);
    CHECK (detail::permilleHalfUp (Int128::from (1), Int128::from (16)) == 63);
    CHECK (detail::permilleHalfUp (Int128::from (5), Int128::from (5)) == 1000);
    CHECK (detail::permilleHalfUp (Int128::from (-5), Int128::from (5)) == -1000);
    CHECK (detail::permilleHalfUp (Int128::from (0), Int128::from (7)) == 0);
    // denominators up to 2^115, the most it is built for (lag-1's n (n S2 - S1^2) stays below 2^113)
    const Int128 large = Int128::from (std::int64_t { 1 } << 54) * Int128::from (std::int64_t { 1 } << 60);
    CHECK (detail::permilleHalfUp (large, large) == 1000);
    CHECK (detail::permilleHalfUp (Int128::from (0) - large, large) == -1000);
    CHECK (detail::permilleHalfUp (Int128::from (0) - large, large + large) == -500);

#if defined(__SIZEOF_INT128__)
    // Against the compiler's own type, where it has one (the code itself never uses it). Unsigned
    // arithmetic wraps as Int128 does, with no undefined overflow in the oracle itself.
    using U = NativeU128;
    std::mt19937_64 rng (7);
    const auto bits = [] (Int128 v) { return (static_cast<U> (v.hi) << 64) | v.lo; };
    const auto wide = [] (std::int64_t v) { return static_cast<U> (static_cast<NativeI128> (v)); };
    const auto less = [] (U a, U b) { return static_cast<NativeI128> (a) < static_cast<NativeI128> (b); };
    for (int i = 0; i < 20000; ++i)
    {
        const auto a = static_cast<std::int64_t> (rng());
        const auto b = static_cast<std::int64_t> (rng()) >> (rng() % 64);
        const auto c = static_cast<std::int64_t> (rng()) >> (rng() % 64);
        const Int128 x = Int128::from (a) * Int128::from (b);
        const U nx = wide (a) * wide (b);
        REQUIRE (bits (x) == nx);
        REQUIRE (bits (x + Int128::from (c)) == nx + wide (c));
        REQUIRE (bits (x - Int128::from (c)) == nx - wide (c));
        REQUIRE ((x < Int128::from (c)) == less (nx, wide (c)));
        REQUIRE (bits (x * Int128::from (3)) == nx * 3);
        REQUIRE (bits (x * x) == nx * nx);
        const Int128 y = Int128::from (c >> 1) * Int128::from (b >> 1);
        REQUIRE ((x < y) == less (nx, bits (y)));
        REQUIRE ((x == y) == (nx == bits (y)));
    }
#endif
}

TEST_CASE ("lag-1 autocorrelation is exact, with the audit's values", "[metrics][measures]")
{
    std::vector<std::int64_t> alternating;
    for (int i = 0; i < 64; ++i)
        alternating.push_back (i % 2 ? 1 : -1);
    CHECK (lag1Permille (alternating) == -984); // -63/64, the audit's G7 fixture

    std::vector<std::int64_t> ramp;
    for (int i = 0; i < 100; ++i)
        ramp.push_back ((i % 7) - 3);
    CHECK (lag1Permille (ramp) == 257); // the audit's value

    CHECK (lag1Permille (whiteNoise()) == 3); // the audit's value

    // 200,000 residuals: n^2 P alone is past 2^64, so this needs the 128-bit arithmetic.
    std::vector<std::int64_t> big;
    for (std::int64_t i = 0; i < 200000; ++i)
        big.push_back (((i * 7919) % 61) - 30);
    CHECK (lag1Permille (big) == 113); // the audit's value (BigInt)

    CHECK_FALSE (lag1Permille (std::vector<std::int64_t> { 5 }).has_value());
    CHECK_FALSE (lag1Permille (std::vector<std::int64_t> (80, 2)).has_value()); // constant
    CHECK_FALSE (lag1Permille (std::vector<std::int64_t> { 1, 2000, 3 }).has_value()); // outside what it is built for
}

TEST_CASE ("DFA alpha: white noise near 0.5, a random walk near 1.5, the audit's exact values", "[metrics][measures]")
{
    const auto white = whiteNoise();
    std::vector<std::int64_t> walk;
    Lcg g;
    for (int i = 0; i < 4096; ++i)
        g.next(); // the walk continues the same LCG, as in the audit's test
    std::int64_t acc = 0;
    for (int i = 0; i < 4096; ++i)
    {
        acc += g.next();
        walk.push_back (acc);
    }
    REQUIRE (white[0] == -2); // the audit's first values: -2,2,-1,2,2,3,-1,3
    REQUIRE (white[7] == 3);
    REQUIRE (walk[0] == 2); // 2,3,5,5,6,9,11,12
    REQUIRE (walk[7] == 12);

    const auto aw = dfaAlphaMilli (white);
    const auto ar = dfaAlphaMilli (walk);
    REQUIRE (aw.has_value());
    REQUIRE (ar.has_value());
    CHECK (*aw > 400);
    CHECK (*aw < 600);
    CHECK (*ar > 1350);
    CHECK (*ar < 1650);
    // The audit's values. The README allows +-1 (only log () may differ); on this platform they agree exactly.
    CHECK (*aw == 538);
    CHECK (*ar == 1482);
    CHECK (lag1Permille (walk) == 999);
    CHECK (sdMilli (walk) == 71384);

    std::vector<std::int64_t> alternating;
    for (int i = 0; i < 64; ++i)
        alternating.push_back (i % 2 ? 1 : -1);
    CHECK (dfaAlphaMilli (alternating) == 76); // the G7 fixture's closed form

    std::vector<std::int64_t> ramp;
    for (int i = 0; i < 100; ++i)
        ramp.push_back ((i % 7) - 3);
    CHECK (dfaAlphaMilli (ramp) == 533);

    CHECK_FALSE (dfaAlphaMilli (std::vector<std::int64_t> (100, 3)).has_value()); // no fluctuation
    CHECK_FALSE (dfaAlphaMilli (std::vector<std::int64_t> (63, 1)).has_value()); // under 64 values
}

TEST_CASE ("pitch-class masks: popcount, rotation, hex", "[metrics][measures]")
{
    CHECK (popcount12 (0x895) == 5);
    CHECK (popcount12 (0) == 0);
    CHECK (rotateMask (0x091, 0) == 0x091); // C E G
    CHECK (rotateMask (1 << 9 | 1 << 0 | 1 << 4, 9) == (1 << 0 | 1 << 3 | 1 << 7)); // A C E from A: 0 3 7
    CHECK (rotateMask (1 << 2, 3) == 1 << 11);
    CHECK (maskHex (0x895) == "895");
    CHECK (maskHex (0x089) == "089");
    CHECK (maskHex (0x001) == "001");
    const std::vector<Note> chord = { note (0, 60), note (0, 64), note (0, 67), note (0, 72) };
    CHECK (pitchClassMask (chord) == 0x091);
}

TEST_CASE ("between, stacks and their spread", "[metrics][measures]")
{
    const std::vector<Note> notes = { note (0), note (5), note (10), note (11), note (30), note (30) };
    CHECK (between (notes, 0, 11).size() == 3);
    CHECK (between (notes, 10, 31).size() == 4);
    CHECK (between (notes, 31, 100).empty());
    CHECK (between (notes, -100, 0).empty());

    const auto st = stacksOf (notes);
    REQUIRE (st.size() == 3);
    CHECK (st[0].tick == 0);
    CHECK (st[0].notes.size() == 3); // 0, 5, 10: each within 10 of the first
    CHECK (st[0].spread() == 10);
    CHECK (st[1].tick == 11); // 11 is 11 after the first
    CHECK (st[1].notes.size() == 1);
    CHECK (st[2].notes.size() == 2);
    CHECK (st[2].spread() == 0);
    CHECK (stacksOf ({}).empty());
}

TEST_CASE ("harmony slots: the nearest stack in the window, ties to the later, carried forward", "[metrics][measures]")
{
    // Slot j starts at 960 j; the window is [start - 250, start + 710).
    std::vector<HarmonyStack> st = { { -250, 0x001, 0 }, { 700, 0x002, 1 }, { 1200, 0x004, 2 }, { 1680, 0x008, 3 }, { 2400, 0x010, 4 } };
    const auto slots = harmonySlots (st, 0, 4);
    REQUIRE (slots.has_value());
    REQUIRE (slots->size() == 4);
    CHECK ((*slots)[0]->mask == 0x001); // -250 is in [-250, 710); 700 is too, but further (700 > 250)
    // slot 1 (960): window [710, 1670) holds 1200 only (700 is before it)
    CHECK ((*slots)[1]->mask == 0x004);
    // slot 2 (1920): [1670, 2630) holds 1680 (240 away) and 2400 (480 away): 1680
    CHECK ((*slots)[2]->mask == 0x008);
    // slot 3 (2880): [2630, 3590) holds nothing: carries slot 2's
    CHECK ((*slots)[3]->mask == 0x008);

    // a tie: 720 and 1200 are both 240 from 960: the later wins
    std::vector<HarmonyStack> tie = { { 720, 0x001, 0 }, { 1200, 0x002, 0 } };
    const auto t = harmonySlots (tie, 960, 1);
    REQUIRE (t.has_value());
    CHECK ((*t)[0]->mask == 0x002);

    // leading empty slots take the first filled slot's harmony
    std::vector<HarmonyStack> late = { { 2 * 960, 0x020, 5 } };
    const auto l = harmonySlots (late, 0, 4);
    REQUIRE (l.has_value());
    CHECK ((*l)[0]->mask == 0x020);
    CHECK ((*l)[1]->mask == 0x020);
    CHECK ((*l)[3]->mask == 0x020);

    CHECK_FALSE (harmonySlots (late, 100000, 4).has_value());
    CHECK_FALSE (harmonySlots ({}, 0, 4).has_value());

    // harmony at a tick: the latest at or before it, the last one before the first
    CHECK (harmonyAt (st, -1000)->mask == 0x010);
    CHECK (harmonyAt (st, 700)->mask == 0x002);
    CHECK (harmonyAt (st, 1199)->mask == 0x002);
    CHECK (harmonyAt (st, 99999)->mask == 0x010);
    CHECK (harmonyAt ({}, 0) == nullptr);
}

TEST_CASE ("swing delays, distinct swings and residuals", "[metrics][measures]")
{
    // odd-16th offsets: tick mod 240 in [60, 180), minus 120
    const std::vector<Note> notes = { note (0), note (130), note (370), note (600), note (59), note (180), note (1060) };
    CHECK (oddOffsets (notes) == std::vector<std::int64_t> { 10, 10, -120 + 120, -20 });
    CHECK (swingDelay (notes, 4) == 0); // sorted -20, 0, 10, 10: lower median 0
    CHECK_FALSE (swingDelay (notes, 5).has_value());

    CHECK (distinctSwings ({ 10, 12, 13 }) == 2);
    CHECK (distinctSwings ({ 10, 11, 19, 24, 31 }) == 4);
    CHECK (distinctSwings ({ 31, 10, 24, 19, 11 }) == 4); // any order
    CHECK (distinctSwings ({}) == 0);

    // the audit's residual test: three onsets at 60 mod 240 make the delay -60, so the grid points
    // are 0, 60 and 240 in each 8th. 30: 30 from 0 and -30 from 60, a tie: +30. 700: nearest 720: -20.
    Song song;
    song.bars = 1;
    const std::vector<Note> r = { note (30), note (60), note (300), note (540), note (700) };
    CHECK (residuals (song, r) == std::vector<std::int64_t> { 30, 0, 0, 0, -20 });
    const std::vector<Note> far = { note (200) }; // 80 from 120, -40 from 240
    CHECK (residuals (song, far).empty());

    CHECK (residual (130, 10) == 0);
    CHECK (residual (125, 10) == -5);
    CHECK (residual (2, 10) == 2);
    CHECK (residual (238, 10) == -2);
    CHECK (residual (-2, 10) == -2);
    CHECK_FALSE (residual (70, 0).has_value()); // 70 from 0, -50 from 120

    CHECK (onSwungGrid (130, 10, 0));
    CHECK (onSwungGrid (240, 10, 0));
    CHECK (onSwungGrid (2, 10, 2));
    CHECK_FALSE (onSwungGrid (120, 10, 2));
    CHECK (onStraightGrid (120, 0));
    CHECK (onStraightGrid (118, 2));
    CHECK_FALSE (onStraightGrid (130, 2));
}

TEST_CASE ("step fingerprints and macro changes", "[metrics][measures]")
{
    // a note 60 ticks before bar 1 belongs to bar 1, step 0; 61 before stays in bar 0, step 15
    const std::vector<Note> notes = { note (0, 36), note (1920 - 60, 38), note (1920 - 61, 40), note (3 * 1920 + 120, 36) };
    const auto bars = stepBars (notes, 0, 4);
    REQUIRE (bars.size() == 4);
    CHECK (bars[0] == StepPrint { 36, 15 * 128 + 40 });
    CHECK (bars[1] == StepPrint { 38 });
    CHECK (bars[2].empty());
    CHECK (bars[3] == StepPrint { 128 + 36 });
    CHECK (stepBars (notes, 1, 2)[0] == StepPrint { 38 });

    // 24 bars: A for 8, then B at 8-11 and A again, then C from 20 on
    std::vector<StepPrint> p (24, StepPrint { 1 });
    for (int i = 8; i < 12; ++i)
        p[static_cast<std::size_t> (i)] = StepPrint { 2 };
    for (int i = 20; i < 24; ++i)
        p[static_cast<std::size_t> (i)] = StepPrint { 3 };
    // 8-11 differ from 0-3 (one change at 8); 16-19 differ from 8-11 (one change at 16, running on
    // into 20-23, which differ from 12-15): one run, 16-23
    CHECK (macroChanges (p) == std::vector<std::int64_t> { 8, 16 });
    CHECK (macroChanges (std::vector<StepPrint> (8, StepPrint { 1 })).empty());
}

TEST_CASE ("LHL syncopation of one bar", "[metrics][measures]")
{
    // the fixtures' cases (expected.json r11), worked by hand in make_fixtures.js
    CHECK (lhl (std::array<std::int32_t, 4> { 0, 4, 8, 12 }) == 0);
    CHECK (lhl (std::array<std::int32_t, 1> { 2 }) == 3);
    CHECK (lhl (std::array<std::int32_t, 2> { 0, 6 }) == 2);
    CHECK (lhl (std::array<std::int32_t, 4> { 3, 7, 11, 15 }) == 11);
    CHECK (lhl (std::span<const std::int32_t> {}) == 0);
}
