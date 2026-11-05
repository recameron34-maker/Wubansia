#pragma once

// The two per-kit manifests, content/plugin/<kit>/<KIT>_kit.json and <KIT>_mix.json, parsed. The
// schema, every field's unit and range, and what the parsers refuse is docs/plugin/manifest.md: it
// is the contract between the exporters (tools/sound/plugin_kit.py, plugin_mix.py) and the engine.
//
// The parsers (P4, engine/kit/manifest.cpp) build on core's strict integer-only gen::json and run
// on the service thread. An error is one line naming the file kind and the JSON path:
//     "kit.json: instruments[3].zones[0].layers[1].rr[0]: file 412 doesn't exist (412 files)".
//
// KitManifest keeps kit.json's references within the file resolved to indices (keymap a/b,
// layered_with: instrument indices; rr and stems: file indices) and its references into mix.json
// as channel ids, which parseMixManifest checks and buildKit resolves.

#include "kit/kit.h"
#include "mix/mix_config.h"
#include "util/result.h"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace wubansia::engine
{
    struct KitMeta
    {
        std::string id;
        std::string title;
        std::string direction;
        std::int32_t bpm = 0;
        std::string key;
        std::string camelot;
        std::int32_t swingPm = 500;
        std::int32_t contentVersion = 0;
        std::array<std::string, kLanes> lanes; // the label per lane; "" where the kit has no such lane
        std::array<AbLabels, kAbSwitches> ab; // both labels "" where the kit has no such switch

        friend bool operator== (const KitMeta&, const KitMeta&) = default;
    };

    struct FileEntry
    {
        std::string path;
        std::int64_t frames = 0;
        std::int32_t channels = 0;
        std::string sha256; // 64 lowercase hex digits
        std::int64_t loopStart = 0;
        std::int64_t loopEnd = 0;
        std::int32_t prerollFrames = 0;

        friend bool operator== (const FileEntry&, const FileEntry&) = default;
    };

    struct LayerEntry
    {
        std::int32_t vel = 127;
        std::vector<std::int32_t> rr; // file indices, 1-8 of them

        friend bool operator== (const LayerEntry&, const LayerEntry&) = default;
    };

    struct ZoneEntry
    {
        std::int32_t root = 60;
        std::int32_t lo = 0;
        std::int32_t hi = 127;
        std::vector<LayerEntry> layers; // by vel, ascending

        friend bool operator== (const ZoneEntry&, const ZoneEntry&) = default;
    };

    struct ChokeEntry
    {
        std::string group; // "" with role none
        ChokeRole role = ChokeRole::none;
        std::int32_t fadeUs = 0;
        ChokeCurve curve = ChokeCurve::linear;

        friend bool operator== (const ChokeEntry&, const ChokeEntry&) = default;
    };

    struct InstrumentEntry
    {
        std::string id;
        std::string name;
        Lane lane = Lane::kick;
        std::string mixChannel; // a mix.json channel id
        SampleMode mode = SampleMode::oneShot;
        Align align = Align::start;
        std::int32_t attackUs = 0;
        std::int32_t releaseUs = 0;
        std::int32_t gainMdb = 0;
        std::int64_t startFrames = 0;
        VelocityLaw velocityLaw = VelocityLaw::layerOnly;
        Interp interp = Interp::sinc;
        bool mono = false;
        Glide glide;
        bool restrikeContinues = false;
        ChokeEntry choke;
        bool duckKey = false;
        std::vector<std::int32_t> panCyclePm;
        std::int32_t rrBlock16ths = 64;
        std::int32_t layeredWith = -1; // instrument index; "" in the file
        std::vector<ZoneEntry> zones; // by root, ascending, ranges disjoint

        friend bool operator== (const InstrumentEntry&, const InstrumentEntry&) = default;
    };

    struct KeymapEntry
    {
        std::int32_t ch = 1; // 1-16
        std::int32_t lo = 0; // keys lo..hi, both included
        std::int32_t hi = 0;
        std::int32_t a = -1; // instrument index
        std::int32_t b = -1; // instrument index, or -1
        std::int32_t ab = -1; // an AbSwitch value with b, else -1

        friend bool operator== (const KeymapEntry&, const KeymapEntry&) = default;
    };

    struct ExtraEntry
    {
        std::int32_t ch = 11; // 11-16
        std::string name;
        Lane lane = Lane::bass;

        friend bool operator== (const ExtraEntry&, const ExtraEntry&) = default;
    };

    struct SliceEntry
    {
        std::int32_t step = 0;
        std::int32_t key = 0;
        std::int64_t start = 0;
        std::int64_t end = 0;
        std::int64_t fade = 0;
        std::int32_t lead = 0;
        std::array<std::int32_t, kMaxHolds> holds {}; // keys, in order; entries from holdCount on are 0
        std::int32_t holdCount = 0;

        friend bool operator== (const SliceEntry&, const SliceEntry&) = default;
    };

    // A phrase's ride grid: one controller value per 60 ticks (960 PPQ) over its cycle, as the MIDI files
    // write rides, so a ride holds cycle16ths x 4 values; a cycle matches it within kRideSlack.
    inline constexpr int kRideTicks = 60;
    inline constexpr int kRideSlack = 1;

    struct PhraseEntry
    {
        Lane lane = Lane::lead;
        std::int32_t ch = 2; // 1-16
        std::int32_t ab = -1; // an AbSwitch value choosing the stem, or -1
        std::string mixChannel; // a mix.json channel id
        std::int32_t gainMdb = 0;
        PhraseThrow throwMode = PhraseThrow::live; // "throw"
        std::int32_t stemA = -1; // file index
        std::int32_t stemB = -1; // file index, or -1 without an alternative
        std::int32_t cycle16ths = 32;
        std::int32_t tolerancePm = 20;
        std::int32_t fadeUs = 5000;
        // The CC74 and CC71 values the stem was rendered with on the ride grid (cycle16ths x 4 each, 0-127),
        // or empty: no constraint on that controller. Only the matcher reads them, on the service thread;
        // the runtime Kit doesn't carry them.
        std::vector<std::uint8_t> ride74;
        std::vector<std::uint8_t> ride71;
        std::vector<SliceEntry> slices; // by step

        friend bool operator== (const PhraseEntry&, const PhraseEntry&) = default;
    };

    struct KitManifest
    {
        KitMeta kit;
        std::vector<FileEntry> files;
        std::vector<InstrumentEntry> instruments;
        std::vector<KeymapEntry> keymap;
        std::vector<ExtraEntry> extras;
        std::vector<PhraseEntry> phrases;

        friend bool operator== (const KitManifest&, const KitManifest&) = default;
    };

    // kit.json. Refuses anything docs/plugin/manifest.md doesn't allow: a float, an unknown or
    // missing key, a value out of range, an unknown enum spelling, a dangling file or instrument
    // reference, overlapping zones or keymap ranges...
    [[nodiscard]] util::Result<KitManifest> parseKitManifest (std::string_view json);

    // mix.json, checked against its kit.json: the same kit id and content version, every channel id
    // kit.json names exists, at most kMaxMixChannels channels, one preUs per onset-keyed lane...
    [[nodiscard]] util::Result<MixConfig> parseMixManifest (std::string_view json, const KitManifest& kit);
}
