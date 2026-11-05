// The engine's contracts as P0 lands them: the permanent lane order, the channel table, and the stubs
// (the silent engine, the pass-through mixer) that the other packages replace.

#include "alloc_counter.h"

#include "engine.h"
#include "kit/channel_map.h"
#include "mix/mixer.h"
#include "sched/phrase_match.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <array>
#include <cmath>
#include <functional>
#include <limits>
#include <string_view>
#include <vector>

using namespace wubansia::engine;

namespace
{
    // Exact comparison, as these tests mean it; std::equal_to keeps -Wfloat-equal quiet and NaN unequal.
    template <class T>
    bool same (T a, T b)
    {
        return std::equal_to<T> {}(a, b);
    }

    // Output buffers prefilled with NaN, so a sample the engine doesn't write shows up.
    struct Output
    {
        explicit Output (int numSamples)
            : left (static_cast<std::size_t> (numSamples), std::numeric_limits<float>::quiet_NaN()),
              right (static_cast<std::size_t> (numSamples), std::numeric_limits<float>::quiet_NaN())
        {
        }

        std::vector<float> left; // declared before channels, which points into them
        std::vector<float> right;
        float* channels[2] { left.data(), right.data() };

        bool silent (int numOutputs, int numSamples) const
        {
            for (int i = 0; i < numSamples; ++i)
            {
                const auto s = static_cast<std::size_t> (i);
                if (! same (left[s], 0.0f) || (numOutputs > 1 && ! same (right[s], 0.0f)))
                    return false;
            }
            return true;
        }
    };

    TransportInfo playingAt (double bpm)
    {
        TransportInfo t;
        t.hasPosition = true;
        t.playing = true;
        t.ppq = 0.0;
        t.bpm = bpm;
        return t;
    }
}

TEST_CASE ("the lanes keep their permanent order and ids", "[smoke]")
{
    // Parameter IDs follow this order (<lane>_level, <lane>_mute), so it never changes.
    const std::array<std::string_view, 10> expected { "kick", "snare", "hats", "perc", "bass", "lead", "chords", "vox", "accents", "fx" };
    CHECK (kLanes == 10);
    CHECK (kLaneIds == expected);
    CHECK (static_cast<int> (Lane::kick) == 0);
    CHECK (static_cast<int> (Lane::fx) == 9);
    CHECK (laneId (Lane::accents) == "accents");
    CHECK (laneIndex ("vox") == 7);
    CHECK (laneIndex ("Vox") == -1);

    CHECK (kAbSwitchIds == std::array<std::string_view, 3> { "kick", "bass", "stab" });
    const Controls defaults;
    CHECK (defaults.ab == std::array<std::uint8_t, 3> { 1, 1, 0 }); // kick B, bass B, stab A: what the club loops play
    CHECK (defaults.mode == PlayMode::loop);
    CHECK (defaults.variant == Variant::vox);
    CHECK (defaults.club);
    for (const float g : defaults.laneGain)
        CHECK (same (g, 1.0f));
}

TEST_CASE ("the channel table equals tools/sound/channels.py", "[smoke]")
{
    // MIDI v2 (ADR 0019; channels.py at midi-v2 9efb865): CHANNELS, KEY_BASE, DRUM_RANGE, ONE_KEY_ROLES,
    // ROLE_LANE and TRACKS. A change lands in both files.
    struct Row
    {
        std::string_view role;
        int channel;
        int lane; // ROLE_LANE, -1 where channels.py has none
        bool oneKey;
    };
    const Row rows[] {
        { "bass", 1, static_cast<int> (Lane::bass), false },
        { "lead", 2, static_cast<int> (Lane::lead), false },
        { "voicing", 3, static_cast<int> (Lane::chords), false },
        { "chords", 4, static_cast<int> (Lane::chords), false },
        { "vox", 5, static_cast<int> (Lane::vox), false },
        { "accents", 6, static_cast<int> (Lane::accents), true },
        { "fx", 7, static_cast<int> (Lane::fx), true },
        { "chops", 8, static_cast<int> (Lane::vox), true },
        { "reserved", 9, -1, false },
        { "drums", 10, -1, true },
    };
    REQUIRE (std::size (rows) == static_cast<std::size_t> (kChannelRoles));
    for (std::size_t i = 0; i < std::size (rows); ++i)
    {
        const auto role = static_cast<ChannelRole> (i);
        CAPTURE (rows[i].role);
        CHECK (kChannelRoleIds[i] == rows[i].role);
        CHECK (channelOf (role) == rows[i].channel);
        CHECK (roleOf (rows[i].channel) == static_cast<int> (i));
        CHECK (roleLane (role) == rows[i].lane);
        CHECK (isOneKeyRole (role) == rows[i].oneKey);
    }
    CHECK (roleOf (0) == -1);
    CHECK (roleOf (11) == -1);
    CHECK (kFirstExtraChannel == 11);
    CHECK (kLastExtraChannel == 16);
    CHECK (kKeyBaseAccents == 48);
    CHECK (kKeyBaseFx == 48);
    CHECK (kKeyBaseChops == 48);
    CHECK (kDrumKeyLo == 35);
    CHECK (kDrumKeyHi == 59);

    struct TrackRow
    {
        std::string_view name;
        std::string_view role;
        std::string_view lane;
    };
    const TrackRow tracks[] {
        { "Kick", "drums", "kick" },
        { "Rumble", "drums", "kick" },
        { "Snare", "drums", "snare" },
        { "Clap", "drums", "snare" },
        { "Snare/Clap", "drums", "snare" },
        { "Hats", "drums", "hats" },
        { "Perc", "drums", "perc" },
        { "Bass", "bass", "bass" },
        { "Lead", "lead", "lead" },
        { "Chords", "chords", "chords" },
        { "Chord voicing", "voicing", "chords" },
        { "Vox", "vox", "vox" },
        { "Vox chops", "chops", "vox" },
        { "Accents", "accents", "accents" },
        { "FX", "fx", "fx" },
    };
    REQUIRE (std::size (tracks) == kTracks.size());
    for (std::size_t i = 0; i < kTracks.size(); ++i)
    {
        CAPTURE (tracks[i].name);
        CHECK (kTracks[i].name == tracks[i].name);
        CHECK (kChannelRoleIds[static_cast<std::size_t> (kTracks[i].role)] == tracks[i].role);
        CHECK (laneId (kTracks[i].lane) == tracks[i].lane);
    }

    // A kit's extras resolve by name (FUT02's, from channels.py's EXTRAS).
    KitInfo fut02;
    fut02.extras = { { 10, "Drop Glide", Lane::bass }, { 11, "Laser Chord Stab", Lane::chords }, { 12, "Glass Ping", Lane::accents } };
    CHECK (trackLane (fut02, "Glass Ping") == static_cast<int> (Lane::accents));
    CHECK (trackLane (fut02, "Rumble") == static_cast<int> (Lane::kick));
    CHECK (trackLane (fut02, "Organ Bass") == -1);
}

TEST_CASE ("timelineIndex orders the Kit's four timelines", "[smoke]")
{
    CHECK (timelineIndex (PlayMode::loop, Variant::vox) == 0);
    CHECK (timelineIndex (PlayMode::loop, Variant::novox) == 1);
    CHECK (timelineIndex (PlayMode::song, Variant::vox) == 2);
    CHECK (timelineIndex (PlayMode::song, Variant::novox) == 3);
    CHECK (timelineIndex (PlayMode::off, Variant::vox) == -1);
    CHECK (kLoopLength == 15360 * kMilliticksPerTick);
    CHECK (kSongLength == 860160 * kMilliticksPerTick);
    CHECK (kMilliticksPerSixteenth == 240000);
}

TEST_CASE ("the engine stub renders silence at every rate, block size and output count", "[smoke]")
{
    const double rate = GENERATE (11025.0, 22050.0, 44100.0, 48000.0, 96000.0, 192000.0);
    const int outputs = GENERATE (1, 2);
    CAPTURE (rate, outputs);

    Engine engine;
    engine.prepare (rate, 512, outputs);
    Mixer mixer; // P8's: the engine reports the mix's latency and tail, fixed per rate whatever the kit
    mixer.prepare (rate, 512, outputs);
    CHECK (engine.latencySamples() == mixer.latencySamples());
    CHECK (engine.latencySamples() > 0);
    CHECK (engine.tailSeconds() >= 6.0);

    const Controls controls;
    const auto transport = playingAt (128.0);
    const MidiEvent midi[] { { 0, 0x99, 36, 100 }, { 10, 0x89, 36, 0 }, { 11, 0xB0, 74, 64 } };
    for (const int block : { 512, 1, 7, 137, 0, 512 })
    {
        CAPTURE (block);
        Output out (512);
        engine.process (out.channels, outputs, block, transport, controls, midi, block > 11 ? 3 : 0);
        CHECK (out.silent (outputs, block));
    }

    engine.reset();
    engine.prepare (44100.0, 64, outputs); // again, at another rate, with nothing in between
    Output out (64);
    engine.process (out.channels, outputs, 64, TransportInfo {}, controls, nullptr, 0);
    CHECK (out.silent (outputs, 64));

    const auto& t = engine.telemetry();
    CHECK (t.kitIndex.load() == -1);
    CHECK (t.voices.load() == 0);
    CHECK (t.songTicks.load() < 0.0); // -1: not playing
    CHECK (t.laneHits.load() == 0u);
}

TEST_CASE ("the engine adopts a published kit at its next block and hands it back once replaced", "[smoke]")
{
    Kit first, second, third, fourth;
    first.info.index = 0;
    second.info.index = 1;
    third.info.index = 2;
    fourth.info.index = 3;

    Engine engine;
    engine.prepare (48000.0, 64, 2);
    const Controls controls;
    const auto transport = playingAt (128.0);
    auto render = [&] {
        Output out (64);
        engine.process (out.channels, 2, 64, transport, controls, nullptr, 0);
        return out.silent (2, 64);
    };
    std::vector<const Kit*> retired;
    auto collect = [&] { engine.collectRetired ([&retired] (const Kit* kit) { retired.push_back (kit); }); };

    CHECK (engine.publish (&first) == nullptr);
    collect();
    CHECK (retired.empty());
    CHECK (render());
    CHECK (engine.telemetry().kitIndex.load() == 0);
    collect();
    CHECK (retired.empty()); // first is playing

    CHECK (engine.publish (&second) == nullptr); // first was adopted, so nothing comes back
    CHECK (render());
    CHECK (engine.telemetry().kitIndex.load() == 1);
    collect();
    CHECK (retired == std::vector<const Kit*> { &first });

    CHECK (engine.publish (&third) == nullptr);
    CHECK (engine.publish (&fourth) == &third); // never adopted: the service may free it at once
    CHECK (render());
    CHECK (engine.telemetry().kitIndex.load() == 3);
    collect();
    CHECK (retired == std::vector<const Kit*> { &first, &second });
}

TEST_CASE ("the engine stub neither allocates nor frees while processing", "[smoke][alloc]")
{
    if (! allocations::available())
        SKIP ("a sanitizer owns the allocation functions in this build");

    Kit kit, next;
    Engine engine;
    engine.prepare (48000.0, 256, 2);
    const Controls controls;
    const auto transport = playingAt (130.0);
    const MidiEvent midi[] { { 0, 0x90, 60, 100 }, { 100, 0x80, 60, 0 } };
    Output out (256);
    engine.publish (&kit);

    allocations::Scope scope;
    for (int block = 0; block < 10; ++block)
    {
        if (block == 5)
            engine.publish (&next); // a kit adoption mid-stream, and the old one retired
        engine.process (out.channels, 2, 256, transport, controls, midi, 2);
    }
    const auto counts = scope.counts();
    CHECK (counts.allocations == 0);
    CHECK (counts.frees == 0);
}

// P0's pass-through mixer is gone (P8's mix/mixer.cpp replaces it): its unity sum, lane gains and mono fold are
// test_mixer.cpp's.

TEST_CASE ("the phrase matcher stub leaves every note on the multisample", "[smoke]")
{
    Timeline timeline;
    timeline.notes.resize (3);
    const auto before = timeline;
    annotatePhrases (timeline, KitManifest {});
    CHECK (timeline == before);
}

TEST_CASE ("the parser stubs refuse until P4 lands", "[smoke]")
{
    CHECK_FALSE (parseKitManifest ("{}").ok());
    CHECK_FALSE (parseMixManifest ("{}", KitManifest {}).ok());
}
