// Tests for core/gen/json: the strict, integer-only JSON reader the content loaders use.

#include "gen/json.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <cstdint>
#include <limits>
#include <string>
#include <string_view>

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
    std::string ok (json::maxDepth, '[');
    ok += std::string (json::maxDepth, ']');
    CHECK (json::parse (ok).ok());
    std::string deep (json::maxDepth + 1, '[');
    deep += std::string (json::maxDepth + 1, ']');
    CHECK_THAT (errorOf (deep), ContainsSubstring ("deep"));
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
