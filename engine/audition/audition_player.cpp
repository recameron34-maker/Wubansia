#include "audition/audition_player.h"

namespace wubansia::engine
{
    AuditionPlayer::~AuditionPlayer()
    {
        delete pending.take();
        delete current.clip;
        delete fading.clip;
        while (const AuditionClip* clip = retired.pop())
            delete clip;
    }

    void AuditionPlayer::prepare (double sampleRate) noexcept { rate_.store (sampleRate, std::memory_order_release); }

    double AuditionPlayer::rate() const noexcept { return rate_.load (std::memory_order_acquire); }

    void AuditionPlayer::stopUpTo (std::uint32_t) noexcept {}

    void AuditionPlayer::process (float* const*, int, int, bool) noexcept {} // not yet

    void AuditionPlayer::reset() noexcept {}

    std::uint32_t AuditionPlayer::sounding() const noexcept { return soundingTicket.load (std::memory_order_acquire); }

    bool AuditionPlayer::offered() const noexcept { return pending.pending(); }

    void AuditionPlayer::letGo (Slot&) noexcept {}

    void AuditionPlayer::letGoAll() noexcept {}

    void AuditionPlayer::render (Slot&, float*, float*, int) noexcept {}
}
