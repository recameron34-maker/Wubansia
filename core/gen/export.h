#pragma once

// Export (plan §2.3; the extended-mix brief): per seed and version ("feel" and "quantized"),
// 1. the full arrangement as one SMF type 1 file at 480 PPQ: a conductor track (name, tempo, 4/4,
//    the kit's key signature, a marker FF 06 at every section start) and one track per lane;
// 2. one region file per section, all lanes, named <order>_<kind>_<bars>bars.mid
//    (03_drop_32bars.mid), so a producer can drag regions onto a DAW timeline;
// 3. regions.json: each region's file, start bar, length, kind and active lanes, its lanes per
//    8-bar block and its one-bar dropouts (each with its start bar, 1 bar and the lanes that sit it
//    out; [] for none).
// No note crosses a section boundary, so the regions tile the song exactly: the region files'
// events, each moved to its section's start, are the full file's events.
//
// Files for a version go to <out>/<version>/: the full file, regions/ and regions.json.

#include "gen/song.h"
#include "midi/writer.h"
#include "util/result.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace wubansia::gen
{
    enum class Version : std::uint8_t {
        feel, // the performance
        quantized // the score
    };

    std::string_view versionName (Version version) noexcept; // "feel", "quantized"

    // "03_drop_32bars.mid": the section's 1-based order (2 digits), kind and bars.
    std::string regionFileName (const Section& section);
    // "FUT01_seed7_134bpm_11A_full.mid"
    std::string fullFileName (const Song& song);

    // The marker text at a section's start: "drop (04, 32 bars)". The kind comes first: the metrics
    // read a section's kind from the marker's first word (tools/legacy_audit/README.md, "Sections").
    std::string sectionMarker (const Section& section);

    midi::SongSpec fullSpec (const Song& song, Version version);
    midi::SongSpec regionSpec (const Song& song, Version version, std::size_t section);

    // regions.json for one version (UTF-8, LF line ends, integers and strings only).
    std::string regionsJson (const Song& song, Version version);

    struct OutputFile
    {
        std::string path; // relative to the version folder, '/' separated
        std::vector<std::uint8_t> bytes;
    };

    // Every file of a version, in a fixed order: the full file, the region files, regions.json.
    [[nodiscard]] util::Result<std::vector<OutputFile>> renderFiles (const Song& song, Version version);

    // Writes the chosen versions under outDir (created as needed). Gives the paths written. One
    // outDir per song: it fails, before writing anything, if either version folder (written or not)
    // holds another song's full file (another *_full.mid) or a regions.json of another kit or seed,
    // or one it can't read, or if a version's regions/ folder being written holds a .mid file this
    // song doesn't write (another song's region, or the same song cut with --bars). It never
    // deletes a file.
    [[nodiscard]] util::Result<std::vector<std::filesystem::path>> writeFiles (const Song& song, const std::filesystem::path& outDir, bool feel, bool quantized);
}
