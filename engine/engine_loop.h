#pragma once

// The engine's block loop (P7): what Engine::process does, as a template over its voices, so engine.cpp
// runs it on P5's Sampler and the tests on voices that record what reaches them. Everything process ()
// calls is noexcept and real-time safe.
//
// Per host block:
// 1. offline (TransportInfo::nonRealtime), a pending Kit is adopted at once (below);
// 2. sync: the scheduler follows the transport (a start, stop, jump, wrap or tempo change), and the voices
//    get its tempo (Sampler::setTempo) and the lane levels;
// 3. pieces: the block is played in pieces that never cross a multiple of 32 samples, counted from the
//    last prepare () or reset () (the design's 32-sample control frames: per-piece work in the voices
//    and the mixer then lands on the same samples whatever the host's block size). Per piece: a kit
//    switch, when one is due (below); the bus and sends are cleared; the scheduler plays the timeline
//    into the voices, the duck keys and the controllers into the mixer; the router plays the piece's
//    MIDI in; the voices render; the mixer mixes into the output;
// 4. retired kits and the telemetry.
//
// Kit switches. A Kit the service publishes is adopted:
// - at once when there is none yet, and at the start of an offline block (TransportInfo::nonRealtime:
//   a D5 render that waited for its Kit starts with it on its first sample), with no fades;
// - while stopped, after the output fades out over 20 ms;
// - while playing, at the next 4/4 bar line (the scheduler's: a multiple of 4 quarters), the output
//   fading out over the 20 ms before it.
// Only while the retire FIFO has room and a fading slot is free. The pending Kit is taken when its
// switch is planned (while playing, within 20 ms of the bar line, so a later publish still wins until
// then). At the switch the old Kit's voices fade (the sampler's 5 ms), the mixer takes the new mix and
// loses its tails, the sounding notes are forgotten and the new Kit's timeline is chased; then the
// output fades back in over 20 ms. A Kit no voice holds any more goes into the retire FIFO.
// A Kit published while it is the adopted one (the current kit picked again; or A, B, A) isn't a
// switch: it goes straight into the retire FIFO, for the service to drop that publish's reference, with
// no fade and no restart.
//
// What the voices get (P5's NoteOn, docs/plugin/manifest.md):
// - a note's instrument: its KeyTarget's a, or b when its A/B switch is on B, resolved at its note-on;
// - startFrame: a chased note's time since its start (48 kHz frames before pitch), plus the pre-roll an
//   instrument has beyond what the timeline moved the note (an A/B pair moves by the smaller); a live
//   note skips its pre-roll, starting on its onset, since it can't be anticipated;
// - lengthSamples for align=end instruments; rr and panIndex; dip and continues from the note's flags;
// - throws: (1 - the channel's send level) x cc / 127 x the channel's static gain (P8's staticGain);
// - phrase lanes (sched/timeline.h's three kinds of note): a phrase decides once per onset cycle whether
//   it plays slices (playsSlices: phrase lanes on Auto and the tempo within the phrase's tolerance of
//   the kit's), latched per phrase on (the scheduler's epoch, the cycle of the note's onset as a host
//   position), so a tempo change never splits a cycle and a start, jump or wrap decides afresh. A slice
//   note plays its slice (the stem its A/B picks) only when the phrase does, with no note-off; its
//   throws come from the phrase's channel (none for a "baked" phrase), held to the end of the note it
//   slices (lengthSamples, from Timeline::sliceThrough), and it continues into the next slice only within
//   20 ppm of the kit's tempo. A multisample twin, or a note a slice holds, plays the
//   multisample only when the phrase doesn't, and lights its lane either way; a slice note never does.
//   Live notes never play slices.

#include "engine.h"
#include "mix/channel.h"
#include "mix/mixer.h"
#include "rt/pending_slot.h"
#include "rt/retire_fifo.h"
#include "sampler/phrase_voice.h"
#include "sampler/sampler.h"
#include "sched/midi_router.h"
#include "sched/scheduler.h"
#include "sched/timeline_builder.h"
#include "util/ints.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <variant>

namespace wubansia::engine
{
    namespace detail
    {
        // An instrument's pre-roll at a key, in 48 kHz frames of time (its files' played at the key's pitch).
        inline double prerollAt (const Kit& kit, int instrument, int key) noexcept
        {
            if (instrument < 0 || static_cast<std::size_t> (instrument) >= kit.instruments.size())
                return 0.0;
            const Instrument& in = kit.instruments[static_cast<std::size_t> (instrument)];
            for (int z = in.firstZone; z < in.firstZone + in.zoneCount && static_cast<std::size_t> (z) < kit.zones.size(); ++z)
            {
                const Zone& zone = kit.zones[static_cast<std::size_t> (z)];
                if (key < zone.lo || key > zone.hi)
                    continue;
                if (zone.layerCount == 0 || zone.firstLayer >= kit.layers.size())
                    return 0.0;
                const Layer& layer = kit.layers[zone.firstLayer];
                if (layer.rrCount == 0 || layer.firstRr >= kit.rr.size())
                    return 0.0;
                const auto file = kit.rr[layer.firstRr];
                if (file >= kit.files.size())
                    return 0.0;
                return prerollTime (kit.files[file].prerollFrames, key - zone.root);
            }
            return 0.0;
        }

        inline void raisePeak (std::atomic<float>& peak, float value) noexcept
        {
            float current = peak.load (std::memory_order_relaxed);
            while (value > current && ! peak.compare_exchange_weak (current, value, std::memory_order_relaxed)) {}
        }
    }

    template <class Voices>
    class BlockLoop
    {
    public:
        static constexpr int kControlFrame = 32;
        static constexpr int kFadingKits = 8; // kits whose voices are still fading out
        static constexpr double kStopFadeSeconds = 0.010; // sequenced voices at a stop or jump
        static constexpr double kKitFadeSeconds = 0.020; // the output before and after a kit switch
        static constexpr double kSoundOffSeconds = 0.005; // live voices at CC120
        static constexpr int kMaxLatchedPhrases = 32; // phrases with a latched decision; any further decide per note
        static constexpr double kContinuePpm = 20.0; // slice notes continue within this of the kit's tempo

        // Message thread, the audio thread stopped: sizes everything for this rate and 1 or 2 outputs, with
        // the club master's limiter holding as `hold` says. Voices, position and fades reset; the adopted
        // Kit stays.
        void prepare (double sampleRate, int maxBlock, int numOutputs, MasterHold hold = MasterHold::lookBack)
        {
            rate_ = sampleRate > 0.0 && std::isfinite (sampleRate) ? sampleRate : 48000.0;
            (void) maxBlock; // pieces are at most kControlFrame, whatever the host's block
            outputs_ = std::clamp (numOutputs, 1, 2);
            voices_.prepare (rate_, kControlFrame);
            mixer_.prepare (rate_, kControlFrame, outputs_, hold);
            mixer_.setConfig (kit_ != nullptr ? &kit_->mix : nullptr);
            stopFadeSamples_ = std::max (1, static_cast<int> (std::lround (kStopFadeSeconds * rate_)));
            kitFadeSamples_ = std::max (1, static_cast<int> (std::lround (kKitFadeSeconds * rate_)));
            soundOffSamples_ = std::max (1, static_cast<int> (std::lround (kSoundOffSeconds * rate_)));
            restart();
        }

        // Audio thread: every voice and tail silent at once; stopped, at sample 0. The adopted Kit stays.
        void reset() noexcept
        {
            voices_.reset();
            mixer_.reset();
            restart();
        }

        template <std::size_t N>
        void process (float* const* out, int numOutputs, int numSamples, const TransportInfo& transport, const Controls& controls,
            const MidiEvent* midi, int numMidi, PendingSlot<Kit>& pending, RetireFifo<Kit, N>& retired, Telemetry& telemetry) noexcept;

        int latencySamples() const noexcept { return mixer_.latencySamples(); }
        double tailSeconds() const noexcept { return mixer_.tailSeconds(); }

        const Kit* kit() const noexcept { return kit_; }
        Voices& voices() noexcept { return voices_; }
        const Scheduler& scheduler() const noexcept { return scheduler_; }
        const MidiRouter& router() const noexcept { return router_; }
        std::int64_t sampleIndex() const noexcept { return absolute_; }
        int fadingKits() const noexcept { return fadingCount_; }

        // The Phrases drawer's overlay (ADR 0031): the hand-off it takes clips from (the Engine's; nullptr: none).
        void setPhrases (PhraseHandoff* phrases) noexcept { phrases_ = phrases; }
        // Audio thread, a bypassed block: the offer goes back unplayed (while the retire FIFO has room).
        void dropPhraseOffer() noexcept
        {
            if (phrases_ != nullptr && phrases_->pending.pending() && phrases_->retired.hasSpace())
                phrases_->retired.push (phrases_->pending.take());
        }
        const Scheduler& overlay() const noexcept { return overlay_; }

    private:
        // The scheduler's Sink: sequenced notes, controllers, duck keys, stops.
        struct SequencerSink
        {
            BlockLoop& loop;
            const Controls& controls;

            int noteOn (const Note& note, int offset, std::int64_t lengthSamples, std::int64_t chaseSamples, std::int64_t startH) noexcept
            {
                return loop.sequencedOn (note, offset, lengthSamples, chaseSamples, startH, controls);
            }

            void noteOff (std::uint8_t, std::uint8_t key, int tag, int offset) noexcept
            {
                loop.voices_.noteOff (tag, key, offset, Source::sequencer);
            }

            void cc (std::uint8_t channel, std::uint8_t controller, std::uint8_t value, int offset, int rampSamples) noexcept
            {
                loop.controller (channel, controller, value, offset, rampSamples);
            }

            void keyHit (std::uint8_t lane, std::uint8_t velocity, int offset) noexcept { loop.mixer_.keyHit (lane, velocity, offset); }

            void stopSequenced (int offset) noexcept { loop.voices_.fadeOut (Source::sequencer, offset, loop.stopFadeSamples_); }
        };

        // The router's Sink: MIDI in. Its offsets are the block's; the piece starts at pieceStart.
        struct LiveSink
        {
            BlockLoop& loop;
            const Controls& controls;
            int pieceStart;
            int pieceLength;

            int at (int offset) const noexcept { return std::clamp (offset - pieceStart, 0, pieceLength - 1); }

            void liveNoteOn (const LiveNote& note) noexcept { loop.liveOn (note, at (note.offset), controls); }

            void liveNoteOff (int instrument, std::uint8_t key, int offset) noexcept
            {
                loop.voices_.noteOff (instrument, key, at (offset), Source::live);
            }

            void cc (std::uint8_t channel, std::uint8_t controller, std::uint8_t value, int offset, int rampSamples) noexcept
            {
                loop.controller (channel, controller, value, at (offset), rampSamples);
            }

            void keyHit (std::uint8_t lane, std::uint8_t velocity, int offset) noexcept { loop.mixer_.keyHit (lane, velocity, at (offset)); }

            void liveReleaseAll (int offset) noexcept { loop.voices_.releaseAll (Source::live, at (offset)); }

            void liveSoundOff (int offset) noexcept { loop.voices_.fadeOut (Source::live, at (offset), loop.soundOffSamples_); }
        };

        // The overlay's Sink: the phrase clip's notes on Source::audition; no controllers, no duck keys.
        struct OverlaySink
        {
            BlockLoop& loop;
            const Controls& controls;

            int noteOn (const Note& note, int offset, std::int64_t lengthSamples, std::int64_t chaseSamples, std::int64_t) noexcept
            {
                return loop.auditionOn (note, offset, lengthSamples, chaseSamples, controls);
            }
            void noteOff (std::uint8_t, std::uint8_t key, int tag, int offset) noexcept { loop.voices_.noteOff (tag, key, offset, Source::audition); }
            void cc (std::uint8_t, std::uint8_t, std::uint8_t, int, int) noexcept {}
            void keyHit (std::uint8_t, std::uint8_t, int) noexcept {}
            void stopSequenced (int offset) noexcept { loop.voices_.fadeOut (Source::audition, offset, loop.stopFadeSamples_); }
        };

        void restart() noexcept
        {
            scheduler_.prepare (rate_);
            overlay_.prepare (rate_);
            dropClip();
            router_.reset();
            absolute_ = 0;
            // A Kit already taken for a switch is adopted at once on the next block, so it is never lost.
            switchAt_ = next_ != nullptr ? 0 : -1;
            fadeFrom_ = 0;
            switchedAt_ = 0;
            fadeInEnd_ = 0;
        }

        const Timeline* timelineFor (const Controls& controls) const noexcept
        {
            const int index = timelineIndex (controls.mode, controls.variant);
            return kit_ != nullptr && index >= 0 ? &kit_->timelines[static_cast<std::size_t> (index)] : nullptr;
        }

        double kitBpm() const noexcept { return kit_ != nullptr && kit_->info.bpm > 0 ? kit_->info.bpm : 120.0; }

        int resolve (const KeyTarget& target, const Controls& controls) const noexcept
        {
            int instrument = target.a;
            if (target.b >= 0 && target.ab >= 0 && target.ab < kAbSwitches && controls.ab[static_cast<std::size_t> (target.ab)] != 0)
                instrument = target.b;
            return instrument >= 0 && static_cast<std::size_t> (instrument) < kit_->instruments.size() ? instrument : -1;
        }

        // A slice note's samples from its note-on to its twin's end (Timeline::sliceThrough), or 0. The note
        // is an element of the scheduler's timeline, so its index is where it sits there.
        std::int64_t throughSamples (const Note& note, std::int64_t startH, int offset) const noexcept
        {
            const Timeline* t = scheduler_.timeline();
            if (t == nullptr || t->notes.empty() || &note < t->notes.data() || &note >= t->notes.data() + t->notes.size())
                return 0;
            const auto index = static_cast<std::uint32_t> (&note - t->notes.data());
            const auto it = std::lower_bound (t->sliceThrough.begin(), t->sliceThrough.end(), index,
                [] (const SliceThrough& s, std::uint32_t i) { return s.note < i; });
            if (it == t->sliceThrough.end() || it->note != index)
                return 0;
            const std::int64_t end = scheduler_.sampleAt (static_cast<double> (startH + it->through));
            return std::max<std::int64_t> (0, end - (scheduler_.sampleIndex() + offset));
        }

        // Whether phrase p plays slices for the note whose onset is onsetH: decided once per (epoch, onset
        // cycle) and latched.
        bool phraseDecides (int p, std::int64_t onsetH, const Controls& controls) noexcept
        {
            const Phrase& phrase = kit_->phrases[static_cast<std::size_t> (p)];
            const std::int64_t cycleLength = std::int64_t { std::max (1, phrase.cycle16ths) } * kMilliticksPerSixteenth;
            const std::int64_t cycle = util::floorDiv (onsetH, cycleLength);
            const std::uint32_t epoch = scheduler_.epoch();
            auto decide = [&] { return playsSlices (phrase, kit_->info.bpm, scheduler_.bpm(), controls.phrases); };
            if (p >= kMaxLatchedPhrases)
                return decide();
            auto& latch = latches_[static_cast<std::size_t> (p)];
            if (! latch.valid || latch.epoch != epoch || latch.cycle != cycle)
                latch = { epoch, cycle, decide(), true };
            return latch.slices;
        }

        // The voice's direct sends for a note's latched throws (0-127) on a mix channel.
        void throws (int mixChannel, std::uint8_t space, std::uint8_t echo, const Controls& controls, NoteOn& on) const noexcept
        {
            if ((space == 0 && echo == 0) || mixChannel < 0 || static_cast<std::size_t> (mixChannel) >= kit_->mix.channels.size())
                return;
            const Channel& channel = kit_->mix.channels[static_cast<std::size_t> (mixChannel)];
            const float gain = mix::staticGain (channel, controls.variant, controls.ab);
            auto send = [&] (Send which, std::uint8_t value) {
                const float base = static_cast<float> (channel.sendsPm[static_cast<std::size_t> (which)]) / 1000.0f;
                return (1.0f - base) * static_cast<float> (value) / 127.0f * gain;
            };
            on.throwSpace = space > 0 ? send (Send::space, space) : 0.0f;
            on.throwEcho = echo > 0 ? send (Send::echo, echo) : 0.0f;
        }

        int sequencedOn (const Note& note, int offset, std::int64_t lengthSamples, std::int64_t chaseSamples, std::int64_t startH,
            const Controls& controls) noexcept
        {
            if (kit_ == nullptr)
                return -1;
            const Kit& kit = *kit_;
            const KeyTarget& target = kit.keys[note.channel & 15u][note.key & 127u];
            const int instrument = resolve (target, controls);
            if (instrument < 0)
                return -1;
            const Instrument& in = kit.instruments[static_cast<std::size_t> (instrument)];
            if ((asideLanes_ & laneBit (in.lane)) != 0)
                return -1; // the Phrases drawer's clip plays this lane now; notes already sounding end at their own note-offs

            // A phrase's notes: its decision for this onset cycle picks the slice note or the multisample.
            const bool phraseNote = note.phrase >= 0 && static_cast<std::size_t> (note.phrase) < kit.phrases.size();
            const bool sliceNote = phraseNote && note.slice >= 0;
            if (phraseNote)
            {
                const bool slices = phraseDecides (note.phrase, startH + note.preroll, controls);
                if (! sliceNote)
                    hits_ |= laneBit (in.lane); // the twin lights its lane, played or not
                if (slices != sliceNote)
                    return -1;
            }

            NoteOn on;
            on.instrument = static_cast<std::int16_t> (instrument);
            on.key = note.key;
            on.velocity = note.velocity;
            on.rr = note.rr;
            on.panIndex = note.panIndex;
            on.dip = (note.flags & NoteFlag::dip) != 0;
            on.source = Source::sequencer;
            on.offset = offset;

            double frames = chaseSamples > 0 ? static_cast<double> (chaseSamples) * kContentSampleRate / rate_ : 0.0;
            int mixChannel = in.mixChannel;
            bool throwsLive = true;
            if (sliceNote)
            {
                const Phrase& phrase = kit.phrases[static_cast<std::size_t> (note.phrase)];
                on.phrase = note.phrase;
                on.slice = note.slice;
                on.character = phrase.ab >= 0 && phrase.ab < kAbSwitches ? controls.ab[static_cast<std::size_t> (phrase.ab)] : 0;
                // Off the kit's exact tempo the next slice's note won't land where this one ends: each is
                // its own head.
                on.continues = (note.flags & NoteFlag::continues) != 0
                               && std::abs (scheduler_.bpm() - kit.info.bpm) <= kContinuePpm * 1.0e-6 * kit.info.bpm;
                mixChannel = phrase.mixChannel;
                throwsLive = phrase.throwMode != PhraseThrow::baked;
                if (throwsLive)
                    on.lengthSamples = throughSamples (note, startH, offset);
            }
            else
            {
                on.continues = (note.flags & NoteFlag::continues) != 0;
                on.lengthSamples = in.align == Align::end ? std::max<std::int64_t> (1, lengthSamples) : 0;
                // The pre-roll this instrument has beyond the timeline's shift (the smaller of an A/B pair's).
                const double own = detail::prerollAt (kit, instrument, note.key);
                if (own > 0.0 && target.b >= 0)
                    frames += own - std::min (detail::prerollAt (kit, target.a, note.key), detail::prerollAt (kit, target.b, note.key));
            }
            on.startFrame = static_cast<std::int64_t> (std::floor (frames + 0.5));
            if (throwsLive)
                throws (mixChannel, note.throwSpace, note.throwEcho, controls, on);

            voices_.noteOn (kit, on);
            if (sliceNote)
                return -1; // a slice has no note-off: no sounding entry
            if (! phraseNote)
                hits_ |= laneBit (in.lane);
            return instrument;
        }

        // A phrase clip's note: the instrument its key resolves to, as a sequenced note that isn't a phrase's.
        int auditionOn (const Note& note, int offset, std::int64_t lengthSamples, std::int64_t chaseSamples, const Controls& controls) noexcept
        {
            if (kit_ == nullptr)
                return -1;
            const Kit& kit = *kit_;
            const KeyTarget& target = kit.keys[note.channel & 15u][note.key & 127u];
            const int instrument = resolve (target, controls);
            if (instrument < 0)
                return -1;
            const Instrument& in = kit.instruments[static_cast<std::size_t> (instrument)];
            NoteOn on;
            on.instrument = static_cast<std::int16_t> (instrument);
            on.key = note.key;
            on.velocity = note.velocity;
            on.rr = note.rr;
            on.panIndex = note.panIndex;
            on.dip = (note.flags & NoteFlag::dip) != 0;
            on.continues = (note.flags & NoteFlag::continues) != 0;
            on.source = Source::audition;
            on.offset = offset;
            on.lengthSamples = in.align == Align::end ? std::max<std::int64_t> (1, lengthSamples) : 0;
            double frames = chaseSamples > 0 ? static_cast<double> (chaseSamples) * kContentSampleRate / rate_ : 0.0;
            const double own = detail::prerollAt (kit, instrument, note.key);
            if (own > 0.0 && target.b >= 0)
                frames += own - std::min (detail::prerollAt (kit, target.a, note.key), detail::prerollAt (kit, target.b, note.key));
            on.startFrame = static_cast<std::int64_t> (std::floor (frames + 0.5));
            throws (in.mixChannel, note.throwSpace, note.throwEcho, controls, on);
            voices_.noteOn (kit, on);
            hits_ |= laneBit (in.lane);
            return instrument;
        }

        // A clip let go: into the retire FIFO, or held silently until there's room.
        void retireClip (const PhraseClip* clip) noexcept
        {
            if (clip == nullptr || phrases_ == nullptr)
                return;
            if (phrases_->retired.push (clip))
                return;
            for (auto& slot : limbo_)
                if (slot == nullptr)
                {
                    slot = clip;
                    return;
                }
        }

        // The clip in hand goes, unfaded (a reset, a prepare, a kit switch); the overlay forgets it.
        void dropClip() noexcept
        {
            if (clip_ != nullptr)
                retireClip (clip_);
            clip_ = nullptr;
            asideLanes_ = 0;
            overlay_.reset();
        }

        // The clip in hand goes; its voices fade over 10 ms.
        void letGoClip() noexcept
        {
            if (clip_ == nullptr)
                return;
            voices_.fadeOut (Source::audition, 0, stopFadeSamples_);
            dropClip();
        }

        // At a block's start, after the transport: the idle drop, offline renders, a stopped ticket, then a new offer
        // (anchored on the main scheduler's position: the next chord line while playing, the one at or before it while
        // stopped); then the overlay follows the main scheduler, shifted by the anchor.
        void phrasesAtBlockStart (const TransportInfo& transport, const Controls& controls) noexcept
        {
            if (phrases_ == nullptr)
                return;
            const bool idle = phrases_->countBlock();
            for (auto& slot : limbo_)
                if (slot != nullptr && phrases_->retired.push (slot))
                    slot = nullptr;
            const bool limboEmpty = limbo_[0] == nullptr && limbo_[1] == nullptr;
            const std::uint32_t stopped = phrases_->stopTicket();
            if (clip_ != nullptr && (idle || transport.nonRealtime || clip_->ticket <= stopped))
                letGoClip();
            if (kit_ != nullptr && limboEmpty && phrases_->pending.pending() && phrases_->retired.hasSpace())
            {
                const PhraseClip* offered = phrases_->pending.take();
                if (offered != nullptr && (transport.nonRealtime || offered->kitIndex != kit_->info.index || offered->ticket <= stopped))
                    phrases_->retired.push (offered); // there was room: hasSpace above
                else if (offered != nullptr)
                {
                    letGoClip();
                    clip_ = offered;
                    asideLanes_ = offered->lanes;
                    const auto pos = scheduler_.position();
                    const double at = pos.ppq * static_cast<double> (kMilliticksPerQuarter) / static_cast<double> (offered->alignH);
                    anchorH_ = offered->alignH * static_cast<std::int64_t> (pos.playing ? std::ceil (at - 1.0e-9) : std::floor (at + 1.0e-9));
                }
            }
            if (clip_ == nullptr)
                return;
            const auto pos = scheduler_.position();
            const double posH = pos.ppq * static_cast<double> (kMilliticksPerQuarter);
            if (pos.playing && posH - static_cast<double> (anchorH_) < -static_cast<double> (clip_->alignH))
            {
                // the host looped or jumped back before the clip's line: re-anchored at the line at or before it
                anchorH_ = clip_->alignH * static_cast<std::int64_t> (std::floor (posH / static_cast<double> (clip_->alignH) + 1.0e-9));
            }
            const double anchorQ = static_cast<double> (anchorH_) / static_cast<double> (kMilliticksPerQuarter);
            TransportInfo shifted;
            shifted.hasPosition = true;
            shifted.playing = pos.playing;
            shifted.ppq = pos.ppq - anchorQ;
            shifted.bpm = pos.bpm;
            shifted.nonRealtime = transport.nonRealtime;
            if (transport.hasPosition && transport.looping)
            {
                shifted.looping = true;
                shifted.loopStartPpq = transport.loopStartPpq - anchorQ;
                shifted.loopEndPpq = transport.loopEndPpq - anchorQ;
            }
            OverlaySink sink { *this, controls };
            overlay_.sync (&clip_->timeline, shifted, kitBpm(), sink);
        }

        void liveOn (const LiveNote& note, int offset, const Controls& controls) noexcept
        {
            if (kit_ == nullptr || note.instrument < 0 || static_cast<std::size_t> (note.instrument) >= kit_->instruments.size())
                return;
            const Kit& kit = *kit_;
            const Instrument& in = kit.instruments[static_cast<std::size_t> (note.instrument)];
            NoteOn on;
            on.instrument = note.instrument;
            on.key = note.key;
            on.velocity = note.velocity;
            on.rr = note.rr;
            on.panIndex = note.rr;
            on.source = Source::live;
            on.offset = offset;
            on.startFrame = static_cast<std::int64_t> (std::floor (detail::prerollAt (kit, note.instrument, note.key) + 0.5));
            throws (in.mixChannel, note.throwSpace, note.throwEcho, controls, on);
            voices_.noteOn (kit, on);
            hits_ |= laneBit (in.lane);
        }

        // A controller on a MIDI channel reaches every mix channel Kit::ccMixChannels names for it.
        void controller (std::uint8_t channel, std::uint8_t number, std::uint8_t value, int offset, int rampSamples) noexcept
        {
            if (kit_ == nullptr)
                return;
            const std::uint32_t mask = kit_->ccMixChannels[channel & 15u];
            for (int c = 0; c < kMaxMixChannels; ++c)
                if ((mask & (1u << c)) != 0)
                    mixer_.cc (c, number, value, offset, rampSamples);
        }

        // Takes the pending Kit for a switch when one can be adopted. The adopted Kit itself goes straight
        // back through the retire FIFO; another becomes next_. Whether next_ is set.
        template <std::size_t N>
        bool takePending (PendingSlot<Kit>& pending, RetireFifo<Kit, N>& retired) noexcept
        {
            if (next_ != nullptr)
                return true;
            if (! pending.pending() || ! retired.hasSpace() || fadingCount_ >= kFadingKits)
                return false;
            const Kit* kit = pending.take();
            if (kit == nullptr)
                return false;
            if (kit == kit_)
            {
                retired.push (kit); // there was room: hasSpace above
                return false;
            }
            next_ = kit;
            return true;
        }

        // Plans a pending Kit's switch (see the top).
        template <std::size_t N>
        void planSwitch (PendingSlot<Kit>& pending, RetireFifo<Kit, N>& retired) noexcept
        {
            if (switchAt_ >= 0)
                return;
            if (kit_ != nullptr && scheduler_.playing())
            {
                // The next bar line, once it is near: then the Kit is taken and the fade begins.
                if (next_ == nullptr && ! pending.pending())
                    return;
                const int bar = scheduler_.barLineWithin (kitFadeSamples_ + kControlFrame);
                if (bar < 0 || ! takePending (pending, retired))
                    return;
                switchAt_ = absolute_ + bar;
                fadeFrom_ = std::max (absolute_, switchAt_ - kitFadeSamples_);
                return;
            }
            if (! takePending (pending, retired))
                return;
            fadeFrom_ = absolute_;
            switchAt_ = kit_ == nullptr ? absolute_ : absolute_ + kitFadeSamples_;
        }

        // The switch to next_, on the current piece's first sample; fadeIn: the output comes back over
        // 20 ms (not for the first Kit, nor offline).
        void doSwitch (const Controls& controls, Telemetry& telemetry, bool fadeIn) noexcept
        {
            switchAt_ = -1;
            const Kit* next = next_;
            next_ = nullptr;
            const bool hadKit = kit_ != nullptr;
            switchedAt_ = absolute_;
            fadeInEnd_ = hadKit && fadeIn ? absolute_ + kitFadeSamples_ : absolute_;
            if (next == nullptr)
                return;
            if (hadKit)
            {
                voices_.fadeKit (kit_, 0);
                fading_[static_cast<std::size_t> (fadingCount_++)] = kit_;
            }
            if (clip_ != nullptr && clip_->kitIndex != next->info.index)
                dropClip(); // its voices fade with the old Kit
            kit_ = next;
            mixer_.setConfig (&next->mix);
            if (hadKit)
                mixer_.reset();
            router_.forgetNotes();
            router_.resetRoundRobins();
            SequencerSink seq { *this, controls };
            scheduler_.restart (timelineFor (controls), seq);
            telemetry.kitIndex.store (next->info.index, std::memory_order_relaxed);
        }

        // The output's gain across a kit switch, per sample.
        void applyFade (float* const* chunk, int outs, int n) const noexcept
        {
            const bool fadingOut = switchAt_ >= 0 && absolute_ + n > fadeFrom_;
            const bool fadingIn = absolute_ < fadeInEnd_ && absolute_ + n > switchedAt_;
            if (! fadingOut && ! fadingIn)
                return;
            for (int i = 0; i < n; ++i)
            {
                const std::int64_t s = absolute_ + i;
                double gain = 1.0;
                if (switchAt_ >= 0 && s >= fadeFrom_ && s < switchAt_)
                    gain = static_cast<double> (switchAt_ - s) / static_cast<double> (switchAt_ - fadeFrom_);
                else if (s >= switchedAt_ && s < fadeInEnd_)
                    gain = static_cast<double> (s - switchedAt_) / static_cast<double> (fadeInEnd_ - switchedAt_);
                else
                    continue;
                for (int c = 0; c < outs; ++c)
                    chunk[c][i] = static_cast<float> (static_cast<double> (chunk[c][i]) * gain);
            }
        }

        template <std::size_t N>
        void retireFaded (RetireFifo<Kit, N>& retired) noexcept
        {
            // Oldest first, so the service sees them in the order they went.
            while (fadingCount_ > 0 && ! voices_.holdsKit (fading_[0]) && retired.push (fading_[0]))
            {
                for (int i = 1; i < fadingCount_; ++i)
                    fading_[static_cast<std::size_t> (i - 1)] = fading_[static_cast<std::size_t> (i)];
                --fadingCount_;
            }
        }

        Voices voices_;
        Mixer mixer_;
        Scheduler scheduler_;
        Scheduler overlay_; // the phrase clip's
        PhraseHandoff* phrases_ = nullptr;
        const PhraseClip* clip_ = nullptr; // the clip in hand: waiting for its line, or sounding
        std::array<const PhraseClip*, 2> limbo_ {}; // let go while the retire FIFO was full
        std::int64_t anchorH_ = 0; // milliticks of the host position where the clip's bar 1 plays
        std::uint32_t asideLanes_ = 0; // the clip's lanes: the timeline's notes there sit out
        MidiRouter router_;

        double rate_ = 48000.0;
        int outputs_ = 2;
        int stopFadeSamples_ = 480;
        int kitFadeSamples_ = 960;
        int soundOffSamples_ = 240;
        std::int64_t absolute_ = 0; // the next sample, from prepare () or reset ()
        std::uint32_t hits_ = 0; // lanes struck in this block
        std::array<float, kLanes> peaks_ {}; // the mixer's lane peaks over this block's pieces
        float reduction_ = 0.0f; // and its deepest gain reduction

        const Kit* kit_ = nullptr;
        const Kit* next_ = nullptr; // taken from the pending slot for a planned switch

        struct Latch
        {
            std::uint32_t epoch = 0;
            std::int64_t cycle = 0;
            bool slices = false;
            bool valid = false;
        };
        std::array<Latch, kMaxLatchedPhrases> latches_ {}; // per phrase: its decision for one onset cycle
        std::array<const Kit*, kFadingKits> fading_ {};
        int fadingCount_ = 0;

        std::int64_t switchAt_ = -1; // a planned switch's sample, or -1
        std::int64_t fadeFrom_ = 0; // where the output starts fading out before it
        std::int64_t switchedAt_ = 0; // the last switch's sample
        std::int64_t fadeInEnd_ = 0; // where the output is back at unity after it
    };

    template <class Voices>
    template <std::size_t N>
    void BlockLoop<Voices>::process (float* const* out, int numOutputs, int numSamples, const TransportInfo& transport,
        const Controls& controls, const MidiEvent* midi, int numMidi, PendingSlot<Kit>& pending, RetireFifo<Kit, N>& retired,
        Telemetry& telemetry) noexcept
    {
        if (out == nullptr || numOutputs <= 0 || numSamples <= 0)
        {
            retireFaded (retired);
            return;
        }
        const int outs = std::min (numOutputs, 2);
        const int mixOuts = std::min (outs, outputs_);
        hits_ = 0;
        peaks_.fill (0.0f);
        reduction_ = 0.0f;

        // Offline, a pending Kit is adopted on the block's first sample, before the transport is read.
        const bool offline = transport.nonRealtime;
        if (offline && takePending (pending, retired))
            doSwitch (controls, telemetry, false);

        // 1. The transport; 2. a switch due now (the first Kit at once, or a bar line on this sample).
        {
            SequencerSink seq { *this, controls };
            const auto sync = scheduler_.sync (timelineFor (controls), transport, kitBpm(), seq);
            if (sync == SyncEvent::started)
                router_.resetRoundRobins();
        }
        phrasesAtBlockStart (transport, controls);
        voices_.setLaneGains (controls.laneGain);
        const double bpm = scheduler_.bpm();
        voices_.setTempo (bpm); // the same tempo playsSlices decides with

        // 3. The pieces.
        int done = 0;
        int nextMidi = 0;
        while (done < numSamples)
        {
            if (! offline)
                planSwitch (pending, retired);
            if (switchAt_ == absolute_)
                doSwitch (controls, telemetry, true);

            int piece = std::min (numSamples - done, kControlFrame - static_cast<int> (absolute_ % kControlFrame));
            if (switchAt_ > absolute_ && switchAt_ < absolute_ + piece)
                piece = static_cast<int> (switchAt_ - absolute_);

            mixer_.bus().clear (piece);
            mixer_.sends().clear (piece);
            {
                SequencerSink seq { *this, controls };
                scheduler_.advance (timelineFor (controls), piece, seq);
            }
            if (clip_ != nullptr)
            {
                OverlaySink ov { *this, controls };
                overlay_.advance (&clip_->timeline, piece, ov);
            }

            // The piece's MIDI in (offsets are the block's; anything late plays at the piece's start, and
            // anything past the block's end in its last piece).
            const bool lastPiece = done + piece == numSamples;
            int last = nextMidi;
            while (last < numMidi && midi != nullptr && (lastPiece || midi[last].offset < done + piece))
                ++last;
            if (last > nextMidi && kit_ != nullptr)
            {
                LiveSink live { *this, controls, done, piece };
                router_.route (*kit_, midi + nextMidi, last - nextMidi, controls, live);
            }
            nextMidi = last;

            voices_.render (mixer_.bus(), mixer_.sends(), piece);
            float* chunk[2] { out[0] + done, outs > 1 ? out[1] + done : nullptr };
            mixer_.process (chunk, mixOuts, piece, controls, bpm);
            if (outs > mixOuts)
                std::copy_n (chunk[0], piece, chunk[1]);
            applyFade (chunk, outs, piece);
            // The meters are the last process ()'s: keep the block's highest.
            const auto& meters = mixer_.meters();
            for (std::size_t lane = 0; lane < static_cast<std::size_t> (kLanes); ++lane)
                peaks_[lane] = std::max (peaks_[lane], meters.lanePeak[lane]);
            reduction_ = std::max (reduction_, meters.gainReduction);

            absolute_ += piece;
            done += piece;
        }

        // 4. Retired kits, and the telemetry.
        retireFaded (retired);
        telemetry.laneHits.fetch_or (hits_, std::memory_order_relaxed);
        telemetry.phraseRecorded.store (voices_.phraseLanes(), std::memory_order_relaxed);
        for (std::size_t lane = 0; lane < static_cast<std::size_t> (kLanes); ++lane)
            detail::raisePeak (telemetry.lanePeak[lane], peaks_[lane]);
        telemetry.gainReduction.store (reduction_, std::memory_order_relaxed);
        const auto position = scheduler_.position();
        telemetry.songTicks.store (position.playing ? position.ticks : -1.0, std::memory_order_relaxed);
        telemetry.section.store (position.playing ? position.section : -1, std::memory_order_relaxed);
        telemetry.voices.store (voices_.activeVoices(), std::memory_order_relaxed);
        telemetry.kitIndex.store (kit_ != nullptr ? kit_->info.index : -1, std::memory_order_relaxed);
        telemetry.phraseSounding.store (clip_ != nullptr ? clip_->ticket : 0u, std::memory_order_relaxed);
        telemetry.phraseStartTicks.store (clip_ != nullptr ? anchorH_ / kMilliticksPerTick : -1, std::memory_order_relaxed);
    }
}
