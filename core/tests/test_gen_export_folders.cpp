// writeFiles and the --out folder: a version's regions/ folder holds exactly one song's regions.
// Region files are named by order, kind and length only (03_drop_32bars.mid), so a second song
// written into the same folder could leave the first one's regions beside its own, where a producer
// dragging the folder into a DAW would take them for part of it. writeFiles refuses that, before it
// writes anything, and never deletes a file.

#include "gen/content.h"
#include "gen/export.h"
#include "gen/song.h"
#include "util/file_io.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

using namespace wubansia;
using namespace wubansia::gen;
using Catch::Matchers::ContainsSubstring;

namespace
{
    Song songOf (std::uint64_t seed, std::int32_t bars)
    {
        static const auto bundle = loadBundle (std::filesystem::path (WUBANSIA_SOURCE_DIR) / "content", "FUT01");
        REQUIRE (bundle.ok());
        Options o;
        o.seed = seed;
        o.bars = bars;
        auto s = generate (bundle.value, o);
        REQUIRE (s.ok());
        return std::move (s.value);
    }

    std::filesystem::path freshFolder (const char* name)
    {
        const auto dir = std::filesystem::path (WUBANSIA_TEST_TMP_DIR) / name;
        std::error_code ec;
        std::filesystem::remove_all (dir, ec);
        return dir;
    }

    std::vector<std::uint8_t> bytesOf (const std::filesystem::path& p)
    {
        auto r = util::readBinaryFile (p);
        REQUIRE (r.ok());
        return r.value;
    }
}

TEST_CASE ("writeFiles writes the same song into its own folder again", "[gen][export][folders]")
{
    const auto out = freshFolder ("export_folders_same");
    const Song s = songOf (1, 0);
    REQUIRE (writeFiles (s, out, true, true).ok());
    const auto before = bytesOf (out / "feel" / "regions.json");
    const auto again = writeFiles (s, out, true, true);
    INFO (again.error);
    REQUIRE (again.ok());
    CHECK (bytesOf (out / "feel" / "regions.json") == before);
}

TEST_CASE ("writeFiles refuses a regions folder that holds another song's regions", "[gen][export][folders]")
{
    const auto out = freshFolder ("export_folders_other");
    const Song full = songOf (1, 0);
    REQUIRE (writeFiles (full, out, true, true).ok());
    const auto feelJson = bytesOf (out / "feel" / "regions.json");
    const auto quantizedJson = bytesOf (out / "quantized" / "regions.json");

    // The first 16 bars: one region, 01_intro_16bars.mid, which the full song's eight don't include.
    const Song cut = songOf (1, 16);
    const auto refused = writeFiles (cut, out, true, true);
    REQUIRE_FALSE (refused.ok());
    CHECK_THAT (refused.error, ContainsSubstring ("regions"));
    CHECK_THAT (refused.error, ContainsSubstring ("01_intro_32bars.mid")); // the first stale file, by name
    CHECK_THAT (refused.error, ContainsSubstring ("its own --out folder"));

    // Nothing was written: not the new region, not either version's regions.json.
    CHECK_FALSE (std::filesystem::exists (out / "feel" / "regions" / "01_intro_16bars.mid"));
    CHECK_FALSE (std::filesystem::exists (out / "quantized" / "regions" / "01_intro_16bars.mid"));
    CHECK (bytesOf (out / "feel" / "regions.json") == feelJson);
    CHECK (bytesOf (out / "quantized" / "regions.json") == quantizedJson);
    // and nothing was deleted
    CHECK (std::filesystem::exists (out / "feel" / "regions" / "08_outro_32bars.mid"));

    // Only the version being written is checked: the quantized folder alone is clean for the cut.
    const auto other = freshFolder ("export_folders_other_quantized");
    REQUIRE (writeFiles (full, other, true, false).ok());
    const auto quantizedOnly = writeFiles (cut, other, false, true);
    INFO (quantizedOnly.error);
    CHECK (quantizedOnly.ok());
}

TEST_CASE ("writeFiles refuses a folder that holds another seed of the same kit", "[gen][export][folders]")
{
    // Every seed of a kit has the same region names (one form per style), so the region check alone
    // can't tell two seeds apart: the full file's name and regions.json do.
    const auto out = freshFolder ("export_folders_seed");
    const Song one = songOf (1, 0);
    REQUIRE (writeFiles (one, out, true, true).ok());
    const auto feelJson = bytesOf (out / "feel" / "regions.json");
    const auto drop = bytesOf (out / "feel" / "regions" / "04_drop_32bars.mid");

    const Song two = songOf (2, 0);
    const auto refused = writeFiles (two, out, true, true);
    REQUIRE_FALSE (refused.ok());
    CHECK_THAT (refused.error, ContainsSubstring ("FUT01_seed1_134bpm_11A_full.mid"));
    CHECK_THAT (refused.error, ContainsSubstring ("its own --out folder"));
    // nothing was written, nothing deleted
    CHECK_FALSE (std::filesystem::exists (out / "feel" / "FUT01_seed2_134bpm_11A_full.mid"));
    CHECK_FALSE (std::filesystem::exists (out / "quantized" / "FUT01_seed2_134bpm_11A_full.mid"));
    CHECK (bytesOf (out / "feel" / "regions.json") == feelJson);
    CHECK (bytesOf (out / "feel" / "regions" / "04_drop_32bars.mid") == drop);
    CHECK (std::filesystem::exists (out / "feel" / "FUT01_seed1_134bpm_11A_full.mid"));

    // the other version's folder counts too: one --out folder is one song, in both versions
    const auto split = freshFolder ("export_folders_seed_split");
    REQUIRE (writeFiles (one, split, true, false).ok());
    const auto other = writeFiles (two, split, false, true);
    REQUIRE_FALSE (other.ok());
    CHECK_THAT (other.error, ContainsSubstring ("feel"));
    CHECK_FALSE (std::filesystem::exists (split / "quantized"));

    // a regions.json of another song refuses too, also with its full file gone
    const auto json = freshFolder ("export_folders_seed_json");
    REQUIRE (writeFiles (one, json, true, false).ok());
    std::filesystem::remove (json / "feel" / "FUT01_seed1_134bpm_11A_full.mid");
    const auto byJson = writeFiles (two, json, true, false);
    REQUIRE_FALSE (byJson.ok());
    CHECK_THAT (byJson.error, ContainsSubstring ("regions.json"));
    CHECK_THAT (byJson.error, ContainsSubstring ("seed 1"));

    // and so does a regions.json it can't read
    const std::string junk = "{ \"kit\": ";
    REQUIRE (util::writeBinaryFile (json / "feel" / "regions.json", std::vector<std::uint8_t> (junk.begin(), junk.end())).ok());
    const auto unreadable = writeFiles (one, json, true, false);
    REQUIRE_FALSE (unreadable.ok());
    CHECK_THAT (unreadable.error, ContainsSubstring ("regions.json"));

    // the same seed again is still fine
    const auto same = freshFolder ("export_folders_seed_same");
    REQUIRE (writeFiles (two, same, true, false).ok());
    const auto again = writeFiles (two, same, true, true);
    INFO (again.error);
    CHECK (again.ok());
}

TEST_CASE ("writeFiles counts only MIDI files in a regions folder", "[gen][export][folders]")
{
    const auto out = freshFolder ("export_folders_notes");
    std::error_code ec;
    std::filesystem::create_directories (out / "feel" / "regions", ec);
    REQUIRE_FALSE (ec);
    const std::string text = "my notes\n";
    REQUIRE (util::writeBinaryFile (out / "feel" / "regions" / "notes.txt", std::vector<std::uint8_t> (text.begin(), text.end())).ok());
    const auto written = writeFiles (songOf (1, 16), out, true, false);
    INFO (written.error);
    CHECK (written.ok());

    // but any .mid file it doesn't write is refused, whoever put it there
    REQUIRE (util::writeBinaryFile (out / "feel" / "regions" / "mine.MID", std::vector<std::uint8_t> (text.begin(), text.end())).ok());
    const auto refused = writeFiles (songOf (1, 16), out, true, false);
    REQUIRE_FALSE (refused.ok());
    CHECK_THAT (refused.error, ContainsSubstring ("mine.MID"));
}
