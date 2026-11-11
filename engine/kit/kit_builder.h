#pragma once

// buildKit (P4): the two parsed manifests, a sample source and the four prebuilt timelines in, one immutable Kit
// out, laid out exactly as kit/kit.h describes. It runs on the service thread (or groovecore render's): it reads
// every listed file, allocates the Kit's audio (tens of MB), and may take a second. The audio thread only ever gets
// the finished Kit, by pointer (engine.h: Engine::publish).
//
// Errors are one line starting with a code, which loadErrorOf () sorts into the plugin's content statuses:
// - the source's own: "missing: <path>: ...", "lfs_pointer: <path>: ...", "rate: ...", "format: ...",
//   "truncated: ...", "unreadable: ..." (file_sample_source.h, wav_reader.h);
// - "mismatch: <path>: 1000 frames and 2 channels; kit.json lists 1200 frames and 2 channels";
// - "cancelled: the load was cancelled after 3 of 52 files";
// - "manifest: ..." the manifests don't fit each other or the Kit's tables (parseKitManifest and parseMixManifest
//   refuse these first; buildKit checks again for structs made in code);
// - "failed: ..." anything else (out of memory).

#include "kit/file_sample_source.h"
#include "kit/function_ref.h"
#include "kit/kit.h"
#include "kit/manifest.h"
#include "kit/sample_source.h"
#include "mix/mix_config.h"
#include "sched/timeline.h"
#include "util/result.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace wubansia::engine
{
    // Builds the Kit. It reads the files in kit.json's order, checking cancel before each one, so a raised flag
    // stops the build within one file ("cancelled: ..."). progress (pct) reports 0, then rising values as the
    // files' audio comes in (by size), then 100, each value higher than the last. Every file must match its
    // manifest entry: frames, channels and 48 kHz. Mono files stay mono. The timelines are moved into the Kit only
    // when it is built. kitIndex is the kit's place in the plugin's kit list (KitInfo::index; -1 for none).
    [[nodiscard]] util::Result<std::shared_ptr<const Kit>> buildKit (const KitManifest& manifest, MixConfig mix, SampleSource& source,
        std::array<Timeline, 4>&& timelines, const std::atomic<bool>& cancel, FunctionRef<void (int pct)> progress,
        std::int32_t kitIndex = -1);

    // What a load error means to the plugin's content status (the design's ready / missing / lfs_pointers /
    // mismatch / failed), from its code.
    enum class LoadError : std::uint8_t {
        none, // "": no error
        missing, // missing: a listed file isn't there
        lfsPointers, // lfs_pointer: the samples are Git LFS pointers (git lfs pull)
        mismatch, // mismatch, rate: the files are another version of the content
        cancelled, // cancelled: a newer request replaced this one
        failed // format, truncated, unreadable, manifest, failed, or anything else
    };

    [[nodiscard]] LoadError loadErrorOf (std::string_view error) noexcept;

    // "" if a file with these frames, channels and rate is what entry lists, else the "mismatch: ..." (or "rate:
    // ...") error naming it.
    [[nodiscard]] std::string fileMismatch (const FileEntry& entry, std::int64_t frames, std::int32_t channels, std::int32_t sampleRate);

    // Whether every file kit.json lists is ready under source's root, from the headers alone (probe): it exists,
    // isn't an LFS pointer, is a WAV the reader takes, at 48 kHz, with the listed frames and channels. The first
    // problem is the error, in buildKit's words; success gives the number of files checked. The installer checks
    // SHA-256; this doesn't. cancel, if given, is checked before each file: a raised flag stops the check within one
    // probe ("cancelled: the check was cancelled after 3 of 189 files", which loadErrorOf sorts as cancelled).
    [[nodiscard]] util::Result<int> checkKitFiles (const KitManifest& manifest, const FileSampleSource& source,
        const std::atomic<bool>* cancel = nullptr);

    // The same check through any probe (path -> its WavInfo or error, as FileSampleSource::probe gives them).
    [[nodiscard]] util::Result<int> checkKitFiles (const KitManifest& manifest, FunctionRef<util::Result<WavInfo> (std::string_view path)> probe,
        const std::atomic<bool>* cancel = nullptr);
}
