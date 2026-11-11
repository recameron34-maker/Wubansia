// The kit service (services/KitService.h) through the processor, with no editor ever created: latest
// wins, one shared_ptr per publish and every Kit handed back, memory released, content problems
// reported and silent, content that appears later, kits shared between instances, and a processor
// destroyed mid-load that returns at once.

#include "helpers/test_helpers.h"

#include "kit/kit_cache.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <chrono>
#include <memory>

using namespace wubansia;
using test::SyntheticContent;
using test::SyntheticPlugin;
using test::SyntheticProbeLog;

namespace
{
    SyntheticContent::Options kits (std::vector<std::string> ids)
    {
        SyntheticContent::Options options;
        options.ids = std::move (ids);
        return options;
    }

    // Plays blocks on the internal transport until the engine has adopted the kit at index, or 2000 blocks.
    bool playUntilAdopted (PluginProcessor& p, int index)
    {
        for (int i = 0; i < 2000; ++i)
        {
            if (p.telemetry().kitIndex.load() == index)
                return true;
            test::process (p, 256);
        }
        return p.telemetry().kitIndex.load() == index;
    }
}

TEST_CASE ("20 kit switches and 1,000 mode, variant and A/B switches leave nothing held and the old kits freed", "[service]")
{
    SyntheticPlugin plugin (kits ({ "SVC01", "SVC02", "SVC03" }));
    test::prepareWithKit (*plugin, 48000.0, 256);
    plugin->controller().setTransport (true, 0.0); // playing: switches land on bar lines
    auto& controller = plugin->controller();
    const auto& service = plugin->kitService();

    const char* ids[] = { "SVC01", "SVC02", "SVC03" };
    for (int i = 1; i <= 20; ++i)
    {
        const auto index = i % 3;
        controller.selectKit (ids[index]);
        REQUIRE (test::waitForStatus (*plugin, ContentState::ready));
        REQUIRE (playUntilAdopted (*plugin, index));
    }
    REQUIRE (plugin->telemetry().kitIndex.load() == 20 % 3);

    juce::Random random (20);
    for (int i = 0; i < 1000; ++i)
    {
        test::setParameter (*plugin, ParameterIds::playMode, (float) random.nextInt (3));
        test::setParameter (*plugin, ParameterIds::voxSynth, (float) random.nextInt (2));
        test::setParameter (*plugin, ParameterIds::ab[(size_t) random.nextInt (3)], (float) random.nextInt (2));
        test::process (*plugin, 64);
    }

    // Everything the engine let go has come back (the service collects every 50 ms): it holds the
    // playing kit's reference alone, and the other kits are freed.
    REQUIRE (test::waitFor ([&] { return service.heldKits() == 1; }));
    CHECK (service.publishedKits() == service.returnedKits() + 1);
    CHECK (engine::KitCache::instance().find ("SVC01", 1) == nullptr);
    CHECK (engine::KitCache::instance().find ("SVC02", 1) == nullptr);
    CHECK (engine::KitCache::instance().find ("SVC03", 1) != nullptr);

    // Mode, variant and A/B changes built nothing and published nothing.
    const auto published = service.publishedKits();
    const auto reads = plugin.content->reads.load();
    for (int i = 0; i < 100; ++i)
    {
        test::setParameter (*plugin, ParameterIds::playMode, (float) (i % 3));
        test::process (*plugin, 64);
    }
    CHECK (service.publishedKits() == published);
    CHECK (plugin.content->reads.load() == reads);
}

TEST_CASE ("latest wins: a burst of kit requests loads the last one, and the ones before give way", "[service]")
{
    auto options = kits ({ "LWN01", "LWN02", "LWN03" });
    options.readDelay = std::chrono::milliseconds (5);
    SyntheticPlugin plugin (options);
    test::prepare (*plugin, 48000.0, 256);
    REQUIRE (test::waitFor ([&] { return plugin.content->readsStarted.load() > 0; }));

    auto& controller = plugin->controller();
    controller.selectKit ("LWN02");
    controller.selectKit ("LWN03");
    controller.selectKit ("LWN02");
    controller.selectKit ("LWN03");
    REQUIRE (test::waitForStatus (*plugin, ContentState::ready));
    CHECK (plugin->kitService().status().kit == "LWN03");
    REQUIRE (playUntilAdopted (*plugin, 2));

    // Far fewer reads than five whole loads: the superseded ones stopped within a file.
    CHECK (plugin.content->reads.load() < 5 * engine::testing::syn::fileCount);
    REQUIRE (test::waitFor ([&] { return plugin->kitService().heldKits() == 1; }));
}

TEST_CASE ("content problems are reported, named and silent; content that appears loads", "[service][content]")
{
    const auto problem = GENERATE (SyntheticContent::Samples::missing, SyntheticContent::Samples::lfsPointers, SyntheticContent::Samples::mismatch,
        SyntheticContent::Samples::failed);
    CAPTURE ((int) problem);
    auto options = kits ({ "CPR01" });
    options.samples = problem;
    SyntheticPlugin plugin (options);
    test::prepare (*plugin, 48000.0, 512);

    const auto expected = problem == SyntheticContent::Samples::missing       ? ContentState::missing
                          : problem == SyntheticContent::Samples::lfsPointers ? ContentState::lfsPointers
                          : problem == SyntheticContent::Samples::mismatch    ? ContentState::mismatch
                                                                              : ContentState::failed;
    REQUIRE (test::waitForStatus (*plugin, expected));
    const auto status = plugin->controller().status();
    CHECK (status.problem.isNotEmpty());
    CHECK (status.searched.size() > 0);
    CHECK (plugin->controller().kits()[0].content.state == expected);
    CHECK (plugin.content->opens == 0); // nothing read

    // Silent, even for MIDI in; the engine has the kit's silent stand-in (its index, no instruments).
    plugin->controller().setTransport (true, 0.0);
    const auto out = test::render (*plugin, 24000, 512, test::midiAt (juce::MidiMessage::noteOn (10, 36, (juce::uint8) 127), 0));
    CHECK (test::silent (out));
    CHECK (plugin->telemetry().kitIndex.load() == 0);

    // The content arrives (another instance chose a folder, or the person picked one): it loads.
    plugin.content->setSamples (SyntheticContent::Samples::ready);
    SECTION ("the folders change")
    {
        plugin.content->bumpFolders();
    }
    SECTION ("the person chooses a folder")
    {
        plugin->controller().setContentFolder (juce::File::getSpecialLocation (juce::File::tempDirectory));
        CHECK (plugin.content->userFolder() == juce::File::getSpecialLocation (juce::File::tempDirectory));
    }
    REQUIRE (test::waitForStatus (*plugin, ContentState::ready, std::chrono::milliseconds (5000)));
    plugin->controller().setTransport (false, 0.0); // stopped, the kit comes in after its 20 ms fade, not at a bar line
    const auto later = test::render (*plugin, 48000, 512, test::midiAt (juce::MidiMessage::noteOn (10, 36, (juce::uint8) 127), 9600));
    CHECK (test::peakOf (later) > 0.05f);
    CHECK (plugin->telemetry().kitIndex.load() == 0);
}

TEST_CASE ("a kit whose content was missing loads once it is there, without anyone asking", "[service][content]")
{
    auto options = kits ({ "LTR01" });
    options.samples = SyntheticContent::Samples::missing;
    SyntheticPlugin plugin (options);
    test::prepare (*plugin, 48000.0, 512);
    REQUIRE (test::waitForStatus (*plugin, ContentState::missing));

    plugin.content->setSamples (SyntheticContent::Samples::ready); // installed while the plugin runs
    CHECK (test::waitForStatus (*plugin, ContentState::ready, std::chrono::milliseconds (6000)));
}

TEST_CASE ("every kit's card has its content status", "[service][content]")
{
    SyntheticPlugin plugin (kits ({ "CRD01", "CRD02" }));
    test::prepare (*plugin, 48000.0, 512);
    REQUIRE (test::waitForStatus (*plugin, ContentState::ready));
    REQUIRE (test::waitFor ([&] {
        const auto cards = plugin->controller().kits();
        return cards.size() == 2 && cards[0].content.state == ContentState::ready && cards[1].content.state == ContentState::ready;
    }));
}

TEST_CASE ("two instances playing one kit share its samples", "[service]")
{
    SyntheticPlugin a (kits ({ "SHR01" }));
    SyntheticPlugin b (kits ({ "SHR01" }));
    test::prepareWithKit (*a);
    test::prepareWithKit (*b);
    CHECK (a.content->reads.load() == engine::testing::syn::fileCount);
    CHECK (b.content->reads.load() == 0); // the cache's Kit, built once
    CHECK (b->kitService().status().content.state == ContentState::ready);
}

TEST_CASE ("destroying the processor mid-load returns within 200 ms", "[service]")
{
    auto options = kits ({ "DST01" });
    options.readDelay = std::chrono::milliseconds (40); // a load of about 1.2 s
    auto plugin = std::make_unique<SyntheticPlugin> (options);
    test::prepare (**plugin, 48000.0, 512);
    auto* content = plugin->content;
    REQUIRE (test::waitFor ([&] { return content->readsStarted.load() >= 3; }));
    REQUIRE ((*plugin)->kitService().loadInFlight());

    const auto start = std::chrono::steady_clock::now();
    plugin.reset();
    CHECK (std::chrono::steady_clock::now() - start < test::limit (std::chrono::milliseconds (200)));
    CHECK (engine::KitCache::instance().find ("DST01", 1) == nullptr);
}

TEST_CASE ("destroying the processor mid-check of a slow content folder stops within the file in hand and returns within 200 ms", "[service][content]")
{
    auto options = kits ({ "DSC01" });
    options.probeDelay = std::chrono::milliseconds (50); // a content check of about 1.5 s, before any read
    options.probeLog = std::make_shared<SyntheticProbeLog>();
    const auto log = options.probeLog; // outlives the processor, which owns the content
    auto plugin = std::make_unique<SyntheticPlugin> (options);
    test::prepare (**plugin, 48000.0, 512);
    auto* content = plugin->content;
    REQUIRE (test::waitFor ([&] { return content->probesStarted.load() >= 3; }));
    REQUIRE ((*plugin)->kitService().loadInFlight());
    REQUIRE (content->readsStarted.load() == 0);

    const auto before = log->started.load();
    const auto start = std::chrono::steady_clock::now();
    plugin.reset();
    const auto end = std::chrono::steady_clock::now();

    // The check gives way within the file in hand (one header read on a slow folder, 50 ms here): no later file
    // starts. Without the cancel flag it would go on through all 30.
    CHECK (log->started.load() <= before + 1);
    // From the end of that file (or the reset, if later), the processor takes under 200 ms. Timed from there, so a
    // slow machine's overrun of the test's 50 ms sleep isn't counted against the processor.
    CHECK (end - std::max (start, log->lastEndTime()) < test::limit (std::chrono::milliseconds (200)));
}

TEST_CASE ("an instance destroyed while another waits for its build leaves the other to finish", "[service]")
{
    auto options = kits ({ "WTR01" });
    options.readDelay = std::chrono::milliseconds (20);
    auto first = std::make_unique<SyntheticPlugin> (options);
    SyntheticPlugin second (options);
    test::prepare (**first, 48000.0, 512);
    REQUIRE (test::waitFor ([&] { return first->content->readsStarted.load() >= 2; }));
    test::prepare (*second, 48000.0, 512); // waits for the first's build in KitCache

    first.reset(); // its build is cancelled; the waiter builds instead
    CHECK (test::waitForStatus (*second, ContentState::ready, std::chrono::milliseconds (5000)));
}
