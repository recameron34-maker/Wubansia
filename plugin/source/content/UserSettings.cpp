#include "content/UserSettings.h"

#include "content/ContentLocator.h"

namespace wubansia
{
    namespace
    {
        constexpr const char* rootTag = "WubansiaSettings";
        constexpr const char* dragDrumsAttribute = "drag_drums_one_track"; // "1" lights the drag strip's 1 drum track (ADR 0025)
        constexpr const char* contentFolderAttribute = "content_folder";
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
            const auto path = xml->getStringAttribute (contentFolderAttribute);
            if (path.isNotEmpty() && juce::File::isAbsolutePath (path))
                folder = juce::File (path);
        }
        dragDrums = ours && xml->getStringAttribute (dragDrumsAttribute) == "1";
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
        return saveAttributeLocked (dragDrumsAttribute, on ? juce::String ("1") : juce::String());
    }

    bool UserSettings::saveAttributeLocked (const char* attribute, const juce::String& value) const
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

    bool UserSettings::setContentFolder (const juce::File& chosen)
    {
        bool saved = false;
        {
            const std::lock_guard lock (mutex);
            loadLocked();
            folder = chosen;

            // Keep whatever else the file holds.
            std::unique_ptr<juce::XmlElement> xml;
            if (settingsFile.existsAsFile())
                xml = juce::XmlDocument::parse (settingsFile);
            if (xml == nullptr || ! xml->hasTagName (rootTag))
                xml = std::make_unique<juce::XmlElement> (rootTag);
            if (chosen == juce::File())
                xml->removeAttribute (contentFolderAttribute);
            else
                xml->setAttribute (contentFolderAttribute, chosen.getFullPathName());

            saved = settingsFile.getParentDirectory().createDirectory().wasOk() && xml->writeTo (settingsFile);
        }
        changes.fetch_add (1, std::memory_order_acq_rel);
        return saved;
    }
}
