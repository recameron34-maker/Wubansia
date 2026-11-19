// The output scope (ADR 0032): machine room's oscilloscope taps the plugin's output after master_gain, mono, through a
// lock-free ring, and only while it's on.

#include "helpers/test_helpers.h"
#include "telemetry/OutputScope.h"

#include <catch2/catch_test_macros.hpp>

#include <vector>

using namespace wubansia;
using test::SyntheticContent;
using test::SyntheticPlugin;

namespace
{
    std::vector<float> pullAll (OutputScope& scope, int max = OutputScope::kCapacity)
    {
        std::vector<float> out ((std::size_t) max);
        out.resize ((std::size_t) scope.pull (out.data(), max));
        return out;
    }
}

TEST_CASE ("the scope keeps the newest output, mono, and nothing while off", "[realtime][looks]")
{
    OutputScope scope;
    scope.prepare (48000.0);
    std::vector<float> left (10000), right (10000);
    for (std::size_t i = 0; i < left.size(); ++i)
    {
        left[i] = (float) i;
        right[i] = 0.0f;
    }

    SECTION ("off: nothing")
    {
        CHECK_FALSE (scope.isOn());
        scope.push (left.data(), right.data(), 1000);
        CHECK (pullAll (scope).empty());
    }

    SECTION ("on: (L + R) / 2 of each frame")
    {
        scope.setOn (true);
        CHECK (scope.isOn());
        const std::vector<float> ones (1000, 1.0f), zeros (1000, 0.0f);
        scope.push (ones.data(), zeros.data(), 1000);
        const auto got = pullAll (scope);
        REQUIRE (got.size() == 1000);
        for (const auto x : got)
            CHECK (x == 0.5f);
        CHECK (pullAll (scope).empty()); // drained
    }

    SECTION ("the newest samples, oldest first; a full ring drops the rest")
    {
        scope.setOn (true);
        scope.push (left.data(), right.data(), 4000);
        scope.push (left.data() + 4000, right.data() + 4000, 6000); // past the capacity: the tail is dropped
        std::vector<float> got (4096);
        REQUIRE (scope.pull (got.data(), 4096) == 4096);
        // The ring held frames 0-8191 (8192 of them); the newest 4096 are 4096-8191.
        CHECK (got.front() == 0.5f * 4096.0f);
        CHECK (got.back() == 0.5f * 8191.0f);
        CHECK (pullAll (scope).empty());

        scope.push (left.data(), right.data(), 10);
        const auto again = pullAll (scope);
        REQUIRE (again.size() == 10);
        CHECK (again.back() == 4.5f);
    }

    SECTION ("at 96 kHz every second frame, exactly across block edges")
    {
        scope.prepare (96000.0);
        scope.setOn (true);
        scope.push (left.data(), right.data(), 1000);
        CHECK (pullAll (scope).size() == 500);

        for (const int block : { 1, 7, 127, 4096 })
        {
            INFO ("blocks of " << block);
            scope.prepare (96000.0);
            std::vector<float> got;
            int frame = 0;
            while (frame + block <= 8000)
            {
                scope.push (left.data() + frame, right.data() + frame, block);
                frame += block;
                const auto part = pullAll (scope);
                got.insert (got.end(), part.begin(), part.end());
            }
            REQUIRE (got.size() == (std::size_t) (frame + 1) / 2);
            for (std::size_t i = 0; i < got.size(); ++i)
                if (got[i] != 0.5f * (float) (2 * i))
                {
                    FAIL ("sample " << i << " is " << got[i]);
                    break;
                }
        }
    }
}

TEST_CASE ("processBlock taps the output after master_gain, and only while the scope is on", "[realtime][looks]")
{
    SyntheticContent::Options options;
    options.ids = { "SCP01" };
    SyntheticPlugin plugin (options);
    test::prepareWithKit (*plugin, 48000.0, 512);
    auto& controller = plugin->controller();
    const auto note = juce::MidiMessage::noteOn (10, 36, (juce::uint8) 127);
    std::vector<float> scope (4096);

    SECTION ("off: nothing")
    {
        test::process (*plugin, 512, test::midiAt (note, 0));
        CHECK (controller.readScope (scope.data(), 4096) == 0);
    }

    SECTION ("on: the output's (L + R) / 2, bit for bit, after master_gain")
    {
        controller.setScopeOn (true);
        for (const float gainDb : { 0.0f, -6.0f })
        {
            INFO ("master_gain " << gainDb << " dB");
            test::setMasterGainDb (*plugin, gainDb);
            test::process (*plugin, 2048); // past the gain's 20 ms ramp
            controller.readScope (scope.data(), 4096);
            const auto out = test::process (*plugin, 512, test::midiAt (note, 0));
            REQUIRE (controller.readScope (scope.data(), 4096) == 512);
            float peak = 0.0f;
            for (int i = 0; i < 512; ++i)
            {
                const auto expected = 0.5f * (out.getSample (0, i) + out.getSample (out.getNumChannels() > 1 ? 1 : 0, i));
                if (scope[(std::size_t) i] != expected)
                    FAIL ("sample " << i << ": " << scope[(std::size_t) i] << " for " << expected);
                peak = std::max (peak, std::abs (scope[(std::size_t) i]));
            }
            CHECK (peak > 0.0f);
        }
        controller.setScopeOn (false);
        test::process (*plugin, 512, test::midiAt (note, 0));
        CHECK (controller.readScope (scope.data(), 4096) == 0);
    }
}
