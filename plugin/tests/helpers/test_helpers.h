#pragma once

// What the processor tests share: a processor on the synthetic kit, host-like calls (prepare, a block,
// blocks), waits for the kit service, and a play head a test steers.

#include "ParameterIds.h"
#include "PluginProcessor.h"
#include "helpers/allocation_counter.h" // WUBANSIA_SANITIZED
#include "helpers/synthetic_content.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <chrono>
#include <cmath>
#include <functional>
#include <memory>
#include <thread>

namespace wubansia::test
{
    using namespace std::chrono_literals;

    // A time limit for a test, ten times longer under a sanitizer (TSan slows everything 5-15 times).
    inline constexpr std::chrono::milliseconds limit (std::chrono::milliseconds ms) { return WUBANSIA_SANITIZED ? ms * 10 : ms; }

    // A processor whose kits are the synthetic content's, playing its first kit, and that content.
    struct SyntheticPlugin
    {
        explicit SyntheticPlugin (SyntheticContent::Options options = {})
        {
            const juce::String first (options.ids.empty() ? std::string() : options.ids.front());
            auto owned = std::make_unique<SyntheticContent> (std::move (options));
            content = owned.get();
            processor = std::make_unique<PluginProcessor>();
            processor->setContentForTesting (std::move (owned), first);
        }

        PluginProcessor& operator*() { return *processor; }
        PluginProcessor* operator->() { return processor.get(); }

        std::unique_ptr<PluginProcessor> processor;
        SyntheticContent* content = nullptr; // owned by the processor
    };

    // What a host wrapper does before playback.
    inline void prepare (PluginProcessor& p, double sampleRate, int blockSize)
    {
        p.setRateAndBufferSizeDetails (sampleRate, blockSize);
        p.prepareToPlay (sampleRate, blockSize);
    }

    inline juce::RangedAudioParameter& parameter (PluginProcessor& p, const juce::String& id)
    {
        auto* parameter = p.apvts.getParameter (id);
        jassert (parameter != nullptr);
        return *parameter;
    }

    // As a host's automation does, notifying the host.
    inline void setParameter (PluginProcessor& p, const juce::String& id, float plainValue)
    {
        auto& param = parameter (p, id);
        param.setValueNotifyingHost (param.convertTo0to1 (plainValue));
    }

    inline float parameterValue (PluginProcessor& p, const juce::String& id) { return p.apvts.getRawParameterValue (id)->load(); }

    inline juce::RangedAudioParameter& masterGainParameter (PluginProcessor& p) { return parameter (p, ParameterIds::masterGain); }
    inline float masterGainDb (PluginProcessor& p) { return parameterValue (p, ParameterIds::masterGain); }
    inline void setMasterGainDb (PluginProcessor& p, float db) { setParameter (p, ParameterIds::masterGain, db); }

    // Runs one block with the given MIDI and returns the rendered audio.
    inline juce::AudioBuffer<float> process (PluginProcessor& p, int numSamples, juce::MidiBuffer midi = {})
    {
        juce::AudioBuffer<float> buffer (p.getTotalNumOutputChannels(), numSamples);
        p.processBlock (buffer, midi);
        return buffer;
    }

    // numSamples in blocks of blockSize; midi's events (at positions from the first sample) go into the
    // blocks that hold them.
    inline juce::AudioBuffer<float> render (PluginProcessor& p, int numSamples, int blockSize, const juce::MidiBuffer& midi = {})
    {
        juce::AudioBuffer<float> out (p.getTotalNumOutputChannels(), numSamples);
        out.clear();
        for (int start = 0; start < numSamples; start += blockSize)
        {
            const auto n = std::min (blockSize, numSamples - start);
            juce::MidiBuffer blockMidi;
            for (const auto m : midi)
                if (m.samplePosition >= start && m.samplePosition < start + n)
                    blockMidi.addEvent (m.data, m.numBytes, m.samplePosition - start);
            auto block = process (p, n, blockMidi);
            for (int ch = 0; ch < out.getNumChannels(); ++ch)
                out.copyFrom (ch, start, block, ch, 0, n);
        }
        return out;
    }

    inline juce::MidiBuffer midiAt (const juce::MidiMessage& message, int samplePosition)
    {
        juce::MidiBuffer midi;
        midi.addEvent (message, samplePosition);
        return midi;
    }

    // Polls until done () or the timeout; whether done () came true.
    inline bool waitFor (const std::function<bool()>& done, std::chrono::milliseconds timeout = 5000ms)
    {
        const auto end = std::chrono::steady_clock::now() + limit (timeout);
        while (! done())
        {
            if (std::chrono::steady_clock::now() > end)
                return false;
            std::this_thread::sleep_for (1ms);
        }
        return true;
    }

    // Waits for the current kit's load to end in state.
    inline bool waitForStatus (PluginProcessor& p, ContentState state, std::chrono::milliseconds timeout = 5000ms)
    {
        return waitFor ([&] { return ! p.kitService().loadInFlight() && p.controller().status().state == state; }, timeout);
    }

    // Waits for the current kit's load, then plays silent blocks until the engine has adopted the kit
    // with this catalog index (with no MIDI: the transport is whatever it is). Whether it got there.
    inline bool waitForKit (PluginProcessor& p, int kitIndex = 0, std::chrono::milliseconds timeout = 5000ms)
    {
        if (! waitFor ([&] { return ! p.kitService().loadInFlight(); }, timeout))
            return false;
        const auto blockSize = juce::jmax (1, p.getBlockSize());
        for (int i = 0; i < 2000; ++i)
        {
            if (p.telemetry().kitIndex.load() == kitIndex)
                return true;
            process (p, blockSize);
        }
        return p.telemetry().kitIndex.load() == kitIndex;
    }

    // A prepared processor on the synthetic kit, its kit adopted.
    inline void prepareWithKit (PluginProcessor& p, double sampleRate = 48000.0, int blockSize = 512)
    {
        prepare (p, sampleRate, blockSize);
        const bool adopted = waitForKit (p);
        jassert (adopted);
        juce::ignoreUnused (adopted);
    }

    // A host's play head that a test steers: a position, a tempo, play and stop, a loop.
    class TestPlayHead final : public juce::AudioPlayHead
    {
    public:
        juce::Optional<PositionInfo> getPosition() const override
        {
            if (! present)
                return {};
            return info;
        }

        void play (double bpm, double ppq = 0.0)
        {
            info.setIsPlaying (true);
            info.setBpm (bpm);
            info.setPpqPosition (ppq);
        }

        void stop() { info.setIsPlaying (false); }

        // Moves the position on by numSamples at the play head's tempo, as a host does between blocks.
        void advance (int numSamples, double sampleRate)
        {
            if (! info.getIsPlaying())
                return;
            const auto bpm = info.getBpm().orFallback (120.0);
            info.setPpqPosition (info.getPpqPosition().orFallback (0.0) + numSamples / sampleRate * bpm / 60.0);
        }

        PositionInfo info;
        bool present = true;
    };

    // Creates the editor as a host does (without a native window), runs the test code, then deletes it.
    inline void withEditor (PluginProcessor& p, const std::function<void (juce::AudioProcessorEditor&)>& testCode)
    {
        auto* editor = p.createEditorAndMakeActive();
        jassert (editor != nullptr);
        testCode (*editor);
        p.editorBeingDeleted (editor);
        delete editor;
    }

    inline bool allFinite (const juce::AudioBuffer<float>& buffer)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                if (! std::isfinite (buffer.getSample (ch, i)))
                    return false;
        return true;
    }

    // NaNs, infinities and subnormals, as pluginval counts them.
    inline int countBad (const juce::AudioBuffer<float>& buffer)
    {
        int bad = 0;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                const auto x = buffer.getSample (ch, i);
                bad += (std::isnan (x) || std::isinf (x) || std::fpclassify (x) == FP_SUBNORMAL) ? 1 : 0;
            }
        return bad;
    }

    // The largest magnitude in [start, start + length) of every channel.
    inline float peakOf (const juce::AudioBuffer<float>& buffer, int start = 0, int length = -1)
    {
        if (length < 0)
            length = buffer.getNumSamples() - start;
        length = std::min (length, buffer.getNumSamples() - start);
        float peak = 0.0f;
        for (int ch = 0; ch < buffer.getNumChannels() && length > 0; ++ch)
            peak = std::max (peak, buffer.getMagnitude (ch, start, length));
        return peak;
    }

    // Quiet: below -120 dBFS. The club master's linear-phase oversamplers ring a little ahead of a hit
    // (under -150 dBFS), so "nothing before the latency" means quiet, not exactly zero.
    inline constexpr float kQuiet = 1.0e-6f;

    // Whether every sample in [start, start + length) of every channel is exactly zero.
    inline bool silent (const juce::AudioBuffer<float>& buffer, int start = 0, int length = -1)
    {
        if (length < 0)
            length = buffer.getNumSamples() - start;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            for (int i = start; i < start + length; ++i)
                if (! juce::exactlyEqual (buffer.getSample (ch, i), 0.0f))
                    return false;
        return true;
    }
}
