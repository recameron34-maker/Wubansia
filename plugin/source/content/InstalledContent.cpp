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

    ContentStatus InstalledContent::locate (const engine::KitManifest& kit, const std::atomic<bool>* cancel)
    {
        std::call_once (systemPlaces, [this] {
            if (useSystemPlaces)
                locator.setPlaces (ContentLocator::systemPlaces());
        });
        locator.setUserFolder (settings.contentFolder());
        return locator.locate (kit, cancel);
    }

    std::unique_ptr<engine::SampleSource> InstalledContent::open (const engine::KitManifest&, const ContentStatus& found, const std::atomic<bool>* cancel)
    {
        return std::make_unique<engine::FileSampleSource> (toPath (found.folder), cancel);
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
