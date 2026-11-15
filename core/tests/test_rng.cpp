// Tests for core/rng/rng.h, the counter-based RNG (docs/plans/S1-futuristic-slice.md §4.2).
//
// The vectors come from two independent reference implementations, written for this test and
// checked against each other before being pasted here: one in Python (arbitrary-precision ints,
// masked to 64 bits) and one in Node (BigInt). The Python one, in full:
//
//     M = (1 << 64) - 1
//     def mix64(z):
//         z = (z + 0x9E3779B97F4A7C15) & M
//         z = ((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9) & M
//         z = ((z ^ (z >> 27)) * 0x94D049BB133111EB) & M
//         return z ^ (z >> 31)
//     def with_(h, x): return mix64(h ^ (x & M))           # x & M: negative values wrap, as uint64_t does
//     def below(h, n): return ((mix64(h) >> 32) * n) >> 32  # Lemire
//     def tag(s):                                           # FNV-1a 64 over UTF-8
//         h = 0xCBF29CE484222325
//         for b in s.encode("utf-8"): h = ((h ^ b) * 0x100000001B3) & M
//         return h
//     def seed_key(seed): return with_(0, seed)
//
// Two of the vector sets are also published elsewhere: SplitMix64 seeded with 0 (Steele, Lea &
// Flood 2014; the sequence Vigna's splitmix64.c gives) and the FNV-1a 64 test strings "", "a"
// and "foobar" (the FNV reference test suite).

#include "rng/rng.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <numeric>
#include <span>
#include <vector>

using namespace wubansia;

// Everything is constexpr, so a generator can fold tags and fixed draws at compile time.
static_assert (rng::mix64 (0) == 0xE220A8397B1DCDAFull);
static_assert (rng::tag ("a") == 0xAF63DC4C8601EC8Cull);
static_assert (rng::tag (u8"a") == rng::tag ("a"));
static_assert (rng::seedKey (7).h == 0x63CBE1E459320DD7ull);
static_assert (rng::below (rng::seedKey (7), 1000) < 1000);

namespace
{
    // The key of the reference vectors: seed 7 / "bass" / bar 3 / step 5.
    constexpr rng::Key bassKey = rng::seedKey (7).with (rng::tag ("bass")).with (3).with (5);
}

TEST_CASE ("mix64 is SplitMix64's output function", "[rng]")
{
    // SplitMix64 adds the golden gamma to its state and then mixes, so its k-th output from
    // seed 0 is mix64 (k * gamma).
    constexpr std::uint64_t gamma = 0x9E3779B97F4A7C15ull;
    const std::array<std::uint64_t, 5> splitMix64Seed0 = {
        0xE220A8397B1DCDAFull, 0x6E789E6AA1B965F4ull, 0x06C45D188009454Full, 0xF88BB8A8724C81ECull, 0x1B39896A51A8749Bull
    };
    for (std::uint64_t k = 0; k < splitMix64Seed0.size(); ++k)
        CHECK (rng::mix64 (k * gamma) == splitMix64Seed0[k]);
}

TEST_CASE ("mix64 matches the reference vectors", "[rng]")
{
    CHECK (rng::mix64 (0) == 0xE220A8397B1DCDAFull);
    CHECK (rng::mix64 (1) == 0x910A2DEC89025CC1ull);
    CHECK (rng::mix64 (2) == 0x975835DE1C9756CEull);
    CHECK (rng::mix64 (42) == 0xBDD732262FEB6E95ull);
    CHECK (rng::mix64 (0x9E3779B97F4A7C15ull) == 0x6E789E6AA1B965F4ull);
    CHECK (rng::mix64 (0x0123456789ABCDEFull) == 0x157A3807A48FAA9Dull);
    CHECK (rng::mix64 (0xFFFFFFFFFFFFFFFFull) == 0xE4D971771B652C20ull);
}

TEST_CASE ("tag is FNV-1a 64 over the UTF-8 bytes", "[rng]")
{
    // The FNV reference test strings.
    CHECK (rng::tag ("") == 0xCBF29CE484222325ull);
    CHECK (rng::tag ("a") == 0xAF63DC4C8601EC8Cull);
    CHECK (rng::tag ("foobar") == 0x85944171F73967E8ull);

    // Lane names.
    CHECK (rng::tag ("kick") == 0xEFA683D7216F667Full);
    CHECK (rng::tag ("bass") == 0x9A7CF79BAA54E7DAull);
    CHECK (rng::tag ("hats") == 0x2E4EA3CC7428F3C3ull);

    // Non-ASCII: the bytes are read as unsigned whatever char's signedness, so a byte above 0x7F
    // hashes the same with -fsigned-char and -funsigned-char. Written as escapes, so the source
    // file's encoding doesn't matter.
    CHECK (rng::tag ("\xC3\xA9") == 0x0AC21707B7181E01ull); // U+00E9
    CHECK (rng::tag ("Gr\xC3\xBC\xC3\x9F"
                     "e")
           == 0x20E72E11E324B0B8ull); // "Gruesse" with U+00FC, U+00DF
    CHECK (rng::tag ("\xF0\x9F\x8E\xB5") == 0xFF4AA0387542F1FDull); // U+1F3B5
    CHECK (rng::tag (u8"é") == 0x0AC21707B7181E01ull);
    CHECK (rng::tag (u8"\U0001F3B5") == 0xFF4AA0387542F1FDull);
}

TEST_CASE ("key paths match the reference vectors", "[rng]")
{
    CHECK (rng::seedKey (0).h == 0xE220A8397B1DCDAFull);
    CHECK (rng::seedKey (1).h == 0x910A2DEC89025CC1ull);
    CHECK (rng::seedKey (7).h == 0x63CBE1E459320DD7ull);
    CHECK (rng::seedKey (134).h == 0xE85C7AFB627BF59Eull);
    CHECK (rng::seedKey (0xFFFFFFFFFFFFFFFFull).h == 0xE4D971771B652C20ull);

    CHECK (bassKey.h == 0xC9A436A3A3830A14ull);
    CHECK (rng::derive (rng::seedKey (7), rng::tag ("bass"), 3, 5) == bassKey);

    SECTION ("a signed component wraps modulo 2^64, like a conversion to uint64_t")
    {
        const rng::Key root = rng::seedKey (7);
        CHECK (root.with (-1).h == 0xA9A96B699DC41760ull);
        CHECK (root.with (-1) == root.with (0xFFFFFFFFFFFFFFFFull));
        CHECK (root.with (std::int8_t { -1 }) == root.with (std::int64_t { -1 }));
        CHECK (root.with (std::uint8_t { 5 }) == root.with (5));
        CHECK (root.with (true) == root.with (1));
    }

    SECTION ("an enum passes its underlying value")
    {
        enum class Lane : std::uint16_t { kick = 3 };
        CHECK (rng::derive (rng::seedKey (7), Lane::kick) == rng::seedKey (7).with (3));
    }
}

TEST_CASE ("the hook's permanent tags match the reference vectors", "[rng]")
{
    // The hook (S1b2b) hangs every draw below root.with (tag ("hook")), and the vox reply below
    // root.with (tag ("reply")): permanent top-level tags, so a renamed one fails here, not only in
    // the generator snapshots (test_gen_hook.cpp checks the generator's own keys against these).
    CHECK (rng::tag ("hook") == 0x4033F5CC7E757B8Aull);
    CHECK (rng::tag ("reply") == 0xF53601F2B1E97107ull);
    CHECK (rng::tag ("rhythm") == 0xCA2349BBCC0E5951ull);
    CHECK (rng::tag ("cell") == 0xD98B1E90FD653435ull);
    CHECK (rng::tag ("FUT01") == 0x67B52ECE1F719483ull);

    // FUT01 seed 7's root (gen::songRoot), then the hook's, the reply's and A's bar-1 cell's keys
    const rng::Key root = rng::seedKey (7).with (rng::tag ("FUT01"));
    CHECK (root.h == 0x4E503AB4C67A1176ull);
    CHECK (root.with (rng::tag ("hook")).h == 0x305D75E36F54C6F0ull);
    CHECK (root.with (rng::tag ("reply")).h == 0x25344576E166B7ACull);
    CHECK (root.with (rng::tag ("hook")).with (rng::tag ("rhythm")).with (rng::tag ("cell")).h == 0xC989FB08A5C35F99ull);
    CHECK (rng::derive (root, rng::tag ("hook"), rng::tag ("rhythm"), rng::tag ("cell")) == root.with (rng::tag ("hook")).with (rng::tag ("rhythm")).with (rng::tag ("cell")));
}

TEST_CASE ("below is Lemire's multiply-shift on the high 32 bits", "[rng]")
{
    CHECK (rng::below (bassKey, 0) == 0u);
    CHECK (rng::below (bassKey, 1) == 0u);
    CHECK (rng::below (bassKey, 2) == 1u);
    CHECK (rng::below (bassKey, 3) == 1u);
    CHECK (rng::below (bassKey, 7) == 3u);
    CHECK (rng::below (bassKey, 16) == 8u);
    CHECK (rng::below (bassKey, 100) == 52u);
    CHECK (rng::below (bassKey, 1000) == 522u);
    CHECK (rng::below (bassKey, 65536) == 34267u);
    CHECK (rng::below (bassKey, 0xFFFFFFFFu) == 2245754774u);

    // 16 draws below 16 along seed 7 / "kick" / bar 0 / step 0..15.
    const rng::Key kickBar0 = rng::seedKey (7).with (rng::tag ("kick")).with (0);
    const std::array<std::uint32_t, 16> expected = { 1, 0, 1, 10, 13, 8, 9, 10, 10, 1, 13, 1, 6, 10, 1, 6 };
    for (std::uint32_t step = 0; step < 16; ++step)
        CHECK (rng::below (kickBar0.with (step), 16) == expected[step]);

    SECTION ("10,000 draws match the reference in bulk")
    {
        const rng::Key root = rng::seedKey (7);
        std::uint64_t sum = 0;
        std::uint32_t x = 0;
        for (std::uint32_t i = 0; i < 10000; ++i)
        {
            const std::uint32_t v = rng::below (root.with (i), 1000);
            REQUIRE (v < 1000u);
            sum += v;
            x ^= v;
        }
        CHECK (sum == 5016603u);
        CHECK (x == 521u);
    }

    SECTION ("draws are spread evenly")
    {
        // 65536 draws below 16: every value within 5% of 4096 (the reference gives 3970-4209).
        const rng::Key k = rng::seedKey (7).with (rng::tag ("uniform"));
        std::array<int, 16> counts {};
        for (std::uint32_t i = 0; i < 65536; ++i)
            ++counts[rng::below (k.with (i), 16)];
        for (const int c : counts)
        {
            CHECK (c >= 3891);
            CHECK (c <= 4301);
        }
    }
}

TEST_CASE ("chance is true with probability p16 / 65536", "[rng]")
{
    // below (bassKey, 65536) is 34267, so chance is true exactly for p16 above it.
    CHECK_FALSE (rng::chance (bassKey, -1));
    CHECK_FALSE (rng::chance (bassKey, 0));
    CHECK_FALSE (rng::chance (bassKey, 1));
    CHECK_FALSE (rng::chance (bassKey, 32768));
    CHECK_FALSE (rng::chance (bassKey, 34267));
    CHECK (rng::chance (bassKey, 34268));
    CHECK (rng::chance (bassKey, 40000));
    CHECK (rng::chance (bassKey, 65535));
    CHECK (rng::chance (bassKey, 65536));
    CHECK (rng::chance (bassKey, 70000));

    SECTION ("p16 = 0 is never and 65536 is always, on every key")
    {
        const rng::Key k = rng::seedKey (3).with (rng::tag ("chance-ends"));
        for (std::uint32_t i = 0; i < 4096; ++i)
        {
            CHECK_FALSE (rng::chance (k.with (i), 0));
            CHECK (rng::chance (k.with (i), 65536));
        }
    }

    SECTION ("the rate matches p16")
    {
        // 25% of 65536 draws: the reference gives 16385; allow 1%.
        const rng::Key k = rng::seedKey (7).with (rng::tag ("chance"));
        int hits = 0;
        for (std::uint32_t i = 0; i < 65536; ++i)
            hits += rng::chance (k.with (i), 16384) ? 1 : 0;
        CHECK (hits == 16385);
        CHECK (std::abs (hits - 16384) < 656);
    }
}

TEST_CASE ("between gives a uniform integer from lo to hi inclusive", "[rng]")
{
    CHECK (rng::between (bassKey, 0, 0) == 0);
    CHECK (rng::between (bassKey, 5, 1) == 5); // hi <= lo gives lo
    CHECK (rng::between (bassKey, -5, 5) == 0);
    CHECK (rng::between (bassKey, 0, 127) == 66);
    CHECK (rng::between (bassKey, 1, 127) == 67);
    CHECK (rng::between (bassKey, std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max()) == 98271127);
    CHECK (rng::between (bassKey, std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::min() + 1) == -2147483647);

    const rng::Key k = rng::seedKey (11).with (rng::tag ("between"));
    for (std::uint32_t i = 0; i < 2000; ++i)
    {
        const std::int32_t v = rng::between (k.with (i), -3, 4);
        CHECK (v >= -3);
        CHECK (v <= 4);
    }
}

TEST_CASE ("pickWeighted picks by integer weights", "[rng]")
{
    const rng::Key root = rng::seedKey (7);
    CHECK (rng::pickWeighted (root.with (0), { 1, 2, 3, 4 }) == 3u);
    CHECK (rng::pickWeighted (root.with (1), { 0, 5, 0, 5 }) == 1u);
    CHECK (rng::pickWeighted (root.with (2), { -3, 7 }) == 1u);
    CHECK (rng::pickWeighted (root.with (5), { 10 }) == 0u);
    CHECK (rng::pickWeighted (root.with (6), { 1, 1, 1, 1, 1, 1, 1, 1 }) == 7u);

    SECTION ("nothing to pick gives the size")
    {
        CHECK (rng::pickWeighted (root.with (3), std::span<const std::int32_t> {}) == 0u);
        CHECK (rng::pickWeighted (root.with (4), { 0, 0 }) == 2u);
        CHECK (rng::pickWeighted (root.with (4), { -1, -5, 0 }) == 3u);
        // A total above 2^32 - 1 can't be drawn from: refused, not wrapped.
        CHECK (rng::pickWeighted (root.with (7), { 2147483647, 2147483647, 2 }) == 3u);
    }

    SECTION ("a span and a braced list pick the same")
    {
        const std::vector<std::int32_t> w = { 5, 0, 9, 2 };
        for (std::uint32_t i = 0; i < 64; ++i)
            CHECK (rng::pickWeighted (root.with (i), w) == rng::pickWeighted (root.with (i), { 5, 0, 9, 2 }));
    }

    SECTION ("weights of 0 or less are never picked, and the rates follow the weights")
    {
        const rng::Key k = root.with (rng::tag ("pick"));
        std::array<int, 3> counts {};
        for (std::uint32_t i = 0; i < 30000; ++i)
        {
            const auto picked = rng::pickWeighted (k.with (i), { 60, 30, 10 });
            REQUIRE (picked < 3u);
            ++counts[picked];
        }
        // The reference gives 17916, 9144 and 2940.
        CHECK (counts == std::array<int, 3> { 17916, 9144, 2940 });
        CHECK (std::abs (counts[0] - 18000) < 400);
        CHECK (std::abs (counts[1] - 9000) < 400);
        CHECK (std::abs (counts[2] - 3000) < 400);

        for (std::uint32_t i = 0; i < 3000; ++i)
        {
            const auto picked = rng::pickWeighted (k.with (i), { 0, 4, -2, 1, 0 });
            CHECK ((picked == 1u || picked == 3u));
        }
    }
}

TEST_CASE ("shuffle is Fisher-Yates with one key per swap", "[rng]")
{
    std::array<int, 10> items {};
    std::iota (items.begin(), items.end(), 0);
    rng::shuffle (rng::seedKey (7).with (rng::tag ("shuffle")), items);
    CHECK (items == std::array<int, 10> { 9, 6, 5, 3, 0, 8, 4, 7, 1, 2 });

    // Always a permutation, and the same key always gives the same one.
    const rng::Key k = rng::seedKey (8).with (rng::tag ("shuffle"));
    for (std::uint32_t i = 0; i < 200; ++i)
    {
        std::vector<int> a (37), b (37);
        std::iota (a.begin(), a.end(), 0);
        std::iota (b.begin(), b.end(), 0);
        rng::shuffle (k.with (i), a);
        rng::shuffle (k.with (i), b);
        CHECK (a == b);
        std::ranges::sort (a);
        for (int v = 0; v < 37; ++v)
            REQUIRE (a[static_cast<std::size_t> (v)] == v);
    }

    std::vector<int> none;
    rng::shuffle (k, none);
    CHECK (none.empty());
}

namespace
{
    // A toy generator: one lane's draws, 16 steps per bar. Each bar is drawn from
    // root / lane / bar / attempt / step, and retryBar keeps attempt `keptAttempt` of that bar, the
    // way a generator retries a bar until a check passes.
    std::vector<std::uint32_t> laneDraws (rng::Key root, std::uint64_t lane, int bars, std::uint32_t range, int retryBar = -1, std::uint32_t keptAttempt = 0)
    {
        std::vector<std::uint32_t> out;
        for (int bar = 0; bar < bars; ++bar)
        {
            const std::uint32_t attempt = bar == retryBar ? keptAttempt : 0u;
            const rng::Key barKey = rng::derive (root, lane, bar, attempt);
            for (std::uint32_t step = 0; step < 16; ++step)
                out.push_back (rng::below (barKey.with (step), range));
        }
        return out;
    }

    std::vector<std::uint32_t> barOf (const std::vector<std::uint32_t>& draws, int bar)
    {
        const auto first = draws.begin() + bar * 16;
        return { first, first + 16 };
    }
}

TEST_CASE ("changing one key component never shifts another lane's draws", "[rng]")
{
    const rng::Key root = rng::seedKey (7);
    const std::uint64_t kick = rng::tag ("kick");
    const std::uint64_t bass = rng::tag ("bass");

    const auto kickBefore = laneDraws (root, kick, 64, 16);
    const auto bassBefore = laneDraws (root, bass, 64, 12);

    SECTION ("adding a lane, and running the lanes in another order")
    {
        const auto perc = laneDraws (root, rng::tag ("perc"), 64, 16);
        const auto bassAfter = laneDraws (root, bass, 64, 12);
        const auto kickAfter = laneDraws (root, kick, 64, 16);
        CHECK (kickAfter == kickBefore);
        CHECK (bassAfter == bassBefore);
        CHECK (perc != kickBefore);
    }

    SECTION ("retrying one bar of the bass changes that bar and nothing else")
    {
        const auto bassRetried = laneDraws (root, bass, 64, 12, 3, 2);
        CHECK (laneDraws (root, kick, 64, 16) == kickBefore);
        for (int bar = 0; bar < 64; ++bar)
        {
            if (bar == 3)
                CHECK (barOf (bassRetried, bar) != barOf (bassBefore, bar));
            else
                CHECK (barOf (bassRetried, bar) == barOf (bassBefore, bar));
        }
    }

    SECTION ("changing the bass's options leaves the kick alone")
    {
        const auto bassNarrow = laneDraws (root, bass, 64, 7);
        CHECK (bassNarrow != bassBefore);
        CHECK (laneDraws (root, kick, 64, 16) == kickBefore);
    }

    SECTION ("another seed changes everything")
    {
        const auto kickSeed8 = laneDraws (rng::seedKey (8), kick, 64, 16);
        int same = 0;
        for (std::size_t i = 0; i < kickBefore.size(); ++i)
            same += kickSeed8[i] == kickBefore[i] ? 1 : 0;
        CHECK (same == 72); // the reference: 72 of 1024; about 1 in 16 agree by chance
        CHECK (same > 31);
        CHECK (same < 97);
    }

    SECTION ("sibling lanes are not correlated")
    {
        const auto kickAsWide = laneDraws (root, kick, 64, 16);
        const auto bassAsWide = laneDraws (root, bass, 64, 16);
        int same = 0;
        for (std::size_t i = 0; i < kickAsWide.size(); ++i)
            same += kickAsWide[i] == bassAsWide[i] ? 1 : 0;
        CHECK (same == 62); // the reference: 62 of 1024, against 64 expected by chance
    }
}

TEST_CASE ("small seeds and lane tags never give the same key", "[rng]")
{
    const std::array<const char*, 16> names = { "kick", "bass", "hats", "snare", "clap", "perc", "pad", "lead", "vox", "fx", "stab", "sub", "reese", "ride", "open", "tick" };
    std::vector<std::uint64_t> keys;
    keys.reserve (4096 * 32);
    for (std::uint64_t seed = 0; seed < 4096; ++seed)
    {
        const rng::Key sk = rng::seedKey (seed);
        for (const char* name : names)
            keys.push_back (sk.with (rng::tag (name)).h);
        for (std::uint64_t lane = 0; lane < 16; ++lane)
            keys.push_back (sk.with (lane).h); // numeric components next to tags
    }
    std::ranges::sort (keys);
    CHECK (std::ranges::adjacent_find (keys) == keys.end());
}
