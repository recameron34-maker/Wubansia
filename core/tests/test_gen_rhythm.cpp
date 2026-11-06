// Tests for core/gen/rhythm: metrical weights, LHL syncopation, Euclidean rhythms and pattern draws.

#include "gen/rhythm.h"

#include <catch2/catch_test_macros.hpp>

#include <bit>
#include <cstdint>
#include <vector>

using namespace wubansia;
using namespace wubansia::gen;

namespace
{
    StepMask maskOf (std::initializer_list<int> steps)
    {
        StepMask m = 0;
        for (const int s : steps)
            m |= StepMask (1) << s;
        return m;
    }

    std::vector<int> hitsOf (const std::vector<bool>& v)
    {
        std::vector<int> out;
        for (std::size_t i = 0; i < v.size(); ++i)
            if (v[i])
                out.push_back (static_cast<int> (i));
        return out;
    }

    DrumGrid gridWith (const Grid& p, const Grid& vel)
    {
        DrumGrid g;
        g.name = "test";
        DrumVoiceGrid v;
        v.voice = "x";
        v.hasCells = true;
        v.p = p;
        v.vel = vel;
        g.voices.push_back (v);
        return g;
    }

    Grid filled (std::int32_t value)
    {
        Grid g {};
        g.fill (value);
        return g;
    }
}

TEST_CASE ("metric weights follow the LHL hierarchy of a 4/4 bar", "[gen][rhythm]")
{
    CHECK (metricWeight (0) == 0);
    CHECK (metricWeight (8) == -1);
    CHECK (metricWeight (4) == -2);
    CHECK (metricWeight (12) == -2);
    CHECK (metricWeight (2) == -3);
    CHECK (metricWeight (14) == -3);
    CHECK (metricWeight (1) == -4);
    CHECK (metricWeight (15) == -4);
    CHECK (metricWeight (16) == 0); // the next bar
    CHECK (metricWeight (-1) == -4);
}

TEST_CASE ("syncopation scores rests on stronger steps after weaker onsets", "[gen][rhythm]")
{
    CHECK (syncopation (0) == 0);
    // four on the floor in both bars: every onset is followed by a weaker rest
    CHECK (syncopation (maskOf ({ 0, 4, 8, 12, 16, 20, 24, 28 })) == 0);
    // a note on every 16th: no rests at all
    CHECK (syncopation (0xFFFFFFFFu) == 0);
    // 0: no stronger rest before 15; 15 (-4) with a rest on the next downbeat (0): 4
    CHECK (syncopation (maskOf ({ 0, 15, 17 })) == 0 + 4 + 3); // 17 (-4) -> rests up to 31 hold 24 (-1)
    // offbeat 8ths only: each (-3) is followed by a rest on a beat (-2) or stronger
    // 2 -> rest 4 (-2): 1; 6 -> rest 8 (-1): 2; 10 -> rest 12: 1; 14 -> rest 16 (0): 3; the same in bar 2
    CHECK (syncopation (maskOf ({ 2, 6, 10, 14, 18, 22, 26, 30 })) == 2 * (1 + 2 + 1 + 3));
    // 2-step: 1 and the and of 3 in both bars. 0 -> rests up to 10 hold 8 (-1): no (0 is stronger);
    // 10 (-3) -> rests 11..15 hold 12 (-2): 1. Twice.
    CHECK (syncopation (maskOf ({ 0, 10, 16, 26 })) == 2);
}

TEST_CASE ("euclid spreads k hits over n steps and rotates them", "[gen][rhythm]")
{
    CHECK (hitsOf (euclid (3, 8, 0)) == std::vector<int> { 0, 3, 6 });
    CHECK (hitsOf (euclid (3, 8, 1)) == std::vector<int> { 1, 4, 7 });
    CHECK (hitsOf (euclid (3, 8, 2)) == std::vector<int> { 0, 2, 5 }); // wraps
    CHECK (hitsOf (euclid (4, 16, 0)) == std::vector<int> { 0, 4, 8, 12 });
    CHECK (hitsOf (euclid (0, 8, 0)).empty());
    CHECK (hitsOf (euclid (8, 8, 3)).size() == 8);
    CHECK (hitsOf (euclid (9, 8, 0)).size() == 8); // clamped
    const auto e516 = hitsOf (euclid (5, 16, 0));
    REQUIRE (e516.size() == 5);
    std::vector<int> gaps;
    for (std::size_t i = 0; i < e516.size(); ++i)
        gaps.push_back ((e516[(i + 1) % e516.size()] - e516[i] + 16) % 16);
    std::sort (gaps.begin(), gaps.end());
    CHECK (gaps == std::vector<int> { 3, 3, 3, 3, 4 });
    CHECK (euclid (3, 8, -1) == euclid (3, 8, 7)); // negative rotations wrap too
}

TEST_CASE ("drawPattern always plays anchors and never plays 0% cells", "[gen][rhythm]")
{
    Grid p {};
    p[0] = 100;
    p[16] = 100;
    p[10] = 50;
    const DrumGrid g = gridWith (p, filled (100));
    for (std::uint64_t seed = 0; seed < 200; ++seed)
    {
        const Pattern pat = drawPattern (g, rng::seedKey (seed), 1);
        REQUIRE (pat.hits.size() == 1);
        CHECK ((pat.hits[0] & maskOf ({ 0, 16 })) == maskOf ({ 0, 16 }));
        CHECK ((pat.hits[0] & ~maskOf ({ 0, 10, 16 })) == 0u);
        CHECK (pat.anchors[0] == maskOf ({ 0, 16 }));
    }
}

TEST_CASE ("drawPattern hits with the cells' probabilities", "[gen][rhythm]")
{
    Grid p {};
    p[3] = 25;
    p[7] = 75;
    const DrumGrid g = gridWith (p, filled (100));
    int n3 = 0, n7 = 0;
    constexpr int runs = 4000;
    for (std::uint64_t seed = 0; seed < runs; ++seed)
    {
        const Pattern pat = drawPattern (g, rng::seedKey (seed), 1);
        n3 += static_cast<int> ((pat.hits[0] >> 3) & 1u);
        n7 += static_cast<int> ((pat.hits[0] >> 7) & 1u);
    }
    // 4000 draws: 3 standard deviations are about 2%
    CHECK (n3 > runs * 22 / 100);
    CHECK (n3 < runs * 28 / 100);
    CHECK (n7 > runs * 72 / 100);
    CHECK (n7 < runs * 78 / 100);
}

TEST_CASE ("drawPattern is a pure function of its key", "[gen][rhythm]")
{
    Grid p = filled (50);
    const DrumGrid g = gridWith (p, filled (90));
    const rng::Key k = rng::seedKey (42).with (rng::tag ("lane"));
    const Pattern a = drawPattern (g, k, 8);
    const Pattern b = drawPattern (g, k, 8);
    CHECK (a.hits == b.hits);
    CHECK (a.attempt == b.attempt);
    const Pattern c = drawPattern (g, k.with (std::uint64_t (1)), 8);
    CHECK (c.hits != a.hits); // 32 cells at 50%: equal by chance with probability 2^-32
}

TEST_CASE ("drawPattern repairs bars that hold a forbidden combination", "[gen][rhythm]")
{
    Grid p {};
    for (const int s : { 0, 4, 8, 12, 16, 20, 24, 28 })
        p[static_cast<std::size_t> (s)] = 90;
    p[0] = 100;
    p[16] = 100;
    DrumGrid g = gridWith (p, filled (100));
    g.never = { { 0, 4, 8, 12 } };
    for (std::uint64_t seed = 0; seed < 300; ++seed)
    {
        const Pattern pat = drawPattern (g, rng::seedKey (seed), 4);
        CHECK_FALSE (holdsAll (pat.hits[0], 0, g.never[0]));
        CHECK_FALSE (holdsAll (pat.hits[0], 16, g.never[0]));
        CHECK ((pat.hits[0] & maskOf ({ 0, 16 })) == maskOf ({ 0, 16 })); // anchors survive the repair
    }
}

TEST_CASE ("drawPattern keeps the syncopation band when an attempt can and else takes the nearest", "[gen][rhythm]")
{
    Grid p = filled (40);
    p[0] = 100;
    p[16] = 100;
    DrumGrid g = gridWith (p, filled (100));

    SECTION ("a reachable band")
    {
        g.sync = Range { 6, 10 };
        int inBand = 0;
        for (std::uint64_t seed = 0; seed < 200; ++seed)
        {
            const Pattern pat = drawPattern (g, rng::seedKey (seed), 16);
            CHECK (pat.sync == syncopation (pat.hits[0]));
            if (pat.inBand)
            {
                ++inBand;
                CHECK (pat.sync >= 6);
                CHECK (pat.sync <= 10);
            }
            CHECK (pat.attempt < 16);
        }
        CHECK (inBand > 190);
    }

    SECTION ("an unreachable band gives the nearest attempt, the first of equals")
    {
        g.sync = Range { 1000, 1000 };
        for (std::uint64_t seed = 0; seed < 20; ++seed)
        {
            const rng::Key k = rng::seedKey (seed);
            const Pattern pat = drawPattern (g, k, 16);
            CHECK_FALSE (pat.inBand);
            // every attempt is below the band, so the nearest is the highest score; ties go to the first
            std::int32_t bestSync = -1;
            std::int32_t bestAttempt = -1;
            for (std::int32_t a = 0; a < 16; ++a)
            {
                const Pattern one = drawAttempt (g, k, a);
                CHECK (one.attempt == a);
                if (one.sync > bestSync)
                {
                    bestSync = one.sync;
                    bestAttempt = a;
                }
            }
            CHECK (pat.attempt == bestAttempt);
            CHECK (pat.sync == bestSync);
            CHECK (pat.hits == drawAttempt (g, k, bestAttempt).hits);
        }
    }

    SECTION ("the first attempt in the band wins")
    {
        g.sync = Range { 6, 10 };
        for (std::uint64_t seed = 0; seed < 50; ++seed)
        {
            const rng::Key k = rng::seedKey (seed);
            const Pattern pat = drawPattern (g, k, 16);
            if (!pat.inBand)
                continue;
            for (std::int32_t a = 0; a < pat.attempt; ++a)
            {
                const Pattern earlier = drawAttempt (g, k, a);
                CHECK ((earlier.sync < 6 || earlier.sync > 10));
            }
        }
    }
}

TEST_CASE ("drawPattern keeps each bar's minimum of hits when an attempt can", "[gen][rhythm]")
{
    Grid p {};
    p[0] = 100;
    p[16] = 100;
    for (const int s : { 6, 7, 10, 22, 23, 26 })
        p[static_cast<std::size_t> (s)] = 25; // a second hit per bar is unlikely on one draw
    DrumGrid g = gridWith (p, filled (100));
    g.voices[0].minPerBar = 2;
    int kept = 0;
    for (std::uint64_t seed = 0; seed < 200; ++seed)
    {
        const Pattern pat = drawPattern (g, rng::seedKey (seed), 16);
        const auto bar1 = std::popcount (pat.hits[0] & 0xFFFFu);
        const auto bar2 = std::popcount (pat.hits[0] >> 16);
        if (pat.inBand)
        {
            ++kept;
            CHECK (bar1 >= 2);
            CHECK (bar2 >= 2);
        }
    }
    CHECK (kept > 190); // P(a bar keeps 1 hit) = 0.75^3, so 16 attempts nearly always find one
    // and a minimum no attempt can reach gives the attempt nearest to it
    g.voices[0].minPerBar = 9;
    const Pattern far = drawPattern (g, rng::seedKey (1), 16);
    CHECK_FALSE (far.inBand);
}
