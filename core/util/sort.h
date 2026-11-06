#pragma once

// Sorting with one answer on every standard library (docs/plans/S1-futuristic-slice.md §4.2).
//
// std::sort may order equal elements differently in libc++ and libstdc++. A stable sort keeps
// equal elements in their input order, so its result is fully determined by the key and the
// input, whichever library runs it. Give it a key that is a total order on what matters, e.g.
// std::tuple (e.tick, e.lane, e.note), and ties are only ever exact duplicates.

#include <algorithm>
#include <concepts>
#include <functional>
#include <iterator>
#include <ranges>
#include <type_traits>

namespace wubansia::util
{
    // Sorts ascending by key (element), compared with <, keeping the input order of equal keys.
    // (std::sortable's third argument is the projection itself: it applies std::projected.)
    template <std::ranges::random_access_range R, typename KeyFn>
        requires std::sortable<std::ranges::iterator_t<R>, std::ranges::less, KeyFn>
    void stableSortBy (R&& range, KeyFn key)
    {
        std::ranges::stable_sort (range, std::ranges::less {}, key);
    }

    // True if two neighbours have equal keys. After stableSortBy, a test can use it to show that
    // the key really is a total order on its data: no tie was left to the input order.
    template <std::ranges::forward_range R, typename KeyFn>
    bool hasTies (const R& range, KeyFn key)
    {
        auto it = std::ranges::begin (range);
        const auto end = std::ranges::end (range);
        if (it == end)
            return false;
        for (auto next = std::ranges::next (it); next != end; it = next, ++next)
            if (!(std::invoke (key, *it) < std::invoke (key, *next)) && !(std::invoke (key, *next) < std::invoke (key, *it)))
                return true;
        return false;
    }
}
