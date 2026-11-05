// The two manifest parsers (P4): docs/plugin/manifest.md, strictly. core's gen::json reads the text (integers only,
// no duplicate keys, valid UTF-8); a cursor here walks the tree with each value's JSON path, checks every key, type,
// range and reference, and keeps the first problem, which is the error:
//     "kit.json: instruments[3].zones[0].layers[1].rr[0]: file 412 doesn't exist (412 files)".
// A text the JSON reader refuses (a float, a duplicate key) has no tree yet: its path is found from the line and
// column the reader stopped at.

#include "kit/manifest.h"

#include "kit/channel_map.h"
#include "kit/file_sample_source.h"

#include "gen/json.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace wubansia::engine
{
    namespace
    {
        using gen::json::Value;
        using Type = Value::Type;

        constexpr std::size_t kUnbounded = std::numeric_limits<std::size_t>::max();

        // ------------------------------------------------------------------ errors, paths, quoting

        // The first problem: "<kind>: <path>: <message>" (no path for the root).
        class Errors
        {
        public:
            explicit Errors (std::string_view kind) : kind_ (kind) {}

            void add (std::string_view path, std::string_view message)
            {
                if (! first_.empty())
                    return;
                first_ = kind_;
                if (! path.empty())
                    (first_ += ": ") += path;
                (first_ += ": ") += message;
            }

            bool any() const noexcept { return ! first_.empty(); }
            const std::string& first() const noexcept { return first_; }

        private:
            std::string kind_;
            std::string first_;
        };

        std::string member (std::string_view path, std::string_view key)
        {
            return path.empty() ? std::string (key) : std::string (path) + "." + std::string (key);
        }

        std::string element (std::string_view path, std::size_t i) { return std::string (path) + "[" + std::to_string (i) + "]"; }

        // A string for a message: in quotes, control characters escaped, a long one cut after 60 characters, so a
        // message stays one readable line.
        std::string quote (std::string_view s)
        {
            std::string out = "\"";
            std::size_t n = 0;
            for (const char ch : s)
            {
                const auto c = static_cast<unsigned char> (ch);
                if (n >= 60 && (c & 0xC0u) != 0x80u)
                {
                    out += "...";
                    break;
                }
                if (c < 0x20 || c == 0x7F)
                {
                    static constexpr char hex[] = "0123456789abcdef";
                    out += "\\u00";
                    out += hex[c >> 4];
                    out += hex[c & 15];
                }
                else if (c == '"' || c == '\\')
                    (out += '\\') += ch;
                else
                    out += ch;
                if ((c & 0xC0u) != 0x80u)
                    ++n;
            }
            return out + "\"";
        }

        template <std::size_t N>
        std::string list (const std::array<std::string_view, N>& ids)
        {
            std::string out;
            for (const auto id : ids)
                out += (out.empty() ? "" : ", ") + std::string (id);
            return out;
        }

        std::string plural (std::size_t n, std::string_view noun) { return std::to_string (n) + " " + std::string (noun); }

        // ------------------------------------------------------------------ the JSON path of a syntax error

        // The path of the value at (line, column) in text, where the JSON reader stopped. Everything before that
        // point is well-formed as far as it goes, so a scan that tracks containers, keys and indices finds it.
        // Lines and columns count as the reader counts them: from 1, columns in characters.
        std::string pathAt (std::string_view text, std::uint32_t line, std::uint32_t column)
        {
            struct Frame
            {
                bool object = false;
                bool hasKey = false;
                std::string key;
                std::size_t index = 0;
            };
            std::vector<Frame> stack;
            bool expectKey = false;
            std::uint32_t l = 1, c = 1;
            std::size_t i = 0;
            const auto reached = [&] { return l > line || (l == line && c >= column); };
            const auto step = [&] {
                const auto ch = static_cast<unsigned char> (text[i]);
                ++i;
                if (ch == '\n')
                {
                    ++l;
                    c = 1;
                }
                else if ((ch & 0xC0u) != 0x80u)
                    ++c;
            };
            while (i < text.size() && ! reached())
            {
                const char ch = text[i];
                if (ch == '"')
                {
                    std::string s;
                    step();
                    bool closed = false;
                    while (i < text.size() && ! reached())
                    {
                        if (text[i] == '\\')
                        {
                            s += text[i];
                            step();
                            if (i < text.size() && ! reached())
                            {
                                s += text[i];
                                step();
                            }
                            continue;
                        }
                        if (text[i] == '"')
                        {
                            step();
                            closed = true;
                            break;
                        }
                        s += text[i];
                        step();
                    }
                    if (! closed)
                        break; // the reader stopped inside this string
                    if (expectKey && ! stack.empty() && stack.back().object)
                    {
                        stack.back().key = std::move (s);
                        stack.back().hasKey = true;
                        expectKey = false;
                    }
                    continue;
                }
                if (ch == '{')
                {
                    stack.push_back ({ true, false, {}, 0 });
                    expectKey = true;
                }
                else if (ch == '[')
                {
                    stack.push_back ({});
                    expectKey = false;
                }
                else if (ch == '}' || ch == ']')
                {
                    if (! stack.empty())
                        stack.pop_back();
                    expectKey = false;
                }
                else if (ch == ',' && ! stack.empty())
                {
                    auto& top = stack.back();
                    if (top.object)
                    {
                        top.hasKey = false;
                        expectKey = true;
                    }
                    else
                        ++top.index;
                }
                step();
            }
            std::string path;
            for (const auto& f : stack)
            {
                if (! f.object)
                    path += "[" + std::to_string (f.index) + "]";
                else if (! f.hasKey)
                    break;
                else
                    path = member (path, f.key);
            }
            return path;
        }

        // The reader's "line L, column C: message", with the path put in front.
        std::string syntaxError (std::string_view kind, std::string_view text, const std::string& error)
        {
            std::uint32_t line = 0, column = 0;
            const std::string_view e (error);
            bool parsed = false;
            if (e.starts_with ("line "))
            {
                const char* const end = e.data() + e.size();
                const auto l = std::from_chars (e.data() + 5, end, line);
                if (l.ec == std::errc {} && std::string_view (l.ptr, static_cast<std::size_t> (end - l.ptr)).starts_with (", column "))
                    parsed = std::from_chars (l.ptr + 9, end, column).ec == std::errc {};
            }
            const auto path = parsed ? pathAt (text, line, column) : std::string();
            return std::string (kind) + ": " + (path.empty() ? "" : path + ": ") + error;
        }

        // ------------------------------------------------------------------ string rules

        std::size_t characters (std::string_view s)
        {
            std::size_t n = 0;
            for (const char c : s)
                n += (static_cast<unsigned char> (c) & 0xC0u) != 0x80u ? 1u : 0u;
            return n;
        }

        // C0 controls, DEL, and C1 controls (U+0080-U+009F, which UTF-8 writes as C2 80 to C2 9F).
        bool hasControl (std::string_view s)
        {
            for (std::size_t i = 0; i < s.size(); ++i)
            {
                const auto c = static_cast<unsigned char> (s[i]);
                if (c < 0x20 || c == 0x7F)
                    return true;
                if (c == 0xC2 && i + 1 < s.size() && static_cast<unsigned char> (s[i + 1]) <= 0x9F)
                    return true;
            }
            return false;
        }

        bool isId (std::string_view s)
        {
            return ! s.empty() && s.size() <= 32
                && std::all_of (s.begin(), s.end(), [] (char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'; });
        }

        bool isKitId (std::string_view s)
        {
            return s.size() == 5 && std::all_of (s.begin(), s.begin() + 3, [] (char c) { return c >= 'A' && c <= 'Z'; })
                && std::all_of (s.begin() + 3, s.end(), [] (char c) { return c >= '0' && c <= '9'; });
        }

        bool isMusicalKey (std::string_view s)
        {
            if (s.empty() || s[0] < 'A' || s[0] > 'G')
                return false;
            auto rest = s.substr (1);
            if (! rest.empty() && (rest[0] == '#' || rest[0] == 'b'))
                rest = rest.substr (1);
            return rest == " minor" || rest == " major";
        }

        bool isCamelot (std::string_view s)
        {
            if (s.size() < 2 || s.size() > 3 || (s.back() != 'A' && s.back() != 'B'))
                return false;
            if (s[0] < '1' || s[0] > '9')
                return false;
            return s.size() == 2 || (s[0] == '1' && s[1] >= '0' && s[1] <= '2');
        }

        bool isSha256 (std::string_view s)
        {
            return s.size() == 64 && std::all_of (s.begin(), s.end(), [] (char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
        }

        // ------------------------------------------------------------------ a cursor over the tree

        std::string_view expectedName (Type type)
        {
            switch (type)
            {
                case Type::integer: return "an integer";
                case Type::string: return "a string";
                case Type::boolean: return "true or false";
                case Type::array: return "an array";
                case Type::object: return "an object";
                case Type::null: return "null";
            }
            return "a value";
        }

        // A value and its JSON path. A missing value (null) was reported where it went missing: reads of it give
        // defaults quietly. Every check records its problem in the shared Errors, which keeps the first.
        class Node
        {
        public:
            Node (const Value* value, std::string path, Errors& errors) : value_ (value), path_ (std::move (path)), errors_ (&errors) {}

            const Value* value() const noexcept { return value_; }
            const std::string& path() const noexcept { return path_; }
            bool present() const noexcept { return value_ != nullptr; }
            bool is (Type type) const noexcept { return value_ != nullptr && value_->type == type; }
            Errors& errors() const noexcept { return *errors_; }
            void fail (std::string_view message) const { errors_->add (path_, message); }

            bool expect (Type type) const
            {
                if (value_ == nullptr)
                    return false;
                if (value_->type == type)
                    return true;
                fail (std::string (expectedName (type)) + " was expected, not " + std::string (gen::json::typeName (value_->type)));
                return false;
            }

            // The elements of an array of lo to hi of them (noun: what they are, for the message).
            std::vector<Node> items (std::size_t lo, std::size_t hi, std::string_view noun) const
            {
                std::vector<Node> out;
                if (! expect (Type::array))
                    return out;
                const auto n = value_->items.size();
                if (n < lo || n > hi)
                {
                    const auto allowed = lo == hi        ? "it must hold " + std::to_string (lo)
                                       : hi == kUnbounded ? "it must hold at least " + std::to_string (lo)
                                                          : "it may hold " + std::to_string (lo) + " to " + std::to_string (hi);
                    fail ("it holds " + plural (n, noun) + ": " + allowed);
                    return out;
                }
                out.reserve (n);
                for (std::size_t i = 0; i < n; ++i)
                    out.emplace_back (&value_->items[i], element (path_, i), *errors_);
                return out;
            }

            std::int64_t integer (std::int64_t lo, std::int64_t hi) const
            {
                if (! expect (Type::integer))
                    return lo;
                const auto v = value_->integer;
                if (v < lo || v > hi)
                {
                    fail (std::to_string (v) + " is out of range (" + std::to_string (lo) + " to " + std::to_string (hi) + ")");
                    return lo;
                }
                return v;
            }

            std::int32_t int32 (std::int32_t lo, std::int32_t hi) const { return static_cast<std::int32_t> (integer (lo, hi)); }

            // An integer of any value, for checks with their own message; 0 when it isn't one.
            std::int64_t raw() const { return expect (Type::integer) ? value_->integer : 0; }

            bool flag() const { return expect (Type::boolean) && value_->boolean; }

            std::string string() const { return expect (Type::string) ? value_->string : std::string(); }

            // [a-z0-9_], 1-32 characters (or "" with allowEmpty).
            std::string id (bool allowEmpty = false) const
            {
                auto s = string();
                if (is (Type::string) && ! isId (s) && ! (allowEmpty && s.empty()))
                    fail (quote (s) + " isn't an id: [a-z0-9_], 1 to 32 characters" + (allowEmpty ? ", or \"\"" : ""));
                return s;
            }

            // 1-64 characters, no control characters (or "" with allowEmpty: a label).
            std::string name (bool allowEmpty = false) const
            {
                auto s = string();
                if (! is (Type::string) || (allowEmpty && s.empty()))
                    return s;
                if (const auto n = characters (s); n < 1 || n > 64)
                    fail (quote (s) + " is " + plural (n, "characters") + ": a name is 1 to 64");
                else if (hasControl (s))
                    fail (quote (s) + " holds a control character");
                return s;
            }

            // One of ids, as its index (0 after an error).
            template <std::size_t N>
            int choice (const std::array<std::string_view, N>& ids) const
            {
                const auto s = string();
                if (! is (Type::string))
                    return 0;
                const int i = idIndex (ids, s);
                if (i < 0)
                {
                    fail (quote (s) + " isn't one of " + list (ids));
                    return 0;
                }
                return i;
            }

            // One of ids, or "" (-1).
            template <std::size_t N>
            int choiceOrNone (const std::array<std::string_view, N>& ids) const
            {
                const auto s = string();
                if (! is (Type::string) || s.empty())
                    return -1;
                const int i = idIndex (ids, s);
                if (i < 0)
                    fail (quote (s) + " isn't one of " + list (ids) + ", \"\"");
                return i;
            }

            // An array of lo to hi integers, each in [vlo, vhi].
            std::vector<std::int32_t> ints (std::size_t lo, std::size_t hi, std::int32_t vlo, std::int32_t vhi, std::string_view noun = "values") const
            {
                std::vector<std::int32_t> out;
                for (const auto& item : items (lo, hi, noun))
                    out.push_back (item.int32 (vlo, vhi));
                return out;
            }

        private:
            const Value* value_;
            std::string path_;
            Errors* errors_;
        };

        // An object with exactly the given keys, plus any of the optional ones: an unknown key, then a missing
        // one, is an error at the object's path. The order of the keys doesn't matter.
        class Obj
        {
        public:
            Obj (const Node& node, std::span<const std::string_view> keys, std::span<const std::string_view> optional = {}) : node_ (node)
            {
                ok_ = node.expect (Type::object);
                if (! ok_)
                    return;
                const auto known = [&] (std::string_view key) {
                    return std::find (keys.begin(), keys.end(), key) != keys.end() || std::find (optional.begin(), optional.end(), key) != optional.end();
                };
                for (const auto& m : node.value()->members)
                    if (! known (m.key))
                    {
                        std::string all;
                        for (const auto k : keys)
                            all += (all.empty() ? "" : ", ") + std::string (k);
                        for (const auto k : optional)
                            all += ", " + std::string (k) + " (optional)";
                        node.fail ("unknown key " + quote (m.key) + " (its keys: " + all + ")");
                        ok_ = false;
                        return;
                    }
                for (const auto k : keys)
                    if (node.value()->find (k) == nullptr)
                    {
                        node.fail ("missing " + quote (k));
                        ok_ = false;
                        return;
                    }
            }

            Obj (const Node& node, std::initializer_list<std::string_view> keys)
                : Obj (node, std::span<const std::string_view> (keys.begin(), keys.size()))
            {
            }

            bool ok() const noexcept { return ok_; }
            const std::string& path() const noexcept { return node_.path(); }
            void fail (std::string_view message) const { node_.fail (message); }

            // The member (missing, quietly, if this isn't a good object or lacks it: that was reported).
            Node operator[] (std::string_view key) const
            {
                const Value* v = ok_ ? node_.value()->find (key) : nullptr;
                return Node (v, member (node_.path(), key), node_.errors());
            }

        private:
            Node node_;
            bool ok_ = false;
        };

        constexpr std::array<std::string_view, 2> kSideIds { "a", "b" };

        // ------------------------------------------------------------------ kit.json

        class KitReader
        {
        public:
            KitReader (Errors& sink, KitManifest& out) : errors (sink), m (out) {}

            void run (const Value& root)
            {
                if (root.type != Type::object)
                {
                    errors.add ("", "the manifest must be an object, not " + std::string (gen::json::typeName (root.type)));
                    return;
                }
                const Obj top (Node (&root, "", errors), { "kit", "files", "instruments", "keymap", "extras", "phrases" });
                if (! top.ok())
                    return;
                readKit (top["kit"]);
                if (! errors.any())
                    readFiles (top["files"]);
                if (! errors.any())
                    readInstruments (top["instruments"]);
                if (! errors.any())
                    readKeymap (top["keymap"]);
                if (! errors.any())
                    readExtras (top["extras"]);
                if (! errors.any())
                    readPhrases (top["phrases"]);
                if (! errors.any())
                    checkFilesPlayed();
            }

        private:
            Errors& errors;
            KitManifest& m;
            std::map<std::string, std::int32_t, std::less<>> instrumentIds;

            void readKit (const Node& n)
            {
                const Obj o (n, { "id", "title", "direction", "bpm", "key", "camelot", "swing_pm", "content_version", "lanes", "ab" });
                if (! o.ok())
                    return;
                auto& k = m.kit;
                const auto id = o["id"];
                k.id = id.string();
                if (id.is (Type::string) && ! isKitId (k.id))
                    id.fail (quote (k.id) + " isn't a kit id: [A-Z]{3}[0-9]{2}, like \"FUT01\"");
                k.title = o["title"].name();
                k.direction = o["direction"].name();
                k.bpm = o["bpm"].int32 (128, 140);
                const auto key = o["key"];
                k.key = key.string();
                if (key.is (Type::string) && ! isMusicalKey (k.key))
                    key.fail (quote (k.key) + " isn't a key: <note> minor or <note> major, the note A-G then # or b or nothing");
                const auto camelot = o["camelot"];
                k.camelot = camelot.string();
                if (camelot.is (Type::string) && ! isCamelot (k.camelot))
                    camelot.fail (quote (k.camelot) + " isn't a Camelot code: 1-12, then A or B");
                k.swingPm = o["swing_pm"].int32 (500, 750);
                k.contentVersion = o["content_version"].int32 (1, 999);
                if (errors.any())
                    return;

                const Obj lanes (o["lanes"], std::span<const std::string_view> (kLaneIds));
                if (! lanes.ok())
                    return;
                for (std::size_t i = 0; i < kLaneIds.size(); ++i)
                    k.lanes[i] = lanes[kLaneIds[i]].name (true);

                const Obj ab (o["ab"], std::span<const std::string_view> (kAbSwitchIds));
                if (! ab.ok())
                    return;
                for (std::size_t i = 0; i < kAbSwitchIds.size() && ! errors.any(); ++i)
                {
                    const Obj pair (ab[kAbSwitchIds[i]], { "a", "b" });
                    if (! pair.ok())
                        return;
                    k.ab[i].a = pair["a"].name (true);
                    k.ab[i].b = pair["b"].name (true);
                    if (! errors.any() && k.ab[i].a.empty() != k.ab[i].b.empty())
                        pair.fail ("both labels are names, or both are \"\" (a is " + quote (k.ab[i].a) + ", b is " + quote (k.ab[i].b) + ")");
                }
            }

            void readFiles (const Node& n)
            {
                std::map<std::string, std::size_t, std::less<>> paths;
                for (const auto& item : n.items (0, kUnbounded, "files"))
                {
                    const Obj o (item, { "path", "frames", "channels", "sha256", "loop_start", "loop_end", "preroll_frames" });
                    if (! o.ok())
                        return;
                    FileEntry f;
                    const auto path = o["path"];
                    f.path = path.string();
                    if (path.is (Type::string))
                    {
                        if (const auto problem = contentPathProblem (f.path); ! problem.empty())
                            path.fail (quote (f.path) + " " + problem);
                        else if (const auto [at, added] = paths.emplace (f.path, m.files.size()); ! added)
                            path.fail (quote (f.path) + " is files[" + std::to_string (at->second) + "]'s path too");
                    }
                    f.frames = o["frames"].integer (1, 28'800'000);
                    f.channels = o["channels"].int32 (1, 2);
                    const auto sha = o["sha256"];
                    f.sha256 = sha.string();
                    if (sha.is (Type::string) && ! isSha256 (f.sha256))
                        sha.fail (quote (f.sha256) + " isn't 64 lowercase hex digits");
                    if (errors.any())
                        return;
                    const auto loopStart = o["loop_start"];
                    f.loopStart = loopStart.integer (0, f.frames - 1);
                    const auto loopEnd = o["loop_end"];
                    f.loopEnd = loopEnd.raw();
                    if (! errors.any() && f.loopEnd != 0 && (f.loopEnd <= f.loopStart || f.loopEnd > f.frames))
                        loopEnd.fail (std::to_string (f.loopEnd) + " is out of range (0, or " + std::to_string (f.loopStart + 1) + " to " + std::to_string (f.frames) + ")");
                    else if (! errors.any() && f.loopEnd == 0 && f.loopStart != 0)
                        loopStart.fail (std::to_string (f.loopStart) + " with loop_end 0: a file without a loop has loop_start 0");
                    f.prerollFrames = o["preroll_frames"].int32 (0, 48'000);
                    if (errors.any())
                        return;
                    m.files.push_back (std::move (f));
                }
            }

            // A file index: 0 to files - 1 (or -1 with allowNone).
            std::int32_t fileIndex (const Node& n, bool allowNone = false) const
            {
                const auto v = n.raw();
                if (errors.any() || (allowNone && v == -1))
                    return -1;
                if (v < 0 || v >= static_cast<std::int64_t> (m.files.size()))
                {
                    n.fail ("file " + std::to_string (v) + " doesn't exist (" + plural (m.files.size(), "files") + ")");
                    return -1;
                }
                return static_cast<std::int32_t> (v);
            }

            void readInstruments (const Node& n)
            {
                const auto list = n.items (0, kUnbounded, "instruments");
                std::vector<std::string> layered (list.size());
                for (std::size_t i = 0; i < list.size(); ++i)
                {
                    InstrumentEntry e;
                    readInstrument (list[i], e, layered[i]);
                    if (errors.any())
                        return;
                    m.instruments.push_back (std::move (e));
                }

                for (std::size_t i = 0; i < list.size(); ++i)
                    instrumentIds.emplace (m.instruments[i].id, static_cast<std::int32_t> (i));
                for (std::size_t i = 0; i < list.size() && ! errors.any(); ++i)
                {
                    if (layered[i].empty())
                        continue;
                    const auto at = member (list[i].path(), "layered_with");
                    const auto found = instrumentIds.find (layered[i]);
                    if (found == instrumentIds.end())
                        errors.add (at, "no instrument " + quote (layered[i]));
                    else if (found->second == static_cast<std::int32_t> (i))
                        errors.add (at, quote (layered[i]) + " is the instrument itself");
                    else
                        m.instruments[i].layeredWith = found->second;
                }
                for (std::size_t i = 0; i < list.size() && ! errors.any(); ++i)
                {
                    const auto target = m.instruments[i].layeredWith;
                    if (target < 0)
                        continue;
                    const auto& t = m.instruments[static_cast<std::size_t> (target)];
                    if (t.layeredWith >= 0)
                        errors.add (member (list[i].path(), "layered_with"),
                            quote (t.id) + " is itself layered (with " + quote (m.instruments[static_cast<std::size_t> (t.layeredWith)].id) + ")");
                }
            }

            void readInstrument (const Node& n, InstrumentEntry& e, std::string& layeredWith)
            {
                const Obj o (n, { "id", "name", "lane", "mix_channel", "mode", "align", "attack_us", "release_us", "gain_mdb", "start_frames",
                                    "velocity_law", "interp", "mono", "glide", "restrike_continues", "choke", "duck_key", "pan_cycle_pm",
                                    "rr_block_16ths", "layered_with", "zones" });
                if (! o.ok())
                    return;
                const auto id = o["id"];
                e.id = id.id();
                for (std::size_t j = 0; j < m.instruments.size() && ! errors.any(); ++j)
                    if (m.instruments[j].id == e.id)
                        id.fail (quote (e.id) + " is instruments[" + std::to_string (j) + "]'s id too");
                e.name = o["name"].name();
                e.lane = static_cast<Lane> (o["lane"].choice (kLaneIds));
                e.mixChannel = o["mix_channel"].id();
                e.mode = static_cast<SampleMode> (o["mode"].choice (kSampleModeIds));
                e.align = static_cast<Align> (o["align"].choice (kAlignIds));
                e.attackUs = o["attack_us"].int32 (0, 10'000'000);
                e.releaseUs = o["release_us"].int32 (0, 10'000'000);
                e.gainMdb = o["gain_mdb"].int32 (-60'000, 24'000);
                const auto startFrames = o["start_frames"];
                e.startFrames = startFrames.raw();
                e.velocityLaw = static_cast<VelocityLaw> (o["velocity_law"].choice (kVelocityLawIds));
                e.interp = static_cast<Interp> (o["interp"].choice (kInterpIds));
                e.mono = o["mono"].flag();
                e.restrikeContinues = o["restrike_continues"].flag();
                e.duckKey = o["duck_key"].flag();
                e.panCyclePm = o["pan_cycle_pm"].ints (0, 16, -1000, 1000);
                e.rrBlock16ths = o["rr_block_16ths"].int32 (1, 4096);
                layeredWith = o["layered_with"].id (true);
                if (errors.any())
                    return;

                const Obj glide (o["glide"], { "curve", "us", "dip_us" });
                if (! glide.ok())
                    return;
                e.glide.curve = static_cast<GlideCurve> (glide["curve"].choice (kGlideCurveIds));
                e.glide.us = glide["us"].int32 (0, 2'000'000);
                e.glide.dipUs = glide["dip_us"].int32 (0, 2'000'000);
                if (errors.any())
                    return;
                if (e.glide.curve == GlideCurve::none && e.glide.us != 0)
                    glide["us"].fail (std::to_string (e.glide.us) + " with curve none: no glide has us 0");
                else if (e.glide.curve != GlideCurve::raisedCosine && e.glide.dipUs != 0)
                    glide["dip_us"].fail (std::to_string (e.glide.dipUs) + " with curve " + std::string (kGlideCurveIds[static_cast<std::size_t> (e.glide.curve)])
                                          + ": only raised_cosine has dips");
                else if (e.glide.curve != GlideCurve::none && ! e.mono)
                    glide.fail ("a glide needs mono: true");
                if (errors.any())
                    return;

                const Obj choke (o["choke"], { "group", "role", "fade_us", "curve" });
                if (! choke.ok())
                    return;
                e.choke.group = choke["group"].id (true);
                e.choke.role = static_cast<ChokeRole> (choke["role"].choice (kChokeRoleIds));
                e.choke.fadeUs = choke["fade_us"].int32 (0, 1'000'000);
                e.choke.curve = static_cast<ChokeCurve> (choke["curve"].choice (kChokeCurveIds));
                if (errors.any())
                    return;
                if (e.choke.group.empty() && e.choke.role != ChokeRole::none)
                    choke.fail ("role " + std::string (kChokeRoleIds[static_cast<std::size_t> (e.choke.role)]) + " with no group");
                else if (! e.choke.group.empty() && e.choke.role == ChokeRole::none)
                    choke.fail ("group " + quote (e.choke.group) + " with role none: an instrument in a group chokes, is choked, or both");
                if (errors.any())
                    return;

                readZones (o["zones"], e);
                if (errors.any())
                    return;

                // What the instrument's files must share: a loop each for sustain_loop, one pre-roll, room for
                // start_frames.
                std::int64_t shortest = std::numeric_limits<std::int64_t>::max();
                std::int32_t first = -1;
                for (const auto& z : e.zones)
                    for (const auto& l : z.layers)
                        for (const auto file : l.rr)
                        {
                            const auto& f = m.files[static_cast<std::size_t> (file)];
                            shortest = std::min (shortest, f.frames);
                            if (e.mode == SampleMode::sustainLoop && f.loopEnd == 0)
                                o.fail ("a sustain_loop instrument, but file " + std::to_string (file) + " (" + quote (f.path) + ") has no loop");
                            if (first < 0)
                                first = file;
                            else if (const auto& f0 = m.files[static_cast<std::size_t> (first)]; f.prerollFrames != f0.prerollFrames)
                                o.fail ("its files' preroll_frames differ: file " + std::to_string (first) + " has " + std::to_string (f0.prerollFrames)
                                        + ", file " + std::to_string (file) + " has " + std::to_string (f.prerollFrames));
                            if (errors.any())
                                return;
                        }
                if (e.startFrames < 0 || e.startFrames > shortest - 1)
                    startFrames.fail (std::to_string (e.startFrames) + " is out of range (0 to " + std::to_string (shortest - 1)
                                      + ", its shortest file's frames less 1)");
            }

            void readZones (const Node& n, InstrumentEntry& e)
            {
                for (const auto& item : n.items (1, 32, "zones"))
                {
                    const Obj o (item, { "root", "lo", "hi", "layers" });
                    if (! o.ok())
                        return;
                    ZoneEntry z;
                    z.root = o["root"].int32 (0, 127);
                    z.lo = o["lo"].int32 (0, 127);
                    if (errors.any())
                        return;
                    z.hi = o["hi"].int32 (z.lo, 127);
                    if (errors.any())
                        return;
                    if (! e.zones.empty())
                    {
                        const auto& prev = e.zones.back();
                        const auto previous = std::to_string (e.zones.size() - 1);
                        if (z.root < prev.root)
                            o.fail ("root " + std::to_string (z.root) + " comes after zones[" + previous + "]'s root " + std::to_string (prev.root)
                                    + ": zones go by root, ascending");
                        else if (z.lo <= prev.hi)
                            o.fail ("keys " + std::to_string (z.lo) + " to " + std::to_string (z.hi) + " overlap zones[" + previous + "]'s "
                                    + std::to_string (prev.lo) + " to " + std::to_string (prev.hi) + ": zone ranges are disjoint and ascending");
                        if (errors.any())
                            return;
                    }

                    for (const auto& litem : o["layers"].items (1, 8, "layers"))
                    {
                        const Obj lo (litem, { "vel", "rr" });
                        if (! lo.ok())
                            return;
                        LayerEntry l;
                        const auto vel = lo["vel"];
                        l.vel = vel.int32 (1, 127);
                        if (! errors.any() && ! z.layers.empty() && l.vel <= z.layers.back().vel)
                            vel.fail (std::to_string (l.vel) + " after " + std::to_string (z.layers.back().vel) + ": layer velocities go strictly ascending");
                        for (const auto& r : lo["rr"].items (1, 8, "files"))
                            l.rr.push_back (fileIndex (r));
                        if (errors.any())
                            return;
                        z.layers.push_back (std::move (l));
                    }
                    if (errors.any())
                        return;

                    // One zone, one channel count.
                    const auto first = z.layers.front().rr.front();
                    const auto channels = m.files[static_cast<std::size_t> (first)].channels;
                    for (const auto& l : z.layers)
                        for (const auto file : l.rr)
                            if (const auto c = m.files[static_cast<std::size_t> (file)].channels; c != channels)
                            {
                                o.fail ("its files don't all have the same channels: file " + std::to_string (first) + " has " + std::to_string (channels)
                                        + ", file " + std::to_string (file) + " has " + std::to_string (c));
                                return;
                            }
                    e.zones.push_back (std::move (z));
                }
            }

            // An instrument id as its index; "" is -1 with allowNone, else an error.
            std::int32_t instrumentRef (const Node& n, bool allowNone) const
            {
                const auto s = n.string();
                if (errors.any() || (allowNone && s.empty()))
                    return -1;
                const auto found = instrumentIds.find (s);
                if (found == instrumentIds.end())
                {
                    n.fail ("no instrument " + quote (s));
                    return -1;
                }
                return found->second;
            }

            // ab names a switch exactly when there's a B side, and only a switch the kit has.
            void checkSwitch (const Node& ab, std::int32_t sw, bool hasB, const std::string& b) const
            {
                if (hasB && sw < 0)
                    ab.fail ("\"\" with " + b + ": ab names the switch exactly when there is a B side");
                else if (! hasB && sw >= 0)
                    ab.fail (quote (kAbSwitchIds[static_cast<std::size_t> (sw)]) + " with no B side: ab is \"\" exactly when there is none");
                else if (sw >= 0 && ! m.kit.ab[static_cast<std::size_t> (sw)].exists())
                {
                    const auto id = std::string (kAbSwitchIds[static_cast<std::size_t> (sw)]);
                    ab.fail (quote (id) + ", but the kit has no " + id + " switch (kit.ab." + id + " is empty)");
                }
            }

            void readKeymap (const Node& n)
            {
                for (const auto& item : n.items (0, kUnbounded, "entries"))
                {
                    const Obj o (item, { "ch", "keys", "a", "b", "ab" });
                    if (! o.ok())
                        return;
                    KeymapEntry k;
                    const auto ch = o["ch"];
                    k.ch = ch.int32 (1, 16);
                    if (! errors.any() && (k.ch == 3 || k.ch == 9))
                        ch.fail ("channel " + std::to_string (k.ch) + (k.ch == 3 ? " (the chord voicing)" : " (reserved)") + " has no entries: Wubansia ignores it");
                    const auto keysNode = o["keys"];
                    const auto keys = keysNode.ints (2, 2, 0, 127, "keys");
                    if (errors.any())
                        return;
                    k.lo = keys[0];
                    k.hi = keys[1];
                    if (k.lo > k.hi)
                        keysNode.fail ("[" + std::to_string (k.lo) + ", " + std::to_string (k.hi) + "]: lo is above hi");
                    else if (k.ch == 10 && (k.lo < 35 || k.hi > 59))
                        keysNode.fail ("channel 10's keys are 35 to 59, not " + std::to_string (k.lo) + " to " + std::to_string (k.hi));
                    for (std::size_t j = 0; j < m.keymap.size() && ! errors.any(); ++j)
                    {
                        const auto& other = m.keymap[j];
                        if (other.ch == k.ch && other.lo <= k.hi && k.lo <= other.hi)
                            keysNode.fail (std::to_string (k.lo) + " to " + std::to_string (k.hi) + " overlaps keymap[" + std::to_string (j) + "]'s "
                                           + std::to_string (other.lo) + " to " + std::to_string (other.hi) + " on channel " + std::to_string (k.ch));
                    }
                    k.a = instrumentRef (o["a"], false);
                    k.b = instrumentRef (o["b"], true);
                    const auto ab = o["ab"];
                    k.ab = ab.choiceOrNone (kAbSwitchIds);
                    if (errors.any())
                        return;
                    checkSwitch (ab, k.ab, k.b >= 0, "b " + quote (o["b"].string()));
                    if (errors.any())
                        return;
                    m.keymap.push_back (k);
                }

                // Every instrument plays from the keymap, or under another.
                std::vector<bool> played (m.instruments.size(), false);
                for (const auto& k : m.keymap)
                {
                    played[static_cast<std::size_t> (k.a)] = true;
                    if (k.b >= 0)
                        played[static_cast<std::size_t> (k.b)] = true;
                }
                for (const auto& in : m.instruments)
                    if (in.layeredWith >= 0)
                        played[static_cast<std::size_t> (in.layeredWith)] = true;
                for (std::size_t i = 0; i < played.size(); ++i)
                    if (! played[i])
                    {
                        errors.add (element ("instruments", i), quote (m.instruments[i].id) + " isn't in the keymap, and no instrument is layered with it");
                        return;
                    }
            }

            void readExtras (const Node& n)
            {
                for (const auto& item : n.items (0, kUnbounded, "extras"))
                {
                    const Obj o (item, { "ch", "name", "lane" });
                    if (! o.ok())
                        return;
                    ExtraEntry e;
                    const auto ch = o["ch"];
                    e.ch = ch.int32 (kFirstExtraChannel, kLastExtraChannel);
                    if (! errors.any() && ! m.extras.empty() && e.ch <= m.extras.back().ch)
                        ch.fail (std::to_string (e.ch) + " after " + std::to_string (m.extras.back().ch) + ": extras go by channel, ascending, one per channel");
                    e.name = o["name"].name();
                    e.lane = static_cast<Lane> (o["lane"].choice (kLaneIds));
                    if (errors.any())
                        return;
                    m.extras.push_back (std::move (e));
                }

                // Keymap channels from 11 up are extras', and play on their lanes.
                for (std::size_t i = 0; i < m.keymap.size(); ++i)
                {
                    const auto& k = m.keymap[i];
                    if (k.ch < kFirstExtraChannel)
                        continue;
                    const auto extra = std::find_if (m.extras.begin(), m.extras.end(), [&] (const ExtraEntry& e) { return e.ch == k.ch; });
                    if (extra == m.extras.end())
                    {
                        errors.add (member (element ("keymap", i), "ch"), "channel " + std::to_string (k.ch) + " has no extra (extras names its track and lane)");
                        return;
                    }
                    for (const auto& [side, target] : { std::pair<std::string_view, std::int32_t> { "a", k.a }, std::pair<std::string_view, std::int32_t> { "b", k.b } })
                    {
                        if (target < 0)
                            continue;
                        const auto& in = m.instruments[static_cast<std::size_t> (target)];
                        if (in.lane != extra->lane)
                        {
                            errors.add (member (element ("keymap", i), side), quote (in.id) + " plays on " + std::string (laneId (in.lane)) + ", but channel "
                                + std::to_string (k.ch) + "'s extra (" + quote (extra->name) + ") is on " + std::string (laneId (extra->lane)));
                            return;
                        }
                    }
                }
            }

            void readPhrases (const Node& n)
            {
                const auto list = n.items (0, kUnbounded, "phrases");
                for (std::size_t i = 0; i < list.size(); ++i)
                {
                    PhraseEntry p;
                    readPhrase (list[i], p);
                    if (errors.any())
                        return;
                    // The matcher cuts a channel's notes into one cycle length.
                    for (std::size_t j = 0; j < m.phrases.size(); ++j)
                        if (m.phrases[j].ch == p.ch && m.phrases[j].cycle16ths != p.cycle16ths)
                        {
                            errors.add (member (list[i].path(), "cycle_16ths"),
                                std::to_string (p.cycle16ths) + ", but phrases[" + std::to_string (j) + "], also on channel " + std::to_string (p.ch) + ", has "
                                    + std::to_string (m.phrases[j].cycle16ths) + ": phrases that share a channel share one cycle_16ths");
                            return;
                        }
                    m.phrases.push_back (std::move (p));
                }
            }

            // A ride: [] or cycle_16ths x 4 controller values (one per kRideTicks), each 0-127.
            std::vector<std::uint8_t> readRide (const Node& n, std::int32_t cycle16ths) const
            {
                std::vector<std::uint8_t> out;
                if (! n.expect (Type::array))
                    return out;
                const auto points = static_cast<std::size_t> (cycle16ths) * (kMilliticksPerSixteenth / kMilliticksPerTick) / kRideTicks;
                const auto count = n.value()->items.size();
                if (count != 0 && count != points)
                {
                    n.fail ("it holds " + plural (count, "values") + ": a ride holds 0, or one per " + std::to_string (kRideTicks) + " ticks over the "
                            + std::to_string (cycle16ths) + "-16th cycle, " + std::to_string (points));
                    return out;
                }
                for (const auto& v : n.items (0, kUnbounded, "values"))
                    out.push_back (static_cast<std::uint8_t> (v.int32 (0, 127)));
                return out;
            }

            void readPhrase (const Node& n, PhraseEntry& p)
            {
                const Obj o (n, { "lane", "ch", "ab", "mix_channel", "gain_mdb", "stems", "cycle_16ths", "tolerance_pm", "fade_us", "ride", "slices" });
                if (! o.ok())
                    return;
                p.lane = static_cast<Lane> (o["lane"].choice (kLaneIds));
                p.ch = o["ch"].int32 (1, 16);
                p.mixChannel = o["mix_channel"].id();
                p.gainMdb = o["gain_mdb"].int32 (-60'000, 24'000);
                if (errors.any())
                    return;
                const Obj stems (o["stems"], { "a", "b" });
                if (! stems.ok())
                    return;
                p.stemA = fileIndex (stems["a"]);
                p.stemB = fileIndex (stems["b"], true);
                const auto ab = o["ab"];
                p.ab = ab.choiceOrNone (kAbSwitchIds);
                if (errors.any())
                    return;
                checkSwitch (ab, p.ab, p.stemB >= 0, "stems.b " + std::to_string (p.stemB));
                const auto cycle = o["cycle_16ths"];
                p.cycle16ths = cycle.int32 (16, 256);
                if (! errors.any() && p.cycle16ths % 16 != 0)
                    cycle.fail (std::to_string (p.cycle16ths) + " isn't a multiple of 16: a cycle is whole bars");
                p.tolerancePm = o["tolerance_pm"].int32 (0, 100);
                p.fadeUs = o["fade_us"].int32 (0, 100'000);
                if (errors.any())
                    return;
                const Obj ride (o["ride"], { "cc74", "cc71" });
                if (! ride.ok())
                    return;
                p.ride74 = readRide (ride["cc74"], p.cycle16ths);
                p.ride71 = readRide (ride["cc71"], p.cycle16ths);
                if (errors.any())
                    return;

                auto shorter = m.files[static_cast<std::size_t> (p.stemA)].frames;
                if (p.stemB >= 0)
                    shorter = std::min (shorter, m.files[static_cast<std::size_t> (p.stemB)].frames);
                for (const auto& item : o["slices"].items (1, 256, "slices"))
                {
                    const Obj s (item, { "step", "key", "start", "end", "fade", "lead", "holds" });
                    if (! s.ok())
                        return;
                    SliceEntry e;
                    const auto step = s["step"];
                    e.step = step.int32 (0, p.cycle16ths - 1);
                    if (! errors.any() && ! p.slices.empty() && e.step < p.slices.back().step)
                        step.fail (std::to_string (e.step) + " after step " + std::to_string (p.slices.back().step) + ": slices go by step, ascending");
                    e.key = s["key"].int32 (0, 127);
                    if (errors.any())
                        return;
                    for (std::size_t j = 0; j < p.slices.size(); ++j)
                        if (p.slices[j].step == e.step && p.slices[j].key == e.key)
                        {
                            s.fail ("step " + std::to_string (e.step) + ", key " + std::to_string (e.key) + " is slices[" + std::to_string (j) + "]'s too");
                            return;
                        }
                    e.start = s["start"].integer (0, shorter - 1);
                    if (errors.any())
                        return;
                    e.end = s["end"].integer (e.start + 1, shorter);
                    if (errors.any())
                        return;
                    e.fade = s["fade"].integer (e.start, e.end);
                    e.lead = s["lead"].int32 (0, 48'000);
                    const auto holds = s["holds"].ints (0, kMaxHolds, 0, 127, "keys");
                    if (errors.any())
                        return;
                    e.holdCount = static_cast<std::int32_t> (holds.size());
                    std::copy (holds.begin(), holds.end(), e.holds.begin()); // the rest stays 0

                    // Its notes (its own and the ones it holds) fall back to the multisample: the keymap must cover them.
                    const auto covered = [&] (std::int32_t key) {
                        return std::any_of (m.keymap.begin(), m.keymap.end(), [&] (const KeymapEntry& k) { return k.ch == p.ch && k.lo <= key && key <= k.hi; });
                    };
                    const auto uncovered = [&] (std::int32_t key) {
                        return "key " + std::to_string (key) + " on channel " + std::to_string (p.ch) + " has no keymap entry: the multisample its note falls back to";
                    };
                    if (! covered (e.key))
                    {
                        s.fail (uncovered (e.key));
                        return;
                    }
                    for (std::size_t h = 0; h < holds.size(); ++h)
                        if (! covered (holds[h]))
                        {
                            errors.add (element (member (s.path(), "holds"), h), uncovered (holds[h]));
                            return;
                        }
                    p.slices.push_back (e);
                }
            }

            void checkFilesPlayed()
            {
                std::vector<bool> played (m.files.size(), false);
                for (const auto& in : m.instruments)
                    for (const auto& z : in.zones)
                        for (const auto& l : z.layers)
                            for (const auto file : l.rr)
                                played[static_cast<std::size_t> (file)] = true;
                for (const auto& p : m.phrases)
                {
                    played[static_cast<std::size_t> (p.stemA)] = true;
                    if (p.stemB >= 0)
                        played[static_cast<std::size_t> (p.stemB)] = true;
                }
                for (std::size_t i = 0; i < played.size(); ++i)
                    if (! played[i])
                    {
                        errors.add (element ("files", i), quote (m.files[i].path) + " isn't played by any instrument or phrase");
                        return;
                    }
            }
        };

        // ------------------------------------------------------------------ mix.json

        constexpr std::array<std::string_view, 3> kWhenVariantIds { "any", "vox", "novox" };
        constexpr std::array<std::string_view, 4> kWhenAbIds { "none", "kick", "bass", "stab" };
        constexpr std::array<std::string_view, 2> kVariantIds { "vox", "novox" };
        constexpr std::array<std::string_view, 1> kWhenKey { "when" };

        // Where an insert list is.
        enum class Where : std::uint8_t {
            channel,
            chain, // a parallel insert's chain (a channel's when Context::channel >= 0)
            group,
            ret,
            bus
        };

        struct Context
        {
            Where where = Where::channel;
            int channel = -1; // the channel these inserts are on, or -1
            int* ccFilters = nullptr; // that channel's cc_filter count
        };

        class MixReader
        {
        public:
            MixReader (Errors& sink, const KitManifest& kitManifest, MixConfig& out) : errors (sink), kit (kitManifest), mix (out) {}

            void run (const Value& root)
            {
                if (root.type != Type::object)
                {
                    errors.add ("", "the manifest must be an object, not " + std::string (gen::json::typeName (root.type)));
                    return;
                }
                const Obj top (Node (&root, "", errors), { "kit", "content_version", "channels", "groups", "returns", "bus_eq", "master", "reference" });
                if (! top.ok())
                    return;

                const auto kitId = top["kit"];
                mix.kit = kitId.string();
                if (! errors.any() && mix.kit != kit.kit.id)
                    kitId.fail (quote (mix.kit) + ", but kit.json's id is " + quote (kit.kit.id));
                const auto version = top["content_version"];
                mix.contentVersion = version.int32 (1, 999);
                if (! errors.any() && mix.contentVersion != kit.kit.contentVersion)
                    version.fail (std::to_string (mix.contentVersion) + ", but kit.json's is " + std::to_string (kit.kit.contentVersion));
                if (errors.any())
                    return;

                readChannels (top["channels"]);
                if (! errors.any())
                    readGroups (top["groups"]);
                if (! errors.any())
                    readReturns (top["returns"]);
                if (! errors.any())
                {
                    const Obj bus (top["bus_eq"], std::span<const std::string_view> (kVariantIds));
                    for (std::size_t v = 0; v < 2 && bus.ok() && ! errors.any(); ++v)
                        mix.busEq[v] = readInserts (bus[kVariantIds[v]], kUnbounded, { Where::bus });
                }
                if (! errors.any())
                    readMaster (top["master"]);
                if (! errors.any())
                    readReference (top["reference"]);
                if (! errors.any())
                    checkLevelDuckCycles();
                if (! errors.any())
                    checkAgainstKit();
            }

        private:
            struct OnsetKey
            {
                bool keyed = false;
                std::int32_t preUs = 0;
                std::string path; // the duck that set it
            };

            struct DuckEdge
            {
                int key = -1; // the channel the level duck keys on
                std::string path; // its key_channel
            };

            Errors& errors;
            const KitManifest& kit;
            MixConfig& mix;
            std::vector<std::string> channelIds; // read before the channels, for key_channel
            std::array<OnsetKey, kLanes> onsetKeys {}; // per key lane: the first onset duck's pre_us
            std::vector<std::vector<DuckEdge>> duckEdges; // per channel: the channels its level ducks key on

            void readChannels (const Node& n)
            {
                const auto list = n.items (1, kMaxMixChannels, "channels");
                if (errors.any())
                    return;
                // The ids first, so a level duck may key on a channel further down.
                for (const auto& item : list)
                {
                    const Value* id = item.is (Type::object) ? item.value()->find ("id") : nullptr;
                    channelIds.push_back (id != nullptr && id->type == Type::string ? id->string : std::string());
                }
                duckEdges.assign (list.size(), {});
                for (std::size_t i = 0; i < list.size(); ++i)
                {
                    const Obj o (list[i], { "id", "lane", "group", "inserts", "sends_pm" });
                    if (! o.ok())
                        return;
                    Channel c;
                    const auto id = o["id"];
                    c.id = id.id();
                    for (std::size_t j = 0; j < i && ! errors.any(); ++j)
                        if (channelIds[j] == c.id)
                            id.fail (quote (c.id) + " is channels[" + std::to_string (j) + "]'s id too");
                    c.lane = static_cast<std::uint8_t> (o["lane"].choice (kLaneIds));
                    c.group = static_cast<Group> (o["group"].choice (kGroupIds));
                    const auto sends = o["sends_pm"].ints (kSends, kSends, 0, 1000);
                    if (errors.any())
                        return;
                    std::copy (sends.begin(), sends.end(), c.sendsPm.begin());
                    int ccFilters = 0;
                    c.inserts = readInserts (o["inserts"], 16, { Where::channel, static_cast<int> (i), &ccFilters });
                    if (errors.any())
                        return;
                    mix.channels.push_back (std::move (c));
                }
            }

            void readGroups (const Node& n)
            {
                const Obj groups (n, { "drums", "music" });
                if (! groups.ok())
                    return;
                const std::array<std::pair<std::string_view, GroupConfig*>, 2> all { { { "drums", &mix.drums }, { "music", &mix.music } } };
                for (const auto& [key, group] : all)
                {
                    const Obj g (groups[key], { "inserts" });
                    if (! g.ok())
                        return;
                    group->inserts = readInserts (g["inserts"], kUnbounded, { Where::group });
                    if (errors.any())
                        return;
                }
            }

            std::vector<Insert> readInserts (const Node& n, std::size_t maxCount, const Context& context)
            {
                std::vector<Insert> out;
                for (const auto& item : n.items (context.where == Where::chain ? 1 : 0, maxCount, "inserts"))
                {
                    auto insert = readInsert (item, context);
                    if (errors.any())
                        return out;
                    out.push_back (std::move (insert));
                }
                return out;
            }

            Band readBand (const Node& n)
            {
                Band b;
                const Obj o (n, { "lo_hz", "hi_hz" });
                if (! o.ok())
                    return b;
                b.loHz = o["lo_hz"].int32 (0, 24'000);
                b.hiHz = o["hi_hz"].int32 (0, 24'000);
                if (! errors.any() && b.loHz > 0 && b.hiHz > 0 && b.loHz >= b.hiHz)
                    o.fail ("lo_hz " + std::to_string (b.loHz) + " isn't under hi_hz " + std::to_string (b.hiHz));
                return b;
            }

            When readWhen (const Node& n)
            {
                When w;
                const Obj o (n, { "variant", "ab", "is" });
                if (! o.ok())
                    return w;
                w.variant = static_cast<std::int8_t> (o["variant"].choice (kWhenVariantIds) - 1);
                w.ab = static_cast<std::int8_t> (o["ab"].choice (kWhenAbIds) - 1);
                w.is = static_cast<std::int8_t> (o["is"].choice (kSideIds));
                if (! errors.any() && w.always())
                    o.fail ("variant any and ab none restrict nothing: leave the when out");
                return w;
            }

            // An insert's keys after "type", by its type (and a duck's mode).
            static std::vector<std::string_view> insertKeys (InsertType type, DuckMode mode)
            {
                switch (type)
                {
                    case InsertType::gain: return { "gain_mdb" };
                    case InsertType::pan: return { "pan_pm" };
                    case InsertType::width: return { "width_pm", "freq_hz", "order" };
                    case InsertType::peak:
                    case InsertType::lowShelf:
                    case InsertType::highShelf: return { "freq_hz", "gain_mdb", "q_pm" };
                    case InsertType::highpass:
                    case InsertType::lowpass: return { "freq_hz", "order", "q_pm", "stages", "band" };
                    case InsertType::ccFilter:
                        return { "response", "cc74_lo_hz", "cc74_hi_hz", "cc74_default", "cc71_lo_pm", "cc71_hi_pm", "cc71_default", "band" };
                    case InsertType::monoBelow: return { "freq_hz", "order" };
                    case InsertType::clip: return { "ceiling_mdb", "knee_mdb" };
                    case InsertType::drive: return { "drive_pm", "bias_pm", "output_mdb" };
                    case InsertType::duck:
                        if (mode == DuckMode::onset)
                            return { "mode", "key_lanes", "min_velocity", "depth_mdb", "ramp_us", "hold_us", "release_us", "pre_us", "band", "pump" };
                        return { "mode", "key_channel", "depth_mdb", "range_mdb", "top_mdb", "attack_us", "release_us", "band", "pump" };
                    case InsertType::comp:
                        return { "threshold_mdb", "ratio_pm", "knee_mdb", "attack_us", "release_fast_us", "release_slow_us", "makeup_mdb" };
                    case InsertType::parallel: return { "level_mdb", "inserts" };
                }
                return {};
            }

            Insert readInsert (const Node& n, const Context& context)
            {
                Insert insert;
                if (! n.expect (Type::object))
                    return insert;
                const Value* typeValue = n.value()->find ("type");
                if (typeValue == nullptr)
                {
                    n.fail ("missing \"type\"");
                    return insert;
                }
                insert.type = static_cast<InsertType> (Node (typeValue, member (n.path(), "type"), errors).choice (kInsertTypeIds));
                if (errors.any())
                    return insert;
                auto mode = DuckMode::onset;
                if (insert.type == InsertType::duck)
                {
                    const Value* modeValue = n.value()->find ("mode");
                    if (modeValue == nullptr)
                    {
                        n.fail ("missing \"mode\"");
                        return insert;
                    }
                    mode = static_cast<DuckMode> (Node (modeValue, member (n.path(), "mode"), errors).choice (kDuckModeIds));
                    if (errors.any())
                        return insert;
                }

                // Where it may go.
                if (insert.type == InsertType::ccFilter)
                {
                    if (context.where == Where::chain)
                        n.fail ("a cc_filter can't be in a parallel chain");
                    else if (context.where != Where::channel)
                        n.fail ("a cc_filter goes on channels only");
                    else if (++*context.ccFilters > 1)
                        n.fail ("a second cc_filter: a channel has at most one cc_filter");
                }
                else if (insert.type == InsertType::parallel && context.where == Where::chain)
                    n.fail ("a parallel inside a parallel: a chain holds no parallel");
                if (errors.any())
                    return insert;

                auto keys = insertKeys (insert.type, mode);
                keys.insert (keys.begin(), "type");
                const Obj o (n, keys, kWhenKey);
                if (! o.ok())
                    return insert;

                switch (insert.type)
                {
                    case InsertType::gain: insert.params = GainParams { o["gain_mdb"].int32 (-60'000, 24'000) }; break;
                    case InsertType::pan: insert.params = PanParams { o["pan_pm"].int32 (-1000, 1000) }; break;
                    case InsertType::width:
                        insert.params = WidthParams { o["width_pm"].int32 (0, 4000), o["freq_hz"].int32 (0, 24'000), o["order"].int32 (1, 8) };
                        break;
                    case InsertType::peak:
                    case InsertType::lowShelf:
                    case InsertType::highShelf:
                        insert.params = EqParams { o["freq_hz"].int32 (10, 24'000), o["gain_mdb"].int32 (-30'000, 30'000), o["q_pm"].int32 (100, 30'000) };
                        break;
                    case InsertType::highpass:
                    case InsertType::lowpass:
                    {
                        PassParams p;
                        p.freqHz = o["freq_hz"].int32 (5, 24'000);
                        p.order = o["order"].int32 (1, 8);
                        const auto q = o["q_pm"];
                        p.qPm = static_cast<std::int32_t> (std::clamp<std::int64_t> (q.raw(), -1, 1'000'000));
                        if (! errors.any() && p.qPm != 0 && (p.qPm < 100 || p.qPm > 30'000))
                            q.fail (std::to_string (q.raw()) + " is out of range (0, or 100 to 30000)");
                        else if (! errors.any() && p.qPm != 0 && p.order != 2)
                            q.fail (std::to_string (p.qPm) + " with order " + std::to_string (p.order) + ": a Q needs order 2");
                        p.stages = o["stages"].int32 (1, 2);
                        p.band = readBand (o["band"]);
                        insert.params = p;
                        break;
                    }
                    case InsertType::ccFilter:
                    {
                        CcFilterParams p;
                        p.response = static_cast<FilterResponse> (o["response"].choice (kFilterResponseIds));
                        p.cc74LoHz = o["cc74_lo_hz"].int32 (10, 24'000);
                        if (errors.any())
                            break;
                        p.cc74HiHz = o["cc74_hi_hz"].int32 (p.cc74LoHz + 1, 24'000);
                        p.cc74Default = o["cc74_default"].int32 (0, 127);
                        p.cc71LoPm = o["cc71_lo_pm"].int32 (100, 30'000);
                        p.cc71HiPm = o["cc71_hi_pm"].int32 (100, 30'000);
                        p.cc71Default = o["cc71_default"].int32 (0, 127);
                        p.band = readBand (o["band"]);
                        insert.params = p;
                        break;
                    }
                    case InsertType::monoBelow: insert.params = MonoBelowParams { o["freq_hz"].int32 (20, 2000), o["order"].int32 (1, 8) }; break;
                    case InsertType::clip: insert.params = ClipParams { o["ceiling_mdb"].int32 (-40'000, 0), o["knee_mdb"].int32 (100, 24'000) }; break;
                    case InsertType::drive:
                        insert.params = DriveParams { o["drive_pm"].int32 (100, 50'000), o["bias_pm"].int32 (-2000, 2000), o["output_mdb"].int32 (-40'000, 24'000) };
                        break;
                    case InsertType::duck: insert.params = readDuck (o, mode, context); break;
                    case InsertType::comp:
                        insert.params = CompParams { o["threshold_mdb"].int32 (-60'000, 0), o["ratio_pm"].int32 (1001, 100'000), o["knee_mdb"].int32 (0, 24'000),
                            o["attack_us"].int32 (100, 1'000'000), o["release_fast_us"].int32 (1000, 10'000'000),
                            o["release_slow_us"].int32 (1000, 10'000'000), o["makeup_mdb"].int32 (-24'000, 24'000) };
                        break;
                    case InsertType::parallel:
                    {
                        ParallelParams p;
                        p.levelMdb = o["level_mdb"].int32 (-60'000, 12'000);
                        if (errors.any())
                            break;
                        p.chain = readInserts (o["inserts"], 16, { Where::chain, context.channel, context.ccFilters });
                        insert.params = std::move (p);
                        break;
                    }
                }
                if (errors.any())
                    return insert;
                if (const auto when = o["when"]; when.present())
                    insert.when = readWhen (when);
                return insert;
            }

            DuckParams readDuck (const Obj& o, DuckMode mode, const Context& context)
            {
                DuckParams d;
                d.mode = mode;
                const auto depth = o["depth_mdb"];
                d.depthMdb = depth.int32 (-24'000, 60'000);
                if (! errors.any() && d.depthMdb == 0)
                    depth.fail ("0: a duck's depth isn't 0");
                d.releaseUs = o["release_us"].int32 (1000, 4'000'000);
                d.band = readBand (o["band"]);
                d.pump = o["pump"].flag();
                if (errors.any())
                    return d;

                if (mode == DuckMode::onset)
                {
                    for (const auto& lane : o["key_lanes"].items (1, kLanes, "lanes"))
                    {
                        const auto i = static_cast<unsigned> (lane.choice (kLaneIds));
                        if (errors.any())
                            return d;
                        const auto bit = static_cast<std::uint16_t> (1u << i);
                        if ((d.keyLanes & bit) != 0)
                        {
                            lane.fail (quote (kLaneIds[i]) + " again");
                            return d;
                        }
                        d.keyLanes = static_cast<std::uint16_t> (d.keyLanes | bit);
                    }
                    d.minVelocity = o["min_velocity"].int32 (1, 127);
                    d.rampUs = o["ramp_us"].int32 (0, 1'000'000);
                    d.holdUs = o["hold_us"].int32 (0, 2'000'000);
                    const auto pre = o["pre_us"];
                    d.preUs = pre.int32 (0, 100'000);
                    if (errors.any())
                        return d;
                    // One pre_us per key lane: the timeline places each lane's keys early once.
                    for (std::size_t lane = 0; lane < kLanes; ++lane)
                    {
                        if ((d.keyLanes & (1u << lane)) == 0)
                            continue;
                        auto& seen = onsetKeys[lane];
                        if (! seen.keyed)
                            seen = { true, d.preUs, o.path() };
                        else if (seen.preUs != d.preUs)
                        {
                            pre.fail (std::to_string (d.preUs) + ", but " + seen.path + " keys the " + std::string (kLaneIds[lane]) + " lane with pre_us "
                                      + std::to_string (seen.preUs) + ": onset ducks keyed on one lane share one pre_us");
                            return d;
                        }
                    }
                    return d;
                }

                const auto key = o["key_channel"];
                const auto id = key.id();
                d.rangeMdb = o["range_mdb"].int32 (1000, 60'000);
                d.topMdb = o["top_mdb"].int32 (-60'000, 12'000);
                d.attackUs = o["attack_us"].int32 (100, 1'000'000);
                if (errors.any())
                    return d;
                const auto found = std::find (channelIds.begin(), channelIds.end(), id);
                if (found == channelIds.end())
                {
                    key.fail ("no channel " + quote (id));
                    return d;
                }
                d.keyChannel = static_cast<std::int32_t> (found - channelIds.begin());
                if (context.channel >= 0)
                {
                    if (d.keyChannel == context.channel)
                        key.fail (quote (id) + " is the duck's own channel: a level duck keys on another channel");
                    else
                        duckEdges[static_cast<std::size_t> (context.channel)].push_back ({ d.keyChannel, key.path() });
                }
                return d;
            }

            // A channel's level duck follows its key channel's signal, so the key channel must be computed first;
            // a loop of them (A ducked by B while B is ducked by A, or longer) has no first channel. Only channels
            // count: groups, returns and the bus come after every channel. The first cycle is refused, naming its
            // channels, at the key_channel that closes it.
            void checkLevelDuckCycles()
            {
                const auto n = duckEdges.size();
                std::vector<int> state (n, 0); // 0 unvisited, 1 on the path, 2 done
                std::vector<int> path;
                std::function<bool (int)> visit = [&] (int c) {
                    state[static_cast<std::size_t> (c)] = 1;
                    path.push_back (c);
                    for (const auto& edge : duckEdges[static_cast<std::size_t> (c)])
                    {
                        const auto k = static_cast<std::size_t> (edge.key);
                        if (state[k] == 1)
                        {
                            std::string cycle;
                            for (auto it = std::find (path.begin(), path.end(), edge.key); it != path.end(); ++it)
                            {
                                const int next = std::next (it) == path.end() ? edge.key : *std::next (it);
                                cycle += (cycle.empty() ? "" : ", ") + mix.channels[static_cast<std::size_t> (*it)].id + " is keyed on "
                                       + mix.channels[static_cast<std::size_t> (next)].id;
                            }
                            errors.add (edge.path, "the level ducks form a cycle: " + cycle);
                            return true;
                        }
                        if (state[k] == 0 && visit (edge.key))
                            return true;
                    }
                    state[static_cast<std::size_t> (c)] = 2;
                    path.pop_back();
                    return false;
                };
                for (std::size_t c = 0; c < n; ++c)
                    if (state[c] == 0 && visit (static_cast<int> (c)))
                        return;
            }

            void readReturns (const Node& n)
            {
                const Obj returns (n, { "space", "echo", "resonator" });
                if (! returns.ok())
                    return;
                readSpace (returns["space"]);
                if (! errors.any())
                    readEcho (returns["echo"]);
                if (! errors.any())
                    readResonator (returns["resonator"]);
            }

            void readSpace (const Node& n)
            {
                const Obj o (n, { "delays_frames", "signs", "t60_ms", "t60_low_ms", "low_hz", "damp_hz", "predelay_us", "send_hp_hz", "send_order",
                                    "shimmer_pm", "shimmer_lo_hz", "shimmer_hi_hz", "window_frames", "mono_hz", "return_mdb", "inserts" });
                if (! o.ok())
                    return;
                auto& s = mix.space;
                const auto delays = o["delays_frames"].ints (8, 8, 1, 48'000);
                if (errors.any())
                    return;
                std::copy (delays.begin(), delays.end(), s.delaysFrames.begin());
                const auto signs = o["signs"].items (8, 8, "values");
                for (std::size_t i = 0; i < signs.size() && ! errors.any(); ++i)
                {
                    const auto v = signs[i].raw();
                    if (! errors.any() && v != -1 && v != 1)
                        signs[i].fail (std::to_string (v) + " isn't -1 or 1");
                    s.signs[i] = v < 0 ? -1 : 1;
                }
                s.t60Ms = o["t60_ms"].int32 (100, 30'000);
                s.t60LowMs = o["t60_low_ms"].int32 (0, 30'000);
                s.lowHz = o["low_hz"].int32 (20, 2000);
                s.dampHz = o["damp_hz"].int32 (500, 24'000);
                s.predelayUs = o["predelay_us"].int32 (0, 500'000);
                s.sendHpHz = o["send_hp_hz"].int32 (0, 2000);
                s.sendOrder = o["send_order"].int32 (1, 8);
                s.shimmerPm = o["shimmer_pm"].int32 (0, 500);
                const auto shimmerLo = o["shimmer_lo_hz"];
                s.shimmerLoHz = shimmerLo.int32 (20, 24'000);
                s.shimmerHiHz = o["shimmer_hi_hz"].int32 (20, 24'000);
                if (! errors.any() && s.shimmerLoHz >= s.shimmerHiHz)
                    shimmerLo.fail (std::to_string (s.shimmerLoHz) + " isn't under shimmer_hi_hz " + std::to_string (s.shimmerHiHz));
                s.windowFrames = o["window_frames"].int32 (64, 16'384);
                s.monoHz = o["mono_hz"].int32 (0, 1000);
                s.returnMdb = o["return_mdb"].int32 (-60'000, 12'000);
                if (errors.any())
                    return;
                s.inserts = readInserts (o["inserts"], 16, { Where::ret });
            }

            void readEcho (const Node& n)
            {
                const Obj o (n, { "steps", "feedback_pm", "lp_hz", "hp_hz", "drive_pm", "wow_ppm", "wow_mhz", "flutter_ppm", "flutter_mhz", "phases_mrad",
                                    "return_mdb", "inserts" });
                if (! o.ok())
                    return;
                auto& e = mix.echo;
                e.steps = o["steps"].int32 (1, 16);
                e.feedbackPm = o["feedback_pm"].int32 (0, 950);
                e.lpHz = o["lp_hz"].int32 (200, 24'000);
                e.hpHz = o["hp_hz"].int32 (10, 5000);
                e.drivePm = o["drive_pm"].int32 (100, 10'000);
                e.wowPpm = o["wow_ppm"].int32 (0, 20'000);
                e.wowMhz = o["wow_mhz"].int32 (0, 20'000);
                e.flutterPpm = o["flutter_ppm"].int32 (0, 20'000);
                e.flutterMhz = o["flutter_mhz"].int32 (0, 50'000);
                const auto phases = o["phases_mrad"].ints (2, 2, 0, 6284);
                if (errors.any())
                    return;
                e.phasesMrad = { phases[0], phases[1] };
                e.returnMdb = o["return_mdb"].int32 (-60'000, 12'000);
                if (errors.any())
                    return;
                e.inserts = readInserts (o["inserts"], 16, { Where::ret });
            }

            void readResonator (const Node& n)
            {
                const Obj o (n, { "tones_mhz", "tau_ms", "send_hp_hz", "return_mdb", "inserts" });
                if (! o.ok())
                    return;
                auto& r = mix.resonator;
                r.tonesMhz = o["tones_mhz"].ints (0, 24, 20'000, 20'000'000);
                r.tauMs = o["tau_ms"].int32 (10, 10'000);
                r.sendHpHz = o["send_hp_hz"].int32 (0, 5000);
                r.returnMdb = o["return_mdb"].int32 (-60'000, 12'000);
                if (errors.any())
                    return;
                r.inserts = readInserts (o["inserts"], 16, { Where::ret });
            }

            void readMaster (const Node& n)
            {
                const Obj o (n, { "lowcut_hz", "lowcut_order", "mono_below_hz", "glue", "vox", "novox", "clip", "limiter", "dc_hz" });
                if (! o.ok())
                    return;
                MasterConfig m;
                m.lowcutHz = o["lowcut_hz"].int32 (5, 200);
                m.lowcutOrder = o["lowcut_order"].int32 (1, 8);
                m.monoBelowHz = o["mono_below_hz"].int32 (0, 1000);
                if (errors.any())
                    return;
                const Obj glue (o["glue"], { "ratio_pm", "knee_mdb", "attack_us", "release_fast_us", "release_slow_us" });
                if (! glue.ok())
                    return;
                m.glueRatioPm = glue["ratio_pm"].int32 (1001, 20'000);
                m.glueKneeMdb = glue["knee_mdb"].int32 (0, 24'000);
                m.glueAttackUs = glue["attack_us"].int32 (100, 1'000'000);
                m.glueReleaseFastUs = glue["release_fast_us"].int32 (1000, 10'000'000);
                m.glueReleaseSlowUs = glue["release_slow_us"].int32 (1000, 10'000'000);
                if (errors.any())
                    return;
                const Obj clip (o["clip"], { "knee_mdb", "ceiling_mdb" });
                if (! clip.ok())
                    return;
                m.clipKneeMdb = clip["knee_mdb"].int32 (100, 12'000);
                m.clipCeilingMdb = clip["ceiling_mdb"].int32 (-12'000, 0);
                if (errors.any())
                    return;
                const Obj limiter (o["limiter"], { "ceiling_mdb", "lookahead_us", "hold_us", "release_us" });
                if (! limiter.ok())
                    return;
                m.limiterCeilingMdb = limiter["ceiling_mdb"].int32 (-12'000, 0);
                m.limiterLookaheadUs = limiter["lookahead_us"].int32 (0, 20'000);
                m.limiterHoldUs = limiter["hold_us"].int32 (0, 200'000);
                m.limiterReleaseUs = limiter["release_us"].int32 (1000, 1'000'000);
                const auto dc = o["dc_hz"];
                m.dcHz = dc.int32 (1, 20);
                if (! errors.any() && m.dcHz >= m.lowcutHz)
                    dc.fail (std::to_string (m.dcHz) + " isn't below lowcut_hz " + std::to_string (m.lowcutHz));
                for (std::size_t v = 0; v < 2 && ! errors.any(); ++v)
                {
                    const Obj variant (o[kVariantIds[v]], { "glue_threshold_mdb", "drive_mdb" });
                    if (! variant.ok())
                        return;
                    mix.master[v] = m;
                    mix.master[v].glueThresholdMdb = variant["glue_threshold_mdb"].int32 (-60'000, 0);
                    mix.master[v].driveMdb = variant["drive_mdb"].int32 (0, 24'000);
                }
            }

            void readReference (const Node& n)
            {
                const Obj reference (n, std::span<const std::string_view> (kVariantIds));
                for (std::size_t v = 0; v < 2 && reference.ok() && ! errors.any(); ++v)
                {
                    const Obj o (reference[kVariantIds[v]], { "pre_master_lufs_mlu", "club_lufs_mlu", "channel_true_peak_mdb" });
                    if (! o.ok())
                        return;
                    auto& ref = mix.reference[v];
                    ref.preMasterLufsMlu = o["pre_master_lufs_mlu"].int32 (-60'000, 0);
                    ref.clubLufsMlu = o["club_lufs_mlu"].int32 (-60'000, 0);
                    const auto peaks = o["channel_true_peak_mdb"];
                    const auto count = mix.channels.size();
                    if (peaks.is (Type::array) && peaks.value()->items.size() != count)
                        peaks.fail ("it holds " + plural (peaks.value()->items.size(), "values") + ": it must hold " + std::to_string (count) + ", one per channel");
                    else
                        ref.channelTruePeakMdb = peaks.ints (count, count, -120'000, 12'000);
                }
            }

            // docs/plugin/manifest.md 3.6: every channel kit.json names is here, on the lane of what plays on it.
            void checkAgainstKit()
            {
                const auto check = [&] (std::string_view what, std::size_t i, const std::string& channel, Lane lane, const std::string& name) {
                    const int c = mix.channelIndex (channel);
                    const auto where = element (what, i);
                    if (c < 0)
                    {
                        errors.add ("channels", "no channel " + quote (channel) + ", which kit.json's " + where + ".mix_channel names");
                        return false;
                    }
                    const auto channelLane = static_cast<Lane> (mix.channels[static_cast<std::size_t> (c)].lane);
                    if (channelLane != lane)
                    {
                        errors.add (member (element ("channels", static_cast<std::size_t> (c)), "lane"),
                            quote (laneId (channelLane)) + ", but kit.json's " + where + " (" + name + ", lane " + std::string (laneId (lane)) + ") plays on it");
                        return false;
                    }
                    return true;
                };
                for (std::size_t i = 0; i < kit.instruments.size(); ++i)
                    if (! check ("instruments", i, kit.instruments[i].mixChannel, kit.instruments[i].lane, quote (kit.instruments[i].id)))
                        return;
                for (std::size_t i = 0; i < kit.phrases.size(); ++i)
                    if (! check ("phrases", i, kit.phrases[i].mixChannel, kit.phrases[i].lane, "a phrase"))
                        return;
            }
        };
    }

    util::Result<KitManifest> parseKitManifest (std::string_view json)
    {
        using R = util::Result<KitManifest>;
        const auto doc = gen::json::parse (json);
        if (! doc.ok())
            return R::failure (syntaxError ("kit.json", json, doc.error));
        Errors errors ("kit.json");
        KitManifest m;
        KitReader (errors, m).run (doc.value);
        if (errors.any())
            return R::failure (errors.first());
        return R::success (std::move (m));
    }

    util::Result<MixConfig> parseMixManifest (std::string_view json, const KitManifest& kit)
    {
        using R = util::Result<MixConfig>;
        const auto doc = gen::json::parse (json);
        if (! doc.ok())
            return R::failure (syntaxError ("mix.json", json, doc.error));
        Errors errors ("mix.json");
        MixConfig mix;
        MixReader (errors, kit, mix).run (doc.value);
        if (errors.any())
            return R::failure (errors.first());
        return R::success (std::move (mix));
    }
}
