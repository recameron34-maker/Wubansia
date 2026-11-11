// The processor's audio on P0's synthetic kit SYN01 (no samples on disk): silence without a kit, a live
// note on its sample, mono, every rate and block size, prepare and reset, bypass, blocks larger than
// prepared, master_gain, the play modes on the host's transport and on the plugin's own, and the
// latency and tail the host is told. They replace M0's sine-voice tests (tuning, the 8-voice cap, the
// 5 ms release), which went with SineVoices.

#include "helpers/test_helpers.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <cmath>

using Catch::Matchers::WithinAbs;
using namespace wubansia;
using test::midiAt;
using test::prepare;
using test::process;
using test::SyntheticContent;
using test::SyntheticPlugin;

namespace
{
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;

    const auto kick = juce::MidiMessage::noteOn (10, 36, (juce::uint8) 127); // channel 10, the kick (B)

    float peak (const juce::AudioBuffer<float>& buffer, int channel, int start, int length)
    {
        length = std::min (length, buffer.getNumSamples() - start);
        return length > 0 ? buffer.getMagnitude (channel, start, length) : 0.0f;
    }

    // numSamples on a play head that moves on block by block, as a host does.
    juce::AudioBuffer<float> playOn (PluginProcessor& p, test::TestPlayHead& head, int numSamples, int block, double rate, const juce::MidiBuffer& midi = {})
    {
        juce::AudioBuffer<float> out (p.getTotalNumOutputChannels(), numSamples);
        out.clear();
        for (int start = 0; start < numSamples; start += block)
        {
            const auto n = std::min (block, numSamples - start);
            juce::MidiBuffer blockMidi;
            for (const auto m : midi)
                if (m.samplePosition >= start && m.samplePosition < start + n)
                    blockMidi.addEvent (m.data, m.numBytes, m.samplePosition - start);
            juce::AudioBuffer<float> buffer (p.getTotalNumOutputChannels(), n);
            p.processBlock (buffer, blockMidi);
            for (int ch = 0; ch < out.getNumChannels(); ++ch)
                out.copyFrom (ch, start, buffer, ch, 0, n);
            head.advance (n, rate);
        }
        return out;
    }
}

TEST_CASE ("silence with no kit", "[audio]")
{
    SECTION ("the content is missing")
    {
        SyntheticContent::Options options;
        options.samples = SyntheticContent::Samples::missing;
        SyntheticPlugin plugin (options);
        test::TestPlayHead head;
        plugin->setPlayHead (&head);
        head.play (128.0);
        prepare (*plugin, sampleRate, blockSize);
        REQUIRE (test::waitForStatus (*plugin, ContentState::missing));

        juce::MidiBuffer midi;
        midi.addEvent (kick, 0);
        midi.addEvent (juce::MidiMessage::noteOn (1, 31, (juce::uint8) 100), 3);
        const auto out = playOn (*plugin, head, 48000, blockSize, sampleRate, midi);
        CHECK (test::silent (out));
        plugin->setPlayHead (nullptr);
    }

    SECTION ("the kit is still loading")
    {
        SyntheticPlugin plugin;
        plugin.content->hold();
        prepare (*plugin, sampleRate, blockSize);
        const auto out = test::render (*plugin, 8 * blockSize, blockSize, midiAt (kick, 0));
        CHECK (test::silent (out));
        plugin.content->release();
        CHECK (test::waitForStatus (*plugin, ContentState::ready));
    }
}

TEST_CASE ("a key-36 note on channel 10 at sample 16 sounds from sample 16 on both channels, after the latency", "[audio]")
{
    SyntheticPlugin plugin;
    test::prepareWithKit (*plugin, sampleRate, 4096);

    const auto latency = plugin->getLatencySamples();
    REQUIRE (latency > 0);
    const auto out = process (*plugin, 4096, midiAt (kick, 16));

    CHECK (out.getNumChannels() == 2);
    CHECK (test::peakOf (out, 0, 16 + latency) < test::kQuiet); // not early
    for (int ch = 0; ch < 2; ++ch)
    {
        CAPTURE (ch);
        CHECK (peak (out, ch, 16 + latency, 64) > 0.05f); // not late either: the kick's 1 ms fade-in, then its body
        CHECK (peak (out, ch, 16 + latency, 2400) > 0.5f);
    }
}

TEST_CASE ("mono output plays both sides folded", "[audio]")
{
    SyntheticPlugin plugin;
    juce::AudioProcessor::BusesLayout mono;
    mono.outputBuses.add (juce::AudioChannelSet::mono());
    REQUIRE (plugin->setBusesLayout (mono));
    REQUIRE (plugin->getTotalNumOutputChannels() == 1);
    test::prepareWithKit (*plugin, sampleRate, blockSize);

    const auto out = test::render (*plugin, 9600, blockSize, midiAt (kick, 0));
    CHECK (out.getNumChannels() == 1);
    CHECK (test::countBad (out) == 0);
    CHECK (peak (out, 0, 0, 9600) > 0.05f);
}

TEST_CASE ("every rate from 11.025 to 192 kHz, with blocks of 1 to 4096 samples", "[audio]")
{
    const auto rate = GENERATE (11025.0, 22050.0, 44100.0, 48000.0, 96000.0, 192000.0);
    CAPTURE (rate);
    SyntheticPlugin plugin;
    test::TestPlayHead head;
    plugin->setPlayHead (&head);
    test::prepareWithKit (*plugin, rate, 4096);

    for (const int block : { 1, 7, 64, 137, 512, 4096 })
    {
        CAPTURE (block);
        prepare (*plugin, rate, block);
        head.play (128.0, 0.0);
        const auto length = (int) (0.25 * rate);
        const auto out = playOn (*plugin, head, length, block, rate, midiAt (kick, 0));
        CHECK (test::countBad (out) == 0);
        CHECK (peak (out, 0, 0, length) > 0.01f);
        CHECK (peak (out, 0, 0, length) < 4.0f);
    }
    plugin->setPlayHead (nullptr);
}

TEST_CASE ("prepare twice without a release in between", "[audio]")
{
    SyntheticPlugin plugin;
    test::prepareWithKit (*plugin, 44100.0, 512);
    process (*plugin, 512, midiAt (kick, 0));

    prepare (*plugin, 96000.0, 256); // no releaseResources ()
    const auto out = test::render (*plugin, 19200, 256, midiAt (kick, 0));
    CHECK (test::countBad (out) == 0);
    CHECK (peak (out, 0, 0, 19200) > 0.05f);
    CHECK (plugin->getLatencySamples() > 0);

    SECTION ("and prepare, release, prepare")
    {
        plugin->releaseResources();
        prepare (*plugin, 48000.0, 64);
        CHECK (test::silent (process (*plugin, 64)));
        CHECK (plugin->controller().snapshot().voices == 0);
    }
}

TEST_CASE ("host reset and bypass leave no voice", "[audio]")
{
    SyntheticPlugin plugin;
    test::prepareWithKit (*plugin, sampleRate, blockSize);

    // A sustained bass note (channel 1, G1: a sustain loop) and a kick.
    juce::MidiBuffer notes;
    notes.addEvent (juce::MidiMessage::noteOn (1, 31, (juce::uint8) 110), 0);
    notes.addEvent (kick, 0);
    process (*plugin, blockSize, notes);
    process (*plugin, blockSize);
    REQUIRE (plugin->controller().snapshot().voices > 0);

    SECTION ("reset () stops every voice and tail before the next block")
    {
        plugin->reset(); // hosts may call this from any thread
        const auto out = process (*plugin, blockSize);
        CHECK (test::silent (out));
        CHECK (plugin->controller().snapshot().voices == 0);
    }

    SECTION ("a note-off while bypassed can't leave the note hanging")
    {
        juce::AudioBuffer<float> buffer (2, blockSize);
        buffer.applyGain (0.0f);
        auto off = midiAt (juce::MidiMessage::noteOff (1, 31), 0);
        plugin->processBlockBypassed (buffer, off);
        CHECK (test::silent (buffer));

        const auto out = process (*plugin, blockSize);
        CHECK (test::silent (out));
        CHECK (plugin->controller().snapshot().voices == 0);
    }
}

TEST_CASE ("a block larger than prepared works, and keeps the host's time", "[audio]")
{
    SyntheticPlugin plugin;
    test::TestPlayHead head;
    plugin->setPlayHead (&head);
    test::prepareWithKit (*plugin, sampleRate, 64);

    test::setParameter (*plugin, ParameterIds::playMode, 1.0f); // Loop
    head.play (128.0, 0.0);
    const auto big = playOn (*plugin, head, 8 * 4096, 4096, sampleRate);

    // The same through blocks of the prepared size.
    SyntheticPlugin other;
    test::TestPlayHead otherHead;
    other->setPlayHead (&otherHead);
    test::prepareWithKit (*other, sampleRate, 64);
    otherHead.play (128.0, 0.0);
    const auto small = playOn (*other, otherHead, 8 * 4096, 64, sampleRate);

    CHECK (test::countBad (big) == 0);
    CHECK (peak (big, 0, 0, big.getNumSamples()) > 0.05f);
    float largest = 0.0f;
    for (int i = 0; i < big.getNumSamples(); ++i)
        largest = std::max (largest, std::abs (big.getSample (0, i) - small.getSample (0, i)));
    CHECK (largest < 1.0e-5f);

    plugin->setPlayHead (nullptr);
    other->setPlayHead (nullptr);
}

TEST_CASE ("master gain", "[audio]")
{
    SyntheticPlugin plugin;

    SECTION ("-60 dB is silence")
    {
        test::setMasterGainDb (*plugin, -60.0f);
        test::prepareWithKit (*plugin, sampleRate, blockSize);
        const auto out = test::render (*plugin, 8 * blockSize, blockSize, midiAt (kick, 0));
        CHECK (test::silent (out));
    }

    SECTION ("changes ramp over about 20 ms instead of jumping")
    {
        test::prepareWithKit (*plugin, sampleRate, blockSize);
        // A held pad (channel 11) rings steadily.
        test::render (*plugin, 8 * blockSize, blockSize, midiAt (juce::MidiMessage::noteOn (11, 55, (juce::uint8) 120), 0));

        test::setMasterGainDb (*plugin, -60.0f);
        const auto out = test::render (*plugin, 4 * blockSize, blockSize);
        const auto rampSamples = (int) (0.02 * sampleRate);

        CHECK (peak (out, 0, 0, 32) > peak (out, 0, rampSamples / 2, 32)); // still fading
        CHECK (peak (out, 0, 0, 32) > 0.001f);
        CHECK (test::silent (out, rampSamples + 1));
    }
}

TEST_CASE ("the play modes: Loop and Song follow the host, Off plays only MIDI in", "[audio]")
{
    SyntheticPlugin plugin;
    test::TestPlayHead head;
    plugin->setPlayHead (&head);
    test::prepareWithKit (*plugin, sampleRate, blockSize);

    SECTION ("Loop")
    {
        test::setParameter (*plugin, ParameterIds::playMode, 1.0f);
        head.play (128.0, 0.0);
        const auto out = playOn (*plugin, head, 96000, blockSize, sampleRate);
        CHECK (peak (out, 0, 0, 96000) > 0.05f);
        const auto snapshot = plugin->controller().snapshot();
        CHECK (snapshot.playing);
        CHECK_THAT (snapshot.bpm, WithinAbs (128.0, 1.0e-9));
        CHECK (snapshot.mode == engine::PlayMode::loop);
        CHECK (snapshot.laneHits != 0u);
        CHECK (snapshot.songTicks >= 0.0);
    }

    SECTION ("Song")
    {
        test::setParameter (*plugin, ParameterIds::playMode, 2.0f);
        head.play (128.0, 0.0);
        const auto out = playOn (*plugin, head, 96000, blockSize, sampleRate);
        CHECK (peak (out, 0, 0, 96000) > 0.05f);
        const auto snapshot = plugin->controller().snapshot();
        CHECK (snapshot.mode == engine::PlayMode::song);
        CHECK (snapshot.section == 0); // the synthetic Song's Intro
    }

    SECTION ("Off")
    {
        test::setParameter (*plugin, ParameterIds::playMode, 0.0f);
        head.play (128.0, 0.0);
        CHECK (test::silent (playOn (*plugin, head, 96000, blockSize, sampleRate)));
        const auto out = playOn (*plugin, head, 9600, blockSize, sampleRate, midiAt (kick, 0));
        CHECK (peak (out, 0, 0, 9600) > 0.05f);
    }

    SECTION ("the host stopped: no sequence")
    {
        test::setParameter (*plugin, ParameterIds::playMode, 1.0f);
        head.stop();
        CHECK (test::silent (playOn (*plugin, head, 48000, blockSize, sampleRate)));
        CHECK_FALSE (plugin->controller().snapshot().playing);
    }

    plugin->setPlayHead (nullptr);
}

TEST_CASE ("the plugin's own transport: Standalone and Preview", "[audio][transport]")
{
    SyntheticPlugin plugin;
    test::TestPlayHead head;
    test::prepareWithKit (*plugin, sampleRate, blockSize);
    auto& controller = plugin->controller();

    SECTION ("no host position (Standalone): play and stop from the editor, at the kit's tempo or the one set")
    {
        head.present = false;
        plugin->setPlayHead (&head);
        CHECK (test::silent (test::render (*plugin, 24000, blockSize)));

        controller.setTransport (true, 0.0);
        auto out = test::render (*plugin, 48000, blockSize);
        CHECK (peak (out, 0, 0, 48000) > 0.05f);
        auto transport = controller.transport();
        CHECK (transport.internal);
        CHECK (transport.playing);
        CHECK_THAT (transport.bpm, WithinAbs (128.0, 1.0e-9)); // SYN01's
        CHECK (juce::exactlyEqual (transport.standaloneBpm, 0.0));

        controller.setTransport (true, 140.0);
        test::render (*plugin, 4800, blockSize);
        transport = controller.transport();
        CHECK_THAT (transport.bpm, WithinAbs (140.0, 1.0e-9));
        CHECK (juce::exactlyEqual (transport.standaloneBpm, 140.0));
        CHECK_THAT (controller.snapshot().bpm, WithinAbs (140.0, 1.0e-9));

        controller.setTransport (false, 140.0);
        process (*plugin, blockSize);
        CHECK_FALSE (controller.transport().playing);
    }

    SECTION ("Preview while the host is stopped; the host's play turns it off")
    {
        plugin->setPlayHead (&head);
        head.stop();
        head.info.setBpm (120.0);
        CHECK (test::silent (test::render (*plugin, 24000, blockSize)));

        controller.setPreview (true);
        CHECK (controller.preview());
        const auto out = test::render (*plugin, 48000, blockSize);
        CHECK (peak (out, 0, 0, 48000) > 0.05f);
        CHECK (controller.transport().internal);
        CHECK_THAT (controller.transport().bpm, WithinAbs (120.0, 1.0e-9)); // the host's tempo

        head.play (120.0, 0.0);
        process (*plugin, blockSize);
        CHECK_FALSE (controller.preview());
        CHECK_FALSE (controller.transport().internal);
        head.stop();
        process (*plugin, blockSize);
        CHECK_FALSE (controller.snapshot().playing);
    }

    plugin->setPlayHead (nullptr);
}

TEST_CASE ("the host is told the engine's latency and tail", "[audio]")
{
    SyntheticPlugin plugin;
    prepare (*plugin, 48000.0, 512);
    CHECK (plugin->getLatencySamples() == 424); // the mix's fixed delay at 48 kHz (ADR 0021)
    prepare (*plugin, 44100.0, 512);
    CHECK (plugin->getLatencySamples() == 404);
    prepare (*plugin, 96000.0, 512);
    CHECK (plugin->getLatencySamples() == 712); // the club clip at 192 kHz inside (ADR 0021)
    CHECK_THAT (plugin->getTailLengthSeconds(), WithinAbs (6.0, 1.0e-9));
}
