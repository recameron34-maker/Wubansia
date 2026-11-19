// groovecore phrases: a kit's phrase library (gen/phrase.h, ADR 0031). It chooses the library from the spec again
// and writes it (--library) or compares it with the committed file (--check), writes phrases as the plugin's drags
// name them (--out), and prints every check (--report) over the library or a scan of seeds.

#include "commands.h"

#include "gen/content.h"
#include "gen/phrase.h"
#include "gen/theory.h"
#include "util/file_io.h"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace wubansia::cli
{
    namespace
    {
        using namespace wubansia::gen;
        namespace fs = std::filesystem;

        void usage (std::ostream& os)
        {
            os << "Usage: groovecore phrases --kit <ID> [options]\n"
                  "\n"
                  "Writes or checks a kit's phrase library (content/phrases/<kit>/), or writes phrases as MIDI files.\n"
                  "\n"
                  "Options:\n"
                  "  --library         choose the library from the spec again and write <KIT>_phrase_library.json under --content\n"
                  "  --check           compare <KIT>_phrase_library.json with a fresh choice; exit 1 if it is stale, writing nothing\n"
                  "  --out <DIR>       write the library's phrases (or --kind's, --seed's, --more's) into <DIR>/<KIT>/, named as their\n"
                  "                    drags are (\"FUT01 Alloy Signal 134 F#m 11A - Phrase Night Bus Hook 8 bars.mid\")\n"
                  "  --kind <KIND>     hook, answer, period, sparse, arp, stabs, counter or bounce\n"
                  "  --seed <N>        one phrase of --kind at seed N (with --out)\n"
                  "  --more <N>        N phrases of --kind (or of each kind) as the plugin's More makes them, from seed 1000000\n"
                  "  --scan <N>        with --report: seeds 0 to N-1 of --kind (or of each kind) instead of the library\n"
                  "  --report          print every phrase's checks\n"
                  "  --content <DIR>   the content folder (default: content)\n"
                  "  --help            this text\n";
        }

        std::optional<std::uint64_t> number (std::string_view v)
        {
            std::uint64_t n = 0;
            const auto [end, ec] = std::from_chars (v.data(), v.data() + v.size(), n);
            if (v.empty() || ec != std::errc() || end != v.data() + v.size())
                return std::nullopt;
            return n;
        }

        std::string text (const std::vector<std::uint8_t>& bytes) { return std::string (bytes.begin(), bytes.end()); }

        // "FUT01 Alloy Signal 134 F#m 11A - Phrase Night Bus Hook 8 bars.mid"
        std::string fileName (const Kit& kit, const std::string& name, PhraseKind kind)
        {
            const std::string key = keyName (kit.tonic, true);
            const std::string tonic = key.substr (0, key.find (' '));
            return kit.id + " " + kit.name + " " + std::to_string (kit.bpm) + " " + tonic + "m " + kit.camelot + " - Phrase " + name + " "
                   + std::string (phraseKindFileWord (kind)) + " " + std::to_string (phraseBars) + " bars.mid";
        }

        void reportLine (std::ostream& out, const PhraseSpec& spec, const Phrase& p, const std::string& name)
        {
            const PhraseChecks c = checkPhrase (spec, p);
            out << phraseId (p.kind, p.seed) << " \"" << name << "\" form " << p.form << " notes " << c.notes << " keys " << c.low << "-" << c.high
                << " strong " << c.strongTones << "/" << c.strong << " units " << c.units << (c.pass() ? " pass" : " FAIL");
            const std::pair<bool, const char*> flags[] = { { c.inKey, "inKey" }, { c.inRange, "inRange" }, { c.everyBar, "everyBar" }, { c.motif, "motif" },
                { c.variation, "variation" }, { c.ending, "ending" }, { c.rhythm, "rhythm" }, { c.clean, "clean" }, { c.synth, "synth" }, { c.melody, "melody" },
                { c.strongChords, "strongChords" } };
            for (const auto& [ok, what] : flags)
                if (!ok)
                    out << " !" << what;
            out << '\n';
        }
    }

    int runPhrases (std::span<const std::string_view> args, std::ostream& out, std::ostream& err)
    {
        std::string kitId, outDir, contentDir = "content";
        std::optional<PhraseKind> kind;
        std::optional<std::uint64_t> seed, more, scan;
        bool library = false, check = false, report = false;
        for (std::size_t i = 0; i < args.size(); ++i)
        {
            const std::string_view a = args[i];
            const auto value = [&] () -> std::optional<std::string_view> {
                if (i + 1 >= args.size())
                    return std::nullopt;
                return args[++i];
            };
            const auto count = [&] (std::optional<std::uint64_t>& to) {
                const auto v = value();
                to = v ? number (*v) : std::nullopt;
                if (!to)
                    err << "groovecore phrases: " << a << " takes a whole number\n";
                return to.has_value();
            };
            if (a == "--help" || a == "-h")
            {
                usage (out);
                return exitOk;
            }
            if (a == "--kit" || a == "--out" || a == "--content" || a == "--kind")
            {
                const auto v = value();
                if (!v)
                {
                    err << "groovecore phrases: " << a << " takes a value\n";
                    return exitUsage;
                }
                if (a == "--kit")
                    kitId = std::string (*v);
                else if (a == "--out")
                    outDir = std::string (*v);
                else if (a == "--content")
                    contentDir = std::string (*v);
                else if (!(kind = phraseKindFromId (*v)))
                {
                    err << "groovecore phrases: unknown kind \"" << *v << "\" (hook, answer, period, sparse, arp, stabs, counter or bounce)\n";
                    return exitUsage;
                }
            }
            else if (a == "--seed" || a == "--more" || a == "--scan")
            {
                if (!count (a == "--seed" ? seed : a == "--more" ? more : scan))
                    return exitUsage;
            }
            else if (a == "--library")
                library = true;
            else if (a == "--check")
                check = true;
            else if (a == "--report")
                report = true;
            else
            {
                err << "groovecore phrases: unknown option \"" << a << "\"\n";
                usage (err);
                return exitUsage;
            }
        }
        if (kitId.empty())
        {
            err << "groovecore phrases: --kit is required\n";
            usage (err);
            return exitUsage;
        }
        if (seed && !kind)
        {
            err << "groovecore phrases: --seed needs --kind\n";
            return exitUsage;
        }
        if ((seed || more) && outDir.empty() && !report)
        {
            err << "groovecore phrases: --seed and --more write files: give --out (or print them with --report)\n";
            return exitUsage;
        }
        if (!library && !check && outDir.empty() && !report)
        {
            err << "groovecore phrases: nothing to do: give --library, --check, --out or --report\n";
            return exitUsage;
        }

        const auto bundle = loadBundle (contentDir, kitId);
        if (!bundle.ok())
        {
            err << "groovecore phrases: " << bundle.error << '\n';
            return exitFailed;
        }
        const Kit& kit = bundle.value.kit;
        const auto specResult = loadPhraseSpec (contentDir, kit.id);
        if (!specResult.ok())
        {
            err << "groovecore phrases: " << specResult.error << '\n';
            return exitFailed;
        }
        const PhraseSpec& spec = specResult.value;
        const fs::path specPath = phraseSpecPath (contentDir, kit.id);
        const fs::path libPath = phraseLibraryPath (contentDir, kit.id);
        const auto specBytes = util::readBinaryFile (specPath);
        if (!specBytes.ok())
        {
            err << "groovecore phrases: " << specBytes.error << '\n';
            return exitFailed;
        }

        const bool fromLibrary = !seed && !more && !scan;
        std::vector<LibraryRow> fresh;
        if (library || check || fromLibrary)
        {
            const auto t0 = std::chrono::steady_clock::now();
            fresh = selectLibrary (spec);
            if (report)
                out << kit.id << ": the library chosen in "
                    << std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - t0).count() << " ms\n";
        }
        if (library || check)
        {
            const std::string json = libraryJson (spec, text (specBytes.value), fresh);
            const auto committed = util::readBinaryFile (libPath);
            const bool same = committed.ok() && text (committed.value) == json;
            if (check)
            {
                if (!same)
                {
                    err << "groovecore phrases: " << libPath.generic_string() << " is not a fresh choice from " << specPath.filename().generic_string()
                        << ": run groovecore phrases --kit " << kit.id << " --library\n";
                    return exitFailed;
                }
                out << libPath.filename().generic_string() << " is up to date (" << fresh.size() << " phrases)\n";
            }
            else
            {
                const auto written = util::writeBinaryFile (libPath, std::span (reinterpret_cast<const std::uint8_t*> (json.data()), json.size()));
                if (!written.ok())
                {
                    err << "groovecore phrases: " << written.error << '\n';
                    return exitFailed;
                }
                out << "wrote " << libPath.generic_string() << " (" << fresh.size() << " phrases" << (same ? ", unchanged" : "") << ")\n";
            }
        }

        // The phrases to write or report.
        std::vector<Phrase> phrases;
        if (fromLibrary)
            for (const LibraryRow& r : fresh)
            {
                if (!kind || r.kind == *kind)
                    phrases.push_back (generatePhrase (spec, r.kind, r.seed));
            }
        else
            for (std::size_t k = 0; k < phraseKindCount; ++k)
            {
                const auto kk = static_cast<PhraseKind> (k);
                if (kind && kk != *kind)
                    continue;
                if (seed)
                    phrases.push_back (generatePhrase (spec, kk, *seed));
                if (scan)
                {
                    std::int32_t pass = 0, low = 127, high = 0;
                    double total = 0.0, slowest = 0.0;
                    for (std::uint64_t s = 0; s < *scan; ++s)
                    {
                        const auto t0 = std::chrono::steady_clock::now();
                        Phrase p = generatePhrase (spec, kk, s);
                        const double us = std::chrono::duration<double, std::micro> (std::chrono::steady_clock::now() - t0).count();
                        total += us;
                        slowest = std::max (slowest, us);
                        const PhraseChecks c = checkPhrase (spec, p);
                        if (c.pass())
                        {
                            ++pass;
                            low = std::min (low, c.low);
                            high = std::max (high, c.high);
                        }
                        if (report && outDir.empty())
                            reportLine (out, spec, p, phraseName (spec, kk, s));
                        else
                            phrases.push_back (std::move (p));
                    }
                    if (report)
                        out << kit.id << " " << phraseKindIds[k] << ": " << pass << " of " << *scan << " pass, keys " << low << "-" << high << ", "
                            << total / static_cast<double> (std::max<std::uint64_t> (1, *scan)) << " us a phrase on average, " << slowest << " at most\n";
                }
                if (more)
                {
                    std::vector<Phrase> against;
                    for (const LibraryRow& r : selectLibrary (spec))
                        against.push_back (generatePhrase (spec, r.kind, r.seed));
                    std::uint64_t from = firstMoreSeed;
                    std::uint64_t left = *more;
                    while (left > 0)
                    {
                        const auto press = morePhrases (spec, kk, from, against, static_cast<std::int32_t> (std::min<std::uint64_t> (left, 8)));
                        if (press.phrases.empty())
                            break;
                        for (const Phrase& p : press.phrases)
                        {
                            against.push_back (p);
                            phrases.push_back (p);
                        }
                        left -= std::min<std::uint64_t> (left, press.phrases.size());
                        from = press.nextSeed;
                    }
                }
            }

        if (report)
            for (const Phrase& p : phrases)
                reportLine (out, spec, p, phraseName (spec, p.kind, p.seed));
        if (!outDir.empty())
        {
            const fs::path dir = fs::path (outDir) / kit.id;
            std::error_code ec;
            fs::create_directories (dir, ec);
            for (const Phrase& p : phrases)
            {
                const std::string name = phraseName (spec, p.kind, p.seed);
                const auto bytes = phraseSmf (spec, p, name);
                const fs::path path = dir / fileName (kit, name, p.kind);
                const auto written = bytes.ok() ? util::writeBinaryFile (path, bytes.value) : util::Result<std::size_t>::failure (bytes.error);
                if (!written.ok())
                {
                    err << "groovecore phrases: " << written.error << '\n';
                    return exitFailed;
                }
            }
            out << "wrote " << phrases.size() << " files under " << dir.generic_string() << '\n';
        }
        return exitOk;
    }
}
