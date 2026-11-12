#include "sounds/SoundCatalog.h"

namespace wubansia::sounds
{
    // Tests first: ADR 0024's catalog arrives with its implementation.
    std::vector<SoundInfo> soundsOf (const engine::KitManifest&)
    {
        return {};
    }

    SoundCategory categoryOf (engine::Lane) noexcept
    {
        return SoundCategory::accents;
    }

    juce::String categoryName (SoundCategory)
    {
        return {};
    }

    int tonicOf (std::string_view) noexcept
    {
        return -1;
    }
}
