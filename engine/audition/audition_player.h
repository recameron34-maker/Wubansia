#pragma once

// The Samples drawer's player (ADR 0024): one dry clip at a time, added after the engine's output and before the
// plugin's master_gain, at a fixed -6 dB. Plain C++ with no framework, like the rest of engine/.
//
// Threads, and who owns a clip:
// - the sound thread is the only producer: it makes clips with new, offers them (latest wins), and deletes what
//   offer () hands back (an offer the audio thread never took) and what collectRetired () hands it;
// - the message thread prepares the player (audio stopped) and stops tickets (stopUpTo, any thread);
// - the audio thread plays: process () and reset (). It never allocates, frees, locks, waits or throws.
// Every clip comes back exactly once: from offer ()'s return, from collectRetired (), or to the destructor.
//
// At the start of each block the audio thread:
// - takes the offered clip, only while no replaced clip is still fading out and the retire FIFO has room (so it can
//   always let a clip go);
// - lets it go unplayed if it was made for another rate or its ticket is stopped; otherwise plays it from its first
//   frame at once, and the clip it replaces fades out over kFadeSeconds;
// - fades the playing clip out over kFadeSeconds once its ticket is stopped (from that block's first sample);
// - lets a clip made for another rate go at once (a prepare at a new rate).
// A clip that ends, or finishes fading, while the FIFO is full is held, silent, until there's room; nothing new is
// taken meanwhile. A silent block (an offline render, a bypassed block) and reset () let every clip go at once,
// unplayed, the offer too. With no clip to play it writes nothing, so an idle player leaves the output exactly as it
// was, -0.0 included: a render is bit for bit the same with or without the player.
//
// Mixing, per sample: the clip fading out is added first, then the one playing, each as out += kGain x (x x g), g
// the fade's gain (1 when not fading). Two outputs take the clip's channels (a mono clip feeds both); one output
// takes (L + R) / 2, as 0.5 x (L + R), the engine's fold.

#include "audition/audition_clip.h"
#include "rt/pending_slot.h"
#include "rt/retire_fifo.h"

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace wubansia::engine
{
    class AuditionPlayer
    {
    public:
        static constexpr std::size_t kRetireSlots = 8;
        static constexpr float kGain = 0.5f; // -6 dB, before the plugin's master_gain
        static constexpr double kFadeSeconds = 0.005; // a replaced or stopped clip: 240 samples at 48 kHz

        AuditionPlayer() = default;
        ~AuditionPlayer(); // deletes every clip it still holds (offered, playing, fading, retired); audio stopped
        AuditionPlayer (const AuditionPlayer&) = delete;
        AuditionPlayer& operator= (const AuditionPlayer&) = delete;

        // Message thread, audio stopped. Clips are made for this rate from now on; a clip made for another goes at the
        // next block. A rate that isn't positive and finite counts as none (0): nothing plays.
        void prepare (double sampleRate) noexcept;
        double rate() const noexcept; // any thread: what clips are made for; 0 before prepare

        // The sound thread, the only producer. nullptr withdraws an offer. Returns the earlier offer if the audio
        // thread never took it (never seen by the audio thread: the sound thread deletes it).
        const AuditionClip* offer (const AuditionClip* clip) noexcept { return pending.publish (clip); }

        // The sound thread: f (const AuditionClip*) for each clip the audio thread let go, oldest first.
        template <class F>
        void collectRetired (F&& f) noexcept
        {
            while (const AuditionClip* clip = retired.pop())
                f (clip);
        }

        // Any thread: clips with a ticket <= this stop (a playing one fades out) or never play. Tickets only grow.
        void stopUpTo (std::uint32_t ticket) noexcept;

        // Audio thread. Adds the playing clip x kGain into out[0..numOutputs) (two outputs at most); one output gets
        // (L + R) / 2. silent (an offline render, a bypassed block), out == nullptr or numOutputs <= 0: writes nothing
        // and lets every clip go unplayed. With nothing to play it touches nothing. Any numSamples: it keeps no
        // buffers, so a block longer than the host's prepared size is fine.
        void process (float* const* out, int numOutputs, int numSamples, bool silent) noexcept;

        // Audio thread (the host's reset): every clip let go now, the offer too.
        void reset() noexcept;

        std::uint32_t sounding() const noexcept; // any thread: the ticket playing (not fading out), 0 when none
        bool offered() const noexcept; // any thread: an offer waits for the audio thread (a snapshot)

        // Any thread: how many blocks process () has run, silent ones too. The sound thread watches it to tell a host
        // that has stopped running blocks.
        std::uint64_t blocksRun() const noexcept;

        // The sound thread, once the host has run no block for a while: if the next block is the first since `seen`
        // blocks had run, the clips in hand (the one playing and the one fading out, never the offer) go at its
        // start, unplayed and unfaded, so a host that comes back plays nothing stale. If a block ran meanwhile, it
        // changes nothing: what plays plays on (or fades out, if it was stopped).
        void dropIfIdleSince (std::uint64_t seen) noexcept;

    private:
        // One clip in the audio thread's hands.
        struct Slot
        {
            const AuditionClip* clip = nullptr;
            std::int64_t position = 0; // the next frame to play
            int fadeLength = 0; // the fade-out's length in samples; 0 while not fading
            int fadeLeft = 0; // samples of the fade-out still to play
            bool done = false; // ended or faded out, waiting, silent, for room in the FIFO
        };

        void letGo (Slot& slot) noexcept; // into the FIFO now, or held (done) until there's room
        void letGoAll() noexcept; // both slots and the offer, as room allows
        void render (Slot& slot, float* left, float* right, int numSamples) noexcept;

        PendingSlot<AuditionClip> pending;
        RetireFifo<AuditionClip, kRetireSlots> retired;
        std::atomic<double> rate_ { 0.0 };
        std::atomic<std::uint32_t> stopTicket { 0 };
        std::atomic<std::uint32_t> soundingTicket { 0 };

        // The audio thread's own: the clip playing (which fades out where it is once stopped), and the one it
        // replaced, fading out.
        Slot current;
        Slot fading;

        static_assert (std::atomic<double>::is_always_lock_free && std::atomic<std::uint32_t>::is_always_lock_free);
    };
}
