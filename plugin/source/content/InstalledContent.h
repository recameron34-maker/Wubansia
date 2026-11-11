#pragma once

// The plugin's own ContentSourceFactory (P9): the kits embedded in the binary (EmbeddedContent), their
// samples found by a ContentLocator and read by P4's FileSampleSource, and the content folder kept in
// the person's UserSettings. Constructing it reads nothing: the settings file is read on the service
// thread's first locate ().

#include "content/ContentLocator.h"
#include "content/ContentSource.h"
#include "content/UserSettings.h"

#include <mutex>

namespace wubansia
{
    class InstalledContent final : public ContentSourceFactory
    {
    public:
        // The system's content folders and the person's settings.
        InstalledContent();

        // Given folders and settings, for tests.
        InstalledContent (ContentLocator::Places places, UserSettings& settings);

        std::span<const KitSource> kits() const override;
        ContentStatus locate (const engine::KitManifest& kit, const std::atomic<bool>* cancel = nullptr) override;
        std::unique_ptr<engine::SampleSource> open (const engine::KitManifest& kit, const ContentStatus& found,
            const std::atomic<bool>* cancel = nullptr) override;
        juce::File userFolder() const override;
        void setUserFolder (const juce::File& folder) override;
        std::uint64_t foldersGeneration() const noexcept override;

    private:
        ContentLocator locator;
        UserSettings& settings;
        std::once_flag systemPlaces; // the system's folders are looked up at the first locate (), not before
        bool useSystemPlaces = false;
    };
}
