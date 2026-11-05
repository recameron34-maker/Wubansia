#pragma once

// The mixer: the buses voices write into, and the mix that turns them into the output (P8; P0's
// mixer_passthrough.cpp sums the channel buses until then). It plays the adopted Kit's MixConfig.
//
// Per block the engine (P7):
// 1. clears the buses (bus ().clear (n), sends ().clear (n));
// 2. lets the voices add into bus () (their mix channel) and sends () (their throws);
// 3. passes duck keys and controllers at their sample offsets (keyHit, cc);
// 4. calls process (), which writes the output.
// Everything but prepare () is noexcept and real-time safe; prepare () allocates every line for
// the largest embedded mix at this rate and 40 BPM, so setConfig () never allocates.

#include "engine.h"
#include "mix/mix_config.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace wubansia::engine
{
    // One stereo buffer per mix channel, maxBlock samples each.
    class MixBus
    {
    public:
        // Allocates numChannels stereo buffers of maxBlock samples, zeroed.
        void prepare (int numChannels, int maxBlock)
        {
            channels = numChannels > 0 ? numChannels : 0;
            stride = maxBlock > 0 ? static_cast<std::size_t> (maxBlock) : 0;
            data.assign (2 * static_cast<std::size_t> (channels) * stride, 0.0f);
        }

        // Zeroes the first numSamples of every buffer.
        void clear (int numSamples) noexcept
        {
            const auto n = numSamples > 0 ? std::min (static_cast<std::size_t> (numSamples), stride) : std::size_t { 0 };
            for (std::size_t b = 0; b < 2 * static_cast<std::size_t> (channels); ++b)
                std::fill_n (data.data() + b * stride, n, 0.0f);
        }

        float* left (int channel) noexcept { return data.data() + static_cast<std::size_t> (2 * channel) * stride; }
        float* right (int channel) noexcept { return left (channel) + stride; }
        const float* left (int channel) const noexcept { return data.data() + static_cast<std::size_t> (2 * channel) * stride; }
        const float* right (int channel) const noexcept { return left (channel) + stride; }

        int numChannels() const noexcept { return channels; }
        int maxBlock() const noexcept { return static_cast<int> (stride); }

    private:
        std::vector<float> data;
        std::size_t stride = 0;
        int channels = 0;
    };

    // The sends' inputs that thrown voices add into directly: one mono buffer per Send, as sends.py
    // takes them (D.mono).
    class SendBus
    {
    public:
        // Allocates a buffer of maxBlock samples per Send, zeroed.
        void prepare (int maxBlock)
        {
            stride = maxBlock > 0 ? static_cast<std::size_t> (maxBlock) : 0;
            data.assign (static_cast<std::size_t> (kSends) * stride, 0.0f);
        }

        // Zeroes the first numSamples of every send.
        void clear (int numSamples) noexcept
        {
            const auto n = numSamples > 0 ? std::min (static_cast<std::size_t> (numSamples), stride) : std::size_t { 0 };
            for (std::size_t s = 0; s < static_cast<std::size_t> (kSends); ++s)
                std::fill_n (data.data() + s * stride, n, 0.0f);
        }

        float* send (Send which) noexcept { return data.data() + static_cast<std::size_t> (which) * stride; }
        const float* send (Send which) const noexcept { return data.data() + static_cast<std::size_t> (which) * stride; }

        int maxBlock() const noexcept { return static_cast<int> (stride); }

    private:
        std::vector<float> data;
        std::size_t stride = 0;
    };

    // What the editor's meters show, from the last process (): each lane's post-fader peak (linear), and
    // the club master's total gain reduction in dB (>= 0).
    struct MixMeters
    {
        std::array<float, kLanes> lanePeak {};
        float gainReduction = 0.0f;
    };

    class Mixer
    {
    public:
        Mixer();
        ~Mixer();
        Mixer (const Mixer&) = delete;
        Mixer& operator= (const Mixer&) = delete;

        void prepare (double sampleRate, int maxBlock, int numOutputs); // allocates
        void reset() noexcept; // clears every tail and envelope

        // The adopted Kit's mix (nullptr: none, which sums the buses at unity). Audio thread, at a
        // kit adoption; the config outlives its use (the Kit does).
        void setConfig (const MixConfig* config) noexcept;

        MixBus& bus() noexcept { return bus_; }
        SendBus& sends() noexcept { return sends_; }

        // A duck key: an onset on keyLane (a Lane value) at a sample offset in this block.
        void keyHit (std::uint8_t keyLane, int velocity, int offset) noexcept;

        // A controller for a mix channel's cc_filter at a sample offset, ramping to value over
        // rampSamples (0: a step).
        void cc (int mixChannel, int controller, int value, int offset, int rampSamples) noexcept;

        // Mixes the buses into out[0..numOutputs), overwriting it. bpm sets the echo's time.
        void process (float* const* out, int numOutputs, int numSamples, const Controls& controls, double bpm) noexcept;

        int latencySamples() const noexcept; // constant after prepare (), whatever the config
        double tailSeconds() const noexcept;

        // The meters of the last process () (zeros from the pass-through mixer).
        const MixMeters& meters() const noexcept;

    private:
        struct Impl; // the mixer's own: mixer_passthrough.cpp's, then P8's
        std::unique_ptr<Impl> impl;
        MixBus bus_;
        SendBus sends_;
    };
}
