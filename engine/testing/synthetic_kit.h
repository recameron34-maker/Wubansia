#pragma once

// The synthetic kit, SYN01: a small, complete kit made in code, so every package tests against the
// same content without samples (CI has none). Everything in it is deterministic, built from integer
// and basic floating-point arithmetic only (no libm), so its bytes are the same on every build and
// platform.
//
// What it has (docs/plugin/manifest.md's fields, each used at least once):
// - 128 BPM, G minor (6A), swing 0.56: a 16th is 5625 frames at 48 kHz, an odd 16th lands 675 frames
//   (28.8 ticks) late, and the MIDI writes it 29 ticks late, as tools/sound/midi.py does;
// - kick A/B on note 36 (B: 2 layers x 2 round robins), a snare (2 x 2), a closed hat (2 round robins,
//   a pan cycle) choking an open hat in one group;
// - bass A/B on channel 1: 3 sustain-loop zones each (G1 D2 G2, F1 played on G1 pitched down), mono
//   legato with a raised-cosine glide and dips (A, Hermite) or an exponential glide (B, sinc, 2 layers);
// - a lead on channel 2 with a recorded phrase: a looped 2-bar stem with 8 contiguous slices, one swung,
//   one holding a retrigger, one with no pre-roll (so its fade_in_us applies), the last wrapping through
//   the stem's end to slice 0's pre-roll; its throw is baked; and a CC74 ride (CC71 empty) that the files' lead track writes in every cycle (the stem isn't
//   filtered: the ride is there to test matching);
// - a 2-zone stab on channel 4, and a held pad on channel 11 (a kit extra) with restrike-continue;
// - a vox zone with 3 velocity layers and a 20 ms pre-roll; an end-aligned riser with start_frames;
// - a mix of 9 channels using every insert type, both duck modes and both kinds of `when`.
// The lanes perc and accents are empty, and stab has no A/B: kits differ in what they have.
//
// The four SMFs (Loop and Song, vox and no-vox) follow midi_plan: 960 PPQ, a conductor with tempo, 4/4,
// the key signature, section markers and cues, then one track per lane on docs/midi-map.md's channels.
// The Song is 16 bars: Intro (drums), Drop (everything), Outro (kick and pad).

#include "engine.h"
#include "kit/kit.h"
#include "kit/manifest.h"
#include "kit/sample_source.h"
#include "mix/mix_config.h"
#include "util/result.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace wubansia::engine::testing
{
    inline constexpr std::string_view kSyntheticKitId = "SYN01";
    inline constexpr std::int32_t kSyntheticBpm = 128;
    inline constexpr std::int32_t kSyntheticSwingPm = 560;
    inline constexpr std::int64_t kSyntheticSixteenthFrames = 5625; // at 48 kHz and 128 BPM
    inline constexpr std::int64_t kSyntheticSwingFrames = 675; // an odd 16th's delay at 48 kHz
    inline constexpr std::int64_t kSyntheticSwingTicks = 29; // round (28.8): what the MIDI writes
    inline constexpr std::int64_t kSyntheticLoopTicks = 15360; // 4 bars
    inline constexpr std::int64_t kSyntheticSongTicks = 61440; // 16 bars

    // Indices into the synthetic manifest's instruments.
    namespace syn
    {
        enum Instrument : int {
            kickA,
            kickB,
            snare,
            hatClosed,
            hatOpen,
            bassA,
            bassB,
            lead,
            stab,
            pad,
            vox,
            riser,
            instrumentCount
        };

        inline constexpr int fileCount = 30;
        inline constexpr int stemFile = 29; // the lead phrase's stem
        inline constexpr int phraseSlices = 8;
    }

    // Everything the synthetic kit is, as bytes and structs. buildSyntheticContent () makes it from
    // scratch (two calls give equal bytes); syntheticContent () makes it once and keeps it.
    struct SyntheticContent
    {
        KitManifest kit;
        MixConfig mix;
        std::string kitJson; // kitManifestJson (kit)
        std::string mixJson; // mixManifestJson (mix)
        std::vector<SampleData> audio; // per file, as 24-bit PCM holds it, so a WAV round trip is exact
        std::vector<std::vector<std::uint8_t>> wavs; // per file: 24-bit PCM WAV, 'fmt ' and 'data' only
        std::array<std::vector<std::uint8_t>, 4> smfs; // by timelineIndex: Loop vox, Loop no-vox, Song vox, Song no-vox
    };

    SyntheticContent buildSyntheticContent();
    const SyntheticContent& syntheticContent();

    KitManifest syntheticKitManifest();
    MixConfig syntheticMixConfig();
    std::string syntheticKitJson();
    std::string syntheticMixJson();
    std::vector<std::uint8_t> syntheticSmf (PlayMode mode, Variant variant); // empty for PlayMode::off

    // Reads the synthetic kit's files by their manifest paths.
    class SyntheticSampleSource : public SampleSource
    {
    public:
        util::Result<SampleData> read (std::string_view relativePath) override;

        int reads = 0; // successful reads, for cache tests
    };

    // Writes every file under kitFolder at its manifest path (a content folder's <kit>/v1/). Returns how
    // many it wrote.
    util::Result<int> writeSyntheticKitFolder (const std::filesystem::path& kitFolder);

    // 24-bit PCM WAV bytes of audio (whose samples must be 24-bit values, as SampleData from here are).
    std::vector<std::uint8_t> wavBytes (const SampleData& audio);

    // The Kit laid out from two manifests and a sample source exactly as kit/kit.h describes: the
    // reference for P4's buildKit, and a way to make test kits before it exists. Timelines stay empty
    // (P7 builds them). Refuses dangling references and files that don't match the manifest.
    util::Result<std::shared_ptr<Kit>> layOutKit (const KitManifest& kit, const MixConfig& mix, SampleSource& source);

    // The synthetic Kit, laid out once and shared.
    std::shared_ptr<const Kit> makeSyntheticKit();
}
