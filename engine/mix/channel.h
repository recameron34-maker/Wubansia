#pragma once

// A mix channel's strip in the graph (ADR 0021; docs/plugin/manifest.md 3.1): its inserts, then the delay that
// aligns it with the slowest channel, then its lane's level and mute (10 ms), then its group (or the mix bus) and,
// post-fader, its three sends. And staticGain (), which the voices' direct sends (CC91 and CC94 throws) apply.

#include "engine.h"
#include "mix/inserts.h"
#include "mix/mix_config.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace wubansia::engine::mix
{
    // A value that follows its target along a straight line over a fixed time (10 ms), per sample, so the same
    // targets give the same samples whatever the block size.
    class Ramp
    {
    public:
        void setup (double sampleRate, double seconds = 0.010) noexcept
        {
            length = std::max (1, static_cast<int> (std::nearbyint (sampleRate * seconds)));
        }

        // Jumps to v at once.
        void jump (double v) noexcept
        {
            value = target = v;
            left = 0;
        }

        // Starts the line to v when v is a new target.
        void moveTo (double v) noexcept
        {
            if (! (v < target) && ! (target < v))
                return;
            target = v;
            left = length;
            step = (target - value) / static_cast<double> (length);
        }

        double next() noexcept
        {
            if (left > 0)
                value = --left == 0 ? target : value + step;
            return value;
        }

        double current() const noexcept { return value; }
        bool moving() const noexcept { return left > 0; }

    private:
        double value = 0.0, target = 0.0, step = 0.0;
        int left = 0;
        int length = 480;
    };

    // The channel's static gain: the product of its `gain` inserts active for this variant and these A/B sides (the
    // part of the channel a voice's direct send passes through before it is sent, post-fader, like the channel's
    // own send). Parallel chains and every other insert count as unity.
    float staticGain (const Channel& channel, Variant variant, const std::array<std::uint8_t, kAbSwitches>& ab) noexcept;

    struct ChannelStrip
    {
        int config = -1; // index into MixConfig::channels
        int firstSlot = 0, slotCount = 0;
        int latency = 0; // through its inserts
        std::uint8_t lane = 0;
        Group group = Group::none;
        std::array<double, kSends> sends {}; // post-fader levels, linear
        bool key = false; // a level duck reads its post-insert signal
        AlignDelay align;
        Ramp level;
    };
}
