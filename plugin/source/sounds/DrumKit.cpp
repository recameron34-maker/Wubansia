#include "sounds/DrumKit.h"

namespace wubansia::sounds
{
    std::string drumNoteName (int) { return {}; }

    std::string drumKitFolderName (const engine::KitMeta&) { return {}; }

    util::Result<DrumKitPlan> planDrumKit (const engine::KitManifest&, const engine::MixConfig&)
    {
        return util::Result<DrumKitPlan>::failure ("not yet");
    }
}
