// P0's pass-through mixer: each channel bus times its lane's gain, summed into the output; no inserts,
// sends, returns or master, no latency and no tail. P8 replaces this file with mixer.cpp.

#include "mix/mixer.h"

#include <algorithm>

namespace wubansia::engine
{
    struct Mixer::Impl
    {
        const MixConfig* config = nullptr;
        int maxBlock = 0;
    };

    Mixer::Mixer() : impl (std::make_unique<Impl>()) {}

    Mixer::~Mixer() = default;

    void Mixer::prepare (double sampleRate, int maxBlock, int numOutputs)
    {
        (void) sampleRate;
        (void) numOutputs;
        impl->maxBlock = std::max (1, maxBlock);
        bus_.prepare (kMaxMixChannels, impl->maxBlock);
        sends_.prepare (impl->maxBlock);
    }

    void Mixer::reset() noexcept {}

    void Mixer::setConfig (const MixConfig* config) noexcept
    {
        impl->config = config;
    }

    void Mixer::keyHit (std::uint8_t keyLane, int velocity, int offset) noexcept
    {
        (void) keyLane;
        (void) velocity;
        (void) offset;
    }

    void Mixer::cc (int mixChannel, int controller, int value, int offset, int rampSamples) noexcept
    {
        (void) mixChannel;
        (void) controller;
        (void) value;
        (void) offset;
        (void) rampSamples;
    }

    void Mixer::process (float* const* out, int numOutputs, int numSamples, const Controls& controls, double bpm) noexcept
    {
        (void) bpm;
        const int n = std::clamp (numSamples, 0, impl->maxBlock);
        if (numOutputs < 1 || n == 0)
            return;

        float* left = out[0];
        float* right = numOutputs > 1 ? out[1] : nullptr;
        std::fill_n (left, n, 0.0f);
        if (right != nullptr)
            std::fill_n (right, n, 0.0f);

        // Without a config every bus plays at unity; with one, only its channels, at their lane's gain.
        const auto* config = impl->config;
        const int channels = config != nullptr ? std::min (static_cast<int> (config->channels.size()), bus_.numChannels())
                                               : bus_.numChannels();

        for (int c = 0; c < channels; ++c)
        {
            float gain = 1.0f;
            if (config != nullptr)
            {
                const auto lane = config->channels[static_cast<std::size_t> (c)].lane;
                gain = lane < kLanes ? controls.laneGain[lane] : 0.0f;
            }

            const float* busLeft = bus_.left (c);
            const float* busRight = bus_.right (c);

            if (right != nullptr)
            {
                for (int i = 0; i < n; ++i)
                {
                    left[i] += busLeft[i] * gain;
                    right[i] += busRight[i] * gain;
                }
            }
            else
            {
                for (int i = 0; i < n; ++i)
                    left[i] += (busLeft[i] + busRight[i]) * 0.5f * gain;
            }
        }
    }

    int Mixer::latencySamples() const noexcept
    {
        return 0;
    }

    double Mixer::tailSeconds() const noexcept
    {
        return 0.0;
    }
}
