// The test-side manifest writer: the canonical layout, byte for byte what
// tools/sound/plugin_common.dumps writes (tools/sound/tests/test_plugin_common.py checks the same text).

#include "testing/manifest_json.h"

#include "gen/json.h"

#include <catch2/catch_test_macros.hpp>

using wubansia::engine::testing::dump;
using wubansia::engine::testing::Json;

TEST_CASE ("the manifest writer's layout is plugin_common.dumps's", "[manifest_json]")
{
    // The same object and text as test_plugin_common.py's test_the_canonical_layout.
    Json obj = Json::object();
    obj.add ("kit", Json::string ("FUT01"));
    obj.add ("n", Json::integer (-3));
    obj.add ("flag", Json::flag (true));
    obj.add ("empty_list", Json::array());
    obj.add ("empty_obj", Json::object());
    obj.add ("ints", Json::ints ({ 1, -2, 3 }));
    obj.add ("names", Json::strings ({ "a", "b" }));
    obj.add ("rows", Json::array().push (Json::object().add ("x", Json::integer (1))).push (Json::object().add ("y", Json::ints ({ 4, 5 }))));
    obj.add ("nested", Json::object().add ("deep", Json::object().add ("v", Json::integer (0))));

    CHECK (dump (obj) == "{\n"
                         "  \"kit\": \"FUT01\",\n"
                         "  \"n\": -3,\n"
                         "  \"flag\": true,\n"
                         "  \"empty_list\": [],\n"
                         "  \"empty_obj\": {},\n"
                         "  \"ints\": [1, -2, 3],\n"
                         "  \"names\": [\"a\", \"b\"],\n"
                         "  \"rows\": [\n"
                         "    {\n"
                         "      \"x\": 1\n"
                         "    },\n"
                         "    {\n"
                         "      \"y\": [4, 5]\n"
                         "    }\n"
                         "  ],\n"
                         "  \"nested\": {\n"
                         "    \"deep\": {\n"
                         "      \"v\": 0\n"
                         "    }\n"
                         "  }\n"
                         "}\n");
}

TEST_CASE ("the manifest writer escapes only quotes, backslashes and control characters", "[manifest_json]")
{
    const std::string text = "F# minor \"x\" \\ \t\n\x01 \xc3\xa9 \xe2\x99\xaf"; // ... é ♯ in UTF-8
    const auto out = dump (Json::object().add ("s", Json::string (text)));
    CHECK (out == "{\n  \"s\": \"F# minor \\\"x\\\" \\\\ \\t\\n\\u0001 \xc3\xa9 \xe2\x99\xaf\"\n}\n");

    const auto parsed = wubansia::gen::json::parse (out);
    REQUIRE (parsed.ok());
    CHECK (parsed.value.find ("s")->string == text);
}

TEST_CASE ("a mixed array goes one element per line", "[manifest_json]")
{
    CHECK (dump (Json::array().push (Json::integer (1)).push (Json::ints ({ 2 }))) == "[\n  1,\n  [2]\n]\n");
}
