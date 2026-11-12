#pragma once

// The person's own settings (P9): today only the content folder chosen with "Choose content folder…".
// They live in one file per user, <app data>/Wubansia Audio/Wubansia/settings.xml next to the default
// content folder (ContentLocator::appDataFolder), never in a project's state, and every plugin
// instance in the process shares them: instance () is one object, and generation () tells the others
// that the folder changed.
//
// Nothing reads the file before the first contentFolder (): the processor's constructor never touches
// it. The setter writes the file at once; call it on the message thread.

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

        // Changes with every setContentFolder (). Any thread.
        std::uint64_t generation() const noexcept { return changes.load (std::memory_order_acquire); }

        const juce::File& file() const noexcept { return settingsFile; }

        // How many times the file was read: once at most, for tests.
        std::uint64_t fileReads() const noexcept { return reads.load (std::memory_order_acquire); }

        // Whether the drag strip's 1 drum track switch is lit (ADR 0025): Loop and Song drags give their drum tracks as
        // one track. Off unless the file says drag_drums_one_track="1". Reads the file once.
        bool dragDrumsOnOneTrack() const;

        // Lights the switch or turns it off, and saves the file at once, keeping whatever else it holds. generation ()
        // stays: the content folders don't change. False when the file couldn't be written: the choice still holds
        // until the process ends.
        bool setDragDrumsOnOneTrack (bool on);

    private:
        void loadLocked() const;

        const juce::File settingsFile;
        mutable std::mutex mutex;
        mutable bool loaded = false;
        mutable juce::File folder;
        std::atomic<std::uint64_t> changes { 0 };
        mutable std::atomic<std::uint64_t> reads { 0 };
    };
}
