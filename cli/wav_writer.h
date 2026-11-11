#pragma once

// WAV files for groovecore render (P12): planar float audio in, the bytes of a RIFF WAVE file out, little-endian on
// every platform, so the same samples give the same bytes everywhere.
// - float32: WAVE_FORMAT_IEEE_FLOAT, 32-bit, an 18-byte 'fmt ' chunk (cbSize 0) and a 'fact' chunk, as libsndfile
//   writes them; the samples as they are, bit for bit (what the fidelity checks read).
// - pcm24: 24-bit PCM, each sample clamped to [-1, 1) and rounded half up: floor (x x 2^23 + 0.5).
// Nothing else goes in the file: no PEAK, LIST or timestamp chunk.

#include "util/result.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

namespace wubansia::cli
{
    enum class WavFormat : std::uint8_t {
        float32,
        pcm24
    };

    // The file's bytes: channels.size () channels (1 or 2), each frames long. Refuses (error) unequal channels, no
    // channels or more than 2, a rate outside 8000-384000 Hz, and data a RIFF size can't hold (4 GB).
    [[nodiscard]] util::Result<std::vector<std::uint8_t>> wavBytes (std::span<const std::span<const float>> channels, int sampleRate,
        WavFormat format);

    // Writes them to path (the folder must exist), replacing any file there. Gives the bytes written.
    [[nodiscard]] util::Result<std::size_t> writeWav (const std::filesystem::path& path, std::span<const std::span<const float>> channels,
        int sampleRate, WavFormat format);
}
