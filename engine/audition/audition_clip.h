#pragma once

// The Samples drawer's audition clips (ADR 0024): one content file, decoded and made ready for the output's rate,
// owning its audio. The plugin's sound thread makes them; AuditionPlayer plays them on the audio thread. A clip
// never refers to a Kit, so nothing in the audition path keeps a kit's samples alive.

#include "kit/sample_source.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace wubansia::engine
{
    struct AuditionClip
    {
        std::vector<float> audio; // planar: channel c is audio[c * frames, (c + 1) * frames)
        std::int64_t frames = 0; // at rate
        std::int32_t channels = 0; // 1 or 2, as the file
        double rate = 0.0; // the output rate it was made for
        std::uint32_t ticket = 0; // the request that made it
    };

    // The highest output rate a clip is made for (16 times the content's).
    inline constexpr double kMaxAuditionRate = 768000.0;

    // The file at `rate`: at 48 kHz an exact copy; otherwise ceil (frames x rate / 48000) frames, frame n read with
    // sincRead at n x 48000 / rate through SincBank::instance ()'s bank for the ratio 48000 / rate, as a voice reads
    // a file it plays straight through. Deterministic. nullptr for a file that isn't 48 kHz with 1 or 2 channels of
    // frames x channels samples, or a rate that isn't positive, finite and at most kMaxAuditionRate. It allocates
    // (and builds the sinc bank on first use): the sound thread, never the audio thread.
    std::unique_ptr<AuditionClip> makeAuditionClip (const SampleData& file, double rate, std::uint32_t ticket);
}
