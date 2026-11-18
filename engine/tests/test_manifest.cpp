// The manifest parsers (kit/manifest.h): they take what docs/plugin/manifest.md allows and refuse everything else,
// each refusal one line naming the file kind and the JSON path.
//
// The valid pair is manifest_samples.h's TST01. Each refusal below edits it in one place (Doc), so the error can
// only come from that edit.

#include "manifest_samples.h"

#include "kit/manifest.h"
#include "testing/manifest_json.h"

#include "gen/json.h"
#include "util/file_io.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

using namespace wubansia;
using namespace wubansia::engine;
using gen::json::Value;

namespace
{
    Value integer (std::int64_t v)
    {
        Value x;
        x.type = Value::Type::integer;
        x.integer = v;
        return x;
    }

    Value str (std::string s)
    {
        Value x;
        x.type = Value::Type::string;
        x.string = std::move (s);
        return x;
    }

    Value flag (bool b)
    {
        Value x;
        x.type = Value::Type::boolean;
        x.boolean = b;
        return x;
    }

    Value object()
    {
        Value x;
        x.type = Value::Type::object;
        return x;
    }

    Value array (std::vector<Value> items)
    {
        Value x;
        x.type = Value::Type::array;
        x.items = std::move (items);
        return x;
    }

    Value ints (std::initializer_list<std::int64_t> values)
    {
        std::vector<Value> items;
        for (const auto v : values)
            items.push_back (integer (v));
        return array (std::move (items));
    }

    Value strs (std::initializer_list<std::string_view> values)
    {
        std::vector<Value> items;
        for (const auto v : values)
            items.push_back (str (std::string (v)));
        return array (std::move (items));
    }

    Value parsed (std::string_view json)
    {
        auto r = gen::json::parse (json);
        REQUIRE (r.ok());
        return std::move (r.value);
    }

    void dumpString (std::string& out, const std::string& s)
    {
        out += '"';
        for (const char c : s)
        {
            if (c == '"' || c == '\\')
                (out += '\\') += c;
            else if (static_cast<unsigned char> (c) < 0x20)
            {
                static constexpr char hex[] = "0123456789abcdef";
                out += "\\u00";
                out += hex[(c >> 4) & 0xF];
                out += hex[c & 0xF];
            }
            else
                out += c;
        }
        out += '"';
    }

    void dump (std::string& out, const Value& v, int depth)
    {
        const std::string pad (static_cast<std::size_t> (2 * (depth + 1)), ' ');
        switch (v.type)
        {
            case Value::Type::null: out += "null"; break;
            case Value::Type::boolean: out += v.boolean ? "true" : "false"; break;
            case Value::Type::integer: out += std::to_string (v.integer); break;
            case Value::Type::string: dumpString (out, v.string); break;
            case Value::Type::array:
                out += "[";
                for (std::size_t i = 0; i < v.items.size(); ++i)
                {
                    out += (i == 0 ? "\n" : ",\n") + pad;
                    dump (out, v.items[i], depth + 1);
                }
                out += v.items.empty() ? "]" : "\n" + pad.substr (2) + "]";
                break;
            case Value::Type::object:
                out += "{";
                for (std::size_t i = 0; i < v.members.size(); ++i)
                {
                    out += (i == 0 ? "\n" : ",\n") + pad;
                    dumpString (out, v.members[i].key);
                    out += ": ";
                    dump (out, v.members[i].value, depth + 1);
                }
                out += v.members.empty() ? "}" : "\n" + pad.substr (2) + "}";
                break;
        }
    }

    // A manifest to break in one place: at ("instruments[1].zones[0]") is the value at that JSON path.
    class Doc
    {
    public:
        explicit Doc (std::string_view json) : root (parsed (json)) {}

        Value& at (std::string_view path) { return *walk (path, false); }
        void set (std::string_view path, Value v) { at (path) = std::move (v); }

        // Removes the member or array item at path.
        void erase (std::string_view path)
        {
            Value& parent = *walk (path, true);
            const auto last = steps (path).back();
            if (last.index >= 0)
                parent.items.erase (parent.items.begin() + last.index);
            else
            {
                auto& m = parent.members;
                const auto found = std::find_if (m.begin(), m.end(), [&] (const auto& member) { return member.key == last.key; });
                REQUIRE (found != m.end());
                m.erase (found);
            }
        }

        void add (std::string_view objectPath, std::string key, Value v)
        {
            Value& o = at (objectPath);
            REQUIRE (o.type == Value::Type::object);
            o.members.push_back ({ std::move (key), std::move (v) });
        }

        void push (std::string_view arrayPath, Value v)
        {
            Value& a = at (arrayPath);
            REQUIRE (a.type == Value::Type::array);
            a.items.push_back (std::move (v));
        }

        std::string text() const
        {
            std::string out;
            dump (out, root, 0);
            if (! raw.empty())
            {
                const auto at = out.find (kSentinel);
                REQUIRE (at != std::string::npos);
                out.replace (at, kSentinel.size(), raw);
            }
            return out + "\n";
        }

        // Puts text the Value can't hold (a float, say) at path.
        void setRaw (std::string_view path, std::string text)
        {
            set (path, integer (4242424242424242));
            raw = std::move (text);
        }

    private:
        struct Step
        {
            std::string key;
            std::ptrdiff_t index = -1;
        };

        static constexpr std::string_view kSentinel = "4242424242424242";

        static std::vector<Step> steps (std::string_view path)
        {
            std::vector<Step> out;
            std::size_t i = 0;
            while (i < path.size())
            {
                if (path[i] == '.')
                    ++i;
                else if (path[i] == '[')
                {
                    const auto close = path.find (']', i);
                    out.push_back ({ {}, std::stol (std::string (path.substr (i + 1, close - i - 1))) });
                    i = close + 1;
                }
                else
                {
                    auto end = path.find_first_of (".[", i);
                    if (end == std::string_view::npos)
                        end = path.size();
                    out.push_back ({ std::string (path.substr (i, end - i)), -1 });
                    i = end;
                }
            }
            return out;
        }

        Value* walk (std::string_view path, bool parentOnly)
        {
            auto s = steps (path);
            if (parentOnly)
                s.pop_back();
            Value* v = &root;
            for (const auto& step : s)
            {
                CAPTURE (path);
                if (step.index >= 0)
                {
                    REQUIRE (v->type == Value::Type::array);
                    REQUIRE (static_cast<std::size_t> (step.index) < v->items.size());
                    v = &v->items[static_cast<std::size_t> (step.index)];
                }
                else
                {
                    REQUIRE (v->type == Value::Type::object);
                    auto& m = v->members;
                    const auto found = std::find_if (m.begin(), m.end(), [&] (const auto& member) { return member.key == step.key; });
                    REQUIRE (found != m.end());
                    v = &found->value;
                }
            }
            return v;
        }

        Value root;
        std::string raw;
    };

    struct Bad
    {
        std::string_view what;
        std::function<void (Doc&)> edit;
        std::string_view starts; // the message starts with this: the file kind and the JSON path
        std::string_view says = {}; // and holds this
    };

    void checkRefusals (std::string_view base, const std::vector<Bad>& cases, const std::function<std::string (const std::string&)>& parse)
    {
        for (const auto& c : cases)
        {
            CAPTURE (c.what);
            Doc doc (base);
            c.edit (doc);
            const auto error = parse (doc.text());
            CAPTURE (error);
            CHECK_FALSE (error.empty());
            CHECK (error.starts_with (c.starts));
            if (! c.says.empty())
                CHECK (error.find (c.says) != std::string::npos);
            CHECK (error.find ('\n') == std::string::npos);
        }
    }

    std::string kitError (const std::string& json) { return parseKitManifest (json).error; }

    KitManifest testKit()
    {
        auto kit = parseKitManifest (test::kKitJson);
        REQUIRE (kit.ok());
        return std::move (kit.value);
    }

    std::string mixError (const std::string& json) { return parseMixManifest (json, testKit()).error; }

    // A ride of n values: 0, 1, 2... wrapping at 128.
    Value ride (std::size_t n)
    {
        std::vector<Value> items;
        for (std::size_t i = 0; i < n; ++i)
            items.push_back (integer (static_cast<std::int64_t> (i % 128)));
        return array (std::move (items));
    }

    // The phrase-fitting edits move the second slice: its fields in one place.
    Value slice (std::int64_t step, std::int64_t key, std::int64_t start, std::int64_t end, std::int64_t fade, std::int64_t lead, Value holds)
    {
        Value s;
        s.type = Value::Type::object;
        s.members = { { "step", integer (step) }, { "key", integer (key) }, { "start", integer (start) }, { "end", integer (end) },
            { "fade", integer (fade) }, { "lead", integer (lead) }, { "holds", std::move (holds) } };
        return s;
    }
}

TEST_CASE ("parseKitManifest reads every field", "[manifest]")
{
    const auto parsed = parseKitManifest (test::kKitJson);
    REQUIRE (parsed.error.empty());
    const auto& m = parsed.value;

    CHECK (m.kit.id == "TST01");
    CHECK (m.kit.title == "Test Signal");
    CHECK (m.kit.direction == "test / synthetic");
    CHECK (m.kit.bpm == 130);
    CHECK (m.kit.key == "F# minor");
    CHECK (m.kit.camelot == "11A");
    CHECK (m.kit.swingPm == 560);
    CHECK (m.kit.contentVersion == 3);
    CHECK (m.kit.lanes == std::array<std::string, kLanes> { "Kick", "Clap", "Hats", "", "Bass", "Lead", "Pad", "Vox", "", "Riser" });
    CHECK (m.kit.ab[0] == AbLabels { "Kick A", "Kick B" });
    CHECK (m.kit.ab[1] == AbLabels { "Bass A", "Bass B" });
    CHECK (m.kit.ab[2] == AbLabels {});

    REQUIRE (m.files.size() == 18);
    CHECK (m.files[0] == FileEntry { "Drums/Kick A.wav", 4800, 2, std::string (64, '0'), 0, 0, 0 });
    CHECK (m.files[9] == FileEntry { "Bass/Bass A C#2.wav", 47000, 1, std::string (64, '9'), 1200, 46000, 0 });
    CHECK (m.files[13].prerollFrames == 960);

    REQUIRE (m.instruments.size() == 12);
    const auto& kickB = m.instruments[1];
    CHECK (kickB.id == "kick_b");
    CHECK (kickB.name == "Kick B");
    CHECK (kickB.lane == Lane::kick);
    CHECK (kickB.mixChannel == "kick");
    CHECK (kickB.gainMdb == -500);
    CHECK (kickB.duckKey);
    REQUIRE (kickB.zones.size() == 1);
    CHECK (kickB.zones[0] == ZoneEntry { 36, 36, 36, { { 80, { 1, 2 } }, { 127, { 3, 4 } } }, {} });
    const auto& hat = m.instruments[3];
    CHECK (hat.choke == ChokeEntry { "hats", ChokeRole::chokes, 4000, ChokeCurve::cos2 });
    CHECK (hat.panCyclePm == std::vector<std::int32_t> { -300, 0, 300 });
    CHECK (hat.rrBlock16ths == 16);
    CHECK (m.instruments[4].choke.role == ChokeRole::choked);
    const auto& bassA = m.instruments[5];
    CHECK (bassA.mode == SampleMode::sustainLoop);
    CHECK (bassA.velocityLaw == VelocityLaw::pow15);
    CHECK (bassA.interp == Interp::hermite);
    CHECK (bassA.mono);
    CHECK (bassA.glide == Glide { GlideCurve::raisedCosine, 70000, 30000 });
    CHECK (bassA.attackUs == 1000);
    CHECK (bassA.releaseUs == 20000);
    REQUIRE (bassA.zones.size() == 2);
    CHECK (bassA.zones[1] == ZoneEntry { 37, 37, 127, { { 100, { 9 } } }, {} });
    CHECK (m.instruments[6].glide == Glide { GlideCurve::exponential, 18000, 0 });
    CHECK (m.instruments[6].layeredWith == 7);
    CHECK (m.instruments[7].layeredWith == -1);
    CHECK (m.instruments[7].mode == SampleMode::gated);
    CHECK (m.instruments[8].restrikeContinues);
    const auto& riser = m.instruments[11];
    CHECK (riser.align == Align::end);
    CHECK (riser.startFrames == 1000);
    CHECK (riser.attackUs == 2000);

    REQUIRE (m.keymap.size() == 9);
    CHECK (m.keymap[0] == KeymapEntry { 10, 36, 36, 0, 1, static_cast<std::int32_t> (AbSwitch::kick) });
    CHECK (m.keymap[1] == KeymapEntry { 10, 39, 39, 2, -1, -1 });
    CHECK (m.keymap[4] == KeymapEntry { 1, 0, 127, 5, 6, static_cast<std::int32_t> (AbSwitch::bass) });
    CHECK (m.keymap[8] == KeymapEntry { 11, 0, 127, 8, -1, -1 });

    REQUIRE (m.extras.size() == 1);
    CHECK (m.extras[0] == ExtraEntry { 11, "Pad", Lane::chords });

    REQUIRE (m.phrases.size() == 1);
    const auto& p = m.phrases[0];
    CHECK (p.lane == Lane::lead);
    CHECK (p.ch == 2);
    CHECK (p.ab == static_cast<std::int32_t> (AbSwitch::bass));
    CHECK (p.mixChannel == "lead");
    CHECK (p.stemA == 15);
    CHECK (p.stemB == 16);
    CHECK (p.cycle16ths == 32);
    CHECK (p.tolerancePm == 20);
    CHECK (p.fadeUs == 5000);
    CHECK (p.throwMode == PhraseThrow::live);
    CHECK (p.fadeInUs == 750);
    CHECK (p.ride74.empty());
    CHECK (p.ride71.empty());
    REQUIRE (p.slices.size() == 4);
    CHECK (p.slices[0] == SliceEntry { 0, 60, 0, 22000, 21000, 0, {}, 0 });
    CHECK (p.slices[1] == SliceEntry { 4, 62, 21500, 44000, 43000, 500, { 62 }, 1 });
    CHECK (p.slices[3] == SliceEntry { 12, 65, 66000, 90000, 88000, 0, { 67, 69 }, 2 });
}

TEST_CASE ("parseKitManifest doesn't depend on key order or layout", "[manifest]")
{
    // The same manifest with every object's keys reversed and no whitespace at all.
    Doc doc (test::kKitJson);
    std::function<void (Value&)> reverse = [&] (Value& v) {
        std::reverse (v.members.begin(), v.members.end());
        for (auto& m : v.members)
            reverse (m.value);
        for (auto& i : v.items)
            reverse (i);
    };
    reverse (doc.at (""));
    const auto a = parseKitManifest (test::kKitJson);
    const auto b = parseKitManifest (doc.text());
    REQUIRE (a.ok());
    REQUIRE (b.ok());
    CHECK (a.value == b.value);
}

TEST_CASE ("parseKitManifest refuses what the contract doesn't allow, naming the path", "[manifest]")
{
    const std::vector<Bad> cases {
        // JSON itself: core's strict reader, with the path found from the line and column.
        { "a float", [] (Doc& d) { d.setRaw ("instruments[1].attack_us", "1.5"); }, "kit.json: instruments[1].attack_us: line ", "1.5 is not an integer" },
        { "an exponent", [] (Doc& d) { d.setRaw ("files[2].frames", "4.8e3"); }, "kit.json: files[2].frames: line ", "not an integer" },
        { "-0", [] (Doc& d) { d.setRaw ("kit.swing_pm", "-0"); }, "kit.json: kit.swing_pm: line ", "-0" },
        { "a leading zero", [] (Doc& d) { d.setRaw ("keymap[3].keys[1]", "046"); }, "kit.json: keymap[3].keys[1]: line ", "leading zero" },
        { "a duplicate key", [] (Doc& d) { d.add ("kit", "bpm", integer (130)); }, "kit.json: kit: line ", "twice" },
        { "not JSON", [] (Doc& d) { d.setRaw ("phrases[0].slices[2].holds", "[1, 2,]"); }, "kit.json: phrases[0].slices[2].holds[2]: line " },
        { "an empty kit", [] (Doc& d) { d.set ("kit", object()); }, "kit.json: kit: missing \"id\"" },
        // Keys.
        { "an unknown key", [] (Doc& d) { d.add ("instruments[2]", "volume", integer (1)); }, "kit.json: instruments[2]: unknown key \"volume\"" },
        { "an unknown top-level key", [] (Doc& d) { d.add ("", "comment", str ("x")); }, "kit.json: unknown key \"comment\"" },
        { "a missing key", [] (Doc& d) { d.erase ("kit.title"); }, "kit.json: kit: missing \"title\"" },
        { "a missing top-level key", [] (Doc& d) { d.erase ("extras"); }, "kit.json: missing \"extras\"" },
        { "a missing glide key", [] (Doc& d) { d.erase ("instruments[5].glide.dip_us"); }, "kit.json: instruments[5].glide: missing \"dip_us\"" },
        { "a missing slice key", [] (Doc& d) { d.erase ("phrases[0].slices[1].holds"); }, "kit.json: phrases[0].slices[1]: missing \"holds\"" },
        { "the old hits key", [] (Doc& d) { d.add ("phrases[0].slices[0]", "hits", integer (1)); }, "kit.json: phrases[0].slices[0]: unknown key \"hits\"" },
        // Types.
        { "a string for an integer", [] (Doc& d) { d.set ("kit.bpm", str ("130")); }, "kit.json: kit.bpm: an integer was expected, not a string" },
        { "a flag for an integer", [] (Doc& d) { d.set ("files[0].frames", flag (true)); }, "kit.json: files[0].frames: an integer was expected, not a boolean" },
        { "null", [] (Doc& d) { d.set ("instruments[0].layered_with", Value {}); }, "kit.json: instruments[0].layered_with: a string was expected, not null" },
        { "an integer for a flag", [] (Doc& d) { d.set ("instruments[0].mono", integer (0)); }, "kit.json: instruments[0].mono: true or false was expected, not an integer" },
        { "an object for an array", [] (Doc& d) { d.set ("files", object()); }, "kit.json: files: an array was expected, not an object" },
        { "an array for an object", [] (Doc& d) { d.set ("kit", array ({})); }, "kit.json: kit: an object was expected, not an array" },
        { "a root that isn't an object", [] (Doc& d) { d.set ("", ints ({ 1 })); }, "kit.json: the manifest must be an object, not an array" },
        // kit.
        { "kit id", [] (Doc& d) { d.set ("kit.id", str ("fut01")); }, "kit.json: kit.id: ", "[A-Z]{3}[0-9]{2}" },
        { "kit id length", [] (Doc& d) { d.set ("kit.id", str ("FUT001")); }, "kit.json: kit.id: " },
        { "an empty title", [] (Doc& d) { d.set ("kit.title", str ("")); }, "kit.json: kit.title: ", "1 to 64" },
        { "a long title", [] (Doc& d) { d.set ("kit.title", str (std::string (65, 'x'))); }, "kit.json: kit.title: ", "65" },
        { "a control character", [] (Doc& d) { d.set ("kit.direction", str ("future\tgarage")); }, "kit.json: kit.direction: \"future\\u0009garage\" holds a control character" },
        { "a C1 control in a name", [] (Doc& d) { d.set ("kit.title", str ("Signal\xC2\x85")); }, "kit.json: kit.title: \"Signal\\u0085\" holds a control character" },
        { "a duplicated key with a newline in it", [] (Doc& d) {
             d.add ("kit", "a\nb", integer (1));
             d.add ("kit", "a\nb", integer (2));
         }, "kit.json: kit: line ", "\"a\\u000ab\" twice" },
        { "bpm low", [] (Doc& d) { d.set ("kit.bpm", integer (127)); }, "kit.json: kit.bpm: 127 is out of range (128 to 140)" },
        { "bpm high", [] (Doc& d) { d.set ("kit.bpm", integer (141)); }, "kit.json: kit.bpm: 141 is out of range (128 to 140)" },
        { "key", [] (Doc& d) { d.set ("kit.key", str ("H minor")); }, "kit.json: kit.key: \"H minor\"" },
        { "key mode", [] (Doc& d) { d.set ("kit.key", str ("F# dorian")); }, "kit.json: kit.key: " },
        { "camelot 13", [] (Doc& d) { d.set ("kit.camelot", str ("13A")); }, "kit.json: kit.camelot: \"13A\"" },
        { "camelot 0", [] (Doc& d) { d.set ("kit.camelot", str ("0B")); }, "kit.json: kit.camelot: " },
        { "camelot 04", [] (Doc& d) { d.set ("kit.camelot", str ("04A")); }, "kit.json: kit.camelot: " },
        { "camelot C", [] (Doc& d) { d.set ("kit.camelot", str ("11C")); }, "kit.json: kit.camelot: " },
        { "swing", [] (Doc& d) { d.set ("kit.swing_pm", integer (751)); }, "kit.json: kit.swing_pm: 751 is out of range (500 to 750)" },
        { "content version", [] (Doc& d) { d.set ("kit.content_version", integer (0)); }, "kit.json: kit.content_version: 0 is out of range (1 to 999)" },
        { "a lane missing", [] (Doc& d) { d.erase ("kit.lanes.perc"); }, "kit.json: kit.lanes: missing \"perc\"" },
        { "an unknown lane", [] (Doc& d) { d.add ("kit.lanes", "pads", str ("")); }, "kit.json: kit.lanes: unknown key \"pads\"" },
        { "a lane label with a control character", [] (Doc& d) { d.set ("kit.lanes.vox", str ("Vox\n")); }, "kit.json: kit.lanes.vox: " },
        { "an A/B with one label", [] (Doc& d) { d.set ("kit.ab.stab.a", str ("Stab A")); }, "kit.json: kit.ab.stab: ", "both" },
        { "an A/B missing", [] (Doc& d) { d.erase ("kit.ab.stab"); }, "kit.json: kit.ab: missing \"stab\"" },
        // files.
        { "no files", [] (Doc& d) { d.set ("files", array ({})); }, "kit.json: " },
        { "a path with ..", [] (Doc& d) { d.set ("files[3].path", str ("Drums/../../etc/x.wav")); }, "kit.json: files[3].path: " },
        { "an absolute path", [] (Doc& d) { d.set ("files[3].path", str ("/Drums/Kick.wav")); }, "kit.json: files[3].path: " },
        { "an empty part", [] (Doc& d) { d.set ("files[3].path", str ("Drums//Kick.wav")); }, "kit.json: files[3].path: " },
        { "a dot part", [] (Doc& d) { d.set ("files[3].path", str ("./Drums/Kick.wav")); }, "kit.json: files[3].path: " },
        { "a C1 control in a path", [] (Doc& d) { d.set ("files[3].path", str ("Drums/Kick\xC2\x85" "B.wav")); }, "kit.json: files[3].path: \"Drums/Kick\\u0085B.wav\" holds a control character" },
        { "a backslash", [] (Doc& d) { d.set ("files[3].path", str ("Drums\\Kick.wav")); }, "kit.json: files[3].path: ", "\\" },
        { "a drive letter", [] (Doc& d) { d.set ("files[3].path", str ("C:/Drums/Kick.wav")); }, "kit.json: files[3].path: ", ":" },
        { "not .wav", [] (Doc& d) { d.set ("files[3].path", str ("Drums/Kick.flac")); }, "kit.json: files[3].path: ", ".wav" },
        { "a duplicate path", [] (Doc& d) { d.set ("files[3].path", str ("Drums/Kick A.wav")); }, "kit.json: files[3].path: ", "files[0]" },
        { "no frames", [] (Doc& d) { d.set ("files[3].frames", integer (0)); }, "kit.json: files[3].frames: 0 is out of range (1 to 28800000)" },
        { "too many frames", [] (Doc& d) { d.set ("files[3].frames", integer (28800001)); }, "kit.json: files[3].frames: " },
        { "3 channels", [] (Doc& d) { d.set ("files[3].channels", integer (3)); }, "kit.json: files[3].channels: 3 is out of range (1 to 2)" },
        { "an uppercase hash", [] (Doc& d) { d.set ("files[3].sha256", str (std::string (64, 'A'))); }, "kit.json: files[3].sha256: " },
        { "a short hash", [] (Doc& d) { d.set ("files[3].sha256", str (std::string (63, 'a'))); }, "kit.json: files[3].sha256: " },
        { "loop start past the end", [] (Doc& d) { d.set ("files[8].loop_start", integer (48000)); }, "kit.json: files[8].loop_start: 48000 is out of range (0 to 47999)" },
        { "loop end at its start", [] (Doc& d) { d.set ("files[8].loop_end", integer (1000)); }, "kit.json: files[8].loop_end: " },
        { "loop end past the end", [] (Doc& d) { d.set ("files[8].loop_end", integer (48001)); }, "kit.json: files[8].loop_end: " },
        { "no loop but a start", [] (Doc& d) { d.set ("files[0].loop_start", integer (10)); }, "kit.json: files[0].loop_start: ", "loop_end" },
        { "pre-roll", [] (Doc& d) { d.set ("files[13].preroll_frames", integer (48001)); }, "kit.json: files[13].preroll_frames: " },
        { "a file nobody plays", [] (Doc& d) {
             Value f = d.at ("files[0]");
             f.members[0].value = str ("Drums/Spare.wav");
             d.push ("files", std::move (f));
         }, "kit.json: files[18]: ", "isn't played" },
        // instruments.
        { "no instruments", [] (Doc& d) { d.set ("instruments", array ({})); }, "kit.json: " },
        { "an id with capitals", [] (Doc& d) { d.set ("instruments[2].id", str ("Clap")); }, "kit.json: instruments[2].id: \"Clap\"" },
        { "a long id", [] (Doc& d) { d.set ("instruments[2].id", str (std::string (33, 'c'))); }, "kit.json: instruments[2].id: " },
        { "a duplicate id", [] (Doc& d) { d.set ("instruments[2].id", str ("kick_a")); }, "kit.json: instruments[2].id: ", "instruments[0]" },
        { "an unknown lane id", [] (Doc& d) { d.set ("instruments[2].lane", str ("clap")); }, "kit.json: instruments[2].lane: \"clap\" isn't one of kick, snare" },
        { "a mix channel that isn't an id", [] (Doc& d) { d.set ("instruments[2].mix_channel", str ("Snare Bus")); }, "kit.json: instruments[2].mix_channel: " },
        { "a mode", [] (Doc& d) { d.set ("instruments[0].mode", str ("loop")); }, "kit.json: instruments[0].mode: \"loop\" isn't one of one_shot, gated, sustain_loop" },
        { "an align", [] (Doc& d) { d.set ("instruments[0].align", str ("middle")); }, "kit.json: instruments[0].align: " },
        { "attack", [] (Doc& d) { d.set ("instruments[0].attack_us", integer (10000001)); }, "kit.json: instruments[0].attack_us: 10000001 is out of range (0 to 10000000)" },
        { "release", [] (Doc& d) { d.set ("instruments[0].release_us", integer (-1)); }, "kit.json: instruments[0].release_us: -1 is out of range" },
        { "gain", [] (Doc& d) { d.set ("instruments[0].gain_mdb", integer (24001)); }, "kit.json: instruments[0].gain_mdb: 24001 is out of range (-60000 to 24000)" },
        { "start frames past a file", [] (Doc& d) { d.set ("instruments[1].start_frames", integer (4800)); }, "kit.json: instruments[1].start_frames: ", "4799" },
        { "a velocity law", [] (Doc& d) { d.set ("instruments[0].velocity_law", str ("linear")); }, "kit.json: instruments[0].velocity_law: " },
        { "an interp", [] (Doc& d) { d.set ("instruments[0].interp", str ("cubic")); }, "kit.json: instruments[0].interp: " },
        { "a glide without mono", [] (Doc& d) { d.set ("instruments[5].mono", flag (false)); }, "kit.json: instruments[5].glide: ", "mono" },
        { "glide none with a time", [] (Doc& d) { d.set ("instruments[0].glide.us", integer (1000)); }, "kit.json: instruments[0].glide.us: " },
        { "a dip on an exponential glide", [] (Doc& d) { d.set ("instruments[6].glide.dip_us", integer (1000)); }, "kit.json: instruments[6].glide.dip_us: ", "raised_cosine" },
        { "a glide curve", [] (Doc& d) { d.set ("instruments[5].glide.curve", str ("cosine")); }, "kit.json: instruments[5].glide.curve: " },
        { "a glide too long", [] (Doc& d) { d.set ("instruments[5].glide.us", integer (2000001)); }, "kit.json: instruments[5].glide.us: " },
        { "a choke group with no role", [] (Doc& d) { d.set ("instruments[3].choke.role", str ("none")); }, "kit.json: instruments[3].choke: " },
        { "a choke role with no group", [] (Doc& d) { d.set ("instruments[0].choke.role", str ("chokes")); }, "kit.json: instruments[0].choke: " },
        { "a choke group that isn't an id", [] (Doc& d) { d.set ("instruments[3].choke.group", str ("Hats!")); }, "kit.json: instruments[3].choke.group: " },
        { "a choke curve", [] (Doc& d) { d.set ("instruments[3].choke.curve", str ("exp")); }, "kit.json: instruments[3].choke.curve: " },
        { "a choke fade", [] (Doc& d) { d.set ("instruments[3].choke.fade_us", integer (1000001)); }, "kit.json: instruments[3].choke.fade_us: " },
        { "17 pans", [] (Doc& d) { d.set ("instruments[2].pan_cycle_pm", ints ({ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 })); }, "kit.json: instruments[2].pan_cycle_pm: ", "17" },
        { "a pan past the right", [] (Doc& d) { d.set ("instruments[2].pan_cycle_pm", ints ({ 0, 1001 })); }, "kit.json: instruments[2].pan_cycle_pm[1]: 1001 is out of range (-1000 to 1000)" },
        { "rr block", [] (Doc& d) { d.set ("instruments[2].rr_block_16ths", integer (0)); }, "kit.json: instruments[2].rr_block_16ths: 0 is out of range (1 to 4096)" },
        { "layered with nobody", [] (Doc& d) { d.set ("instruments[6].layered_with", str ("subby")); }, "kit.json: instruments[6].layered_with: no instrument \"subby\"" },
        { "layered with itself", [] (Doc& d) { d.set ("instruments[6].layered_with", str ("bass_b")); }, "kit.json: instruments[6].layered_with: ", "itself" },
        { "layered with a layered one", [] (Doc& d) { d.set ("instruments[5].layered_with", str ("bass_b")); }, "kit.json: instruments[5].layered_with: ", "layered" },
        { "no zones", [] (Doc& d) { d.set ("instruments[2].zones", array ({})); }, "kit.json: instruments[2].zones: ", "1 to 32" },
        { "a root past 127", [] (Doc& d) { d.set ("instruments[2].zones[0].root", integer (128)); }, "kit.json: instruments[2].zones[0].root: 128 is out of range (0 to 127)" },
        { "hi under lo", [] (Doc& d) { d.set ("instruments[2].zones[0].hi", integer (38)); }, "kit.json: instruments[2].zones[0].hi: 38 is out of range (39 to 127)" },
        { "overlapping zones", [] (Doc& d) { d.set ("instruments[5].zones[1].lo", integer (36)); }, "kit.json: instruments[5].zones[1]: ", "overlap" },
        { "zones out of order", [] (Doc& d) {
             auto& zones = d.at ("instruments[5].zones").items;
             std::swap (zones[0], zones[1]);
         }, "kit.json: instruments[5].zones[1]: " },
        { "no layers", [] (Doc& d) { d.set ("instruments[2].zones[0].layers", array ({})); }, "kit.json: instruments[2].zones[0].layers: ", "1 to 8" },
        { "layers out of order", [] (Doc& d) { d.set ("instruments[1].zones[0].layers[1].vel", integer (80)); }, "kit.json: instruments[1].zones[0].layers[1].vel: ", "ascending" },
        { "a layer velocity 0", [] (Doc& d) { d.set ("instruments[0].zones[0].layers[0].vel", integer (0)); }, "kit.json: instruments[0].zones[0].layers[0].vel: 0 is out of range (1 to 127)" },
        { "no round robins", [] (Doc& d) { d.set ("instruments[0].zones[0].layers[0].rr", array ({})); }, "kit.json: instruments[0].zones[0].layers[0].rr: " },
        { "9 round robins", [] (Doc& d) { d.set ("instruments[0].zones[0].layers[0].rr", ints ({ 0, 0, 0, 0, 0, 0, 0, 0, 0 })); }, "kit.json: instruments[0].zones[0].layers[0].rr: " },
        { "a dangling file", [] (Doc& d) { d.set ("instruments[1].zones[0].layers[1].rr[0]", integer (18)); }, "kit.json: instruments[1].zones[0].layers[1].rr[0]: file 18 doesn't exist (18 files)" },
        { "a negative file", [] (Doc& d) { d.set ("instruments[1].zones[0].layers[1].rr[1]", integer (-1)); }, "kit.json: instruments[1].zones[0].layers[1].rr[1]: file -1 doesn't exist (18 files)" },
        { "a zone mixing mono and stereo", [] (Doc& d) { d.set ("instruments[1].zones[0].layers[1].rr[1]", integer (6)); }, "kit.json: instruments[1].zones[0]: ", "channels" },
        { "a sustain loop without a loop", [] (Doc& d) { d.set ("instruments[7].mode", str ("sustain_loop")); }, "kit.json: instruments[7]: ", "Bass/Sub Layer.wav" },
        { "pre-rolls that differ", [] (Doc& d) { d.set ("files[2].preroll_frames", integer (100)); }, "kit.json: instruments[1]: ", "preroll" },
        // keymap.
        { "a channel past 16", [] (Doc& d) { d.set ("keymap[2].ch", integer (17)); }, "kit.json: keymap[2].ch: 17 is out of range (1 to 16)" },
        { "channel 3", [] (Doc& d) { d.set ("keymap[5].ch", integer (3)); }, "kit.json: keymap[5].ch: ", "3" },
        { "channel 9", [] (Doc& d) { d.set ("keymap[5].ch", integer (9)); }, "kit.json: keymap[5].ch: ", "9" },
        { "a drum key past 59", [] (Doc& d) { d.set ("keymap[3].keys", ints ({ 46, 60 })); }, "kit.json: keymap[3].keys: ", "35" },
        { "keys reversed", [] (Doc& d) { d.set ("keymap[5].keys", ints ({ 64, 60 })); }, "kit.json: keymap[5].keys: " },
        { "three keys", [] (Doc& d) { d.set ("keymap[5].keys", ints ({ 0, 1, 2 })); }, "kit.json: keymap[5].keys: " },
        { "overlapping ranges", [] (Doc& d) { d.set ("keymap[2].keys", ints ({ 39, 42 })); }, "kit.json: keymap[2].keys: ", "keymap[1]" },
        { "an unknown instrument", [] (Doc& d) { d.set ("keymap[0].b", str ("kick_c")); }, "kit.json: keymap[0].b: no instrument \"kick_c\"" },
        { "no a", [] (Doc& d) { d.set ("keymap[1].a", str ("")); }, "kit.json: keymap[1].a: " },
        { "b without a switch", [] (Doc& d) { d.set ("keymap[0].ab", str ("")); }, "kit.json: keymap[0].ab: " },
        { "a switch without b", [] (Doc& d) { d.set ("keymap[1].ab", str ("kick")); }, "kit.json: keymap[1].ab: " },
        { "an unknown switch", [] (Doc& d) { d.set ("keymap[0].ab", str ("snare")); }, "kit.json: keymap[0].ab: " },
        { "a switch the kit hasn't", [] (Doc& d) { d.set ("keymap[0].ab", str ("stab")); }, "kit.json: keymap[0].ab: ", "kit.ab.stab" },
        { "an instrument nothing plays", [] (Doc& d) { d.erase ("keymap[6]"); }, "kit.json: instruments[10]: ", "keymap" },
        { "a slice key without a multisample", [] (Doc& d) { d.set ("keymap[5].keys", ints ({ 0, 63 })); }, "kit.json: phrases[0].slices[2]: ", "64" },
        { "a phrase channel without a multisample", [] (Doc& d) { d.set ("phrases[0].ch", integer (4)); }, "kit.json: phrases[0]" },
        { "a held key without a multisample", [] (Doc& d) {
             d.set ("keymap[5].keys", ints ({ 0, 68 }));
         }, "kit.json: phrases[0].slices[3].holds[1]: key 69 on channel 2 has no keymap entry" },
        // extras.
        { "an extra on channel 10", [] (Doc& d) { d.set ("extras[0].ch", integer (10)); }, "kit.json: extras[0].ch: 10 is out of range (11 to 16)" },
        { "two extras on one channel", [] (Doc& d) { d.push ("extras", d.at ("extras[0]")); }, "kit.json: extras[1].ch: " },
        { "extras out of order", [] (Doc& d) {
             Value e = d.at ("extras[0]");
             e.members[0].value = integer (12);
             d.at ("extras").items.insert (d.at ("extras").items.begin(), e);
         }, "kit.json: extras[1].ch: " },
        { "a keymap channel with no extra", [] (Doc& d) { d.set ("extras[0].ch", integer (12)); }, "kit.json: keymap[8].ch: ", "extra" },
        { "an extra's instrument on another lane", [] (Doc& d) { d.set ("extras[0].lane", str ("lead")); }, "kit.json: keymap[8].a: ", "chords" },
        { "an extra's name", [] (Doc& d) { d.set ("extras[0].name", str ("")); }, "kit.json: extras[0].name: " },
        // phrases.
        { "a phrase lane", [] (Doc& d) { d.set ("phrases[0].lane", str ("leads")); }, "kit.json: phrases[0].lane: " },
        { "a phrase stem that doesn't exist", [] (Doc& d) { d.set ("phrases[0].stems.b", integer (99)); }, "kit.json: phrases[0].stems.b: file 99 doesn't exist (18 files)" },
        { "stem a -1", [] (Doc& d) { d.set ("phrases[0].stems.a", integer (-1)); }, "kit.json: phrases[0].stems.a: file -1 doesn't exist (18 files)" },
        { "a switch with no stem b", [] (Doc& d) { d.set ("phrases[0].stems.b", integer (-1)); }, "kit.json: phrases[0].ab: " },
        { "a stem b with no switch", [] (Doc& d) { d.set ("phrases[0].ab", str ("")); }, "kit.json: phrases[0].ab: " },
        { "a cycle not of whole bars", [] (Doc& d) { d.set ("phrases[0].cycle_16ths", integer (40)); }, "kit.json: phrases[0].cycle_16ths: ", "16" },
        { "a cycle too long", [] (Doc& d) { d.set ("phrases[0].cycle_16ths", integer (272)); }, "kit.json: phrases[0].cycle_16ths: " },
        { "a tolerance", [] (Doc& d) { d.set ("phrases[0].tolerance_pm", integer (101)); }, "kit.json: phrases[0].tolerance_pm: 101 is out of range (0 to 100)" },
        { "a phrase fade", [] (Doc& d) { d.set ("phrases[0].fade_us", integer (100001)); }, "kit.json: phrases[0].fade_us: " },
        { "a phrase gain", [] (Doc& d) { d.set ("phrases[0].gain_mdb", integer (-60001)); }, "kit.json: phrases[0].gain_mdb: " },
        { "no slices", [] (Doc& d) { d.set ("phrases[0].slices", array ({})); }, "kit.json: phrases[0].slices: " },
        { "a step past the cycle", [] (Doc& d) { d.set ("phrases[0].slices[3].step", integer (32)); }, "kit.json: phrases[0].slices[3].step: 32 is out of range (0 to 31)" },
        { "steps out of order", [] (Doc& d) { d.set ("phrases[0].slices[2].step", integer (2)); }, "kit.json: phrases[0].slices[2].step: ", "ascending" },
        { "a step and key twice", [] (Doc& d) { d.set ("phrases[0].slices[1]", slice (0, 60, 21500, 44000, 43000, 500, ints ({}))); }, "kit.json: phrases[0].slices[1]: ", "slices[0]" },
        { "a slice longer than its stem", [] (Doc& d) { d.set ("phrases[0].slices[3].end", integer (66000 + 190000 + 1)); }, "kit.json: phrases[0].slices[3].end: 256001 is out of range (66001 to 256000): a slice is at most its stem's 190000 frames long" },
        { "an end at its start", [] (Doc& d) { d.set ("phrases[0].slices[3].end", integer (66000)); }, "kit.json: phrases[0].slices[3].end: " },
        { "a fade past the end", [] (Doc& d) { d.set ("phrases[0].slices[3].fade", integer (90001)); }, "kit.json: phrases[0].slices[3].fade: 90001 is out of range (66000 to 90000)" },
        { "a fade before the start", [] (Doc& d) { d.set ("phrases[0].slices[3].fade", integer (65999)); }, "kit.json: phrases[0].slices[3].fade: " },
        { "a negative start", [] (Doc& d) { d.set ("phrases[0].slices[0].start", integer (-1)); }, "kit.json: phrases[0].slices[0].start: " },
        { "a start at the stem's end", [] (Doc& d) { d.set ("phrases[0].slices[0].start", integer (190000)); }, "kit.json: phrases[0].slices[0].start: 190000 is out of range (0 to 189999)" },
        { "a lead", [] (Doc& d) { d.set ("phrases[0].slices[1].lead", integer (48001)); }, "kit.json: phrases[0].slices[1].lead: " },
        { "9 holds", [] (Doc& d) { d.set ("phrases[0].slices[1].holds", ints ({ 1, 2, 3, 4, 5, 6, 7, 8, 9 })); }, "kit.json: phrases[0].slices[1].holds: ", "0 to 8" },
        { "a hold past 127", [] (Doc& d) { d.set ("phrases[0].slices[1].holds", ints ({ 128 })); }, "kit.json: phrases[0].slices[1].holds[0]: 128 is out of range (0 to 127)" },
        { "a hold that isn't a key", [] (Doc& d) { d.set ("phrases[0].slices[1].holds", strs ({ "C4" })); }, "kit.json: phrases[0].slices[1].holds[0]: an integer was expected, not a string" },
        { "a slice key past 127", [] (Doc& d) { d.set ("phrases[0].slices[1].key", integer (128)); }, "kit.json: phrases[0].slices[1].key: " },
        { "a phrase's switch the kit hasn't", [] (Doc& d) { d.set ("phrases[0].ab", str ("stab")); }, "kit.json: phrases[0].ab: ", "kit.ab.stab" },
        // throw: live or baked, always written.
        { "no throw", [] (Doc& d) { d.erase ("phrases[0].throw"); }, "kit.json: phrases[0]: missing \"throw\"" },
        { "a bad throw", [] (Doc& d) { d.set ("phrases[0].throw", str ("both")); }, "kit.json: phrases[0].throw: \"both\" isn't one of live, baked" },
        { "a throw that isn't a string", [] (Doc& d) { d.set ("phrases[0].throw", flag (true)); }, "kit.json: phrases[0].throw: a string was expected, not a boolean" },
        // fade_in_us: 0-100 000, always written.
        { "no fade-in", [] (Doc& d) { d.erase ("phrases[0].fade_in_us"); }, "kit.json: phrases[0]: missing \"fade_in_us\"" },
        { "a negative fade-in", [] (Doc& d) { d.set ("phrases[0].fade_in_us", integer (-1)); }, "kit.json: phrases[0].fade_in_us: -1 is out of range (0 to 100000)" },
        { "a fade-in too long", [] (Doc& d) { d.set ("phrases[0].fade_in_us", integer (100001)); }, "kit.json: phrases[0].fade_in_us: 100001 is out of range (0 to 100000)" },
        { "a fade-in that isn't a number", [] (Doc& d) { d.set ("phrases[0].fade_in_us", str ("500")); }, "kit.json: phrases[0].fade_in_us: an integer was expected, not a string" },
        // Any slice may wrap through its stem's end, but it is at most one stem long (the shorter stem's 190000 frames).
        { "a wrapping slice 0 longer than its stem", [] (Doc& d) {
             d.set ("phrases[0].slices[0].start", integer (190000 - 2400));
             d.set ("phrases[0].slices[0].end", integer (190000 - 2400 + 190000 + 1));
         }, "kit.json: phrases[0].slices[0].end: 377601 is out of range (187601 to 377600)" },
        { "a slice longer than stem A alone", [] (Doc& d) { // without stem B, the stem is A's 192000 frames
             d.set ("phrases[0].stems.b", integer (-1));
             d.set ("phrases[0].ab", str (""));
             d.set ("phrases[0].slices[3].end", integer (66000 + 192000 + 1));
         }, "kit.json: phrases[0].slices[3].end: 258001 is out of range (66001 to 258000)" },
        { "a slice in the middle longer than its stem", [] (Doc& d) { d.set ("phrases[0].slices[2].end", integer (44000 + 190000 + 1)); },
            "kit.json: phrases[0].slices[2].end: 234001 is out of range (44001 to 234000)" },
        // A wrap needs a looped stem: the last slice ends where the next cycle's slice 0 starts, modulo the frames.
        { "a wrapping last slice that misses slice 0", [] (Doc& d) {
             d.set ("phrases[0].slices[0].start", integer (5000));
             d.set ("phrases[0].slices[3].end", integer (194000));
         }, "kit.json: phrases[0].slices[3].end: 194000 is past the stem's 190000 frames, but the phrase isn't looped" },
        { "a wrapping slice 0 whose last slice ends at the stem's end", [] (Doc& d) {
             d.set ("phrases[0].slices[0].start", integer (187600));
             d.set ("phrases[0].slices[0].end", integer (204437));
             d.set ("phrases[0].slices[0].fade", integer (204000));
             d.set ("phrases[0].slices[3].end", integer (190000));
         }, "kit.json: phrases[0].slices[0].end: 204437 is past the stem's 190000 frames, but the phrase isn't looped" },
        { "a fade past a wrapping end", [] (Doc& d) {
             d.set ("phrases[0].slices[3].end", integer (195000));
             d.set ("phrases[0].slices[3].fade", integer (195001));
         }, "kit.json: phrases[0].slices[3].fade: 195001 is out of range (66000 to 195000)" },
        // ride: [] or cycle_16ths x 4 values 0-127 each.
        { "no ride", [] (Doc& d) { d.erase ("phrases[0].ride"); }, "kit.json: phrases[0]: missing \"ride\"" },
        { "a ride that isn't an object", [] (Doc& d) { d.set ("phrases[0].ride", ints ({})); }, "kit.json: phrases[0].ride: an object was expected, not an array" },
        { "a ride without cc71", [] (Doc& d) { d.erase ("phrases[0].ride.cc71"); }, "kit.json: phrases[0].ride: missing \"cc71\"" },
        { "a ride with another controller", [] (Doc& d) { d.add ("phrases[0].ride", "cc1", ints ({})); }, "kit.json: phrases[0].ride: unknown key \"cc1\"" },
        { "a ride one value short", [] (Doc& d) { d.set ("phrases[0].ride.cc74", ride (127)); }, "kit.json: phrases[0].ride.cc74: it holds 127 values", "128" },
        { "a ride one value long", [] (Doc& d) { d.set ("phrases[0].ride.cc71", ride (129)); }, "kit.json: phrases[0].ride.cc71: it holds 129 values", "128" },
        { "a ride for another cycle", [] (Doc& d) { d.set ("phrases[0].ride.cc74", ride (256)); }, "kit.json: phrases[0].ride.cc74: " },
        { "a ride value past 127", [] (Doc& d) {
             auto r = ride (128);
             r.items[100] = integer (128);
             d.set ("phrases[0].ride.cc74", r);
         }, "kit.json: phrases[0].ride.cc74[100]: 128 is out of range (0 to 127)" },
        { "a negative ride value", [] (Doc& d) {
             auto r = ride (128);
             r.items[3] = integer (-1);
             d.set ("phrases[0].ride.cc71", r);
         }, "kit.json: phrases[0].ride.cc71[3]: -1 is out of range (0 to 127)" },
        { "a ride value that isn't a number", [] (Doc& d) {
             auto r = ride (128);
             r.items[0] = str ("64");
             d.set ("phrases[0].ride.cc74", r);
         }, "kit.json: phrases[0].ride.cc74[0]: an integer was expected, not a string" },
        // Phrases that share a channel share one cycle.
        { "two cycles on one channel", [] (Doc& d) {
             Value other = d.at ("phrases[0]");
             d.push ("phrases", std::move (other));
             d.set ("phrases[1].cycle_16ths", integer (64));
         }, "kit.json: phrases[1].cycle_16ths: 64, but phrases[0], also on channel 2, has 32" },
    };
    checkRefusals (test::kKitJson, cases, kitError);
}

TEST_CASE ("parseKitManifest reads rides, and phrases that share a channel and its cycle", "[manifest]")
{
    Doc doc (test::kKitJson);
    doc.set ("phrases[0].ride.cc74", ride (128));
    Value cc71 = ride (128);
    for (auto& v : cc71.items)
        v.integer = 127 - v.integer;
    doc.set ("phrases[0].ride.cc71", cc71);
    // An alternate line on the same channel and cycle, with stem A only and no ride.
    Value other = doc.at ("phrases[0]");
    doc.push ("phrases", std::move (other));
    doc.set ("phrases[1].ab", str (""));
    doc.set ("phrases[1].stems.b", integer (-1));
    doc.set ("phrases[1].ride.cc74", ints ({}));
    doc.set ("phrases[1].ride.cc71", ints ({}));

    const auto parsed = parseKitManifest (doc.text());
    REQUIRE (parsed.error.empty());
    const auto& phrases = parsed.value.phrases;
    REQUIRE (phrases.size() == 2);
    REQUIRE (phrases[0].ride74.size() == 128);
    REQUIRE (phrases[0].ride71.size() == 128);
    for (std::size_t i = 0; i < 128; ++i)
    {
        CHECK (phrases[0].ride74[i] == i);
        CHECK (phrases[0].ride71[i] == 127 - i);
    }
    CHECK (phrases[1].ch == phrases[0].ch);
    CHECK (phrases[1].stemB == -1);
    CHECK (phrases[1].ab == -1);
    CHECK (phrases[1].ride74.empty());
    CHECK (phrases[1].ride71.empty());

    // One controller may ride while the other doesn't; a 4-bar cycle rides 256 values.
    Doc wide (test::kKitJson);
    wide.set ("phrases[0].cycle_16ths", integer (64));
    wide.set ("phrases[0].ride.cc71", ride (256));
    const auto four = parseKitManifest (wide.text());
    REQUIRE (four.error.empty());
    CHECK (four.value.phrases[0].ride74.empty());
    CHECK (four.value.phrases[0].ride71.size() == 256);
}

TEST_CASE ("parseKitManifest: slices wrap through a looped stem's end; throw and fade_in_us", "[manifest]")
{
    // The stems are 192000 and 190000 frames: a slice starts inside the shorter one and is at most 190000 long.
    SECTION ("a wrapping last slice that continues into slice 0, a baked throw, a fade-in")
    {
        // Slice 0 starts 5000 frames in, at its pre-roll; the last slice runs through the stem's end on to frame 5000.
        Doc doc (test::kKitJson);
        doc.set ("phrases[0].throw", str ("baked"));
        doc.set ("phrases[0].fade_in_us", integer (3000));
        doc.set ("phrases[0].slices[0].start", integer (5000));
        doc.set ("phrases[0].slices[3].end", integer (190000 + 5000));
        doc.set ("phrases[0].slices[3].fade", integer (190000 + 4000));
        const auto parsed = parseKitManifest (doc.text());
        REQUIRE (parsed.error.empty());
        const auto& p = parsed.value.phrases[0];
        CHECK (p.throwMode == PhraseThrow::baked);
        CHECK (p.fadeInUs == 3000);
        CHECK (p.slices[3].end == 195000);
        CHECK (p.slices[3].fade == 194000);
        CHECK (p.slices[3].end % 190000 == p.slices[0].start); // it continues into the next cycle's slice 0
    }

    SECTION ("a wrapping slice 0: it starts before its cycle, at frames less its lead (FUT02's riff twice)")
    {
        Doc doc (test::kKitJson);
        doc.set ("phrases[0].slices[0].lead", integer (2400));
        doc.set ("phrases[0].slices[0].start", integer (190000 - 2400));
        doc.set ("phrases[0].slices[0].end", integer (190000 - 2400 + 16837)); // through the stem's end, 14437 frames on
        doc.set ("phrases[0].slices[0].fade", integer (190000 - 2400 + 16000));
        doc.set ("phrases[0].slices[3].end", integer (190000 - 2400)); // the last slice ends where the next cycle's slice 0 starts
        const auto parsed = parseKitManifest (doc.text());
        REQUIRE (parsed.error.empty());
        const auto& s0 = parsed.value.phrases[0].slices[0];
        CHECK (s0.start == 187600);
        CHECK (s0.end == 204437);
        CHECK (s0.end - 190000 == 14437);
    }

    SECTION ("a slice exactly one stem long, and a single slice wrapping to its own start")
    {
        Doc doc (test::kKitJson);
        doc.set ("phrases[0].slices[2].end", integer (44000 + 190000));
        doc.set ("phrases[0].slices[3].end", integer (190000)); // looped: the last slice ends at slice 0's start, 0
        REQUIRE (parseKitManifest (doc.text()).error.empty());

        Doc one (test::kKitJson);
        one.at ("phrases[0].slices").items.resize (1);
        one.set ("phrases[0].slices[0].start", integer (5000));
        one.set ("phrases[0].slices[0].end", integer (195000));
        one.set ("phrases[0].slices[0].holds", ints ({}));
        REQUIRE (parseKitManifest (one.text()).error.empty());
        one.set ("phrases[0].slices[0].end", integer (195001));
        CHECK (parseKitManifest (one.text()).error.starts_with ("kit.json: phrases[0].slices[0].end: 195001 is out of range (5001 to 195000)"));
    }
}

TEST_CASE ("parseKitManifest's syntax errors carry the JSON path", "[manifest]")
{
    // Text the JSON reader refuses before there is a tree: the parser finds the path from the line and column.
    const std::string json (test::kKitJson);
    auto error = [] (std::string text) { return parseKitManifest (text).error; };

    std::string withBom = "\xEF\xBB\xBF" + json;
    CHECK (error (withBom).starts_with ("kit.json: line 1, column 1: "));
    CHECK (error (json + "x").starts_with ("kit.json: line "));
    CHECK (error ("").starts_with ("kit.json: "));

    auto at = json.find ("\"bpm\": 130");
    REQUIRE (at != std::string::npos);
    std::string floatBpm = json;
    floatBpm.replace (at, 10, "\"bpm\": 130.0");
    CHECK (error (floatBpm).starts_with ("kit.json: kit.bpm: line 3, column "));
    CHECK (error (floatBpm).find ("130.0 is not an integer") != std::string::npos);

    // Deep in an array of objects.
    at = json.find ("\"rr\": [3, 4]");
    REQUIRE (at != std::string::npos);
    std::string floatRr = json;
    floatRr.replace (at, 12, "\"rr\": [3, 4.0]");
    CHECK (error (floatRr).starts_with ("kit.json: instruments[1].zones[0].layers[1].rr[1]: line "));

    // A string key that repeats.
    at = json.find ("\"lead\": 500");
    REQUIRE (at != std::string::npos);
    std::string twice = json;
    twice.replace (at, 11, "\"lead\": 500, \"lead\": 500");
    CHECK (error (twice).starts_with ("kit.json: phrases[0].slices[1]: line "));
    CHECK (error (twice).find ("\"lead\" twice") != std::string::npos);

    // Braces and brackets inside strings don't confuse the path.
    at = json.find ("\"title\": \"Test Signal\"");
    REQUIRE (at != std::string::npos);
    std::string tricky = json;
    tricky.replace (at, 22, "\"title\": \"Test [Signal] {x}, \\\"q\\\"\"");
    at = tricky.find ("\"bpm\": 130");
    tricky.replace (at, 10, "\"bpm\": 1e2");
    CHECK (error (tricky).starts_with ("kit.json: kit.bpm: line "));
}

TEST_CASE ("a zone's voicing is read as intervals over its root", "[manifest]")
{
    // Chords as chords (ADR 0027, amended): a chord sample's zone may say which chord it plays.
    Doc doc (test::kKitJson);
    doc.add ("instruments[0].zones[0]", "voicing", ints ({ 0, 7, 10, 14, 15 }));
    const auto parsed = parseKitManifest (doc.text());
    REQUIRE (parsed.error.empty());
    const auto& m = parsed.value;
    CHECK (m.instruments[0].zones[0].voicing == std::vector<std::int32_t> { 0, 7, 10, 14, 15 });

    // A zone without it has none: no chord.
    std::size_t zones = 0;
    for (std::size_t i = 0; i < m.instruments.size(); ++i)
        for (std::size_t j = 0; j < m.instruments[i].zones.size(); ++j)
            if (i != 0 || j != 0)
            {
                CHECK (m.instruments[i].zones[j].voicing.empty());
                ++zones;
            }
    CHECK (zones > 10);
    CHECK (parseKitManifest (test::kKitJson).value.instruments[0].zones[0].voicing.empty());

    // The test side's writer puts it back where plugin_common puts it, after hi, and the round trip holds.
    const auto written = testing::kitManifestJson (m);
    CHECK (written.find ("\"hi\": 36,\n          \"voicing\": [0, 7, 10, 14, 15],\n          \"layers\": [") != std::string::npos);
    const auto again = parseKitManifest (written);
    REQUIRE (again.error.empty());
    CHECK (again.value == m);
    CHECK (testing::kitManifestJson (parseKitManifest (test::kKitJson).value).find ("voicing") == std::string::npos);
}

TEST_CASE ("a zone's voicing is refused unless it rises from 0, in at most 8 steps up to 48, naming its path", "[manifest]")
{
    const auto voicing = [] (Value v) { return [v] (Doc& d) { d.add ("instruments[0].zones[0]", "voicing", v); }; };
    const std::vector<Bad> cases {
        { "no intervals", voicing (ints ({})), "kit.json: instruments[0].zones[0].voicing: ", "it holds 0 intervals: it may hold 1 to 8" },
        { "nine intervals", voicing (ints ({ 0, 2, 3, 5, 7, 9, 10, 12, 14 })), "kit.json: instruments[0].zones[0].voicing: ", "it holds 9 intervals" },
        { "not from 0", voicing (ints ({ 1, 5 })), "kit.json: instruments[0].zones[0].voicing[0]: ", "starts at 0" },
        { "descending", voicing (ints ({ 7, 0 })), "kit.json: instruments[0].zones[0].voicing[0]: ", "starts at 0" },
        { "a repeat", voicing (ints ({ 0, 7, 7 })), "kit.json: instruments[0].zones[0].voicing[2]: ", "7 after 7" },
        { "down after up", voicing (ints ({ 0, 10, 7 })), "kit.json: instruments[0].zones[0].voicing[2]: ", "7 after 10" },
        { "past 48", voicing (ints ({ 0, 49 })), "kit.json: instruments[0].zones[0].voicing[1]: ", "49 is out of range (0 to 48)" },
        { "below 0", voicing (ints ({ 0, -5 })), "kit.json: instruments[0].zones[0].voicing[1]: ", "-5 is out of range (0 to 48)" },
        { "a float", [] (Doc& d) {
             d.add ("instruments[0].zones[0]", "voicing", ints ({ 0 }));
             d.setRaw ("instruments[0].zones[0].voicing[0]", "0.5");
         }, "kit.json: instruments[0].zones[0].voicing[0]: line ", "0.5 is not an integer" },
        { "a string", voicing (strs ({ "0" })), "kit.json: instruments[0].zones[0].voicing[0]: ", "an integer was expected, not a string" },
        { "not an array", voicing (integer (0)), "kit.json: instruments[0].zones[0].voicing: ", "an array was expected, not an integer" },
    };
    checkRefusals (test::kKitJson, cases, kitError);

    // Every error names the path: instruments[i].zones[j].voicing.
    Doc doc (test::kKitJson);
    doc.add ("instruments[5].zones[1]", "voicing", ints ({ 0, 7, 7 }));
    CHECK (kitError (doc.text()).starts_with ("kit.json: instruments[5].zones[1].voicing[2]: "));
}

TEST_CASE ("parseMixManifest reads every field", "[manifest]")
{
    const auto kit = testKit();
    const auto parsed = parseMixManifest (test::kMixJson, kit);
    REQUIRE (parsed.error.empty());
    const auto& mix = parsed.value;

    CHECK (mix.kit == "TST01");
    CHECK (mix.contentVersion == 3);
    REQUIRE (mix.channels.size() == 9);
    CHECK (mix.channelIndex ("sub") == 4);
    const auto& kick = mix.channels[0];
    CHECK (kick.id == "kick");
    CHECK (kick.lane == static_cast<std::uint8_t> (Lane::kick));
    CHECK (kick.group == Group::drums);
    CHECK (kick.sendsPm == std::array<std::int32_t, 3> { 0, 0, 0 });
    REQUIRE (kick.inserts.size() == 2);
    CHECK (kick.inserts[0] == Insert { InsertType::gain, {}, GainParams { -1500 } });
    CHECK (kick.inserts[1] == Insert { InsertType::clip, {}, ClipParams { -3000, 1500 } });

    const auto& snare = mix.channels[1];
    CHECK (snare.sendsPm == std::array<std::int32_t, 3> { 120, 0, 0 });
    CHECK (snare.inserts[0] == Insert { InsertType::pan, {}, PanParams { 100 } });
    CHECK (snare.inserts[1] == Insert { InsertType::peak, {}, EqParams { 200, -2000, 1400 } });
    DuckParams lift;
    lift.mode = DuckMode::onset;
    lift.keyLanes = static_cast<std::uint16_t> (laneBit (Lane::kick));
    lift.minVelocity = 1;
    lift.depthMdb = -2000;
    lift.holdUs = 5000;
    lift.releaseUs = 40000;
    lift.preUs = 3000;
    CHECK (snare.inserts[2] == Insert { InsertType::duck, {}, lift });

    const auto& hats = mix.channels[2];
    CHECK (hats.inserts[0] == Insert { InsertType::highpass, {}, PassParams { 300, 2, 0, 1, {} } });
    CHECK (hats.inserts[1] == Insert { InsertType::width, {}, WidthParams { 1300, 0, 2 } });

    const auto& bass = mix.channels[3];
    CHECK (bass.group == Group::music);
    CHECK (bass.inserts[0] == Insert { InsertType::ccFilter, {}, CcFilterParams { FilterResponse::lowpass, 650, 2210, 127, 707, 1600, 0, { 120, 0 } } });
    const auto& pump = std::get<DuckParams> (bass.inserts[1].params);
    CHECK (pump.keyLanes == (laneBit (Lane::kick) | laneBit (Lane::snare)));
    CHECK (pump.depthMdb == 6000);
    CHECK (pump.rampUs == 2000);
    CHECK (pump.band == Band { 0, 120 });
    CHECK (pump.pump);
    CHECK (bass.inserts[2] == Insert { InsertType::monoBelow, {}, MonoBelowParams { 150, 8 } });

    CHECK (mix.channels[4].inserts[0] == Insert { InsertType::lowpass, {}, PassParams { 120, 4, 0, 2, {} } });
    CHECK (std::get<DuckParams> (mix.channels[4].inserts[1].params).minVelocity == 20);

    const auto& lead = mix.channels[5];
    CHECK (lead.sendsPm == std::array<std::int32_t, 3> { 150, 250, 100 });
    CHECK (lead.inserts[0] == Insert { InsertType::highShelf, {}, EqParams { 8000, 2000, 707 } });
    CHECK (lead.inserts[1] == Insert { InsertType::drive, When { 1, -1, 0 }, DriveParams { 2000, 100, -1000 } });

    const auto& chords = mix.channels[6];
    CHECK (chords.inserts[0] == Insert { InsertType::lowShelf, {}, EqParams { 200, -3000, 707 } });
    CHECK (chords.inserts[1] == Insert { InsertType::comp, {}, CompParams { -18000, 2000, 6000, 30000, 150000, 1500000, 2000 } });
    const auto& parallel = std::get<ParallelParams> (chords.inserts[2].params);
    CHECK (chords.inserts[2].type == InsertType::parallel);
    CHECK (parallel.levelMdb == -6000);
    REQUIRE (parallel.chain.size() == 2);
    CHECK (parallel.chain[0] == Insert { InsertType::drive, {}, DriveParams { 4000, 0, -6000 } });
    CHECK (parallel.chain[1] == Insert { InsertType::highpass, {}, PassParams { 200, 2, 900, 1, { 0, 8000 } } });

    const auto& vox = mix.channels[7];
    CHECK (vox.group == Group::none);
    CHECK (vox.inserts[0] == Insert { InsertType::gain, When { -1, static_cast<std::int8_t> (AbSwitch::bass), 1 }, GainParams { 1000 } });

    const auto& fx = mix.channels[8].inserts[0];
    CHECK (fx.when == When { 0, static_cast<std::int8_t> (AbSwitch::kick), 0 });
    const auto& level = std::get<DuckParams> (fx.params);
    CHECK (level.mode == DuckMode::level);
    CHECK (level.keyChannel == 7);
    CHECK (level.depthMdb == 4000);
    CHECK (level.rangeMdb == 20000);
    CHECK (level.topMdb == -6000);
    CHECK (level.attackUs == 5000);
    CHECK (level.releaseUs == 150000);
    CHECK_FALSE (level.pump);

    REQUIRE (mix.drums.inserts.size() == 1);
    CHECK (mix.drums.inserts[0].type == InsertType::comp);
    REQUIRE (mix.music.inserts.size() == 1);

    CHECK (mix.space.delaysFrames == std::array<std::int32_t, 8> { 1031, 1327, 1523, 1801, 2063, 2341, 2617, 2903 });
    CHECK (mix.space.signs == std::array<std::int32_t, 8> { 1, -1, 1, -1, 1, 1, -1, -1 });
    CHECK (mix.space.t60Ms == 2200);
    CHECK (mix.space.t60LowMs == 1500);
    CHECK (mix.space.lowHz == 250);
    CHECK (mix.space.dampHz == 7000);
    CHECK (mix.space.predelayUs == 20000);
    CHECK (mix.space.sendHpHz == 200);
    CHECK (mix.space.sendOrder == 4);
    CHECK (mix.space.shimmerPm == 120);
    CHECK (mix.space.shimmerLoHz == 1500);
    CHECK (mix.space.shimmerHiHz == 9000);
    CHECK (mix.space.windowFrames == 2048);
    CHECK (mix.space.monoHz == 150);
    CHECK (mix.space.returnMdb == -6000);
    REQUIRE (mix.space.inserts.size() == 1);
    CHECK (std::get<DuckParams> (mix.space.inserts[0].params).keyChannel == 7);

    CHECK (mix.echo.steps == 3);
    CHECK (mix.echo.feedbackPm == 450);
    CHECK (mix.echo.lpHz == 6000);
    CHECK (mix.echo.hpHz == 300);
    CHECK (mix.echo.drivePm == 1500);
    CHECK (mix.echo.wowPpm == 400);
    CHECK (mix.echo.wowMhz == 500);
    CHECK (mix.echo.flutterPpm == 100);
    CHECK (mix.echo.flutterMhz == 6000);
    CHECK (mix.echo.phasesMrad == std::array<std::int32_t, 2> { 0, 3142 });
    CHECK (mix.echo.returnMdb == -9000);
    CHECK (mix.echo.inserts.empty());

    CHECK (mix.resonator.tonesMhz == std::vector<std::int32_t> { 46250, 92500, 138750 });
    CHECK (mix.resonator.tauMs == 400);
    CHECK (mix.resonator.sendHpHz == 200);
    CHECK (mix.resonator.returnMdb == -12000);

    REQUIRE (mix.busEq[0].size() == 1);
    CHECK (mix.busEq[0][0] == Insert { InsertType::peak, {}, EqParams { 2500, -1000, 800 } });
    CHECK (mix.busEq[1].empty());

    for (const auto& master : mix.master)
    {
        CHECK (master.lowcutHz == 25);
        CHECK (master.lowcutOrder == 4);
        CHECK (master.monoBelowHz == 150);
        CHECK (master.glueRatioPm == 2000);
        CHECK (master.glueKneeMdb == 6000);
        CHECK (master.glueAttackUs == 30000);
        CHECK (master.glueReleaseFastUs == 150000);
        CHECK (master.glueReleaseSlowUs == 1500000);
        CHECK (master.clipKneeMdb == 1500);
        CHECK (master.clipCeilingMdb == -520);
        CHECK (master.limiterCeilingMdb == -1020);
        CHECK (master.limiterLookaheadUs == 5000);
        CHECK (master.limiterHoldUs == 60000);
        CHECK (master.limiterReleaseUs == 80000);
        CHECK (master.dcHz == 5);
    }
    CHECK (mix.master[0].glueThresholdMdb == -14000);
    CHECK (mix.master[0].driveMdb == 2000);
    CHECK (mix.master[1].glueThresholdMdb == -15000);
    CHECK (mix.master[1].driveMdb == 2500);

    CHECK (mix.reference[0] == Reference { -14200, -9730, { -1000, -3000, -9000, -4000, -6000, -7000, -8000, -5000, -12000 } });
    CHECK (mix.reference[1].channelTruePeakMdb[7] == -120000);
}

TEST_CASE ("parseMixManifest refuses what the contract doesn't allow, naming the path", "[manifest]")
{
    auto onsetDuck = [] (std::string_view lane, std::int64_t pre) {
        Value d = parsed (R"({ "type": "duck", "mode": "onset", "key_lanes": ["kick"], "min_velocity": 1, "depth_mdb": 3000,
            "ramp_us": 0, "hold_us": 0, "release_us": 50000, "pre_us": 0, "band": { "lo_hz": 0, "hi_hz": 0 }, "pump": false })");
        d.members[2].value = strs ({ lane });
        d.members[8].value = integer (pre);
        return d;
    };
    const std::vector<Bad> cases {
        // The file and its kit.
        { "a float", [] (Doc& d) { d.setRaw ("channels[3].inserts[0].cc74_lo_hz", "650.5"); }, "mix.json: channels[3].inserts[0].cc74_lo_hz: line ", "not an integer" },
        { "another kit", [] (Doc& d) { d.set ("kit", str ("TST02")); }, "mix.json: kit: ", "TST01" },
        { "another version", [] (Doc& d) { d.set ("content_version", integer (4)); }, "mix.json: content_version: ", "3" },
        { "an unknown key", [] (Doc& d) { d.add ("", "notes", str ("x")); }, "mix.json: unknown key \"notes\"" },
        { "a missing key", [] (Doc& d) { d.erase ("bus_eq"); }, "mix.json: missing \"bus_eq\"" },
        // Channels.
        { "no channels", [] (Doc& d) { d.set ("channels", array ({})); }, "mix.json: channels: " },
        { "25 channels", [] (Doc& d) {
             for (int i = 0; i < 16; ++i)
             {
                 Value c = d.at ("channels[8]");
                 c.members[0].value = str ("fx_" + std::to_string (i));
                 d.push ("channels", std::move (c));
             }
         }, "mix.json: channels: ", "24" },
        { "a channel id twice", [] (Doc& d) { d.set ("channels[4].id", str ("bass")); }, "mix.json: channels[4].id: ", "channels[3]" },
        { "a channel id with capitals", [] (Doc& d) { d.set ("channels[4].id", str ("Sub")); }, "mix.json: channels[4].id: " },
        { "a channel kit.json names, gone", [] (Doc& d) { d.set ("channels[4].id", str ("subby")); }, "mix.json: channels: ", "instruments[7].mix_channel" },
        { "a phrase's channel, gone", [] (Doc& d) {
             d.set ("channels[5].id", str ("leads"));
         }, "mix.json: channels: ", "mix_channel" },
        { "a channel on another lane than its instrument", [] (Doc& d) { d.set ("channels[4].lane", str ("perc")); }, "mix.json: channels[4].lane: ", "instruments[7]" },
        { "an unknown lane", [] (Doc& d) { d.set ("channels[4].lane", str ("subs")); }, "mix.json: channels[4].lane: " },
        { "a group", [] (Doc& d) { d.set ("channels[4].group", str ("bass")); }, "mix.json: channels[4].group: \"bass\" isn't one of none, drums, music" },
        { "two sends", [] (Doc& d) { d.set ("channels[4].sends_pm", ints ({ 0, 0 })); }, "mix.json: channels[4].sends_pm: " },
        { "a send past 1000", [] (Doc& d) { d.set ("channels[4].sends_pm", ints ({ 0, 1001, 0 })); }, "mix.json: channels[4].sends_pm[1]: 1001 is out of range (0 to 1000)" },
        { "17 inserts", [] (Doc& d) {
             for (int i = 0; i < 15; ++i)
                 d.push ("channels[0].inserts", d.at ("channels[0].inserts[0]"));
         }, "mix.json: channels[0].inserts: ", "16" },
        // Inserts.
        { "an unknown type", [] (Doc& d) { d.set ("channels[0].inserts[0].type", str ("reverb")); }, "mix.json: channels[0].inserts[0].type: \"reverb\" isn't one of gain, pan" },
        { "no type", [] (Doc& d) { d.erase ("channels[0].inserts[0].type"); }, "mix.json: channels[0].inserts[0]: missing \"type\"" },
        { "an unknown insert key", [] (Doc& d) { d.add ("channels[0].inserts[0]", "q_pm", integer (707)); }, "mix.json: channels[0].inserts[0]: unknown key \"q_pm\"" },
        { "a missing insert key", [] (Doc& d) { d.erase ("channels[1].inserts[1].q_pm"); }, "mix.json: channels[1].inserts[1]: missing \"q_pm\"" },
        { "a gain", [] (Doc& d) { d.set ("channels[0].inserts[0].gain_mdb", integer (24001)); }, "mix.json: channels[0].inserts[0].gain_mdb: 24001 is out of range (-60000 to 24000)" },
        { "a pan", [] (Doc& d) { d.set ("channels[1].inserts[0].pan_pm", integer (-1001)); }, "mix.json: channels[1].inserts[0].pan_pm: " },
        { "a width", [] (Doc& d) { d.set ("channels[2].inserts[1].width_pm", integer (4001)); }, "mix.json: channels[2].inserts[1].width_pm: " },
        { "a width order", [] (Doc& d) { d.set ("channels[2].inserts[1].order", integer (9)); }, "mix.json: channels[2].inserts[1].order: " },
        { "a peak frequency", [] (Doc& d) { d.set ("channels[1].inserts[1].freq_hz", integer (9)); }, "mix.json: channels[1].inserts[1].freq_hz: 9 is out of range (10 to 24000)" },
        { "a peak q", [] (Doc& d) { d.set ("channels[1].inserts[1].q_pm", integer (99)); }, "mix.json: channels[1].inserts[1].q_pm: " },
        { "a peak gain", [] (Doc& d) { d.set ("channels[1].inserts[1].gain_mdb", integer (30001)); }, "mix.json: channels[1].inserts[1].gain_mdb: " },
        { "a highpass q with order 4", [] (Doc& d) { d.set ("channels[4].inserts[0].q_pm", integer (900)); }, "mix.json: channels[4].inserts[0].q_pm: ", "order 2" },
        { "a highpass q under 100", [] (Doc& d) { d.set ("channels[7].inserts[1].q_pm", integer (50)); }, "mix.json: channels[7].inserts[1].q_pm: " },
        { "three stages", [] (Doc& d) { d.set ("channels[4].inserts[0].stages", integer (3)); }, "mix.json: channels[4].inserts[0].stages: 3 is out of range (1 to 2)" },
        { "a band upside down", [] (Doc& d) { d.set ("channels[6].inserts[2].inserts[1].band.lo_hz", integer (9000)); }, "mix.json: channels[6].inserts[2].inserts[1].band: " },
        { "a band key", [] (Doc& d) { d.erase ("channels[3].inserts[0].band.hi_hz"); }, "mix.json: channels[3].inserts[0].band: missing \"hi_hz\"" },
        { "a cc_filter response", [] (Doc& d) { d.set ("channels[3].inserts[0].response", str ("notch")); }, "mix.json: channels[3].inserts[0].response: " },
        { "a cc74 range upside down", [] (Doc& d) { d.set ("channels[3].inserts[0].cc74_hi_hz", integer (650)); }, "mix.json: channels[3].inserts[0].cc74_hi_hz: 650 is out of range (651 to 24000)" },
        { "a cc74 default", [] (Doc& d) { d.set ("channels[3].inserts[0].cc74_default", integer (128)); }, "mix.json: channels[3].inserts[0].cc74_default: " },
        { "a cc71 q", [] (Doc& d) { d.set ("channels[3].inserts[0].cc71_hi_pm", integer (30001)); }, "mix.json: channels[3].inserts[0].cc71_hi_pm: " },
        { "two cc_filters on a channel", [] (Doc& d) { d.push ("channels[3].inserts", d.at ("channels[3].inserts[0]")); }, "mix.json: channels[3].inserts[3]: ", "one cc_filter" },
        { "a cc_filter on a group", [] (Doc& d) { d.push ("groups.music.inserts", d.at ("channels[3].inserts[0]")); }, "mix.json: groups.music.inserts[1]: ", "channels" },
        { "a cc_filter on a return", [] (Doc& d) { d.push ("returns.echo.inserts", d.at ("channels[3].inserts[0]")); }, "mix.json: returns.echo.inserts[0]: " },
        { "a cc_filter on the bus", [] (Doc& d) { d.push ("bus_eq.novox", d.at ("channels[3].inserts[0]")); }, "mix.json: bus_eq.novox[0]: " },
        { "a cc_filter in a parallel chain", [] (Doc& d) { d.push ("channels[6].inserts[2].inserts", d.at ("channels[3].inserts[0]")); }, "mix.json: channels[6].inserts[2].inserts[2]: " },
        { "a parallel in a parallel", [] (Doc& d) { d.push ("channels[6].inserts[2].inserts", d.at ("channels[6].inserts[2]")); }, "mix.json: channels[6].inserts[2].inserts[2]: ", "parallel" },
        { "an empty parallel", [] (Doc& d) { d.set ("channels[6].inserts[2].inserts", array ({})); }, "mix.json: channels[6].inserts[2].inserts: " },
        { "a parallel level", [] (Doc& d) { d.set ("channels[6].inserts[2].level_mdb", integer (12001)); }, "mix.json: channels[6].inserts[2].level_mdb: " },
        { "a mono_below frequency", [] (Doc& d) { d.set ("channels[3].inserts[2].freq_hz", integer (19)); }, "mix.json: channels[3].inserts[2].freq_hz: 19 is out of range (20 to 2000)" },
        { "a clip ceiling", [] (Doc& d) { d.set ("channels[0].inserts[1].ceiling_mdb", integer (1)); }, "mix.json: channels[0].inserts[1].ceiling_mdb: " },
        { "a clip knee", [] (Doc& d) { d.set ("channels[0].inserts[1].knee_mdb", integer (99)); }, "mix.json: channels[0].inserts[1].knee_mdb: " },
        { "a drive", [] (Doc& d) { d.set ("channels[5].inserts[1].drive_pm", integer (99)); }, "mix.json: channels[5].inserts[1].drive_pm: " },
        { "a drive bias", [] (Doc& d) { d.set ("channels[5].inserts[1].bias_pm", integer (2001)); }, "mix.json: channels[5].inserts[1].bias_pm: " },
        { "a comp ratio", [] (Doc& d) { d.set ("channels[6].inserts[1].ratio_pm", integer (1000)); }, "mix.json: channels[6].inserts[1].ratio_pm: 1000 is out of range (1001 to 100000)" },
        { "a comp threshold", [] (Doc& d) { d.set ("channels[6].inserts[1].threshold_mdb", integer (1)); }, "mix.json: channels[6].inserts[1].threshold_mdb: " },
        // Ducks.
        { "a duck mode", [] (Doc& d) { d.set ("channels[1].inserts[2].mode", str ("sidechain")); }, "mix.json: channels[1].inserts[2].mode: " },
        { "an onset duck with a level key", [] (Doc& d) { d.add ("channels[1].inserts[2]", "key_channel", str ("vox")); }, "mix.json: channels[1].inserts[2]: unknown key \"key_channel\"" },
        { "a level duck with onset keys", [] (Doc& d) { d.set ("channels[8].inserts[0].mode", str ("onset")); }, "mix.json: channels[8].inserts[0]: " },
        { "a duck of depth 0", [] (Doc& d) { d.set ("channels[1].inserts[2].depth_mdb", integer (0)); }, "mix.json: channels[1].inserts[2].depth_mdb: ", "0" },
        { "a duck too deep", [] (Doc& d) { d.set ("channels[1].inserts[2].depth_mdb", integer (60001)); }, "mix.json: channels[1].inserts[2].depth_mdb: " },
        { "no key lanes", [] (Doc& d) { d.set ("channels[1].inserts[2].key_lanes", strs ({})); }, "mix.json: channels[1].inserts[2].key_lanes: " },
        { "a key lane twice", [] (Doc& d) { d.set ("channels[1].inserts[2].key_lanes", strs ({ "kick", "kick" })); }, "mix.json: channels[1].inserts[2].key_lanes[1]: " },
        { "an unknown key lane", [] (Doc& d) { d.set ("channels[1].inserts[2].key_lanes", strs ({ "kik" })); }, "mix.json: channels[1].inserts[2].key_lanes[0]: " },
        { "a min velocity 0", [] (Doc& d) { d.set ("channels[1].inserts[2].min_velocity", integer (0)); }, "mix.json: channels[1].inserts[2].min_velocity: " },
        { "a release under 1 ms", [] (Doc& d) { d.set ("channels[1].inserts[2].release_us", integer (999)); }, "mix.json: channels[1].inserts[2].release_us: " },
        { "pre too long", [] (Doc& d) { d.set ("channels[1].inserts[2].pre_us", integer (100001)); }, "mix.json: channels[1].inserts[2].pre_us: " },
        { "two pre-rolls on one lane", [] (Doc& d) { d.set ("channels[4].inserts[1].pre_us", integer (4000)); }, "mix.json: channels[4].inserts[1].pre_us: ", "3000" },
        { "two pre-rolls on one lane in a group", [onsetDuck] (Doc& d) { d.push ("groups.music.inserts", onsetDuck ("snare", 0)); }, "mix.json: groups.music.inserts[1].pre_us: " },
        { "a level duck keyed on itself", [] (Doc& d) { d.set ("channels[8].inserts[0].key_channel", str ("fx")); }, "mix.json: channels[8].inserts[0].key_channel: ", "own" },
        { "a level duck keyed on nothing", [] (Doc& d) { d.set ("channels[8].inserts[0].key_channel", str ("voxx")); }, "mix.json: channels[8].inserts[0].key_channel: no channel \"voxx\"" },
        { "a return's level duck keyed on nothing", [] (Doc& d) { d.set ("returns.space.inserts[0].key_channel", str ("voxx")); }, "mix.json: returns.space.inserts[0].key_channel: " },
        { "a level duck's range", [] (Doc& d) { d.set ("channels[8].inserts[0].range_mdb", integer (999)); }, "mix.json: channels[8].inserts[0].range_mdb: " },
        { "a level duck's top", [] (Doc& d) { d.set ("channels[8].inserts[0].top_mdb", integer (12001)); }, "mix.json: channels[8].inserts[0].top_mdb: " },
        { "a level duck's attack", [] (Doc& d) { d.set ("channels[8].inserts[0].attack_us", integer (99)); }, "mix.json: channels[8].inserts[0].attack_us: " },
        // when.
        { "a when that restricts nothing", [] (Doc& d) { d.set ("channels[5].inserts[1].when.variant", str ("any")); }, "mix.json: channels[5].inserts[1].when: " },
        { "a when variant", [] (Doc& d) { d.set ("channels[5].inserts[1].when.variant", str ("instrumental")); }, "mix.json: channels[5].inserts[1].when.variant: " },
        { "a when switch", [] (Doc& d) { d.set ("channels[7].inserts[0].when.ab", str ("snare")); }, "mix.json: channels[7].inserts[0].when.ab: " },
        { "a when side", [] (Doc& d) { d.set ("channels[7].inserts[0].when.is", str ("c")); }, "mix.json: channels[7].inserts[0].when.is: " },
        { "a when missing is", [] (Doc& d) { d.erase ("channels[7].inserts[0].when.is"); }, "mix.json: channels[7].inserts[0].when: missing \"is\"" },
        { "a when key", [] (Doc& d) { d.add ("channels[7].inserts[0].when", "mode", str ("loop")); }, "mix.json: channels[7].inserts[0].when: unknown key \"mode\"" },
        // Groups, returns, the bus.
        { "a third group", [] (Doc& d) { d.add ("groups", "fx", d.at ("groups.music")); }, "mix.json: groups: unknown key \"fx\"" },
        { "a group key", [] (Doc& d) { d.add ("groups.drums", "gain_mdb", integer (0)); }, "mix.json: groups.drums: unknown key \"gain_mdb\"" },
        { "seven delays", [] (Doc& d) { d.set ("returns.space.delays_frames", ints ({ 1, 2, 3, 4, 5, 6, 7 })); }, "mix.json: returns.space.delays_frames: " },
        { "a delay of 0", [] (Doc& d) { d.at ("returns.space.delays_frames").items[3] = integer (0); }, "mix.json: returns.space.delays_frames[3]: 0 is out of range (1 to 48000)" },
        { "a sign of 0", [] (Doc& d) { d.at ("returns.space.signs").items[2] = integer (0); }, "mix.json: returns.space.signs[2]: " },
        { "a t60", [] (Doc& d) { d.set ("returns.space.t60_ms", integer (99)); }, "mix.json: returns.space.t60_ms: " },
        { "a shimmer band upside down", [] (Doc& d) { d.set ("returns.space.shimmer_lo_hz", integer (9000)); }, "mix.json: returns.space.shimmer_lo_hz: " },
        { "a shimmer window", [] (Doc& d) { d.set ("returns.space.window_frames", integer (63)); }, "mix.json: returns.space.window_frames: " },
        { "echo steps", [] (Doc& d) { d.set ("returns.echo.steps", integer (17)); }, "mix.json: returns.echo.steps: " },
        { "echo feedback", [] (Doc& d) { d.set ("returns.echo.feedback_pm", integer (951)); }, "mix.json: returns.echo.feedback_pm: " },
        { "echo phases", [] (Doc& d) { d.set ("returns.echo.phases_mrad", ints ({ 0, 6285 })); }, "mix.json: returns.echo.phases_mrad[1]: " },
        { "25 tones", [] (Doc& d) {
             std::vector<Value> tones;
             for (int i = 0; i < 25; ++i)
                 tones.push_back (integer (50000 + i));
             d.set ("returns.resonator.tones_mhz", array (std::move (tones)));
         }, "mix.json: returns.resonator.tones_mhz: " },
        { "a tone too low", [] (Doc& d) { d.at ("returns.resonator.tones_mhz").items[0] = integer (19999); }, "mix.json: returns.resonator.tones_mhz[0]: " },
        { "a return gain", [] (Doc& d) { d.set ("returns.resonator.return_mdb", integer (12001)); }, "mix.json: returns.resonator.return_mdb: " },
        { "a bus without novox", [] (Doc& d) { d.erase ("bus_eq.novox"); }, "mix.json: bus_eq: missing \"novox\"" },
        // master and reference.
        { "a low cut", [] (Doc& d) { d.set ("master.lowcut_hz", integer (201)); }, "mix.json: master.lowcut_hz: " },
        { "a glue ratio", [] (Doc& d) { d.set ("master.glue.ratio_pm", integer (20001)); }, "mix.json: master.glue.ratio_pm: " },
        { "a glue threshold", [] (Doc& d) { d.set ("master.novox.glue_threshold_mdb", integer (1)); }, "mix.json: master.novox.glue_threshold_mdb: " },
        { "a master drive", [] (Doc& d) { d.set ("master.vox.drive_mdb", integer (-1)); }, "mix.json: master.vox.drive_mdb: " },
        { "a clip ceiling", [] (Doc& d) { d.set ("master.clip.ceiling_mdb", integer (-12001)); }, "mix.json: master.clip.ceiling_mdb: " },
        { "a limiter release", [] (Doc& d) { d.set ("master.limiter.release_us", integer (999)); }, "mix.json: master.limiter.release_us: " },
        { "a DC blocker over the low cut", [] (Doc& d) { d.set ("master.dc_hz", integer (20)); d.set ("master.lowcut_hz", integer (20)); }, "mix.json: master.dc_hz: ", "lowcut_hz" },
        { "a reference of the wrong length", [] (Doc& d) { d.at ("reference.novox.channel_true_peak_mdb").items.pop_back(); }, "mix.json: reference.novox.channel_true_peak_mdb: ", "9" },
        { "a reference LUFS", [] (Doc& d) { d.set ("reference.vox.club_lufs_mlu", integer (1)); }, "mix.json: reference.vox.club_lufs_mlu: " },
    };
    checkRefusals (test::kMixJson, cases, mixError);
}

TEST_CASE ("parseMixManifest refuses level ducks that form a cycle, naming the channels", "[manifest]")
{
    // fx ducks under vox in the sample. A level duck keyed on a channel needs that channel computed first.
    auto levelDuck = [] (std::string_view key) {
        Value d = parsed (R"({ "type": "duck", "mode": "level", "key_channel": "x", "depth_mdb": 3000, "range_mdb": 20000,
            "top_mdb": -6000, "attack_us": 5000, "release_us": 100000, "band": { "lo_hz": 0, "hi_hz": 0 }, "pump": false })");
        d.members[2].value = str (std::string (key));
        return d;
    };
    auto error = [] (const Doc& doc) { return parseMixManifest (doc.text(), testKit()).error; };

    SECTION ("two channels: fx keyed on vox, vox keyed on fx")
    {
        Doc doc (test::kMixJson);
        doc.push ("channels[7].inserts", levelDuck ("fx"));
        const auto e = error (doc);
        CAPTURE (e);
        CHECK (e.starts_with ("mix.json: channels[8].inserts[0].key_channel: the level ducks form a cycle: "));
        CHECK (e.find ("vox is keyed on fx") != std::string::npos);
        CHECK (e.find ("fx is keyed on vox") != std::string::npos);
    }

    SECTION ("three channels: fx on vox, vox on lead, lead on fx")
    {
        Doc doc (test::kMixJson);
        doc.push ("channels[7].inserts", levelDuck ("lead"));
        doc.push ("channels[5].inserts", levelDuck ("fx"));
        const auto e = error (doc);
        CAPTURE (e);
        CHECK (e.starts_with ("mix.json: channels[7].inserts[2].key_channel: the level ducks form a cycle: "));
        CHECK (e.find ("lead is keyed on fx, fx is keyed on vox, vox is keyed on lead") != std::string::npos);
    }

    SECTION ("a cycle closed inside a parallel chain")
    {
        Doc doc (test::kMixJson);
        doc.push ("channels[7].inserts", levelDuck ("chords"));
        doc.push ("channels[6].inserts[2].inserts", levelDuck ("fx"));
        const auto e = error (doc);
        CAPTURE (e);
        CHECK (e.find ("the level ducks form a cycle: ") != std::string::npos);
        CHECK (e.find ("chords is keyed on fx") != std::string::npos);
        CHECK (e.find ("fx is keyed on vox") != std::string::npos);
        CHECK (e.find ("vox is keyed on chords") != std::string::npos);
    }

    SECTION ("a chain without a loop is fine, and so are returns keyed on any channel")
    {
        Doc doc (test::kMixJson);
        doc.push ("channels[7].inserts", levelDuck ("lead")); // fx on vox, vox on lead: no loop
        doc.push ("returns.echo.inserts", levelDuck ("fx"));
        doc.push ("groups.music.inserts", levelDuck ("vox"));
        doc.push ("bus_eq.novox", levelDuck ("kick"));
        CHECK (error (doc).empty());
    }
}

TEST_CASE ("both parsers accept the committed manifests", "[manifest]")
{
    // content/plugin/<kit>/<KIT>_kit.json and <KIT>_mix.json, exported by P2 and P3. Each must parse, and its mix
    // against its kit.
    const auto root = std::filesystem::path (WUBANSIA_SOURCE_DIR) / "content" / "plugin";
    if (! std::filesystem::is_directory (root))
        SKIP ("no content/plugin in this checkout yet");
    int kits = 0;
    int mixes = 0;
    for (const auto& folder : std::filesystem::directory_iterator (root))
    {
        if (! folder.is_directory())
            continue;
        const auto name = folder.path().filename().string();
        std::string upper;
        for (const char c : name)
            upper += static_cast<char> (c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c);
        const auto kitPath = folder.path() / (upper + "_kit.json");
        if (! std::filesystem::exists (kitPath))
            continue;
        CAPTURE (kitPath.string());
        const auto kitBytes = util::readBinaryFile (kitPath);
        REQUIRE (kitBytes.ok());
        const auto kit = parseKitManifest (std::string_view (reinterpret_cast<const char*> (kitBytes.value.data()), kitBytes.value.size()));
        CHECK (kit.error == ""); // shows the refusal, with its JSON path
        if (kit.ok())
            CHECK (kit.value.kit.id == upper);
        ++kits;

        const auto mixPath = folder.path() / (upper + "_mix.json");
        if (! kit.ok() || ! std::filesystem::exists (mixPath))
            continue;
        CAPTURE (mixPath.string());
        const auto mixBytes = util::readBinaryFile (mixPath);
        REQUIRE (mixBytes.ok());
        const auto mix = parseMixManifest (std::string_view (reinterpret_cast<const char*> (mixBytes.value.data()), mixBytes.value.size()), kit.value);
        CHECK (mix.error == "");
        ++mixes;
    }
    if (kits == 0)
        SKIP ("no committed manifests yet");
    INFO (kits << " kit.json and " << mixes << " mix.json parsed");
    CHECK (kits > 0);
}
