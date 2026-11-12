// CLAUDE.md's audio-thread rules, as the tests can see them: processBlock allocates nothing and frees
// nothing, with the player in Song mode on the host's transport, live notes, a SysEx, parameter changes,
// across a kit adoption, and with the Samples drawer's auditions (ADR 0024). The counter
// (helpers/allocation_counter.h) sees malloc and free on Linux, operator new and delete elsewhere; locks
// are left to TSan and, in M3, the realtime sanitizer.

#include "helpers/allocation_counter.h"
#include "helpers/test_helpers.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdlib>
#include <thread>

using namespace wubansia;
using test::SyntheticContent;
using test::SyntheticPlugin;

namespace
{
    // Where the counter test parks its allocations, so the optimiser can't remove them.
    void* volatile sink = nullptr;

    // Counts allocations and frees inside one processBlock. Buffers and MIDI are built outside the count.
    allocations::Counts countsDuring (PluginProcessor& p, juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
    {
        allocations::Scope scope;
        p.processBlock (buffer, midi);
        return scope.counts();
    }
}

TEST_CASE ("the allocation counter counts allocations and frees", "[realtime]")
{
    if (! allocations::available())
        SKIP ("a sanitizer owns the allocation functions in this build");

    // Without this, a counter that silently counts nothing would make every check below pass.
    {
        allocations::Scope scope;
        auto* p = new int (1);
        sink = p;
        delete p;
        const auto counts = scope.counts();
        CHECK (counts.allocations == 1);
        CHECK (counts.frees == 1);
    }

#if WUBANSIA_COUNTS_MALLOC
    {
        allocations::Scope scope;
        juce::HeapBlock<float> block (64); // JUCE's containers allocate with std::malloc
        sink = block.get();
        CHECK (scope.count() >= 1);
    }
    {
        // Called through volatile pointers: the optimiser knows what std::malloc and std::free do, and
        // may move them past the counter's own bookkeeping or drop them.
        void* (*volatile allocate) (std::size_t) = std::malloc;
        void (*volatile release) (void*) = std::free;
        allocations::Scope scope;
        sink = allocate (64);
        release (sink);
        const auto counts = scope.counts();
        CHECK (counts.allocations == 1);
        CHECK (counts.frees == 1);
    }
#endif
}

TEST_CASE ("processBlock neither allocates nor frees", "[realtime]")
{
    if (! allocations::available())
        SKIP ("a sanitizer owns the allocation functions in this build");

    SyntheticContent::Options options;
    options.ids = { "SYN01", "SYN02" };
    SyntheticPlugin plugin (options);
    test::TestPlayHead head;
    plugin->setPlayHead (&head);
    test::prepareWithKit (*plugin, 48000.0, 512);

    juce::AudioBuffer<float> buffer (plugin->getTotalNumOutputChannels(), 512);
    juce::MidiBuffer midi;
    const auto block = [&] {
        const auto counts = countsDuring (*plugin, buffer, midi);
        head.advance (512, 48000.0);
        midi.clear();
        return counts;
    };

    SECTION ("the first block after prepare")
    {
        test::prepare (*plugin, 48000.0, 512);
        CHECK (block().none());
    }

    SECTION ("Song mode on the host's transport, with 12 live notes, a SysEx and parameter changes")
    {
        test::setParameter (*plugin, ParameterIds::playMode, 2.0f);
        head.play (128.0, 0.0);
        for (int i = 0; i < 4; ++i)
            CHECK (block().none());

        for (int note = 48; note < 60; ++note) // channel 2, the lead
            midi.addEvent (juce::MidiMessage::noteOn (2, note, (juce::uint8) 100), note - 48);
        midi.addEvent (juce::MidiMessage::noteOn (10, 36, (juce::uint8) 127), 20);
        midi.addEvent (juce::MidiMessage::noteOff (2, 49), 100);
        const juce::uint8 payload[18] = { 0x7e, 0x7f, 0x06, 0x01 };
        midi.addEvent (juce::MidiMessage::createSysExMessage (payload, (int) sizeof (payload)), 30); // 20 bytes
        midi.addEvent (juce::MidiMessage::controllerEvent (1, 74, 90), 40);
        midi.addEvent (juce::MidiMessage::allNotesOff (2), 500);
        CHECK (block().none());

        juce::Random random (32);
        for (int i = 0; i < 200; ++i)
        {
            // Parameter changes before each block, as a host's automation makes them.
            for (int j = 0; j < 5; ++j)
            {
                auto* parameter = plugin->getParameters()[random.nextInt (plugin->getParameters().size())];
                parameter->setValueNotifyingHost (random.nextFloat());
            }
            test::setParameter (*plugin, ParameterIds::playMode, 2.0f);
            midi.addEvent (juce::MidiMessage::noteOn (10, 42, (juce::uint8) 90), i % 512);
            const auto counts = block();
            REQUIRE (counts.allocations == 0);
            REQUIRE (counts.frees == 0);
        }
    }

    SECTION ("across a kit adoption")
    {
        test::setParameter (*plugin, ParameterIds::playMode, 2.0f);
        head.play (128.0, 0.0);
        plugin->controller().selectKit ("SYN02");
        REQUIRE (test::waitFor ([&] { return ! plugin->kitService().loadInFlight(); }));

        // Played on until the new kit is in (the next bar line) and a while after.
        int blocks = 0;
        while (plugin->telemetry().kitIndex.load() != 1 && blocks < 1000)
        {
            const auto counts = block();
            REQUIRE (counts.allocations == 0);
            REQUIRE (counts.frees == 0);
            ++blocks;
        }
        CHECK (plugin->telemetry().kitIndex.load() == 1);
        for (int i = 0; i < 50; ++i)
            REQUIRE (block().none());
    }

    SECTION ("auditions: offered, replaced, stopped, finished, bypassed and offline")
    {
        // The sound thread allocates the clips and frees them; the audio thread only takes, plays and lets them go.
        auto& controller = plugin->controller();
        const auto& auditions = plugin->auditionPlayer();
        const auto offer = [&] (int file) {
            REQUIRE (controller.audition ({ "SYN01", file }) > 0u);
            REQUIRE (test::waitFor ([&] { return auditions.offered(); }));
        };
        test::setParameter (*plugin, ParameterIds::playMode, 1.0f);
        head.play (128.0, 0.0);

        offer (28); // FX/Riser.wav: taken, then plays
        CHECK (block().none());
        CHECK (auditions.sounding() > 0u);
        for (int i = 0; i < 4; ++i)
            CHECK (block().none());

        offer (29); // Loops/Lead Phrase.wav replaces it: the riser fades out
        CHECK (block().none());
        CHECK (block().none());

        controller.stopAudition(); // fades out
        CHECK (block().none());
        CHECK (block().none());
        CHECK (auditions.sounding() == 0u);

        offer (9); // Drums/Closed Hat rr1.wav, 4800 frames: it ends
        for (int i = 0; i < 12; ++i)
            CHECK (block().none());
        CHECK (auditions.sounding() == 0u);

        offer (28); // bypassed: let go unplayed
        {
            allocations::Scope scope;
            plugin->processBlockBypassed (buffer, midi);
            CHECK (scope.counts().none());
        }
        CHECK_FALSE (auditions.offered());
        CHECK (block().none());

        plugin->setNonRealtime (true); // an offline render: the same
        offer (28);
        CHECK (block().none());
        CHECK_FALSE (auditions.offered());
        CHECK (auditions.sounding() == 0u);
        plugin->setNonRealtime (false);
        CHECK (block().none());
    }

    plugin->setPlayHead (nullptr);
}

TEST_CASE ("host automation path: allocations only", "[realtime][!mayfail]")
{
    // Informational, never fails (docs/decisions/0001-framework.md, D5): VST3 hosts apply automation
    // on the audio thread through JUCE's parameter code, which we don't control. This counts
    // allocations on that path only. The editor polls instead of listening (P10), so the editor being
    // open adds nothing.
    SyntheticPlugin plugin;
    auto& gain = test::masterGainParameter (*plugin);

    const auto automateFromAnotherThread = [&gain] {
        int count = -1;
        std::thread audioThread ([&] {
            allocations::Scope scope;
            gain.setValueNotifyingHost (0.25f);
            count = scope.count();
        });
        audioThread.join();
        return count;
    };

    SECTION ("without an editor")
    {
        CHECK (automateFromAnotherThread() == 0);
    }

    SECTION ("with the editor open")
    {
        test::withEditor (*plugin, [&] (juce::AudioProcessorEditor&) { CHECK (automateFromAnotherThread() == 0); });
    }
}
