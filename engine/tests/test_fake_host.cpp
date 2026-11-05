// The fake host (engine/testing/fake_host.h): what each block gets, as a plugin host would give it.

#include "alloc_counter.h"

#include "testing/fake_host.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <array>
#include <vector>

using namespace wubansia::engine;
using namespace wubansia::engine::testing;

namespace
{
    // Stands in for the Engine and records each block.
    struct Recorder
    {
        struct Block
        {
            int numSamples;
            int numOutputs;
            TransportInfo transport;
            std::vector<MidiEvent> midi;
        };

        void prepare (double rate, int block, int outputs)
        {
            preparedRate = rate;
            preparedBlock = block;
            preparedOutputs = outputs;
        }

        void process (float* const* out, int numOutputs, int numSamples, const TransportInfo& transport, const Controls&, const MidiEvent* midi, int numMidi)
        {
            for (int c = 0; c < numOutputs; ++c)
                for (int i = 0; i < numSamples; ++i)
                    out[c][i] = static_cast<float> (c + 1);
            blocks.push_back ({ numSamples, numOutputs, transport, std::vector<MidiEvent> (midi, midi + numMidi) });
        }

        double preparedRate = 0.0;
        int preparedBlock = 0;
        int preparedOutputs = 0;
        std::vector<Block> blocks;
    };
}

TEST_CASE ("the fake host moves its transport on by the samples rendered", "[fake_host]")
{
    FakeHost host (48000.0, 512);
    Recorder engine;
    host.prepare (engine);
    CHECK (engine.preparedRate == 48000.0);
    CHECK (engine.preparedBlock == 512);
    CHECK (engine.preparedOutputs == 2);

    host.setTempo (120.0);
    host.play (4.0);
    FakeHost::Output out;
    const Controls controls;
    host.render (engine, controls, 48000, out); // one second: 2 quarters at 120 BPM

    CHECK (host.ppq() == Catch::Approx (6.0));
    CHECK (host.samplePosition() == 48000);
    REQUIRE (engine.blocks.size() == 94); // 93 blocks of 512 and one of 384
    CHECK (engine.blocks.back().numSamples == 384);
    CHECK (engine.blocks.front().transport.ppq == 4.0);
    CHECK (engine.blocks[1].transport.ppq == Catch::Approx (4.0 + 512.0 * 2.0 / 48000.0));
    CHECK (out.left.size() == 48000);
    CHECK (out.right.size() == 48000);
    CHECK (out.left[47999] == 1.0f);
    CHECK (out.right[0] == 2.0f);

    host.stop();
    host.render (engine, controls, 1000, out);
    CHECK (host.ppq() == Catch::Approx (6.0)); // stopped: the position holds
    CHECK_FALSE (engine.blocks.back().transport.playing);
}

TEST_CASE ("the fake host wraps its loop region and cycles block sizes", "[fake_host]")
{
    FakeHost host (44100.0, 2048, 1);
    Recorder engine;
    host.setTempo (147.0);
    host.setLoop (8.0, 12.0);
    host.play (11.5);
    FakeHost::Output out;
    const std::array<int, 3> sizes { 1, 17, 2048 };
    host.render (engine, Controls {}, 44100, out, sizes);

    CHECK (out.right.empty()); // one output
    CHECK (engine.blocks[0].numSamples == 1);
    CHECK (engine.blocks[1].numSamples == 17);
    CHECK (engine.blocks[2].numSamples == 2048);
    double expected = 11.5 + 44100.0 * 147.0 / (60.0 * 44100.0); // 2.45 quarters on
    expected = 8.0 + (expected - 12.0); // wrapped once
    CHECK (host.ppq() == Catch::Approx (expected));
    for (const auto& b : engine.blocks)
    {
        CHECK (b.transport.looping);
        CHECK (b.transport.ppq >= 8.0);
        CHECK (b.transport.ppq < 12.0);
    }
}

TEST_CASE ("the fake host delivers MIDI in the block that holds it, at its offset", "[fake_host]")
{
    FakeHost host (48000.0, 100);
    Recorder engine;
    host.addMidi (250, 0x90, 60, 100);
    host.addMidi (0, 0x99, 36, 127);
    host.addMidi (250, 0x80, 60, 0); // same sample: the order it was added in
    host.addMidi (199, 0xB0, 74, 64);
    host.play (0.0);
    FakeHost::Output out;
    host.render (engine, Controls {}, 400, out, std::array { 100 });

    REQUIRE (engine.blocks.size() == 4);
    REQUIRE (engine.blocks[0].midi.size() == 1);
    CHECK (engine.blocks[0].midi[0].offset == 0);
    CHECK (engine.blocks[0].midi[0].status == 0x99);
    REQUIRE (engine.blocks[1].midi.size() == 1);
    CHECK (engine.blocks[1].midi[0].offset == 99);
    REQUIRE (engine.blocks[2].midi.size() == 2);
    CHECK (engine.blocks[2].midi[0].offset == 50);
    CHECK (engine.blocks[2].midi[0].status == 0x90);
    CHECK (engine.blocks[2].midi[1].status == 0x80);
    CHECK (engine.blocks[3].midi.empty());

    host.addMidi (100, 0x90, 61, 1); // already past: goes out at the next block's start
    host.render (engine, Controls {}, 100, out);
    REQUIRE (engine.blocks.back().midi.size() == 1);
    CHECK (engine.blocks.back().midi[0].offset == 0);
}

TEST_CASE ("the fake host's transport can say nothing, or go offline", "[fake_host]")
{
    FakeHost host (48000.0, 64);
    host.setHasPosition (false);
    host.setTempo (0.0);
    host.setNonRealtime (true);
    const auto t = host.transport();
    CHECK_FALSE (t.hasPosition);
    CHECK (t.bpm == 0.0);
    CHECK (t.nonRealtime);
    CHECK_FALSE (t.playing);
}

TEST_CASE ("renderBlock allocates nothing, with the real engine", "[fake_host][alloc]")
{
    if (! allocations::available())
        SKIP ("a sanitizer owns the allocation functions in this build");

    Engine engine;
    FakeHost host (48000.0, 256);
    host.prepare (engine);
    host.setTempo (128.0);
    host.play (0.0);
    for (int i = 0; i < 64; ++i)
        host.addMidi (i * 37, 0x99, 36, 100);
    std::vector<float> left (256), right (256);
    float* out[] { left.data(), right.data() };
    const Controls controls;

    allocations::Scope scope;
    for (int block = 0; block < 10; ++block)
        host.renderBlock (engine, controls, 256, out);
    CHECK (scope.counts().none());
}
