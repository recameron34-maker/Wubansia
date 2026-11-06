// Tests for core/metrics/value.h: the result tree, its JSON text, the reader and the comparison
// the legacy test uses.

#include "metrics/value.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <array>
#include <string>

using namespace wubansia::metrics;
using Catch::Matchers::ContainsSubstring;

TEST_CASE ("Value objects keep their order and replace a member in place", "[metrics][value]")
{
    Value v = Value::object();
    v.set ("b", 2);
    v.set ("a", "text");
    v.set ("c", Value::array());
    v.set ("b", 5); // replaced where it was, as a JavaScript assignment does

    REQUIRE (v.members().size() == 3);
    CHECK (v.members()[0].first == "b");
    CHECK (v.members()[1].first == "a");
    CHECK (v.members()[2].first == "c");
    CHECK (v.num ("b") == 5);
    CHECK (v["a"].string() == "text");
    CHECK (v["c"].isArray());
    CHECK (v["nothing"].isMissing());
    CHECK (v["nothing"]["deeper"][3].isMissing()); // never fails
    CHECK (v.num ("nothing") == 0);
    CHECK (v.has ("a"));
    CHECK_FALSE (v.has ("z"));

    Value list = Value::array();
    list.push (Value::of (1));
    list.push (Value::of (std::string_view ("two")));
    CHECK (list.size() == 2);
    CHECK (list[0].integer() == 1);
    CHECK (list[1].string() == "two");
    CHECK (list[2].isMissing());

    Value flag = Value::object();
    flag.set ("pass", 3 > 2); // a bool is stored as 1 or 0, as the audit's pass flags are
    CHECK (flag.num ("pass") == 1);
}

TEST_CASE ("toJson writes what JSON.stringify (value, null, 1) writes", "[metrics][value]")
{
    // From Node 22.22.0: JSON.stringify ({a:1, b:"x\"y\\z\n\u0001\u001f\u007fé ", c:[], d:{},
    // e:[1, {f:-2, g:[]}], h:-9999}, null, 1), byte for byte.
    const std::string expected = "{\n"
                                 " \"a\": 1,\n"
                                 " \"b\": \"x\\\"y\\\\z\\n\\u0001\\u001f\x7f\xc3\xa9\xe2\x80\xa8\",\n"
                                 " \"c\": [],\n"
                                 " \"d\": {},\n"
                                 " \"e\": [\n"
                                 "  1,\n"
                                 "  {\n"
                                 "   \"f\": -2,\n"
                                 "   \"g\": []\n"
                                 "  }\n"
                                 " ],\n"
                                 " \"h\": -9999\n"
                                 "}";
    Value v = Value::object();
    v.set ("a", 1);
    v.set ("b", std::string ("x\"y\\z\n\x01\x1f\x7f\xc3\xa9\xe2\x80\xa8"));
    v.set ("c", Value::array());
    v.set ("d", Value::object());
    Value e = Value::array();
    e.push (Value::of (1));
    Value inner = Value::object();
    inner.set ("f", -2);
    inner.set ("g", Value::array());
    e.push (inner);
    v.set ("e", e);
    v.set ("h", -9999);
    CHECK (toJson (v) == expected);

    CHECK (toJson (Value::of (0)) == "0");
    CHECK (toJson (Value::of (std::int64_t { -9223372036854775807 - 1 })) == "-9223372036854775808");
    CHECK (toJson (Value::of (std::string_view ("\b\f\r\t"))) == "\"\\b\\f\\r\\t\"");
}

TEST_CASE ("parseJson reads what toJson writes and refuses what the expected files never hold", "[metrics][value]")
{
    Value v = Value::object();
    v.set ("n", -12);
    v.set ("s", "drop2 (four to the floor) \xc3\xa9");
    Value list = Value::array();
    list.push (Value::of (1));
    list.push (Value::object());
    v.set ("list", list);

    const auto back = parseJson (toJson (v));
    REQUIRE (back.ok());
    CHECK (back.value == v);

    for (const char* bad : { "null", "{\"a\": true}", "[1.5]", "{\"a\": 1, \"a\": 2}", "[1,]", "" })
    {
        INFO (bad);
        CHECK_FALSE (parseJson (bad).ok());
    }
    CHECK_THAT (parseJson ("{\n \"a\": true\n}").error, ContainsSubstring ("line 2"));
}

TEST_CASE ("compare finds every difference and applies a key's tolerance", "[metrics][value]")
{
    const auto make = [] (std::int64_t alpha, std::int64_t sd) {
        Value v = Value::object();
        Value lane = Value::object();
        lane.set ("alpha_milli", alpha);
        lane.set ("sd_milli", sd);
        v.set ("hats", lane);
        v.set ("name", "x");
        return v;
    };
    const std::array<Tolerance, 1> alphaPlusMinus1 = { { { "alpha_milli", 1 } } };

    {
        const auto c = compare (make (524, 2376), make (524, 2376), alphaPlusMinus1);
        CHECK (c.ok());
        CHECK (c.compared == 3);
        CHECK (c.withinTolerance == 0);
    }
    {
        const auto c = compare (make (525, 2376), make (524, 2376), alphaPlusMinus1);
        CHECK (c.ok());
        CHECK (c.withinTolerance == 1);
    }
    {
        const auto c = compare (make (526, 2376), make (524, 2376), alphaPlusMinus1);
        REQUIRE_FALSE (c.ok());
        CHECK_THAT (c.differences[0], ContainsSubstring ("hats.alpha_milli"));
        CHECK_THAT (c.differences[0], ContainsSubstring ("526"));
        CHECK_THAT (c.differences[0], ContainsSubstring ("524"));
    }
    {
        // no tolerance on another key, and none without the list
        CHECK_FALSE (compare (make (524, 2377), make (524, 2376), alphaPlusMinus1).ok());
        CHECK_FALSE (compare (make (525, 2376), make (524, 2376)).ok());
    }
    {
        Value a = make (524, 2376);
        Value b = make (524, 2376);
        a.set ("extra", 1);
        b.set ("missing", 1);
        const auto c = compare (a, b);
        CHECK (c.differenceCount == 2);
        CHECK_THAT (c.differences[0] + c.differences[1], ContainsSubstring ("extra"));
        CHECK_THAT (c.differences[0] + c.differences[1], ContainsSubstring ("missing"));
    }
    {
        Value a = Value::array();
        Value b = Value::array();
        a.push (Value::of (1));
        b.push (Value::of (1));
        b.push (Value::of (2));
        CHECK_FALSE (compare (a, b).ok()); // lengths
        CHECK_FALSE (compare (Value::of (1), Value::of (std::string_view ("1"))).ok()); // kinds
        CHECK_FALSE (compare (Value::of (std::string_view ("a")), Value::of (std::string_view ("b"))).ok());
    }
    {
        // the report stops at the limit, the count does not
        Value a = Value::array();
        Value b = Value::array();
        for (int i = 0; i < 10; ++i)
        {
            a.push (Value::of (i));
            b.push (Value::of (i + 1));
        }
        const auto c = compare (a, b, {}, 3);
        CHECK (c.differences.size() == 3);
        CHECK (c.differenceCount == 10);
        CHECK_THAT (c.differences[0], ContainsSubstring ("[0]"));
    }
}
