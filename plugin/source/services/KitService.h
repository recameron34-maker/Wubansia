#pragma once

// The kit service (P9): one thread per plugin instance that makes Kits and hands them to the engine,
// so the audio thread never waits, allocates or reads a file for a kit. The processor owns it, so it
// works with the editor closed; it starts at the first request (the processor's first prepareToPlay
// or setStateInformation), never in a constructor, so the hosts' and validators' scans that make
// instances by the dozen start no thread.
//
// One load slot, latest wins: a request replaces any queued one and cancels the load in progress
// (P4's buildKit stops within a file; KitCache's wait at once). A load, in order:
// 1. the kit's manifests, parsed once per instance (KitCatalog);
// 2. the content folder (ContentSourceFactory::locate): not ready, and the engine gets a silent Kit
//    (the kit's mix and timelines, no instruments), so the plugin is silent rather than playing the
//    kit before, and the status names the first problem;
// 3. KitCache::acquire, which shares a live Kit with other instances or builds it here: the four
//    timelines (P7), then the samples (P4's buildKit, reporting progress);
// 4. Engine::publish.
// Every 50 ms (and between a build's files) it collects the Kits the engine has let go. It holds one
// shared_ptr per publish (never one per pointer): a Kit comes back once per publish, from publish ()
// (never adopted) or collectRetired () (adopted and let go, or a duplicate of the adopted Kit), and
// the service drops one reference per pointer that comes back; a Kit the engine holds when the
// service is destroyed is dropped then. The last reference frees a Kit here, never on the audio
// thread.
//
// Between loads it checks every kit's content folder for the kit cards, again whenever the folders
// change (in any instance), and every 2 s for kits that weren't found, reloading the current kit
// when its content appears.

#include "content/ContentSource.h"
#include "services/KitCatalog.h"

#include "engine.h"
#include "kit/kit.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace wubansia
{
    class KitService
    {
    public:
        // Where the last request stands.
        struct LoadStatus
        {
            juce::String kit; // the kit asked for last; "" before any request
            ContentStatus content; // loading (with a percentage), ready, or the first problem
            std::uint64_t generation = 0; // the request it reports on: 1 for the first
        };

        static constexpr auto kDrainInterval = std::chrono::milliseconds (50);
        static constexpr auto kRecheckInterval = std::chrono::seconds (2);

        KitService (engine::Engine& engine, ContentSourceFactory& content);

        // Cancels a load in progress and stops the thread: back within a file's read (200 ms at worst
        // for the content's largest files), then drops every Kit it holds.
        ~KitService();

        KitService (const KitService&) = delete;
        KitService& operator= (const KitService&) = delete;

        // The kits, parsed on first use. Any thread but the audio thread.
        const KitCatalog& catalog() const;

        // Queues a load of kitId, replacing any queued one and cancelling the one in progress; starts
        // the thread the first time. contentVersion is the version a project asks for: the service
        // loads the version the plugin carries, and the status says so when they differ. Any thread
        // but the audio thread; returns at once.
        void request (std::string_view kitId, int contentVersion);

        // Whether request () was ever called (the thread runs).
        bool started() const noexcept { return startedFlag.load (std::memory_order_acquire); }

        LoadStatus status() const;

        // From a request until its load has published its Kit or failed. The audio thread reads it for
        // the offline wait (ADR 0005 D5): never true for long when content is missing.
        bool loadInFlight() const noexcept { return inFlight.load (std::memory_order_acquire); }

        // Counts requests: the offline wait gives each load its own 30 s.
        std::uint64_t loadGeneration() const noexcept { return generation.load (std::memory_order_acquire); }

        // A kit's content status for its card (by its catalog index): the current kit's load, or the
        // last check of its folders.
        ContentStatus kitStatus (int index) const;

        // The content folders changed: check every kit again, and reload the current kit unless it is
        // ready. Any thread but the audio thread.
        void contentChanged();

        // For tests and diagnostics: the references the service holds (one per publish the engine
        // hasn't handed back), how many Kits it published, and how many came back.
        int heldKits() const noexcept { return held.load (std::memory_order_acquire); }
        int publishedKits() const noexcept { return publishedCount.load (std::memory_order_acquire); }
        int returnedKits() const noexcept { return returnedCount.load (std::memory_order_acquire); }

    private:
        struct Request
        {
            std::string kit;
            int contentVersion = 0;
            std::uint64_t generation = 0;
        };

        void run();
        void load (const Request& job);
        void publish (std::shared_ptr<const engine::Kit> kit);
        void drop (const engine::Kit* kit);
        void drain();
        void idle (std::chrono::steady_clock::time_point now);
        void survey (bool onlyMissing);
        bool cancelled() const noexcept { return cancel.load (std::memory_order_acquire); }
        void finish (const Request& job, const ContentStatus& content, const KitCatalog::Entry* entry);
        std::shared_ptr<const engine::Kit> silentKit (const KitCatalog::Entry* entry, std::string_view id);
        void setLoading (const Request& job, int percent, const ContentStatus* found);

        engine::Engine& engine;
        ContentSourceFactory& content;

        mutable std::once_flag catalogOnce;
        mutable std::unique_ptr<KitCatalog> catalogPtr;

        // Guarded by mutex: the request slot, the thread's life, and what status () and kitStatus () read.
        mutable std::mutex mutex;
        std::condition_variable wake;
        std::optional<Request> pending;
        bool quit = false;
        std::thread thread;
        LoadStatus current;
        std::vector<ContentStatus> statuses; // by catalog index
        std::string lastKit; // the last request's kit and version, for reloads
        int lastVersion = 0;

        std::atomic<bool> startedFlag { false };
        std::atomic<bool> inFlight { false };
        std::atomic<bool> cancel { false };
        std::atomic<std::uint64_t> generation { 0 };

        // The service thread's own.
        std::vector<std::shared_ptr<const engine::Kit>> kits; // one per publish not yet handed back
        std::chrono::steady_clock::time_point lastDrain {};
        std::chrono::steady_clock::time_point lastRecheck {};
        std::uint64_t surveyedFolders = 0;
        bool surveyed = false;

        std::atomic<int> held { 0 };
        std::atomic<int> publishedCount { 0 };
        std::atomic<int> returnedCount { 0 };
    };
}
