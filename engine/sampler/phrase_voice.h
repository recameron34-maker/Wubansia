#pragma once

// A phrase voice (P6): plays a phrase lane's recorded stem, slice by slice, in place of the
// multisample.
// - Contiguous slices read on continuously, within +-2 source frames.
// - It re-syncs at each cycle wrap, with a 1 ms crossfade when a frame or more off.
// - A jump to a non-contiguous slice crossfades over 3 ms.
// - A slice with no follower fades from its fade frame over the phrase's fade_us.
// - After a seek it starts mid-slice (chaseFrames).
// It writes into its phrase's mix channel.
//
// This is P0's stub: it never sounds. P6 replaces this file and adds phrase_voice.cpp.

#include "kit/kit.h"
#include "mix/mixer.h"

#include <cstdint>

namespace wubansia::engine
{
    class PhraseVoice
    {
    public:
        void prepare (double sampleRate) noexcept { rate = sampleRate; }

        // Starts slice `slice` of phrase `phrase` at a sample offset in this block. character picks
        // the stem (0 = A, 1 = B); chaseFrames skips into the slice (a seek).
        void start (const Kit& kit, int phrase, int slice, int character, int offset, std::int64_t chaseFrames) noexcept
        {
            (void) kit;
            (void) phrase;
            (void) slice;
            (void) character;
            (void) offset;
            (void) chaseFrames;
        }

        // The next note's slice, at a sample offset in this block: read on if it follows contiguously.
        void continueWith (int slice, int offset) noexcept
        {
            (void) slice;
            (void) offset;
        }

        // Fades out over fadeSamples from a sample offset in this block.
        void stop (int offset, int fadeSamples) noexcept
        {
            (void) offset;
            (void) fadeSamples;
        }

        void render (MixBus& bus, SendBus& sends, int numSamples) noexcept
        {
            (void) bus;
            (void) sends;
            (void) numSamples;
        }

        bool active() const noexcept { return false; }

    private:
        double rate = 48000.0;
    };
}
