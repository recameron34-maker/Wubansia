// pluginval's sharpest checks (strictness 10, ADR 0004), run in ctest on the synthetic kit so they fail
// here first: random automation every 32 samples, parameters set from another thread while blocks run,
// parameters set from a background thread with the editor open, state from a background thread with
// the editor open, noise-filled buffers at every rate, and never a NaN, an infinity or a subnormal. The
// byte-for-byte state restoration is in StateTests, larger-than-prepared blocks in AudioTests. A host's
// parameter set reaches the parameter's listeners (JUCE's wrappers send it on), so these set values
// with setValueNotifyingHost, which does the same.

#include "helpers/test_helpers.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <atomic>
#include <thread>

using namespace wubansia;
using test::SyntheticContent;
using test::SyntheticPlugin;

namespace
{
    void fillNoise (juce::AudioBuffer<float>& buffer, juce::Random& random)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                buffer.setSample (ch, i, random.nextFloat() * 2.0f - 1.0f);
    }

    juce::Array<juce::AudioProcessorParameter*> parameters (PluginProcessor& p) { return p.getParameters(); }
}

TEST_CASE ("pluginval's automation: 10 random parameters every 32 samples, at 44.1, 48 and 96 kHz", "[validator]")
{
    const auto rate = GENERATE (44100.0, 48000.0, 96000.0);
    CAPTURE (rate);
    SyntheticPlugin plugin;
    test::TestPlayHead head;
    plugin->setPlayHead (&head);
    test::prepareWithKit (*plugin, rate, 1024);
    head.play (128.0, 0.0);
    juce::Random random (1024);

    for (const int blockSize : { 64, 256, 1024 })
    {
        test::prepare (*plugin, rate, blockSize);
        juce::AudioBuffer<float> block (2, blockSize);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (random.nextInt ({ 1, 17 }), random.nextInt (128), (juce::uint8) 100), 10);
        for (int round = 0; round < 40; ++round)
        {
            fillNoise (block, random);
            for (int start = 0; start < blockSize; start += 32)
            {
                for (int i = 0; i < 10; ++i)
                    parameters (*plugin)[random.nextInt (parameters (*plugin).size())]->setValueNotifyingHost (random.nextFloat());
                const auto n = std::min (32, blockSize - start);
                juce::AudioBuffer<float> sub (block.getArrayOfWritePointers(), 2, start, n);
                plugin->processBlock (sub, midi);
                midi.clear();
                head.advance (n, rate);
            }
            REQUIRE (test::countBad (block) == 0);
        }
    }
    plugin->setPlayHead (nullptr);
}

TEST_CASE ("pluginval's audio processing: noise in, 10 blocks at every rate and block size, a random note", "[validator]")
{
    SyntheticPlugin plugin;
    test::prepareWithKit (*plugin, 44100.0, 64);
    juce::Random random (17);

    for (const double rate : { 44100.0, 48000.0, 96000.0 })
        for (const int blockSize : { 64, 128, 256, 512, 1024 })
        {
            CAPTURE (rate, blockSize);
            plugin->releaseResources();
            test::prepare (*plugin, rate, blockSize);
            juce::AudioBuffer<float> buffer (2, blockSize);
            juce::MidiBuffer midi;
            const auto channel = random.nextInt ({ 1, 17 });
            const auto note = random.nextInt (128);
            midi.addEvent (juce::MidiMessage::noteOn (channel, note, (juce::uint8) 100), std::min (10, blockSize - 1));
            for (int i = 0; i < 10; ++i)
            {
                if (i == 9)
                    midi.addEvent (juce::MidiMessage::noteOff (channel, note), 0);
                fillNoise (buffer, random);
                plugin->processBlock (buffer, midi);
                midi.clear();
                REQUIRE (test::countBad (buffer) == 0);
            }
        }
}

TEST_CASE ("pluginval's parameter thread safety: every parameter set from another thread while 500 blocks of 32 run", "[validator]")
{
    SyntheticPlugin plugin;
    test::TestPlayHead head;
    plugin->setPlayHead (&head);
    test::prepareWithKit (*plugin, 44100.0, 32);
    head.play (128.0, 0.0);

    std::atomic<bool> started { false };
    std::thread ui ([&] {
        juce::Random random (500);
        started = true;
        for (int i = 0; i < 500; ++i)
            for (auto* parameter : parameters (*plugin))
                parameter->setValueNotifyingHost (random.nextFloat());
    });
    while (! started)
        std::this_thread::yield();

    juce::Random random (32);
    juce::AudioBuffer<float> buffer (2, 32);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (2, 60, (juce::uint8) 100), 10);
    int bad = 0;
    for (int i = 0; i < 500; ++i)
    {
        if (i == 499)
            midi.addEvent (juce::MidiMessage::noteOff (2, 60), 0);
        for (auto* parameter : parameters (*plugin))
            parameter->setValueNotifyingHost (random.nextFloat()); // a host's automation, on the audio thread
        fillNoise (buffer, random);
        plugin->processBlock (buffer, midi);
        midi.clear();
        head.advance (32, 44100.0);
        bad += test::countBad (buffer);
    }
    ui.join();
    CHECK (bad == 0);
    plugin->setPlayHead (nullptr);
}

TEST_CASE ("pluginval's editor automation: setValue from a background thread with the editor open", "[validator][editor]")
{
    SyntheticPlugin plugin;
    test::prepareWithKit (*plugin, 48000.0, 512);
    test::withEditor (*plugin, [&] (juce::AudioProcessorEditor& editor) {
        std::thread host ([&] {
            juce::Random random (5);
            for (int round = 0; round < 200; ++round)
                for (auto* parameter : parameters (*plugin))
                    parameter->setValueNotifyingHost (random.nextFloat());
        });
        // The message thread keeps running the editor meanwhile.
        for (int i = 0; i < 20; ++i)
        {
            juce::MessageManager::getInstance()->runDispatchLoopUntil (5);
            juce::Image image (juce::Image::ARGB, editor.getWidth(), editor.getHeight(), true, juce::SoftwareImageType());
            juce::Graphics g (image);
            editor.paintEntireComponent (g, false);
        }
        host.join();
    });
    CHECK (test::countBad (test::process (*plugin, 512)) == 0);
}

TEST_CASE ("pluginval's background thread state: the state set from another thread with the editor open", "[validator][editor][state]")
{
    SyntheticContent::Options options;
    options.ids = { "BGS01", "BGS02" };
    SyntheticPlugin plugin (options);
    test::prepareWithKit (*plugin, 48000.0, 512);

    juce::MemoryBlock state;
    plugin->getStateInformation (state);
    test::withEditor (*plugin, [&] (juce::AudioProcessorEditor&) {
        juce::Random random (9);
        for (auto* parameter : parameters (*plugin))
            parameter->setValueNotifyingHost (random.nextFloat());
        plugin->controller().selectKit ("BGS02");
        std::thread host ([&] { plugin->setStateInformation (state.getData(), (int) state.getSize()); });
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        host.join();
    });

    juce::MemoryBlock after;
    plugin->getStateInformation (after);
    CHECK (after == state);
    CHECK (test::waitForStatus (*plugin, ContentState::ready));
}
