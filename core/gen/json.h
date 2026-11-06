#pragma once

// A strict, integer-only JSON reader for the content files (content/kits, styles, feels). The plan
// keeps core/ free of floating point in every decision (docs/plans/S1-futuristic-slice.md §4.2), so
// the content is integers and strings only, and this reader refuses anything else rather than
// rounding it. No dependency: nlohmann/json was declined (plan §6.1).
//
// What parse () accepts is RFC 8259 JSON with these restrictions, each refused with a line and
// column (counted in characters, from 1):
// - numbers are integers that fit int64: -?(0|[1-9][0-9]*). A fraction or an exponent ("1.0",
//   "1e3") is refused, and so are "-0" and leading zeros ("007").
// - an object may not name the same key twice
// - the text must be valid UTF-8, with no byte order mark; strings may not hold raw control
//   characters (below U+0020) or lone surrogates (\uD800 on its own)
// - nesting is at most maxDepth deep
// Object members keep their file order, so error messages and iteration follow the file.
//
// Node is a read-only cursor for the loaders: it knows its path ("lanes[2].grids.main"), checks
// types and ranges, refuses keys it wasn't told about (a typo in the content is an error, never a
// silent default) and records the first problem in a shared Errors. Loaders read everything and
// check the error once at the end.

#include "util/result.h"

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace wubansia::gen::json
{
    inline constexpr std::size_t maxDepth = 64;

    struct Member;

    struct Value
    {
        enum class Type : std::uint8_t {
            null,
            boolean,
            integer,
            string,
            array,
            object
        };

        Type type = Type::null;
        bool boolean = false;
        std::int64_t integer = 0;
        std::string string; // UTF-8
        std::vector<Value> items; // array
        std::vector<Member> members; // object, in file order
        std::uint32_t line = 0; // where the value starts, from 1
        std::uint32_t column = 0;

        // The member called key, or nullptr (objects only).
        const Value* find (std::string_view key) const noexcept;
    };

    struct Member
    {
        std::string key;
        Value value;
    };

    // The name of a type for messages: "an integer", "an object"...
    std::string_view typeName (Value::Type type) noexcept;

    // Parses one JSON text. On failure the error reads "line L, column C: what is wrong".
    [[nodiscard]] util::Result<Value> parse (std::string_view text);

    // The first problem a loader finds, with the file and path it was found at.
    class Errors
    {
    public:
        explicit Errors (std::string source) : source_ (std::move (source)) {}

        // Keeps the first message only: later ones are usually consequences of it.
        void add (std::string_view path, std::string_view message);
        bool any() const noexcept { return !first_.empty(); }
        const std::string& first() const noexcept { return first_; }
        const std::string& source() const noexcept { return source_; }

    private:
        std::string source_;
        std::string first_;
    };

    class Node
    {
    public:
        Node (const Value* value, std::string path, Errors* errors) : value_ (value), path_ (std::move (path)), errors_ (errors) {}

        bool exists() const noexcept { return value_ != nullptr; }
        const std::string& path() const noexcept { return path_; }
        const Value* value() const noexcept { return value_; }
        Errors& errors() const noexcept { return *errors_; }

        bool isObject() const noexcept { return value_ != nullptr && value_->type == Value::Type::object; }
        bool isArray() const noexcept { return value_ != nullptr && value_->type == Value::Type::array; }
        bool isInteger() const noexcept { return value_ != nullptr && value_->type == Value::Type::integer; }
        bool isString() const noexcept { return value_ != nullptr && value_->type == Value::Type::string; }

        // Records an error at this node's path.
        void fail (std::string_view message) const;

        // A member that must be there (an error if it isn't, or if this isn't an object).
        Node operator[] (std::string_view key) const;
        // A member that may be missing: !exists () then, and no error.
        Node optional (std::string_view key) const;
        // An error for every member whose key isn't in allowed. Also an error if this isn't an object.
        void only (std::initializer_list<std::string_view> allowed) const;

        // Arrays.
        std::size_t size() const; // 0 (and an error) if this isn't an array
        Node at (std::size_t i) const;

        // Objects, in file order.
        std::vector<std::pair<std::string, Node>> members() const;

        // Typed reads. On a type or range error they record it and return lo (or "" / false).
        std::int64_t integer (std::int64_t lo, std::int64_t hi) const;
        std::int32_t int32 (std::int32_t lo, std::int32_t hi) const;
        std::string string() const;
        std::string string (std::size_t minLength, std::size_t maxLength) const;
        bool boolean() const;

        // An array of integers, each in [lo, hi], with a length in [minCount, maxCount].
        std::vector<std::int32_t> ints (std::size_t minCount, std::size_t maxCount, std::int32_t lo, std::int32_t hi) const;
        // An array of strings, each non-empty.
        std::vector<std::string> strings (std::size_t minCount, std::size_t maxCount) const;

        // optional (key) read with a default when it is missing.
        std::int32_t int32Or (std::string_view key, std::int32_t fallback, std::int32_t lo, std::int32_t hi) const;

    private:
        const Value* value_;
        std::string path_;
        Errors* errors_;
    };

    // The root node of a parsed document.
    inline Node root (const Value& value, Errors& errors) { return Node (&value, "", &errors); }
}
