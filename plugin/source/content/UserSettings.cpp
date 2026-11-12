#include "content/UserSettings.h"

#include "content/ContentLocator.h"

namespace wubansia
{
    namespace
    {
        constexpr const char* rootTag = "WubansiaSettings";
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
        if (const auto xml = juce::XmlDocument::parse (settingsFile); xml != nullptr && xml->hasTagName (rootTag))
        {
            const auto path = xml->getStringAttribute (contentFolderAttribute);
            if (path.isNotEmpty() && juce::File::isAbsolutePath (path))
                folder = juce::File (path);
        }
    }

    juce::File UserSettings::contentFolder() const
    {
        const std::lock_guard lock (mutex);
        loadLocked();
        return folder;
    }

    bool UserSettings::dragDrumsOnOneTrack() const
    {
        return false; // not yet
    }

    bool UserSettings::setDragDrumsOnOneTrack (bool on)
    {
        juce::ignoreUnused (on); // not yet
        return false;
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
