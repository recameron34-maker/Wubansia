#include "gen/json.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>

namespace wubansia::gen::json
{
    namespace
    {
        // The length of the well-formed UTF-8 sequence at text[pos] (RFC 3629), or 0.
        std::size_t utf8Length (std::string_view text, std::size_t pos) noexcept
        {
            const auto byte = [&] (std::size_t i) { return static_cast<unsigned char> (text[i]); };
            const unsigned char b0 = byte (pos);
            std::size_t need = 0;
            unsigned char lo = 0x80u, hi = 0xBFu;
            if (b0 < 0x80u)
                return 1;
            if (b0 >= 0xC2u && b0 <= 0xDFu)
                need = 1;
            else if (b0 == 0xE0u)
            {
                need = 2;
                lo = 0xA0u;
            }
            else if ((b0 >= 0xE1u && b0 <= 0xECu) || b0 == 0xEEu || b0 == 0xEFu)
                need = 2;
            else if (b0 == 0xEDu)
            {
                need = 2;
                hi = 0x9Fu;
            }
            else if (b0 == 0xF0u)
            {
                need = 3;
                lo = 0x90u;
            }
            else if (b0 >= 0xF1u && b0 <= 0xF3u)
                need = 3;
            else if (b0 == 0xF4u)
            {
                need = 3;
                hi = 0x8Fu;
            }
            else
                return 0;
            if (text.size() - pos <= need)
                return 0;
            if (byte (pos + 1) < lo || byte (pos + 1) > hi)
                return 0;
            for (std::size_t k = 2; k <= need; ++k)
                if ((byte (pos + k) & 0xC0u) != 0x80u)
                    return 0;
            return need + 1;
        }

        void appendUtf8 (std::string& out, std::uint32_t cp)
        {
            if (cp < 0x80u)
                out += static_cast<char> (cp);
            else if (cp < 0x800u)
            {
                out += static_cast<char> (0xC0u | (cp >> 6));
                out += static_cast<char> (0x80u | (cp & 0x3Fu));
            }
            else if (cp < 0x10000u)
            {
                out += static_cast<char> (0xE0u | (cp >> 12));
                out += static_cast<char> (0x80u | ((cp >> 6) & 0x3Fu));
                out += static_cast<char> (0x80u | (cp & 0x3Fu));
            }
            else
            {
                out += static_cast<char> (0xF0u | (cp >> 18));
                out += static_cast<char> (0x80u | ((cp >> 12) & 0x3Fu));
                out += static_cast<char> (0x80u | ((cp >> 6) & 0x3Fu));
                out += static_cast<char> (0x80u | (cp & 0x3Fu));
            }
        }

        std::string describe (unsigned char c)
        {
            if (c >= 0x21 && c <= 0x7E)
                return std::string ("'") + static_cast<char> (c) + "'";
            static constexpr std::array<char, 16> hex = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
            std::string s = c < 0x80 ? "U+00" : "byte 0x";
            s += hex[c >> 4];
            s += hex[c & 15];
            return s;
        }

        class Parser
        {
        public:
            explicit Parser (std::string_view text) : text_ (text) {}

            util::Result<Value> run()
            {
                if (text_.size() >= 3 && static_cast<unsigned char> (text_[0]) == 0xEF && static_cast<unsigned char> (text_[1]) == 0xBB
                    && static_cast<unsigned char> (text_[2]) == 0xBF)
                    return fail ("the text starts with a UTF-8 byte order mark; save it without one");
                skipSpace();
                if (atEnd())
                    return fail ("the text is empty: a JSON value was expected");
                Value v;
                if (!value (v, 1))
                    return util::Result<Value>::failure (error_);
                skipSpace();
                if (!atEnd())
                    return fail ("unexpected " + describe (peek()) + " after the value");
                return util::Result<Value>::success (std::move (v));
            }

        private:
            std::string_view text_;
            std::size_t pos_ = 0;
            std::uint32_t line_ = 1;
            std::uint32_t column_ = 1;
            std::string error_;

            bool atEnd() const noexcept { return pos_ >= text_.size(); }
            unsigned char peek() const noexcept { return static_cast<unsigned char> (text_[pos_]); }

            void advance() noexcept
            {
                const unsigned char c = peek();
                ++pos_;
                if (c == '\n')
                {
                    ++line_;
                    column_ = 1;
                }
                else if ((c & 0xC0u) != 0x80u)
                    ++column_;
            }

            void skipSpace() noexcept
            {
                while (!atEnd() && (peek() == ' ' || peek() == '\t' || peek() == '\n' || peek() == '\r'))
                    advance();
            }

            util::Result<Value> fail (const std::string& message) { return fail (message, line_, column_); }
            util::Result<Value> fail (const std::string& message, std::uint32_t line, std::uint32_t column)
            {
                return util::Result<Value>::failure ("line " + std::to_string (line) + ", column " + std::to_string (column) + ": " + message);
            }

            bool error (const std::string& message) { return error (message, line_, column_); }
            bool error (const std::string& message, std::uint32_t line, std::uint32_t column)
            {
                if (error_.empty())
                    error_ = "line " + std::to_string (line) + ", column " + std::to_string (column) + ": " + message;
                return false;
            }

            bool unexpected (std::string_view expected)
            {
                if (atEnd())
                    return error ("unexpected end of the text: " + std::string (expected) + " was expected");
                return error ("unexpected " + describe (peek()) + ": " + std::string (expected) + " was expected");
            }

            bool value (Value& v, std::size_t depth)
            {
                if (depth > maxDepth)
                    return error ("nested too deep: more than " + std::to_string (maxDepth) + " levels");
                v.line = line_;
                v.column = column_;
                if (atEnd())
                    return unexpected ("a value");
                const unsigned char c = peek();
                if (c == '{')
                    return object (v, depth);
                if (c == '[')
                    return array (v, depth);
                if (c == '"')
                {
                    v.type = Value::Type::string;
                    return string (v.string);
                }
                if (c == '-' || (c >= '0' && c <= '9'))
                    return number (v);
                if (c == 't')
                    return literal (v, "true", Value::Type::boolean, true);
                if (c == 'f')
                    return literal (v, "false", Value::Type::boolean, false);
                if (c == 'n')
                    return literal (v, "null", Value::Type::null, false);
                return unexpected ("a value");
            }

            bool literal (Value& v, std::string_view word, Value::Type type, bool b)
            {
                for (const char w : word)
                {
                    if (atEnd() || peek() != static_cast<unsigned char> (w))
                        return unexpected (std::string ("\"") + std::string (word) + "\"");
                    advance();
                }
                v.type = type;
                v.boolean = b;
                return true;
            }

            bool number (Value& v)
            {
                const std::uint32_t line = line_, column = column_;
                const std::size_t start = pos_;
                bool negative = false;
                if (peek() == '-')
                {
                    negative = true;
                    advance();
                    if (atEnd() || peek() < '0' || peek() > '9')
                        return error ("a digit was expected after '-'", line, column);
                }
                // The magnitude, up to 2^63 (the most negative int64).
                constexpr std::uint64_t limit = static_cast<std::uint64_t> (std::numeric_limits<std::int64_t>::max()) + 1u;
                std::uint64_t magnitude = 0;
                bool overflow = false;
                const bool leadingZero = peek() == '0';
                while (!atEnd() && peek() >= '0' && peek() <= '9')
                {
                    const auto digit = static_cast<std::uint64_t> (peek() - '0');
                    if (magnitude > (limit - digit) / 10u)
                        overflow = true;
                    else
                        magnitude = magnitude * 10u + digit;
                    advance();
                }
                const std::string_view digits = text_.substr (start, pos_ - start);
                if (!atEnd() && (peek() == '.' || peek() == 'e' || peek() == 'E'))
                {
                    std::size_t end = pos_;
                    while (end < text_.size() && (text_[end] == '.' || text_[end] == 'e' || text_[end] == 'E' || text_[end] == '+' || text_[end] == '-' || (text_[end] >= '0' && text_[end] <= '9')))
                        ++end;
                    return error (std::string (text_.substr (start, end - start)) + " is not an integer: the content JSON is integer-only", line, column);
                }
                if (leadingZero && digits.size() > (negative ? 2u : 1u))
                    return error (std::string (digits) + " has a leading zero", line, column);
                if (negative && magnitude == 0 && !overflow)
                    return error ("write 0, not -0", line, column);
                if (overflow || (!negative && magnitude == limit))
                    return error (std::string (digits) + " is out of range: integers must fit in 64 bits", line, column);
                v.type = Value::Type::integer;
                v.integer = negative ? (magnitude == limit ? std::numeric_limits<std::int64_t>::min() : -static_cast<std::int64_t> (magnitude))
                                     : static_cast<std::int64_t> (magnitude);
                return true;
            }

            bool hex4 (std::uint32_t& out)
            {
                out = 0;
                for (int i = 0; i < 4; ++i)
                {
                    if (atEnd())
                        return false;
                    const unsigned char c = peek();
                    std::uint32_t d = 0;
                    if (c >= '0' && c <= '9')
                        d = c - '0';
                    else if (c >= 'a' && c <= 'f')
                        d = 10u + (c - 'a');
                    else if (c >= 'A' && c <= 'F')
                        d = 10u + (c - 'A');
                    else
                        return false;
                    out = out * 16u + d;
                    advance();
                }
                return true;
            }

            bool string (std::string& out)
            {
                const std::uint32_t line = line_, column = column_;
                advance(); // the opening quote
                while (true)
                {
                    if (atEnd())
                        return error ("unterminated string", line, column);
                    const unsigned char c = peek();
                    if (c == '"')
                    {
                        advance();
                        return true;
                    }
                    if (c == '\\')
                    {
                        const std::uint32_t el = line_, ec = column_;
                        advance();
                        if (atEnd())
                            return error ("unterminated string", line, column);
                        const unsigned char e = peek();
                        advance();
                        switch (e)
                        {
                            case '"':
                                out += '"';
                                break;
                            case '\\':
                                out += '\\';
                                break;
                            case '/':
                                out += '/';
                                break;
                            case 'b':
                                out += '\b';
                                break;
                            case 'f':
                                out += '\f';
                                break;
                            case 'n':
                                out += '\n';
                                break;
                            case 'r':
                                out += '\r';
                                break;
                            case 't':
                                out += '\t';
                                break;
                            case 'u':
                            {
                                std::uint32_t cp = 0;
                                if (!hex4 (cp))
                                    return error ("an invalid \\u escape: 4 hex digits were expected", el, ec);
                                if (cp >= 0xDC00u && cp <= 0xDFFFu)
                                    return error ("a lone surrogate \\u escape (a low surrogate with no high one before it)", el, ec);
                                if (cp >= 0xD800u && cp <= 0xDBFFu)
                                {
                                    std::uint32_t low = 0;
                                    if (atEnd() || peek() != '\\')
                                        return error ("a lone surrogate \\u escape (a high surrogate with no low one after it)", el, ec);
                                    advance();
                                    if (atEnd() || peek() != 'u')
                                        return error ("a lone surrogate \\u escape (a high surrogate with no low one after it)", el, ec);
                                    advance();
                                    if (!hex4 (low) || low < 0xDC00u || low > 0xDFFFu)
                                        return error ("a lone surrogate \\u escape (a high surrogate with no low one after it)", el, ec);
                                    cp = 0x10000u + ((cp - 0xD800u) << 10) + (low - 0xDC00u);
                                }
                                appendUtf8 (out, cp);
                                break;
                            }
                            default:
                                return error ("an invalid escape \\" + std::string (1, static_cast<char> (e)) + " in a string", el, ec);
                        }
                        continue;
                    }
                    if (c < 0x20)
                        return error ("a raw control character (" + describe (c) + ") in a string: write it as an escape");
                    const std::size_t n = utf8Length (text_, pos_);
                    if (n == 0)
                        return error ("invalid UTF-8 in a string (" + describe (c) + ")");
                    out.append (text_.substr (pos_, n));
                    for (std::size_t k = 0; k < n; ++k)
                        advance();
                }
            }

            bool array (Value& v, std::size_t depth)
            {
                v.type = Value::Type::array;
                advance(); // [
                skipSpace();
                if (!atEnd() && peek() == ']')
                {
                    advance();
                    return true;
                }
                while (true)
                {
                    skipSpace();
                    if (atEnd() || peek() == ']' || peek() == ',')
                        return unexpected ("a value");
                    Value item;
                    if (!value (item, depth + 1))
                        return false;
                    v.items.push_back (std::move (item));
                    skipSpace();
                    if (atEnd())
                        return unexpected ("',' or ']'");
                    if (peek() == ',')
                    {
                        advance();
                        continue;
                    }
                    if (peek() == ']')
                    {
                        advance();
                        return true;
                    }
                    return unexpected ("',' or ']'");
                }
            }

            bool object (Value& v, std::size_t depth)
            {
                v.type = Value::Type::object;
                advance(); // {
                skipSpace();
                if (!atEnd() && peek() == '}')
                {
                    advance();
                    return true;
                }
                while (true)
                {
                    skipSpace();
                    if (atEnd() || peek() != '"')
                        return unexpected ("a key in double quotes");
                    const std::uint32_t kl = line_, kc = column_;
                    Member m;
                    if (!string (m.key))
                        return false;
                    for (const auto& other : v.members)
                        if (other.key == m.key)
                            return error ("the object has the key \"" + m.key + "\" twice", kl, kc);
                    skipSpace();
                    if (atEnd() || peek() != ':')
                        return unexpected ("':' after the key");
                    advance();
                    skipSpace();
                    if (!value (m.value, depth + 1))
                        return false;
                    v.members.push_back (std::move (m));
                    skipSpace();
                    if (atEnd())
                        return unexpected ("',' or '}'");
                    if (peek() == ',')
                    {
                        advance();
                        continue;
                    }
                    if (peek() == '}')
                    {
                        advance();
                        return true;
                    }
                    return unexpected ("',' or '}'");
                }
            }
        };

        std::string child (const std::string& path, std::string_view key)
        {
            return path.empty() ? std::string (key) : path + "." + std::string (key);
        }
    }

    const Value* Value::find (std::string_view key) const noexcept
    {
        if (type != Type::object)
            return nullptr;
        for (const auto& m : members)
            if (m.key == key)
                return &m.value;
        return nullptr;
    }

    std::string_view typeName (Value::Type type) noexcept
    {
        switch (type)
        {
            case Value::Type::null:
                return "null";
            case Value::Type::boolean:
                return "a boolean";
            case Value::Type::integer:
                return "an integer";
            case Value::Type::string:
                return "a string";
            case Value::Type::array:
                return "an array";
            case Value::Type::object:
                return "an object";
        }
        return "a value";
    }

    util::Result<Value> parse (std::string_view text) { return Parser (text).run(); }

    void Errors::add (std::string_view path, std::string_view message)
    {
        if (!first_.empty())
            return;
        first_ = source_;
        if (!path.empty())
            first_ += std::string (first_.empty() ? "" : ": ") + std::string (path);
        first_ += std::string (first_.empty() ? "" : ": ") + std::string (message);
    }

    void Node::fail (std::string_view message) const
    {
        if (value_ != nullptr && value_->line > 0)
            errors_->add (path_, "line " + std::to_string (value_->line) + ": " + std::string (message));
        else
            errors_->add (path_, message);
    }

    Node Node::operator[] (std::string_view key) const
    {
        if (value_ == nullptr)
            return Node (nullptr, child (path_, key), errors_);
        if (value_->type != Value::Type::object)
        {
            fail ("an object was expected, not " + std::string (typeName (value_->type)));
            return Node (nullptr, child (path_, key), errors_);
        }
        const Value* v = value_->find (key);
        if (v == nullptr)
            fail ("missing \"" + std::string (key) + "\"");
        return Node (v, child (path_, key), errors_);
    }

    Node Node::optional (std::string_view key) const
    {
        if (value_ == nullptr)
            return Node (nullptr, child (path_, key), errors_);
        if (value_->type != Value::Type::object)
        {
            fail ("an object was expected, not " + std::string (typeName (value_->type)));
            return Node (nullptr, child (path_, key), errors_);
        }
        return Node (value_->find (key), child (path_, key), errors_);
    }

    void Node::only (std::initializer_list<std::string_view> allowed) const
    {
        if (value_ == nullptr)
            return;
        if (value_->type != Value::Type::object)
        {
            fail ("an object was expected, not " + std::string (typeName (value_->type)));
            return;
        }
        for (const auto& m : value_->members)
        {
            bool known = false;
            for (const auto a : allowed)
                known = known || m.key == a;
            if (!known)
            {
                std::string list;
                for (const auto a : allowed)
                    list += (list.empty() ? "" : ", ") + std::string (a);
                Node (&m.value, path_, errors_).fail ("unknown key \"" + m.key + "\" (known: " + list + ")");
            }
        }
    }

    std::size_t Node::size() const
    {
        if (value_ == nullptr)
            return 0;
        if (value_->type != Value::Type::array)
        {
            fail ("an array was expected, not " + std::string (typeName (value_->type)));
            return 0;
        }
        return value_->items.size();
    }

    Node Node::at (std::size_t i) const
    {
        const std::string p = path_ + "[" + std::to_string (i) + "]";
        if (value_ == nullptr || value_->type != Value::Type::array || i >= value_->items.size())
            return Node (nullptr, p, errors_);
        return Node (&value_->items[i], p, errors_);
    }

    std::vector<std::pair<std::string, Node>> Node::members() const
    {
        std::vector<std::pair<std::string, Node>> out;
        if (value_ == nullptr)
            return out;
        if (value_->type != Value::Type::object)
        {
            fail ("an object was expected, not " + std::string (typeName (value_->type)));
            return out;
        }
        for (const auto& m : value_->members)
            out.emplace_back (m.key, Node (&m.value, child (path_, m.key), errors_));
        return out;
    }

    std::int64_t Node::integer (std::int64_t lo, std::int64_t hi) const
    {
        if (value_ == nullptr)
            return lo;
        if (value_->type != Value::Type::integer)
        {
            fail ("an integer was expected, not " + std::string (typeName (value_->type)));
            return lo;
        }
        if (value_->integer < lo || value_->integer > hi)
        {
            fail (std::to_string (value_->integer) + " is out of range: it must be " + std::to_string (lo) + " to " + std::to_string (hi));
            return lo;
        }
        return value_->integer;
    }

    std::int32_t Node::int32 (std::int32_t lo, std::int32_t hi) const { return static_cast<std::int32_t> (integer (lo, hi)); }

    std::string Node::string() const
    {
        if (value_ == nullptr)
            return {};
        if (value_->type != Value::Type::string)
        {
            fail ("a string was expected, not " + std::string (typeName (value_->type)));
            return {};
        }
        return value_->string;
    }

    std::string Node::string (std::size_t minLength, std::size_t maxLength) const
    {
        std::string s = string();
        if (value_ != nullptr && value_->type == Value::Type::string && (s.size() < minLength || s.size() > maxLength))
        {
            fail ("the string must be " + std::to_string (minLength) + " to " + std::to_string (maxLength) + " bytes long, not " + std::to_string (s.size()));
            return {};
        }
        return s;
    }

    bool Node::boolean() const
    {
        if (value_ == nullptr)
            return false;
        if (value_->type != Value::Type::boolean)
        {
            fail ("true or false was expected, not " + std::string (typeName (value_->type)));
            return false;
        }
        return value_->boolean;
    }

    std::vector<std::int32_t> Node::ints (std::size_t minCount, std::size_t maxCount, std::int32_t lo, std::int32_t hi) const
    {
        std::vector<std::int32_t> out;
        const std::size_t n = size();
        if (value_ == nullptr || value_->type != Value::Type::array)
            return out;
        if (n < minCount || n > maxCount)
        {
            if (minCount == maxCount)
                fail ("it must hold " + std::to_string (minCount) + " values, not " + std::to_string (n));
            else
                fail ("it must hold " + std::to_string (minCount) + " to " + std::to_string (maxCount) + " values, not " + std::to_string (n));
            return out;
        }
        out.reserve (n);
        for (std::size_t i = 0; i < n; ++i)
            out.push_back (at (i).int32 (lo, hi));
        return out;
    }

    std::vector<std::string> Node::strings (std::size_t minCount, std::size_t maxCount) const
    {
        std::vector<std::string> out;
        const std::size_t n = size();
        if (value_ == nullptr || value_->type != Value::Type::array)
            return out;
        if (n < minCount || n > maxCount)
        {
            fail ("it must hold " + std::to_string (minCount) + " to " + std::to_string (maxCount) + " strings, not " + std::to_string (n));
            return out;
        }
        for (std::size_t i = 0; i < n; ++i)
            out.push_back (at (i).string (1, 256));
        return out;
    }

    std::int32_t Node::int32Or (std::string_view key, std::int32_t fallback, std::int32_t lo, std::int32_t hi) const
    {
        const Node n = optional (key);
        return n.exists() ? n.int32 (lo, hi) : fallback;
    }
}
