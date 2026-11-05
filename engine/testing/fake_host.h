#pragma once

// A fake host for engine tests: it drives an Engine block by block the way a plugin host drives the
// processor. It has a transport that moves on by the samples rendered (tempo, play, stop, seek, a loop
// region, no position at all, offline), blocks of any size pattern, and MIDI at absolute sample
// positions delivered in the block that holds them, at their offset. Header-only.
//
// renderBlock () allocates nothing (up to kMaxMidiPerBlock events a block), so a test can count
// allocations around it. The render calls take any engine-like type with Engine::process's signature,
// so a test can also pass a stand-in that records what each block got.
//
//     FakeHost host (48000.0, 512);
//     host.setTempo (128.0);
//     host.play (0.0);
//     host.addMidi (100, 0x99, 36, 100);
//     FakeHost::Output out;
//     host.render (engine, controls, 48000, out, std::array { 1, 17, 512 });

#include "engine.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace wubansia::engine::testing
{
    class FakeHost
    {
    public:
        static constexpr std::size_t kMaxMidiPerBlock = 4096;

        struct Output
        {
            std::vector<float> left;
            std::vector<float> right; // empty with one output
        };

        FakeHost (double sampleRate, int maxBlock, int numOutputs = 2)
            : rate (sampleRate), blockLimit (std::max (1, maxBlock)), outputs (std::clamp (numOutputs, 1, 2))
        {
            blockMidi.reserve (kMaxMidiPerBlock);
        }

        double sampleRate() const noexcept { return rate; }
        int maxBlock() const noexcept { return blockLimit; }
        int numOutputs() const noexcept { return outputs; }

        template <class EngineLike>
        void prepare (EngineLike& engine) const
        {
            engine.prepare (rate, blockLimit, outputs);
        }

        // The transport. bpm 0: the host reports no tempo.
        void setTempo (double bpm) noexcept { tempo = bpm; }
        void play (double fromPpq = 0.0) noexcept
        {
            position = fromPpq;
            playing = true;
        }
        void stop() noexcept { playing = false; }
        void seek (double ppq) noexcept { position = ppq; } // a jump, playing or not
        void setLoop (double startPpq, double endPpq) noexcept
        {
            looping = endPpq > startPpq;
            loopStart = startPpq;
            loopEnd = endPpq;
        }
        void clearLoop() noexcept { looping = false; }
        void setHasPosition (bool has) noexcept { hasPosition = has; } // false: a Standalone-like host
        void setNonRealtime (bool offline) noexcept { nonRealtime = offline; }

        // What the next block sees.
        TransportInfo transport() const noexcept
        {
            TransportInfo t;
            t.hasPosition = hasPosition;
            t.playing = playing;
            t.ppq = hasPosition ? position : 0.0;
            t.bpm = tempo;
            t.looping = looping;
            t.loopStartPpq = loopStart;
            t.loopEndPpq = loopEnd;
            t.nonRealtime = nonRealtime;
            return t;
        }

        double ppq() const noexcept { return position; }
        std::int64_t samplePosition() const noexcept { return samples; } // rendered since construction

        // A 3-byte MIDI message at an absolute sample (counted like samplePosition ()). Messages at one
        // sample keep the order they were added in. One before the next block goes out at its start.
        void addMidi (std::int64_t sample, std::uint8_t status, std::uint8_t data1, std::uint8_t data2)
        {
            const Scheduled event { std::max (sample, samples), { 0, status, data1, data2 } };
            const auto at = std::upper_bound (midi.begin() + static_cast<std::ptrdiff_t> (nextMidi), midi.end(), event,
                [] (const Scheduled& a, const Scheduled& b) { return a.sample < b.sample; });
            midi.insert (at, event);
        }

        // One block of n samples (1 to maxBlock) into out[0..numOutputs). Allocates nothing.
        template <class EngineLike>
        void renderBlock (EngineLike& engine, const Controls& controls, int n, float* const* out)
        {
            n = std::clamp (n, 0, blockLimit);
            blockMidi.clear();
            while (nextMidi < midi.size() && midi[nextMidi].sample < samples + n)
            {
                const auto& scheduled = midi[nextMidi++];
                auto event = scheduled.event;
                event.offset = static_cast<std::int32_t> (std::max<std::int64_t> (0, scheduled.sample - samples));
                if (blockMidi.size() < kMaxMidiPerBlock)
                    blockMidi.push_back (event);
            }
            engine.process (out, outputs, n, transport(), controls, blockMidi.data(), static_cast<int> (blockMidi.size()));
            advance (n);
        }

        // numSamples in blocks whose sizes cycle through blockSizes (maxBlock each when empty), appended
        // to out.
        template <class EngineLike>
        void render (EngineLike& engine, const Controls& controls, std::int64_t numSamples, Output& out, std::span<const int> blockSizes = {})
        {
            const auto start = out.left.size();
            out.left.resize (start + static_cast<std::size_t> (numSamples), 0.0f);
            if (outputs > 1)
                out.right.resize (start + static_cast<std::size_t> (numSamples), 0.0f);

            std::int64_t done = 0;
            std::size_t pattern = 0;
            while (done < numSamples)
            {
                int n = blockSizes.empty() ? blockLimit : blockSizes[pattern++ % blockSizes.size()];
                n = static_cast<int> (std::min<std::int64_t> (std::clamp (n, 1, blockLimit), numSamples - done));
                const auto at = start + static_cast<std::size_t> (done);
                float* channels[2] { out.left.data() + at, outputs > 1 ? out.right.data() + at : nullptr };
                renderBlock (engine, controls, n, channels);
                done += n;
            }
        }

    private:
        struct Scheduled
        {
            std::int64_t sample;
            MidiEvent event;
        };

        // The transport moves on as a host's does: by the block's length at the tempo, wrapping inside
        // the loop region.
        void advance (int n) noexcept
        {
            samples += n;
            if (! playing || tempo <= 0.0)
                return;
            position += static_cast<double> (n) * tempo / (60.0 * rate);
            if (looping && position >= loopEnd)
                position = loopStart + std::fmod (position - loopStart, loopEnd - loopStart);
        }

        double rate;
        int blockLimit;
        int outputs;

        double tempo = 120.0;
        double position = 0.0;
        bool playing = false;
        bool hasPosition = true;
        bool looping = false;
        double loopStart = 0.0;
        double loopEnd = 0.0;
        bool nonRealtime = false;
        std::int64_t samples = 0;

        std::vector<Scheduled> midi; // by sample
        std::size_t nextMidi = 0;
        std::vector<MidiEvent> blockMidi; // reserved, so renderBlock never allocates
    };
}
