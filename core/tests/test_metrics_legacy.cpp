// The C++ metrics on the 516-file legacy baseline give core/tests/legacy_expected.json: the "test of
// the test" (docs/plans/S1-futuristic-slice.md §5.1, tools/legacy_audit/README.md). Every value must
// match exactly, except DFA alpha, which may differ by 1 (README, "Tolerances for the C++ port").
// legacy_expected.json is never regenerated to make this pass: if it fails, fix the metric code.
//
// The baseline isn't committed: it is exported into build/golden/ by golden_check
// (cmake --build --preset release --target golden_check) or by node tools/export_golden.js. Without
// it this test is skipped and says so. WUBANSIA_GOLDEN_DIR in the environment points it elsewhere;
// ctest sets it from the CMake cache variable of that name, golden_check's export folder.
// Like the audit, it first checks every file against content/golden/legacy-v21.sha256, so it always
// measures the committed baseline.

#include "metrics/suite.h"
#include "metrics/value.h"
#include "util/file_io.h"
#include "util/sha256.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

using namespace wubansia;
using namespace wubansia::metrics;

namespace
{
    const std::filesystem::path sourceDir = WUBANSIA_SOURCE_DIR;

    // SHA-256 (FIPS 180-4), to check the export against the manifest as audit.js does.
    using util::sha256Hex;

    std::filesystem::path goldenDir()
    {
        if (const char* env = std::getenv ("WUBANSIA_GOLDEN_DIR"); env != nullptr && *env != '\0')
            return env;
        return sourceDir / "build" / "golden";
    }

    struct ManifestEntry
    {
        std::string hash;
        std::string path;
    };

    std::vector<ManifestEntry> readManifest()
    {
        const auto bytes = util::readBinaryFile (sourceDir / "content" / "golden" / "legacy-v21.sha256");
        REQUIRE (bytes.ok());
        std::vector<ManifestEntry> out;
        std::istringstream lines (std::string (bytes.value.begin(), bytes.value.end()));
        for (std::string line; std::getline (lines, line);)
        {
            if (line.empty() || line[0] == '#')
                continue;
            REQUIRE (line.size() > 66);
            REQUIRE (line.substr (64, 2) == "  ");
            out.push_back ({ line.substr (0, 64), line.substr (66) });
        }
        return out;
    }

    const Value& expectedAudit()
    {
        static const Value v = [] {
            auto r = readJsonFile (sourceDir / "core" / "tests" / "legacy_expected.json");
            INFO (r.error);
            REQUIRE (r.ok());
            return r.value;
        }();
        return v;
    }

    // The baseline, checked against the manifest; empty if the export isn't there (the test skips).
    struct Baseline
    {
        std::vector<Song> songs;
        std::string skipReason;
    };

    const Baseline& baseline()
    {
        static const Baseline b = [] {
            Baseline out;
            const auto dir = goldenDir();
            const auto manifest = readManifest();
            REQUIRE (manifest.size() == 516);
            std::size_t present = 0;
            for (const auto& e : manifest)
                if (std::filesystem::is_regular_file (dir / e.path))
                    ++present;
            if (present == 0)
            {
                out.skipReason = "the legacy baseline isn't exported: no files in " + dir.string()
                                 + ". Run cmake --build --preset release --target golden_check, or node tools/export_golden.js,"
                                   " then run this test again (CI runs golden_check first).";
                return out;
            }
            std::vector<std::string> problems;
            for (const auto& e : manifest)
            {
                const auto bytes = util::readBinaryFile (dir / e.path);
                if (!bytes.ok())
                {
                    problems.push_back ("missing " + e.path);
                    continue;
                }
                if (sha256Hex (bytes.value) != e.hash)
                {
                    problems.push_back (e.path + " differs from content/golden/legacy-v21.sha256");
                    continue;
                }
                auto song = loadSong (bytes.value, e.path);
                if (!song.ok())
                {
                    problems.push_back (song.error);
                    continue;
                }
                out.songs.push_back (std::move (song.value));
            }
            for (std::size_t i = 0; i < problems.size() && i < 10; ++i)
                UNSCOPED_INFO (problems[i]);
            INFO (problems.size() << " of 516 baseline files in " << dir.string()
                                  << " can't be used: export it again (golden_check), never edit the manifest");
            REQUIRE (problems.empty());
            return out;
        }();
        return b;
    }

    const Value& actualAudit()
    {
        static const Value v = auditLegacy (baseline().songs);
        return v;
    }

    // The integers and strings in a tree.
    std::size_t leafCount (const Value& v)
    {
        if (v.isInteger() || v.isString())
            return 1;
        std::size_t n = 0;
        for (const auto& item : v.items())
            n += leafCount (item);
        for (const auto& [k, m] : v.members())
            n += leafCount (m);
        return n;
    }

    void skipWithoutBaseline()
    {
        if (!baseline().skipReason.empty())
            SKIP (baseline().skipReason);
    }
}

TEST_CASE ("sha256Hex gives the FIPS 180-4 test vectors", "[metrics][legacy]")
{
    const std::string abc = "abc";
    CHECK (sha256Hex ({ reinterpret_cast<const std::uint8_t*> (abc.data()), abc.size() })
           == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK (sha256Hex ({}) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    const std::string two = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    CHECK (sha256Hex ({ reinterpret_cast<const std::uint8_t*> (two.data()), two.size() })
           == "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
}

TEST_CASE ("the C++ metrics on the legacy baseline give legacy_expected.json", "[metrics][legacy]")
{
    skipWithoutBaseline();
    const Value& actual = actualAudit();
    const Value& expected = expectedAudit();
    REQUIRE (baseline().songs.size() == 516);

    // Everything the audit measures. The rest of the file is a header only Node can write
    // (its version, the manifest's SHA-256, core.js's SHA-256) or prose.
    std::size_t compared = 0, withinTolerance = 0;
    for (const char* key : { "files", "notes", "sets", "section_kinds", "gating", "reported", "invariants" })
    {
        INFO (key);
        REQUIRE (expected.has (key));
        const auto c = compare (actual[key], expected[key], portTolerances(), 40);
        for (const auto& d : c.differences)
            UNSCOPED_INFO (key << "." << d);
        INFO (c.differenceCount << " differences; fix the metric code, never legacy_expected.json");
        CHECK (c.ok());
        compared += c.compared;
        withinTolerance += c.withinTolerance;
    }
    // Every number and string in those parts of the file was compared, none skipped.
    std::size_t leaves = 0;
    for (const char* key : { "files", "notes", "sets", "section_kinds", "gating", "reported", "invariants" })
        leaves += leafCount (expected[key]);
    CHECK (compared == leaves);
    CHECK (leaves > 5000); // 5,842 when this was written: the whole file but its 7-line header
    WARN (compared << " values compared with legacy_expected.json; " << withinTolerance
                   << " differ within the port's tolerance (DFA alpha, +-1)");
}

TEST_CASE ("legacy fails every gating band on the plan's file sets", "[metrics][legacy]")
{
    // The plan's claim (§4.3, §5.1): the legacy baseline fails the gates. Each gate is judged on
    // its primary set (the plan's "Legacy (audit)" column); legacy_expected.json's legacy_fails
    // says the audit found the same.
    skipWithoutBaseline();
    const Value& gating = actualAudit()["gating"];
    for (const GateDef& g : gateDefs())
    {
        INFO (g.id << " on " << g.legacySets[0] << ": " << gateSummary (g.id, gating[g.id][g.legacySets[0]]));
        CHECK (gating[g.id]["primary"].string() == g.legacySets[0]);
        CHECK (gating[g.id][g.legacySets[0]].num ("pass") == 0);
        CHECK (gating[g.id].num ("legacy_fails") == 1);
        CHECK (expectedAudit()["gating"][g.id].num ("legacy_fails") == 1);
    }
    // G4 is one-sided: the legacy FullSong's clockwork changes pass it (README, "G4").
    CHECK (gating["G4"]["FullSong"].num ("pass") == 1);
    // and the ExtendedMix's 19 custom forms pass G10
    CHECK (gating["G10"]["ExtendedMix"].num ("pass") == 1);
}

TEST_CASE ("the legacy result doesn't depend on the order the files come in", "[metrics][legacy]")
{
    skipWithoutBaseline();
    std::vector<Song> reversed (baseline().songs.rbegin(), baseline().songs.rend());
    CHECK (auditLegacy (reversed) == actualAudit());
    CHECK (toJson (auditLegacy (reversed)) == toJson (actualAudit()));
}
