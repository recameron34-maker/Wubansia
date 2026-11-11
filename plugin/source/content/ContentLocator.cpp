#include "content/ContentLocator.h"

#include "kit/file_sample_source.h"

#include <algorithm>
#include <optional>
#include <utility>

namespace wubansia
{
    ContentState contentStateOf (engine::LoadError error) noexcept
    {
        switch (error)
        {
            case engine::LoadError::none: return ContentState::ready;
            case engine::LoadError::missing: return ContentState::missing;
            case engine::LoadError::lfsPointers: return ContentState::lfsPointers;
            case engine::LoadError::mismatch: return ContentState::mismatch;
            case engine::LoadError::cancelled:
            case engine::LoadError::failed: break;
        }
        return ContentState::failed;
    }

    juce::String problemText (const juce::String& problem)
    {
        auto text = problem.trim();
        while (text.endsWithChar ('.'))
            text = text.dropLastCharacters (1).trimEnd();
        return text;
    }

    std::filesystem::path toPath (const juce::File& file)
    {
#if JUCE_WINDOWS
        return std::filesystem::path (file.getFullPathName().toWideCharPointer());
#else
        return std::filesystem::path (file.getFullPathName().toStdString());
#endif
    }

    ContentLocator::ContentLocator() : ContentLocator (systemPlaces()) {}

    ContentLocator::ContentLocator (Places p) : places (std::move (p)) {}

    juce::File ContentLocator::appDataFolder()
    {
#if JUCE_MAC
        return juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile ("Library").getChildFile ("Application Support").getChildFile ("Wubansia Audio").getChildFile ("Wubansia");
#elif JUCE_WINDOWS
        // %APPDATA% (Roaming), as tools/content/kitfiles.default_content_dir has it.
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Wubansia Audio").getChildFile ("Wubansia");
#else
        // ~/.local/share, not JUCE's userApplicationDataDirectory (~/.config): the installers' folder.
        return juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile (".local").getChildFile ("share").getChildFile ("Wubansia Audio").getChildFile ("Wubansia");
#endif
    }

    juce::File ContentLocator::platformDefaultFolder()
    {
        return appDataFolder().getChildFile ("Content");
    }

    ContentLocator::Places ContentLocator::systemPlaces()
    {
        Places p;
        const auto environment = juce::SystemStats::getEnvironmentVariable ("WUBANSIA_CONTENT_DIR", {}).trim();
        if (environment.isNotEmpty())
            p.environment = juce::File::getCurrentWorkingDirectory().getChildFile (environment);
        p.platformDefault = platformDefaultFolder();
#if defined(WUBANSIA_DEV_CONTENT_DIR)
        p.debug = juce::File (juce::String::fromUTF8 (WUBANSIA_DEV_CONTENT_DIR));
#endif
        return p;
    }

    juce::String ContentLocator::kitFolderName (std::string_view kitId)
    {
        return juce::String::fromUTF8 (kitId.data(), static_cast<int> (kitId.size())).toLowerCase();
    }

    void ContentLocator::setUserFolder (const juce::File& folder)
    {
        const std::lock_guard lock (mutex);
        places.user = folder;
    }

    void ContentLocator::setPlaces (Places p)
    {
        const std::lock_guard lock (mutex);
        places = std::move (p);
    }

    std::vector<juce::File> ContentLocator::roots() const
    {
        Places p;
        {
            const std::lock_guard lock (mutex);
            p = places;
        }
        std::vector<juce::File> out;
        for (const auto* folder : { &p.environment, &p.user, &p.platformDefault, &p.debug })
            if (*folder != juce::File() && std::find (out.begin(), out.end(), *folder) == out.end())
                out.push_back (*folder);
        return out;
    }

    ContentStatus ContentLocator::locate (const engine::KitManifest& kit) const
    {
        ContentStatus status;
        const auto folderName = kitFolderName (kit.kit.id);
        const auto versionName = "v" + juce::String (kit.kit.contentVersion);
        std::optional<ContentStatus> firstProblem;

        for (const auto& root : roots())
        {
            const auto kitFolder = root.getChildFile (folderName);
            for (const auto& candidate : { kitFolder.getChildFile (versionName), kitFolder })
            {
                status.searched.add (candidate.getFullPathName());
                if (! candidate.isDirectory())
                    continue;

                const engine::FileSampleSource source (toPath (candidate));
                const auto checked = engine::checkKitFiles (kit, source);
                if (checked.ok())
                {
                    status.state = ContentState::ready;
                    status.folder = candidate;
                    status.problem = {};
                    return status;
                }
                if (! firstProblem)
                {
                    ContentStatus problem;
                    problem.state = contentStateOf (engine::loadErrorOf (checked.error));
                    problem.problem = problemText (juce::String::fromUTF8 (checked.error.data(), static_cast<int> (checked.error.size())));
                    problem.folder = candidate;
                    firstProblem = problem;
                }
            }
        }

        if (firstProblem)
        {
            status.state = firstProblem->state;
            status.problem = firstProblem->problem;
            status.folder = firstProblem->folder;
            return status;
        }
        status.state = ContentState::missing;
        status.problem = "No content folder holds " + juce::String (kit.kit.id) + " (" + folderName + "/" + versionName + " or " + folderName
                         + "): install its content, or choose the folder that holds it";
        return status;
    }
}
