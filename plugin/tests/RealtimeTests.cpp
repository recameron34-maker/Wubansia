#include "helpers/allocation_counter.h"
#include "helpers/test_helpers.h"

#include <catch2/catch_test_macros.hpp>

#include <thread>

using wubansia::PluginProcessor;

namespace
{
    // Where the counter test parks its allocations, so the optimiser can't remove them.
    void* volatile sink = nullptr;

    // Counts allocations inside one processBlock. Buffers and MIDI are built outside the count.
    int allocationsDuring (PluginProcessor& p, juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
    {
        allocations::Scope scope;
        p.processBlock (buffer, midi);
        return scope.count();
    }
}

TEST_CASE ("the allocation counter counts", "[realtime]")
{
    // Without this, a counter that silently counts nothing would make every check below pass.
    {
        allocations::Scope scope;
        auto* p = new int (1);
        sink = p;
        delete p;
        CHECK (scope.count() == 1);
    }

#if WUBANSIA_COUNTS_MALLOC
    {
        allocations::Scope scope;
        juce::HeapBlock<float> block (64); // JUCE's containers allocate with std::malloc
        sink = block.get();
        CHECK (scope.count() >= 1);
    }
#endif
}

TEST_CASE ("processBlock does not allocate", "[realtime]")
{
    // CLAUDE.md: processBlock must not allocate. This counter sees operator new only, not
    // malloc or locks; the realtime sanitizer arrives in M3.
    PluginProcessor plugin;
    wubansia::test::prepare (plugin, 48000.0, 512);

    juce::AudioBuffer<float> buffer (plugin.getTotalNumOutputChannels(), 512);
    juce::MidiBuffer midi;

    SECTION ("first block after prepare")
    {
        CHECK (allocationsDuring (plugin, buffer, midi) == 0);
    }

    SECTION ("note-ons and note-offs")
    {
        for (int note = 60; note < 72; ++note)
            midi.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), note - 60);
        midi.addEvent (juce::MidiMessage::noteOff (1, 61), 100);
        CHECK (allocationsDuring (plugin, buffer, midi) == 0);
    }

    SECTION ("a SysEx message in the buffer")
    {
        const juce::uint8 payload[18] = { 0x7e, 0x7f, 0x06, 0x01 };
        midi.addEvent (juce::MidiMessage::createSysExMessage (payload, (int) sizeof (payload)), 0); // 20 bytes
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 1);
        CHECK (allocationsDuring (plugin, buffer, midi) == 0);
    }

    SECTION ("a gain change made before the block")
    {
        wubansia::test::setMasterGainDb (plugin, -24.0f);
        CHECK (allocationsDuring (plugin, buffer, midi) == 0);
    }

    SECTION ("steady state")
    {
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        allocationsDuring (plugin, buffer, midi);
        midi.clear();

        for (int i = 0; i < 100; ++i)
            REQUIRE (allocationsDuring (plugin, buffer, midi) == 0);
    }
}

TEST_CASE ("host automation path: allocations only", "[realtime][!mayfail]")
{
    // Informational, never fails (docs/decisions/0001-framework.md, D5): VST3 hosts apply
    // automation on the audio thread through JUCE's parameter code, which we don't control.
    // This counts allocations on that path only. The locks it takes (listenerLock, the APVTS
    // lock) and the message-queue post with the editor open are not measured here.
    PluginProcessor plugin;
    auto& gain = wubansia::test::masterGainParameter (plugin);

    const auto automateFromAnotherThread = [&gain]
    {
        int count = -1;
        std::thread audioThread ([&]
            {
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
        wubansia::test::withEditor (plugin, [&] (juce::AudioProcessorEditor&)
            { CHECK (automateFromAnotherThread() == 0); });
    }
}
