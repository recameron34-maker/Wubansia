#include "sounds/DrumKitWriter.h"

#include <cstdio>
#include <cstring>
#include <map>
#include <optional>
#include <string_view>
#include <vector>

#if JUCE_MAC
    #include <stdio.h> // renamex_np, RENAME_EXCL
#elif JUCE_WINDOWS
extern "C" __declspec (dllimport) int __stdcall MoveFileExW (const wchar_t* existing, const wchar_t* replacement, unsigned long flags);
#elif defined(__linux__)
    #include <cerrno>
    #include <fcntl.h>
    #include <sys/syscall.h>
    #include <unistd.h>
    #ifndef RENAME_NOREPLACE
        #define RENAME_NOREPLACE (1 << 0)
    #endif
#endif

namespace wubansia::sounds
{
    namespace
    {
        // The most names writeDrumKit tries: the plain one, then " (2)" ... " (999)", as exportCopy does.
        constexpr int kMaxCopies = 999;

        using Bytes = std::vector<std::uint8_t>;

        juce::String text (std::string_view s) { return juce::String::fromUTF8 (s.data(), static_cast<int> (s.size())); }

        // The kit as SampleExport's refusals name it.
        engine::KitInfo infoOf (const engine::KitManifest& kit)
        {
            engine::KitInfo info;
            info.id = kit.kit.id;
            info.title = kit.kit.title;
            info.contentVersion = kit.kit.contentVersion;
            return info;
        }

        // Whether file is a plain file (not a link) holding exactly these bytes.
        bool holdsBytes (const juce::File& file, const void* data, std::size_t size)
        {
            if (file.isSymbolicLink() || ! file.existsAsFile() || file.getSize() != static_cast<juce::int64> (size))
                return false;
            juce::MemoryBlock existing;
            return file.loadFileAsData (existing) && existing.getSize() == size && (size == 0 || std::memcmp (existing.getData(), data, size) == 0);
        }

        // Whether file is a plain file holding the bytes kit.json lists (its size, then its SHA-256).
        bool holdsListed (const juce::File& file, const engine::FileEntry& listed)
        {
            if (file.isSymbolicLink() || ! file.existsAsFile() || file.getSize() != static_cast<juce::int64> (listedBytes (listed)))
                return false;
            juce::MemoryBlock existing;
            if (! file.loadFileAsData (existing))
                return false;
            return matchesListing (std::span<const std::uint8_t> (static_cast<const std::uint8_t*> (existing.getData()), existing.getSize()), listed);
        }

        // bytes as a new file, unbuffered so a full disk shows in the write, checked by its size.
        bool writeWhole (const juce::File& target, const void* data, std::size_t size)
        {
            bool written = false;
            {
                juce::FileOutputStream out (target, 0);
                written = out.openedOk() && (size == 0 || out.write (data, size));
            }
            return written && target.getSize() == static_cast<juce::int64> (size);
        }

        // A hidden folder in samplesFolder no one else holds: ".wubansia-drumkit-<KIT>-<8 hex digits>".
        juce::File stagingIn (const juce::File& samplesFolder, const std::string& kit)
        {
            juce::Random random;
            for (;;)
            {
                const auto name = ".wubansia-drumkit-" + text (kit) + "-" + juce::String::toHexString (static_cast<int> (random.nextInt())).paddedLeft ('0', 8);
                const auto folder = samplesFolder.getChildFile (name);
                if (! folder.exists() && ! folder.isSymbolicLink())
                    return folder;
            }
        }

        DrumKitWrite found (const juce::File& folder)
        {
            DrumKitWrite out;
            out.folder = folder;
            return out;
        }

        DrumKitWrite failed (ExportResult why)
        {
            DrumKitWrite out;
            out.failure = std::move (why);
            return out;
        }
    }

    bool holdsDrumKit (const juce::File& folder, const engine::KitManifest& kit, const DrumKitPlan& plan)
    {
        if (folder.isSymbolicLink() || ! folder.isDirectory())
            return false;
        for (const auto& f : plan.files)
        {
            const auto file = folder.getChildFile (text (f.path));
            const bool holds = f.source >= 0 ? f.source < static_cast<int> (kit.files.size()) && holdsListed (file, kit.files[static_cast<std::size_t> (f.source)])
                                             : holdsBytes (file, f.text.data(), f.text.size());
            if (! holds)
                return false;
        }
        return true;
    }

    DrumKitWrite writeDrumKit (ContentSourceFactory& content, const ContentStatus& status, const engine::KitManifest& kit, const DrumKitPlan& plan,
        const juce::File& samplesFolder, const std::atomic<bool>& cancel, const std::function<void (int)>& progress)
    {
        if (samplesFolder == juce::File())
        {
            ExportResult none;
            none.problem = "there is no samples folder to save " + kit.kit.id + "'s drum kit into";
            return failed (none);
        }

        const auto plain = text (plan.folder);
        const auto nameAt = [&] (int k) { return samplesFolder.getChildFile (k == 1 ? plain : plain + " (" + juce::String (k) + ")"); };
        const auto isFree = [] (const juce::File& f) { return ! f.exists() && ! f.isSymbolicLink(); };

        // The kit's folder already there, or the first free name. A name holding anything else is never touched.
        int free = 0;
        for (int k = 1; k <= kMaxCopies && free == 0; ++k)
        {
            const auto target = nameAt (k);
            if (isFree (target))
                free = k;
            else if (holdsDrumKit (target, kit, plan))
                return found (target);
        }
        if (free == 0)
            return failed (folderRefusal (FolderProblem::namesTaken, nameAt (1)));

        // Written whole into a staging folder only this writes, then renamed into place.
        if (samplesFolder.createDirectory().failed())
            return failed (folderRefusal (FolderProblem::makeFolder, samplesFolder));
        const auto staging = stagingIn (samplesFolder, kit.kit.id);
        const auto drop = [&staging] (DrumKitWrite out) {
            staging.deleteRecursively();
            return out;
        };
        if (staging.createDirectory().failed() || staging.getChildFile ("Pads").createDirectory().failed()
            || staging.getChildFile ("Samples").createDirectory().failed())
            return drop (failed (folderRefusal (FolderProblem::makeFolder, nameAt (free))));

        const auto info = infoOf (kit);
        std::map<int, Bytes> read; // each installed file once: a pad's file is also in Samples/
        int done = 0;
        for (const auto& f : plan.files)
        {
            if (cancel.load (std::memory_order_relaxed))
            {
                DrumKitWrite out;
                out.cancelled = true;
                return drop (out);
            }
            const void* data = f.text.data();
            std::size_t size = f.text.size();
            if (f.source >= 0)
            {
                const auto& listed = kit.files[static_cast<std::size_t> (f.source)];
                auto at = read.find (f.source);
                if (at == read.end())
                {
                    auto bytes = content.readFile (status, listed.path);
                    if (! bytes.ok())
                        return drop (failed (unreadableSource (info, listed, bytes.error)));
                    if (! matchesListing (bytes.value, listed))
                        return drop (failed (mismatchedSource (info, listed)));
                    at = read.emplace (f.source, std::move (bytes.value)).first;
                }
                data = at->second.data();
                size = at->second.size();
            }
            if (! writeWhole (staging.getChildFile (text (f.path)), data, size))
                return drop (failed (folderRefusal (FolderProblem::write, nameAt (free).getChildFile (text (f.path)))));
            ++done;
            if (progress != nullptr)
                progress (done);
        }
        if (cancel.load (std::memory_order_relaxed))
        {
            DrumKitWrite out;
            out.cancelled = true;
            return drop (out);
        }

        // Into place under the first free name; another instance may take it first, with this kit or something else.
        for (int k = free; k <= kMaxCopies; ++k)
        {
            const auto target = nameAt (k);
            if (isFree (target))
            {
                if (renameNoReplace (staging, target))
                {
                    auto out = found (target);
                    out.wrote = true;
                    return out;
                }
                if (isFree (target))
                    return drop (failed (folderRefusal (FolderProblem::move, target)));
            }
            if (holdsDrumKit (target, kit, plan))
                return drop (found (target));
        }
        return drop (failed (folderRefusal (FolderProblem::namesTaken, nameAt (1))));
    }

    bool renameNoReplace (const juce::File& from, const juce::File& to)
    {
        if (to.exists() || to.isSymbolicLink())
            return false;
        const auto source = from.getFullPathName();
        const auto target = to.getFullPathName();
#if JUCE_MAC
        return renamex_np (source.toRawUTF8(), target.toRawUTF8(), RENAME_EXCL) == 0;
#elif JUCE_WINDOWS
        return MoveFileExW (source.toWideCharPointer(), target.toWideCharPointer(), 0) != 0; // no MOVEFILE_REPLACE_EXISTING
#elif defined(__linux__)
        if (syscall (SYS_renameat2, AT_FDCWD, source.toRawUTF8(), AT_FDCWD, target.toRawUTF8(), RENAME_NOREPLACE) == 0)
            return true;
        if (errno != EINVAL && errno != ENOSYS)
            return false;
        // A file system without the flag: a plain rename, while nothing is at the name (rename would replace a file or an
        // empty folder there).
        if (to.exists() || to.isSymbolicLink())
            return false;
        return std::rename (source.toRawUTF8(), target.toRawUTF8()) == 0;
#else
        if (to.exists() || to.isSymbolicLink())
            return false;
        return std::rename (source.toRawUTF8(), target.toRawUTF8()) == 0;
#endif
    }
}
