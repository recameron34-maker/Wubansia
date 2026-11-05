#pragma once

// A phrase voice (P6): plays a phrase lane's recorded stem, slice by slice, in place of the multisample
// (ADR 0020, docs/plugin/manifest.md section 2.6).
//
// This is a stand-in with P6's interface and a silent body, so the sampler (P5) builds and tests its
// routing against the calls P6's voice takes: P6's phrase_voice.h and phrase_voice.cpp replace it. It
// keeps just enough state to behave like P6's voice from outside: a start () makes it active, a stop ()
// keeps it active until its fade has run out, and reset () silences it at once.
//
// The sampler hands it every note with slice >= 0: start () for a phrase no voice is playing,
// continueWith () for the next note of the phrase it plays, stop () to fade it out (a jump, a stop, a
// kit change, a steal). Events queue at sample offsets within the next render () call, which applies
// them in order. Everything but prepare () is noexcept and real-time safe. A PhraseVoice is plain data:
// the sampler copies a stolen one into a slot of its own to fade there.

#include "engine.h"
#include "kit/kit.h"
#include "mix/mixer.h"
#include "sampler/sampler.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace wubansia::engine
{
    // Whether phrase notes play the phrase's slices: with PhraseMode::automatic, while
    // abs (hostBpm / kitBpm - 1) x 1000 <= phrase.tolerancePm (20: 2 %). Otherwise, and for MIDI in, the
    // phrase's notes play their multisample.
    inline bool playsSlices (const Phrase& phrase, std::int32_t kitBpm, double hostBpm, PhraseMode mode) noexcept
    {
        if (mode != PhraseMode::automatic || kitBpm <= 0 || ! (hostBpm > 0.0))
            return false;
        return std::fabs (hostBpm / static_cast<double> (kitBpm) - 1.0) * 1000.0 <= static_cast<double> (phrase.tolerancePm);
    }

    class PhraseVoice
    {
    public:
        static constexpr int kMaxEvents = 16; // per render () call; more are dropped (Stats::dropped)

        // What the voice did, since prepare () or reset ().
        struct Stats
        {
            int runs = 0; // start () calls applied
            int readOns = 0; // continueWith () calls applied
            int resyncs = 0;
            int jumps = 0;
            int missed = 0;
            int dropped = 0; // events past kMaxEvents
        };

        // Message thread: the host's rate. (P6's voice builds the sampler's shared sinc bank if it isn't
        // yet; the sampler builds it first, so this never allocates.) Silent.
        void prepare (double sampleRate)
        {
            rate = sampleRate;
            reset();
        }

        // Audio thread: silent at once, the queue empty.
        void reset() noexcept
        {
            eventCount = 0;
            playing = false;
            fadeLeft = -1;
            kit_ = nullptr;
            phrase_ = -1;
            stats_ = Stats {};
        }

        // A note of a phrase this voice doesn't play yet, at a sample offset in the next render ().
        void start (const Kit& kit, const NoteOn& note, int offset) noexcept
        {
            kit_ = &kit;
            phrase_ = note.phrase;
            push (Kind::start, offset, 0);
        }

        // The next note of the phrase this voice plays, at a sample offset in the next render ().
        void continueWith (const NoteOn& note, int offset) noexcept
        {
            (void) note;
            push (Kind::next, offset, 0);
        }

        // Fades out over fadeSamples from a sample offset in the next render () (0: at once).
        void stop (int offset, int fadeSamples) noexcept { push (Kind::stop, offset, fadeSamples); }

        // Applies the queued events and adds numSamples (silence, here). laneSend: the phrase lane's send
        // gain per sample (nullptr: 1).
        void render (MixBus& bus, SendBus& sends, int numSamples, const float* laneSend = nullptr) noexcept
        {
            (void) bus;
            (void) sends;
            (void) laneSend;
            const int n = std::max (0, numSamples);
            for (int i = 0; i < eventCount; ++i)
            {
                const auto& e = events[static_cast<std::size_t> (i)];
                if (e.kind == Kind::start)
                {
                    ++stats_.runs;
                    playing = true;
                    fadeLeft = -1;
                }
                else if (e.kind == Kind::next)
                {
                    ++stats_.readOns;
                }
                else if (playing)
                {
                    const int end = std::clamp (e.offset, 0, std::max (0, n - 1)) + std::max (0, e.fadeSamples);
                    fadeLeft = fadeLeft < 0 ? end : std::min (fadeLeft, end);
                }
            }
            eventCount = 0;
            if (playing && fadeLeft >= 0)
            {
                fadeLeft = std::max (0, fadeLeft - n);
                playing = fadeLeft > 0;
            }
        }

        // It sounds, or an event waits.
        bool active() const noexcept { return eventCount > 0 || playing; }

        // The Kit and phrase it plays (nullptr and -1 before start ()).
        const Kit* kit() const noexcept { return kit_; }
        int phrase() const noexcept { return phrase_; }

        // Where it reads, in stem frames (-1: nowhere; the stand-in never reads).
        double readPosition() const noexcept { return -1.0; }

        const Stats& stats() const noexcept { return stats_; }

    private:
        enum class Kind : std::uint8_t {
            start,
            next,
            stop
        };

        struct Event
        {
            Kind kind = Kind::next;
            int offset = 0;
            int fadeSamples = 0;
        };

        void push (Kind kind, int offset, int fadeSamples) noexcept
        {
            if (eventCount >= kMaxEvents)
            {
                ++stats_.dropped;
                if (kind != Kind::stop)
                    return;
                eventCount = kMaxEvents - 1; // a stop must not get lost: it takes the last place
            }
            events[static_cast<std::size_t> (eventCount++)] = Event { kind, offset, fadeSamples };
        }

        double rate = 48000.0;
        const Kit* kit_ = nullptr;
        int phrase_ = -1;
        bool playing = false;
        int fadeLeft = -1; // after a stop: samples until silent; -1: no stop under way
        std::array<Event, kMaxEvents> events {};
        int eventCount = 0;
        Stats stats_;
    };
}
