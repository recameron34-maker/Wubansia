#pragma once

// The engine's enums are spelled as strings in the manifests (docs/plugin/manifest.md). Each enum's
// spellings sit next to it in an std::array<std::string_view, N> indexed by its value; idIndex ()
// looks a spelling up. Parsers, the test-side writers and tools/sound/plugin_common.py use the same
// spellings.

#include <array>
#include <cstddef>
#include <string_view>

namespace wubansia::engine
{
    // The position of id in ids, or -1 if it isn't there.
    template <std::size_t N>
    constexpr int idIndex (const std::array<std::string_view, N>& ids, std::string_view id) noexcept
    {
        for (std::size_t i = 0; i < N; ++i)
            if (ids[i] == id)
                return static_cast<int> (i);
        return -1;
    }
}
