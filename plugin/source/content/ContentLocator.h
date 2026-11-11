#pragma once

// Where a kit's samples are (P9; ADR 0022, "Where the plugin looks"). Per kit, the first ready folder
// among these content folders, in this order:
// 1. $WUBANSIA_CONTENT_DIR, for development, tests and groovecore render;
// 2. the folder chosen with "Choose content folder…" (UserSettings: per user, never in project state);
// 3. the platform's content folder, the same as tools/content/kitfiles.default_content_dir:
//    macOS ~/Library/Application Support/Wubansia Audio/Wubansia/Content, Linux
//    ~/.local/share/Wubansia Audio/Wubansia/Content (not JUCE's ~/.config), Windows
//    %APPDATA%\Wubansia Audio\Wubansia\Content;
// 4. Debug builds only: the checkout's content/samples, set at configure time.
// Inside each, <kit>/v<N>/ (N: the embedded kit.json's content_version), then <kit>/ (the repo's
// layout); kit folders are lowercase (fut01). A folder is ready when every file kit.json lists exists,
// isn't a Git LFS pointer, and is a 48 kHz WAV with the listed frames and channels (P4's
// checkKitFiles, from the headers alone). Other files there (the installers' SHA256SUMS, a README)
// are ignored. When no folder is ready, the status is the first problem met, in that order: missing
// files, LFS pointers (run git lfs pull), another version of the content (mismatch), or a file that
// can't be read (failed); or "missing" when no content folder has the kit at all.
//
// locate () reads the disk: call it on the service thread. setUserFolder () may come from the message
// thread meanwhile.

#include "Controller.h"

#include "kit/kit_builder.h"
#include "kit/manifest.h"

#include <juce_core/juce_core.h>

#include <filesystem>
#include <mutex>
#include <string_view>
#include <vector>

namespace wubansia
{
    class ContentLocator
    {
    public:
        // The content folders, by where they come from. juce::File () where there is none.
        struct Places
        {
            juce::File environment; // $WUBANSIA_CONTENT_DIR
            juce::File user; // "Choose content folder…"
            juce::File platformDefault;
            juce::File debug; // Debug builds: the checkout's content/samples
        };

        // The system's places (systemPlaces ()), with no user folder until setUserFolder ().
        ContentLocator();
        explicit ContentLocator (Places places);

        // The platform's content folder, and the folder that holds it and settings.xml:
        // <app data>/Wubansia Audio/Wubansia.
        static juce::File platformDefaultFolder();
        static juce::File appDataFolder();

        // $WUBANSIA_CONTENT_DIR (made absolute from the working directory), the platform's folder,
        // and in Debug builds the checkout's content/samples.
        static Places systemPlaces();

        // A kit's folder name: its id in lowercase ("fut01").
        static juce::String kitFolderName (std::string_view kitId);

        void setUserFolder (const juce::File& folder);
        void setPlaces (Places p);

        // The content folders in search order, each once, leaving out the ones not set.
        std::vector<juce::File> roots() const;

        // Where kit's samples are (see the top). searched lists every kit folder looked at, in order.
        ContentStatus locate (const engine::KitManifest& kit) const;

    private:
        mutable std::mutex mutex;
        Places places;
    };

    // A juce::File as a std::filesystem::path, for P4's FileSampleSource (UTF-8 everywhere but Windows).
    std::filesystem::path toPath (const juce::File& file);

    // A load error's content state, from its code (P4's loadErrorOf), never from its words: missing,
    // LFS pointers, another version (mismatch), anything else failed.
    ContentState contentStateOf (engine::LoadError error) noexcept;

    // A problem as the editor shows it: one sentence, as given, without a full stop (the panel adds one).
    juce::String problemText (const juce::String& problem);
}
