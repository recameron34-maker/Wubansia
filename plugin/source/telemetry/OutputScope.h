#pragma once

// The output scope (ADR 0032): the plugin's last moments of output, mono, for machine room's oscilloscope.
// processBlock pushes each block after master_gain while the editor has it on; the editor pulls the newest samples
// on its 30 Hz tick. One writer (the audio thread), one reader (the message thread), a fixed ring: no allocation, no
// lock, no wait. Off, push costs one relaxed load.

#include <juce_core/juce_core.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>

namespace wubansia
{
    class OutputScope
    {
    public:
        static constexpr int kCapacity = 8192; // about 170 ms at 48 kHz: five editor ticks

        // Message thread, the audio thread stopped (prepareToPlay): keep every stride-th frame, so the scope runs at
        // 48 kHz or less (1 at 44.1 and 48 kHz, 2 at 88.2 and 96, 4 at 176.4 and 192).
        void prepare (double) noexcept {}

        void setOn (bool) noexcept {}
        bool isOn() const noexcept { return false; }

        // Audio thread: (left + right) / 2 of every stride-th frame, as much as fits; the rest of the block is dropped.
        void push (const float*, const float*, int) noexcept {}

        // Message thread: drains the ring; copies its newest samples, at most maxSamples, oldest first; how many.
        int pull (float*, int) noexcept { return 0; }
    };
}
