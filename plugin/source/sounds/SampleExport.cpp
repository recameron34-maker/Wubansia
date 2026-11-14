#include "sounds/SampleExport.h"

#include "dragout/DragFiles.h"

#include "util/sha256.h"

#include <algorithm>
#include <string_view>
#include <utility>
#include <vector>

namespace wubansia::sounds
{
    namespace
    {
        // The most names exportCopy tries: the plain one, then " (2)" ... " (999)".
        constexpr int kMaxCopies = 999;

        juce::String text (std::string_view s) { return juce::String::fromUTF8 (s.data(), static_cast<int> (s.size())); }

        // A kit.json path's file name, "Rave Stab v2.wav", and without ".wav", "Rave Stab v2".
        std::string_view fileNameOf (std::string_view path)
        {
            const auto slash = path.find_last_of ('/');
            return slash == std::string_view::npos ? path : path.substr (slash + 1);
        }

        std::string_view stemOf (std::string_view path)
        {
            auto name = fileNameOf (path);
            if (name.ends_with (".wav"))
                name.remove_suffix (4);
            return name;
        }

        // Never a hidden file: no leading dot (or the space a dropped one leaves).
        std::string visible (std::string name)
        {
            const auto start = name.find_first_not_of (". ");
            return start == std::string::npos ? std::string() : name.substr (start);
        }

        // Whether file holds exactly these bytes: its size, then a byte compare.
        bool holds (const juce::File& file, const std::vector<std::uint8_t>& bytes)
        {
            if (! file.existsAsFile() || file.getSize() != static_cast<juce::int64> (bytes.size()))
                return false;
            juce::MemoryBlock existing;
            return file.loadFileAsData (existing) && existing.getSize() == bytes.size()
                   && std::equal (bytes.begin(), bytes.end(), static_cast<const std::uint8_t*> (existing.getData()));
        }

        // A copy made, or found there already.
        ExportResult copied (const juce::File& file)
        {
            ExportResult result;
            result.file = file;
            return result;
        }

        // No copy, for a reason that isn't the kit's own file: the samples folder's.
        ExportResult refused (std::string problem)
        {
            ExportResult result;
            result.problem = std::move (problem);
            return result;
        }

        // bytes as target, which doesn't exist yet: a hidden temporary file beside it, written whole and checked,
        // then renamed into place, so target is never a part-written file. Why not, or "".
        // (File::replaceWithData () would rename a temporary file whose write failed half way.)
        std::string writeNew (const juce::File& target, const std::vector<std::uint8_t>& bytes)
        {
            const juce::TemporaryFile temp (target, juce::TemporaryFile::useHiddenFile);
            bool written = false;
            {
                // Unbuffered, so write () reports the write itself: a full disk shows here, not in a destructor.
                juce::FileOutputStream out (temp.getFile(), 0);
                written = out.openedOk() && out.write (bytes.data(), bytes.size());
            }
            if (! written || temp.getFile().getSize() != static_cast<juce::int64> (bytes.size()))
                return "can't write " + target.getFullPathName().toStdString();
            if (! temp.overwriteTargetFileWithTemporary())
                return "can't move the copy into place as " + target.getFullPathName().toStdString();
            return {};
        }
    }

    juce::File defaultSamplesFolder()
    {
        return juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Wubansia").getChildFile ("Samples");
    }

    std::string kitFolderName (const engine::KitInfo& kit)
    {
        return visible (dragout::safeName (kit.id + " " + kit.title));
    }

    std::string exportName (const engine::KitInfo& kit, const engine::FileEntry& file)
    {
        const auto version = kit.contentVersion >= 2 ? " c" + std::to_string (kit.contentVersion) : std::string();
        const auto suffix = dragout::safeName (kit.id + " " + std::to_string (kit.bpm) + " " + dragout::shortKey (kit.key) + " " + kit.camelot + version);

        // The installed name, less a leading "Accent " as the drawer's names drop it (SoundCatalog's rule 6): a pad or
        // Simpler shows the start of a name, which must be the sound.
        auto installed = stemOf (file.path);
        if (installed.starts_with ("Accent ") && installed.size() > 7)
            installed.remove_prefix (7);
        auto stem = visible (dragout::safeName (installed));

        // A long installed name is cut, so the kit's part always fits.
        const auto room = kMaxNameStem > suffix.size() + 3 ? kMaxNameStem - suffix.size() - 3 : std::size_t { 1 };
        if (stem.size() > room)
            stem = visible (dragout::safeName (std::string_view (stem).substr (0, room)));
        if (stem.empty())
            stem = "Sound";
        return stem + " - " + suffix + ".wav";
    }

    std::uint64_t listedBytes (const engine::FileEntry& file) noexcept
    {
        if (file.frames <= 0 || file.channels <= 0)
            return 0;
        const auto audio = static_cast<std::uint64_t> (file.frames) * static_cast<std::uint64_t> (file.channels) * 3u;
        return 44u + audio + (audio & 1u);
    }

    bool matchesListing (std::span<const std::uint8_t> bytes, const engine::FileEntry& file)
    {
        return bytes.size() == listedBytes (file) && util::sha256Hex (bytes) == file.sha256;
    }

    std::string listingProblem (const engine::KitInfo& kit, const engine::FileEntry& file)
    {
        return std::string (fileNameOf (file.path)) + " isn't the file " + kit.id + "'s content v" + std::to_string (kit.contentVersion)
               + " lists: reinstall " + kit.id;
    }

    ExportResult unreadableSource (const engine::KitInfo&, const engine::FileEntry& file, const std::string& error)
    {
        auto result = refused (std::string (fileNameOf (file.path)) + " can't be read: " + error); // not yet: the tests come first
        result.sourceProblem = true;
        return result;
    }

    SoundExport soundExportOf (const ExportResult& result)
    {
        SoundExport copy;
        copy.file = result.file;
        copy.problem = text (result.problem);
        copy.sourceProblem = result.sourceProblem;
        return copy; // not yet: the tests come first
    }

    ExportResult exportCopy (ContentSourceFactory& content, const ContentStatus& found, const engine::KitInfo& kit,
        const engine::FileEntry& file, const juce::File& samplesFolder)
    {
        // The installed bytes, checked against kit.json before anything is written: the kit's file is the problem.
        const auto bytes = content.readFile (found, file.path);
        if (! bytes.ok())
            return unreadableSource (kit, file, bytes.error);
        if (! matchesListing (bytes.value, file))
        {
            auto result = refused (listingProblem (kit, file));
            result.sourceProblem = true;
            return result;
        }
        if (samplesFolder == juce::File())
            return refused ("there is no samples folder to copy " + std::string (fileNameOf (file.path)) + " into");

        const auto folder = samplesFolder.getChildFile (text (kitFolderName (kit)));
        const auto name = text (exportName (kit, file));
        const auto stem = name.dropLastCharacters (4); // ".wav"
        for (int k = 1; k <= kMaxCopies; ++k)
        {
            const auto target = folder.getChildFile (k == 1 ? name : stem + " (" + juce::String (k) + ").wav");
            if (! target.exists() && ! target.isSymbolicLink())
            {
                if (folder.createDirectory().failed())
                    return refused ("can't make the folder " + folder.getFullPathName().toStdString());
                if (auto problem = writeNew (target, bytes.value); ! problem.empty())
                    return refused (std::move (problem));
                return copied (target);
            }
            if (holds (target, bytes.value))
                return copied (target); // the copy is there already: nothing is written
            // Another file holds the name (a copy edited in a DAW, say): it is never touched; the next name.
        }
        return refused ("every name from " + name.toStdString() + " to its (" + std::to_string (kMaxCopies) + ") is taken in "
                        + folder.getFullPathName().toStdString());
    }
}
