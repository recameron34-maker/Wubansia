// The synthetic kit (engine/testing/synthetic_kit.h): deterministic, complete, and laid out as kit.h
// says. Every package tests against it, so its shape is pinned here.

#include "testing/manifest_json.h"
#include "testing/synthetic_kit.h"

#include "kit/channel_map.h"

#include "gen/json.h"
#include "midi/reader.h"
#include "util/file_io.h"
#include "util/sha256.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <bit>
#include <cmath>
#include <filesystem>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

using namespace wubansia::engine;
using namespace wubansia::engine::testing;
namespace midi = wubansia::midi;
namespace json = wubansia::gen::json;

namespace
{
    // One digest over every byte the synthetic kit is: both manifests, every WAV and every SMF. A change
    // to the synthetic kit changes it: update it in the same commit, and tell the packages that use it.
    constexpr const char* kSyntheticContentSha256 = "1473652862e7c33c8c8ed0bb2bbfd794a4db95675c85296ab54013c64b884d1e";

    std::string contentDigest (const SyntheticContent& c)
    {
        std::vector<std::uint8_t> all (c.kitJson.begin(), c.kitJson.end());
        all.insert (all.end(), c.mixJson.begin(), c.mixJson.end());
        for (const auto& w : c.wavs)
            all.insert (all.end(), w.begin(), w.end());
        for (const auto& s : c.smfs)
            all.insert (all.end(), s.begin(), s.end());
        return wubansia::util::sha256Hex (all);
    }

    const InstrumentEntry& instrument (const KitManifest& m, int index)
    {
        return m.instruments.at (static_cast<std::size_t> (index));
    }

    std::vector<std::string> keysOf (const json::Value& object)
    {
        std::vector<std::string> keys;
        for (const auto& member : object.members)
            keys.push_back (member.key);
        return keys;
    }

    void collectTypes (const json::Value& v, std::set<std::string>& types)
    {
        if (const auto* type = v.find ("type"); type != nullptr && type->type == json::Value::Type::string)
            types.insert (type->string);
        for (const auto& item : v.items)
            collectTypes (item, types);
        for (const auto& member : v.members)
            collectTypes (member.value, types);
    }

    // Reads a 24-bit PCM WAV with 'fmt ' and 'data' chunks only, as wavBytes writes it.
    SampleData readWav24 (const std::vector<std::uint8_t>& b)
    {
        auto byte = [&b] (std::size_t at) { return static_cast<std::uint32_t> (b[at]); };
        auto u32 = [&byte] (std::size_t at) { return byte (at) | (byte (at + 1) << 8) | (byte (at + 2) << 16) | (byte (at + 3) << 24); };
        auto u16 = [&byte] (std::size_t at) { return byte (at) | (byte (at + 1) << 8); };
        SampleData d;
        d.channels = static_cast<std::int32_t> (u16 (22));
        d.sampleRate = static_cast<std::int32_t> (u32 (24));
        const auto dataBytes = u32 (40);
        d.frames = dataBytes / (3u * static_cast<std::uint32_t> (d.channels));
        d.audio.assign (static_cast<std::size_t> (d.frames * d.channels), 0.0f);
        for (std::int64_t n = 0; n < d.frames; ++n)
            for (std::int32_t c = 0; c < d.channels; ++c)
            {
                const std::size_t at = 44u + 3u * static_cast<std::size_t> (n * d.channels + c);
                auto v = static_cast<std::int32_t> (byte (at) | (byte (at + 1) << 8) | (byte (at + 2) << 16));
                if (v & 0x800000)
                    v -= 0x1000000;
                d.audio[static_cast<std::size_t> (c * d.frames + n)] = static_cast<float> (static_cast<double> (v) / 8388608.0);
            }
        return d;
    }

    struct TrackNotes
    {
        std::string name;
        std::vector<midi::Note> notes;
        std::vector<midi::Controller> controllers;
    };

    std::vector<TrackNotes> lanesOf (const midi::File& file)
    {
        std::vector<TrackNotes> out;
        for (std::size_t t = 1; t < file.tracks.size(); ++t)
            out.push_back ({ midi::trackName (file.tracks[t]), midi::notesOf (file.tracks[t]), midi::controllersOf (file.tracks[t]) });
        return out;
    }
}

TEST_CASE ("the synthetic kit and its four SMFs are byte-identical across two builds", "[synthetic]")
{
    const auto a = buildSyntheticContent();
    const auto b = buildSyntheticContent();
    CHECK (a.kitJson == b.kitJson);
    CHECK (a.mixJson == b.mixJson);
    CHECK (a.wavs == b.wavs);
    CHECK (a.smfs == b.smfs);
    CHECK (a.audio == b.audio);
    CHECK (a.kit == b.kit);
    CHECK (a.mix == b.mix);

    const auto& kept = syntheticContent();
    CHECK (kept.kitJson == a.kitJson);
    CHECK (kept.smfs == a.smfs);

    // The same bytes on every build and platform: generated with integer and basic floating-point
    // arithmetic only, no libm.
    CHECK (contentDigest (a) == kSyntheticContentSha256);
}

TEST_CASE ("the synthetic kit has what the packages test against", "[synthetic]")
{
    const auto m = syntheticKitManifest();
    REQUIRE (m.instruments.size() == static_cast<std::size_t> (syn::instrumentCount));
    REQUIRE (m.files.size() == static_cast<std::size_t> (syn::fileCount));
    CHECK (m.kit.id == kSyntheticKitId);
    CHECK (m.kit.bpm == kSyntheticBpm);
    CHECK (m.kit.swingPm == kSyntheticSwingPm);

    SECTION ("a kick with A and B, B with 2 layers and 2 round robins")
    {
        CHECK (instrument (m, syn::kickA).lane == Lane::kick);
        const auto& b = instrument (m, syn::kickB);
        REQUIRE (b.zones.size() == 1);
        REQUIRE (b.zones[0].layers.size() == 2);
        CHECK (b.zones[0].layers[0].rr.size() == 2);
        CHECK (b.zones[0].layers[1].rr.size() == 2);
        CHECK (b.duckKey);
        CHECK (m.kit.ab[static_cast<std::size_t> (AbSwitch::kick)].exists());
    }

    SECTION ("a snare with 2 layers and 2 round robins")
    {
        const auto& s = instrument (m, syn::snare);
        CHECK (s.lane == Lane::snare);
        CHECK (s.zones.at (0).layers.size() == 2);
        CHECK (s.zones.at (0).layers.at (1).rr.size() == 2);
    }

    SECTION ("a closed and an open hat in one choke group")
    {
        const auto& closed = instrument (m, syn::hatClosed);
        const auto& open = instrument (m, syn::hatOpen);
        CHECK (closed.choke.group == open.choke.group);
        CHECK_FALSE (closed.choke.group.empty());
        CHECK (closed.choke.role == ChokeRole::chokes);
        CHECK (open.choke.role == ChokeRole::choked);
        CHECK (closed.panCyclePm.size() == 2);
        CHECK (closed.zones.at (0).layers.at (0).rr.size() == 2);
    }

    SECTION ("a 3-zone sustain-loop bass, A and B, mono legato with glides")
    {
        for (const int i : { syn::bassA, syn::bassB })
        {
            const auto& bass = instrument (m, i);
            CHECK (bass.mode == SampleMode::sustainLoop);
            CHECK (bass.zones.size() == 3);
            CHECK (bass.mono);
            CHECK (bass.glide.curve != GlideCurve::none);
            for (const auto& z : bass.zones)
                for (const auto& l : z.layers)
                    for (const auto f : l.rr)
                        CHECK (m.files.at (static_cast<std::size_t> (f)).loopEnd > m.files.at (static_cast<std::size_t> (f)).loopStart);
        }
        CHECK (instrument (m, syn::bassA).interp == Interp::hermite);
        CHECK (instrument (m, syn::bassA).glide.dipUs > 0);
        CHECK (instrument (m, syn::bassB).zones.at (0).layers.size() == 2);
        CHECK (instrument (m, syn::bassA).zones.at (0).lo < instrument (m, syn::bassA).zones.at (0).root); // F1 plays the G1 zone pitched down
        CHECK (m.kit.ab[static_cast<std::size_t> (AbSwitch::bass)].exists());
    }

    SECTION ("a 2-zone stab, and no stab A/B")
    {
        CHECK (instrument (m, syn::stab).zones.size() == 2);
        CHECK_FALSE (m.kit.ab[static_cast<std::size_t> (AbSwitch::stab)].exists());
    }

    SECTION ("a held pad with restrike-continue, on a kit extra")
    {
        const auto& pad = instrument (m, syn::pad);
        CHECK (pad.restrikeContinues);
        CHECK (pad.mode == SampleMode::sustainLoop);
        REQUIRE (m.extras.size() == 1);
        CHECK (m.extras[0].ch == 11);
        CHECK (m.extras[0].lane == Lane::chords);
    }

    SECTION ("a vox zone with velocity layers and a pre-roll")
    {
        const auto& vox = instrument (m, syn::vox);
        REQUIRE (vox.zones.size() == 1);
        CHECK (vox.zones[0].layers.size() == 3);
        for (const auto& l : vox.zones[0].layers)
            CHECK (m.files.at (static_cast<std::size_t> (l.rr.at (0))).prerollFrames == 960);
    }

    SECTION ("an end-aligned riser that starts into its file")
    {
        const auto& riser = instrument (m, syn::riser);
        CHECK (riser.align == Align::end);
        CHECK (riser.startFrames > 0);
        CHECK (riser.attackUs > 0);
    }

    SECTION ("a looped 2-bar phrase stem with 8 contiguous slices, one holding a retrigger, the last wrapping")
    {
        REQUIRE (m.phrases.size() == 1);
        const auto& p = m.phrases[0];
        CHECK (p.cycle16ths == 32);
        CHECK (p.stemA == syn::stemFile);
        CHECK (p.stemB == -1);
        CHECK (p.throwMode == PhraseThrow::baked);
        const auto stemFrames = m.files.at (static_cast<std::size_t> (syn::stemFile)).frames;
        CHECK (stemFrames == 32 * kSyntheticSixteenthFrames);
        REQUIRE (p.slices.size() == static_cast<std::size_t> (syn::phraseSlices));
        CHECK (p.slices.front().start > 0); // slice 0 starts at its own pre-roll
        CHECK (p.slices.back().end > stemFrames); // the last slice wraps through the stem's end...
        CHECK (p.slices.back().end - stemFrames == p.slices.front().start); // ...and continues into slice 0
        CHECK (p.slices.back().fade > stemFrames);
        int holding = 0;
        for (std::size_t i = 0; i < p.slices.size(); ++i)
        {
            const auto& s = p.slices[i];
            CAPTURE (i);
            CHECK (s.start < s.fade);
            CHECK (s.fade <= s.end);
            if (i + 1 < p.slices.size())
                CHECK (s.end == p.slices[i + 1].start);
            const auto onset = s.step * kSyntheticSixteenthFrames + (s.step % 2 != 0 ? kSyntheticSwingFrames : 0);
            CHECK (s.start == onset - s.lead);
            if (s.holdCount > 0)
            {
                ++holding;
                CHECK (s.holds[0] == s.key); // a retrigger of its own key
            }
            for (std::size_t h = static_cast<std::size_t> (s.holdCount); h < s.holds.size(); ++h)
                CHECK (s.holds[h] == 0);
        }
        CHECK (holding == 1);
        CHECK (std::any_of (p.slices.begin(), p.slices.end(), [] (const SliceEntry& s) { return s.step % 2 != 0; }));
    }

    SECTION ("the lanes it has and hasn't")
    {
        CHECK (m.kit.lanes[static_cast<std::size_t> (Lane::perc)].empty());
        CHECK (m.kit.lanes[static_cast<std::size_t> (Lane::accents)].empty());
        CHECK_FALSE (m.kit.lanes[static_cast<std::size_t> (Lane::fx)].empty());
    }
}

TEST_CASE ("the synthetic files: 24-bit values, exact loops, and WAVs that hash as the manifest says", "[synthetic]")
{
    const auto& c = syntheticContent();
    for (std::size_t i = 0; i < c.kit.files.size(); ++i)
    {
        const auto& f = c.kit.files[i];
        const auto& audio = c.audio[i];
        CAPTURE (f.path);
        CHECK (audio.frames == f.frames);
        CHECK (audio.channels == f.channels);
        CHECK (audio.sampleRate == kContentSampleRate);
        CHECK (wubansia::util::sha256Hex (c.wavs[i]) == f.sha256);
        CHECK (readWav24 (c.wavs[i]) == audio); // the WAV holds exactly these values

        bool values24 = true, inRange = true, sounds = false;
        for (const float x : audio.audio)
        {
            const double scaled = static_cast<double> (x) * 8388608.0;
            values24 = values24 && std::equal_to<double> {}(scaled, std::floor (scaled));
            inRange = inRange && std::fabs (x) < 1.0f;
            sounds = sounds || std::not_equal_to<float> {}(x, 0.0f);
        }
        CHECK (values24);
        CHECK (inRange);
        CHECK (sounds);

        if (f.loopEnd > 0) // a sustain loop: it wraps without a step, sample for sample
        {
            REQUIRE (f.loopEnd < f.frames);
            for (std::int32_t ch = 0; ch < audio.channels; ++ch)
            {
                const auto* x = audio.audio.data() + static_cast<std::size_t> (ch * audio.frames);
                CHECK (std::bit_cast<std::uint32_t> (x[f.loopEnd]) == std::bit_cast<std::uint32_t> (x[f.loopStart]));
                CHECK (std::bit_cast<std::uint32_t> (x[f.loopEnd - 1]) == std::bit_cast<std::uint32_t> (x[f.loopStart - 1]));
            }
        }
    }
}

TEST_CASE ("the synthetic manifests are strict integer-only JSON in the documented order", "[synthetic]")
{
    const auto kit = json::parse (syntheticKitJson());
    REQUIRE (kit.ok());
    CHECK (keysOf (kit.value) == std::vector<std::string> { "kit", "files", "instruments", "keymap", "extras", "phrases" });
    const auto* instruments = kit.value.find ("instruments");
    REQUIRE (instruments != nullptr);
    CHECK (keysOf (instruments->items.at (0)) == std::vector<std::string> { "id", "name", "lane", "mix_channel", "mode", "align", "attack_us", "release_us", "gain_mdb", "start_frames", "velocity_law", "interp", "mono", "glide", "restrike_continues", "choke", "duck_key", "pan_cycle_pm", "rr_block_16ths", "layered_with", "zones" });
    const auto* phrases = kit.value.find ("phrases");
    REQUIRE (phrases != nullptr);
    CHECK (keysOf (phrases->items.at (0)) == std::vector<std::string> { "lane", "ch", "ab", "mix_channel", "gain_mdb", "throw", "stems", "cycle_16ths", "tolerance_pm", "fade_us", "ride", "slices" });
    CHECK (keysOf (*phrases->items.at (0).find ("ride")) == std::vector<std::string> { "cc74", "cc71" });
    CHECK (keysOf (phrases->items.at (0).find ("slices")->items.at (0)) == std::vector<std::string> { "step", "key", "start", "end", "fade", "lead", "holds" });

    const auto mix = json::parse (syntheticMixJson());
    REQUIRE (mix.ok());
    CHECK (keysOf (mix.value) == std::vector<std::string> { "kit", "content_version", "channels", "groups", "returns", "bus_eq", "master", "reference" });

    // Every insert type is used, so every parser path is exercised.
    std::set<std::string> types;
    collectTypes (mix.value, types);
    std::set<std::string> all (kInsertTypeIds.begin(), kInsertTypeIds.end());
    CHECK (types == all);
    CHECK (syntheticMixJson().find ("\"mode\": \"level\"") != std::string::npos);
    CHECK (syntheticMixJson().find ("\"when\"") != std::string::npos);
}

TEST_CASE ("the synthetic SMFs follow the MIDI plan and the channel map", "[synthetic]")
{
    const auto kit = makeSyntheticKit();
    REQUIRE (kit != nullptr);
    const auto m = syntheticKitManifest();

    for (const auto mode : { PlayMode::loop, PlayMode::song })
        for (const auto variant : { Variant::vox, Variant::novox })
        {
            const bool song = mode == PlayMode::song;
            const bool vox = variant == Variant::vox;
            CAPTURE (song, vox);
            const auto bytes = syntheticSmf (mode, variant);
            const auto read = midi::readSmf (bytes);
            REQUIRE (read.ok());
            const auto& file = read.value;
            CHECK (file.format == 1);
            CHECK (file.ppq == 960);

            const auto tempos = midi::tempoMap (file);
            REQUIRE (tempos.size() == 1);
            CHECK (tempos[0].microsPerQuarter == 468750);
            const auto signatures = midi::timeSignaturesOf (file);
            REQUIRE (signatures.size() == 1);
            CHECK (signatures[0].numerator == 4);
            CHECK (signatures[0].denominatorPow2 == 2);
            const auto keys = midi::keySignaturesOf (file);
            REQUIRE (keys.size() == 1);
            CHECK (keys[0].sharps == -2);
            CHECK (keys[0].minor);
            const auto end = song ? kSyntheticSongTicks : kSyntheticLoopTicks;
            for (const auto& t : file.tracks)
                CHECK (t.endTick == end);

            const auto markers = midi::markersOf (file);
            const auto cues = midi::textsOf (file.tracks[0]);
            if (song)
            {
                CHECK (markers.size() == 3);
                CHECK (std::count_if (cues.begin(), cues.end(), [] (const midi::TextEvent& t) { return t.type == midi::meta::cuePoint; }) == 2);
            }
            else
            {
                CHECK (markers.empty());
            }

            bool hasVox = false;
            for (const auto& lane : lanesOf (file))
            {
                CAPTURE (lane.name);
                hasVox = hasVox || lane.name == "Vox";
                CHECK (trackLane (kit->info, lane.name) >= 0); // every track resolves to a lane

                // No same-key overlaps within a track.
                std::map<int, std::int64_t> sounding;
                for (const auto& n : lane.notes)
                {
                    const int key = n.channel * 128 + n.key;
                    if (sounding.count (key) > 0)
                        CHECK (n.tick >= sounding[key]);
                    sounding[key] = n.tick + n.length;
                    CHECK (n.tick + n.length <= end);
                }

                for (const auto& n : lane.notes)
                {
                    const int channel = n.channel + 1;
                    CHECK (channel != 9);
                    if (channel == 10)
                    {
                        CHECK (n.key >= kDrumKeyLo);
                        CHECK (n.key <= kDrumKeyHi);
                    }
                    if (channel == 3)
                        continue; // the voicing: Wubansia ignores it
                    const auto& target = kit->keys[n.channel][n.key];
                    CHECK (target.a >= 0); // every other note plays something
                }
            }
            CHECK (hasVox == vox);
        }
}

TEST_CASE ("the phrase lane's notes are its slices and their holds", "[synthetic]")
{
    const auto m = syntheticKitManifest();
    const auto& phrase = m.phrases.at (0);
    const auto read = midi::readSmf (syntheticSmf (PlayMode::loop, Variant::vox));
    REQUIRE (read.ok());

    std::vector<midi::Note> lead;
    for (std::size_t t = 1; t < read.value.tracks.size(); ++t)
        if (midi::trackName (read.value.tracks[t]) == "Lead")
            lead = midi::notesOf (read.value.tracks[t]);
    REQUIRE (lead.size() == 18); // 9 notes, twice

    // A note's 16th is the one whose swung start is the latest at or before it (the matcher's rule).
    auto sixteenth = [] (std::int64_t tick) {
        auto step = tick / 240;
        if (step % 2 != 0 && step * 240 + kSyntheticSwingTicks > tick)
            --step; // before an odd 16th's swung start: still the 16th before
        return static_cast<int> (step);
    };
    std::size_t sliced = 0, held = 0;
    const SliceEntry* last = nullptr;
    for (const auto& n : lead)
    {
        const int step = sixteenth (n.tick) % phrase.cycle16ths;
        const auto slice = std::find_if (phrase.slices.begin(), phrase.slices.end(), [&] (const SliceEntry& s) { return s.step == step && s.key == n.key; });
        if (slice != phrase.slices.end())
        {
            ++sliced;
            last = &*slice;
            continue;
        }
        REQUIRE (last != nullptr);
        CHECK (std::find (last->holds.begin(), last->holds.begin() + last->holdCount, n.key) != last->holds.begin() + last->holdCount);
        ++held;
    }
    CHECK (sliced == 16);
    CHECK (held == 2);
}

TEST_CASE ("the lead phrase's ride matches the files' controllers in every cycle it plays", "[synthetic]")
{
    const auto m = syntheticKitManifest();
    const auto& phrase = m.phrases.at (0);
    REQUIRE (phrase.ride74.size() == static_cast<std::size_t> (phrase.cycle16ths * 240 / kRideTicks));
    CHECK (phrase.ride71.empty()); // no constraint on CC71

    // The contract's rule: a cycle's value at a grid tick is the last CC on the phrase's channel at or
    // before it, 0 before any; the cycle matches where every value is within kRideSlack.
    auto controllers = [&phrase] (const std::vector<std::uint8_t>& smf) {
        std::vector<midi::Controller> cc74;
        const auto read = midi::readSmf (smf);
        REQUIRE (read.ok());
        for (const auto& track : read.value.tracks)
            for (const auto& c : midi::controllersOf (track))
                if (c.channel == phrase.ch - 1 && c.number == 74)
                    cc74.push_back (c);
        std::stable_sort (cc74.begin(), cc74.end(), [] (const midi::Controller& a, const midi::Controller& b) { return a.tick < b.tick; });
        return cc74;
    };
    auto matches = [&phrase] (const std::vector<midi::Controller>& cc74, int cycle, const std::vector<std::uint8_t>& ride) {
        const std::int64_t start = static_cast<std::int64_t> (cycle) * phrase.cycle16ths * 240;
        for (std::size_t i = 0; i < ride.size(); ++i)
        {
            const auto at = start + static_cast<std::int64_t> (i) * kRideTicks;
            int value = 0;
            for (const auto& c : cc74)
                if (c.tick <= at)
                    value = c.value;
            if (std::abs (value - ride[i]) > kRideSlack)
                return false;
        }
        return true;
    };

    const auto loop = controllers (syntheticSmf (PlayMode::loop, Variant::vox));
    CHECK (matches (loop, 0, phrase.ride74));
    CHECK (matches (loop, 1, phrase.ride74));

    auto nudged = phrase.ride74; // within 1 still matches
    nudged[40] = static_cast<std::uint8_t> (nudged[40] + 1);
    CHECK (matches (loop, 0, nudged));
    auto other = phrase.ride74; // 2 off at one point is another ride: the multisample plays
    other[40] = static_cast<std::uint8_t> (other[40] + 2);
    CHECK_FALSE (matches (loop, 0, other));

    const auto song = controllers (syntheticSmf (PlayMode::song, Variant::novox));
    for (const int cycle : { 2, 3, 4, 5 }) // the Drop's lead
        CHECK (matches (song, cycle, phrase.ride74));
    CHECK_FALSE (matches (song, 0, phrase.ride74)); // the Intro: no ride written (0), and no lead either
}

TEST_CASE ("layOutKit lays the synthetic kit out as kit.h describes", "[synthetic]")
{
    const auto kit = makeSyntheticKit();
    REQUIRE (kit != nullptr);
    const auto m = syntheticKitManifest();
    const auto& k = *kit;

    REQUIRE (k.instruments.size() == m.instruments.size());
    REQUIRE (k.files.size() == m.files.size());
    REQUIRE (k.audio.size() == m.files.size());

    std::size_t zones = 0, layers = 0, rr = 0;
    for (std::size_t i = 0; i < m.instruments.size(); ++i)
    {
        const auto& in = k.instruments[i];
        const auto& e = m.instruments[i];
        CAPTURE (e.id);
        CHECK (in.id == e.id);
        CHECK (in.firstZone == zones);
        REQUIRE (in.zoneCount == e.zones.size());
        CHECK (k.mix.channels.at (in.mixChannel).id == e.mixChannel);
        CHECK (k.mix.channels.at (in.mixChannel).lane == static_cast<std::uint8_t> (e.lane));
        for (std::size_t z = 0; z < e.zones.size(); ++z)
        {
            const auto& zone = k.zones.at (in.firstZone + z);
            CHECK (zone.root == e.zones[z].root);
            CHECK (zone.firstLayer == layers);
            for (std::size_t l = 0; l < e.zones[z].layers.size(); ++l)
            {
                const auto& layer = k.layers.at (zone.firstLayer + l);
                CHECK (layer.velocity == e.zones[z].layers[l].vel);
                CHECK (layer.firstRr == rr);
                for (std::size_t r = 0; r < e.zones[z].layers[l].rr.size(); ++r)
                    CHECK (k.rr.at (layer.firstRr + r) == e.zones[z].layers[l].rr[r]);
                rr += e.zones[z].layers[l].rr.size();
            }
            layers += e.zones[z].layers.size();
        }
        zones += e.zones.size();
    }
    CHECK (k.zones.size() == zones);
    CHECK (k.layers.size() == layers);
    CHECK (k.rr.size() == rr);
    CHECK (k.pans == std::vector<std::int16_t> { -200, 200 });

    // Keys: (channel 10, 36) is kick A or B by the kick switch; channel 1 is bass A or B.
    const auto& kickKey = k.keys[9][36];
    CHECK (kickKey.a == syn::kickA);
    CHECK (kickKey.b == syn::kickB);
    CHECK (kickKey.ab == static_cast<std::int8_t> (AbSwitch::kick));
    CHECK (k.keys[0][29].a == syn::bassA); // F1, below the lowest root
    CHECK (k.keys[0][29].b == syn::bassB);
    CHECK (k.keys[9][37].a == -1);
    CHECK (k.keys[2][60].a == -1); // channel 3: nothing plays
    CHECK (k.keys[10][55].a == syn::pad);

    // Choke groups numbered from 1; CC routes.
    CHECK (k.instruments[syn::hatClosed].choke.group == 1);
    CHECK (k.instruments[syn::hatOpen].choke.group == 1);
    CHECK (k.instruments[syn::kickA].choke.group == 0);
    const auto bassChannel = static_cast<unsigned> (k.mix.channelIndex ("bass"));
    const auto leadChannel = static_cast<unsigned> (k.mix.channelIndex ("lead"));
    const auto stemChannel = static_cast<unsigned> (k.mix.channelIndex ("lead_stem"));
    CHECK (k.ccMixChannels[0] == (1u << bassChannel));
    CHECK (k.ccMixChannels[1] == ((1u << leadChannel) | (1u << stemChannel)));
    CHECK (k.ccMixChannels[2] == 0u);

    // The phrase.
    REQUIRE (k.phrases.size() == 1);
    CHECK (k.phrases[0].stem[0] == syn::stemFile);
    CHECK (k.phrases[0].stem[1] == syn::stemFile); // no alternative: B is A
    CHECK (k.phrases[0].ab == -1);
    CHECK (k.phrases[0].throwMode == PhraseThrow::baked);
    CHECK (k.phrases[0].channel == 1);
    CHECK (k.phrases[0].sliceCount == syn::phraseSlices);
    CHECK (k.slices.size() == static_cast<std::size_t> (syn::phraseSlices));

    // What the kit has.
    CHECK (k.info.lanes == (0x3FFu & ~(laneBit (Lane::perc) | laneBit (Lane::accents))));
    CHECK (k.info.abSwitches == 0b011u);
    REQUIRE (k.info.extras.size() == 1);
    CHECK (k.info.extras[0].channel == 10);
    CHECK (trackLane (k.info, "Pad") == static_cast<int> (Lane::chords));

    // Views.
    for (std::size_t i = 0; i < k.files.size(); ++i)
    {
        const auto v = k.view (i);
        CHECK (v.frames == k.files[i].frames);
        if (v.channels == 1)
            CHECK (v.right == v.left);
        else
            CHECK (v.right == v.left + v.frames);
    }
    CHECK (k.view (k.files.size()).left == nullptr);
}

TEST_CASE ("layOutKit refuses a kit that doesn't hold together", "[synthetic]")
{
    SyntheticSampleSource source;

    SECTION ("an unknown mix channel")
    {
        auto m = syntheticKitManifest();
        m.instruments[0].mixChannel = "nowhere";
        const auto laidOut = layOutKit (m, syntheticMixConfig(), source);
        CHECK_FALSE (laidOut.ok());
        CHECK (laidOut.error.find ("nowhere") != std::string::npos);
    }

    SECTION ("a dangling round-robin file")
    {
        auto m = syntheticKitManifest();
        m.instruments[0].zones[0].layers[0].rr[0] = 99;
        CHECK_FALSE (layOutKit (m, syntheticMixConfig(), source).ok());
    }

    SECTION ("a file of another length")
    {
        auto m = syntheticKitManifest();
        m.files[3].frames += 1;
        const auto laidOut = layOutKit (m, syntheticMixConfig(), source);
        CHECK_FALSE (laidOut.ok());
        CHECK (laidOut.error.find ("mismatch") == 0);
    }

    SECTION ("a file the source doesn't have")
    {
        auto m = syntheticKitManifest();
        m.files[5].path = "Drums/Nothing.wav";
        CHECK_FALSE (layOutKit (m, syntheticMixConfig(), source).ok());
    }
}

TEST_CASE ("the synthetic kit's folder holds the files the manifest lists", "[synthetic]")
{
    const auto folder = std::filesystem::path (WUBANSIA_TEST_TMP_DIR) / "synthetic" / "syn01" / "v1";
    std::filesystem::remove_all (folder);
    const auto written = writeSyntheticKitFolder (folder);
    REQUIRE (written.ok());
    CHECK (written.value == syn::fileCount);
    for (const auto& f : syntheticKitManifest().files)
    {
        const auto bytes = wubansia::util::readBinaryFile (folder / std::filesystem::path (f.path));
        REQUIRE (bytes.ok());
        CHECK (wubansia::util::sha256Hex (bytes.value) == f.sha256);
    }

    SyntheticSampleSource source;
    CHECK_FALSE (source.read ("Drums/Nothing.wav").ok());
    CHECK (source.read ("Loops/Lead Phrase.wav").ok());
    CHECK (source.reads == 1);
}
