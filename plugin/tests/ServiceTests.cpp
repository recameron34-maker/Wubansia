// The kit service (services/KitService.h) through the processor, with no editor ever created: latest
// wins, one shared_ptr per publish and every Kit handed back, memory released, content problems
// reported and silent, content that appears later, kits shared between instances, and a processor
// destroyed mid-load that returns at once.

#include "helpers/test_helpers.h"

#include "kit/kit_cache.h"
#include "testing/synthetic_kit.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <chrono>
#include <atomic>
#include <memory>
#include <thread>

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

// Drum kits (ADR 0030): the drum kit thread, services/DrumKitService.h.

namespace
{
    struct DrumKitRig
    {
        explicit DrumKitRig (SyntheticContent::Options options = {}) : content (std::move (options))
        {
            samples = juce::File::getSpecialLocation (juce::File::tempDirectory).getNonexistentChildFile ("WubansiaDrumKitService", "", false);
            found = content.locate (engine::testing::syntheticContent().kit);
        }
        ~DrumKitRig()
        {
            service.reset();
            samples.deleteRecursively();
        }

        DrumKitService::Request request (int side = 1) const
        {
            DrumKitService::Request r;
            r.kit = "SYN01";
            r.manifest = engine::testing::syntheticContent().kit;
            r.mix = engine::testing::syntheticContent().mix;
            r.found = found;
            r.samplesFolder = samples;
            r.kickSide = side;
            return r;
        }

        // Waits up to 10 s for the ticket's status to reach stage.
        DrumKitStatus waitFor (std::uint32_t ticket, DrumKitStage stage) const
        {
            const auto until = std::chrono::steady_clock::now() + std::chrono::seconds (10);
            auto s = service->status();
            while ((s.ticket != ticket || s.stage != stage) && std::chrono::steady_clock::now() < until)
            {
                std::this_thread::sleep_for (std::chrono::milliseconds (2));
                s = service->status();
            }
            return s;
        }

        SyntheticContent content;
        juce::File samples;
        ContentStatus found;
        std::unique_ptr<DrumKitService> service = std::make_unique<DrumKitService> (content);
    };

    const juce::String synFolder ("SYN01 Synthetic Test Kit Drum Kit");
}

TEST_CASE ("the drum kit thread starts with the first request", "[drumkit][service]")
{
    PluginProcessor plugin;
    CHECK_FALSE (plugin.drumKitService().started());

    SyntheticPlugin synthetic;
    test::prepareWithKit (*synthetic);
    CHECK_FALSE (synthetic->drumKitService().started()); // a prepare and a kit load start nothing

    DrumKitRig rig;
    CHECK_FALSE (rig.service->started());
    rig.service->refuse ("SYN01", "SYN01's samples aren't installed");
    CHECK_FALSE (rig.service->started()); // a refusal starts nothing
    const auto ticket = rig.service->request (rig.request());
    CHECK (ticket != 0);
    CHECK (rig.service->started());
}

TEST_CASE ("a press and its click are one job; another kit or side replaces the queued job", "[drumkit][service]")
{
    SyntheticContent::Options slow;
    slow.readDelay = std::chrono::milliseconds (15);
    DrumKitRig rig (slow);
    const auto first = rig.service->request (rig.request (1));
    CHECK (rig.service->request (rig.request (1)) == first); // its click: the same job
    const auto side = rig.service->request (rig.request (0));
    CHECK (side != first);
    CHECK (rig.service->request (rig.request (0)) == side);
    const auto done = rig.waitFor (side, DrumKitStage::ready);
    REQUIRE (done.stage == DrumKitStage::ready);
    CHECK (done.sfz.getFileName() == "SYN01 Synthetic Test Kit Drum Kit - Kick A.sfz");
    CHECK (rig.samples.getNumberOfChildFiles (juce::File::findFilesAndDirectories) == 1); // one folder, no staging

    // Another kit (here another content folder's copy): another job.
    auto other = rig.request (0);
    other.kit = "SYN02";
    other.manifest.kit.id = "SYN02";
    CHECK (rig.service->request (other) != side);
}

TEST_CASE ("saving, then ready with the folder and the side's SFZ; failures in the drawer's words; refuse () at once", "[drumkit][service]")
{
    {
        DrumKitRig rig;
        const auto ticket = rig.service->request (rig.request (1));
        const auto now = rig.service->status();
        CHECK (now.ticket == ticket);
        CHECK ((now.stage == DrumKitStage::saving || now.stage == DrumKitStage::ready));
        CHECK (now.kit == "SYN01");
        const auto done = rig.waitFor (ticket, DrumKitStage::ready);
        REQUIRE (done.stage == DrumKitStage::ready);
        CHECK (done.folder == rig.samples.getChildFile (synFolder));
        CHECK (done.sfz == done.folder.getChildFile ("SYN01 Synthetic Test Kit Drum Kit - Kick B.sfz"));
        CHECK (done.sfz.existsAsFile());
        CHECK (done.wrote);
        CHECK (done.files == 20);
        CHECK (done.problem.isEmpty());

        rig.service->refuse ("SYN01", "SYN01's samples aren't installed");
        const auto refused = rig.service->status();
        CHECK (refused.ticket == 0);
        CHECK (refused.stage == DrumKitStage::failed);
        CHECK (refused.kit == "SYN01");
        CHECK (refused.problem == "SYN01's samples aren't installed");
    }
    {
        SyntheticContent::Options altered;
        altered.alteredPath = "Drums/Snare v1 rr2.wav";
        DrumKitRig rig (altered);
        const auto ticket = rig.service->request (rig.request());
        const auto failed = rig.waitFor (ticket, DrumKitStage::failed);
        REQUIRE (failed.stage == DrumKitStage::failed);
        CHECK (failed.problem == "Snare v1 rr2.wav isn't the file SYN01's content v1 lists: reinstall SYN01");
        CHECK (failed.sourceProblem);
        CHECK (failed.reinstall);
        CHECK (failed.folder == juce::File());
        CHECK (rig.samples.getNumberOfChildFiles (juce::File::findFilesAndDirectories) == 0);
    }
    {
        // A pad the SFZ can't carry: its words, the pad named.
        DrumKitRig rig;
        auto request = rig.request();
        request.manifest.instruments[engine::testing::syn::snare].align = engine::Align::end;
        const auto ticket = rig.service->request (std::move (request));
        const auto failed = rig.waitFor (ticket, DrumKitStage::failed);
        CHECK (failed.problem == "its pad 38 D1 (Snare) is end-aligned, which a drum kit can't carry");
        CHECK_FALSE (failed.sourceProblem);
    }
}

TEST_CASE ("destroyed mid-save: stops between files, joins, leaves no staging folder", "[drumkit][service]")
{
    DrumKitRig rig;
    std::atomic<bool> reached { false };
    std::atomic<int> after { 0 };
    rig.service->setFileProbeForTesting ([&] (int done) {
        if (done == 2)
        {
            reached = true;
            std::this_thread::sleep_for (std::chrono::milliseconds (100)); // the file in hand
        }
        if (done > 2)
            ++after;
    });
    rig.service->request (rig.request());
    const auto until = std::chrono::steady_clock::now() + std::chrono::seconds (10);
    while (! reached && std::chrono::steady_clock::now() < until)
        std::this_thread::sleep_for (std::chrono::milliseconds (1));
    REQUIRE (reached);
    const auto start = std::chrono::steady_clock::now();
    rig.service.reset();
    CHECK (std::chrono::steady_clock::now() - start < std::chrono::seconds (2));
    CHECK (after.load() <= 1); // at most the file after the one in hand
    CHECK (rig.samples.getNumberOfChildFiles (juce::File::findFilesAndDirectories) == 0);
}

TEST_CASE ("a press equal to the last ready request keeps its ticket and stays ready", "[drumkit][service]")
{
    DrumKitRig rig;
    const auto ticket = rig.service->request (rig.request());
    const auto done = rig.waitFor (ticket, DrumKitStage::ready);
    REQUIRE (done.stage == DrumKitStage::ready);
    const juce::Time past (2001, 0, 1, 12, 0, 0, 0, false);
    for (const auto& entry : juce::RangedDirectoryIterator (done.folder, true, "*", juce::File::findFiles))
        entry.getFile().setLastModificationTime (past);
    int probed = 0;
    rig.service->setFileProbeForTesting ([&] (int) { ++probed; });
    const auto reads = rig.content.fileReads.load();

    CHECK (rig.service->request (rig.request()) == ticket);
    const auto again = rig.service->status(); // at once, in the same mouse event
    CHECK (again.ticket == ticket);
    CHECK (again.stage == DrumKitStage::ready);
    CHECK (again.sfz == done.sfz);
    std::this_thread::sleep_for (std::chrono::milliseconds (50));
    CHECK (rig.service->status().stage == DrumKitStage::ready);
    CHECK (rig.content.fileReads.load() == reads); // no second write, no read
    CHECK (probed == 0);
    for (const auto& entry : juce::RangedDirectoryIterator (done.folder, true, "*", juce::File::findFiles))
        CHECK (entry.getFile().getLastModificationTime() == past);
}

TEST_CASE ("a press after the saved folder was deleted saves it again", "[drumkit][service]")
{
    DrumKitRig rig;
    const auto ticket = rig.service->request (rig.request());
    const auto done = rig.waitFor (ticket, DrumKitStage::ready);
    REQUIRE (done.stage == DrumKitStage::ready);
    REQUIRE (done.folder.deleteRecursively());

    const auto again = rig.service->request (rig.request());
    CHECK (again != ticket);
    const auto now = rig.service->status();
    CHECK (now.ticket == again);
    CHECK ((now.stage == DrumKitStage::saving || now.stage == DrumKitStage::ready));
    const auto saved = rig.waitFor (again, DrumKitStage::ready);
    REQUIRE (saved.stage == DrumKitStage::ready);
    CHECK (saved.wrote);
    CHECK (saved.sfz.existsAsFile());
}
