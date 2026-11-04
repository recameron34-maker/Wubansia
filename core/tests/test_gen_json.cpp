// Tests for core/gen/json: the strict, integer-only JSON reader the content loaders use.

#include "gen/json.h"
#include "util/utf8.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

using namespace wubansia::gen;
using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::StartsWith;
using Type = json::Value::Type;

namespace
{
    std::string errorOf (std::string_view text)
    {
        const auto r = json::parse (text);
        return r.ok() ? std::string ("(parsed)") : r.error;
    }
}

TEST_CASE ("json parses objects arrays integers and strings in file order", "[gen][json]")
{
    const auto r = json::parse (R"({ "b": 2, "a": [1, -3, 0, "x"], "c": { "d": "e" }, "t": true, "f": false, "n": null })");
    REQUIRE (r.ok());
    const json::Value& v = r.value;
    REQUIRE (v.type == Type::object);
    REQUIRE (v.members.size() == 6);
    CHECK (v.members[0].key == "b"); // file order, not sorted
    CHECK (v.members[1].key == "a");
    CHECK (v.members[0].value.integer == 2);
    const json::Value* a = v.find ("a");
    REQUIRE (a != nullptr);
    REQUIRE (a->items.size() == 4);
    CHECK (a->items[1].integer == -3);
    CHECK (a->items[3].type == Type::string);
    CHECK (a->items[3].string == "x");
    CHECK (v.find ("c")->find ("d")->string == "e");
    CHECK (v.find ("t")->type == Type::boolean);
    CHECK (v.find ("t")->boolean);
    CHECK_FALSE (v.find ("f")->boolean);
    CHECK (v.find ("n")->type == Type::null);
    CHECK (v.find ("zz") == nullptr);
}

TEST_CASE ("json reads the int64 range exactly and refuses beyond it", "[gen][json]")
{
    CHECK (json::parse ("9223372036854775807").value.integer == std::numeric_limits<std::int64_t>::max());
    CHECK (json::parse ("-9223372036854775808").value.integer == std::numeric_limits<std::int64_t>::min());
    CHECK_THAT (errorOf ("9223372036854775808"), ContainsSubstring ("out of range"));
    CHECK_THAT (errorOf ("-9223372036854775809"), ContainsSubstring ("out of range"));
    CHECK_THAT (errorOf ("123456789012345678901234567890"), ContainsSubstring ("out of range"));
}

TEST_CASE ("json is integer only", "[gen][json]")
{
    CHECK_THAT (errorOf ("1.5"), ContainsSubstring ("integer"));
    CHECK_THAT (errorOf ("[1.0]"), ContainsSubstring ("integer"));
    CHECK_THAT (errorOf ("1e3"), ContainsSubstring ("integer"));
    CHECK_THAT (errorOf ("1E-3"), ContainsSubstring ("integer"));
    CHECK_THAT (errorOf ("007"), ContainsSubstring ("leading zero"));
    CHECK_THAT (errorOf ("-0"), ContainsSubstring ("-0"));
    CHECK_THAT (errorOf ("+1"), ContainsSubstring ("unexpected"));
    CHECK_THAT (errorOf ("-"), ContainsSubstring ("digit"));
    CHECK_THAT (errorOf (".5"), ContainsSubstring ("unexpected"));
    CHECK (json::parse ("0").value.integer == 0);
    CHECK (json::parse ("-12").value.integer == -12);
}

TEST_CASE ("json decodes string escapes to UTF-8", "[gen][json]")
{
    const auto r = json::parse (R"(["a\"b\\c\/d\b\f\n\r\t", "\u00e9\u20ac", "\ud83d\ude00", "F#"])");
    REQUIRE (r.ok());
    CHECK (r.value.items[0].string == "a\"b\\c/d\b\f\n\r\t");
    CHECK (r.value.items[1].string == "\xC3\xA9\xE2\x82\xAC"); // e acute, euro
    CHECK (r.value.items[2].string == "\xF0\x9F\x98\x80"); // a surrogate pair: U+1F600
    CHECK (r.value.items[3].string == "F#");
    // Raw UTF-8 passes through.
    CHECK (json::parse ("\"\xC3\xA9\"").value.string == "\xC3\xA9");
}

TEST_CASE ("json refuses what RFC 8259 refuses and the strict extras", "[gen][json]")
{
    CHECK_THAT (errorOf (""), ContainsSubstring ("empty"));
    CHECK_THAT (errorOf ("   "), ContainsSubstring ("empty"));
    CHECK_THAT (errorOf ("[1, 2,]"), ContainsSubstring ("unexpected"));
    CHECK_THAT (errorOf (R"({"a": 1,})"), ContainsSubstring ("unexpected"));
    CHECK_THAT (errorOf (R"({"a" 1})"), ContainsSubstring ("':'"));
    CHECK_THAT (errorOf (R"({a: 1})"), ContainsSubstring ("unexpected"));
    CHECK_THAT (errorOf (R"({"a": 1, "a": 2})"), ContainsSubstring ("\"a\" twice"));
    CHECK_THAT (errorOf ("[1] [2]"), ContainsSubstring ("after the value"));
    CHECK_THAT (errorOf ("// note\n1"), ContainsSubstring ("unexpected"));
    CHECK_THAT (errorOf ("[1 /* x */]"), ContainsSubstring ("unexpected"));
    CHECK_THAT (errorOf ("\xEF\xBB\xBF{}"), ContainsSubstring ("byte order mark"));
    CHECK_THAT (errorOf ("\"a\tb\""), ContainsSubstring ("control character"));
    CHECK_THAT (errorOf ("\"\\x41\""), ContainsSubstring ("escape"));
    CHECK_THAT (errorOf ("\"\\u12\""), ContainsSubstring ("escape"));
    CHECK_THAT (errorOf ("\"\\ud800\""), ContainsSubstring ("surrogate"));
    CHECK_THAT (errorOf ("\"\\ude00\""), ContainsSubstring ("surrogate"));
    CHECK_THAT (errorOf ("\"abc"), ContainsSubstring ("unterminated"));
    CHECK_THAT (errorOf ("\"\xC3\""), ContainsSubstring ("UTF-8"));
    CHECK_THAT (errorOf ("[\"\xFF\"]"), ContainsSubstring ("UTF-8"));
    CHECK_THAT (errorOf ("tru"), ContainsSubstring ("unexpected"));
    CHECK_THAT (errorOf ("nul"), ContainsSubstring ("unexpected"));
    CHECK_THAT (errorOf ("{"), ContainsSubstring ("end of the text"));
    CHECK_THAT (errorOf ("[1"), ContainsSubstring ("end of the text"));
    CHECK_THAT (errorOf ("\x0C[]"), ContainsSubstring ("unexpected")); // form feed isn't JSON whitespace
}

TEST_CASE ("json limits nesting depth", "[gen][json]")
{
    // ADR 0013 documents 64 levels: the literal, not json::maxDepth, so a change to the limit shows
    CHECK (json::maxDepth == 64);
    std::string ok (64, '[');
    ok += std::string (64, ']');
    CHECK (json::parse (ok).ok());
    std::string deep (65, '[');
    deep += std::string (65, ']');
    CHECK_THAT (errorOf (deep), ContainsSubstring ("deep"));
}

TEST_CASE ("json refuses ill-formed UTF-8 in strings as RFC 3629 does", "[gen][json]")
{
    // RFC 3629's ill-formed sequences: overlong leads, overlong 3- and 4-byte forms, surrogates and
    // beyond U+10FFFF, each inside an otherwise good string
    for (const std::string_view bad : { std::string_view ("\xC0\xAF"), std::string_view ("\xC1\xBF"), std::string_view ("\xE0\x80\xAF"),
             std::string_view ("\xE0\x9F\xBF"), std::string_view ("\xED\xA0\x80"), std::string_view ("\xED\xBF\xBF"), std::string_view ("\xF0\x8F\xBF\xBF"),
             std::string_view ("\xF4\x90\x80\x80"), std::string_view ("\xF5\x80\x80\x80"), std::string_view ("\x80"), std::string_view ("\xE2\x28\xA1") })
    {
        INFO ("bytes of length " << bad.size());
        CHECK_THAT (errorOf ("\"a" + std::string (bad) + "b\""), ContainsSubstring ("UTF-8"));
    }
    // and agrees with util::isValidUtf8 (the MIDI writer's check) on every case of its table
    // (test_util.cpp) but NUL, which a JSON string may not hold raw
    const std::vector<std::string_view> table = { "", "drop 2 (16 bars)", "\xC2\x80", "\xDF\xBF", "\xE0\xA0\x80", "\xED\x9F\xBF", "\xEE\x80\x80", "\xEF\xBF\xBF",
        "\xF0\x90\x80\x80", "\xF4\x8F\xBF\xBF", "\x80", "\xC3", "\xE2\x99", "\xC0\xAF", "\xC1\xBF", "\xE0\x9F\xBF", "\xF0\x8F\xBF\xBF", "\xED\xA0\x80",
        "\xED\xBF\xBF", "\xF4\x90\x80\x80", "\xF5\x80\x80\x80", "\xFF", "caf\xE9", "\xE2\x28\xA1", "\xE2\x99\x28" };
    for (std::size_t i = 0; i < table.size(); ++i)
    {
        const std::string_view bytes = table[i];
        INFO ("case " << i);
        const auto r = json::parse ("\"" + std::string (bytes) + "\"");
        CHECK (r.ok() == wubansia::util::isValidUtf8 (bytes));
        if (r.ok())
            CHECK (r.value.string == bytes);
    }
}

TEST_CASE ("json names UTF-16 text instead of failing on its first byte", "[gen][json]")
{
    // What Windows PowerShell 5.1's > and Out-File, and Notepad's "Unicode", write: with or without
    // a byte order mark, either byte order. A NUL is never valid JSON, so the check can't misfire.
    using namespace std::string_view_literals;
    for (const std::string_view text : { "\xFF\xFE{\0}\0"sv, "\xFE\xFF\0{\0}"sv, "{\0}\0"sv, "\0{\0}"sv })
    {
        INFO ("bytes " << text.size() << ", first " << int (static_cast<unsigned char> (text[0])));
        CHECK_THAT (errorOf (text), ContainsSubstring ("UTF-16"));
        CHECK_THAT (errorOf (text), ContainsSubstring ("save it as UTF-8"));
    }
    CHECK_THAT (errorOf ("\xEF\xBB\xBF{}"), ContainsSubstring ("UTF-8 byte order mark"));
}

TEST_CASE ("json errors give the line and column of the problem", "[gen][json]")
{
    CHECK_THAT (errorOf ("{\n  \"a\": 1,\n  \"b\": 2.5\n}"), StartsWith ("line 3, column 8:")); // the number's start
    CHECK_THAT (errorOf ("[\"\xC3\xA9\", x]"), StartsWith ("line 1, column 7:")); // columns count characters
    const auto r = json::parse ("{\n  \"k\": [10,\n    20]\n}");
    REQUIRE (r.ok());
    const json::Value* k = r.value.find ("k");
    CHECK (k->line == 2);
    CHECK (k->column == 8);
    CHECK (k->items[1].line == 3);
    CHECK (k->items[1].column == 5);
}

TEST_CASE ("json Node reads typed values and reports the first problem with its path", "[gen][json]")
{
    const auto r = json::parse (R"({ "name": "Kick", "n": 36, "list": [1, 2, 3], "sub": { "x": 5 }, "s": ["a", "b"] })");
    REQUIRE (r.ok());

    SECTION ("good reads leave no error")
    {
        json::Errors errors ("test.json");
        const json::Node root = json::root (r.value, errors);
        root.only ({ "name", "n", "list", "sub", "s" });
        CHECK (root["name"].string() == "Kick");
        CHECK (root["n"].int32 (0, 127) == 36);
        CHECK (root["list"].ints (1, 4, 0, 9) == std::vector<std::int32_t> { 1, 2, 3 });
        CHECK (root["sub"]["x"].integer (0, 10) == 5);
        CHECK (root["s"].strings (1, 2) == std::vector<std::string> { "a", "b" });
        CHECK_FALSE (root.optional ("missing").exists());
        CHECK (root.int32Or ("missing", 7, 0, 10) == 7);
        CHECK (root.int32Or ("n", 7, 0, 127) == 36);
        CHECK_FALSE (errors.any());
    }

    SECTION ("an unknown key is an error naming it")
    {
        json::Errors errors ("test.json");
        json::root (r.value, errors).only ({ "name", "n", "list", "sub" });
        REQUIRE (errors.any());
        CHECK_THAT (errors.first(), ContainsSubstring ("test.json"));
        CHECK_THAT (errors.first(), ContainsSubstring ("unknown key \"s\""));
    }

    SECTION ("a range error names the path")
    {
        json::Errors errors ("test.json");
        const json::Node root = json::root (r.value, errors);
        CHECK (root["sub"]["x"].int32 (6, 9) == 6);
        REQUIRE (errors.any());
        CHECK_THAT (errors.first(), ContainsSubstring ("sub.x"));
        CHECK_THAT (errors.first(), ContainsSubstring ("6 to 9"));
    }

    SECTION ("a type error names the path and both types")
    {
        json::Errors errors ("test.json");
        const json::Node root = json::root (r.value, errors);
        (void) root["list"].at (1).string();
        REQUIRE (errors.any());
        CHECK_THAT (errors.first(), ContainsSubstring ("list[1]"));
        CHECK_THAT (errors.first(), ContainsSubstring ("a string"));
        CHECK_THAT (errors.first(), ContainsSubstring ("an integer"));
    }

    SECTION ("a missing key and a wrong array length are errors")
    {
        json::Errors e1 ("test.json");
        (void) json::root (r.value, e1)["nope"];
        CHECK_THAT (e1.first(), ContainsSubstring ("missing \"nope\""));
        json::Errors e2 ("test.json");
        (void) json::root (r.value, e2)["list"].ints (4, 4, 0, 9);
        CHECK_THAT (e2.first(), ContainsSubstring ("list"));
        CHECK_THAT (e2.first(), ContainsSubstring ("4 values"));
    }

    SECTION ("only the first error is kept")
    {
        json::Errors errors ("test.json");
        const json::Node root = json::root (r.value, errors);
        (void) root["n"].int32 (0, 1);
        (void) root["name"].int32 (0, 1);
        CHECK_THAT (errors.first(), ContainsSubstring (".n") || ContainsSubstring ("n:"));
        CHECK_THAT (errors.first(), !ContainsSubstring ("name"));
    }
}
