#include "services/SoundService.h"

#include "sounds/SampleExport.h"

#include "kit/kit.h"
#include "kit/sample_source.h"
#include "kit/wav_reader.h"

#include <algorithm>
#include <utility>
#include <vector>

namespace wubansia
{
    namespace
    {
        juce::String text (const std::string& s) { return juce::String::fromUTF8 (s.data(), static_cast<int> (s.size())); }

        // A kit.json path's file name: "FX/Shift Riser.wav" -> "Shift Riser.wav".
        std::string fileNameOf (const std::string& path)
        {
            const auto slash = path.find_last_of ('/');
            return slash == std::string::npos ? path : path.substr (slash + 1);
        }
    }

    SoundService::SoundService (ContentSourceFactory& c, engine::AuditionPlayer& p) : content (c), player (p) {}

    SoundService::~SoundService()
    {
        {
            const std::lock_guard lock (mutex);
            quit = true;
            queued.reset();
        }
        wake.notify_all();
        if (thread.joinable())
            thread.join();

        // The audio has stopped (the processor is going, or never played): an offer no block took and the clips the
        // player let go are this service's to delete. A clip the player still plays is the player's (~AuditionPlayer).
        back (player.offer (nullptr));
        drain();
    }

    std::uint32_t SoundService::audition (Request request)
    {
        std::uint32_t ticket = 0;
        {
            const std::lock_guard lock (mutex);
            if (quit)
                return 0;
            ticket = ++latest; // 4 billion requests before it wraps
            queued = Job { ticket, std::move (request) };
            failure = {};
            hostSilent = false;
            if (! thread.joinable())
            {
                startedFlag.store (true, std::memory_order_release);
                thread = std::thread ([this] { run(); });
            }
        }
        wake.notify_all();
        return ticket;
    }

    void SoundService::stop()
    {
        stopAll (nullptr);
    }

    void SoundService::refuse (const juce::String& problem)
    {
        Failure turnedDown; // its own words only: no detail, and not the kit's file's fault
        turnedDown.problem = problem;
        stopAll (&turnedDown);
    }

    void SoundService::stopAll (const Failure* why)
    {
        std::uint32_t upTo = 0;
        {
            const std::lock_guard lock (mutex);
            upTo = latest;
            stoppedUpTo = std::max (stoppedUpTo, latest);
            queued.reset();
            if (why != nullptr)
            {
                failure = *why;
                hostSilent = false;
            }
            checkWanted = thread.joinable(); // the thread withdraws an offer no block has taken yet
        }
        player.stopUpTo (upTo); // a playing clip fades out from the next block
        wake.notify_all();
    }

    AuditionState SoundService::status() const
    {
        AuditionState s;
        const std::lock_guard lock (mutex);
        s.latest = latest;
        s.hostSilent = hostSilent;
        s.problem = failure.problem;
        s.detail = failure.detail;
        s.sourceProblem = failure.sourceProblem;
        s.reinstall = failure.reinstall;
        const auto playing = player.sounding();
        s.sounding = playing > stoppedUpTo ? playing : 0; // a stopped ticket only fades out
        const bool live = latest != 0 && latest > stoppedUpTo && ! hostSilent;
        const bool reading = queued.has_value() || (inHand != 0 && inHand == latest);
        const bool offeredNow = offeredTicket != 0 && offeredTicket == latest && player.offered();
        s.waiting = live && (reading || offeredNow);
        return s;
    }

    void SoundService::run()
    {
        juce::Thread::setCurrentThreadName ("Wubansia sounds");
        std::unique_lock lock (mutex);
        while (! quit)
        {
            if (! queued && ! checkWanted)
            {
                const auto ready = [this] { return quit || queued.has_value() || checkWanted; };
                if (outstanding > 0)
                {
                    // The player holds clips of this service's: back every 50 ms to collect them, and when an offer's
                    // 300 ms are up.
                    auto until = Clock::now() + kDrainInterval;
                    if (offeredTicket != 0)
                        until = std::min (until, offeredAt + kUntakenLimit);
                    wake.wait_until (lock, until, ready);
                }
                else
                {
                    wake.wait (lock, ready); // nothing out: asleep until the next request
                }
            }
            if (quit)
                break;

            std::optional<Job> job;
            if (queued)
            {
                job = std::move (queued);
                queued.reset();
                inHand = job->ticket;
            }
            checkWanted = false;
            lock.unlock();

            if (wakeProbe != nullptr)
                wakeProbe();
            drain();
            // The give-up before the offer: a late wake can find both 300 ms limits passed, and the give-up must still
            // see a newer clip's offer waiting, as it does when they fall in separate wakes, so that a block coming
            // during the give-up takes that clip. Either order leaves the same state when no block comes.
            checkHeld();
            checkOffer();
            if (job)
                work (*job);

            lock.lock();
            inHand = 0;
        }
    }

    bool SoundService::superseded (std::uint32_t ticket) const
    {
        const std::lock_guard lock (mutex);
        return quit || ticket != latest || ticket <= stoppedUpTo;
    }

    void SoundService::work (const Job& job)
    {
        const auto& file = job.request.file;
        engine::KitInfo kit; // for the words: the kit and its content version
        kit.id = job.request.kit.toStdString();
        kit.contentVersion = job.request.contentVersion;

        // The kit's own file is why: said as a copy of it is refused (SampleExport.h), the reader's words the detail.
        const auto refused = [this, &job] (const sounds::ExportResult& why)
        { fail (job, { text (why.problem), text (why.detail), why.sourceProblem, why.reinstall }); };

        // 1. The bytes, as installed.
        auto bytes = content.readFile (job.request.found, file.path);
        if (superseded (job.ticket))
            return;
        if (! bytes.ok())
        {
            refused (sounds::unreadableSource (kit, file, bytes.error));
            return;
        }

        // 2. Exactly the file kit.json lists: its size, then its SHA-256.
        if (! sounds::matchesListing (bytes.value, file))
        {
            refused (sounds::mismatchedSource (kit, file));
            return;
        }

        // 3. Decoded, with the listed frames and channels.
        auto decoded = engine::readWav (bytes.value);
        std::vector<std::uint8_t>().swap (bytes.value);
        if (! decoded.ok())
        {
            refused (sounds::unreadableSource (kit, file, decoded.error));
            return;
        }
        if (decoded.value.frames != file.frames || decoded.value.channels != file.channels)
        {
            refused (sounds::mismatchedSource (kit, file));
            return;
        }
        if (superseded (job.ticket))
            return;

        // 4. The clip, for the rate the player plays at (48 kHz before the first prepare). The file is fine.
        const double prepared = player.rate();
        const double rate = prepared > 0.0 ? prepared : static_cast<double> (engine::kContentSampleRate);
        auto clip = engine::makeAuditionClip (decoded.value, rate, job.ticket);
        if (clip == nullptr)
        {
            Failure unplayable;
            unplayable.problem = text (fileNameOf (file.path) + " can't be played at " + std::to_string (static_cast<long long> (rate)) + " Hz");
            fail (job, unplayable);
            return;
        }
        made.fetch_add (1, std::memory_order_acq_rel);

        // 5. Offered, unless a newer request or a stop came in meanwhile.
        offer (job, std::move (clip));
    }

    void SoundService::fail (const Job& job, const Failure& why)
    {
        {
            const std::lock_guard lock (mutex);
            if (quit || job.ticket != latest || job.ticket <= stoppedUpTo)
                return; // replaced or stopped meanwhile: the newer request reports
            failure = why;
            stoppedUpTo = job.ticket;
        }
        player.stopUpTo (job.ticket); // what played before stops, as this sound would have replaced it
        checkOffer(); // and an earlier offer still waiting goes
    }

    void SoundService::offer (const Job& job, std::unique_ptr<engine::AuditionClip> clip)
    {
        bool wanted = false;
        {
            const std::lock_guard lock (mutex);
            wanted = ! quit && job.ticket == latest && job.ticket > stoppedUpTo;
            if (wanted)
            {
                offeredTicket = job.ticket;
                offeredAt = Clock::now();
            }
        }
        if (! wanted)
        {
            discard (std::move (clip));
            return;
        }
        ++outstanding;
        back (player.offer (clip.release())); // the earlier offer, if no block took it
    }

    void SoundService::checkOffer()
    {
        std::uint32_t ticket = 0;
        bool unwanted = false;
        bool stale = false;
        {
            const std::lock_guard lock (mutex);
            ticket = offeredTicket;
            if (ticket == 0)
                return;
            unwanted = quit || ticket != latest || ticket <= stoppedUpTo;
            stale = Clock::now() - offeredAt >= kUntakenLimit;
        }
        if (! player.offered())
        {
            // A block took it (and plays it, or let it go): the player hands it back when it's done.
            const std::lock_guard lock (mutex);
            if (offeredTicket == ticket)
                offeredTicket = 0;
            return;
        }
        if (! unwanted && ! stale)
            return;

        // Stopped, replaced, or no block came for it in 300 ms: withdrawn. (nullptr: a block took it just now.)
        const auto* withdrawn = player.offer (nullptr);
        {
            const std::lock_guard lock (mutex);
            if (offeredTicket == ticket)
                offeredTicket = 0;
            if (withdrawn != nullptr && ! unwanted && ticket == latest && ticket > stoppedUpTo)
                hostSilent = true; // the host runs no audio: the drawer says so instead of a stale click later
        }
        back (withdrawn);
    }

    void SoundService::checkHeld()
    {
        // The offer, then the playing ticket, then the blocks: a block that takes the offer has counted itself before it
        // does. A block that runs between these reads moves the count, so nothing is given up; one that runs after them
        // (before the drop below) meets only a stop for the ticket read here, older than any clip that block took, which
        // fades out as a stop does. A newer clip it took plays on, and no hint shows.
        const bool offerWaits = player.offered();
        const auto playing = player.sounding();
        const auto blocks = player.blocksRun();
        const auto now = Clock::now();
        const int held = outstanding - (offerWaits ? 1 : 0); // clips a block took, still in the player's hands
        if (blocks != blocksSeen || held <= 0)
        {
            blocksSeen = blocks;
            blocksSeenAt = now;
            return;
        }
        if (blocks == givenUpAt || now - blocksSeenAt < kUntakenLimit)
            return;

        // No block for 300 ms with clips in hand: the host stopped running blocks mid-clip. They never play again: the
        // one playing is stopped (if a block comes before the drop, it fades out as a stop does), and the next block
        // lets them all go unheard. The player is told first, so status () never reads stopped before it knows.
        givenUpAt = blocks;
        if (giveUpProbe != nullptr)
            giveUpProbe();
        player.stopUpTo (playing);
        player.dropIfIdleSince (blocks);
        {
            const std::lock_guard lock (mutex);
            if (playing != 0 && playing == latest && playing > stoppedUpTo)
                hostSilent = true; // the drawer says why the sound stopped
            stoppedUpTo = std::max (stoppedUpTo, playing);
        }
        givenUp.fetch_add (1, std::memory_order_acq_rel);
    }

    void SoundService::drain()
    {
        player.collectRetired ([this] (const engine::AuditionClip* clip) { back (clip); });
    }

    void SoundService::back (const engine::AuditionClip* clip)
    {
        if (clip == nullptr)
            return;
        --outstanding;
        delete clip;
        deleted.fetch_add (1, std::memory_order_acq_rel);
    }

    void SoundService::discard (std::unique_ptr<engine::AuditionClip> clip)
    {
        if (clip == nullptr)
            return;
        clip.reset();
        deleted.fetch_add (1, std::memory_order_acq_rel);
    }
}
