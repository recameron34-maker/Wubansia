#pragma once

// Integer helpers with rounding defined for negative numbers too. C++'s / and % truncate toward
// zero (-1 / 120 == 0), which puts a note pushed one tick early into the wrong 16th. These floor.
//
// The two arguments may have different signed types (an int64_t tick and an int step size);
// the result has their common type. Unsigned arguments are rejected on purpose.

#include <concepts>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <utility>

namespace wubansia::util
{
    // floor (a / b): floorDiv (-1, 120) == -1, floorDiv (-120, 120) == -1, floorDiv (7, -2) == -4.
    // b must not be 0, and (a, b) must not be (the type's minimum, -1).
    template <std::signed_integral A, std::signed_integral B>
    constexpr std::common_type_t<A, B> floorDiv (A a, B b) noexcept
    {
        using T = std::common_type_t<A, B>;
        const T x = a;
        const T y = b;
        const T q = static_cast<T> (x / y);
        const T r = static_cast<T> (x % y);
        return (r != 0 && ((r < 0) != (y < 0))) ? static_cast<T> (q - 1) : q;
    }

    // a - b * floorDiv (a, b), which has the sign of b: floorMod (-1, 120) == 119,
    // floorMod (7, -2) == -1. b must not be 0.
    template <std::signed_integral A, std::signed_integral B>
    constexpr std::common_type_t<A, B> floorMod (A a, B b) noexcept
    {
        using T = std::common_type_t<A, B>;
        const T x = a;
        const T y = b;
        const T r = static_cast<T> (x % y);
        return (r != 0 && ((r < 0) != (y < 0))) ? static_cast<T> (r + y) : r;
    }

    // ceil (a / b): ceilDiv (7, 2) == 4, ceilDiv (-7, 2) == -3. b must not be 0.
    template <std::signed_integral A, std::signed_integral B>
    constexpr std::common_type_t<A, B> ceilDiv (A a, B b) noexcept
    {
        using T = std::common_type_t<A, B>;
        const T x = a;
        const T y = b;
        const T q = static_cast<T> (x / y);
        const T r = static_cast<T> (x % y);
        return (r != 0 && ((r > 0) == (y > 0))) ? static_cast<T> (q + 1) : q;
    }

    // a / b rounded to the nearest integer, halves up toward +infinity: floor (a / b + 1/2), the
    // integer form of JS Math.round (a / b) (CLAUDE.md's parity note). roundDiv (5, 10) == 1,
    // roundDiv (-5, 10) == 0, roundDiv (-6, 10) == -1, roundDiv (25, 10) == 3.
    // Computed as floorDiv (2a + b, 2b), in 64 bits for narrower types; int64_t arguments must
    // stay within +-2^61. b must not be 0.
    template <std::signed_integral A, std::signed_integral B>
    constexpr std::common_type_t<A, B> roundDiv (A a, B b) noexcept
    {
        using T = std::common_type_t<A, B>;
        using W = std::conditional_t<(sizeof (T) < sizeof (std::int64_t)), std::int64_t, T>;
        const W x = a;
        const W y = b;
        return static_cast<T> (floorDiv (static_cast<W> (2 * x + y), static_cast<W> (2 * y)));
    }

    // v clamped into To's range: saturateCast<std::uint8_t> (300) == 255, saturateCast<int8_t> (-200) == -128.
    template <std::integral To, std::integral From>
    constexpr To saturateCast (From v) noexcept
    {
        if (std::cmp_less (v, std::numeric_limits<To>::min()))
            return std::numeric_limits<To>::min();
        if (std::cmp_greater (v, std::numeric_limits<To>::max()))
            return std::numeric_limits<To>::max();
        return static_cast<To> (v);
    }
}
