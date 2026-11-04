#pragma once

// A metric result: a tree of integers, strings, arrays and objects, the C++ form of the plain
// objects tools/legacy_audit/metrics.js returns. Object members keep the order they were set in,
// so toJson () writes a result the way the audit's JSON.stringify (result, null, 1) does, and a
// C++ result and core/tests/legacy_expected.json can be compared key by key (differences ()).
//
// Integers and strings only, like legacy_expected.json: a JavaScript null (a permille of nothing,
// the median of no values) is -1 here, the README's "no value"; lag-1 and DFA alpha use -9999
// for "undefined", as the audit does. Pass flags are the integers 1 and 0.

#include "util/result.h"

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace wubansia::metrics
{
    class Value
    {
    public:
        enum class Kind : std::uint8_t {
            missing, // what operator[] gives for a key or index that isn't there
            integer,
            string,
            array,
            object
        };

        using Member = std::pair<std::string, Value>;

        Value() = default; // missing

        template <std::integral T>
        static Value of (T v)
        {
            Value x;
            x.kind_ = Kind::integer;
            x.integer_ = static_cast<std::int64_t> (v);
            return x;
        }
        static Value of (std::string_view s);
        static Value array();
        static Value object();

        Kind kind() const noexcept { return kind_; }
        bool isMissing() const noexcept { return kind_ == Kind::missing; }
        bool isInteger() const noexcept { return kind_ == Kind::integer; }
        bool isString() const noexcept { return kind_ == Kind::string; }
        bool isArray() const noexcept { return kind_ == Kind::array; }
        bool isObject() const noexcept { return kind_ == Kind::object; }

        // The value; 0 or "" for another kind.
        std::int64_t integer() const noexcept { return kind_ == Kind::integer ? integer_ : 0; }
        const std::string& string() const noexcept { return string_; }
        const std::vector<Value>& items() const noexcept { return items_; }
        const std::vector<Member>& members() const noexcept { return members_; }
        std::size_t size() const noexcept { return kind_ == Kind::object ? members_.size() : items_.size(); }

        // A member or an item, or a missing value. Never fails.
        const Value& operator[] (std::string_view key) const noexcept;
        const Value& operator[] (std::size_t index) const noexcept;
        bool has (std::string_view key) const noexcept { return !(*this)[key].isMissing(); }
        // (*this)[key].integer (): the number a result holds under key.
        std::int64_t num (std::string_view key) const noexcept { return (*this)[key].integer(); }

        // Objects: sets a member, replacing one of that name in place or appending it (like a
        // JavaScript property assignment). Returns the member's value. A Value that isn't an object
        // (a missing one, typically) becomes an empty object first.
        Value& set (std::string_view key, Value v);
        template <std::integral T>
        Value& set (std::string_view key, T v)
        {
            return set (key, of (v));
        }
        Value& set (std::string_view key, std::string_view s) { return set (key, of (s)); }
        Value& set (std::string_view key, const char* s) { return set (key, of (std::string_view (s))); }
        Value& set (std::string_view key, const std::string& s) { return set (key, of (std::string_view (s))); }

        // Arrays: appends an item and returns it. A Value that isn't an array becomes an empty one first.
        Value& push (Value v);

        friend bool operator== (const Value&, const Value&) = default;

    private:
        Kind kind_ = Kind::missing;
        std::int64_t integer_ = 0;
        std::string string_;
        std::vector<Value> items_;
        std::vector<Member> members_;
    };

    // The value as JSON text, as JavaScript's JSON.stringify (value, null, 1) writes it: one space
    // of indent per level, "key": value, empty arrays and objects as [] and {}, strings with the
    // quote, the backslash and control characters escaped and everything else (UTF-8) as it is. No
    // final newline. A missing value inside an array is written null; inside an object it is left out.
    std::string toJson (const Value& value);

    // Reads JSON text (integers, strings, arrays and objects only; null, booleans and fractions are
    // refused) through core's integer-only JSON reader, gen/json.h. For legacy_expected.json and
    // the fixtures' expected.json.
    [[nodiscard]] util::Result<Value> parseJson (std::string_view text);

    // parseJson () on a file; errors start with the file's name.
    [[nodiscard]] util::Result<Value> readJsonFile (const std::filesystem::path& file);

    // A key whose integer values may differ by up to plusMinus, wherever it appears.
    struct Tolerance
    {
        std::string_view key;
        std::int64_t plusMinus = 0;
    };

    struct Comparison
    {
        std::vector<std::string> differences; // "path: what differs", at most the limit given
        std::size_t differenceCount = 0; // all of them, also past the limit
        std::size_t compared = 0; // integers and strings compared
        std::size_t withinTolerance = 0; // integers that differ, but within their tolerance
        bool ok() const noexcept { return differenceCount == 0; }
    };

    // Compares actual with expected: the same kinds, the same keys (in any order), equal arrays
    // and equal values, integers within the tolerance for their key. Paths read like
    // "gating.G7.ExtendedMix.hats.median_alpha_milli".
    Comparison compare (const Value& actual, const Value& expected, std::span<const Tolerance> tolerances = {}, std::size_t limit = 50);
}
