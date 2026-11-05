#pragma once

// The test side's manifest writers: kit.json and mix.json text from the structs, with the keys in
// docs/plugin/manifest.md's order and the canonical layout, byte for byte what
// tools/sound/plugin_common.dumps writes for the same data. The synthetic kit's manifests come from
// here, and P4's parsers can check a round trip: parseKitManifest (kitManifestJson (m)) == m.

#include "kit/manifest.h"
#include "mix/mix_config.h"

#include <cstdint>
#include <string>
#include <vector>

namespace wubansia::engine::testing
{
    // A JSON value in the manifests' subset: integers, strings, flags, arrays, and objects whose
    // members keep the order they were added in.
    struct Json
    {
        enum class Kind : std::uint8_t {
            integer,
            string,
            flag,
            array,
            object
        };

        Kind kind = Kind::object;
        std::int64_t number = 0;
        std::string text;
        bool on = false;
        std::vector<std::string> keys; // object: its members' keys, in order
        std::vector<Json> items; // array: its elements; object: its members' values, matching keys

        static Json integer (std::int64_t v);
        static Json string (std::string v);
        static Json flag (bool v);
        static Json array();
        static Json object();
        static Json ints (const std::vector<std::int32_t>& values);
        static Json strings (const std::vector<std::string>& values);

        Json& add (std::string key, Json value); // objects; returns *this
        Json& push (Json value); // arrays; returns *this
    };

    // The canonical layout (docs/plugin/manifest.md section 1), with a final newline.
    std::string dump (const Json& value);

    Json kitManifestToJson (const KitManifest& kit);
    Json mixManifestToJson (const MixConfig& mix);

    std::string kitManifestJson (const KitManifest& kit);
    std::string mixManifestJson (const MixConfig& mix);
}
