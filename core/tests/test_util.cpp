// Tests for core/util: integer division that floors, saturating casts, Result, the stable sort
// helper, UTF-8 validation and whole-file I/O.

#include "util/file_io.h"
#include "util/ints.h"
#include "util/result.h"
#include "util/sort.h"
#include "util/utf8.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <vector>

using namespace wubansia;

static_assert (util::floorDiv (-1, 120) == -1);
static_assert (util::floorMod (-1, 120) == 119);
static_assert (util::roundDiv (5, 10) == 1);
static_assert (util::isValidUtf8 ("F\xE2\x99\xAF minor")); // U+266F, the sharp sign

TEST_CASE ("floorDiv and floorMod floor toward minus infinity", "[util]")
{
    CHECK (util::floorDiv (7, 2) == 3);
    CHECK (util::floorDiv (-7, 2) == -4);
    CHECK (util::floorDiv (7, -2) == -4);
    CHECK (util::floorDiv (-7, -2) == 3);
    CHECK (util::floorDiv (-1, 120) == -1); // a note pushed one tick early sits in the 16th before
    CHECK (util::floorDiv (-120, 120) == -1);
    CHECK (util::floorDiv (-121, 120) == -2);
    CHECK (util::floorDiv (0, 120) == 0);
    CHECK (util::floorDiv (119, 120) == 0);

    CHECK (util::floorMod (-1, 120) == 119);
    CHECK (util::floorMod (-120, 120) == 0);
    CHECK (util::floorMod (7, -2) == -1);
    CHECK (util::floorMod (-7, -2) == -1);
    CHECK (util::floorMod (1921, 1920) == 1);

    SECTION ("mixed signed types take their common type")
    {
        const std::int64_t tick = -5'000'000'000;
        const int bar = 1920;
        const auto q = util::floorDiv (tick, bar);
        static_assert (std::is_same_v<decltype (q), const std::int64_t>);
        CHECK (q == -2604167);
        CHECK (util::floorMod (tick, bar) == 640);
    }

    SECTION ("the extremes")
    {
        constexpr auto lo = std::numeric_limits<std::int64_t>::min();
        constexpr auto hi = std::numeric_limits<std::int64_t>::max();
        CHECK (util::floorDiv (lo, std::int64_t { 1 }) == lo);
        CHECK (util::floorDiv (hi, std::int64_t { -1 }) == -hi);
        CHECK (util::floorDiv (lo, std::int64_t { 2 }) == lo / 2);
        CHECK (util::floorMod (lo, std::int64_t { 3 }) == 1); // -2^63 = 3 * (-3074457345618258603) + 1
    }

    SECTION ("against the definition, exhaustively on a small range")
    {
        // q = floorDiv (a, b) is the integer with q*b <= a < (q+1)*b for b > 0 (reversed for b < 0),
        // and floorMod (a, b) == a - b*q.
        for (int a = -500; a <= 500; ++a)
        {
            for (int b = -17; b <= 17; ++b)
            {
                if (b == 0)
                    continue;
                const int q = util::floorDiv (a, b);
                if (b > 0)
                    REQUIRE ((q * b <= a && a < (q + 1) * b));
                else
                    REQUIRE ((q * b >= a && a > (q + 1) * b));
                REQUIRE (util::floorMod (a, b) == a - b * q);
            }
        }
    }
}

TEST_CASE ("ceilDiv rounds toward plus infinity", "[util]")
{
    CHECK (util::ceilDiv (7, 2) == 4);
    CHECK (util::ceilDiv (-7, 2) == -3);
    CHECK (util::ceilDiv (7, -2) == -3);
    CHECK (util::ceilDiv (-7, -2) == 4);
    CHECK (util::ceilDiv (8, 2) == 4);
    CHECK (util::ceilDiv (0, 5) == 0);

    for (int a = -500; a <= 500; ++a)
        for (int b = -17; b <= 17; ++b)
            if (b != 0)
                REQUIRE (util::ceilDiv (a, b) == -util::floorDiv (-a, b));
}

TEST_CASE ("roundDiv rounds halves up like JS Math.round (a / b)", "[util]")
{
    // { a, b, Math.round (a / b) } from Node 22 (CLAUDE.md: Math.round rounds .5 up).
    struct Case
    {
        int a, b, expected;
    };
    const std::array<Case, 22> fromNode = { {
        { 5, 10, 1 },
        { -5, 10, 0 },
        { -6, 10, -1 },
        { 25, 10, 3 },
        { -25, 10, -2 },
        { 15, -10, -1 },
        { -15, -10, 2 },
        { 1, 2, 1 },
        { -1, 2, 0 },
        { 3, 2, 2 },
        { -3, 2, -1 },
        { 7, 3, 2 },
        { -7, 3, -2 },
        { 119, 240, 0 },
        { 120, 240, 1 },
        { -120, 240, 0 },
        { -121, 240, -1 },
        { 1, 1000, 0 },
        { -500, 1000, 0 },
        { 500, 1000, 1 },
        { 1499, 1000, 1 },
        { -1501, 1000, -2 },
    } };
    for (const auto& c : fromNode)
    {
        INFO (c.a << " / " << c.b);
        CHECK (util::roundDiv (c.a, c.b) == c.expected);
    }

    SECTION ("the plan's swing delay: ((sw - 500) * 240 + 500) / 1000 ticks")
    {
        CHECK (util::roundDiv ((540 - 500) * 240, 1000) == 10);
        CHECK (util::roundDiv ((630 - 500) * 240, 1000) == 31);
        CHECK (util::roundDiv ((570 - 500) * 240, 1000) == 17); // FUT01's base swing: 16.8 ticks
        CHECK (util::roundDiv ((500 - 500) * 240, 1000) == 0);
    }

    SECTION ("against the definition: r <= a/b + 1/2 < r + 1")
    {
        for (int a = -500; a <= 500; ++a)
        {
            for (int b = -17; b <= 17; ++b)
            {
                if (b == 0)
                    continue;
                const int r = util::roundDiv (a, b);
                // Multiply through by 2b (flipping for b < 0): 2rb <= 2a + b < 2(r + 1)b.
                if (b > 0)
                    REQUIRE ((2 * r * b <= 2 * a + b && 2 * a + b < 2 * (r + 1) * b));
                else
                    REQUIRE ((2 * r * b >= 2 * a + b && 2 * a + b > 2 * (r + 1) * b));
            }
        }
    }

    SECTION ("narrow types are widened, so 2a + b can't overflow")
    {
        constexpr std::int32_t big = std::numeric_limits<std::int32_t>::max();
        CHECK (util::roundDiv (big, 2) == 1073741824);
        CHECK (util::roundDiv (std::int16_t { 32767 }, std::int16_t { 2 }) == 16384);
    }
}

TEST_CASE ("saturateCast clamps into the target type", "[util]")
{
    CHECK (util::saturateCast<std::uint8_t> (300) == 255);
    CHECK (util::saturateCast<std::uint8_t> (-1) == 0);
    CHECK (util::saturateCast<std::uint8_t> (127) == 127);
    CHECK (util::saturateCast<std::int8_t> (-200) == -128);
    CHECK (util::saturateCast<std::int8_t> (200) == 127);
    CHECK (util::saturateCast<std::int32_t> (std::numeric_limits<std::uint64_t>::max()) == std::numeric_limits<std::int32_t>::max());
    CHECK (util::saturateCast<std::uint32_t> (std::numeric_limits<std::int64_t>::min()) == 0u);
}

TEST_CASE ("Result holds a value or one error line", "[util]")
{
    const auto good = util::Result<int>::success (42);
    CHECK (good.ok());
    CHECK (static_cast<bool> (good));
    CHECK (good.value == 42);
    CHECK (good.error.empty());

    const auto bad = util::Result<int>::failure ("note 61 at tick 3 is out of range");
    CHECK_FALSE (bad.ok());
    CHECK (bad.error == "note 61 at tick 3 is out of range");

    // A failure always says something, so ok () can't be fooled by an empty message.
    CHECK_FALSE (util::Result<int>::failure ("").ok());
}

TEST_CASE ("stableSortBy sorts by a key and keeps the input order of ties", "[util]")
{
    struct Ev
    {
        int tick, lane, id;
    };
    std::vector<Ev> evs = { { 5, 1, 0 }, { 0, 2, 1 }, { 5, 0, 2 }, { 0, 2, 3 }, { -3, 9, 4 }, { 5, 1, 5 } };

    SECTION ("a partial key leaves ties in input order")
    {
        util::stableSortBy (evs, [] (const Ev& e) { return e.tick; });
        std::vector<int> ids;
        for (const auto& e : evs)
            ids.push_back (e.id);
        CHECK (ids == std::vector<int> { 4, 1, 3, 0, 2, 5 });
        CHECK (util::hasTies (evs, [] (const Ev& e) { return e.tick; }));
    }

    SECTION ("a tuple key that is a total order on the data has no ties")
    {
        const auto key = [] (const Ev& e) { return std::tuple (e.tick, e.lane, e.id); };
        util::stableSortBy (evs, key);
        std::vector<int> ids;
        for (const auto& e : evs)
            ids.push_back (e.id);
        CHECK (ids == std::vector<int> { 4, 1, 3, 2, 0, 5 });
        CHECK_FALSE (util::hasTies (evs, key));
    }

    SECTION ("every input order gives the same result when the key is total")
    {
        const auto key = [] (const Ev& e) { return std::tuple (e.tick, e.lane, e.id); };
        auto a = evs;
        auto b = std::vector<Ev> (evs.rbegin(), evs.rend());
        util::stableSortBy (a, key);
        util::stableSortBy (b, key);
        for (std::size_t i = 0; i < a.size(); ++i)
            CHECK (a[i].id == b[i].id);
    }

    SECTION ("empty and single ranges")
    {
        std::vector<Ev> none;
        util::stableSortBy (none, [] (const Ev& e) { return e.tick; });
        CHECK_FALSE (util::hasTies (none, [] (const Ev& e) { return e.tick; }));
        std::vector<Ev> one = { { 1, 1, 1 } };
        CHECK_FALSE (util::hasTies (one, [] (const Ev& e) { return e.tick; }));
    }
}

TEST_CASE ("isValidUtf8 follows RFC 3629", "[util]")
{
    // Valid: ASCII, every sequence length, the edges of each range.
    CHECK (util::isValidUtf8 (""));
    CHECK (util::isValidUtf8 ("drop 2 (16 bars)"));
    CHECK (util::isValidUtf8 ("\xC2\x80")); // U+0080
    CHECK (util::isValidUtf8 ("\xDF\xBF")); // U+07FF
    CHECK (util::isValidUtf8 ("\xE0\xA0\x80")); // U+0800
    CHECK (util::isValidUtf8 ("\xED\x9F\xBF")); // U+D7FF, the last before the surrogates
    CHECK (util::isValidUtf8 ("\xEE\x80\x80")); // U+E000
    CHECK (util::isValidUtf8 ("\xEF\xBF\xBF")); // U+FFFF
    CHECK (util::isValidUtf8 ("\xF0\x90\x80\x80")); // U+10000
    CHECK (util::isValidUtf8 ("\xF4\x8F\xBF\xBF")); // U+10FFFF
    CHECK (util::isValidUtf8 (std::string_view ("a\0b", 3))); // NUL is a valid character

    // Invalid.
    CHECK_FALSE (util::isValidUtf8 ("\x80")); // a lone continuation byte
    CHECK_FALSE (util::isValidUtf8 ("\xC3")); // truncated
    CHECK_FALSE (util::isValidUtf8 ("\xE2\x99")); // truncated
    CHECK_FALSE (util::isValidUtf8 ("\xC0\xAF")); // overlong "/"
    CHECK_FALSE (util::isValidUtf8 ("\xC1\xBF")); // overlong
    CHECK_FALSE (util::isValidUtf8 ("\xE0\x9F\xBF")); // overlong U+07FF
    CHECK_FALSE (util::isValidUtf8 ("\xF0\x8F\xBF\xBF")); // overlong U+FFFF
    CHECK_FALSE (util::isValidUtf8 ("\xED\xA0\x80")); // U+D800, a surrogate
    CHECK_FALSE (util::isValidUtf8 ("\xED\xBF\xBF")); // U+DFFF
    CHECK_FALSE (util::isValidUtf8 ("\xF4\x90\x80\x80")); // U+110000
    CHECK_FALSE (util::isValidUtf8 ("\xF5\x80\x80\x80"));
    CHECK_FALSE (util::isValidUtf8 ("\xFF"));
    CHECK_FALSE (util::isValidUtf8 ("caf\xE9")); // Latin-1, not UTF-8
    CHECK_FALSE (util::isValidUtf8 ("\xE2\x28\xA1")); // bad second byte
    CHECK_FALSE (util::isValidUtf8 ("\xE2\x99\x28")); // bad third byte
}

TEST_CASE ("binary files round-trip through writeBinaryFile and readBinaryFile", "[util]")
{
    const std::filesystem::path dir = std::filesystem::path (WUBANSIA_TEST_TMP_DIR) / "file_io";
    std::filesystem::create_directories (dir);
    const auto path = dir / "bytes.bin";

    std::vector<std::uint8_t> bytes;
    for (int i = 0; i < 1000; ++i)
        bytes.push_back (static_cast<std::uint8_t> ((i * 37) & 0xFF)); // every byte value, 0x0D and 0x0A included
    const auto written = util::writeBinaryFile (path, bytes);
    REQUIRE (written.ok());
    CHECK (written.value == bytes.size());

    const auto read = util::readBinaryFile (path);
    REQUIRE (read.ok());
    CHECK (read.value == bytes);

    SECTION ("an empty file")
    {
        REQUIRE (util::writeBinaryFile (path, {}).ok());
        const auto empty = util::readBinaryFile (path);
        REQUIRE (empty.ok());
        CHECK (empty.value.empty());
    }

    SECTION ("errors name the path")
    {
        const auto missing = util::readBinaryFile (dir / "no-such-file.mid");
        CHECK_FALSE (missing.ok());
        CHECK (missing.error.find ("no-such-file.mid") != std::string::npos);

        const auto noFolder = util::writeBinaryFile (dir / "no-such-folder" / "x.mid", bytes);
        CHECK_FALSE (noFolder.ok());
        CHECK (noFolder.error.find ("no-such-folder") != std::string::npos);

        CHECK_FALSE (util::readBinaryFile (dir).ok()); // a folder isn't a file
    }
}
