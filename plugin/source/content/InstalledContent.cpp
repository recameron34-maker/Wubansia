#include "content/InstalledContent.h"

#include "content/EmbeddedContent.h"

#include "kit/file_sample_source.h"
#include "util/file_io.h"

#include <string>

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

    util::Result<std::vector<std::uint8_t>> InstalledContent::readFile (const ContentStatus& found, std::string_view path)
    {
        using R = util::Result<std::vector<std::uint8_t>>;
        const auto shown = engine::escapeControlCharacters (path);
        if (found.folder == juce::File())
            return R::failure ("missing: " + shown + ": no content folder holds the kit");
        const auto folder = engine::escapeControlCharacters (found.folder.getFullPathName().toStdString());

        // The manifests' own path rule first, so nothing outside the kit's folder is ever touched.
        if (const auto problem = engine::contentPathProblem (path); ! problem.empty())
            return R::failure ("missing: " + shown + ": not a file inside " + folder + " (the path " + problem + ")");

        const auto file = found.folder.getChildFile (juce::String::fromUTF8 (path.data(), static_cast<int> (path.size())));
        if (file.isDirectory())
            return R::failure ("unreadable: " + shown + ": it is a folder, not a file");
        if (! file.existsAsFile())
            return R::failure ("missing: " + shown + ": no such file in " + folder);
        if (const auto size = file.getSize(); size < 0 || static_cast<std::uint64_t> (size) > engine::FileSampleSource::kMaxFileBytes)
            return R::failure ("format: " + shown + ": " + std::to_string (size) + " bytes, more than any content file holds");

        auto bytes = util::readBinaryFile (toPath (file));
        if (! bytes.ok())
            return R::failure ("unreadable: " + shown + ": it can't be read");
        return bytes;
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
