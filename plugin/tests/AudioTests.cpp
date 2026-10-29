#include "SineVoices.h"
#include "helpers/test_helpers.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using Catch::Matchers::WithinAbs;
using wubansia::PluginProcessor;
using wubansia::test::prepare;
using wubansia::test::process;

namespace
{
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;

    juce::MidiBuffer midiAt (const juce::MidiMessage& message, int samplePosition)
    {
        juce::MidiBuffer midi;
        midi.addEvent (message, samplePosition);
        return midi;
    }

    // Renders numSamples in blocks of blockSize, with the MIDI going into the first block.
    juce::AudioBuffer<float> render (PluginProcessor& p, int numSamples, const juce::MidiBuffer& midi = {})
    {
        juce::AudioBuffer<float> out (p.getTotalNumOutputChannels(), numSamples);
        auto firstMidi = midi;

        for (int start = 0; start < numSamples; start += blockSize)
        {
            const auto n = std::min (blockSize, numSamples - start);
            auto block = process (p, n, std::exchange (firstMidi, {}));
            for (int ch = 0; ch < out.getNumChannels(); ++ch)
                out.copyFrom (ch, start, block, ch, 0, n);
        }

        return out;
    }

    // Frequency from positive-going zero crossings, interpolated between samples.
    double measureFrequency (const juce::AudioBuffer<float>& buffer, double rate, int skipSamples)
    {
        const auto* x = buffer.getReadPointer (0);
        double first = -1.0, last = -1.0;
        int crossings = 0;

        for (int i = skipSamples + 1; i < buffer.getNumSamples(); ++i)
        {
            if (x[i - 1] < 0.0f && x[i] >= 0.0f)
            {
                const auto t = (i - 1) + (double) x[i - 1] / (double) (x[i - 1] - x[i]);
                if (first < 0.0)
                    first = t;
                last = t;
                ++crossings;
            }
        }

        return crossings > 1 ? (crossings - 1) * rate / (last - first) : 0.0;
    }

    bool allFinite (const juce::AudioBuffer<float>& buffer)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                if (! std::isfinite (buffer.getSample (ch, i)))
                    return false;
        return true;
    }
}

TEST_CASE ("silence without notes", "[audio]")
{
    PluginProcessor plugin;
    prepare (plugin, sampleRate, blockSize);

    const auto out = render (plugin, 4 * blockSize);
    CHECK (juce::exactlyEqual (out.getMagnitude (0, out.getNumSamples()), 0.0f));
}

TEST_CASE ("note-on is sample-accurate", "[audio]")
{
    PluginProcessor plugin;
    prepare (plugin, sampleRate, blockSize);

    const auto out = process (plugin, blockSize, midiAt (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 16));

    CHECK (out.getNumChannels() == 2);
    CHECK (juce::exactlyEqual (out.getMagnitude (0, 0, 16), 0.0f));
    CHECK (out.getMagnitude (0, 16, 2) > 0.0f); // not late either
    CHECK (out.getMagnitude (0, 16, blockSize - 16) > 0.01f);
    CHECK (out.getMagnitude (1, 16, blockSize - 16) > 0.01f);
}

TEST_CASE ("tuning", "[audio]")
{
    const auto rate = GENERATE (44100.0, 48000.0, 96000.0);
    CAPTURE (rate);

    PluginProcessor plugin;
    prepare (plugin, rate, blockSize);

    const auto out = render (plugin, (int) rate, midiAt (juce::MidiMessage::noteOn (1, 69, (juce::uint8) 100), 0));
    CHECK_THAT (measureFrequency (out, rate, (int) (0.01 * rate)), WithinAbs (440.0, 0.5));
}

TEST_CASE ("note-off, all notes off and all sound off", "[audio]")
{
    PluginProcessor plugin;
    prepare (plugin, sampleRate, blockSize);
    render (plugin, blockSize, midiAt (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0)); // past the 5 ms attack

    const auto releaseSamples = (int) (0.005 * sampleRate);

    SECTION ("note-off releases within 5 ms")
    {
        const auto out = process (plugin, blockSize, midiAt (juce::MidiMessage::noteOff (1, 60), 0));
        CHECK (out.getMagnitude (0, 0, 16) > 0.0f); // a ramp, not a click
        CHECK (juce::exactlyEqual (out.getMagnitude (0, releaseSamples + 16, blockSize - releaseSamples - 16), 0.0f));
    }

    SECTION ("note-on with velocity 0 is a note-off")
    {
        juce::MidiBuffer midi;
        const juce::uint8 noteOnZero[] = { 0x90, 60, 0 };
        midi.addEvent (noteOnZero, 3, 0);
        const auto out = process (plugin, blockSize, midi);
        CHECK (juce::exactlyEqual (out.getMagnitude (0, releaseSamples + 16, blockSize - releaseSamples - 16), 0.0f));
    }

    SECTION ("CC123 (all notes off) releases within 5 ms")
    {
        const auto out = process (plugin, blockSize, midiAt (juce::MidiMessage::allNotesOff (1), 0));
        CHECK (juce::exactlyEqual (out.getMagnitude (0, releaseSamples + 16, blockSize - releaseSamples - 16), 0.0f));
    }

    SECTION ("CC120 (all sound off) is immediate")
    {
        const auto out = process (plugin, blockSize, midiAt (juce::MidiMessage::allSoundOff (1), 0));
        CHECK (juce::exactlyEqual (out.getMagnitude (0, 0, blockSize), 0.0f));
    }
}

TEST_CASE ("polyphony", "[audio]")
{
    wubansia::SineVoices voices;
    voices.prepare (sampleRate);

    juce::AudioBuffer<float> buffer (2, blockSize);
    buffer.clear();

    for (int i = 0; i < 12; ++i)
    {
        const auto message = juce::MidiMessage::noteOn (1, 48 + i, (juce::uint8) 127);
        voices.handleMidi (message.getRawData(), message.getRawDataSize());
    }

    CHECK (voices.getNumActiveVoices() == wubansia::SineVoices::numVoices);

    for (int i = 0; i < 20; ++i)
    {
        buffer.clear();
        voices.render (buffer, 0, blockSize);
        REQUIRE (allFinite (buffer));
        REQUIRE (buffer.getMagnitude (0, blockSize) <= 1.0f);
    }

    SECTION ("a repeated note reuses its voice")
    {
        wubansia::SineVoices fresh;
        fresh.prepare (sampleRate);
        const auto message = juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100);
        fresh.handleMidi (message.getRawData(), message.getRawDataSize());
        fresh.handleMidi (message.getRawData(), message.getRawDataSize());
        CHECK (fresh.getNumActiveVoices() == 1);
    }
}

TEST_CASE ("master gain", "[audio]")
{
    PluginProcessor plugin;

    SECTION ("-60 dB is silence")
    {
        wubansia::test::setMasterGainDb (plugin, -60.0f);
        prepare (plugin, sampleRate, blockSize);
        const auto out = render (plugin, 4 * blockSize, midiAt (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 127), 0));
        CHECK (juce::exactlyEqual (out.getMagnitude (0, out.getNumSamples()), 0.0f));
    }

    SECTION ("changes ramp over about 20 ms instead of jumping")
    {
        prepare (plugin, sampleRate, blockSize);
        render (plugin, 2 * blockSize, midiAt (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 127), 0));

        wubansia::test::setMasterGainDb (plugin, -60.0f);
        const auto out = render (plugin, 4 * blockSize);
        const auto rampSamples = (int) (0.02 * sampleRate);

        CHECK (out.getMagnitude (0, 0, 32) > out.getMagnitude (0, rampSamples / 2, 32)); // still fading
        CHECK (out.getMagnitude (0, 0, 32) > 0.01f);
        CHECK (juce::exactlyEqual (out.getMagnitude (0, rampSamples + 1, out.getNumSamples() - rampSamples - 1), 0.0f));
    }
}

TEST_CASE ("odd block sizes", "[audio]")
{
    PluginProcessor plugin;
    prepare (plugin, sampleRate, 4096);

    CHECK (process (plugin, 0).getNumSamples() == 0);
    CHECK (allFinite (process (plugin, 1)));
    CHECK (allFinite (process (plugin, 7, midiAt (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 3))));
    CHECK (allFinite (process (plugin, 4096)));

    SECTION ("prepare, release, prepare")
    {
        REQUIRE (plugin.getNumActiveVoices() == 1);
        plugin.releaseResources();
        CHECK (plugin.getNumActiveVoices() == 0);

        prepare (plugin, 96000.0, 64);
        const auto out = process (plugin, 64);
        CHECK (juce::exactlyEqual (out.getMagnitude (0, 64), 0.0f));
    }
}

TEST_CASE ("host reset and bypass leave no stuck notes", "[audio]")
{
    PluginProcessor plugin;
    prepare (plugin, sampleRate, blockSize);
    render (plugin, blockSize, midiAt (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0));
    REQUIRE (plugin.getNumActiveVoices() == 1);

    SECTION ("reset() stops every voice before the next block")
    {
        plugin.reset(); // hosts may call this from any thread
        const auto out = process (plugin, blockSize);
        CHECK (juce::exactlyEqual (out.getMagnitude (0, blockSize), 0.0f));
        CHECK (plugin.getNumActiveVoices() == 0);
    }

    SECTION ("a note-off while bypassed can't leave the note hanging")
    {
        juce::AudioBuffer<float> buffer (2, blockSize);
        auto midi = midiAt (juce::MidiMessage::noteOff (1, 60), 0);
        plugin.processBlockBypassed (buffer, midi);
        CHECK (juce::exactlyEqual (buffer.getMagnitude (0, blockSize), 0.0f));

        const auto out = process (plugin, blockSize);
        CHECK (juce::exactlyEqual (out.getMagnitude (0, blockSize), 0.0f));
    }
}

TEST_CASE ("retriggering or stealing a voice doesn't click", "[audio]")
{
    PluginProcessor plugin;
    prepare (plugin, sampleRate, blockSize);
    render (plugin, blockSize, midiAt (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 127), 0));

    // Retrigger the same note much quieter, inside the block. A sine at this pitch and level
    // changes by at most about 0.005 per sample, so a jump in level shows up as a far bigger step.
    const auto out = process (plugin, blockSize, midiAt (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 1), 16));
    float largestStep = 0.0f;
    for (int i = 1; i < blockSize; ++i)
        largestStep = std::max (largestStep, std::abs (out.getSample (0, i) - out.getSample (0, i - 1)));
    CHECK (largestStep < 0.01f);
}
