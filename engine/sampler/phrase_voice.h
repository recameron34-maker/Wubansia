#pragma once

// A phrase voice (P6): plays a phrase lane's recorded stem, slice by slice, in place of the multisample
// (ADR 0020, docs/plugin/manifest.md section 2.6). tools/sound/phrase_tables.py's play () is its model:
// at the kit's tempo and 48 kHz, notes on the model's grid give the model's output to the last bit.
//
// The sampler (P5) hands it every note with slice >= 0: start () for a phrase no voice is playing,
// continueWith () for the next note of the phrase it plays. A phrase note starts at its slice's first
// frame (the slice's lead before its onset: annotatePhrases places it there), so the voice reads
// stem frame slice.start + startFrame at the note's offset.
//
// What it does with the notes, as the model:
// - A run of slices reads the stem continuously: a note whose slice is the next one (or, on a looped
//   stem, the first one after the last: the cycle's wrap) reads on, without a seam.
// - A slice whose note has NoteFlag::continues reads on into the next slice. One without it fades from
//   its fade frame over the phrase's fade_us (0.5 + 0.5 cos), and stops at its end.
// - A run's first slice comes in over 3 ms from its first frame (0.5 - 0.5 cos), under whatever still
//   plays out: a slice that doesn't continue fades on its own schedule, so a run can start under the
//   last one's tail (FUT03's throw slice plays across the cycle's wrap while the next cycle starts).
// What it adds for a live host (the model plays a fixed grid):
// - Reading on tolerates a note up to 2 frames off where the voice reads. At a cycle's wrap it
//   re-syncs: a frame or more off, it crossfades over 1 ms to where the note says. Further off mid-cycle,
//   another slice than the expected one, or the other A/B stem: it crossfades over 3 ms (a jump).
// - A slice that expected a follower which doesn't come (a switch, a tempo past the tolerance) fades
//   over the phrase's fade_us once it reads 2 frames into the follower.
// - startFrame > 0 (a chase after a start or a seek) starts mid-slice, fading in over the sampler's
//   chase fade.
// - Off 48 kHz it resamples with the sampler's sinc bank (no pitch: 48000 / rate frames a sample).
// It ignores note-offs and velocity: the stem has its own lengths and dynamics.
//
// Output: the stem at the phrase's gain_mdb into the phrase's mix channel; a thrown note's slice also
// goes, as mono, into the Space and Echo sends at NoteOn::throwSpace and throwEcho, times laneSend.
//
// Events queue (start, continueWith, stop) at sample offsets within the next render () call, which
// applies them in order. Everything but prepare () is noexcept and real-time safe: fixed arrays, no
// allocation, lock, I/O or wait. Pieces may have any size: nothing here depends on where they split.

#include "engine.h"
#include "kit/kit.h"
#include "mix/mixer.h"
#include "sampler/sampler.h"

#include <array>
#include <cstdint>

namespace wubansia::engine
{
    // Whether phrase notes play the phrase's slices: with PhraseMode::automatic, while
    // abs (hostBpm / kitBpm - 1) x 1000 <= phrase.tolerancePm (20: 2 %). Otherwise, and for MIDI in, the
    // phrase's notes play their multisample.
    bool playsSlices (const Phrase& phrase, std::int32_t kitBpm, double hostBpm, PhraseMode mode) noexcept;

    class PhraseVoice
    {
    public:
        static constexpr int kHeads = 4; // read positions sounding at once: a run, and what fades under it
        static constexpr int kMaxEvents = 16; // per render () call; more are dropped (Stats::dropped)
        static constexpr std::int32_t kRunFadeUs = 3000; // a run's first slice comes in over this (by position)
        static constexpr std::int32_t kJumpFadeUs = 3000; // a jump's crossfade
        static constexpr std::int32_t kResyncFadeUs = 1000; // a cycle wrap's re-sync
        static constexpr std::int32_t kThrowRampUs = 1000; // a throw's send moving to the next slice's
        static constexpr double kReadOnFrames = 2.0; // a next slice's note this close to the read position reads on
        static constexpr double kResyncFrames = 1.0; // at a cycle wrap, this far off re-syncs

        // What the voice did, since prepare () or reset (): for tests and telemetry.
        struct Stats
        {
            int runs = 0; // runs started (a first slice fading in from its first frame, or a chase)
            int readOns = 0; // notes that read on from the slice before
            int resyncs = 0; // cycle wraps re-synced with the 1 ms crossfade
            int jumps = 0; // 3 ms crossfades
            int missed = 0; // slices whose expected follower never came
            int dropped = 0; // events past kMaxEvents, or naming no slice of the phrase
        };

        // Message thread: the host's rate (11025-192000 Hz). Builds the sampler's shared sinc bank if it
        // isn't yet (allocates once per process). Every head stops.
        void prepare (double sampleRate);

        // Audio thread: silent at once, the queue empty.
        void reset() noexcept;

        // A note of a phrase this voice doesn't play yet (note.phrase, note.slice, note.character,
        // note.continues, note.startFrame, note.throwSpace and throwEcho), at a sample offset in the next
        // render (). The Kit must outlive the voice's sound (fadeKit and holdsKit in the sampler).
        void start (const Kit& kit, const NoteOn& note, int offset) noexcept;

        // The next note of the phrase this voice plays, at a sample offset in the next render ().
        void continueWith (const NoteOn& note, int offset) noexcept;

        // Every head fades out over fadeSamples from a sample offset in the next render () (0: at once).
        void stop (int offset, int fadeSamples) noexcept;

        // Applies the queued events and adds numSamples into bus (the phrase's mix channel) and sends.
        // laneSend: the phrase lane's send gain per sample (nullptr: 1), which scales the throws.
        void render (MixBus& bus, SendBus& sends, int numSamples, const float* laneSend = nullptr) noexcept;

        // A head sounds, or an event waits.
        bool active() const noexcept;

        // The Kit and phrase it plays (nullptr and -1 before start ()).
        const Kit* kit() const noexcept { return kit_; }
        int phrase() const noexcept { return phrase_; }

        // Where the newest head reads, in stem frames (-1 when none reads): for tests.
        double readPosition() const noexcept;

        const Stats& stats() const noexcept { return stats_; }

    private:
        enum class EventKind : std::uint8_t {
            start,
            next,
            stop
        };

        struct Event
        {
            EventKind kind = EventKind::next;
            int offset = 0;
            int fadeSamples = 0;
            const Kit* kit = nullptr;
            NoteOn note;
        };

        struct Head
        {
            bool on = false;
            std::uint32_t order = 0; // start order: the newest is the voice's current run
            std::uint8_t character = 0;
            SampleView view;
            bool stereo = false;
            double pos = 0.0; // the stem frame it reads next
            int slice = 0; // the phrase's own slice index its position is in
            bool seen = false; // the note of `slice` has come
            bool follows = false; // ...with NoteFlag::continues
            bool wrapped = false; // it came into `slice` across the stem's end (a cycle's wrap)
            int earlySlice = -1; // the next slice's note, come before the position reached it
            bool earlyFollows = false;
            float earlySpace = 0.0f;
            float earlyEcho = 0.0f;
            // The run's fade-in by position (the model's): frames [inFrom, inFrom + inLength) of its first slice.
            bool posIn = false;
            double inFrom = 0.0;
            double inLength = 0.0;
            // Fades by time: in (a chase, a crossfade) and out (a crossfade, a stop, a missed follower).
            double timeInLength = 0.0; // 0: none
            double timeInCount = 0.0;
            double timeOutLength = 0.0; // 0: none
            double timeOutCount = 0.0;
            // The throws: where the sends are, and where they go.
            float space = 0.0f;
            float echo = 0.0f;
            double spaceNow = 0.0;
            double echoNow = 0.0;
        };

        // The phrase's constants, from the Kit at start ().
        struct Setup
        {
            const Phrase* phrase = nullptr;
            const PhraseSlice* slices = nullptr;
            int count = 0;
            bool looped = false; // its first slice starts on frame 0 and its last ends on the stem's end
            double gain = 1.0; // gain_mdb
            int channel = 0; // its mix channel
            double fadeFrames = 0.0; // fade_us in stem frames, as the model counts them
            double runFrames = 0.0; // kRunFadeUs in stem frames
        };

        void apply (const Event& e) noexcept;
        void take (const NoteOn& note) noexcept;
        void renderHeads (MixBus& bus, SendBus& sends, const float* laneSend, int from, int to) noexcept;
        bool setUp (const Kit& kit, int phrase) noexcept;
        int follower (int slice) const noexcept;
        bool expects (const Head& h, int slice) const noexcept;
        Head* current() noexcept;
        Head& newHead() noexcept;
        void startHead (Head& h, const NoteOn& note, int slice, double pos) noexcept;
        void fadeOutBy (Head& h, double samples) noexcept;
        void read (const Head& h, double& left, double& right) const noexcept;

        double rate = 48000.0;
        double ratio = 1.0; // stem frames a host sample
        bool unit = true; // the host runs at 48 kHz: plain copies
        const float* sinc = nullptr; // the sinc bank's table for ratio
        double samplesPerUs = 0.048;

        const Kit* kit_ = nullptr;
        int phrase_ = -1;
        Setup setup;

        std::array<Head, kHeads> heads {};
        std::uint32_t order = 0;
        std::array<Event, kMaxEvents> events {};
        int eventCount = 0;
        Stats stats_;
    };
}
