#include "content/UserSettings.h"

#include "content/ContentLocator.h"

#include <algorithm>

namespace wubansia
{
    namespace
    {
        constexpr const char* rootTag = "WubansiaSettings";
        constexpr const char* dragDrumsAttribute = "drag_drums_one_track"; // "1" lights the drag strip's 1 drum track (ADR 0025)
        constexpr const char* dragSynthsAttribute = "drag_for_synths"; // "0" turns the drag strip's For my synths off (ADR 0027)
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
        dragSynths = ! (ours && xml->getStringAttribute (dragSynthsAttribute) == "0");
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

    bool UserSettings::dragForSynths() const
    {
        const std::lock_guard lock (mutex);
        loadLocked();
        return dragSynths;
    }

    bool UserSettings::setDragForSynths (bool on)
    {
        // generation () stays: the content folders don't change. On is the default, so the file says only "0".
        const std::lock_guard lock (mutex);
        loadLocked();
        dragSynths = on;
        return saveLocked (dragSynthsAttribute, on ? juce::String() : juce::String ("0"));
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

    // The Phrases drawer's (ADR 0031): read from the file each time, under the lock, so another process's writes count.
    namespace
    {
        juce::String phraseAttribute (const char* what, const juce::String& kit) { return juce::String ("phrases_") + what + "_" + kit.toLowerCase(); }
    }

    juce::String UserSettings::fileAttributeLocked (const juce::String& attribute) const
    {
        if (! settingsFile.existsAsFile())
            return {};
        const auto xml = juce::XmlDocument::parse (settingsFile);
        return xml != nullptr && xml->hasTagName (rootTag) ? xml->getStringAttribute (attribute) : juce::String();
    }

    std::uint64_t UserSettings::phrasesNextSeed (const juce::String& kit) const
    {
        const std::lock_guard lock (mutex);
        const auto value = fileAttributeLocked (phraseAttribute ("next", kit)).trim();
        if (value.isEmpty() || ! value.containsOnly ("0123456789"))
            return firstMoreSeed;
        return std::max<std::uint64_t> (firstMoreSeed, static_cast<std::uint64_t> (value.getLargeIntValue()));
    }

    bool UserSettings::setPhrasesNextSeed (const juce::String& kit, std::uint64_t seed)
    {
        const std::lock_guard lock (mutex);
        const auto attribute = phraseAttribute ("next", kit);
        const auto inFile = fileAttributeLocked (attribute).trim();
        auto next = seed;
        if (inFile.isNotEmpty() && inFile.containsOnly ("0123456789"))
            next = std::max (next, static_cast<std::uint64_t> (inFile.getLargeIntValue())); // never back
        return saveLocked (attribute.toRawUTF8(), juce::String (static_cast<juce::int64> (next)));
    }

    juce::StringArray UserSettings::keptPhrases (const juce::String& kit) const
    {
        const std::lock_guard lock (mutex);
        return juce::StringArray::fromTokens (fileAttributeLocked (phraseAttribute ("kept", kit)), " ", "");
    }

    bool UserSettings::setPhraseKept (const juce::String& kit, const juce::String& id, bool kept)
    {
        if (id.isEmpty() || id.containsAnyOf (" \t\r\n"))
            return false;
        const std::lock_guard lock (mutex);
        const auto attribute = phraseAttribute ("kept", kit);
        auto ids = juce::StringArray::fromTokens (fileAttributeLocked (attribute), " ", ""); // as the file has them now
        ids.removeEmptyStrings();
        if (kept)
            ids.addIfNotAlreadyThere (id);
        else
            ids.removeString (id);
        return saveLocked (attribute.toRawUTF8(), ids.joinIntoString (" "));
    }
}
