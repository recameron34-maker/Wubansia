#pragma once

// Where a kit's audio comes from. P4's FileSampleSource reads WAVs under a content folder; the tests
// use testing/synthetic_kit.h's SyntheticSampleSource, which makes the synthetic kit's audio in code.
// read () runs on the service thread and may allocate, block and fail.

#include "util/result.h"

#include <cstdint>
#include <string_view>
#include <vector>

namespace wubansia::engine
{
    inline constexpr std::int32_t kContentSampleRate = 48000; // every sample the manifests list

    // One file's audio as 32-bit float, planar: channel c is audio[c * frames, (c + 1) * frames).
    // Mono files stay mono (channels == 1).
    struct SampleData
    {
        std::int64_t frames = 0;
        std::int32_t channels = 0; // 1 or 2
        std::int32_t sampleRate = kContentSampleRate;
        std::vector<float> audio;

        friend bool operator== (const SampleData&, const SampleData&) = default;
    };

    struct SampleSource
    {
        SampleSource() = default;
        SampleSource (const SampleSource&) = delete; // a source is used through a reference, never copied
        SampleSource& operator= (const SampleSource&) = delete;

        // The file at relativePath (kit.json's files[].path: forward slashes, relative to the kit's
        // folder). The error names the file and what is wrong; P4's reader starts it with one of
        // lfs_pointer, rate, format, truncated, missing or unreadable (an I/O error, or a folder at that path).
        virtual util::Result<SampleData> read (std::string_view relativePath) = 0;

        virtual ~SampleSource() = default;
    };
}
