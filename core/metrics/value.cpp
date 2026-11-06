#include "metrics/value.h"

#include "gen/json.h"
#include "util/file_io.h"
#include "util/utf8.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>

namespace wubansia::metrics
{
    namespace
    {
        const Value& missingValue() noexcept
        {
            static const Value m;
            return m;
        }

        // s with every byte that isn't part of a valid UTF-8 sequence replaced by U+FFFD, as
        // JavaScript's TextDecoder would read it. Valid text comes back unchanged.
        // The length of the UTF-8 sequence a lead byte starts.
        std::size_t sequenceLength (unsigned char lead)
        {
            if (lead < 0x80)
                return 1;
            if (lead >= 0xF0)
                return 4;
            if (lead >= 0xE0)
                return 3;
            return 2;
        }

        std::string validUtf8 (std::string_view s)
        {
            if (util::isValidUtf8 (s))
                return std::string (s);
            std::string out;
            for (std::size_t i = 0; i < s.size();)
            {
                const std::size_t len = sequenceLength (static_cast<unsigned char> (s[i]));
                if (len <= s.size() - i && util::isValidUtf8 (s.substr (i, len)))
                {
                    out.append (s.substr (i, len));
                    i += len;
                }
                else
                {
                    out += "\xEF\xBF\xBD";
                    ++i;
                }
            }
            return out;
        }

        // JSON.stringify's string quoting: ", \ and control characters escaped (the short forms
        // where JSON has them, else \u00xx), everything else as it is, so the text is always valid JSON.
        void quote (std::string& out, std::string_view text)
        {
            constexpr char hex[] = "0123456789abcdef";
            const std::string s = validUtf8 (text);
            out += '"';
            for (const char ch : s)
            {
                const auto c = static_cast<unsigned char> (ch);
                switch (c)
                {
                    case '"':
                        out += "\\\"";
                        break;
                    case '\\':
                        out += "\\\\";
                        break;
                    case '\b':
                        out += "\\b";
                        break;
                    case '\f':
                        out += "\\f";
                        break;
                    case '\n':
                        out += "\\n";
                        break;
                    case '\r':
                        out += "\\r";
                        break;
                    case '\t':
                        out += "\\t";
                        break;
                    default:
                        if (c < 0x20)
                        {
                            out += "\\u00";
                            out += hex[c >> 4];
                            out += hex[c & 0xF];
                        }
                        else
                            out += ch;
                }
            }
            out += '"';
        }

        void write (std::string& out, const Value& v, std::size_t depth)
        {
            switch (v.kind())
            {
                case Value::Kind::missing:
                    out += "null"; // only reached inside an array, as JSON.stringify writes undefined there
                    return;
                case Value::Kind::integer:
                    out += std::to_string (v.integer());
                    return;
                case Value::Kind::string:
                    quote (out, v.string());
                    return;
                case Value::Kind::array:
                {
                    if (v.items().empty())
                    {
                        out += "[]";
                        return;
                    }
                    out += "[\n";
                    for (std::size_t i = 0; i < v.items().size(); ++i)
                    {
                        out.append (depth + 1, ' ');
                        write (out, v.items()[i], depth + 1);
                        out += i + 1 < v.items().size() ? ",\n" : "\n";
                    }
                    out.append (depth, ' ');
                    out += ']';
                    return;
                }
                case Value::Kind::object:
                {
                    bool any = false;
                    for (const auto& [k, m] : v.members())
                    {
                        if (m.isMissing())
                            continue; // JSON.stringify leaves undefined members out
                        out += any ? ",\n" : "{\n";
                        any = true;
                        out.append (depth + 1, ' ');
                        quote (out, k);
                        out += ": ";
                        write (out, m, depth + 1);
                    }
                    if (!any)
                    {
                        out += "{}";
                        return;
                    }
                    out += '\n';
                    out.append (depth, ' ');
                    out += '}';
                    return;
                }
            }
        }

        util::Result<Value> convert (const gen::json::Value& j)
        {
            using R = util::Result<Value>;
            using T = gen::json::Value::Type;
            const auto where = [&j] { return "line " + std::to_string (j.line) + ", column " + std::to_string (j.column) + ": "; };
            switch (j.type)
            {
                case T::integer:
                    return R::success (Value::of (j.integer));
                case T::string:
                    return R::success (Value::of (std::string_view (j.string)));
                case T::array:
                {
                    Value out = Value::array();
                    for (const auto& item : j.items)
                    {
                        auto c = convert (item);
                        if (!c.ok())
                            return c;
                        out.push (std::move (c.value));
                    }
                    return R::success (std::move (out));
                }
                case T::object:
                {
                    Value out = Value::object();
                    for (const auto& m : j.members)
                    {
                        auto c = convert (m.value);
                        if (!c.ok())
                            return c;
                        out.set (m.key, std::move (c.value));
                    }
                    return R::success (std::move (out));
                }
                case T::null:
                    return R::failure (where() + "null isn't allowed: the results hold integers and strings only (-1 is \"no value\")");
                case T::boolean:
                    return R::failure (where() + "true and false aren't allowed: the results hold integers and strings only (1 and 0)");
            }
            return R::failure (where() + "unknown value");
        }

        std::string shownString (const std::string& s)
        {
            std::string out;
            quote (out, s.size() > 80 ? s.substr (0, 77) + "..." : s);
            return out;
        }

        std::string kindName (Value::Kind k)
        {
            switch (k)
            {
                case Value::Kind::missing:
                    return "missing";
                case Value::Kind::integer:
                    return "an integer";
                case Value::Kind::string:
                    return "a string";
                case Value::Kind::array:
                    return "an array";
                case Value::Kind::object:
                    return "an object";
            }
            return "?";
        }

        struct Comparer
        {
            std::span<const Tolerance> tolerances;
            std::size_t limit = 0;
            Comparison result;

            void differ (const std::string& path, const std::string& what)
            {
                ++result.differenceCount;
                if (result.differences.size() < limit)
                    result.differences.push_back ((path.empty() ? std::string ("(top)") : path) + ": " + what);
            }

            std::int64_t toleranceFor (std::string_view key) const
            {
                for (const auto& t : tolerances)
                    if (t.key == key)
                        return t.plusMinus;
                return 0;
            }

            // key: the name of the member the value is under (or the nearest one above, for array items)
            void run (const Value& a, const Value& e, const std::string& path, std::string_view key)
            {
                if (a.kind() != e.kind())
                {
                    if (a.isInteger() || a.isString() || e.isInteger() || e.isString())
                        ++result.compared;
                    if (e.isMissing())
                        differ (path, "not in the expected result");
                    else if (a.isMissing())
                        differ (path, "missing");
                    else
                        differ (path, kindName (a.kind()) + ", expected " + kindName (e.kind()));
                    return;
                }
                switch (e.kind())
                {
                    case Value::Kind::missing:
                        return;
                    case Value::Kind::integer:
                    {
                        ++result.compared;
                        if (a.integer() == e.integer())
                            return;
                        const std::int64_t tol = toleranceFor (key);
                        const std::int64_t diff = a.integer() > e.integer() ? a.integer() - e.integer() : e.integer() - a.integer();
                        if (diff <= tol)
                        {
                            ++result.withinTolerance;
                            return;
                        }
                        differ (path, std::to_string (a.integer()) + ", expected " + std::to_string (e.integer()) + (tol > 0 ? " (tolerance +-" + std::to_string (tol) + ")" : ""));
                        return;
                    }
                    case Value::Kind::string:
                        ++result.compared;
                        if (a.string() != e.string())
                            differ (path, shownString (a.string()) + ", expected " + shownString (e.string()));
                        return;
                    case Value::Kind::array:
                    {
                        if (a.items().size() != e.items().size())
                            differ (path, std::to_string (a.items().size()) + " items, expected " + std::to_string (e.items().size()));
                        const std::size_t n = std::min (a.items().size(), e.items().size());
                        for (std::size_t i = 0; i < n; ++i)
                            run (a.items()[i], e.items()[i], path + "[" + std::to_string (i) + "]", key);
                        return;
                    }
                    case Value::Kind::object:
                    {
                        for (const auto& [k, m] : e.members())
                        {
                            const std::string p = path.empty() ? k : path + "." + k;
                            const Value& am = a[k];
                            if (am.isMissing() && !m.isMissing())
                            {
                                differ (p, "missing");
                                continue;
                            }
                            run (am, m, p, k);
                        }
                        for (const auto& [k, m] : a.members())
                            if (!e.has (k) && !m.isMissing())
                                differ (path.empty() ? k : path + "." + k, "not expected");
                        return;
                    }
                }
            }
        };
    }

    Value Value::of (std::string_view s)
    {
        Value x;
        x.kind_ = Kind::string;
        x.string_ = std::string (s);
        return x;
    }

    Value Value::array()
    {
        Value x;
        x.kind_ = Kind::array;
        return x;
    }

    Value Value::object()
    {
        Value x;
        x.kind_ = Kind::object;
        return x;
    }

    const Value& Value::operator[] (std::string_view key) const noexcept
    {
        if (kind_ == Kind::object)
            for (const auto& [k, v] : members_)
                if (k == key)
                    return v;
        return missingValue();
    }

    const Value& Value::operator[] (std::size_t index) const noexcept
    {
        if (kind_ == Kind::array && index < items_.size())
            return items_[index];
        return missingValue();
    }

    Value& Value::set (std::string_view key, Value v)
    {
        if (kind_ != Kind::object)
        {
            *this = object();
        }
        for (auto& [k, existing] : members_)
            if (k == key)
            {
                existing = std::move (v);
                return existing;
            }
        members_.emplace_back (std::string (key), std::move (v));
        return members_.back().second;
    }

    Value& Value::push (Value v)
    {
        if (kind_ != Kind::array)
            *this = array();
        items_.push_back (std::move (v));
        return items_.back();
    }

    std::string toJson (const Value& value)
    {
        std::string out;
        write (out, value, 0);
        return out;
    }

    util::Result<Value> parseJson (std::string_view text)
    {
        auto parsed = gen::json::parse (text);
        if (!parsed.ok())
            return util::Result<Value>::failure (parsed.error);
        return convert (parsed.value);
    }

    util::Result<Value> readJsonFile (const std::filesystem::path& file)
    {
        const auto bytes = util::readBinaryFile (file);
        if (!bytes.ok())
            return util::Result<Value>::failure (bytes.error);
        auto v = parseJson (std::string_view (reinterpret_cast<const char*> (bytes.value.data()), bytes.value.size()));
        if (!v.ok())
        {
            const auto name = file.generic_u8string();
            return util::Result<Value>::failure (std::string (name.begin(), name.end()) + ": " + v.error);
        }
        return v;
    }

    Comparison compare (const Value& actual, const Value& expected, std::span<const Tolerance> tolerances, std::size_t limit)
    {
        Comparer c { tolerances, limit, {} };
        c.run (actual, expected, "", "");
        return c.result;
    }
}
