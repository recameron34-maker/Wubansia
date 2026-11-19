#pragma once

// The Phrases drawer's thread (ADR 0031): one per processor ("Wubansia phrases"), started by the first show (),
// more () or audition (), never by a constructor, so the hosts' and validators' scans start nothing. It owns the
// drawer's lists, More, the audition clips and Keep's writes.
//
// - A list (show ()) is the kit's 64 library rows (KitCatalog::Entry's embedded library, each regenerated for its
//   roll), then the kit's own phrase files and its lead line (dragout::kitPhraseFiles, read with midi::readSmf), then
//   the kept phrases the library doesn't hold (a More phrase, or a library phrase cut later: regenerated from its kind
//   and seed), then this session's More phrases. Each row's sound and lanes come from its track through the kit's
//   manifest, as the overlay resolves them (engine::makePhraseClip). Published by shared_ptr swap; generation ()
//   bumps with every new list.
// - More (more ()) starts at phrases_next_<kit> (settings.xml; 1,000,000 when absent) and makes kMore phrases of a
//   kind (one of each kind for ""), each passing every check and no near-duplicate of the list, at most
//   kMaxCandidates seeds a kind, then saves the next seed. They stay until the processor goes; a star keeps one.
// - An audition (audition ()) gets a ticket, latest wins; the thread builds the phrase's file (gen::phraseSmf, or the
//   kit's embedded bytes), makes the clip for the kit's manifest, mix and index with its chord line (the spec's
//   chords x 3,840 ticks; one bar without a spec) and offers it to the engine (engine::Engine::offerPhrase).
// - While clips are out it wakes every kDrainInterval: it deletes the clips the engine hands back, withdraws an offer
//   that is stopped, replaced, or that no block took within kUntakenLimit (status ().hostSilent: the host runs no
//   audio), and gives up a clip a block took once no block has come for kUntakenLimit (engine's
//   dropPhrasesIfIdleSince: blocks that come back play nothing stale), as the Samples drawer's SoundService does.
//   Either way onGiveUp runs, so the Preview the audition started goes off.
//
// The service makes every clip with new and deletes every one: those the engine hands back as they come, and at its
// destruction every one still out (the Engine never deletes a clip and never touches one once the audio thread has
// stopped, which it has when a processor goes).

#include "Controller.h"
#include "services/KitCatalog.h"

#include "audition/phrase_clip.h"
#include "engine.h"
#include "gen/phrase.h"
#include "util/result.h"

#include <juce_core/juce_core.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace wubansia
{
    class UserSettings;

    // One kit's list, as the drawer shows it.
    struct PhraseList
    {
        juce::String kit; // "FUT01"
        std::vector<PhraseInfo> phrases;
    };

    class PhraseService
    {
    public:
        static constexpr int kMore = 8;
        static constexpr int kMaxCandidates = 1000; // per More press and kind
        static constexpr auto kDrainInterval = std::chrono::milliseconds (50);
        static constexpr auto kUntakenLimit = std::chrono::milliseconds (300);

        // catalog is called on the phrase thread (KitService::catalog () parses once, from any thread). Starts nothing.
        PhraseService (engine::Engine& engine, std::function<const KitCatalog&()> catalog);
        ~PhraseService(); // joins; deletes every clip it made (the audio thread stopped)

        PhraseService (const PhraseService&) = delete;
        PhraseService& operator= (const PhraseService&) = delete;

        // The settings file More and Keep use: UserSettings::instance () unless set. Before the thread starts, or from the
        // message thread while it runs (an atomic pointer).
        void setUserSettings (UserSettings* settings) noexcept { userSettings.store (settings, std::memory_order_release); }

        // Message thread: (re)builds kit's list on the thread, unless it is the list already shown or being built.
        void show (const juce::String& kit);
        // Any thread but the audio thread: the last list made (nullptr before the first).
        std::shared_ptr<const PhraseList> list() const;
        std::uint64_t generation() const noexcept { return listGeneration.load (std::memory_order_acquire); }

        // Message thread: kMore new phrases of kind ("" one of each kind) for kit, made on the thread and added to its
        // list; false while a press is in flight, or for an unknown kind or a kit without a spec.
        bool more (const juce::String& kit, const juce::String& kind);
        bool moreInFlight() const;
        // What the last press gave, for the status line: the kind ("" for all), how many it made.
        struct MoreResult
        {
            std::uint64_t press = 0; // counts presses done
            juce::String kind;
            int made = 0;
        };
        MoreResult lastMore() const;

        // Message thread: the phrase id of kit plays from the next chord line (latest wins); its ticket, 0 when the id
        // isn't in kit's list.
        std::uint32_t audition (const juce::String& kit, const juce::String& id);
        // Message thread: every ticket so far stops (a sounding phrase fades out over 10 ms).
        void stop();
        PhraseAuditionState status() const; // any thread but the audio thread; startedTransport is the Controller's
        // Runs on the phrase thread when it gives up an audition because the host ran no block for 300 ms. Before the
        // thread starts.
        void setOnGiveUp (std::function<void()> f) { onGiveUp = std::move (f); }

        // Message thread, for a drag: the phrase's file (gen::phraseSmf, or the kit's embedded bytes), its name and its
        // kind's file word ("Hook"); the kit's own files are dragged as DragWhat::phrase, not through this.
        struct File
        {
            std::vector<std::uint8_t> bytes;
            std::string name; // "Night Bus"
            std::string kindWord; // "Hook"
        };
        util::Result<File> file (const juce::String& kit, const juce::String& id) const;

        // Message thread: stars id (or takes the star off) in settings.xml and lists it so.
        void setKept (const juce::String& kit, const juce::String& id, bool kept);
        // Message thread: id was played or dragged; its New badge goes.
        void markSeen (const juce::String& kit, const juce::String& id);

        bool started() const noexcept { return startedFlag.load (std::memory_order_acquire); }

        // For tests.
        int clipsMade() const noexcept { return made.load (std::memory_order_acquire); }
        int clipsDeleted() const noexcept { return deleted.load (std::memory_order_acquire); }
        int clipsGivenUp() const noexcept { return givenUp.load (std::memory_order_acquire); }
        void setWakeProbeForTesting (std::function<void()> probe) { wakeProbe = std::move (probe); } // before the thread starts

    private:
        using Clock = std::chrono::steady_clock;

        struct Job
        {
            std::uint32_t ticket = 0;
            std::string kit, id;
        };

        void startLocked();
        void run();
        UserSettings& settings() const;
        std::shared_ptr<PhraseList> build (const std::string& kit);
        void publish (std::shared_ptr<PhraseList> next);
        void makeMore (const std::string& kit, const std::string& kind);
        void work (const Job& job);
        void offer (std::uint32_t ticket, std::unique_ptr<engine::PhraseClip> clip);
        void checkOffer();
        void checkHeld();
        void drain();
        void back (const engine::PhraseClip* clip);

        engine::Engine& engine;
        std::function<const KitCatalog&()> catalog;
        std::atomic<UserSettings*> userSettings { nullptr };
        std::function<void()> onGiveUp;
        std::function<void()> wakeProbe;

        // Guarded by mutex.
        mutable std::mutex mutex;
        std::condition_variable wake;
        std::shared_ptr<const PhraseList> current;
        std::string wantedKit; // the list show () asked for last
        bool listWanted = false;
        std::optional<std::pair<std::string, std::string>> moreWanted; // kit, kind
        bool moreBusy = false;
        MoreResult moreDone;
        std::map<std::string, std::vector<std::pair<gen::PhraseKind, std::uint64_t>>> sessionMore; // kit: this session's More phrases
        std::map<std::string, std::set<std::string>> freshIds; // kit: More phrases not yet played or dragged
        std::optional<Job> queued;
        std::uint32_t latest = 0;
        std::uint32_t stoppedUpTo = 0;
        std::uint32_t inHand = 0;
        std::string latestId;
        juce::String problem;
        std::uint32_t offeredTicket = 0;
        Clock::time_point offeredAt {};
        bool hostSilent = false;
        bool checkWanted = false;
        bool quit = false;
        std::thread thread;

        std::atomic<std::uint64_t> listGeneration { 0 };
        std::atomic<bool> startedFlag { false };
        std::atomic<int> made { 0 };
        std::atomic<int> deleted { 0 };
        std::atomic<int> givenUp { 0 };

        // The thread's own (and the destructor's once it has joined): every clip made and not yet deleted, the engine's
        // block count when last seen to move, and when, and the count at which the clips in hand were last given up.
        std::set<const engine::PhraseClip*> out;
        std::uint64_t blocksSeen = 0;
        Clock::time_point blocksSeenAt {};
        std::uint64_t givenUpAt = ~std::uint64_t { 0 };
    };
}
