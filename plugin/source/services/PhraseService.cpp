#include "services/PhraseService.h"

#include "content/UserSettings.h"
#include "dragout/DragFiles.h"
#include "editor/EditorModel.h"

#include "kit/channel_map.h"
#include "midi/reader.h"

#include <algorithm>
#include <array>
#include <utility>

namespace wubansia
{
    namespace
    {
        juce::String text (std::string_view s) { return juce::String::fromUTF8 (s.data(), static_cast<int> (s.size())); }

        constexpr std::int64_t kBarTicks = 3840; // 960 PPQ, 4/4

        // The kit's chord line in 960-PPQ ticks: bar 1 of an audition starts on a multiple of it (one bar without a spec).
        std::int64_t chordLine (const KitCatalog::Entry& entry)
        {
            const auto chords = entry.phraseSpec ? static_cast<std::int64_t> (entry.phraseSpec->chords.size()) : 1;
            return std::max<std::int64_t> (1, chords) * kBarTicks;
        }

        const dragout::MidiFile* ownFile (const KitCatalog::Entry& entry, std::string_view name)
        {
            for (const auto& f : entry.midiFiles)
                if (f.name == name)
                    return &f;
            return nullptr;
        }

        bool isOwn (const KitCatalog::Entry& entry, const juce::String& id)
        {
            return dragout::kitPhraseFiles (entry.id, entry.midiFiles).contains (id);
        }

        // What a track plays on, in the drawer's words: the kit's label for a lane's own track (Lead is the lead lane's
        // label: Pluck, Arp, Keys, Mod Blip), else the track's name (an extra: FUT02's Glass Ping).
        juce::String soundOf (const KitCatalog::Entry& entry, std::string_view track)
        {
            for (const auto& t : engine::kTracks)
                if (t.name == track)
                {
                    const auto& label = entry.info.laneLabels[static_cast<std::size_t> (t.lane)];
                    return label.empty() ? text (track) : text (label);
                }
            return text (track);
        }

        std::vector<PhraseNoteView> rollOf (const gen::Phrase& phrase)
        {
            std::vector<PhraseNoteView> roll;
            for (const auto& n : phrase.notes)
            {
                if (n.step < 0 || n.step > 127)
                    continue;
                const auto len = std::clamp<std::int64_t> ((n.length + 60) / 120, 1, 128); // 480 PPQ: a 16th is 120 ticks
                roll.push_back ({ static_cast<std::uint8_t> (n.step), n.key, static_cast<std::uint8_t> (len) });
            }
            return roll;
        }

        // A file's notes as a roll, and the name of the first track that plays one.
        std::vector<PhraseNoteView> rollOf (const midi::File& file, std::string& track)
        {
            std::vector<PhraseNoteView> roll;
            const auto sixteenth = std::max<std::int64_t> (1, file.ppq / 4);
            for (std::size_t t = 1; t < file.tracks.size(); ++t)
            {
                const auto& events = file.tracks[t].events;
                for (std::size_t i = 0; i < events.size(); ++i)
                {
                    if (! events[i].isNoteOn())
                        continue;
                    if (track.empty())
                        track = midi::trackName (file.tracks[t]);
                    std::int64_t end = file.tracks[t].endTick;
                    for (std::size_t j = i + 1; j < events.size(); ++j)
                        if (events[j].isNoteOff() && events[j].data1 == events[i].data1 && events[j].channel() == events[i].channel())
                        {
                            end = events[j].tick;
                            break;
                        }
                    const auto step = events[i].tick / sixteenth;
                    if (step > 127)
                        continue;
                    const auto len = std::clamp<std::int64_t> ((end - events[i].tick + sixteenth / 2) / sixteenth, 1, 128);
                    roll.push_back ({ static_cast<std::uint8_t> (step), events[i].data1, static_cast<std::uint8_t> (len) });
                }
            }
            std::stable_sort (roll.begin(), roll.end(), [] (const PhraseNoteView& a, const PhraseNoteView& b) { return a.step < b.step; });
            return roll;
        }

        // The lanes a file's notes play on, as the overlay resolves them (0 when none does).
        std::uint32_t lanesOf (const KitCatalog::Entry& entry, std::span<const std::uint8_t> smf)
        {
            const auto clip = engine::makePhraseClip (smf, entry.manifest, entry.mix, entry.index, chordLine (entry), 0);
            return clip != nullptr ? clip->lanes : 0u;
        }

        // One of the kit's own files as the drawer lists it: editor::phraseTitle's words without the bar count, which
        // gives its bars. "FUT01_Phrase_OrganRiff_4bars.mid" is Organ Riff, 4 bars, and the lead line
        // "FUT01_Lead_ChromePluck_2bars.mid" Lead Chrome Pluck, 2 bars, as its drag's file name has it.
        PhraseInfo ownPhrase (const KitCatalog::Entry& entry, const juce::String& file)
        {
            PhraseInfo phrase;
            phrase.id = file;
            phrase.kind = "kit";
            phrase.origin = PhraseOrigin::kit;
            const auto dot = editor::middleDot();
            auto title = editor::phraseTitle (file, text (entry.id)); // "Lead · Chrome Pluck · 2 bars"
            const auto last = title.fromLastOccurrenceOf (dot, false, false);
            const auto count = last.upToFirstOccurrenceOf (" ", false, false);
            if (title.contains (dot) && count.isNotEmpty() && count.containsOnly ("0123456789") && (last == count + " bars" || last == count + " bar"))
            {
                phrase.bars = count.getIntValue();
                title = title.upToLastOccurrenceOf (dot, false, false);
            }
            phrase.name = title.replace (dot, " ");
            if (const auto* f = ownFile (entry, file.toStdString()))
            {
                if (const auto parsed = midi::readSmf (f->bytes); parsed.ok())
                {
                    std::string track;
                    phrase.roll = rollOf (parsed.value, track);
                    phrase.sound = soundOf (entry, track);
                }
                phrase.lanes = lanesOf (entry, f->bytes);
            }
            return phrase;
        }

        // A library, kept or More phrase, regenerated from its kind and seed.
        PhraseInfo madePhrase (const KitCatalog::Entry& entry, gen::PhraseKind kind, std::uint64_t seed, const std::string& name,
            const std::vector<std::string>& tags, PhraseOrigin origin, std::uint32_t lanes)
        {
            const auto& spec = *entry.phraseSpec;
            PhraseInfo p;
            p.id = text (gen::phraseId (kind, seed));
            p.name = text (name);
            p.kind = text (gen::phraseKindIds[static_cast<std::size_t> (kind)]);
            p.origin = origin;
            p.bars = gen::phraseBars;
            p.seed = seed;
            for (const auto& t : tags)
                p.tags.add (text (t));
            p.roll = rollOf (gen::generatePhrase (spec, kind, seed));
            p.sound = soundOf (entry, spec.kinds[static_cast<std::size_t> (kind)].track);
            p.lanes = lanes;
            return p;
        }

        std::set<std::string> idSet (const juce::StringArray& ids)
        {
            std::set<std::string> out;
            for (const auto& id : ids)
                out.insert (id.toStdString());
            return out;
        }
    }

    PhraseService::PhraseService (engine::Engine& e, std::function<const KitCatalog&()> c) : engine (e), catalog (std::move (c)) {}

    PhraseService::~PhraseService()
    {
        {
            const std::lock_guard lock (mutex);
            quit = true;
            queued.reset();
        }
        wake.notify_all();
        if (thread.joinable())
            thread.join();

        // The audio thread has stopped (a processor goes, or a test's engine never ran): every clip still out is ours.
        back (engine.offerPhrase (nullptr));
        drain();
        for (const auto* clip : out)
        {
            delete clip;
            deleted.fetch_add (1, std::memory_order_acq_rel);
        }
        out.clear();
    }

    UserSettings& PhraseService::settings() const
    {
        auto* chosen = userSettings.load (std::memory_order_acquire);
        return chosen != nullptr ? *chosen : UserSettings::instance();
    }

    void PhraseService::startLocked()
    {
        if (thread.joinable() || quit)
            return;
        startedFlag.store (true, std::memory_order_release);
        thread = std::thread ([this] { run(); });
    }

    void PhraseService::show (const juce::String& kit)
    {
        {
            const std::lock_guard lock (mutex);
            const auto id = kit.toStdString();
            if (id == wantedKit && (listWanted || (current != nullptr && current->kit == kit)))
                return;
            wantedKit = id;
            listWanted = true;
            startLocked();
        }
        wake.notify_all();
    }

    std::shared_ptr<const PhraseList> PhraseService::list() const
    {
        const std::lock_guard lock (mutex);
        return current;
    }

    bool PhraseService::more (const juce::String& kit, const juce::String& kind)
    {
        if (kind.isNotEmpty() && ! gen::phraseKindFromId (kind.toStdString()))
            return false;
        const auto* entry = catalog().find (kit.toStdString());
        if (entry == nullptr || ! entry->phraseSpec)
            return false;
        {
            const std::lock_guard lock (mutex);
            if (moreBusy || quit)
                return false;
            moreBusy = true;
            moreWanted = std::make_pair (kit.toStdString(), kind.toStdString());
            startLocked();
        }
        wake.notify_all();
        return true;
    }

    bool PhraseService::moreInFlight() const
    {
        const std::lock_guard lock (mutex);
        return moreBusy;
    }

    PhraseService::MoreResult PhraseService::lastMore() const
    {
        const std::lock_guard lock (mutex);
        return moreDone;
    }

    std::uint32_t PhraseService::audition (const juce::String& kit, const juce::String& id)
    {
        // Only what the drawer can list: one of the kit's own files, or a phrase of a kind and seed the kit's spec makes.
        const auto* entry = catalog().find (kit.toStdString());
        if (entry == nullptr || ! entry->ok())
            return 0;
        if (! isOwn (*entry, id) && ! (entry->phraseSpec && gen::parsePhraseId (id.toStdString())))
            return 0;

        std::uint32_t ticket = 0;
        {
            const std::lock_guard lock (mutex);
            if (quit)
                return 0;
            ticket = ++latest; // 4 billion requests before it wraps
            latestId = id.toStdString();
            queued = Job { ticket, kit.toStdString(), id.toStdString() };
            problem.clear();
            hostSilent = false;
            startLocked();
        }
        wake.notify_all();
        return ticket;
    }

    void PhraseService::stop()
    {
        std::uint32_t upTo = 0;
        {
            const std::lock_guard lock (mutex);
            upTo = latest;
            stoppedUpTo = std::max (stoppedUpTo, latest);
            queued.reset();
            checkWanted = thread.joinable(); // the thread withdraws an offer no block has taken yet
        }
        engine.stopPhrasesUpTo (upTo); // a sounding clip fades out from the next block
        wake.notify_all();
    }

    PhraseAuditionState PhraseService::status() const
    {
        PhraseAuditionState s;
        const auto& telemetry = engine.telemetry();
        const auto inHandTicket = telemetry.phraseSounding.load (std::memory_order_relaxed);
        const auto startTicks = telemetry.phraseStartTicks.load (std::memory_order_relaxed);
        const auto ticks = telemetry.phraseTicks.load (std::memory_order_relaxed);
        const bool playing = telemetry.songTicks.load (std::memory_order_relaxed) >= 0.0;
        const auto offered = engine.phraseOffered();

        const std::lock_guard lock (mutex);
        s.latest = latest;
        s.id = text (latestId);
        s.hostSilent = hostSilent;
        s.problem = problem;
        const bool live = latest != 0 && latest > stoppedUpTo && ! hostSilent && problem.isEmpty();
        if (! live)
            return s;
        if (inHandTicket == latest)
        {
            s.startsAtBar = startTicks >= 0 ? static_cast<int> (startTicks / kBarTicks) + 1 : 0;
            if (playing && ticks >= 0)
                s.sounding = latest;
            else
                s.waiting = true; // its chord line is still to come, or the transport is stopped
            return s;
        }
        const bool reading = queued.has_value() || (inHand != 0 && inHand == latest);
        const bool offeredNow = offeredTicket == latest && offered;
        s.waiting = reading || offeredNow;
        return s;
    }

    util::Result<PhraseService::File> PhraseService::file (const juce::String& kit, const juce::String& id) const
    {
        using R = util::Result<File>;
        const auto* entry = catalog().find (kit.toStdString());
        if (entry == nullptr || ! entry->phraseSpec)
            return R::failure ("the kit has no phrase library");
        const auto parsed = gen::parsePhraseId (id.toStdString());
        if (! parsed)
            return R::failure ("'" + id.toStdString() + "' isn't a phrase of the library or More");
        const auto [kind, seed] = *parsed;

        // Only a phrase the drawer lists: a library row, a kept phrase, or one of this session's More.
        std::string name;
        for (const auto& row : entry->phraseRows)
            if (row.kind == kind && row.seed == seed)
                name = row.name;
        if (name.empty())
        {
            bool listed = settings().keptPhrases (kit).contains (id);
            {
                const std::lock_guard lock (mutex);
                if (const auto it = sessionMore.find (entry->id); it != sessionMore.end())
                    listed = listed || std::find (it->second.begin(), it->second.end(), std::make_pair (kind, seed)) != it->second.end();
            }
            if (! listed)
                return R::failure ("'" + id.toStdString() + "' isn't listed for " + entry->id);
            name = gen::phraseName (*entry->phraseSpec, kind, seed);
        }
        auto smf = gen::phraseSmf (*entry->phraseSpec, gen::generatePhrase (*entry->phraseSpec, kind, seed), name);
        if (! smf.ok())
            return R::failure (std::move (smf.error));
        return R::success ({ std::move (smf.value), name, std::string (gen::phraseKindFileWord (kind)) });
    }

    void PhraseService::setKept (const juce::String& kit, const juce::String& id, bool kept)
    {
        settings().setPhraseKept (kit, id, kept); // the person's file, at once, as the other settings
        {
            const std::lock_guard lock (mutex);
            if (wantedKit == kit.toStdString())
                listWanted = true;
            startLocked();
        }
        wake.notify_all();
    }

    void PhraseService::markSeen (const juce::String& kit, const juce::String& id)
    {
        {
            const std::lock_guard lock (mutex);
            const auto it = freshIds.find (kit.toStdString());
            if (it == freshIds.end() || it->second.erase (id.toStdString()) == 0)
                return;
            if (wantedKit == kit.toStdString())
                listWanted = true;
        }
        wake.notify_all();
    }

    //==================================================================================================================
    void PhraseService::run()
    {
        juce::Thread::setCurrentThreadName ("Wubansia phrases");
        std::unique_lock lock (mutex);
        while (! quit)
        {
            const auto ready = [this] { return quit || queued.has_value() || checkWanted || listWanted || moreWanted.has_value(); };
            if (! ready())
            {
                if (! out.empty())
                {
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
            auto more = std::move (moreWanted);
            moreWanted.reset();
            const bool rebuild = listWanted;
            const auto kit = wantedKit;
            listWanted = false;
            checkWanted = false;
            lock.unlock();

            if (wakeProbe != nullptr)
                wakeProbe();
            drain();
            checkHeld();
            checkOffer();
            if (job)
                work (*job);
            if (more)
                makeMore (more->first, more->second);
            if (rebuild || (more && more->first == kit))
                publish (build (kit));

            lock.lock();
            inHand = 0;
            if (more)
                moreBusy = false;
        }
    }

    std::shared_ptr<PhraseList> PhraseService::build (const std::string& kit)
    {
        auto list = std::make_shared<PhraseList>();
        list->kit = text (kit);
        const auto* entry = catalog().find (kit);
        if (entry == nullptr || ! entry->ok())
            return list;

        const auto kept = idSet (settings().keptPhrases (list->kit));
        std::set<std::string> listed;
        std::vector<std::pair<gen::PhraseKind, std::uint64_t>> session;
        std::set<std::string> fresh;
        {
            const std::lock_guard lock (mutex);
            if (const auto it = sessionMore.find (kit); it != sessionMore.end())
                session = it->second;
            if (const auto it = freshIds.find (kit); it != freshIds.end())
                fresh = it->second;
        }

        // Each kind's lanes, from one of its files: all of a kind's phrases share its track.
        std::array<std::optional<std::uint32_t>, gen::phraseKindCount> kindLanes;
        const auto lanesFor = [&] (gen::PhraseKind kind, std::uint64_t seed) {
            auto& lanes = kindLanes[static_cast<std::size_t> (kind)];
            if (! lanes)
            {
                const auto& spec = *entry->phraseSpec;
                const auto smf = gen::phraseSmf (spec, gen::generatePhrase (spec, kind, seed), "lanes");
                lanes = smf.ok() ? lanesOf (*entry, smf.value) : 0u;
            }
            return *lanes;
        };
        const auto add = [&] (PhraseInfo p) {
            const auto id = p.id.toStdString();
            p.kept = kept.count (id) > 0;
            p.fresh = fresh.count (id) > 0;
            listed.insert (id);
            list->phrases.push_back (std::move (p));
        };

        // The library, row by row.
        if (entry->phraseSpec)
            for (const auto& row : entry->phraseRows)
                add (madePhrase (*entry, row.kind, row.seed, row.name, row.tags, PhraseOrigin::library, lanesFor (row.kind, row.seed)));

        // The kit's own phrase files, then its lead line.
        for (const auto& file : dragout::kitPhraseFiles (entry->id, entry->midiFiles))
            add (ownPhrase (*entry, file));

        if (! entry->phraseSpec)
            return list;
        const auto& spec = *entry->phraseSpec;

        // The kept phrases the library doesn't hold (a More phrase of an earlier session, or a library phrase cut since):
        // regenerated from their kind and seed. Then this session's More.
        const auto addMade = [&] (gen::PhraseKind kind, std::uint64_t seed) {
            if (listed.count (gen::phraseId (kind, seed)) > 0)
                return;
            const auto phrase = gen::generatePhrase (spec, kind, seed);
            add (madePhrase (*entry, kind, seed, gen::phraseName (spec, kind, seed), gen::phraseTags (spec, phrase),
                seed >= gen::firstMoreSeed ? PhraseOrigin::more : PhraseOrigin::library, lanesFor (kind, seed)));
        };
        for (const auto& id : settings().keptPhrases (list->kit))
            if (const auto parsed = gen::parsePhraseId (id.toStdString()))
                addMade (parsed->first, parsed->second);
        for (const auto& [kind, seed] : session)
            addMade (kind, seed);
        return list;
    }

    void PhraseService::publish (std::shared_ptr<PhraseList> next)
    {
        {
            const std::lock_guard lock (mutex);
            current = std::move (next);
        }
        listGeneration.fetch_add (1, std::memory_order_acq_rel);
    }

    void PhraseService::makeMore (const std::string& kit, const std::string& kind)
    {
        MoreResult result;
        result.kind = text (kind);
        const auto* entry = catalog().find (kit);
        if (entry != nullptr && entry->phraseSpec)
        {
            const auto& spec = *entry->phraseSpec;

            // Unlike everything the list holds: its library, kept and More phrases.
            std::vector<gen::Phrase> against;
            for (const auto& row : entry->phraseRows)
                against.push_back (gen::generatePhrase (spec, row.kind, row.seed));
            std::vector<std::pair<gen::PhraseKind, std::uint64_t>> earlier;
            {
                const std::lock_guard lock (mutex);
                if (const auto it = sessionMore.find (kit); it != sessionMore.end())
                    earlier = it->second;
            }
            for (const auto& id : settings().keptPhrases (text (kit)))
                if (const auto parsed = gen::parsePhraseId (id.toStdString()))
                    earlier.push_back (*parsed);
            for (const auto& [k, seed] : earlier)
                against.push_back (gen::generatePhrase (spec, k, seed));

            std::vector<gen::PhraseKind> kinds;
            if (kind.empty())
                for (std::size_t k = 0; k < gen::phraseKindCount; ++k)
                    kinds.push_back (static_cast<gen::PhraseKind> (k));
            else
                kinds.push_back (*gen::phraseKindFromId (kind));
            const auto count = kind.empty() ? 1 : kMore; // one of each kind under All

            const auto from = std::max (settings().phrasesNextSeed (text (kit)), gen::firstMoreSeed);
            auto next = from;
            std::vector<std::pair<gen::PhraseKind, std::uint64_t>> pressed;
            for (const auto k : kinds)
            {
                const auto press = gen::morePhrases (spec, k, from, against, count, kMaxCandidates);
                for (const auto& phrase : press.phrases)
                {
                    pressed.emplace_back (k, phrase.seed);
                    against.push_back (phrase);
                }
                next = std::max (next, press.nextSeed);
            }
            settings().setPhrasesNextSeed (text (kit), next);
            {
                const std::lock_guard lock (mutex);
                auto& session = sessionMore[kit];
                auto& fresh = freshIds[kit];
                for (const auto& [k, seed] : pressed)
                {
                    session.emplace_back (k, seed);
                    fresh.insert (gen::phraseId (k, seed));
                }
            }
            result.made = static_cast<int> (pressed.size());
        }
        const std::lock_guard lock (mutex);
        result.press = moreDone.press + 1;
        moreDone = result;
    }

    //==================================================================================================================
    void PhraseService::work (const Job& job)
    {
        const auto superseded = [this, &job] {
            const std::lock_guard lock (mutex);
            return quit || job.ticket != latest || job.ticket <= stoppedUpTo;
        };
        const auto fail = [this, &job] (const juce::String& why) {
            {
                const std::lock_guard lock (mutex);
                if (quit || job.ticket != latest || job.ticket <= stoppedUpTo)
                    return; // replaced or stopped meanwhile
                problem = why;
                stoppedUpTo = job.ticket;
            }
            engine.stopPhrasesUpTo (job.ticket); // what played before stops, as this phrase would have replaced it
            checkOffer();
        };

        const auto* entry = catalog().find (job.kit);
        if (entry == nullptr || ! entry->ok())
        {
            fail ("the kit isn't in this build");
            return;
        }
        std::vector<std::uint8_t> bytes;
        if (const auto* own = isOwn (*entry, text (job.id)) ? ownFile (*entry, job.id) : nullptr)
            bytes.assign (own->bytes.begin(), own->bytes.end());
        else if (const auto parsed = gen::parsePhraseId (job.id); parsed && entry->phraseSpec)
        {
            const auto& spec = *entry->phraseSpec;
            auto smf = gen::phraseSmf (spec, gen::generatePhrase (spec, parsed->first, parsed->second), gen::phraseName (spec, parsed->first, parsed->second));
            if (smf.ok())
                bytes = std::move (smf.value);
        }
        if (superseded())
            return;
        auto clip = bytes.empty() ? nullptr : engine::makePhraseClip (bytes, entry->manifest, entry->mix, entry->index, chordLine (*entry), job.ticket);
        if (clip == nullptr)
        {
            fail ("it has no note this kit plays");
            return;
        }
        made.fetch_add (1, std::memory_order_acq_rel);
        offer (job.ticket, std::move (clip));
    }

    void PhraseService::offer (std::uint32_t ticket, std::unique_ptr<engine::PhraseClip> clip)
    {
        bool wanted = false;
        {
            const std::lock_guard lock (mutex);
            wanted = ! quit && ticket == latest && ticket > stoppedUpTo;
            if (wanted)
            {
                offeredTicket = ticket;
                offeredAt = Clock::now();
            }
        }
        const auto* raw = clip.release();
        out.insert (raw);
        if (! wanted)
        {
            back (raw);
            return;
        }
        back (engine.offerPhrase (raw)); // the earlier offer, if no block took it
    }

    void PhraseService::checkOffer()
    {
        std::uint32_t ticket = 0;
        bool unwanted = false, stale = false;
        {
            const std::lock_guard lock (mutex);
            ticket = offeredTicket;
            if (ticket == 0)
                return;
            unwanted = quit || ticket != latest || ticket <= stoppedUpTo;
            stale = Clock::now() - offeredAt >= kUntakenLimit;
        }
        if (! engine.phraseOffered())
        {
            const std::lock_guard lock (mutex);
            if (offeredTicket == ticket)
                offeredTicket = 0;
            return;
        }
        if (! unwanted && ! stale)
            return;

        const auto* withdrawn = engine.offerPhrase (nullptr);
        bool silent = false;
        {
            const std::lock_guard lock (mutex);
            if (offeredTicket == ticket)
                offeredTicket = 0;
            if (withdrawn != nullptr && ! unwanted && ticket == latest && ticket > stoppedUpTo)
            {
                hostSilent = true; // the host runs no audio: the drawer says so instead of a stale phrase later
                silent = true;
            }
        }
        back (withdrawn);
        if (silent && onGiveUp != nullptr)
            onGiveUp();
    }

    void PhraseService::checkHeld()
    {
        const bool offerWaits = engine.phraseOffered();
        const auto playing = engine.telemetry().phraseSounding.load (std::memory_order_relaxed);
        const auto blocks = engine.blocksRun();
        const auto now = Clock::now();
        const auto held = static_cast<int> (out.size()) - (offerWaits ? 1 : 0); // clips a block took, not yet handed back
        if (blocks != blocksSeen || held <= 0)
        {
            blocksSeen = blocks;
            blocksSeenAt = now;
            return;
        }
        if (blocks == givenUpAt || now - blocksSeenAt < kUntakenLimit)
            return;

        // No block for 300 ms with a clip in hand: the host stopped running audio. The next block lets it go unplayed.
        givenUpAt = blocks;
        engine.stopPhrasesUpTo (playing);
        engine.dropPhrasesIfIdleSince (blocks);
        bool silent = false;
        {
            const std::lock_guard lock (mutex);
            if (playing != 0 && playing == latest && playing > stoppedUpTo)
            {
                hostSilent = true; // the drawer says why the phrase stopped
                silent = true;
            }
            stoppedUpTo = std::max (stoppedUpTo, playing);
        }
        givenUp.fetch_add (1, std::memory_order_acq_rel);
        if (silent && onGiveUp != nullptr)
            onGiveUp();
    }

    void PhraseService::drain()
    {
        engine.collectRetiredPhrases ([this] (const engine::PhraseClip* clip) { back (clip); });
    }

    void PhraseService::back (const engine::PhraseClip* clip)
    {
        if (clip == nullptr || out.erase (clip) == 0)
            return;
        delete clip;
        deleted.fetch_add (1, std::memory_order_acq_rel);
    }
}
