#include "audition/audition_player.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <initializer_list>

namespace wubansia::engine
{
    namespace
    {
        // The fade-out's length at a rate, in samples: kFadeSeconds, at least one.
        int fadeSamplesAt (double rate) noexcept
        {
            return rate > 0.0 ? std::max (1, static_cast<int> (std::lround (AuditionPlayer::kFadeSeconds * rate))) : 1;
        }

        // Whether a clip was made for this rate: exactly the rate the player was prepared with.
        bool madeFor (const AuditionClip& clip, double rate) noexcept { return std::equal_to<double> {}(clip.rate, rate); }
    }

    AuditionPlayer::~AuditionPlayer()
    {
        delete pending.take();
        delete current.clip;
        delete fading.clip;
        while (const AuditionClip* clip = retired.pop())
            delete clip;
    }

    void AuditionPlayer::prepare (double sampleRate) noexcept
    {
        rate_.store (sampleRate > 0.0 && std::isfinite (sampleRate) ? sampleRate : 0.0, std::memory_order_release);
    }

    double AuditionPlayer::rate() const noexcept { return rate_.load (std::memory_order_acquire); }

    void AuditionPlayer::stopUpTo (std::uint32_t ticket) noexcept
    {
        auto seen = stopTicket.load (std::memory_order_relaxed);
        while (seen < ticket && ! stopTicket.compare_exchange_weak (seen, ticket, std::memory_order_acq_rel, std::memory_order_relaxed))
        {
        }
    }

    std::uint32_t AuditionPlayer::sounding() const noexcept { return soundingTicket.load (std::memory_order_acquire); }

    bool AuditionPlayer::offered() const noexcept { return pending.pending(); }

    void AuditionPlayer::letGo (Slot& slot) noexcept
    {
        if (slot.clip == nullptr)
            return;
        if (retired.push (slot.clip))
            slot = Slot {};
        else
            slot.done = true; // held, silent, until the FIFO has room
    }

    void AuditionPlayer::letGoAll() noexcept
    {
        letGo (current);
        letGo (fading);
        if (retired.hasSpace())
            if (const AuditionClip* next = pending.take())
                retired.push (next); // never played; hasSpace () promised the room
        soundingTicket.store (0, std::memory_order_release);
    }

    void AuditionPlayer::reset() noexcept
    {
        letGoAll();
    }

    void AuditionPlayer::process (float* const* out, int numOutputs, int numSamples, bool silent) noexcept
    {
        // A clip that ended while the FIFO was full goes first, if there's room now.
        if (current.done)
            letGo (current);
        if (fading.done)
            letGo (fading);

        if (silent || out == nullptr || numOutputs <= 0 || out[0] == nullptr)
        {
            letGoAll();
            return;
        }

        // After a prepare at another rate, what was made for the old one can't play.
        const double outputRate = rate_.load (std::memory_order_acquire);
        for (Slot* slot : { &current, &fading })
            if (slot->clip != nullptr && ! slot->done && ! madeFor (*slot->clip, outputRate))
                letGo (*slot);

        // The offer: taken only while no replaced clip still fades out and the FIFO has room, so whatever happens next
        // a clip can be let go. (With room, nothing above is held: a held clip means a full FIFO.)
        const AuditionClip* next = nullptr;
        if (fading.clip == nullptr && retired.hasSpace())
            next = pending.take();
        const auto stop = stopTicket.load (std::memory_order_acquire); // read after the take: a stop made before the offer covers it
        const int fadeLength = fadeSamplesAt (outputRate);
        if (next != nullptr)
        {
            if (! madeFor (*next, outputRate) || next->ticket <= stop)
                retired.push (next); // never played
            else
            {
                if (current.clip != nullptr) // replaced: it fades out from where it is (on with its fade, if stopped)
                {
                    fading = current;
                    if (fading.fadeLength == 0)
                    {
                        fading.fadeLength = fadeLength;
                        fading.fadeLeft = fadeLength;
                    }
                }
                current = Slot {};
                current.clip = next;
            }
        }

        // A stopped ticket: the playing clip fades out where it is, from this block's first sample.
        if (current.clip != nullptr && ! current.done && current.fadeLength == 0 && current.clip->ticket <= stop)
        {
            current.fadeLength = fadeLength;
            current.fadeLeft = fadeLength;
        }

        // The clip fading out first, then the one playing.
        float* left = out[0];
        float* right = numOutputs > 1 ? out[1] : nullptr;
        if (fading.clip != nullptr && ! fading.done)
            render (fading, left, right, numSamples);
        if (current.clip != nullptr && ! current.done)
            render (current, left, right, numSamples);

        const bool playing = current.clip != nullptr && ! current.done && current.fadeLength == 0;
        soundingTicket.store (playing ? current.clip->ticket : 0, std::memory_order_release);
    }

    void AuditionPlayer::render (Slot& slot, float* left, float* right, int numSamples) noexcept
    {
        const AuditionClip& clip = *slot.clip;
        const bool fadingOut = slot.fadeLength > 0;
        auto count = std::min<std::int64_t> (numSamples, clip.frames - slot.position);
        if (fadingOut)
            count = std::min<std::int64_t> (count, slot.fadeLeft);
        const auto n = static_cast<int> (std::max<std::int64_t> (count, 0));

        const float* a = clip.audio.data() + slot.position;
        const float* b = clip.channels > 1 ? a + clip.frames : a; // a mono clip: the same on both sides
        const float length = static_cast<float> (slot.fadeLength);
        const int fadeLeft = slot.fadeLeft;
        const auto gain = [fadingOut, length, fadeLeft] (int i) noexcept { return fadingOut ? static_cast<float> (fadeLeft - i) / length : 1.0f; };
        if (right != nullptr)
        {
            for (int i = 0; i < n; ++i)
            {
                const float g = gain (i);
                left[i] += kGain * (a[i] * g);
                right[i] += kGain * (b[i] * g);
            }
        }
        else if (clip.channels > 1)
        {
            for (int i = 0; i < n; ++i)
                left[i] += kGain * ((0.5f * (a[i] + b[i])) * gain (i)); // the engine's fold
        }
        else
        {
            for (int i = 0; i < n; ++i)
                left[i] += kGain * (a[i] * gain (i));
        }

        slot.position += n;
        if (fadingOut)
            slot.fadeLeft -= n;
        if (slot.position >= clip.frames || (fadingOut && slot.fadeLeft <= 0))
            letGo (slot); // ended, or faded out
    }
}
