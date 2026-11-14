#include "content/UserSettings.h"

#include "content/ContentLocator.h"

namespace wubansia
{
    namespace
    {
        constexpr const char* rootTag = "WubansiaSettings";
        constexpr const char* dragDrumsAttribute = "drag_drums_one_track"; // "1" lights the drag strip's 1 drum track (ADR 0025)
        constexpr const char* contentFolderAttribute = "content_folder";
        constexpr const char* samplesFolderAttribute = "samples_folder"; // ADR 0024

        // The folder an attribute names: an absolute path, else none.
        juce::File folderIn (const juce::XmlElement& xml, const char* attribute)
        {
            const auto path = xml.getStringAttribute (attribute);
            return path.isNotEmpty() && juce::File::isAbsolutePath (path) ? juce::File (path) : juce::File();
        }
    }

    UserSettings& UserSettings::instance()
    {
        static UserSettings settings (defaultFile());
        return settings;
    }

    UserSettings::UserSettings (juce::File file) : settingsFile (std::move (file)) {}

    juce::File UserSettings::defaultFile()
    {
        return ContentLocator::appDataFolder().getChildFile ("settings.xml");
    }

    void UserSettings::loadLocked() const
    {
        if (loaded)
            return;
        loaded = true;
        reads.fetch_add (1, std::memory_order_acq_rel);
        if (! settingsFile.existsAsFile())
            return;
        const auto xml = juce::XmlDocument::parse (settingsFile);
        const bool ours = xml != nullptr && xml->hasTagName (rootTag);
        if (ours)
        {
            folder = folderIn (*xml, contentFolderAttribute);
            samples = folderIn (*xml, samplesFolderAttribute);
        }
        dragDrums = ours && xml->getStringAttribute (dragDrumsAttribute) == "1";
    }

    bool UserSettings::saveLocked (const char* attribute, const juce::String& value) const
    {
        // Keep whatever else the file holds.
        std::unique_ptr<juce::XmlElement> xml;
        if (settingsFile.existsAsFile())
            xml = juce::XmlDocument::parse (settingsFile);
        if (xml == nullptr || ! xml->hasTagName (rootTag))
            xml = std::make_unique<juce::XmlElement> (rootTag);
        if (value.isEmpty())
            xml->removeAttribute (attribute);
        else
            xml->setAttribute (attribute, value);

        return settingsFile.getParentDirectory().createDirectory().wasOk() && xml->writeTo (settingsFile);
    }

    juce::File UserSettings::contentFolder() const
    {
        const std::lock_guard lock (mutex);
        loadLocked();
        return folder;
    }

    bool UserSettings::dragDrumsOnOneTrack() const
    {
        const std::lock_guard lock (mutex);
        loadLocked();
        return dragDrums;
    }

    bool UserSettings::setDragDrumsOnOneTrack (bool on)
    {
        // generation () stays: the content folders don't change.
        const std::lock_guard lock (mutex);
        loadLocked();
        dragDrums = on;
        return saveLocked (dragDrumsAttribute, on ? juce::String ("1") : juce::String());
    }

    bool UserSettings::setContentFolder (const juce::File& chosen)
    {
        bool saved = false;
        {
            const std::lock_guard lock (mutex);
            loadLocked();
            folder = chosen;
            saved = saveLocked (contentFolderAttribute, chosen.getFullPathName());
        }
        changes.fetch_add (1, std::memory_order_acq_rel);
        return saved;
    }

    juce::File UserSettings::samplesFolder() const
    {
        const std::lock_guard lock (mutex);
        loadLocked();
        return samples;
    }

    bool UserSettings::setSamplesFolder (const juce::File& chosen)
    {
        // generation () stays: the content folders the service searches don't change.
        const std::lock_guard lock (mutex);
        loadLocked();
        samples = chosen;
        return saveLocked (samplesFolderAttribute, chosen.getFullPathName());
    }
}
