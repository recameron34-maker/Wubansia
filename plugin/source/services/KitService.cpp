#include "services/KitService.h"

#include "content/ContentLocator.h"

#include "kit/kit_builder.h"
#include "kit/kit_cache.h"

#include <juce_core/juce_core.h>

#include <algorithm>
#include <utility>

namespace wubansia
{
    namespace
    {
        juce::String text (std::string_view s) { return juce::String::fromUTF8 (s.data(), static_cast<int> (s.size())); }

        // Before the service has looked at a kit's folders.
        ContentStatus unchecked()
        {
            ContentStatus status;
            status.state = ContentState::loading;
            return status;
        }

        ContentStatus failedWith (juce::String problem)
        {
            ContentStatus status;
            status.state = ContentState::failed;
            status.problem = std::move (problem);
            return status;
        }
    }

    KitService::KitService (engine::Engine& e, ContentSourceFactory& c) : engine (e), content (c) {}

    KitService::~KitService()
    {
        {
            const std::lock_guard lock (mutex);
            quit = true;
            cancel.store (true, std::memory_order_release);
        }
        wake.notify_all();
        if (thread.joinable())
            thread.join();

        // The engine may still point at a Kit held here, but nothing plays it again: the processor
        // destroys the service and then the engine, and never processes in between.
        kits.clear();
        held.store (0, std::memory_order_release);
    }

    const KitCatalog& KitService::catalog() const
    {
        std::call_once (catalogOnce, [this] { catalogPtr = std::make_unique<KitCatalog> (content.kits()); });
        return *catalogPtr;
    }

    void KitService::request (std::string_view kitId, int contentVersion)
    {
        const std::lock_guard lock (mutex);
        if (quit)
            return;
        const auto g = generation.load (std::memory_order_relaxed) + 1;
        pending = Request { std::string (kitId), contentVersion, g };
        generation.store (g, std::memory_order_release);
        inFlight.store (true, std::memory_order_release);
        cancel.store (true, std::memory_order_release); // the load in progress, if any, gives way

        current.kit = text (kitId);
        current.generation = g;
        current.content = unchecked();
        lastKit = std::string (kitId);
        lastVersion = contentVersion;

        if (! thread.joinable())
        {
            startedFlag.store (true, std::memory_order_release);
            thread = std::thread ([this] { run(); });
        }
        wake.notify_all();
    }

    KitService::LoadStatus KitService::status() const
    {
        const std::lock_guard lock (mutex);
        return current;
    }

    ContentStatus KitService::kitStatus (int index) const
    {
        const std::lock_guard lock (mutex);
        if (index >= 0 && static_cast<std::size_t> (index) < statuses.size())
            return statuses[static_cast<std::size_t> (index)];
        return unchecked();
    }

    void KitService::contentChanged()
    {
        std::string kit;
        int version = 0;
        {
            const std::lock_guard lock (mutex);
            if (lastKit.empty() || quit)
                return;
            kit = lastKit;
            version = lastVersion;
        }
        request (kit, version);
    }

    void KitService::run()
    {
        juce::Thread::setCurrentThreadName ("Wubansia kits");
        const auto& kitList = catalog();
        {
            const std::lock_guard lock (mutex);
            if (statuses.empty())
                statuses.assign (kitList.entries().size(), unchecked());
        }

        std::unique_lock lock (mutex);
        while (! quit)
        {
            if (! pending)
                wake.wait_for (lock, kDrainInterval, [this] { return quit || pending.has_value(); });
            if (quit)
                break;

            std::optional<Request> job;
            if (pending)
            {
                job = std::move (pending);
                pending.reset();
                cancel.store (false, std::memory_order_release);
            }
            lock.unlock();

            drain();
            if (job)
                load (*job);
            else
                idle (std::chrono::steady_clock::now());

            lock.lock();
        }
    }

    void KitService::setLoading (const Request& job, int percent, const ContentStatus* found)
    {
        const std::lock_guard lock (mutex);
        if (job.generation != generation.load (std::memory_order_relaxed))
            return; // a newer request reports instead
        current.content.state = ContentState::loading;
        current.content.percent = std::clamp (percent, 0, 100);
        if (found != nullptr)
        {
            current.content.folder = found->folder;
            current.content.searched = found->searched;
            current.content.problem = {};
        }
        if (const auto* entry = catalog().find (job.kit))
            if (static_cast<std::size_t> (entry->index) < statuses.size())
                statuses[static_cast<std::size_t> (entry->index)] = current.content;
    }

    void KitService::finish (const Request& job, const ContentStatus& status, const KitCatalog::Entry* entry)
    {
        const std::lock_guard lock (mutex);
        if (entry != nullptr && static_cast<std::size_t> (entry->index) < statuses.size())
            statuses[static_cast<std::size_t> (entry->index)] = status;
        if (job.generation != generation.load (std::memory_order_relaxed))
            return; // superseded: the newer request is in flight
        current.content = status;
        inFlight.store (false, std::memory_order_release);
    }

    std::shared_ptr<const engine::Kit> KitService::silentKit (const KitCatalog::Entry* entry, std::string_view id)
    {
        auto kit = std::make_shared<engine::Kit>();
        if (entry != nullptr && entry->ok())
        {
            kit->info = entry->info;
            kit->mix = entry->mix;
            if (auto timelines = buildTimelines (*entry); timelines.ok())
                kit->timelines = std::move (timelines.value);
        }
        else
        {
            kit->info.id = std::string (id);
            kit->info.index = entry != nullptr ? entry->index : -1;
        }
        return kit;
    }

    void KitService::load (const Request& job)
    {
        const auto& kitList = catalog();
        const auto* entry = kitList.find (job.kit);

        if (entry == nullptr)
        {
            juce::StringArray ids;
            for (const auto& e : kitList.entries())
                ids.add (text (e.id));
            publish (silentKit (nullptr, job.kit));
            finish (job, failedWith ("This build of Wubansia has no kit " + text (job.kit) + " (it has " + ids.joinIntoString (", ") + ")"), nullptr);
            return;
        }
        if (! entry->ok())
        {
            publish (silentKit (entry, job.kit));
            finish (job, failedWith (problemText (text (entry->error))), entry);
            return;
        }

        // Where its samples are.
        auto found = content.locate (entry->manifest);
        if (cancelled())
            return; // a newer request (or the end) replaced it
        if (found.state != ContentState::ready)
        {
            publish (silentKit (entry, job.kit));
            finish (job, found, entry);
            return;
        }
        setLoading (job, 0, &found);

        auto source = content.open (entry->manifest, found);
        if (source == nullptr)
        {
            auto status = found;
            status.state = ContentState::failed;
            status.problem = "The samples in " + found.folder.getFullPathName() + " can't be read";
            publish (silentKit (entry, job.kit));
            finish (job, status, entry);
            return;
        }

        // The Kit: the one another instance holds, or built here. Retired Kits are collected between files.
        const auto progress = [&] (int pct) {
            setLoading (job, pct, nullptr);
            if (std::chrono::steady_clock::now() - lastDrain >= kDrainInterval)
                drain();
        };
        const auto version = entry->manifest.kit.contentVersion;
        auto built = engine::KitCache::instance().acquire (
            entry->id, version,
            [&]() -> engine::KitCache::KitResult {
                auto timelines = buildTimelines (*entry, &cancel);
                if (! timelines.ok())
                    return engine::KitCache::KitResult::failure (std::move (timelines.error));
                return engine::buildKit (entry->manifest, entry->mix, *source, std::move (timelines.value), cancel, progress, entry->index);
            },
            &cancel);

        if (! built.ok())
        {
            const auto error = engine::loadErrorOf (built.error);
            if (error == engine::LoadError::cancelled)
                return; // a newer request (or the end) replaced it
            auto status = found;
            status.state = contentStateOf (error);
            status.percent = 0;
            status.problem = problemText (text (built.error));
            publish (silentKit (entry, job.kit));
            finish (job, status, entry);
            return;
        }
        if (cancelled())
            return; // built, but already replaced: it goes with this scope (or stays in KitCache for whoever holds it)

        publish (std::move (built.value));
        auto status = found;
        status.state = ContentState::ready;
        status.percent = 100;
        status.problem = {};
        if (job.contentVersion > 0 && job.contentVersion != version)
            status.problem = "This project was made with " + text (entry->id) + "'s content version " + juce::String (job.contentVersion)
                             + "; this build of Wubansia plays version " + juce::String (version);
        finish (job, status, entry);
    }

    void KitService::publish (std::shared_ptr<const engine::Kit> kit)
    {
        const engine::Kit* raw = kit.get();
        kits.push_back (std::move (kit)); // one reference per publish
        held.store (static_cast<int> (kits.size()), std::memory_order_release);
        publishedCount.fetch_add (1, std::memory_order_acq_rel);
        if (const engine::Kit* back = engine.publish (raw))
            drop (back); // the one before, never adopted
    }

    void KitService::drop (const engine::Kit* kit)
    {
        const auto it = std::find_if (kits.begin(), kits.end(), [kit] (const std::shared_ptr<const engine::Kit>& k) { return k.get() == kit; });
        jassert (it != kits.end()); // every Kit that comes back was published here
        if (it != kits.end())
            kits.erase (it); // the last reference frees the Kit here, on this thread
        held.store (static_cast<int> (kits.size()), std::memory_order_release);
        returnedCount.fetch_add (1, std::memory_order_acq_rel);
    }

    void KitService::drain()
    {
        lastDrain = std::chrono::steady_clock::now();
        engine.collectRetired ([this] (const engine::Kit* kit) { drop (kit); });
    }

    void KitService::idle (std::chrono::steady_clock::time_point now)
    {
        // Every kit's folders: first, whenever the folders change (in any instance), and every 2 s for
        // the kits no folder held.
        const auto folders = content.foldersGeneration();
        if (! surveyed || folders != surveyedFolders)
        {
            surveyed = true;
            surveyedFolders = folders;
            lastRecheck = now;
            survey (false);
            return;
        }
        if (now - lastRecheck >= kRecheckInterval)
        {
            lastRecheck = now;
            survey (true);
        }
    }

    void KitService::survey (bool onlyMissing)
    {
        for (const auto& entry : catalog().entries())
        {
            if (! entry.ok())
                continue;

            bool isCurrent = false;
            ContentState currentState = ContentState::missing;
            {
                const std::lock_guard lock (mutex);
                if (quit || pending)
                    return; // a load goes first; the next idle pass carries on
                if (onlyMissing && statuses[static_cast<std::size_t> (entry.index)].state != ContentState::missing)
                    continue;
                isCurrent = current.kit == text (entry.id);
                currentState = current.content.state;
            }

            // The current kit's own load says where it stands, unless it failed: then a check that finds
            // its content now ready reloads it.
            if (isCurrent && (currentState == ContentState::ready || currentState == ContentState::loading))
                continue;
            const auto found = content.locate (entry.manifest);
            if (isCurrent)
            {
                if (found.state == ContentState::ready)
                    contentChanged();
                continue;
            }
            const std::lock_guard lock (mutex);
            statuses[static_cast<std::size_t> (entry.index)] = found;
        }
    }
}
