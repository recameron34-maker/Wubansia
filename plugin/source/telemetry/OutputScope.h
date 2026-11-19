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
        void prepare (double sampleRate) noexcept
        {
            stride = std::max (1, (int) std::lround (sampleRate / 48000.0));
            phase = 0;
        }

        void setOn (bool on) noexcept { wanted.store (on, std::memory_order_relaxed); }
        bool isOn() const noexcept { return wanted.load (std::memory_order_relaxed); }

        // Audio thread: (left + right) / 2 of every stride-th frame, as much as fits; the rest of the block is dropped.
        void push (const float* left, const float* right, int numFrames) noexcept
        {
            if (! isOn() || numFrames <= 0)
                return;
            const auto kept = phase < numFrames ? (numFrames - phase + stride - 1) / stride : 0;
            const auto scope = fifo.write (std::min (kept, fifo.getFreeSpace()));
            auto frame = phase;
            const auto copy = [&] (int start, int size)
            {
                for (int i = 0; i < size; ++i, frame += stride)
                    ring[(std::size_t) (start + i)] = 0.5f * (left[frame] + right[frame]);
            };
            copy (scope.startIndex1, scope.blockSize1);
            copy (scope.startIndex2, scope.blockSize2);
            phase = phase + kept * stride - numFrames; // frames into the next block before the next kept one
        }

        // Message thread: drains the ring; copies its newest samples, at most maxSamples, oldest first; how many.
        int pull (float* dest, int maxSamples) noexcept
        {
            const auto ready = fifo.getNumReady();
            const auto skip = std::max (0, ready - std::max (0, maxSamples));
            const auto scope = fifo.read (ready);
            int written = 0, index = 0;
            const auto copy = [&] (int start, int size)
            {
                for (int i = 0; i < size; ++i, ++index)
                    if (index >= skip)
                        dest[written++] = ring[(std::size_t) (start + i)];
            };
            copy (scope.startIndex1, scope.blockSize1);
            copy (scope.startIndex2, scope.blockSize2);
            return written;
        }

    private:
        juce::AbstractFifo fifo { kCapacity + 1 }; // AbstractFifo holds one less than its size
        std::array<float, kCapacity + 1> ring {};
        std::atomic<bool> wanted { false };
        int stride = 1; // audio thread after prepare
        int phase = 0; // audio thread: frames into the next block before the next kept one
    };
}
