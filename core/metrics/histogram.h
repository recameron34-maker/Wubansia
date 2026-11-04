#pragma once

// Histograms as the audit writes them (tools/legacy_audit/README.md, "Building blocks"): lists sorted
// by key, and the most common value with ties going to the key that is smallest as a string in byte
// order. Internal to core/metrics (gating.cpp, reported.cpp).

#include "metrics/value.h"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace wubansia::metrics::detail
{
    inline std::string keyText (const std::string& k) { return k; }
    inline std::string keyText (std::int64_t k) { return std::to_string (k); }

    // The key with the highest count; on a tie, the smallest key compared as a string in byte order.
    template <typename K>
    std::optional<std::pair<K, std::int64_t>> topOf (const std::map<K, std::int64_t>& hist)
    {
        std::optional<std::pair<K, std::int64_t>> best;
        for (const auto& [k, c] : hist)
            if (!best || c > best->second || (c == best->second && keyText (k) < keyText (best->first)))
                best = std::make_pair (k, c);
        return best;
    }

    // [{"value": 7, "count": 405}, ...] by value.
    inline Value numberHistogram (const std::map<std::int64_t, std::int64_t>& hist)
    {
        Value out = Value::array();
        for (const auto& [k, c] : hist)
        {
            Value e = Value::object();
            e.set ("value", k);
            e.set ("count", c);
            out.push (std::move (e));
        }
        return out;
    }

    // [{"<name>": "2,2,2,2", "count": 26}, ...] by key in byte order.
    inline Value stringHistogram (const std::map<std::string, std::int64_t>& hist, std::string_view name)
    {
        Value out = Value::array();
        for (const auto& [k, c] : hist)
        {
            Value e = Value::object();
            e.set (name, k);
            e.set ("count", c);
            out.push (std::move (e));
        }
        return out;
    }
}
