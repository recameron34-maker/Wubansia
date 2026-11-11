// render_kit.h: groovecore render's kit, from the manifests, the MIDI and a content folder.

#include "render_kit.h"

#include "engine.h"
#include "kit/file_sample_source.h"
#include "kit/kit_builder.h"
#include "sched/timeline_builder.h"
#include "util/file_io.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdlib>
#include <string_view>
#include <system_error>

namespace wubansia::cli
{
    namespace
    {
        std::string shown (const std::filesystem::path& p)
        {
            const auto s = p.generic_u8string();
            return std::string (s.begin(), s.end());
        }

        std::string lower (std::string s)
        {
            for (auto& c : s)
                if (c >= 'A' && c <= 'Z')
                    c = static_cast<char> (c - 'A' + 'a');
            return s;
        }

        std::filesystem::path environmentPath (const char* name)
        {
            const char* v = std::getenv (name);
            return v != nullptr && *v != '\0' ? std::filesystem::path (v) : std::filesystem::path();
        }

        std::filesystem::path platformContentFolder()
        {
            const std::filesystem::path tail = std::filesystem::path ("Wubansia Audio") / "Wubansia" / "Content";
#if defined(_WIN32)
            auto appData = environmentPath ("APPDATA");
            if (appData.empty())
            {
                const auto home = environmentPath ("USERPROFILE");
                if (home.empty())
                    return {};
                appData = home / "AppData" / "Roaming";
            }
            return appData / tail;
#elif defined(__APPLE__)
            const auto home = environmentPath ("HOME");
            return home.empty() ? std::filesystem::path() : home / "Library" / "Application Support" / tail;
#else
            const auto home = environmentPath ("HOME");
            return home.empty() ? std::filesystem::path() : home / ".local" / "share" / tail;
#endif
        }

        util::Result<std::string> readText (const std::filesystem::path& path)
        {
            auto bytes = util::readBinaryFile (path);
            if (! bytes.ok())
                return util::Result<std::string>::failure (bytes.error);
            return util::Result<std::string>::success (std::string (bytes.value.begin(), bytes.value.end()));
        }

        // <midi>/<kit>/extended/<vox|novox>/<KIT>_ExtendedMix_<Vox|NoVox>_<N>bars.mid: the one file there.
        util::Result<std::filesystem::path> songFile (const std::filesystem::path& kitMidi, const std::string& id, bool vox)
        {
            using R = util::Result<std::filesystem::path>;
            const auto folder = kitMidi / "extended" / (vox ? "vox" : "novox");
            const std::string prefix = id + (vox ? "_ExtendedMix_Vox_" : "_ExtendedMix_NoVox_");
            std::vector<std::filesystem::path> found;
            std::error_code ec;
            for (std::filesystem::directory_iterator it (folder, ec), end; ! ec && it != end; it.increment (ec))
            {
                const auto name = it->path().filename().generic_u8string();
                const std::string_view n (reinterpret_cast<const char*> (name.data()), name.size());
                if (n.size() > prefix.size() + 8 && n.substr (0, prefix.size()) == prefix && n.substr (n.size() - 8) == "bars.mid")
                    found.push_back (it->path());
            }
            if (ec)
                return R::failure ("no folder " + shown (folder) + ": " + ec.message());
            if (found.size() != 1)
                return R::failure (std::to_string (found.size()) + " files " + prefix + "<N>bars.mid in " + shown (folder) + ": one expected");
            return R::success (found.front());
        }
    }

    std::vector<std::filesystem::path> contentRoots (const std::filesystem::path& explicitFolder)
    {
        if (! explicitFolder.empty())
            return { explicitFolder };
        std::vector<std::filesystem::path> roots;
        for (const auto& p : { environmentPath ("WUBANSIA_CONTENT_DIR"), platformContentFolder() })
            if (! p.empty() && std::find (roots.begin(), roots.end(), p) == roots.end())
                roots.push_back (p);
        return roots;
    }

    util::Result<RenderKit> loadRenderKit (const std::string& kitId, const std::filesystem::path& manifests, const std::filesystem::path& midi,
        const std::vector<std::filesystem::path>& roots)
    {
        using R = util::Result<RenderKit>;
        const std::string folder = lower (kitId);
        RenderKit out;

        // 1. The manifests.
        const auto kitPath = manifests / folder / (kitId + "_kit.json");
        const auto mixPath = manifests / folder / (kitId + "_mix.json");
        const auto kitText = readText (kitPath);
        if (! kitText.ok())
            return R::failure ("no kit \"" + kitId + "\": " + kitText.error);
        auto manifest = engine::parseKitManifest (kitText.value);
        if (! manifest.ok())
            return R::failure (shown (kitPath) + ": " + manifest.error);
        const auto mixText = readText (mixPath);
        if (! mixText.ok())
            return R::failure (mixText.error);
        auto mix = engine::parseMixManifest (mixText.value, manifest.value);
        if (! mix.ok())
            return R::failure (shown (mixPath) + ": " + mix.error);
        if (manifest.value.kit.id != kitId)
            return R::failure (shown (kitPath) + ": the kit is " + manifest.value.kit.id + ", not " + kitId);

        // 2. The timelines: Loop vox, Loop no-vox, Song vox, Song no-vox.
        const auto kitMidi = midi / folder;
        std::array<std::filesystem::path, 4> files { kitMidi / (kitId + "_Loop_Vox_4bars.mid"), kitMidi / (kitId + "_Loop_NoVox_4bars.mid") };
        for (int v = 0; v < 2; ++v)
        {
            auto song = songFile (kitMidi, kitId, v == 0);
            if (! song.ok())
                return R::failure (song.error);
            files[static_cast<std::size_t> (2 + v)] = song.value;
        }
        std::array<engine::Timeline, 4> timelines;
        for (std::size_t i = 0; i < files.size(); ++i)
        {
            const auto smf = util::readBinaryFile (files[i]);
            if (! smf.ok())
                return R::failure (smf.error);
            engine::TimelineOptions options;
            options.loopTicks = i < 2 ? engine::kLoopLength / engine::kMilliticksPerTick : 0;
            auto timeline = engine::buildTimeline (smf.value, manifest.value, mix.value, options);
            if (! timeline.ok())
                return R::failure (shown (files[i]) + ": " + timeline.error);
            timelines[i] = std::move (timeline.value);
            out.midi.push_back (files[i]);
        }

        // 3. The samples: the first ready kit folder.
        std::vector<std::string> searched;
        std::string firstProblem;
        std::filesystem::path chosen;
        for (const auto& root : roots)
        {
            for (const auto& dir : { root / folder / ("v" + std::to_string (manifest.value.kit.contentVersion)), root / folder })
            {
                std::error_code ec;
                if (! std::filesystem::is_directory (dir, ec))
                    continue;
                searched.push_back (shown (dir));
                engine::FileSampleSource probe (dir);
                const auto checked = engine::checkKitFiles (manifest.value, probe);
                if (checked.ok())
                {
                    chosen = dir;
                    break;
                }
                if (firstProblem.empty())
                    firstProblem = checked.error;
            }
            if (! chosen.empty())
                break;
        }
        if (chosen.empty())
        {
            std::string where;
            for (const auto& root : roots)
                where += (where.empty() ? "" : ", ") + shown (root / folder);
            std::string message = "samples: " + kitId + "'s samples aren't installed";
            if (! firstProblem.empty())
            {
                message += ": " + firstProblem;
                std::string list;
                for (const auto& s : searched)
                    list += (list.empty() ? "" : ", ") + s;
                message += " (folders checked: " + list + ")";
            }
            else
                message += ": no kit folder in " + (where.empty() ? std::string ("any content folder") : where);
            message += ". Set WUBANSIA_CONTENT_DIR or pass --content <DIR> (a folder holding " + folder + "/ or " + folder + "/v"
                       + std::to_string (manifest.value.kit.contentVersion) + "/).";
            return R::failure (message);
        }

        // 4. The Kit.
        engine::FileSampleSource source (chosen);
        const std::atomic<bool> cancel { false };
        auto built = engine::buildKit (manifest.value, std::move (mix.value), source, std::move (timelines), cancel, [] (int) {});
        if (! built.ok())
            return R::failure ("samples: " + built.error);
        out.kit = std::move (built.value);
        out.manifest = std::move (manifest.value);
        out.samples = chosen;
        return R::success (std::move (out));
    }
}
