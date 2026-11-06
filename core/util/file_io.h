#pragma once

// Whole-file binary reads and writes, for the groovecore commands and tests: MIDI files, regions.json.
// Never on the audio thread (CLAUDE.md): these block on the disk.

#include "util/result.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

namespace wubansia::util
{
    // The file's bytes, or an error naming the path ("cannot open", "read failed").
    [[nodiscard]] Result<std::vector<std::uint8_t>> readBinaryFile (const std::filesystem::path& path);

    // Writes bytes to path, replacing what was there; the folder must exist. Gives the number of
    // bytes written, or an error naming the path.
    [[nodiscard]] Result<std::size_t> writeBinaryFile (const std::filesystem::path& path, std::span<const std::uint8_t> bytes);
}
