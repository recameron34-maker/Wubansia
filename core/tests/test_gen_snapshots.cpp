// The generator snapshots (docs/plans/S1-futuristic-slice.md §4.2, "Proof"): the SHA-256 of every
// file `groovecore gen` writes for 16 seeds x {feel, quantized} x the four kits, committed as
// core/tests/snapshots/gen.sha256. This test regenerates them in memory and compares the whole
// manifest byte for byte; cli/tests/snapshots.cmake checks the same file through groovecore itself,
// files on disk included.
//
// These are generator snapshots, not goldens (CLAUDE.md, "Determinism and golden files"). A
// deliberate change to the generator or to content/ changes them: say so and why, then copy the
// fresh manifest this test writes into the build tree, in its own commit:
//     cp <build>/core/tests/tmp/snapshots/gen.sha256 core/tests/snapshots/gen.sha256
// An accidental change is a determinism bug: fix the code, never the manifest.

#include "gen/content.h"
#include "gen/export.h"
#include "gen/song.h"
#include "util/file_io.h"
#include "util/sha256.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

using namespace wubansia;
using namespace wubansia::gen;

namespace
{
    const std::filesystem::path sourceDir = WUBANSIA_SOURCE_DIR;
    const std::filesystem::path committedPath = sourceDir / "core" / "tests" / "snapshots" / "gen.sha256";
    const std::filesystem::path freshPath = std::filesystem::path (WUBANSIA_TEST_TMP_DIR) / "snapshots" / "gen.sha256";

    // What the snapshots cover. Changing either list changes the manifest, in its own commit.
    constexpr std::array<std::string_view, 4> snapshotKits = { "FUT01", "FUT02", "FUT03", "FUT04" };
    // 0 to 10 (the S1b1 listening seeds are 1 to 10), a few spread out, and the extremes of 64 bits.
    constexpr std::array<std::uint64_t, 16> snapshotSeeds = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 42, 977, 65536, 123456789, 18446744073709551615ull };

    std::string shown (const std::filesystem::path& p)
    {
        const auto s = p.generic_u8string();
        return std::string (s.begin(), s.end());
    }

    // path -> SHA-256, for every file groovecore gen --kit <kit> --seed <seed> --out <kit>/seed<seed>
    // writes, by path from the folder that holds the kit folders.
    std::map<std::string, std::string> freshEntries()
    {
        std::map<std::string, std::string> entries;
        for (const std::string_view kit : snapshotKits)
        {
            const auto bundle = loadBundle (sourceDir / "content", kit);
            INFO (kit << ": " << bundle.error);
            REQUIRE (bundle.ok());
            for (const std::uint64_t seed : snapshotSeeds)
            {
                Options options;
                options.seed = seed;
                const auto song = generate (bundle.value, options);
                INFO (kit << " seed " << seed << ": " << song.error);
                REQUIRE (song.ok());
                for (const Version v : { Version::feel, Version::quantized })
                {
                    const auto files = renderFiles (song.value, v);
                    INFO (kit << " seed " << seed << " " << versionName (v) << ": " << files.error);
                    REQUIRE (files.ok());
                    for (const auto& f : files.value)
                    {
                        const std::string path = std::string (kit) + "/seed" + std::to_string (seed) + "/" + std::string (versionName (v)) + "/" + f.path;
                        const bool fresh = entries.emplace (path, util::sha256Hex (f.bytes)).second;
                        INFO (path);
                        REQUIRE (fresh); // two files on one path would overwrite each other
                    }
                }
            }
        }
        return entries;
    }

    std::string manifestText (const std::map<std::string, std::string>& entries)
    {
        std::string kits, seeds;
        for (const auto k : snapshotKits)
            kits += " " + std::string (k);
        for (const auto s : snapshotSeeds)
            seeds += " " + std::to_string (s);

        std::string text;
        text += "# Generator snapshots (docs/plans/S1-futuristic-slice.md section 4.2): the SHA-256 of every file\n";
        text += "# groovecore gen writes for 16 seeds x 2 versions (feel, quantized) x 4 kits, by path, where\n";
        text += "# <KIT>/seed<N> is the --out folder. core/tests/test_gen_snapshots.cpp regenerates them and\n";
        text += "# compares, and cli/tests/snapshots.cmake runs groovecore gen itself. Snapshots, not goldens:\n";
        text += "# they change only in their own commit (CLAUDE.md, \"Determinism and golden files\").\n";
        text += "# kits" + kits + "\n";
        text += "# seeds" + seeds + "\n";
        text += "# files " + std::to_string (entries.size()) + "\n";
        for (const auto& [path, hash] : entries) // std::map: sorted by path in byte order
            text += hash + "  " + path + "\n";
        return text;
    }

    std::map<std::string, std::string> parseEntries (const std::string& text)
    {
        std::map<std::string, std::string> out;
        std::istringstream lines (text);
        for (std::string line; std::getline (lines, line);)
            if (!line.empty() && line[0] != '#' && line.size() > 66 && line.compare (64, 2, "  ") == 0)
                out.emplace (line.substr (66), line.substr (0, 64));
        return out;
    }

    // Up to `limit` items, one per line.
    std::string listed (const std::vector<std::string>& items, std::size_t limit = 20)
    {
        std::string s;
        for (std::size_t i = 0; i < items.size() && i < limit; ++i)
            s += "\n    " + items[i];
        if (items.size() > limit)
            s += "\n    ... and " + std::to_string (items.size() - limit) + " more";
        return s;
    }
}

TEST_CASE ("the generator snapshots match core/tests/snapshots/gen.sha256", "[gen][snapshots]")
{
    const auto entries = freshEntries();
    const std::string fresh = manifestText (entries);

    std::error_code ec;
    std::filesystem::create_directories (freshPath.parent_path(), ec);
    REQUIRE_FALSE (ec);
    REQUIRE (util::writeBinaryFile (freshPath, std::vector<std::uint8_t> (fresh.begin(), fresh.end())).ok());

    const auto committedBytes = util::readBinaryFile (committedPath);
    const std::string committed = committedBytes.ok() ? std::string (committedBytes.value.begin(), committedBytes.value.end()) : std::string();
    if (committed == fresh)
    {
        SUCCEED (entries.size() << " files match");
        return;
    }

    // What differs, then what to do.
    std::ostringstream why;
    if (!committedBytes.ok())
        why << "There is no committed manifest (" << committedBytes.error << ").\n";
    else
    {
        const auto old = parseEntries (committed);
        std::vector<std::string> changed, missing, added;
        for (const auto& [path, hash] : entries)
        {
            const auto it = old.find (path);
            if (it == old.end())
                added.push_back (path);
            else if (it->second != hash)
                changed.push_back (path);
        }
        for (const auto& [path, hash] : old)
            if (!entries.contains (path))
                missing.push_back (path);
        if (!changed.empty())
            why << changed.size() << " files have another SHA-256:" << listed (changed) << "\n";
        if (!added.empty())
            why << added.size() << " files are new:" << listed (added) << "\n";
        if (!missing.empty())
            why << missing.size() << " files are no longer written:" << listed (missing) << "\n";
        if (changed.empty() && added.empty() && missing.empty())
            why << "Every file matches, but the manifest's text differs (its header, its line ends or its order).\n";
    }
    why << "See how: diff -u " << shown (committedPath) << " " << shown (freshPath) << "\n"
        << "An accidental change is a determinism bug: fix the code, never the manifest. A deliberate change to the\n"
        << "generator or to content/ regenerates the snapshots in its own commit (CLAUDE.md), with the reason:\n"
        << "    cp " << shown (freshPath) << " core/tests/snapshots/gen.sha256";
    FAIL (why.str());
}
