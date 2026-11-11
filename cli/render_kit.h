#pragma once

// groovecore render's kit (P12): a kit assembled the way the plugin's kit service assembles it (P9's KitCatalog and
// KitService), from files instead of the plugin's embedded copies:
// 1. its two manifests, <manifests>/<kit>/<KIT>_kit.json and <KIT>_mix.json (P4's parsers);
// 2. its four timelines (P7's buildTimeline): <midi>/<kit>/<KIT>_Loop_Vox_4bars.mid and _Loop_NoVox_4bars.mid
//    (loops of 4 bars), extended/vox/<KIT>_ExtendedMix_Vox_<N>bars.mid and extended/novox/..._NoVox_<N>bars.mid;
// 3. its samples, from the first ready content folder (below), through P4's FileSampleSource;
// 4. buildKit (P4).
// Kit folders are the kit's id in lowercase (fut01).
//
// The content folders (ADR 0022's order without the editor's choice): --content <DIR> alone when given; otherwise
// $WUBANSIA_CONTENT_DIR, then the platform's folder (macOS ~/Library/Application Support/Wubansia Audio/Wubansia/
// Content, Linux ~/.local/share/Wubansia Audio/Wubansia/Content, Windows %APPDATA%\Wubansia Audio\Wubansia\Content,
// as tools/content/kitfiles.default_content_dir). In each, <kit>/v<N>/ (N: kit.json's content_version), then <kit>/.
// A folder is ready when P4's checkKitFiles passes (every listed file there, no LFS pointer, 48 kHz, the listed frames
// and channels). When none is, the error names every folder looked at and the first problem met.

#include "kit/kit.h"
#include "kit/manifest.h"
#include "mix/mix_config.h"
#include "util/result.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace wubansia::cli
{
    struct RenderKit
    {
        std::shared_ptr<const engine::Kit> kit;
        engine::KitManifest manifest;
        std::filesystem::path samples; // the kit folder the samples came from
        std::vector<std::filesystem::path> midi; // the four SMFs, by engine::timelineIndex
    };

    // The content folders to search, in order (see the top). explicitFolder: --content's, or empty.
    std::vector<std::filesystem::path> contentRoots (const std::filesystem::path& explicitFolder);

    // The kit, or an error. A problem with the samples starts "samples: " and names the folders searched; the others
    // name the file at fault.
    util::Result<RenderKit> loadRenderKit (const std::string& kitId, const std::filesystem::path& manifests,
        const std::filesystem::path& midi, const std::vector<std::filesystem::path>& roots);
}
