#pragma once

// The person's own settings (P9): the content folder chosen with "Choose content folder…" and the
// samples folder chosen with the Samples drawer's "Change…" (ADR 0024). They live in one file per user,
// <app data>/Wubansia Audio/Wubansia/settings.xml next to the default content folder
// (ContentLocator::appDataFolder), as the root's attributes content_folder and samples_folder, never in
// a project's state, and every plugin instance in the process shares them: instance () is one object,
// and generation () tells the others that the content folder changed.
//
// Nothing reads the file before the first contentFolder () or samplesFolder (): the processor's
// constructor never touches it. The setters write the file at once, keeping whatever else it holds;
// call them on the message thread.

#include <juce_core/juce_core.h>

#include <atomic>
#include <cstdint>
#include <mutex>

namespace wubansia
{
    class UserSettings
    {
    public:
        // The person's settings file, shared by every instance in the process.
        static UserSettings& instance();

        // Settings kept in file, for tests.
        explicit UserSettings (juce::File file);

        // <app data>/Wubansia Audio/Wubansia/settings.xml.
        static juce::File defaultFile();

        // The chosen content folder, or juce::File () when none was chosen. Reads the file once.
        juce::File contentFolder() const;

        // Chooses folder (juce::File () forgets the choice) and saves the file. False when the file
        // couldn't be written: the folder is still used until the process ends.
        bool setContentFolder (const juce::File& folder);

        // The chosen samples folder, where the Samples drawer copies sounds, or juce::File () when none was
        // chosen. Reads the file once.
        juce::File samplesFolder() const;

        // Chooses folder (juce::File () forgets the choice) and saves the file, as setContentFolder () does but
        // without moving generation (): the content folders searched don't change. False when the file couldn't
        // be written: the folder is still used until the process ends.
        bool setSamplesFolder (const juce::File& folder);

        // Changes with every setContentFolder (), never with setSamplesFolder (). Any thread.
        std::uint64_t generation() const noexcept { return changes.load (std::memory_order_acquire); }

        const juce::File& file() const noexcept { return settingsFile; }

        // How many times the file was read: once at most, for tests.
        std::uint64_t fileReads() const noexcept { return reads.load (std::memory_order_acquire); }

    private:
        void loadLocked() const;

        // Writes the file with attribute set to chosen's path (removed for juce::File ()), keeping the rest.
        bool saveLocked (const char* attribute, const juce::File& chosen) const;

        const juce::File settingsFile;
        mutable std::mutex mutex;
        mutable bool loaded = false;
        mutable juce::File folder;
        mutable juce::File samples;
        std::atomic<std::uint64_t> changes { 0 };
        mutable std::atomic<std::uint64_t> reads { 0 };
    };
}
