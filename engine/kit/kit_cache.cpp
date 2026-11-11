#include "kit/kit_cache.h"

#include "kit/kit_builder.h"

#include <algorithm>
#include <chrono>
#include <utility>

namespace wubansia::engine
{
    KitCache& KitCache::instance()
    {
        // Immortal on purpose: a host can exit, or unload the plugin, while a kit service's thread is still inside
        // acquire (). A destructible static would free the entries, mutex and condition variable under that thread.
        // The cache holds only weak references, short strings and at most a hand-off reference, so nothing of value
        // leaks.
        static KitCache* const cache = new KitCache();
        return *cache;
    }

    KitCache::Entry* KitCache::findEntry (std::string_view id, std::int32_t version) const
    {
        for (const auto& e : entries)
            if (e->id == id && e->version == version)
                return e.get();
        return nullptr;
    }

    void KitCache::prune()
    {
        std::erase_if (entries, [] (const std::unique_ptr<Entry>& e) { return ! e->building && e->waiters == 0 && e->kit.expired(); });
    }

    KitCache::KitResult KitCache::acquire (std::string_view id, std::int32_t version, FunctionRef<KitResult()> build, const std::atomic<bool>* cancel)
    {
        std::shared_ptr<const Kit> released; // a hand-off reference this call lets go of: after the lock, never under it
        std::unique_lock lock (mutex);
        prune();
        Entry* entry = findEntry (id, version);
        if (entry == nullptr)
        {
            entries.push_back (std::make_unique<Entry>());
            entry = entries.back().get();
            entry->id = std::string (id);
            entry->version = version;
        }

        while (true)
        {
            if (auto kit = entry->kit.lock())
                return KitResult::success (std::move (kit));
            if (! entry->building)
                break;

            // Another thread is building this kit: wait for that build to end, then share what it made.
            const auto seen = entry->builds;
            ++entry->waiters;
            bool cancelled = false;
            while (entry->builds == seen)
            {
                if (cancel == nullptr)
                    finished.wait (lock);
                else if (cancel->load (std::memory_order_acquire))
                {
                    cancelled = true;
                    break;
                }
                else
                    finished.wait_for (lock, std::chrono::milliseconds (5));
            }
            // The build's hand-off reference keeps its Kit alive until its last waiter has looked.
            auto kit = entry->kit.lock();
            if (--entry->waiters == 0)
                released = std::move (entry->handoff);
            if (cancelled)
                return KitResult::failure ("cancelled: the load was cancelled while waiting for kit " + entry->id + " v" + std::to_string (version));
            if (kit != nullptr)
                return KitResult::success (std::move (kit));
            // It failed, or its kit is already gone. Its error is ours too, unless it was its caller's cancellation
            // (or another build has started since): then go round, and build or wait again.
            if (! entry->building && ! entry->lastError.empty() && loadErrorOf (entry->lastError) != LoadError::cancelled)
                return KitResult::failure (entry->lastError);
        }

        // Build it here, outside the lock, so other kits build meanwhile. The entry can't be pruned while
        // building is set.
        entry->building = true;
        lock.unlock();

        // Whatever happens in build (), the entry's waiters must hear that it ended.
        struct Finish
        {
            KitCache& cache;
            Entry& entry;
            KitResult* result = nullptr;

            ~Finish()
            {
                {
                    const std::lock_guard guard (cache.mutex);
                    entry.building = false;
                    ++entry.builds;
                    if (result != nullptr && result->ok())
                    {
                        entry.kit = result->value;
                        entry.lastError.clear();
                        // Its caller may drop the Kit at once (a request replaced meanwhile): hold it for the
                        // threads waiting on this build, so none of them decodes it again.
                        if (entry.waiters > 0)
                            entry.handoff = result->value;
                    }
                    else
                        entry.lastError = result != nullptr ? result->error : "failed: building the kit threw";
                }
                cache.finished.notify_all();
            }
        };

        KitResult result;
        {
            Finish finish { *this, *entry };
            result = build();
            if (result.ok() && result.value == nullptr)
                result = KitResult::failure ("failed: building kit " + std::string (id) + " gave no kit");
            finish.result = &result;
        }
        return result;
    }

    std::shared_ptr<const Kit> KitCache::find (std::string_view id, std::int32_t version) const
    {
        const std::lock_guard lock (mutex);
        const Entry* entry = findEntry (id, version);
        return entry != nullptr ? entry->kit.lock() : nullptr;
    }

    int KitCache::waiting (std::string_view id, std::int32_t version) const
    {
        const std::lock_guard lock (mutex);
        const Entry* entry = findEntry (id, version);
        return entry != nullptr ? entry->waiters : 0;
    }
}
