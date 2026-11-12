#include "audition/audition_clip.h"

#include "sampler/sinc_bank.h"

#include <cmath>
#include <cstddef>

namespace wubansia::engine
{
    std::unique_ptr<AuditionClip> makeAuditionClip (const SampleData& file, double rate, std::uint32_t ticket)
    {
        constexpr double contentRate = kContentSampleRate;
        if (file.channels < 1 || file.channels > 2 || file.frames <= 0 || file.sampleRate != kContentSampleRate
            || file.audio.size() != static_cast<std::size_t> (file.frames) * static_cast<std::size_t> (file.channels))
            return nullptr;
        if (! (rate > 0.0) || ! std::isfinite (rate) || rate > kMaxAuditionRate)
            return nullptr;

        auto clip = std::make_unique<AuditionClip>();
        clip->channels = file.channels;
        clip->rate = rate;
        clip->ticket = ticket;

        // At the content's own rate: the file itself, bit for bit.
        if (std::fabs (rate - contentRate) < 1.0e-9)
        {
            clip->frames = file.frames;
            clip->audio = file.audio;
            return clip;
        }

        // Elsewhere: read straight through at 48000 / rate frames per output sample, as a voice reads a file at its root
        // key; frame n at n x ratio (no running sum, so no drift). The last read, (frames' - 1) x ratio, lies inside the file.
        const double ratio = contentRate / rate;
        const auto frames = static_cast<std::int64_t> (std::ceil (static_cast<double> (file.frames) * rate / contentRate));
        clip->frames = frames;
        clip->audio.resize (static_cast<std::size_t> (frames) * static_cast<std::size_t> (file.channels));
        const auto& bank = SincBank::instance();
        const float* table = bank.table (bank.bankFor (ratio));
        for (std::size_t c = 0; c < static_cast<std::size_t> (file.channels); ++c)
        {
            const float* in = file.audio.data() + c * static_cast<std::size_t> (file.frames);
            float* out = clip->audio.data() + c * static_cast<std::size_t> (frames);
            for (std::int64_t n = 0; n < frames; ++n)
                out[n] = sincRead (table, in, file.frames, static_cast<double> (n) * ratio);
        }
        return clip;
    }
}
