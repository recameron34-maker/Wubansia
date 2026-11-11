// KitCache (kit/kit_cache.h): one Kit per (kit, content version) for the whole process, freed after its last holder
// lets go, and built once even when two instances ask for it at the same time.

#include "manifest_samples.h"

#include "kit/kit_builder.h"
#include "kit/kit_cache.h"
#include "kit/manifest.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

using namespace wubansia;
using namespace wubansia::engine;
using namespace std::chrono_literals;

namespace
{
    using KitResult = KitCache::KitResult;

    // A Kit whose deletion raises a flag.
    std::shared_ptr<const Kit> trackedKit (std::string id, std::atomic<int>& freed)
    {
        auto kit = new Kit();
        kit->info.id = std::move (id);
        return std::shared_ptr<const Kit> (kit, [&freed] (const Kit* k) {
            ++freed;
            delete k;
        });
    }

    // Waits until done () holds, for at most 5 s.
    template <class F>
    bool eventually (F&& done)
    {
        const auto until = std::chrono::steady_clock::now() + 5s;
        while (! done())
        {
            if (std::chrono::steady_clock::now() > until)
                return false;
            std::this_thread::sleep_for (1ms);
        }
        return true;
    }

    // A build that blocks until released, so a test can line another thread up behind it.
    struct GatedBuild
    {
        std::atomic<bool> started { false };
        std::atomic<bool> release { false };
        std::atomic<int> calls { 0 };
        std::atomic<int>* freed = nullptr;
        std::string error; // the build fails with this when not empty
        bool throws = false;

        KitResult operator()()
        {
            ++calls;
            started = true;
            while (! release.load())
                std::this_thread::sleep_for (1ms);
            if (throws)
                throw std::runtime_error ("the build broke");
            if (! error.empty())
                return KitResult::failure (error);
            return KitResult::success (trackedKit ("FUT01", *freed));
        }
    };
}

TEST_CASE ("KitCache: two acquisitions share one Kit, built once", "[kit_cache]")
{
    KitCache cache;
    std::atomic<int> freed { 0 };
    int builds = 0;
    auto build = [&] {
        ++builds;
        return KitResult::success (trackedKit ("FUT01", freed));
    };
    const auto a = cache.acquire ("FUT01", 1, build);
    const auto b = cache.acquire ("FUT01", 1, build);
    REQUIRE (a.ok());
    REQUIRE (b.ok());
    CHECK (a.value.get() == b.value.get());
    CHECK (builds == 1);
    CHECK (cache.find ("FUT01", 1).get() == a.value.get());

    // Another version or another kit is another Kit.
    const auto v2 = cache.acquire ("FUT01", 2, build);
    const auto other = cache.acquire ("FUT02", 1, build);
    CHECK (v2.value.get() != a.value.get());
    CHECK (other.value.get() != a.value.get());
    CHECK (builds == 3);
}

TEST_CASE ("KitCache: the Kit is freed after its last release, then built afresh", "[kit_cache]")
{
    KitCache cache;
    std::atomic<int> freed { 0 };
    int builds = 0;
    auto build = [&] {
        ++builds;
        return KitResult::success (trackedKit ("FUT01", freed));
    };
    auto a = cache.acquire ("FUT01", 1, build).value;
    auto b = cache.acquire ("FUT01", 1, build).value;
    a.reset();
    CHECK (freed == 0);
    CHECK (cache.find ("FUT01", 1) != nullptr);
    b.reset();
    CHECK (freed == 1);
    CHECK (cache.find ("FUT01", 1) == nullptr);

    const auto again = cache.acquire ("FUT01", 1, build);
    REQUIRE (again.ok());
    CHECK (builds == 2);
}

TEST_CASE ("KitCache: a failed build isn't kept", "[kit_cache]")
{
    KitCache cache;
    std::atomic<int> freed { 0 };
    int builds = 0;
    const auto failed = cache.acquire ("FUT01", 1, [&] {
        ++builds;
        return KitResult::failure ("missing: Drums/Kick.wav: no such file");
    });
    CHECK_FALSE (failed.ok());
    CHECK (failed.error == "missing: Drums/Kick.wav: no such file");
    const auto good = cache.acquire ("FUT01", 1, [&] {
        ++builds;
        return KitResult::success (trackedKit ("FUT01", freed));
    });
    CHECK (good.ok());
    CHECK (builds == 2);

    // A build that gives no Kit is a failure too.
    const auto none = cache.acquire ("FUT03", 1, [] { return KitResult::success (nullptr); });
    CHECK_FALSE (none.ok());
    CHECK (none.error.starts_with ("failed: "));
}

TEST_CASE ("KitCache: a second request waits for the first build instead of building again", "[kit_cache]")
{
    KitCache cache;
    std::atomic<int> freed { 0 };
    GatedBuild first;
    first.freed = &freed;
    KitResult a, b;
    int secondBuilds = 0;

    std::thread one ([&] { a = cache.acquire ("FUT01", 1, std::ref (first)); });
    REQUIRE (eventually ([&] { return first.started.load(); }));
    std::thread two ([&] {
        b = cache.acquire ("FUT01", 1, [&] {
            ++secondBuilds;
            return KitResult::success (trackedKit ("FUT01", freed));
        });
    });
    REQUIRE (eventually ([&] { return cache.waiting ("FUT01", 1) == 1; }));

    // Meanwhile another kit builds at once: the wait is per kit.
    const auto other = cache.acquire ("FUT02", 1, [&] { return KitResult::success (trackedKit ("FUT02", freed)); });
    CHECK (other.ok());

    first.release = true;
    one.join();
    two.join();
    REQUIRE (a.ok());
    REQUIRE (b.ok());
    CHECK (a.value.get() == b.value.get());
    CHECK (first.calls == 1);
    CHECK (secondBuilds == 0);
    CHECK (cache.waiting ("FUT01", 1) == 0);
}

TEST_CASE ("KitCache: waiters share a failure, but not a cancellation", "[kit_cache]")
{
    std::atomic<int> freed { 0 };

    SECTION ("the first build fails: the waiter gets its error and builds nothing")
    {
        KitCache cache;
        GatedBuild first;
        first.freed = &freed;
        first.error = "lfs_pointer: Drums/Kick.wav: a Git LFS pointer";
        KitResult a, b;
        int secondBuilds = 0;
        std::thread one ([&] { a = cache.acquire ("FUT01", 1, std::ref (first)); });
        REQUIRE (eventually ([&] { return first.started.load(); }));
        std::thread two ([&] {
            b = cache.acquire ("FUT01", 1, [&] {
                ++secondBuilds;
                return KitResult::success (trackedKit ("FUT01", freed));
            });
        });
        REQUIRE (eventually ([&] { return cache.waiting ("FUT01", 1) == 1; }));
        first.release = true;
        one.join();
        two.join();
        CHECK (a.error == first.error);
        CHECK (b.error == first.error);
        CHECK (secondBuilds == 0);
    }

    SECTION ("the first build is cancelled by its own caller: the waiter builds instead")
    {
        KitCache cache;
        GatedBuild first;
        first.freed = &freed;
        first.error = "cancelled: the load was cancelled after 3 of 52 files";
        KitResult a, b;
        int secondBuilds = 0;
        std::thread one ([&] { a = cache.acquire ("FUT01", 1, std::ref (first)); });
        REQUIRE (eventually ([&] { return first.started.load(); }));
        std::thread two ([&] {
            b = cache.acquire ("FUT01", 1, [&] {
                ++secondBuilds;
                return KitResult::success (trackedKit ("FUT01", freed));
            });
        });
        REQUIRE (eventually ([&] { return cache.waiting ("FUT01", 1) == 1; }));
        first.release = true;
        one.join();
        two.join();
        CHECK (loadErrorOf (a.error) == LoadError::cancelled);
        REQUIRE (b.ok());
        CHECK (secondBuilds == 1);
    }

    SECTION ("the first build throws: the waiter isn't stranded")
    {
        KitCache cache;
        GatedBuild first;
        first.freed = &freed;
        first.throws = true;
        bool threw = false;
        KitResult b;
        std::thread one ([&] {
            try
            {
                (void) cache.acquire ("FUT01", 1, std::ref (first));
            }
            catch (const std::runtime_error&)
            {
                threw = true;
            }
        });
        REQUIRE (eventually ([&] { return first.started.load(); }));
        std::thread two ([&] { b = cache.acquire ("FUT01", 1, [&] { return KitResult::success (trackedKit ("FUT01", freed)); }); });
        REQUIRE (eventually ([&] { return cache.waiting ("FUT01", 1) == 1; }));
        first.release = true;
        one.join();
        two.join();
        CHECK (threw);
        CHECK_FALSE (b.ok());
        CHECK (b.error.starts_with ("failed: "));
        // And the next request builds.
        CHECK (cache.acquire ("FUT01", 1, [&] { return KitResult::success (trackedKit ("FUT01", freed)); }).ok());
    }
}

TEST_CASE ("KitCache: a waiter's cancel ends its wait at once", "[kit_cache]")
{
    KitCache cache;
    std::atomic<int> freed { 0 };
    GatedBuild first;
    first.freed = &freed;
    KitResult a, b;
    std::atomic<bool> cancel { false };
    std::atomic<bool> returned { false };

    std::thread one ([&] { a = cache.acquire ("FUT01", 1, std::ref (first)); });
    REQUIRE (eventually ([&] { return first.started.load(); }));
    std::thread two ([&] {
        b = cache.acquire ("FUT01", 1, [] { return KitResult::failure ("failed: never called"); }, &cancel);
        returned = true;
    });
    REQUIRE (eventually ([&] { return cache.waiting ("FUT01", 1) == 1; }));
    const auto raised = std::chrono::steady_clock::now();
    cancel = true;
    REQUIRE (eventually ([&] { return returned.load(); }));
    CHECK (std::chrono::steady_clock::now() - raised < 1s);
    two.join();
    CHECK (loadErrorOf (b.error) == LoadError::cancelled);
    CHECK (cache.waiting ("FUT01", 1) == 0);

    first.release = true;
    one.join();
    CHECK (a.ok());
}

TEST_CASE ("KitCache with buildKit: two instances loading one kit decode its files once", "[kit_cache]")
{
    const auto kit = parseKitManifest (test::kKitJson);
    REQUIRE (kit.error.empty());
    const auto mix = parseMixManifest (test::kMixJson, kit.value);
    REQUIRE (mix.error.empty());
    auto fill = [&] (test::MemorySource& source) {
        for (std::size_t i = 0; i < kit.value.files.size(); ++i)
        {
            const auto& f = kit.value.files[i];
            source.files.emplace (f.path, util::Result<SampleData>::success (test::sampleFor (f.frames, f.channels, static_cast<std::int32_t> (i))));
        }
    };

    KitCache cache;
    // Instance A's source holds its first read until B is waiting, so both requests overlap.
    test::MemorySource sourceA, sourceB;
    fill (sourceA);
    fill (sourceB);
    std::atomic<bool> aReading { false }, go { false };
    sourceA.beforeRead = [&] (std::size_t read) {
        if (read != 0)
            return;
        aReading = true;
        while (! go.load())
            std::this_thread::sleep_for (1ms);
    };
    const std::atomic<bool> cancelA { false }, cancelB { false };
    auto load = [&] (test::MemorySource& source, const std::atomic<bool>& cancel) {
        return cache.acquire (kit.value.kit.id, kit.value.kit.contentVersion, [&] {
            return buildKit (kit.value, mix.value, source, std::array<Timeline, 4> {}, cancel, [] (int) {});
        }, &cancel);
    };

    KitResult a, b;
    std::thread one ([&] { a = load (sourceA, cancelA); });
    REQUIRE (eventually ([&] { return aReading.load(); }));
    std::thread two ([&] { b = load (sourceB, cancelB); });
    REQUIRE (eventually ([&] { return cache.waiting ("TST01", 3) == 1; }));
    go = true;
    one.join();
    two.join();

    REQUIRE (a.error.empty());
    REQUIRE (b.error.empty());
    CHECK (a.value.get() == b.value.get());
    CHECK (sourceA.reads.size() == kit.value.files.size());
    CHECK (sourceB.reads.empty());

    // Both let go: the Kit is freed, and the next load decodes again.
    a.value.reset();
    b.value.reset();
    CHECK (cache.find ("TST01", 3) == nullptr);
    const auto again = load (sourceB, cancelB);
    REQUIRE (again.ok());
    CHECK (sourceB.reads.size() == kit.value.files.size());
}

TEST_CASE ("KitCache: a waiter shares a finished build even when its builder drops the Kit at once", "[kit_cache]")
{
    // The builder's caller lets go of its Kit the moment acquire returns (its own request was replaced meanwhile).
    // The build's hand-off reference keeps the Kit for the thread waiting on it, which never decodes it again.
    for (int round = 0; round < 20; ++round)
    {
        CAPTURE (round);
        KitCache cache;
        std::atomic<int> freed { 0 };
        GatedBuild first;
        first.freed = &freed;
        KitResult b;
        int secondBuilds = 0;
        std::thread one ([&] { (void) cache.acquire ("FUT01", 1, std::ref (first)); });
        REQUIRE (eventually ([&] { return first.started.load(); }));
        std::thread two ([&] {
            b = cache.acquire ("FUT01", 1, [&] {
                ++secondBuilds;
                return KitResult::success (trackedKit ("FUT01", freed));
            });
        });
        REQUIRE (eventually ([&] { return cache.waiting ("FUT01", 1) == 1; }));
        first.release = true;
        one.join();
        two.join();
        REQUIRE (b.ok());
        CHECK (secondBuilds == 0);
        CHECK (freed == 0);
        b.value.reset(); // the last holder: the hand-off is gone, so the Kit is freed
        CHECK (freed == 1);
        CHECK (cache.find ("FUT01", 1) == nullptr);
    }
}

TEST_CASE ("KitCache::instance is one cache for the process", "[kit_cache]")
{
    CHECK (&KitCache::instance() == &KitCache::instance());
}
