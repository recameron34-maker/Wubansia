// P0's stub, so code that calls the parsers links before they exist: both refuse every manifest.
// P4 replaces this file with the parsers docs/plugin/manifest.md describes.

#include "kit/manifest.h"

namespace wubansia::engine
{
    util::Result<KitManifest> parseKitManifest (std::string_view)
    {
        return util::Result<KitManifest>::failure ("kit.json: the manifest parser isn't built yet (P4)");
    }

    util::Result<MixConfig> parseMixManifest (std::string_view, const KitManifest&)
    {
        return util::Result<MixConfig>::failure ("mix.json: the manifest parser isn't built yet (P4)");
    }
}
