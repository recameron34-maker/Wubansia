#include "sounds/SampleExport.h"

namespace wubansia::sounds
{
    // Tests first: ADR 0024's copies arrive with their implementation.
    juce::File defaultSamplesFolder()
    {
        return {};
    }

    std::string kitFolderName (const engine::KitInfo&)
    {
        return {};
    }

    std::string exportName (const engine::KitInfo&, const engine::FileEntry&)
    {
        return {};
    }

    std::uint64_t listedBytes (const engine::FileEntry&) noexcept
    {
        return 0;
    }

    bool matchesListing (std::span<const std::uint8_t>, const engine::FileEntry&)
    {
        return false;
    }

    std::string listingProblem (const engine::KitInfo&, const engine::FileEntry&)
    {
        return {};
    }

    ExportResult exportCopy (ContentSourceFactory&, const ContentStatus&, const engine::KitInfo&, const engine::FileEntry&, const juce::File&)
    {
        return { {}, "not implemented yet" };
    }
}
