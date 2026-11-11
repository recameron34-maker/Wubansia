#include "content/InstalledContent.h"

#include "content/EmbeddedContent.h"

#include "kit/file_sample_source.h"

namespace wubansia
{
    InstalledContent::InstalledContent() : locator (ContentLocator::Places {}), settings (UserSettings::instance()), useSystemPlaces (true) {}

    InstalledContent::InstalledContent (ContentLocator::Places places, UserSettings& s) : locator (std::move (places)), settings (s) {}

    std::span<const KitSource> InstalledContent::kits() const
    {
        return EmbeddedContent::sources();
    }

    ContentStatus InstalledContent::locate (const engine::KitManifest& kit)
    {
        std::call_once (systemPlaces, [this] {
            if (useSystemPlaces)
                locator.setPlaces (ContentLocator::systemPlaces());
        });
        locator.setUserFolder (settings.contentFolder());
        return locator.locate (kit);
    }

    std::unique_ptr<engine::SampleSource> InstalledContent::open (const engine::KitManifest&, const ContentStatus& found)
    {
        return std::make_unique<engine::FileSampleSource> (toPath (found.folder));
    }

    juce::File InstalledContent::userFolder() const
    {
        return settings.contentFolder();
    }

    void InstalledContent::setUserFolder (const juce::File& folder)
    {
        settings.setContentFolder (folder);
    }

    std::uint64_t InstalledContent::foldersGeneration() const noexcept
    {
        return settings.generation();
    }
}
