#pragma once

// A phrase voice (P6): plays a phrase lane's recorded stem, slice by slice, in place of the multisample
// (ADR 0020, docs/plugin/manifest.md section 2.6). tools/sound/phrase_tables.py's play () is its model: at the
// kit's tempo and 48 kHz, notes on the model's grid give the model's output to the last bit. Through the engine
// that holds over a loop's first pass; after it the voice re-syncs at each cycle's wrap over 1 ms, because a
// cycle at the exact BPM isn't a whole number of samples (FUT03's is: 360 000 frames for 4 bars at 128 BPM).
//
// The sampler (P5) hands it every note with slice >= 0: start () for a phrase no voice is playing,
// continueWith () for the next note of the phrase it plays. A slice note starts at its slice's start frame
// (annotatePhrases places it there), so the voice reads stem frame slice.start + startFrame at the note's
// offset.
//
// What it does with the notes, as the model:
// - A run of slices reads the stem continuously: a note whose slice is the next one (or, on a looped
//   stem, slice 0 after the last: the cycle's wrap) reads on, without a seam. A looped stem's last slice may
//   wrap through the stem's end (its end up to frames + slice 0's start): it is read modulo the stem's length.
// - A slice whose note has NoteFlag::continues reads on into the next slice. One without it fades from its
//   fade frame over the phrase's fade_us (0.5 + 0.5 cos), and stops at its end.
// - A run's first slice comes in from its start frame (0.5 - 0.5 cos) over kRunFadeUs, or kRunFadeLeadlessUs
//   when its lead is 0, under whatever still plays out: a slice that doesn't continue fades on its own
//   schedule, so a run can start under the last one's tail (FUT03's throw slice plays across the cycle's wrap
//   while the next cycle starts).
// What it adds for a live host (the model plays a fixed grid):
// - At the kit's tempo, reading on tolerates a note up to 2 frames off where the voice reads. At a cycle's
//   wrap it re-syncs: 1 or 2 frames off, it crossfades over 1 ms to where the note says. Further, another
//   slice than the expected one, or the other A/B stem: it crossfades over 3 ms (a jump). A follower that
//   doesn't come (a switch, a stop) fades over 1 ms once the voice reads 2 frames into it.
// - Off the kit's tempo (setTempo ()), a slice reads on into its follower only while the tempo keeps it
//   within a frame over its length (kDriftFrames: about 20 ppm for a 1-bar slice). Otherwise every slice is
//   its own run: it fades from its fade frame, the next comes in over the run's fade, nothing reads into a
//   follower, and each is anchored on its onset (its lead at the host's tempo: it starts later, or part-way
//   in, so the onset frame lands on the note's onset).
// - startFrame > 0 (a chase after a start, a jump or a host loop's wrap) starts mid-slice, fading in over the
//   sampler's chase fade, on a head of its own: it never takes over another slice's head. A head already
//   reading that slice takes the note instead: within a frame it reads on, 1 or 2 frames off it re-syncs
//   over 1 ms, further it jumps (so a slice a host loop re-chases doesn't sound twice).
// - Off 48 kHz it resamples with the sampler's sinc bank (no pitch: 48000 / rate frames a sample).
// It ignores note-offs and velocity: the stem has its own lengths and dynamics. MIDI in never plays slices: a
// note from Source::live plays nothing here.
//
// Output: the stem at the phrase's gain_mdb into the phrase's mix channel. A live throw (NoteOn::throwSpace,
// throwEcho; none for a phrase whose throw is baked into its stem) sends the head's sound, as mono, into the
// Space and Echo inputs times laneSend: for NoteOn::lengthSamples from the note when that is set (to the end of
// the thrown note, across the slices after it), else until the next note sets the head's throw.
//
// Events queue (start, continueWith, stop) at sample offsets within the next render () call, which applies
// them in order. Everything but prepare () is noexcept and real-time safe: fixed arrays, no allocation, lock,
// I/O or wait. Pieces may have any size: nothing here depends on where they split.

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
    // phrase's notes play their multisample. The engine decides once per phrase per onset cycle.
    bool playsSlices (const Phrase& phrase, std::int32_t kitBpm, double hostBpm, PhraseMode mode) noexcept;

    class PhraseVoice
    {
    public:
        static constexpr int kHeads = 4; // read positions sounding at once: a run, and what fades under it
        static constexpr int kMaxEvents = 16; // per render () call; more are dropped (Stats::dropped)
        // A run's first slice comes in over this from its start frame (by position, as the model). A slice with
        // no lead takes kRunFadeLeadlessUs: the design check's 0.5 ms (500) keeps plucks' transients, and moves
        // here with P2's play (), which still brings every run in over 3 ms.
        static constexpr std::int32_t kRunFadeUs = 3000;
        static constexpr std::int32_t kRunFadeLeadlessUs = 3000;
        static constexpr std::int32_t kJumpFadeUs = 3000; // a jump's crossfade
        static constexpr std::int32_t kResyncFadeUs = 1000; // a cycle wrap's (or a chase's) re-sync
        static constexpr std::int32_t kMissedFadeUs = 1000; // a follower that doesn't come
        static constexpr std::int32_t kThrowRampUs = 1000; // a throw's send moving to another level
        static constexpr double kReadOnFrames = 2.0; // a next slice's note this close to the read position reads on
        static constexpr double kResyncFrames = 1.0; // at a cycle wrap, this far off re-syncs
        static constexpr double kDriftFrames = 1.0; // off tempo, a slice reads on only while it drifts less than this

        // What the voice did, since prepare () or reset (): for tests and telemetry.
        struct Stats
        {
            int runs = 0; // runs started: a first slice, a chase, a slice off tempo
            int readOns = 0; // notes a head took as it read on (a next slice, or a chase it was already reading)
            int resyncs = 0; // 1 ms re-syncs: a cycle's wrap, or a chase 1 or 2 frames off
            int jumps = 0; // 3 ms crossfades
            int missed = 0; // slices whose expected follower never came
            int dropped = 0; // events past kMaxEvents, naming no slice of the phrase, or from MIDI in
        };

        // Message thread: the host's rate (11025-192000 Hz). Builds the sampler's shared sinc bank if it
        // isn't yet (allocates once per process). Every head stops; the tempo is the kit's.
        void prepare (double sampleRate);

        // Audio thread: silent at once, the queue empty.
        void reset() noexcept;

        // The host's tempo for the notes from now on (0 or less: the kit's own). The engine sets it per block.
        void setTempo (double hostBpm) noexcept { bpm = hostBpm; }

        // A note of a phrase this voice doesn't play yet (note.phrase, note.slice, note.character,
        // note.continues, note.startFrame, note.throwSpace, throwEcho and lengthSamples), at a sample offset in
        // the next render (). The Kit must outlive the voice's sound (fadeKit and holdsKit in the sampler).
        void start (const Kit& kit, const NoteOn& note, int offset) noexcept;

        // The next note of the phrase this voice plays, at a sample offset in the next render ().
        void continueWith (const NoteOn& note, int offset) noexcept;

        // Every head fades out over fadeSamples from a sample offset in the next render () (0: at once).
        void stop (int offset, int fadeSamples) noexcept;

        // Applies the queued events and adds numSamples into bus (the phrase's mix channel) and sends.
        // laneSend: the phrase lane's send gain per sample (nullptr: 1), which scales the throws.
        void render (MixBus& bus, SendBus& sends, int numSamples, const float* laneSend = nullptr) noexcept;

        // A head sounds (or waits to), or an event waits.
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

        // A throw on a head: its levels, and how long it lasts (0: until the next note sets another).
        struct Throw
        {
            float space = 0.0f;
            float echo = 0.0f;
            double left = 0.0;
        };

        struct Head
        {
            bool on = false;
            std::uint32_t order = 0; // start order: the newest is the voice's current run
            std::uint8_t character = 0;
            SampleView view;
            bool stereo = false;
            double pos = 0.0; // the stem frame it reads next, unwrapped within its slice (a wrapping slice runs past frames)
            double delay = 0.0; // host samples before it starts (a slice anchored on its onset off tempo)
            int slice = 0; // the phrase's own slice index its position is in
            bool seen = false; // the note of `slice` has come
            bool follows = false; // ...and it reads on into the next slice
            bool wrapped = false; // it came into `slice` across the stem's end (a cycle's wrap)
            int earlySlice = -1; // the next slice's note, come before the position reached it
            bool earlyFollows = false;
            Throw earlyThrow;
            // The run's fade-in by position (the model's): frames [inFrom, inFrom + inLength) of its first slice.
            bool posIn = false;
            double inFrom = 0.0;
            double inLength = 0.0;
            double inFrames = 0.0; // the fade's whole length, in frames
            // Fades by time: in (a chase, a crossfade, an anchored start) and out (a crossfade, a stop, a missed follower).
            double timeInLength = 0.0; // 0: none
            double timeInCount = 0.0;
            double timeOutLength = 0.0; // 0: none
            double timeOutCount = 0.0;
            // The throw, and where the sends are now.
            Throw thrown;
            double spaceNow = 0.0;
            double echoNow = 0.0;
        };

        // The phrase's constants, from the Kit at start ().
        struct Setup
        {
            const Phrase* phrase = nullptr;
            const PhraseSlice* slices = nullptr;
            int count = 0;
            std::int64_t frames = 0; // the shorter stem's
            bool looped = false; // its last slice ends (wrapping) where slice 0 starts: end - frames == slice 0's start
            double gain = 1.0; // gain_mdb
            int channel = 0; // its mix channel
            double fadeFrames = 0.0; // fade_us in stem frames, as the model counts them
            double kitBpm = 0.0;
        };

        void apply (const Event& e) noexcept;
        void take (const NoteOn& note) noexcept;
        void renderHeads (MixBus& bus, SendBus& sends, const float* laneSend, int from, int to) noexcept;
        bool setUp (const Kit& kit, int phrase) noexcept;
        int follower (int slice) const noexcept;
        bool expects (const Head& h, int slice) const noexcept;
        Head& newHead (const Head* keep = nullptr) noexcept;
        void startHead (Head& h, const NoteOn& note, int slice, double pos, bool follows) noexcept;
        void crossfade (Head& from, const NoteOn& note, int slice, double pos, bool follows, std::int32_t us) noexcept;
        void runFrom (const NoteOn& note, int slice, bool follows, bool anchored) noexcept;
        void fadeOutBy (Head& h, double samples) noexcept;
        Throw throwOf (const NoteOn& note) const noexcept;
        void read (const Head& h, double& left, double& right) const noexcept;

        double rate = 48000.0;
        double ratio = 1.0; // stem frames a host sample
        bool unit = true; // the host runs at 48 kHz: plain copies
        const float* sinc = nullptr; // the sinc bank's table for ratio
        double samplesPerUs = 0.048;
        double bpm = 0.0; // the host's tempo; 0: the kit's

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
