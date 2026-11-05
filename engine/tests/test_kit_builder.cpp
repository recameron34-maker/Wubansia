// buildKit (kit/kit_builder.h): the manifests, a sample source and the timelines in, the Kit out, laid out as
// kit/kit.h describes; every file checked against its manifest entry; cancellable within one file; progress rising.

#include "manifest_samples.h"

#include "kit/file_sample_source.h"
#include "kit/kit_builder.h"
#include "kit/manifest.h"

#include "util/file_io.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

#if __has_include("testing/synthetic_kit.h")
    #include "testing/synthetic_kit.h"
    #define WUBANSIA_HAVE_SYNTHETIC_KIT 1
#else
    #define WUBANSIA_HAVE_SYNTHETIC_KIT 0
#endif

using namespace wubansia;
using namespace wubansia::engine;

namespace
{
    KitManifest testKit()
    {
        auto kit = parseKitManifest (test::kKitJson);
        REQUIRE (kit.error.empty());
        return std::move (kit.value);
    }

    MixConfig testMix (const KitManifest& kit)
    {
        auto mix = parseMixManifest (test::kMixJson, kit);
        REQUIRE (mix.error.empty());
        return std::move (mix.value);
    }

    // Every file of the manifest, made in code. (Sources aren't copied: SampleSource is an interface.)
    void addFiles (test::MemorySource& source, const KitManifest& kit)
    {
        for (std::size_t i = 0; i < kit.files.size(); ++i)
            source.files.emplace (kit.files[i].path,
                util::Result<SampleData>::success (test::sampleFor (kit.files[i].frames, kit.files[i].channels, static_cast<std::int32_t> (i))));
    }

    std::array<Timeline, 4> someTimelines()
    {
        std::array<Timeline, 4> t;
        for (std::size_t i = 0; i < t.size(); ++i)
        {
            t[i].length = i < 2 ? kLoopLength : kSongLength;
            t[i].loops = i < 2;
            Note n;
            n.pos = static_cast<std::int64_t> (i) * kMilliticksPerSixteenth;
            n.len = kMilliticksPerSixteenth;
            n.channel = 9;
            n.key = 36;
            n.velocity = 100;
            t[i].notes.push_back (n);
            t[i].sections.push_back ({ 0, "Intro" });
        }
        return t;
    }

    void noProgress (int) {}

    struct Built
    {
        util::Result<std::shared_ptr<const Kit>> result;
        std::vector<int> progress;
    };

    Built build (const KitManifest& kit, const MixConfig& mix, SampleSource& source, const std::atomic<bool>& cancel, std::int32_t index = -1)
    {
        Built b;
        b.result = buildKit (kit, mix, source, someTimelines(), cancel, [&] (int pct) { b.progress.push_back (pct); }, index);
        return b;
    }

    // 16-bit PCM WAV bytes for a file of the manifest, and the floats they read back as.
    std::vector<std::uint8_t> wav16 (std::int64_t frames, std::int32_t channels, std::int32_t seed, std::vector<float>& planar)
    {
        std::vector<std::uint8_t> b;
        auto put = [&] (std::uint32_t v, int bytes) {
            for (int i = 0; i < bytes; ++i)
                b.push_back (static_cast<std::uint8_t> ((v >> (8 * i)) & 0xFFu));
        };
        const auto dataBytes = static_cast<std::uint32_t> (frames * channels * 2);
        b.insert (b.end(), { 'R', 'I', 'F', 'F' });
        put (36 + dataBytes, 4);
        b.insert (b.end(), { 'W', 'A', 'V', 'E', 'f', 'm', 't', ' ' });
        put (16, 4);
        put (1, 2);
        put (static_cast<std::uint32_t> (channels), 2);
        put (48000, 4);
        put (48000u * static_cast<std::uint32_t> (channels) * 2, 4);
        put (static_cast<std::uint32_t> (channels) * 2, 2);
        put (16, 2);
        b.insert (b.end(), { 'd', 'a', 't', 'a' });
        put (dataBytes, 4);
        planar.assign (static_cast<std::size_t> (frames * channels), 0.0f);
        for (std::int64_t f = 0; f < frames; ++f)
            for (std::int32_t c = 0; c < channels; ++c)
            {
                const auto s = static_cast<std::int16_t> (((f * 131 + c * 977 + seed * 7919) % 60001) - 30000);
                put (static_cast<std::uint16_t> (s), 2);
                planar[static_cast<std::size_t> (c * frames + f)] = static_cast<float> (s) / 32768.0f;
            }
        return b;
    }

    std::filesystem::path scratch (std::string_view name)
    {
        const auto dir = std::filesystem::path (WUBANSIA_TEST_TMP_DIR) / "kit_builder" / std::string (name);
        std::filesystem::remove_all (dir);
        std::filesystem::create_directories (dir);
        return dir;
    }

    void writeFile (const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes)
    {
        std::filesystem::create_directories (path.parent_path());
        REQUIRE (util::writeBinaryFile (path, bytes).ok());
    }
}

TEST_CASE ("buildKit lays out the manifests as kit/kit.h describes", "[kit_builder]")
{
    const auto manifest = testKit();
    const auto mix = testMix (manifest);
    test::MemorySource source;
    addFiles (source, manifest);
    const std::atomic<bool> cancel { false };
    auto built = build (manifest, mix, source, cancel, 2);
    REQUIRE (built.result.error.empty());
    const Kit& kit = *built.result.value;

    // The kit.
    CHECK (kit.info.id == "TST01");
    CHECK (kit.info.title == "Test Signal");
    CHECK (kit.info.direction == "test / synthetic");
    CHECK (kit.info.key == "F# minor");
    CHECK (kit.info.camelot == "11A");
    CHECK (kit.info.bpm == 130);
    CHECK (kit.info.swingPm == 560);
    CHECK (kit.info.contentVersion == 3);
    CHECK (kit.info.index == 2);
    CHECK (kit.info.laneLabels == manifest.kit.lanes);
    CHECK (kit.info.ab == manifest.kit.ab);
    CHECK (kit.info.lanes == (0x3FFu & ~(laneBit (Lane::perc) | laneBit (Lane::accents))));
    CHECK (kit.info.abSwitches == 0b011u);
    CHECK (kit.info.extras == std::vector<KitExtra> { { 10, "Pad", Lane::chords } });

    // Files and their audio, in the manifest's order; mono stays mono.
    REQUIRE (kit.files.size() == 18);
    REQUIRE (kit.audio.size() == 18);
    for (std::size_t i = 0; i < kit.files.size(); ++i)
    {
        const auto& f = manifest.files[i];
        CHECK (kit.files[i] == KitFile { f.path, f.frames, f.channels, f.loopStart, f.loopEnd, f.prerollFrames });
        CHECK (kit.audio[i] == source.files.at (f.path).value);
    }
    CHECK (kit.audio[6].channels == 1);
    CHECK (kit.audio[6].audio.size() == 2400);
    const auto hat = kit.view (6);
    CHECK (hat.left == kit.audio[6].audio.data());
    CHECK (hat.right == hat.left);
    const auto bass = kit.view (10);
    CHECK (bass.right == bass.left + 48000);
    CHECK (bass.loopStart == 2000);
    CHECK (bass.loopEnd == 48000);
    CHECK (kit.view (13).prerollFrames == 960);

    // Instruments, zones, layers, round robins and pans: flat tables, appended in order.
    REQUIRE (kit.instruments.size() == 12);
    const std::vector<std::uint8_t> mixChannels { 0, 0, 1, 2, 2, 3, 3, 4, 6, 5, 7, 8 };
    const std::vector<std::uint16_t> firstZones { 0, 1, 2, 3, 4, 5, 7, 8, 9, 10, 11, 12 };
    const std::vector<std::uint16_t> firstPans { 0, 0, 0, 2, 5, 5, 5, 5, 5, 5, 5, 5 };
    for (std::size_t i = 0; i < 12; ++i)
    {
        CAPTURE (i);
        const auto& in = kit.instruments[i];
        const auto& e = manifest.instruments[i];
        CHECK (in.id == e.id);
        CHECK (in.name == e.name);
        CHECK (in.lane == e.lane);
        CHECK (in.mixChannel == mixChannels[i]);
        CHECK (in.mode == e.mode);
        CHECK (in.align == e.align);
        CHECK (in.attackUs == e.attackUs);
        CHECK (in.releaseUs == e.releaseUs);
        CHECK (in.gainMdb == e.gainMdb);
        CHECK (in.startFrames == e.startFrames);
        CHECK (in.velocityLaw == e.velocityLaw);
        CHECK (in.interp == e.interp);
        CHECK (in.mono == e.mono);
        CHECK (in.glide == e.glide);
        CHECK (in.restrikeContinues == e.restrikeContinues);
        CHECK (in.duckKey == e.duckKey);
        CHECK (in.rrBlock16ths == e.rrBlock16ths);
        CHECK (in.firstZone == firstZones[i]);
        CHECK (in.zoneCount == e.zones.size());
        CHECK (in.firstPan == firstPans[i]);
        CHECK (in.panCount == e.panCyclePm.size());
    }
    CHECK (kit.instruments[6].layeredWith == 7);
    CHECK (kit.instruments[5].layeredWith == -1);
    CHECK (kit.instruments[3].choke == Choke { 1, ChokeRole::chokes, 4000, ChokeCurve::cos2 });
    CHECK (kit.instruments[4].choke == Choke { 1, ChokeRole::choked, 4000, ChokeCurve::cos2 });
    CHECK (kit.instruments[0].choke == Choke {});
    CHECK (kit.pans == std::vector<std::int16_t> { -200, 200, -300, 0, 300 });

    REQUIRE (kit.zones.size() == 13);
    CHECK (kit.zones[1] == Zone { 36, 36, 36, 1, 2 });
    CHECK (kit.zones[5] == Zone { 30, 0, 36, 6, 1 });
    CHECK (kit.zones[6] == Zone { 37, 37, 127, 7, 1 });
    REQUIRE (kit.layers.size() == 14);
    CHECK (kit.layers[0] == Layer { 127, 0, 1 });
    CHECK (kit.layers[1] == Layer { 80, 1, 2 });
    CHECK (kit.layers[2] == Layer { 127, 3, 2 });
    CHECK (kit.layers[9] == Layer { 127, 11, 1 });
    CHECK (kit.rr == std::vector<std::uint16_t> { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 17, 11, 12, 13, 14 });

    // What each (channel, key) plays.
    CHECK (kit.keys[9][36] == KeyTarget { 0, 1, static_cast<std::int8_t> (AbSwitch::kick) });
    CHECK (kit.keys[9][39] == KeyTarget { 2, -1, -1 });
    CHECK (kit.keys[9][42] == KeyTarget { 3, -1, -1 });
    CHECK (kit.keys[9][46] == KeyTarget { 4, -1, -1 });
    CHECK (kit.keys[9][37] == KeyTarget {});
    for (int key = 0; key < 128; ++key)
    {
        CAPTURE (key);
        const auto k = static_cast<std::size_t> (key);
        CHECK (kit.keys[0][k] == KeyTarget { 5, 6, static_cast<std::int8_t> (AbSwitch::bass) });
        CHECK (kit.keys[1][k] == KeyTarget { 9, -1, -1 });
        CHECK (kit.keys[2][k] == KeyTarget {}); // channel 3: the voicing, ignored
        CHECK (kit.keys[4][k] == KeyTarget { 10, -1, -1 });
        CHECK (kit.keys[6][k] == (key == 48 ? KeyTarget { 11, -1, -1 } : KeyTarget {}));
        CHECK (kit.keys[10][k] == KeyTarget { 8, -1, -1 });
        CHECK (kit.keys[11][k] == KeyTarget {});
    }

    // CCs reach the mix channels of the keymap's instruments, what they're layered with, and the phrases.
    std::array<std::uint32_t, 16> cc {};
    cc[9] = 0b111; // kick, snare, hats
    cc[0] = (1u << 3) | (1u << 4); // bass A and B, and B's sub layer
    cc[1] = 1u << 5; // the lead and its phrase
    cc[4] = 1u << 7; // vox
    cc[6] = 1u << 8; // fx
    cc[10] = 1u << 6; // the pad, on chords
    CHECK (kit.ccMixChannels == cc);

    // The phrase and its slices, holds zero-filled past holdCount.
    REQUIRE (kit.phrases.size() == 1);
    Phrase p;
    p.lane = Lane::lead;
    p.channel = 1;
    p.ab = static_cast<std::int8_t> (AbSwitch::bass);
    p.mixChannel = 5;
    p.gainMdb = 0;
    p.stem = { 15, 16 };
    p.cycle16ths = 32;
    p.tolerancePm = 20;
    p.fadeUs = 5000;
    p.firstSlice = 0;
    p.sliceCount = 4;
    CHECK (kit.phrases[0] == p);
    REQUIRE (kit.slices.size() == 4);
    CHECK (kit.slices[0] == PhraseSlice { 0, 60, 0, 22000, 21000, 0, {}, 0 });
    CHECK (kit.slices[1] == PhraseSlice { 4, 62, 21500, 44000, 43000, 500, { 62, 0, 0, 0, 0, 0, 0, 0 }, 1 });
    CHECK (kit.slices[3] == PhraseSlice { 12, 65, 66000, 90000, 88000, 0, { 67, 69 }, 2 });

    // The mix and the timelines, moved in whole.
    CHECK (kit.mix == mix);
    CHECK (kit.timelines == someTimelines());
}

TEST_CASE ("buildKit: a phrase without an alternative plays stem A on both sides", "[kit_builder]")
{
    auto manifest = testKit();
    manifest.phrases[0].stemB = -1;
    manifest.phrases[0].ab = -1;
    manifest.files.pop_back(); // keep the layout simple: drop nothing that matters, then put it back
    manifest = testKit();
    manifest.phrases[0].stemB = -1;
    manifest.phrases[0].ab = -1;
    const auto mix = testMix (testKit());
    test::MemorySource source;
    addFiles (source, manifest);
    const std::atomic<bool> cancel { false };
    const auto built = build (manifest, mix, source, cancel);
    REQUIRE (built.result.error.empty());
    CHECK (built.result.value->phrases[0].stem == std::array<std::uint16_t, 2> { 15, 15 });
    CHECK (built.result.value->phrases[0].ab == -1);
}

TEST_CASE ("buildKit checks every file against its manifest entry, naming it", "[kit_builder]")
{
    const auto manifest = testKit();
    const auto mix = testMix (manifest);
    const std::atomic<bool> cancel { false };

    struct Case
    {
        std::string_view what;
        std::size_t file;
        std::function<util::Result<SampleData> (const FileEntry&)> gives;
        std::string_view starts;
        LoadError kind;
    };
    const Case cases[] {
        { "more frames", 3, [] (const FileEntry& f) { return util::Result<SampleData>::success (test::sampleFor (f.frames + 1, f.channels, 3)); },
            "mismatch: Drums/Kick B v2 rr1.wav: 4803 frames", LoadError::mismatch },
        { "fewer frames", 15, [] (const FileEntry& f) { return util::Result<SampleData>::success (test::sampleFor (f.frames - 1, f.channels, 3)); },
            "mismatch: Loops/Lead Phrase A.wav: 191999 frames", LoadError::mismatch },
        { "stereo for mono", 6, [] (const FileEntry& f) { return util::Result<SampleData>::success (test::sampleFor (f.frames, 2, 6)); },
            "mismatch: Drums/Hat Closed.wav: 2400 frames and 2 channels; kit.json lists 2400 frames and 1 channel", LoadError::mismatch },
        { "mono for stereo", 0, [] (const FileEntry& f) { return util::Result<SampleData>::success (test::sampleFor (f.frames, 1, 0)); },
            "mismatch: Drums/Kick A.wav: ", LoadError::mismatch },
        { "another rate", 5, [] (const FileEntry& f) {
             auto d = test::sampleFor (f.frames, f.channels, 5);
             d.sampleRate = 44100;
             return util::Result<SampleData>::success (d);
         },
            "rate: Drums/Clap.wav: 44100 Hz", LoadError::mismatch },
        { "a source error", 5, [] (const FileEntry&) { return util::Result<SampleData>::failure ("lfs_pointer: Drums/Clap.wav: a Git LFS pointer"); },
            "lfs_pointer: Drums/Clap.wav: a Git LFS pointer", LoadError::lfsPointers },
        { "a missing file", 17, [] (const FileEntry&) { return util::Result<SampleData>::failure ("missing: Bass/Sub Layer.wav: no such file"); },
            "missing: Bass/Sub Layer.wav: ", LoadError::missing },
        { "a source that miscounts", 7, [] (const FileEntry& f) {
             auto d = test::sampleFor (f.frames, f.channels, 7);
             d.audio.pop_back();
             return util::Result<SampleData>::success (d);
         },
            "failed: Drums/Hat Open.wav: ", LoadError::failed },
    };
    for (const auto& c : cases)
    {
        CAPTURE (c.what);
        test::MemorySource source;
        addFiles (source, manifest);
        const auto& entry = manifest.files[c.file];
        source.files.at (entry.path) = c.gives (entry);
        auto timelines = someTimelines();
        const auto result = buildKit (manifest, mix, source, std::move (timelines), cancel, noProgress);
        REQUIRE_FALSE (result.ok());
        CAPTURE (result.error);
        CHECK (result.error.starts_with (c.starts));
        CHECK (loadErrorOf (result.error) == c.kind);
        CHECK (result.value == nullptr);
        // It stops at the first bad file.
        CHECK (source.reads.size() == c.file + 1);
        // The caller's timelines stay with the caller when the build fails.
        CHECK (timelines == someTimelines());
    }
}

TEST_CASE ("buildKit refuses manifests that don't fit each other or the Kit", "[kit_builder]")
{
    const auto manifest = testKit();
    const auto mix = testMix (manifest);
    const std::atomic<bool> cancel { false };
    auto expectRefused = [&] (const KitManifest& m, const MixConfig& x, std::string_view says) {
        test::MemorySource source;
        addFiles (source, manifest);
        const auto result = buildKit (m, x, source, someTimelines(), cancel, noProgress);
        REQUIRE_FALSE (result.ok());
        CAPTURE (result.error);
        CHECK (result.error.starts_with ("manifest: "));
        CHECK (result.error.find (says) != std::string::npos);
        CHECK (loadErrorOf (result.error) == LoadError::failed);
        CHECK (source.reads.empty()); // refused before reading a file
    };

    SECTION ("a mix channel the mix hasn't")
    {
        auto x = mix;
        x.channels[4].id = "subby";
        expectRefused (manifest, x, "instruments[7].mix_channel");
    }
    SECTION ("a phrase's mix channel the mix hasn't")
    {
        auto m = manifest;
        m.phrases[0].mixChannel = "lead_stem";
        expectRefused (m, mix, "phrases[0].mix_channel");
    }
    SECTION ("an instrument on another lane than its channel")
    {
        auto m = manifest;
        m.instruments[2].lane = Lane::perc;
        expectRefused (m, mix, "instruments[2]");
    }
    SECTION ("another kit's mix")
    {
        auto x = mix;
        x.kit = "TST02";
        expectRefused (manifest, x, "TST02");
    }
    SECTION ("another content version's mix")
    {
        auto x = mix;
        x.contentVersion = 4;
        expectRefused (manifest, x, "content version");
    }
    SECTION ("a dangling file")
    {
        auto m = manifest;
        m.instruments[1].zones[0].layers[1].rr[0] = 18;
        expectRefused (m, mix, "instruments[1].zones[0].layers[1].rr[0]");
    }
    SECTION ("a dangling stem")
    {
        auto m = manifest;
        m.phrases[0].stemB = 40;
        expectRefused (m, mix, "phrases[0].stems");
    }
    SECTION ("a dangling instrument in the keymap")
    {
        auto m = manifest;
        m.keymap[3].a = 12;
        expectRefused (m, mix, "keymap[3]");
    }
    SECTION ("a dangling layer")
    {
        auto m = manifest;
        m.instruments[6].layeredWith = 12;
        expectRefused (m, mix, "instruments[6].layered_with");
    }
    SECTION ("too many holds")
    {
        auto m = manifest;
        m.phrases[0].slices[0].holdCount = kMaxHolds + 1;
        expectRefused (m, mix, "phrases[0].slices[0].holds");
    }
    SECTION ("a key out of range")
    {
        auto m = manifest;
        m.keymap[0].hi = 128;
        expectRefused (m, mix, "keymap[0]");
    }
    SECTION ("more files than the Kit's tables hold")
    {
        auto m = manifest;
        m.files.resize (65536, m.files[0]);
        expectRefused (m, mix, "65536 files");
    }
    SECTION ("more mix channels than the Kit's masks hold")
    {
        auto x = mix;
        while (x.channels.size() <= kMaxMixChannels)
            x.channels.push_back (x.channels.back());
        expectRefused (manifest, x, "25 mix channels");
    }
}

TEST_CASE ("buildKit stops within one file of a cancel", "[kit_builder]")
{
    const auto manifest = testKit();
    const auto mix = testMix (manifest);

    SECTION ("raised before the build: nothing is read")
    {
        test::MemorySource source;
        addFiles (source, manifest);
        const std::atomic<bool> cancel { true };
        const auto built = build (manifest, mix, source, cancel);
        REQUIRE_FALSE (built.result.ok());
        CHECK (built.result.error.starts_with ("cancelled: "));
        CHECK (loadErrorOf (built.result.error) == LoadError::cancelled);
        CHECK (source.reads.empty());
    }

    SECTION ("raised while a file is read: the build stops before the next")
    {
        for (const std::size_t at : { std::size_t { 0 }, std::size_t { 5 }, std::size_t { 17 } })
        {
            CAPTURE (at);
            test::MemorySource source;
            addFiles (source, manifest);
            std::atomic<bool> cancel { false };
            source.beforeRead = [&] (std::size_t read) {
                if (read == at)
                    cancel.store (true);
            };
            const auto built = build (manifest, mix, source, cancel);
            CHECK (source.reads.size() == at + 1);
            if (at + 1 < manifest.files.size())
            {
                REQUIRE_FALSE (built.result.ok());
                CHECK (built.result.error == "cancelled: the load was cancelled after " + std::to_string (at + 1) + " of 18 files");
            }
            else
                CHECK (built.result.ok()); // the last file was read: the kit is done
        }
    }
}

TEST_CASE ("buildKit's progress only rises, from 0 to 100", "[kit_builder]")
{
    const auto manifest = testKit();
    const auto mix = testMix (manifest);
    test::MemorySource source;
    addFiles (source, manifest);
    const std::atomic<bool> cancel { false };
    const auto built = build (manifest, mix, source, cancel);
    REQUIRE (built.result.ok());
    REQUIRE (built.progress.size() >= 3);
    CHECK (built.progress.front() == 0);
    CHECK (built.progress.back() == 100);
    for (std::size_t i = 1; i < built.progress.size(); ++i)
        CHECK (built.progress[i] > built.progress[i - 1]);

    // A failed build rises too, and stops short of 100.
    test::MemorySource failing;
    addFiles (failing, manifest);
    failing.files.at ("Loops/Lead Phrase B.wav") = util::Result<SampleData>::failure ("missing: Loops/Lead Phrase B.wav: gone");
    const auto failed = build (manifest, mix, failing, cancel);
    REQUIRE_FALSE (failed.result.ok());
    REQUIRE_FALSE (failed.progress.empty());
    CHECK (failed.progress.back() < 100);
    for (std::size_t i = 1; i < failed.progress.size(); ++i)
        CHECK (failed.progress[i] > failed.progress[i - 1]);
}

TEST_CASE ("buildKit and checkKitFiles read a kit folder through FileSampleSource", "[kit_builder]")
{
    const auto manifest = testKit();
    const auto mix = testMix (manifest);
    const auto root = scratch ("tst01");
    std::vector<std::vector<float>> expected (manifest.files.size());
    for (std::size_t i = 0; i < manifest.files.size(); ++i)
    {
        const auto& f = manifest.files[i];
        writeFile (root / std::filesystem::path (f.path), wav16 (f.frames, f.channels, static_cast<std::int32_t> (i), expected[i]));
    }
    FileSampleSource source (root);

    const auto checked = checkKitFiles (manifest, source);
    REQUIRE (checked.error.empty());
    CHECK (checked.value == 18);

    const std::atomic<bool> cancel { false };
    const auto built = build (manifest, mix, source, cancel);
    REQUIRE (built.result.error.empty());
    for (std::size_t i = 0; i < manifest.files.size(); ++i)
    {
        CAPTURE (i);
        CHECK (built.result.value->audio[i].channels == manifest.files[i].channels);
        CHECK (built.result.value->audio[i].audio == expected[i]);
    }

    SECTION ("checkKitFiles names the first problem, as buildKit would")
    {
        struct Case
        {
            std::string_view what;
            std::function<void()> breakIt;
            std::string_view starts;
            LoadError kind;
        };
        std::vector<float> scratchAudio;
        const Case cases[] {
            { "a missing file", [&] { std::filesystem::remove (root / "Drums" / "Clap.wav"); }, "missing: Drums/Clap.wav: ", LoadError::missing },
            { "an LFS pointer", [&] {
                 const std::string pointer = "version https://git-lfs.github.com/spec/v1\noid sha256:00\nsize 24044\n";
                 writeFile (root / "Drums" / "Clap.wav", std::vector<std::uint8_t> (pointer.begin(), pointer.end()));
             },
                "lfs_pointer: Drums/Clap.wav: ", LoadError::lfsPointers },
            { "a file of another length", [&] { writeFile (root / "Drums" / "Clap.wav", wav16 (5999, 2, 5, scratchAudio)); },
                "mismatch: Drums/Clap.wav: 5999 frames", LoadError::mismatch },
            { "a mono file for a stereo one", [&] { writeFile (root / "Drums" / "Clap.wav", wav16 (6000, 1, 5, scratchAudio)); },
                "mismatch: Drums/Clap.wav: ", LoadError::mismatch },
            { "a 44.1 kHz file", [&] {
                 auto bytes = wav16 (6000, 2, 5, scratchAudio);
                 bytes[24] = 0x44; // 44100 = 0xAC44
                 bytes[25] = 0xAC;
                 bytes[26] = 0;
                 writeFile (root / "Drums" / "Clap.wav", bytes);
             },
                "rate: Drums/Clap.wav: 44100 Hz", LoadError::mismatch },
            { "a cut-off file", [&] {
                 auto bytes = wav16 (6000, 2, 5, scratchAudio);
                 bytes.resize (bytes.size() - 100);
                 writeFile (root / "Drums" / "Clap.wav", bytes);
             },
                "truncated: Drums/Clap.wav: ", LoadError::failed },
        };
        for (const auto& c : cases)
        {
            CAPTURE (c.what);
            c.breakIt();
            const auto problem = checkKitFiles (manifest, source);
            REQUIRE_FALSE (problem.ok());
            CAPTURE (problem.error);
            CHECK (problem.error.starts_with (c.starts));
            CHECK (loadErrorOf (problem.error) == c.kind);
            const auto failed = build (manifest, mix, source, cancel);
            REQUIRE_FALSE (failed.result.ok());
            CHECK (failed.result.error.starts_with (c.starts));
        }
    }
}

TEST_CASE ("loadErrorOf sorts errors by their code", "[kit_builder]")
{
    CHECK (loadErrorOf ("") == LoadError::none);
    CHECK (loadErrorOf ("missing: Drums/Kick.wav: no such file") == LoadError::missing);
    CHECK (loadErrorOf ("lfs_pointer: Drums/Kick.wav: a pointer") == LoadError::lfsPointers);
    CHECK (loadErrorOf ("mismatch: Drums/Kick.wav: 3 frames") == LoadError::mismatch);
    CHECK (loadErrorOf ("rate: Drums/Kick.wav: 44100 Hz") == LoadError::mismatch);
    CHECK (loadErrorOf ("cancelled: the load was cancelled") == LoadError::cancelled);
    CHECK (loadErrorOf ("format: Drums/Kick.wav: 8-bit") == LoadError::failed);
    CHECK (loadErrorOf ("truncated: Drums/Kick.wav: cut") == LoadError::failed);
    CHECK (loadErrorOf ("unreadable: Drums/Kick.wav: a folder") == LoadError::failed);
    CHECK (loadErrorOf ("manifest: kit.json: x") == LoadError::failed);
    CHECK (loadErrorOf ("failed: out of memory") == LoadError::failed);
    CHECK (loadErrorOf ("kit.json: kit.bpm: 1 is out of range") == LoadError::failed);
    CHECK (loadErrorOf ("missingno") == LoadError::failed);
}

TEST_CASE ("fileMismatch says what differs", "[kit_builder]")
{
    const FileEntry entry { "Bass/Sub.wav", 1000, 1, std::string (64, 'a'), 0, 0, 0 };
    CHECK (fileMismatch (entry, 1000, 1, 48000).empty());
    CHECK (fileMismatch (entry, 999, 1, 48000) == "mismatch: Bass/Sub.wav: 999 frames and 1 channel; kit.json lists 1000 frames and 1 channel");
    CHECK (fileMismatch (entry, 1000, 2, 48000) == "mismatch: Bass/Sub.wav: 1000 frames and 2 channels; kit.json lists 1000 frames and 1 channel");
    CHECK (fileMismatch (entry, 1000, 1, 44100) == "rate: Bass/Sub.wav: 44100 Hz; the content is 48000 Hz");
}

#if WUBANSIA_HAVE_SYNTHETIC_KIT
TEST_CASE ("buildKit and layOutKit agree on the synthetic kit", "[kit_builder]")
{
    // engine/testing's layOutKit is the reference layout (P0): buildKit must give the very same Kit.
    const auto kitManifest = testing::syntheticKitManifest();
    const auto mixConfig = testing::syntheticMixConfig();
    testing::SyntheticSampleSource referenceSource;
    const auto reference = testing::layOutKit (kitManifest, mixConfig, referenceSource);
    REQUIRE (reference.error.empty());

    testing::SyntheticSampleSource source;
    const std::atomic<bool> cancel { false };
    const auto built = buildKit (kitManifest, mixConfig, source, std::array<Timeline, 4> {}, cancel, noProgress);
    REQUIRE (built.error.empty());
    CHECK (*built.value == *reference.value);
    CHECK (source.reads == static_cast<int> (kitManifest.files.size()));

    // And the synthetic manifests' JSON parses back to the same structs.
    const auto parsedKit = parseKitManifest (testing::syntheticKitJson());
    REQUIRE (parsedKit.error.empty());
    CHECK (parsedKit.value == kitManifest);
    const auto parsedMix = parseMixManifest (testing::syntheticMixJson(), parsedKit.value);
    REQUIRE (parsedMix.error.empty());
    CHECK (parsedMix.value == mixConfig);
}
#endif

TEST_CASE ("buildKit builds the committed kits from their real WAVs", "[.real]")
{
    // Hidden: it reads 160-200 MB of WAV. Run it on a content folder (the repo's layout, <kit>/ or <kit>/v<N>/):
    //     WUBANSIA_CONTENT_DIR=/path/to/samples engine_tests "[.real]"
    // WUBANSIA_MANIFEST_DIR points at other manifests than the checkout's content/plugin. Without a mix.json
    // (P3's), the kit plays through a bare mix: one channel per id kit.json names.
    const char* content = std::getenv ("WUBANSIA_CONTENT_DIR");
    if (content == nullptr)
        SKIP ("set WUBANSIA_CONTENT_DIR to a content folder");
    const char* manifestDir = std::getenv ("WUBANSIA_MANIFEST_DIR");
    const auto manifests = manifestDir != nullptr ? std::filesystem::path (manifestDir) : std::filesystem::path (WUBANSIA_SOURCE_DIR) / "content" / "plugin";

    for (const std::string folder : { "fut01", "fut02", "fut03", "fut04" })
    {
        std::string id;
        for (const char c : folder)
            id += static_cast<char> (c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c);
        CAPTURE (id);
        const auto kitBytes = util::readBinaryFile (manifests / folder / (id + "_kit.json"));
        REQUIRE (kitBytes.ok());
        const auto kit = parseKitManifest (std::string_view (reinterpret_cast<const char*> (kitBytes.value.data()), kitBytes.value.size()));
        REQUIRE (kit.error.empty());
        const auto& m = kit.value;

        MixConfig mix;
        const auto mixPath = manifests / folder / (id + "_mix.json");
        if (std::filesystem::exists (mixPath))
        {
            const auto mixBytes = util::readBinaryFile (mixPath);
            REQUIRE (mixBytes.ok());
            auto parsed = parseMixManifest (std::string_view (reinterpret_cast<const char*> (mixBytes.value.data()), mixBytes.value.size()), m);
            REQUIRE (parsed.error.empty());
            mix = std::move (parsed.value);
        }
        else
        {
            mix.kit = m.kit.id;
            mix.contentVersion = m.kit.contentVersion;
            auto add = [&] (const std::string& channel, Lane lane) {
                if (mix.channelIndex (channel) >= 0)
                    return;
                Channel c;
                c.id = channel;
                c.lane = static_cast<std::uint8_t> (lane);
                mix.channels.push_back (c);
            };
            for (const auto& in : m.instruments)
                add (in.mixChannel, in.lane);
            for (const auto& p : m.phrases)
                add (p.mixChannel, p.lane);
        }

        auto root = std::filesystem::path (content) / folder / ("v" + std::to_string (m.kit.contentVersion));
        if (! std::filesystem::is_directory (root))
            root = std::filesystem::path (content) / folder;
        FileSampleSource source (root);

        using Clock = std::chrono::steady_clock;
        const auto t0 = Clock::now();
        const auto checked = checkKitFiles (m, source);
        const auto t1 = Clock::now();
        REQUIRE (checked.error.empty());

        const std::atomic<bool> cancel { false };
        std::vector<int> progress;
        const auto built = buildKit (m, mix, source, std::array<Timeline, 4> {}, cancel, [&] (int pct) { progress.push_back (pct); });
        const auto t2 = Clock::now();
        REQUIRE (built.error.empty());
        const Kit& k = *built.value;
        REQUIRE (k.audio.size() == m.files.size());
        std::int64_t samples = 0;
        int mono = 0;
        for (std::size_t i = 0; i < m.files.size(); ++i)
        {
            CHECK (k.audio[i].frames == m.files[i].frames);
            CHECK (k.audio[i].channels == m.files[i].channels);
            mono += k.audio[i].channels == 1 ? 1 : 0;
            samples += k.audio[i].frames * k.audio[i].channels;
        }
        CHECK (progress.back() == 100);

        const auto ms = [] (Clock::duration d) { return std::chrono::duration_cast<std::chrono::milliseconds> (d).count(); };
        std::ostringstream line;
        line << id << ": " << m.files.size() << " files (" << mono << " mono), " << m.instruments.size() << " instruments, " << m.phrases.size()
             << " phrases, " << mix.channels.size() << " mix channels; " << (samples * 4 + 500'000) / 1'000'000 << " MB of float audio; check "
             << ms (t1 - t0) << " ms, build " << ms (t2 - t1) << " ms";
        WARN (line.str());
    }
}
