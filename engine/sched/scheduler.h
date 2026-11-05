#pragma once

// The scheduler: plays a Timeline in sync with the host's transport (or the engine's own clock), and
// tells a Sink what happens at which sample. Audio thread; nothing here allocates, locks or throws.
//
// Positions. The host position H is in milliticks (quarters x 960000) from the host's zero. Each block
// starts with sync (), which follows the transport; then advance () plays the block piece by piece. A
// position maps to a sample through an anchor (a sample index paired with a position), set at a start,
// a jump, a tempo change and a loop wrap:
//     sample (H) = anchorSample + floor ((H - anchorH) / mps + 1/2),   mps = bpm x 960000 / (60 x rate)
// so an event lands on the sample nearest its position (as phrase.Grid.at rounds), and on the same
// sample whatever the host's block size: offsets never depend on where a block starts.
// - Loop timelines play H mod length from H = 0 (quarters mod 16 for the 4-bar loops, whatever the
//   host's time signature); song timelines play H as it is, and nothing past their length.
// - Negative positions are silent. Playing on into 0 counts as a start there.
// - A host position more than 1 ms off the one expected is a jump: sequenced voices fade (the Sink's
//   stopSequenced), and the new position is chased. A tempo change re-anchors without a jump.
// - A host loop region wraps inside the block at its end: the sounding notes get their note-offs there,
//   and play goes on from the loop's start (chased), so no note is stranded.
//
// The sounding table holds every sequenced note that started, with its note-off position (in H,
// unwrapped), so note-offs come from it, not from the timeline: switching mode, variant or timeline
// mid-note never loses one, and a loop's notes end in the next cycle. Starts, jumps and wraps chase:
// the notes Timeline::chase lists that sound at the position start part-way in, and each cc_filter
// ride gets its value there.
//
// At one sample the Sink hears note-offs first, then controllers, then duck keys, then note-ons.
//
// The Sink (any type with these members, all noexcept):
//   int  noteOn (const Note& note, int offset, std::int64_t lengthSamples, std::int64_t chaseSamples);
//        A note starts at offset. lengthSamples: host samples from offset to its end at the current
//        tempo; chaseSamples: host samples since its own start (0 unless chased). Returns a tag that
//        comes back with its note-off, or -1 if nothing played it (it then gets no note-off).
//   void noteOff (std::uint8_t channel, std::uint8_t key, int tag, int offset);
//   void cc (std::uint8_t channel, std::uint8_t controller, std::uint8_t value, int offset, int rampSamples);
//        A controller moves to value over rampSamples from offset (0: at once), from where it is.
//   void keyHit (std::uint8_t lane, std::uint8_t velocity, int offset);
//   void stopSequenced (int offset);  // fade every sequenced voice (stop, jump)
// Offsets count from the start of the current advance () (sync ()'s and restart ()'s are 0: the start
// of the next one).

#include "engine.h"
#include "sched/timeline.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace wubansia::engine
{
    // Where the scheduler is, for the editor's telemetry.
    struct Position
    {
        bool playing = false;
        double ppq = 0.0; // the host position of the next sample, in quarters
        double bpm = 0.0;
        double ticks = -1.0; // the playing timeline's position in 960-PPQ ticks; -1 when stopped, silent or past the end
        std::int32_t section = -1; // the last of the timeline's sections at or before it; -1 before the first
    };

    // What sync () found.
    enum class SyncEvent : std::uint8_t {
        none,
        started, // the transport started (chased at its position)
        stopped, // the transport stopped (sequenced voices fade)
        jumped, // the position moved (sequenced voices fade; chased at the new one)
        wrapped, // the host's loop region wrapped on the block's first sample
        retimed // the tempo changed (re-anchored)
    };

    class Scheduler
    {
    public:
        static constexpr int kSoundingSlots = 256;
        static constexpr double kMinBpm = 10.0;
        static constexpr double kMaxBpm = 1000.0;
        static constexpr double kJumpSeconds = 0.001; // a host position this far off is a jump
        static constexpr int kMinLoopSamples = 64; // a host loop region shorter than this doesn't wrap
        static constexpr int kMaxSteps = 1 << 16; // events per advance (), a guard: content has far fewer

        // The host's sample rate. Resets everything (allocates nothing).
        void prepare (double sampleRate) noexcept;

        // Stopped, at sample 0, with an empty sounding table (no note-offs are sent).
        void reset() noexcept;

        // At a block's start: follows the transport. defaultBpm is the tempo when the host gives none
        // (the kit's); with no host position the scheduler runs its own clock, from 0 at each start.
        template <class Sink>
        SyncEvent sync (const Timeline* timeline, const TransportInfo& transport, double defaultBpm, Sink& sink) noexcept;

        // Plays the next numSamples: every event whose sample falls in them, at its offset from their
        // start. timeline (nullptr: none, as in Off) changing between calls applies from here to new
        // notes; sounding notes still end at their own note-offs.
        template <class Sink>
        void advance (const Timeline* timeline, int numSamples, Sink& sink) noexcept;

        // After a kit change: forgets the sounding notes (their voices fade with their Kit) and, while
        // playing, chases timeline at the current position.
        template <class Sink>
        void restart (const Timeline* timeline, Sink& sink) noexcept;

        // The offset of the first 4/4 bar line (H a multiple of 4 quarters) within the next numSamples,
        // or -1; always -1 while stopped.
        int barLineWithin (int numSamples) const noexcept;

        bool playing() const noexcept { return playing_; }
        double bpm() const noexcept { return bpm_; }
        std::int64_t sampleIndex() const noexcept { return now_; } // the next sample, from prepare () or reset ()
        int sounding() const noexcept { return count_; }
        Position position() const noexcept;

        // The position of a sample and the sample of a position, under the current anchor.
        double positionAt (std::int64_t sample) const noexcept
        {
            return anchorH_ + static_cast<double> (sample - anchorSample_) * mps_;
        }

        std::int64_t sampleAt (double h) const noexcept
        {
            return anchorSample_ + static_cast<std::int64_t> (std::floor ((h - anchorH_) / mps_ + 0.5));
        }

    private:
        struct Cursor
        {
            std::size_t index = 0;
            std::int64_t cycle = 0;
        };

        struct Sounding
        {
            std::int64_t off = 0; // H of its note-off
            std::int32_t tag = -1;
            std::uint8_t channel = 0;
            std::uint8_t key = 0;
        };

        enum Kind : int {
            kOff,
            kChase0,
            kCc,
            kKey,
            kOn,
            kNone
        };

        static constexpr double kMaxPosition = 1.0e15; // milliticks; anything beyond is a broken host

        void setTempo (double bpm) noexcept;
        void anchor (std::int64_t sample, double h) noexcept;
        void armLoop (const TransportInfo& transport) noexcept;
        void seek (const Timeline* timeline, std::int64_t fromSample) noexcept;
        void clearSounding() noexcept { count_ = 0; }
        int earliestOff() const noexcept;

        bool loops() const noexcept { return current_ != nullptr && current_->loops; }
        std::int64_t cycleBase (const Cursor& c) const noexcept { return loops() ? c.cycle * current_->length : 0; }

        // The H of the stream's next event, or false when it has none.
        template <class Event>
        bool peek (const std::vector<Event>& events, const Cursor& c, std::int64_t& h) const noexcept
        {
            if (current_ == nullptr || events.empty() || c.index >= events.size())
                return false;
            if (loops() && current_->length <= 0)
                return false;
            h = cycleBase (c) + events[c.index].pos;
            return true;
        }

        template <class Event>
        void step (const std::vector<Event>& events, Cursor& c) const noexcept
        {
            if (++c.index >= events.size() && loops())
            {
                c.index = 0;
                ++c.cycle;
            }
        }

        // The first event with H >= 0 whose sample is fromSample or later.
        template <class Event>
        Cursor seekStream (const std::vector<Event>& events, std::int64_t fromSample) const noexcept
        {
            Cursor c;
            if (current_ == nullptr || events.empty())
                return c;
            auto due = [this, fromSample] (std::int64_t h) { return h >= 0 && sampleAt (static_cast<double> (h)) >= fromSample; };
            if (! loops())
            {
                const auto it = std::partition_point (events.begin(), events.end(), [&] (const Event& e) { return ! due (e.pos); });
                c.index = static_cast<std::size_t> (it - events.begin());
                return c;
            }
            const std::int64_t length = current_->length;
            if (length <= 0)
                return c;
            const double h = positionAt (fromSample);
            std::int64_t cycle = h <= 0.0 ? 0 : static_cast<std::int64_t> (std::floor (h / static_cast<double> (length))) - 1;
            cycle = std::max<std::int64_t> (cycle, 0);
            for (int tries = 0; tries < 4; ++tries, ++cycle)
            {
                const std::int64_t base = cycle * length;
                const auto it = std::partition_point (events.begin(), events.end(), [&] (const Event& e) { return ! due (base + e.pos); });
                if (it != events.end())
                {
                    c.index = static_cast<std::size_t> (it - events.begin());
                    c.cycle = cycle;
                    return c;
                }
            }
            c.cycle = cycle;
            return c;
        }

        template <class Sink>
        void insertSounding (std::int64_t off, std::int32_t tag, std::uint8_t channel, std::uint8_t key, int offset, Sink& sink) noexcept
        {
            if (count_ == kSoundingSlots)
            {
                // Full: the note due to end first ends now, so no note-off is ever lost.
                const int first = earliestOff();
                const auto& s = slots_[static_cast<std::size_t> (first)];
                sink.noteOff (s.channel, s.key, s.tag, offset);
                slots_[static_cast<std::size_t> (first)] = slots_[static_cast<std::size_t> (count_ - 1)];
                --count_;
            }
            slots_[static_cast<std::size_t> (count_++)] = { off, tag, channel, key };
        }

        template <class Sink>
        void endSounding (int offset, Sink& sink) noexcept
        {
            // Oldest note-off first, as the table would have sent them.
            while (count_ > 0)
            {
                const int first = earliestOff();
                const auto s = slots_[static_cast<std::size_t> (first)];
                slots_[static_cast<std::size_t> (first)] = slots_[static_cast<std::size_t> (count_ - 1)];
                --count_;
                sink.noteOff (s.channel, s.key, s.tag, offset);
            }
        }

        // Chases the current timeline at sample `at` (position h): each cc_filter ride's value there, then
        // the chase-listed notes sounding there, part-way in. Before 0 it waits for 0 instead.
        template <class Sink>
        void chase (double h, std::int64_t at, Sink& sink) noexcept;

        double rate_ = 48000.0;
        double bpm_ = 120.0;
        double mps_ = 0.0; // milliticks per sample
        std::int64_t now_ = 0; // the next sample
        std::int64_t chunkStart_ = 0; // offsets count from here
        bool playing_ = false;
        bool pendingZero_ = false; // playing before 0: chase when play reaches 0

        std::int64_t anchorSample_ = 0;
        double anchorH_ = 0.0;

        bool loopActive_ = false; // the host's loop region wraps in this block
        double loopStartH_ = 0.0;
        double loopEndH_ = 0.0;

        const Timeline* current_ = nullptr; // the timeline the cursors are in
        Cursor notes_;
        Cursor ccs_;
        Cursor keys_;

        std::array<Sounding, kSoundingSlots> slots_ {};
        int count_ = 0;
    };

    // ---- the templates -------------------------------------------------------------------------------

    template <class Sink>
    SyncEvent Scheduler::sync (const Timeline* timeline, const TransportInfo& transport, double defaultBpm, Sink& sink) noexcept
    {
        chunkStart_ = now_;
        double bpm = transport.bpm > 0.0 && std::isfinite (transport.bpm) ? transport.bpm : defaultBpm;
        if (! (bpm > 0.0) || ! std::isfinite (bpm))
            bpm = 120.0;
        bpm = std::clamp (bpm, kMinBpm, kMaxBpm);
        const bool hasPosition = transport.hasPosition && std::isfinite (transport.ppq);
        auto hostH = [&transport] {
            return std::clamp (transport.ppq * static_cast<double> (kMilliticksPerQuarter), -kMaxPosition, kMaxPosition);
        };

        if (! transport.playing)
        {
            const bool was = playing_;
            if (playing_)
            {
                sink.stopSequenced (0);
                clearSounding();
                playing_ = false;
            }
            pendingZero_ = false;
            loopActive_ = false;
            setTempo (bpm);
            return was ? SyncEvent::stopped : SyncEvent::none;
        }

        if (! playing_)
        {
            playing_ = true;
            setTempo (bpm);
            const double h = hasPosition ? hostH() : 0.0;
            anchor (now_, h);
            armLoop (transport);
            seek (timeline, now_);
            chase (h, now_, sink);
            return SyncEvent::started;
        }

        const double expected = positionAt (now_);
        const bool wasLooping = loopActive_;
        const double previousLoopEnd = loopEndH_;
        const std::int64_t previousWrap = wasLooping ? sampleAt (loopEndH_) : -1; // where the loop region's end fell
        const bool tempoChanged = std::abs (bpm - bpm_) > 1.0e-9 * bpm_;
        setTempo (bpm);

        if (hasPosition)
        {
            const double host = hostH();
            const double threshold = kJumpSeconds * rate_ * mps_; // 1 ms, in milliticks
            if (std::abs (host - expected) > threshold)
            {
                const double loopStart = transport.loopStartPpq * static_cast<double> (kMilliticksPerQuarter);
                const bool wrap = wasLooping && transport.looping && std::abs (expected - previousLoopEnd) <= threshold
                                  && std::abs (host - loopStart) <= threshold;
                if (wrap)
                    endSounding (0, sink);
                else
                {
                    sink.stopSequenced (0);
                    clearSounding();
                }
                // A wrap the host made on this block's first sample lands where an in-block wrap would have
                // (the loop's start on the end's sample), not on the host's rounding of it.
                if (wrap && std::abs (previousWrap - now_) <= 1)
                    anchor (previousWrap, loopStart);
                else
                    anchor (now_, host);
                armLoop (transport);
                seek (timeline, now_);
                chase (host, now_, sink);
                return wrap ? SyncEvent::wrapped : SyncEvent::jumped;
            }
            if (tempoChanged)
                anchor (now_, host);
        }
        else if (tempoChanged)
            anchor (now_, expected);

        armLoop (transport);
        if (timeline != current_)
            seek (timeline, now_);
        return tempoChanged ? SyncEvent::retimed : SyncEvent::none;
    }

    template <class Sink>
    void Scheduler::advance (const Timeline* timeline, int numSamples, Sink& sink) noexcept
    {
        if (numSamples <= 0)
            return;
        chunkStart_ = now_;
        const std::int64_t end = now_ + numSamples;
        if (! playing_)
        {
            now_ = end;
            return;
        }
        if (timeline != current_)
            seek (timeline, now_);

        for (int steps = 0; steps < kMaxSteps; ++steps)
        {
            // The earliest event due, by (sample, kind). With a loop region, nothing at or past its end is
            // due: the wrap comes first.
            std::int64_t bestSample = end;
            Kind best = kNone;
            auto consider = [&] (std::int64_t sample, Kind kind) {
                if (sample >= end)
                    return;
                if (sample < bestSample || (sample == bestSample && kind < best))
                {
                    bestSample = sample;
                    best = kind;
                }
            };
            auto eligible = [this] (std::int64_t h) { return ! loopActive_ || static_cast<double> (h) < loopEndH_; };

            const int off = earliestOff();
            if (off >= 0 && eligible (slots_[static_cast<std::size_t> (off)].off))
                consider (sampleAt (static_cast<double> (slots_[static_cast<std::size_t> (off)].off)), kOff);
            if (pendingZero_ && (! loopActive_ || 0.0 < loopEndH_))
                consider (sampleAt (0.0), kChase0);
            std::int64_t ccH = 0, keyH = 0, onH = 0;
            const bool hasCc = current_ != nullptr && peek (current_->ccs, ccs_, ccH) && eligible (ccH);
            const bool hasKey = current_ != nullptr && peek (current_->keys, keys_, keyH) && eligible (keyH);
            const bool hasOn = current_ != nullptr && peek (current_->notes, notes_, onH) && eligible (onH);
            if (hasCc)
                consider (sampleAt (static_cast<double> (ccH)), kCc);
            if (hasKey)
                consider (sampleAt (static_cast<double> (keyH)), kKey);
            if (hasOn)
                consider (sampleAt (static_cast<double> (onH)), kOn);

            if (best == kNone)
            {
                // Nothing more before the end of the piece, or before the loop region's end.
                if (! loopActive_)
                    break;
                const std::int64_t wrapAt = sampleAt (loopEndH_);
                if (wrapAt >= end)
                    break;
                const int offset = static_cast<int> (std::max<std::int64_t> (wrapAt, now_) - chunkStart_);
                endSounding (offset, sink);
                anchor (std::max<std::int64_t> (wrapAt, now_), loopStartH_);
                seek (current_, anchorSample_);
                chase (loopStartH_, anchorSample_, sink);
                continue;
            }

            const std::int64_t sample = std::max (bestSample, now_);
            const int offset = static_cast<int> (sample - chunkStart_);
            switch (best)
            {
                case kOff:
                {
                    const auto s = slots_[static_cast<std::size_t> (off)];
                    slots_[static_cast<std::size_t> (off)] = slots_[static_cast<std::size_t> (count_ - 1)];
                    --count_;
                    sink.noteOff (s.channel, s.key, s.tag, offset);
                    break;
                }
                case kChase0:
                    pendingZero_ = false;
                    chase (0.0, sample, sink);
                    break;
                case kCc:
                {
                    const Cc& c = current_->ccs[ccs_.index];
                    const std::int64_t base = cycleBase (ccs_);
                    sink.cc (c.channel, c.controller, c.value, offset, 0);
                    if (c.rampTo > c.pos)
                    {
                        const std::int64_t ramp = sampleAt (static_cast<double> (base + c.rampTo)) - sample;
                        if (ramp > 0)
                            sink.cc (c.channel, c.controller, c.nextValue, offset, static_cast<int> (std::min<std::int64_t> (ramp, std::numeric_limits<int>::max())));
                    }
                    step (current_->ccs, ccs_);
                    break;
                }
                case kKey:
                {
                    const DuckKey& k = current_->keys[keys_.index];
                    sink.keyHit (k.keyLane, k.velocity, offset);
                    step (current_->keys, keys_);
                    break;
                }
                case kOn:
                {
                    const Note& n = current_->notes[notes_.index];
                    const std::int64_t offH = onH + n.len;
                    const std::int64_t length = std::max<std::int64_t> (1, sampleAt (static_cast<double> (offH)) - sample);
                    const int tag = sink.noteOn (n, offset, length, 0);
                    if (tag >= 0)
                        insertSounding (offH, tag, n.channel, n.key, offset, sink);
                    step (current_->notes, notes_);
                    break;
                }
                case kNone:
                    break;
            }
        }
        now_ = end;
    }

    template <class Sink>
    void Scheduler::restart (const Timeline* timeline, Sink& sink) noexcept
    {
        chunkStart_ = now_;
        clearSounding();
        current_ = nullptr;
        if (! playing_)
            return;
        seek (timeline, now_);
        chase (positionAt (now_), now_, sink);
    }

    template <class Sink>
    void Scheduler::chase (double h, std::int64_t at, Sink& sink) noexcept
    {
        pendingZero_ = h < 0.0;
        if (pendingZero_)
            return;
        if (current_ == nullptr)
            return;
        const Timeline& t = *current_;
        const bool loop = t.loops && t.length > 0;
        if (t.loops && t.length <= 0)
            return;
        if (! loop && h >= static_cast<double> (t.length))
            return;
        const int offset = static_cast<int> (at - chunkStart_);
        const std::int64_t length = t.length;
        const std::int64_t cycle = loop ? static_cast<std::int64_t> (std::floor (h / static_cast<double> (length))) : 0;

        // The rides: per (channel, controller), the last point before the chase's sample (the seek left
        // it behind), in this cycle or (a loop) the one before. A point on the chase's sample plays from
        // the stream, as any other.
        if (! t.ccs.empty())
        {
            std::uint32_t found = 0; // bit per (channel x 2 + (controller == 71))
            const std::int64_t base0 = loop ? cycle * length : 0;
            auto before = [this, at] (std::int64_t base, const Cc& c) { return sampleAt (static_cast<double> (base + c.pos)) < at; };
            const auto firstAfter = static_cast<std::size_t> (
                std::partition_point (t.ccs.begin(), t.ccs.end(), [&] (const Cc& c) { return before (base0, c); }) - t.ccs.begin());
            const std::size_t total = t.ccs.size();
            for (std::size_t n = 0; n < total; ++n)
            {
                std::size_t i;
                std::int64_t base = base0;
                if (n < firstAfter)
                    i = firstAfter - 1 - n;
                else if (loop)
                {
                    i = total - 1 - (n - firstAfter);
                    base -= length;
                    if (! before (base, t.ccs[i]))
                        continue;
                }
                else
                    break;
                const Cc& c = t.ccs[i];
                const std::uint32_t bit = 1u << (static_cast<unsigned> (c.channel & 15) * 2u + (c.controller == 71 ? 1u : 0u));
                if ((found & bit) != 0)
                    continue;
                found |= bit;
                const double pointH = static_cast<double> (base + c.pos);
                const double rampEndH = static_cast<double> (base + c.rampTo);
                const std::int64_t ramp = sampleAt (rampEndH) - at;
                if (c.rampTo > c.pos && ramp > 0)
                {
                    const double u = std::clamp ((h - pointH) / (rampEndH - pointH), 0.0, 1.0);
                    const double v = static_cast<double> (c.value) + (static_cast<double> (c.nextValue) - static_cast<double> (c.value)) * u;
                    const auto value = static_cast<std::uint8_t> (std::clamp (std::floor (v + 0.5), 0.0, 127.0));
                    sink.cc (c.channel, c.controller, value, offset, 0);
                    sink.cc (c.channel, c.controller, c.nextValue, offset, static_cast<int> (std::min<std::int64_t> (ramp, std::numeric_limits<int>::max())));
                }
                else
                    sink.cc (c.channel, c.controller, c.rampTo > c.pos ? c.nextValue : c.value, offset, 0);
            }
        }

        // The notes sounding at h that a plain start would miss.
        for (const std::uint32_t index : t.chase)
        {
            if (index >= t.notes.size())
                continue;
            const Note& n = t.notes[index];
            for (std::int64_t c = cycle - (loop ? 1 : 0); c <= cycle; ++c)
            {
                const std::int64_t onH = c * length * (loop ? 1 : 0) + n.pos;
                const std::int64_t offH = onH + n.len;
                const std::int64_t onSample = sampleAt (static_cast<double> (onH));
                const std::int64_t offSample = sampleAt (static_cast<double> (offH));
                if (onSample >= at || offSample <= at)
                    continue; // not sounding there, or a plain note-on still to come
                const int tag = sink.noteOn (n, offset, offSample - at, at - onSample);
                if (tag >= 0)
                    insertSounding (offH, tag, n.channel, n.key, offset, sink);
            }
        }
    }
}
