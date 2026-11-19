#pragma once

// The Phrases drawer's audition clips (ADR 0031): one phrase file as the scheduler plays it, made for one Kit, and
// the hand-off that takes it to the audio thread, where BlockLoop's overlay plays it over the groove in time with
// the main scheduler (engine_loop.h).
//
// Threads, as the Kits' (engine.h):
// - the phrase thread is the only producer: it makes clips (makePhraseClip), offers them (latest wins), and keeps
//   each one until it comes back: from offer ()'s return (an offer the audio thread never took) or from
//   collectRetired (). The Engine never deletes a clip; a clip it still holds when it is destroyed is the phrase
//   thread's to delete afterwards;
// - any thread stops tickets (stopUpTo);
// - the audio thread takes an offer, plays it, and lets it go into the retire FIFO (stopped, replaced, for another
//   Kit, a kit switch, offline, a reset, the idle drop, a bypassed block's dropOffer).

#include "kit/manifest.h"
#include "mix/mix_config.h"
#include "rt/pending_slot.h"
#include "rt/retire_fifo.h"
#include "sched/timeline.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <span>

namespace wubansia::engine
{
    struct PhraseClip
    {
        Timeline timeline; // the phrase as the scheduler plays it: a loop of its bars, no phrase slices
        std::int32_t kitIndex = -1; // KitInfo::index of the Kit it was built for; another Kit lets it go unplayed
        std::uint32_t lanes = 0; // bit per Lane its notes play on: those lanes' timeline notes sit out while it sounds
        std::int64_t alignH = 0; // milliticks: bar 1 starts on a multiple of this (the kit's chord line)
        std::uint32_t ticket = 0; // the request that made it
    };

    // Service thread: buildTimeline (smf, kit, mix, { loopTicks = the file's bars x 3840, phrases = false }), the
    // lanes its notes play on (either side of an A/B pair), alignTicks (960 PPQ) as milliticks. nullptr if the file
    // doesn't build, holds no note the keymap plays, or alignTicks isn't positive.
    std::unique_ptr<PhraseClip> makePhraseClip (std::span<const std::uint8_t> smf, const KitManifest& kit, const MixConfig& mix,
        std::int32_t kitIndex, std::int64_t alignTicks, std::uint32_t ticket);

    // The hand-off between the phrase thread and the audio thread.
    class PhraseHandoff
    {
    public:
        static constexpr std::size_t kRetireSlots = 8;

        // The phrase thread. nullptr withdraws an offer. Returns the earlier offer if the audio thread never took it.
        const PhraseClip* offer (const PhraseClip* clip) noexcept { return pending.publish (clip); }

        // The phrase thread: f (const PhraseClip*) for each clip the audio thread let go, oldest first.
        template <class F>
        void collectRetired (F&& f) noexcept
        {
            while (const PhraseClip* clip = retired.pop())
                f (clip);
        }

        // Any thread: clips with a ticket <= this stop (a sounding one fades out) or never play. Tickets only grow.
        void stopUpTo (std::uint32_t ticket) noexcept;
        std::uint32_t stopTicket() const noexcept { return stopTicket_.load (std::memory_order_acquire); }

        // Any thread: blocks the engine has run, bypassed ones not counted.
        std::uint64_t blocksRun() const noexcept { return blocks_.load (std::memory_order_acquire); }

        // The phrase thread, once no block has run for a while: if the next block is the first since `seen` blocks had
        // run, the clip in hand goes at its start, unplayed (never the offer). A block in between changes nothing.
        void dropIfIdleSince (std::uint64_t seen) noexcept { dropAt_.store (seen + 1, std::memory_order_release); }

        // The audio thread (BlockLoop), at a block's start: counts it; true if the idle drop is due.
        bool countBlock() noexcept
        {
            const auto ran = blocks_.load (std::memory_order_relaxed);
            const bool drop = dropAt_.load (std::memory_order_acquire) == ran + 1;
            blocks_.store (ran + 1, std::memory_order_release);
            return drop;
        }

        PendingSlot<PhraseClip> pending;
        RetireFifo<PhraseClip, kRetireSlots> retired;

    private:
        std::atomic<std::uint32_t> stopTicket_ { 0 };
        std::atomic<std::uint64_t> blocks_ { 0 };
        std::atomic<std::uint64_t> dropAt_ { 0 };
    };
}
