// groovecore gen: an extended mix from a kit and a seed, as MIDI (docs/plans/S1-futuristic-slice.md
// §2.3, the S1b1 brief). Writes, for each version (with feel, quantized):
//   <out>/<version>/<KIT>_seed<N>_<bpm>bpm_<camelot>_full.mid   the whole arrangement, section markers
//   <out>/<version>/regions/NN_<kind>_<bars>bars.mid               one region per section, all lanes
//   <out>/<version>/regions.json                                   the regions: start, length, kind, lanes
// One --out folder per song: region names carry no seed, so writeFiles refuses a folder that holds
// another song's full file, a regions.json of another kit or seed, or region files this song doesn't
// write.

#include "commands.h"

#include "gen/content.h"
#include "gen/export.h"
#include "gen/song.h"

#include <charconv>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>

namespace wubansia::cli
{
    namespace
    {
        void printHelp (std::ostream& os)
        {
            os << "Usage: groovecore gen --kit <ID> --seed <N> --out <DIR> [options]\n"
                  "\n"
                  "Writes an extended mix of the kit's style as MIDI regions, its form drawn from the\n"
                  "seed: the full arrangement (SMF type 1, 480 PPQ, one track per lane, a marker at every\n"
                  "section), one file per section and regions.json, with feel and quantized.\n"
                  "\n"
                  "Options:\n"
                  "  --kit <ID>        the kit, e.g. FUT01 (content/kits/<id>.json)\n"
                  "  --seed <N>        the seed, 0 to 18446744073709551615: the same seed gives the same bytes\n"
                  "  --out <DIR>       where to write (created if missing): <DIR>/feel/ and <DIR>/quantized/.\n"
                  "                    One folder per song: gen refuses a folder that holds another\n"
                  "                    song (another seed or kit, or other regions), and never deletes a file\n"
                  "  --quantized       write only the quantized version\n"
                  "  --feel            write only the version with feel\n"
                  "  --bars <N>        the whole song's first N bars, N a multiple of 8 (the default is the\n"
                  "                    whole form): each note that starts in them in the score, ended where\n"
                  "                    they end (with feel, a note on the next downbeat played a few ticks\n"
                  "                    early is left out)\n"
                  "  --feel-amount <P> the feel's amount in permille, 0 to 2000 (the default is the feel file's)\n"
                  "  --content <DIR>   the content folder (default: content, from the current folder)\n"
                  "  --help            this text\n";
        }

        template <typename T>
        bool parseNumber (std::string_view text, T& out)
        {
            if (text.empty())
                return false;
            const auto* first = text.data();
            const auto* last = text.data() + text.size();
            const auto [ptr, ec] = std::from_chars (first, last, out);
            return ec == std::errc() && ptr == last;
        }

        std::string shown (const std::filesystem::path& p)
        {
            const auto s = p.generic_u8string();
            return std::string (s.begin(), s.end());
        }
    }

    int runGen (std::span<const std::string_view> args, std::ostream& out, std::ostream& err)
    {
        std::string kit;
        std::string outDir;
        std::string contentDir = "content";
        std::uint64_t seed = 0;
        bool haveSeed = false, onlyQuantized = false, onlyFeel = false;
        gen::Options options;

        for (std::size_t i = 0; i < args.size(); ++i)
        {
            const std::string_view a = args[i];
            const auto value = [&] (std::string_view& v) {
                if (i + 1 >= args.size())
                    return false;
                v = args[++i];
                return true;
            };
            std::string_view v;
            if (a == "--help" || a == "-h")
            {
                printHelp (out);
                return exitOk;
            }
            else if (a == "--kit" && value (v))
                kit = std::string (v);
            else if (a == "--seed" && value (v))
            {
                if (!parseNumber (v, seed))
                {
                    err << "groovecore gen: --seed takes a whole number from 0 to 18446744073709551615, not \"" << v << "\"\n";
                    return exitUsage;
                }
                haveSeed = true;
            }
            else if (a == "--out" && value (v))
                outDir = std::string (v);
            else if (a == "--content" && value (v))
                contentDir = std::string (v);
            else if (a == "--bars" && value (v))
            {
                if (!parseNumber (v, options.bars) || options.bars <= 0)
                {
                    err << "groovecore gen: --bars takes a positive multiple of 8, not \"" << v << "\"\n";
                    return exitUsage;
                }
            }
            else if (a == "--feel-amount" && value (v))
            {
                if (!parseNumber (v, options.feelAmount) || options.feelAmount < 0 || options.feelAmount > 2000)
                {
                    err << "groovecore gen: --feel-amount takes 0 to 2000 (permille), not \"" << v << "\"\n";
                    return exitUsage;
                }
            }
            else if (a == "--quantized")
                onlyQuantized = true;
            else if (a == "--feel")
                onlyFeel = true;
            else
            {
                const bool missing = (a == "--kit" || a == "--seed" || a == "--out" || a == "--content" || a == "--bars" || a == "--feel-amount");
                err << "groovecore gen: " << (missing ? "missing value for " : "unknown option ") << "\"" << a << "\"\n\n";
                printHelp (err);
                return exitUsage;
            }
        }
        if (kit.empty() || !haveSeed || outDir.empty())
        {
            err << "groovecore gen: --kit, --seed and --out are required\n\n";
            printHelp (err);
            return exitUsage;
        }
        if (onlyFeel && onlyQuantized)
        {
            err << "groovecore gen: --feel and --quantized each ask for only one version; leave both out for both\n";
            return exitUsage;
        }
        options.seed = seed;

        const auto bundle = gen::loadBundle (contentDir, kit);
        if (!bundle.ok())
        {
            err << "groovecore gen: " << bundle.error << '\n';
            std::error_code ec;
            if (!std::filesystem::is_directory (contentDir, ec))
                err << "(no content folder at \"" << contentDir << "\": run from the repository root, or pass --content <DIR>)\n";
            return exitFailed;
        }
        const auto song = gen::generate (bundle.value, options);
        if (!song.ok())
        {
            // generate () only refuses options it can't honour: a usage error
            err << "groovecore gen: " << song.error << '\n';
            return exitUsage;
        }
        const auto written = gen::writeFiles (song.value, outDir, !onlyQuantized, !onlyFeel);
        if (!written.ok())
        {
            err << "groovecore gen: " << written.error << '\n';
            return exitFailed;
        }

        const gen::Song& s = song.value;
        const std::int32_t seconds = s.totalBars * 240 / s.bpm;
        out << s.kitId << " \"" << s.kitName << "\", seed " << s.seed << ": " << s.totalBars << " bars at " << s.bpm << " BPM ("
            << seconds / 60 << ":" << (seconds % 60 < 10 ? "0" : "") << seconds % 60 << "), " << s.keyName << " (" << s.camelot << "), "
            << gen::modeName (s.mode) << '\n';
        for (const auto& sec : s.sections)
            out << "  bar " << sec.startBar + 1 << (sec.startBar + 1 < 10 ? "  " : sec.startBar + 1 < 100 ? " "
                                                                                                          : "")
                << "  " << gen::regionFileName (sec) << '\n';
        out << "wrote " << written.value.size() << " files under " << shown (outDir) << '\n';
        return exitOk;
    }
}
