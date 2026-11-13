#pragma once

// The Samples drawer's sound thread (ADR 0024): one per processor ("Wubansia sounds"), started by the first audition (),
// never by a constructor, so the hosts' and validators' scans start nothing. It makes the clips the processor's
// engine::AuditionPlayer plays, so the audio thread never reads, checks, decodes or frees a file.
//
// One request slot, latest wins: audition () gives each request a ticket and replaces the request still queued, if
// any. For the latest request the thread, in order:
// 1. reads the file's bytes as installed (ContentSourceFactory::readFile);
// 2. refuses them unless their size and SHA-256 are kit.json's (sounds::matchesListing);
// 3. decodes them (engine::readWav) and checks the frames and channels;
// 4. makes the clip at the player's rate, 48 kHz before the first prepare (engine::makeAuditionClip);
// 5. offers it to the player, unless a newer request or a stop came in meanwhile: then it deletes it.
// A request that fails stops what plays and says why (status ().problem), as a new sound would have replaced it.
//
// While the player holds clips of its (every 50 ms, and whenever woken) the thread deletes the clips the player let go,
// and withdraws an offer that is stopped, replaced by a newer request, or that no block took within 300 ms: a host that
// runs no audio (Logic, for a stopped track that isn't selected or armed) gets status ().hostSilent instead of a stale
// click when it next plays. With no clip of its out, it sleeps until the next request.
//
// stop () stops every ticket so far at once (a playing clip fades out over 5 ms) and drops the queued request. Every
// clip is made here with new and deleted here, once the player hands it back; one the player still holds when it is
// destroyed is the player's to delete (~AuditionPlayer).

#include "Controller.h"
#include "content/ContentSource.h"

#include "audition/audition_clip.h"
#include "audition/audition_player.h"
#include "kit/manifest.h"

#include <juce_core/juce_core.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace wubansia
{
    class SoundService
    {
    public:
        static constexpr auto kDrainInterval = std::chrono::milliseconds (50);
        static constexpr auto kUntakenLimit = std::chrono::milliseconds (300);

        struct Request
        {
            juce::String kit; // for messages: "FUT01"
            int contentVersion = 0; // for messages: "FUT01's content v1"
            ContentStatus found; // the kit's located folder (ready)
            engine::FileEntry file; // as kit.json lists it: path, frames, channels, sha256
        };

        SoundService (ContentSourceFactory& content, engine::AuditionPlayer& player); // starts nothing
        ~SoundService(); // stops after the file in hand; deletes what it holds (the audio thread stopped)

        SoundService (const SoundService&) = delete;
        SoundService& operator= (const SoundService&) = delete;

        // Message thread: queues the request (latest wins) and starts the thread the first time; its ticket.
        std::uint32_t audition (Request request);

        // Message thread: every ticket so far stops; the queued request is dropped.
        void stop();

        // Message thread: a request turned down before it got a ticket (the kit's samples aren't there, say). What plays
        // stops, as the request would have replaced it, and status ().problem says why.
        void refuse (const juce::String& problem);

        AuditionState status() const; // any thread but the audio thread
        bool started() const noexcept { return startedFlag.load (std::memory_order_acquire); }

        // For tests: the clips made here, and the ones deleted here (the rest are in the player's hands).
        int clipsMade() const noexcept { return made.load (std::memory_order_acquire); }
        int clipsDeleted() const noexcept { return deleted.load (std::memory_order_acquire); }

        // For tests: how often the thread gave up the clips in the player's hands because no block came for 300 ms.
        int clipsGivenUp() const noexcept { return 0; } // not yet: the tests come first

    private:
        using Clock = std::chrono::steady_clock;

        struct Job
        {
            std::uint32_t ticket = 0;
            Request request;
        };

        void run();
        void work (const Job& job);
        bool superseded (std::uint32_t ticket) const;
        void fail (const Job& job, const std::string& problem);
        void offer (const Job& job, std::unique_ptr<engine::AuditionClip> clip);
        void checkOffer();
        void drain();
        void back (const engine::AuditionClip* clip); // a clip the player handed back: deleted
        void discard (std::unique_ptr<engine::AuditionClip> clip); // one it never had: deleted
        void stopAll (const juce::String* problem);

        ContentSourceFactory& content;
        engine::AuditionPlayer& player;

        // Guarded by mutex: the request slot, the tickets, what status () reports, and the thread's life.
        mutable std::mutex mutex;
        std::condition_variable wake;
        std::optional<Job> queued;
        std::uint32_t latest = 0; // the last ticket given
        std::uint32_t stoppedUpTo = 0; // tickets up to this one are stopped (or failed)
        std::uint32_t inHand = 0; // the ticket the thread works on; 0 when none
        std::uint32_t offeredTicket = 0; // the ticket of the clip offered last, until the audio thread takes it or it's withdrawn
        Clock::time_point offeredAt {};
        bool hostSilent = false;
        juce::String problem;
        bool checkWanted = false; // stop () asks the thread to withdraw a stopped offer
        bool quit = false;
        std::thread thread;

        std::atomic<bool> startedFlag { false };
        std::atomic<int> made { 0 };
        std::atomic<int> deleted { 0 };

        // The thread's own (and the destructor's, once it has joined): clips in the player's hands.
        int outstanding = 0;
    };
}
