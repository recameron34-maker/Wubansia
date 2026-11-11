// groovecore_render_fixture (a test tool, never installed): writes the synthetic kit, SYN01 (engine/testing), in the
// layout groovecore render reads, so ctest can render a whole kit without the real samples:
//   <dir>/plugin/syn01/SYN01_kit.json, SYN01_mix.json
//   <dir>/midi/syn01/SYN01_Loop_Vox_4bars.mid, SYN01_Loop_NoVox_4bars.mid,
//                    extended/vox/SYN01_ExtendedMix_Vox_16bars.mid, extended/novox/SYN01_ExtendedMix_NoVox_16bars.mid
//   <dir>/samples/syn01/v<N>/... every file kit.json lists
// Usage: groovecore_render_fixture <dir>

#include "testing/synthetic_kit.h"
#include "util/file_io.h"

#include <filesystem>
#include <iostream>
#include <span>
#include <string>
#include <system_error>

namespace
{
    namespace eng = wubansia::engine;

    bool write (const std::filesystem::path& path, std::span<const std::uint8_t> bytes)
    {
        std::error_code ec;
        std::filesystem::create_directories (path.parent_path(), ec);
        const auto wrote = wubansia::util::writeBinaryFile (path, bytes);
        if (! wrote.ok())
            std::cerr << "groovecore_render_fixture: " << wrote.error << '\n';
        return wrote.ok();
    }

    bool writeText (const std::filesystem::path& path, const std::string& text)
    {
        return write (path, std::span<const std::uint8_t> (reinterpret_cast<const std::uint8_t*> (text.data()), text.size()));
    }
}

int main (int argc, char* argv[])
{
    if (argc != 2)
    {
        std::cerr << "Usage: groovecore_render_fixture <dir>\n";
        return 2;
    }
    const std::filesystem::path dir (argv[1]);
    const auto& content = eng::testing::syntheticContent();
    const std::string id (eng::testing::kSyntheticKitId);
    const std::string folder = "syn01";

    bool ok = writeText (dir / "plugin" / folder / (id + "_kit.json"), content.kitJson)
              && writeText (dir / "plugin" / folder / (id + "_mix.json"), content.mixJson);
    const auto midi = dir / "midi" / folder;
    const std::string bars = std::to_string (eng::testing::kSyntheticSongTicks / 3840) + "bars.mid";
    ok = ok && write (midi / (id + "_Loop_Vox_4bars.mid"), content.smfs[0]) && write (midi / (id + "_Loop_NoVox_4bars.mid"), content.smfs[1])
         && write (midi / "extended" / "vox" / (id + "_ExtendedMix_Vox_" + bars), content.smfs[2])
         && write (midi / "extended" / "novox" / (id + "_ExtendedMix_NoVox_" + bars), content.smfs[3]);
    if (! ok)
        return 1;
    const auto samples = eng::testing::writeSyntheticKitFolder (dir / "samples" / folder / ("v" + std::to_string (content.kit.kit.contentVersion)));
    if (! samples.ok())
    {
        std::cerr << "groovecore_render_fixture: " << samples.error << '\n';
        return 1;
    }
    std::cout << "wrote " << id << ": manifests, 4 MIDI files and " << samples.value << " samples under " << dir.generic_string() << '\n';
    return 0;
}
