#pragma once

// Counter-based random numbers for the generator (docs/plans/S1-futuristic-slice.md §4.2).
//
// Every draw is a pure function of a key path, such as (seed, "bass", bar, step):
//
//     const rng::Key root = rng::seedKey (seed);
//     const auto r = rng::below (root.with (rng::tag ("bass")).with (bar).with (step), 16);
//
// Nothing here has state, so adding a lane, or retrying one bar with an attempt counter in its
// path, never shifts another lane's numbers. Integers only: no float, no std::*_distribution
// (libc++ and libstdc++ differ), so the same path gives the same number on every platform.

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <iterator>
#include <ranges>
#include <span>
#include <string_view>
#include <type_traits>

namespace wubansia::rng
{
    namespace detail
    {
        // char and wchar_t are signed on some platforms and unsigned on others, so the same byte
        // would give different keys there. Key paths take numbers and tag ()s instead.
        template <typename T>
        concept PlatformChar = std::same_as<std::remove_cv_t<T>, char> || std::same_as<std::remove_cv_t<T>, wchar_t>;
    }

    // SplitMix64's output function (Steele, Lea & Flood 2014), a bijection on 64-bit integers.
    // mix64 (0) == 0xE220A8397B1DCDAF, the first output of SplitMix64 seeded with 0.
    constexpr std::uint64_t mix64 (std::uint64_t z) noexcept
    {
        z += 0x9E3779B97F4A7C15ull;
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }

    // A position in the key tree. with (x) steps one level down: Key { mix64 (h ^ x) }.
    struct Key
    {
        std::uint64_t h = 0;

        constexpr Key with (std::uint64_t x) const noexcept { return { mix64 (h ^ x) }; }

        // Other integer types take the value their conversion to uint64_t gives: modulo 2^64, so
        // int -1 is 0xFFFFFFFFFFFFFFFF, the same as the implicit conversion. This overload only
        // spares callers the cast (and the sign-conversion warning).
        template <std::integral T>
        constexpr Key with (T x) const noexcept
        {
            static_assert (!detail::PlatformChar<T>, "char and wchar_t differ in signedness between platforms: pass a number or a tag ()");
            return with (static_cast<std::uint64_t> (x));
        }

        friend constexpr bool operator== (const Key&, const Key&) noexcept = default;
    };

    // A number in [0, n): the high 32 bits of mix64 (k.h), scaled by n with Lemire's multiply-shift
    // (no rejection loop, so a draw always costs the same). below (k, 0) is 0.
    constexpr std::uint32_t below (Key k, std::uint32_t n) noexcept
    {
        return std::uint32_t ((std::uint64_t (std::uint32_t (mix64 (k.h) >> 32)) * n) >> 32);
    }

    // The fixed hash for names in key paths: FNV-1a, 64 bit, over the string's bytes, which are
    // UTF-8 (offset basis 0xCBF29CE484222325, prime 0x100000001B3). Bytes are read as unsigned,
    // so the result doesn't depend on whether char is signed. tag ("") == 0xCBF29CE484222325,
    // tag ("a") == 0xAF63DC4C8601EC8C, tag ("foobar") == 0x85944171F73967E8.
    // A tag's hash is permanent: renaming a tag changes every draw below it.
    constexpr std::uint64_t tag (std::string_view s) noexcept
    {
        std::uint64_t h = 0xCBF29CE484222325ull;
        for (const char c : s)
        {
            h ^= static_cast<std::uint64_t> (static_cast<unsigned char> (c));
            h *= 0x100000001B3ull;
        }
        return h;
    }

    constexpr std::uint64_t tag (std::u8string_view s) noexcept
    {
        std::uint64_t h = 0xCBF29CE484222325ull;
        for (const char8_t c : s)
        {
            h ^= static_cast<std::uint64_t> (c);
            h *= 0x100000001B3ull;
        }
        return h;
    }

    // The root of a song's key tree: Key {}.with (seed). Start every path here rather than at
    // Key { seed }, so small seeds and tag hashes never meet in the same XOR.
    constexpr Key seedKey (std::uint64_t seed) noexcept { return Key {}.with (seed); }

    // What derive () takes: integers other than char and wchar_t (as in Key::with), and enums
    // whose underlying type isn't one of those two.
    template <typename T>
    concept KeyPart = (std::integral<T> && !detail::PlatformChar<T>)
                      || (std::is_enum_v<T> && !detail::PlatformChar<std::underlying_type_t<T>>);

    // k.with (a).with (b)... in one call: derive (root, tag ("kick"), bar, step). An enum passes its
    // underlying value; signed values wrap as in Key::with.
    template <KeyPart... Parts>
    constexpr Key derive (Key k, Parts... parts) noexcept
    {
        const auto toU64 = [] (auto part) noexcept {
            using Part = decltype (part);
            if constexpr (std::is_enum_v<Part>)
                return static_cast<std::uint64_t> (static_cast<std::underlying_type_t<Part>> (part));
            else
                return static_cast<std::uint64_t> (part);
        };
        ((k = k.with (toU64 (parts))), ...);
        return k;
    }

    // True with probability p16 / 65536: p16 <= 0 is never, p16 >= 65536 is always.
    // Same as below (k, 65536) < p16, which reads the top 16 bits of mix64 (k.h).
    constexpr bool chance (Key k, std::int32_t p16) noexcept
    {
        if (p16 <= 0)
            return false;
        if (p16 >= 65536)
            return true;
        return below (k, 65536u) < static_cast<std::uint32_t> (p16);
    }

    // A uniform integer in [lo, hi], both ends included: lo + below (k, hi - lo + 1).
    // hi <= lo gives lo. The full int32 range uses the high 32 bits of mix64 (k.h) as they are.
    constexpr std::int32_t between (Key k, std::int32_t lo, std::int32_t hi) noexcept
    {
        if (hi <= lo)
            return lo;
        const auto count = static_cast<std::uint64_t> (static_cast<std::int64_t> (hi) - lo) + 1u; // 2 .. 2^32
        const std::uint64_t r = count > 0xFFFFFFFFull ? (mix64 (k.h) >> 32)
                                                      : below (k, static_cast<std::uint32_t> (count));
        return static_cast<std::int32_t> (static_cast<std::int64_t> (lo) + static_cast<std::int64_t> (r));
    }

    // A uniform index in [0, n), for picking from a container: v[pickIndex (k, v.size ())].
    // n == 0 gives 0; n above 2^32 - 1 counts as 2^32 - 1.
    constexpr std::size_t pickIndex (Key k, std::size_t n) noexcept
    {
        const auto n32 = n > 0xFFFFFFFFu ? 0xFFFFFFFFu : static_cast<std::uint32_t> (n);
        return below (k, n32);
    }

    // Picks index i with probability weights[i] / (sum of the weights): integer weights, so there
    // is nothing to round. Draws r = below (k, total) and returns the first i whose running sum
    // passes r. Weights <= 0 are never picked. Returns weights.size () when nothing can be picked:
    // no weights, none above 0, or a total above 2^32 - 1.
    constexpr std::size_t pickWeighted (Key k, std::span<const std::int32_t> weights) noexcept
    {
        std::uint64_t total = 0;
        for (const std::int32_t w : weights)
            if (w > 0)
                total += static_cast<std::uint64_t> (w);

        if (total == 0 || total > 0xFFFFFFFFull)
            return weights.size();

        std::uint32_t r = below (k, static_cast<std::uint32_t> (total));
        for (std::size_t i = 0; i < weights.size(); ++i)
        {
            if (weights[i] <= 0)
                continue;
            const auto w = static_cast<std::uint32_t> (weights[i]);
            if (r < w)
                return i;
            r -= w;
        }
        return weights.size(); // not reached: r < total
    }

    // pickWeighted (k, { 60, 30, 10 }): std::span can't take a braced list in C++20.
    constexpr std::size_t pickWeighted (Key k, std::initializer_list<std::int32_t> weights) noexcept
    {
        return pickWeighted (k, std::span<const std::int32_t> (weights.begin(), weights.size()));
    }

    // Fisher-Yates from the back: for i = n - 1 down to 1, swap items[i] with
    // items[below (k.with (i), i + 1)]. Each swap has its own key, so the result depends only on
    // k and n. For ranges of fewer than 2^32 items.
    template <std::ranges::random_access_range R>
        requires std::ranges::sized_range<R> && std::permutable<std::ranges::iterator_t<R>>
    constexpr void shuffle (Key k, R&& items)
    {
        using Diff = std::ranges::range_difference_t<R>;
        const auto n = static_cast<std::uint64_t> (std::ranges::size (items));
        const auto first = std::ranges::begin (items);
        for (std::uint64_t i = n; i > 1; --i)
        {
            const std::uint64_t last = i - 1;
            const std::uint64_t j = below (k.with (last), static_cast<std::uint32_t> (i));
            std::ranges::iter_swap (first + static_cast<Diff> (last), first + static_cast<Diff> (j));
        }
    }
}
