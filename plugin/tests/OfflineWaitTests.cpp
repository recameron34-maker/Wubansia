// The offline wait (ADR 0005 D5; CLAUDE.md's one exception to the audio-thread rules): in a
// non-realtime render, and only there, processBlock waits for a kit load in flight, 1 ms at a time and
// 30 s per load at most, then renders the kit from the block's first sample. Real time never waits, and
// a load that ends without a kit (missing content) never holds a render up.

#include "helpers/test_helpers.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>

using namespace wubansia;
using test::SyntheticContent;
using test::SyntheticPlugin;
using Clock = std::chrono::steady_clock;

namespace
{
    SyntheticContent::Options slowKit (const char* id, std::chrono::milliseconds perFile)
    {
        SyntheticContent::Options options;
        options.ids = { id };
        options.readDelay = perFile;
        return options;
    }

    const auto kick = juce::MidiMessage::noteOn (10, 36, (juce::uint8) 127);
}

TEST_CASE ("the offline wait is 30 s per load", "[offline]")
{
    CHECK (PluginProcessor::kOfflineWaitLimit == std::chrono::seconds (30));
}

TEST_CASE ("a non-realtime block waits for the load in flight, then renders the kit from its first sample", "[offline]")
{
    SyntheticPlugin plugin (slowKit ("OFL01", std::chrono::milliseconds (5))); // about 150 ms
    plugin->setNonRealtime (true);
    test::prepare (*plugin, 48000.0, 4096);
    REQUIRE (plugin->kitService().loadInFlight());

    const auto start = Clock::now();
    const auto out = test::process (*plugin, 4096, test::midiAt (kick, 0));
    const auto took = Clock::now() - start;

    CHECK (took > std::chrono::milliseconds (50)); // it waited
    CHECK_FALSE (plugin->kitService().loadInFlight());
    CHECK (plugin->telemetry().kitIndex.load() == 0); // adopted on the first sample
    const auto latency = plugin->getLatencySamples();
    CHECK (test::peakOf (out, 0, latency) < test::kQuiet);
    CHECK (test::peakOf (out, latency, 64) > 0.05f); // the kick, on time
}

TEST_CASE ("real time never waits", "[offline]")
{
    SyntheticPlugin plugin (slowKit ("RTW01", std::chrono::milliseconds (20)));
    plugin->setNonRealtime (false);
    test::prepare (*plugin, 48000.0, 512);
    REQUIRE (plugin->kitService().loadInFlight());

    for (int i = 0; i < 10; ++i)
    {
        const auto start = Clock::now();
        const auto out = test::process (*plugin, 512, test::midiAt (kick, 0));
        CHECK (Clock::now() - start < std::chrono::milliseconds (20));
        CHECK (test::silent (out)); // no kit yet
    }
    CHECK (plugin->kitService().loadInFlight());
}

TEST_CASE ("missing content never holds a render up", "[offline]")
{
    SyntheticContent::Options options;
    options.ids = { "MSW01" };
    options.samples = SyntheticContent::Samples::missing;
    SyntheticPlugin plugin (options);
    plugin->setNonRealtime (true);
    test::prepare (*plugin, 48000.0, 512);

    // The first block may wait while the service looks (a few file checks), never for a load.
    auto start = Clock::now();
    test::process (*plugin, 512);
    CHECK (Clock::now() - start < std::chrono::milliseconds (1000));
    REQUIRE (test::waitForStatus (*plugin, ContentState::missing));

    // Once it knows, no block waits.
    for (int i = 0; i < 10; ++i)
    {
        start = Clock::now();
        CHECK (test::silent (test::process (*plugin, 512, test::midiAt (kick, 0))));
        CHECK (Clock::now() - start < std::chrono::milliseconds (20));
    }
}

TEST_CASE ("the wait gives up at its limit, once per load", "[offline]")
{
    SyntheticPlugin plugin (slowKit ("GVU01", std::chrono::milliseconds (0)));
    plugin.content->hold(); // the load can't finish
    plugin->setNonRealtime (true);
    plugin->setOfflineWaitLimitForTesting (std::chrono::milliseconds (300));
    test::prepare (*plugin, 48000.0, 512);

    auto start = Clock::now();
    CHECK (test::silent (test::process (*plugin, 512)));
    const auto first = Clock::now() - start;
    CHECK (first >= std::chrono::milliseconds (290));
    CHECK (first < std::chrono::milliseconds (2000));

    // The same load: its time is spent; no more waiting.
    start = Clock::now();
    test::process (*plugin, 512);
    CHECK (Clock::now() - start < std::chrono::milliseconds (50));

    // A new load gets its own time.
    plugin.content->release();
    REQUIRE (test::waitForStatus (*plugin, ContentState::ready));
}
