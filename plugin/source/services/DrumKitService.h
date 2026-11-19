#pragma once

// Drum kits (ADR 0030): the drum kit thread, one per processor ("Wubansia drum kits"), started by the first request (),
// never by a constructor, so the hosts' and validators' scans start nothing. It plans the kit's drum kit
// (sounds::planDrumKit) and writes or finds its folder in the samples folder (sounds::writeDrumKit): tens of
// milliseconds on a warm disk, far longer on a cold one or a network folder, too long for a mouse event.
//
// One request slot, latest wins. A request equal to the one queued or in hand (kit, content version, samples folder,
// content folder, kick side) keeps its ticket: a press and its click are one job. A different one replaces the queued
// one and cancels the one in hand between two files. The last finished request is kept with its result: a request equal
// to it gets its ticket back, ready at once, while its folder is a folder and its SFZ a file (two stats on the message
// thread, never a byte re-check, so a drag that follows the press starts inside its mouse event); if either is gone the
// kit is saved again under a new ticket.
//
// The request copies the kit's parsed manifests, its ready content status, the samples folder and the kick side: the
// thread holds nothing of the catalog's. status () is read under a mutex by the editor's 30 Hz tick. The destructor
// cancels between files, joins, and the writer has deleted its staging folder by then. Nothing here touches the audio
// thread.

#include "Controller.h"
#include "content/ContentSource.h"

#include "kit/manifest.h"
#include "mix/mix_config.h"

#include <juce_core/juce_core.h>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <thread>

namespace wubansia
{
    class DrumKitService
    {
    public:
        struct Request
        {
            juce::String kit; // "FUT01"
            engine::KitManifest manifest; // copied: the thread holds nothing of the catalog's
            engine::MixConfig mix;
            ContentStatus found; // ready
            juce::File samplesFolder;
            int kickSide = 1; // engine::Controls::ab[kick]: which SFZ the status names
        };

        explicit DrumKitService (ContentSourceFactory& content); // starts nothing
        ~DrumKitService(); // cancels between files, joins

        DrumKitService (const DrumKitService&) = delete;
        DrumKitService& operator= (const DrumKitService&) = delete;

        std::uint32_t request (Request request); // message thread: its ticket (the same job keeps its own)
        void refuse (const juce::String& kit, const juce::String& problem); // message thread: turned down, no ticket
        DrumKitStatus status() const; // any thread but the audio thread
        bool started() const noexcept { return startedFlag.load (std::memory_order_acquire); }

        // Called on the drum kit thread after each file is written, with the files done so far. Before the first request.
        void setFileProbeForTesting (std::function<void (int filesDone)> probe) { fileProbe = std::move (probe); }

    private:
        // What makes two requests one job.
        struct Key
        {
            juce::String kit;
            int contentVersion = 0;
            juce::File samplesFolder, contentFolder;
            int kickSide = 1;

            bool operator== (const Key&) const = default;
        };

        struct Job
        {
            std::uint32_t ticket = 0;
            Key key;
            Request request;
        };

        void run();

        ContentSourceFactory& content;
        std::function<void (int)> fileProbe;

        mutable std::mutex mutex;
        std::condition_variable wake;
        std::optional<Job> queued;
        std::optional<Key> inHandKey; // the job the thread is on
        std::uint32_t inHandTicket = 0;
        std::uint32_t latest = 0; // the last ticket given
        std::uint32_t wanted = 0; // the ticket whose result status () shows; 0: a refusal, or none
        std::optional<std::pair<Key, DrumKitStatus>> lastReady; // the last finished request that is ready
        DrumKitStatus current;
        bool quit = false;
        std::atomic<bool> cancel { false }; // the job in hand stops between files
        std::atomic<bool> startedFlag { false };
        std::thread thread;
    };
}
