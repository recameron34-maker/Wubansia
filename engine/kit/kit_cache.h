#pragma once

// KitCache (P4): one Kit per (kit ID, content version) in the process, shared by every plugin instance, so two
// instances playing FUT01 hold its samples once.
//
// It keeps a weak_ptr per entry: the Kit lives while someone holds the shared_ptr acquire () gave (the instances'
// kit services), and is freed when the last one lets go, on that thread (a service thread, after the audio thread
// has retired it). A request for a kit another thread is building waits for that build instead of decoding twice.
//
// Only the message and service threads call it; it locks a mutex, and the build runs outside it, so different kits
// build in parallel. Never call it from the audio thread.

#include "kit/function_ref.h"
#include "kit/kit.h"
#include "util/result.h"

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace wubansia::engine
{
    class KitCache
    {
    public:
        using KitResult = util::Result<std::shared_ptr<const Kit>>;

        // Tests make their own caches; the plugin uses instance ().
        KitCache() = default;
        KitCache (const KitCache&) = delete;
        KitCache& operator= (const KitCache&) = delete;

        // The process-wide cache.
        static KitCache& instance();

        // The Kit for (id, version):
        // - the live one, if anyone still holds it;
        // - else, if another thread is building it, the one that build makes (this waits for it);
        // - else what build () makes, here on this thread, which the cache then shares.
        // A failed build isn't kept: the next acquire builds again. Threads that waited for it get the same error,
        // unless it was a cancellation ("cancelled: ..."), which only concerns its own caller: then one of them builds.
        // cancel, if given, ends a wait at once with "cancelled: ..." (it is checked every few milliseconds); pass
        // the same flag to the build, so a cancelled request never waits long.
        KitResult acquire (std::string_view id, std::int32_t version, FunctionRef<KitResult()> build,
            const std::atomic<bool>* cancel = nullptr);

        // The live Kit for (id, version), or null. Never builds or waits.
        std::shared_ptr<const Kit> find (std::string_view id, std::int32_t version) const;

        // For tests and diagnostics: how many threads wait for a build of (id, version) now.
        int waiting (std::string_view id, std::int32_t version) const;

    private:
        struct Entry
        {
            std::string id;
            std::int32_t version = 0;
            std::weak_ptr<const Kit> kit;
            bool building = false;
            int waiters = 0;
            std::uint64_t builds = 0; // finished builds, so a waiter sees that one ended
            std::string lastError; // the last finished build's error ("" if it succeeded)
        };

        Entry* findEntry (std::string_view id, std::int32_t version) const;
        void prune();

        mutable std::mutex mutex;
        std::condition_variable finished;
        std::vector<std::unique_ptr<Entry>> entries;
    };
}
