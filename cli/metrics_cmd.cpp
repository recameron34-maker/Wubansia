// groovecore metrics: the generator metrics (docs/plans/S1-futuristic-slice.md §5.1, defined in
// tools/legacy_audit/README.md) on MIDI files, through core/metrics. The same code scores the legacy
// baseline and generator v2: it reads only the MIDI bytes.

#include "commands.h"

#include "metrics/suite.h"
#include "metrics/value.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace wubansia::cli
{
    namespace
    {
        void printHelp (std::ostream& os)
        {
            os << "Usage: groovecore metrics [options] <FILE|DIR>...\n"
                  "\n"
                  "Scores MIDI files with the generator metrics and prints one line per gating metric\n"
                  "(G1-G10, with the plan's two-sided bands). The definitions are in\n"
                  "tools/legacy_audit/README.md.\n"
                  "\n"
                  "Folders are searched for .mid files; a regions/ folder is skipped (a region is one\n"
                  "section of a song). A file is reported by its path from the folder given (a folder\n"
                  "named feel or quantized keeps its name), or as <folder>/<name> when given on its own.\n"
                  "\n"
                  "Two layouts:\n"
                  "  legacy  every path is <KIT>/<KIT>_<slot>.mid, as node tools/export_golden.js writes\n"
                  "          build/golden/: each gate on the legacy file sets, as\n"
                  "          tools/legacy_audit/audit.js measures them (its primary set decides).\n"
                  "  seeds   anything else: one set of seeds. A file in a feel/ folder and the file at the\n"
                  "          same path in quantized/ are one seed, and each metric reads the version it is\n"
                  "          defined on; any other file is a seed of its own. The gates judge a style's\n"
                  "          seeds together (plan section 5.1: 64 seeds), so score one kit at a time.\n"
                  "\n"
                  "Options:\n"
                  "  --json          print the whole result as JSON (per-file values included)\n"
                  "  --gate          exit 1 if any gating band fails\n"
                  "  --expect-fail   exit 1 unless every gating band fails (for the legacy layout, on its\n"
                  "                  primary set): golden_check's assertion that the legacy baseline fails\n"
                  "  --legacy        insist on the legacy layout\n"
                  "  --seeds         use the seeds layout, also for legacy paths\n"
                  "  --check <FILE>  compare every value with an expected result, e.g.\n"
                  "                  core/tests/legacy_expected.json for build/golden/; exit 1 on any\n"
                  "                  difference beyond the port's tolerance (DFA alpha +-1), and on a\n"
                  "                  section only one of them has (the expected file's header aside)\n"
                  "  --help          this text\n"
                  "\n"
                  "Exit status: 0 done, 1 a file it couldn't read, a failed gate (--gate), a passing\n"
                  "gate (--expect-fail) or a difference (--check), 2 bad arguments.\n";
        }

        std::string shown (const std::filesystem::path& p)
        {
            const auto s = p.generic_u8string();
            return std::string (s.begin(), s.end());
        }

        bool isMidiFile (const std::filesystem::path& p)
        {
            std::string ext = shown (p.extension());
            std::transform (ext.begin(), ext.end(), ext.begin(), [] (unsigned char c) { return static_cast<char> (std::tolower (c)); });
            return ext == ".mid" || ext == ".midi";
        }

        // The last component of a folder's path, also when it ends with a slash ("out/feel/": "feel").
        std::string folderName (const std::filesystem::path& p)
        {
            auto n = p.lexically_normal();
            if (n.filename().empty())
                n = n.parent_path();
            return shown (n.filename());
        }

        struct Input
        {
            std::filesystem::path file;
            std::string path; // how it is reported
        };

        // The .mid files under the arguments, sorted by reported path in byte order. Writes the
        // problem to err and returns false if an argument is missing or two files clash.
        bool collect (const std::vector<std::string>& args, std::vector<Input>& out, std::ostream& err)
        {
            for (const auto& a : args)
            {
                const std::filesystem::path p (a);
                std::error_code ec;
                if (std::filesystem::is_directory (p, ec))
                {
                    // A version folder keeps its name, so out/feel and out/quantized pair up as seeds.
                    const std::string name = folderName (p);
                    const std::string prefix = name == "feel" || name == "quantized" ? name + "/" : std::string();
                    std::vector<Input> found;
                    for (std::filesystem::recursive_directory_iterator it (p, ec), end; !ec && it != end; it.increment (ec))
                    {
                        if (!it->is_regular_file (ec) || !isMidiFile (it->path()))
                            continue;
                        const std::string rel = shown (it->path().lexically_relative (p));
                        if (metrics::isRegionPath (rel))
                            continue;
                        found.push_back ({ it->path(), prefix + rel });
                    }
                    if (ec)
                    {
                        err << "groovecore metrics: can't read the folder " << shown (p) << ": " << ec.message() << '\n';
                        return false;
                    }
                    out.insert (out.end(), found.begin(), found.end());
                }
                else if (std::filesystem::is_regular_file (p, ec))
                {
                    const auto parent = p.parent_path().filename();
                    out.push_back ({ p, parent.empty() ? shown (p.filename()) : shown (parent / p.filename()) });
                }
                else
                {
                    err << "groovecore metrics: no file or folder " << shown (p) << '\n';
                    return false;
                }
            }
            std::stable_sort (out.begin(), out.end(), [] (const Input& x, const Input& y) { return x.path < y.path; });
            for (std::size_t i = 1; i < out.size(); ++i)
                if (out[i].path == out[i - 1].path)
                {
                    err << "groovecore metrics: " << shown (out[i - 1].file) << " and " << shown (out[i].file) << " would both be reported as " << out[i].path
                        << "; score them separately\n";
                    return false;
                }
            return true;
        }

        // The result against the expected file, every section both ways: a section the expected file
        // has and the result doesn't (the metrics stopped computing it), or the other way round,
        // differs. Left out: the result's "about" and "format", and the expected file's header, the
        // fields legacy_expected.json records it was made with (the Node, the manifest, core.js).
        metrics::Comparison check (const metrics::Value& actual, const metrics::Value& expected)
        {
            static constexpr std::array<std::string_view, 7> header = { "about", "format", "node", "manifest", "manifest_sha256", "manifest_node", "core_js_sha256" };
            metrics::Value a = metrics::Value::object();
            metrics::Value e = metrics::Value::object();
            for (const auto& [k, v] : actual.members())
                if (k != "about" && k != "format")
                    a.set (k, v);
            for (const auto& [k, v] : expected.members())
                if (std::find (header.begin(), header.end(), k) == header.end())
                    e.set (k, v);
            return metrics::compare (a, e, metrics::portTolerances(), 20);
        }
    }

    int runMetrics (std::span<const std::string_view> args, std::ostream& out, std::ostream& err)
    {
        bool json = false, gate = false, expectFail = false, forceLegacy = false, forceSeeds = false;
        std::string checkFile;
        std::vector<std::string> inputs;
        for (std::size_t i = 0; i < args.size(); ++i)
        {
            const std::string_view a = args[i];
            if (a == "--help" || a == "-h")
            {
                printHelp (out);
                return exitOk;
            }
            if (a == "--json")
                json = true;
            else if (a == "--gate")
                gate = true;
            else if (a == "--expect-fail")
                expectFail = true;
            else if (a == "--legacy")
                forceLegacy = true;
            else if (a == "--seeds")
                forceSeeds = true;
            else if (a == "--check")
            {
                if (i + 1 >= args.size())
                {
                    err << "groovecore metrics: missing value for \"--check\"\n\n";
                    printHelp (err);
                    return exitUsage;
                }
                checkFile = std::string (args[++i]);
            }
            else if (a.starts_with ("-") && a != "-")
            {
                err << "groovecore metrics: unknown option \"" << a << "\"\n\n";
                printHelp (err);
                return exitUsage;
            }
            else
                inputs.emplace_back (a);
        }
        if (inputs.empty())
        {
            err << "groovecore metrics: name the MIDI files or folders to score\n\n";
            printHelp (err);
            return exitUsage;
        }
        if (forceLegacy && forceSeeds)
        {
            err << "groovecore metrics: --legacy and --seeds are two layouts; choose one\n";
            return exitUsage;
        }
        if (gate && expectFail)
        {
            err << "groovecore metrics: --gate wants every band to pass and --expect-fail every band to fail; choose one\n";
            return exitUsage;
        }

        std::vector<Input> files;
        if (!collect (inputs, files, err))
            return exitFailed;
        if (files.empty())
        {
            err << "groovecore metrics: no .mid files found\n";
            return exitFailed;
        }

        std::vector<metrics::Song> songs;
        songs.reserve (files.size());
        for (const auto& f : files)
        {
            auto s = metrics::loadSongFile (f.file, f.path);
            if (!s.ok())
            {
                err << "groovecore metrics: " << s.error << '\n';
                return exitFailed;
            }
            songs.push_back (std::move (s.value));
        }

        const bool allLegacy = std::all_of (songs.begin(), songs.end(), [] (const metrics::Song& s) { return !s.slot.empty(); });
        if (forceLegacy && !allLegacy)
        {
            const auto bad = std::find_if (songs.begin(), songs.end(), [] (const metrics::Song& s) { return s.slot.empty(); });
            err << "groovecore metrics: --legacy needs every path to be <KIT>/<KIT>_<slot>.mid, and " << bad->path << " isn't\n";
            return exitUsage;
        }
        const bool legacy = !forceSeeds && allLegacy;

        metrics::Value result;
        std::string table;
        std::vector<std::string_view> passing; // the gates that pass (legacy: on their primary sets)
        if (legacy)
        {
            result = metrics::auditLegacy (songs);
            table = metrics::legacyTable (result);
            for (const auto& g : metrics::gateDefs())
                if (result["gating"][g.id].num ("legacy_fails") == 0)
                    passing.push_back (g.id);
        }
        else
        {
            std::vector<std::string> paths;
            for (const auto& s : songs)
                paths.push_back (s.path);
            std::vector<metrics::Seed> seeds;
            for (const auto& p : metrics::pairVersions (paths))
                seeds.push_back ({ &songs[p.quantized], &songs[p.feel] });
            result = metrics::scoreSeeds (seeds);
            table = metrics::seedsTable (result);
            for (const auto& g : metrics::gateDefs())
                if (result["gating"][g.id].num ("pass") == 1)
                    passing.push_back (g.id);
        }
        const bool failedGate = passing.size() != metrics::gateDefs().size();

        if (json)
            out << metrics::toJson (result) << '\n';
        else
            out << table;

        int status = exitOk;
        if (!checkFile.empty())
        {
            std::ostream& note = json ? err : out; // stdout stays JSON
            const auto expected = metrics::readJsonFile (checkFile);
            if (!expected.ok())
            {
                err << "groovecore metrics: " << expected.error << '\n';
                return exitFailed;
            }
            const auto c = check (result, expected.value);
            if (c.ok())
                note << "check: all " << c.compared << " values match " << checkFile
                     << (c.withinTolerance > 0 ? " (" + std::to_string (c.withinTolerance) + " within the DFA alpha tolerance)" : std::string()) << '\n';
            else
            {
                err << "groovecore metrics: " << c.differenceCount << " values differ from " << checkFile << ":\n";
                for (const auto& d : c.differences)
                    err << "  " << d << '\n';
                if (c.differenceCount > c.differences.size())
                    err << "  ... and " << c.differenceCount - c.differences.size() << " more\n";
                err << "Fix the metric code, never the expected file.\n";
                status = exitFailed;
            }
        }
        if (gate && failedGate)
        {
            err << "groovecore metrics: gating bands fail" << (legacy ? std::string() : ": " + result["failed"].string()) << '\n';
            status = exitFailed;
        }
        if (expectFail && !passing.empty())
        {
            err << "groovecore metrics: every gating band should fail" << (legacy ? " on the legacy baseline" : "") << ", but these pass:";
            for (const auto id : passing)
                err << ' ' << id;
            err << '\n';
            status = exitFailed;
        }
        return status;
    }
}
