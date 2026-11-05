#pragma once

// The sampler (P5): every multisample voice the engine plays, and the phrase voices (P6) beside them.
//
// The engine (P7) drives it per piece of audio:
// 1. it queues the piece's note events: noteOn (), noteOff (), releaseAll (), fadeOut (), fadeKit (), each
//    at a sample offset within the next render () call;
// 2. it calls render (), which adds every sounding voice into its mix channel's stereo bus (MixBus) and the
//    thrown part of each thrown voice into the Space and Echo send inputs (SendBus).
// Pieces never cross a multiple of 32 samples counted from the last prepare () or reset (), and every
// per-sample decision here counts from there too, so the output doesn't depend on the host's block size.
// Events at the same offset apply in the order they were queued: the engine queues note-offs, then
// note-ons.
//
// The rules come from the recipes (phrase.hits_lane, context4.zoned_lane, choked_lane and bass_voice,
// basslines.zone_of, layer_of and hermite) and docs/plugin/manifest.md:
// - Zone: the instrument's zone whose lo..hi holds the key (the exporter sets the ranges so that is the
//   highest root at or below the key, the lowest zone extending down); the key repitches it by
//   (key - root) semitones. A key no zone holds plays nothing.
// - Layer: the nearest layer velocity, a tie to the louder (selection.h). Gain: the layer alone
//   (layer_only), or times (velocity / layer velocity) ^ 1.5 (pow15); then the instrument's gain_mdb.
// - Round robin and pan: the note's rr and panIndex, as the engine counts them (the sampler keeps no
//   counters): the layer's file rr % count, the pan cycle's entry panIndex % length.
// - one_shot plays to the end of its file and ignores note-off; gated releases on a raised cosine of
//   release_us; sustain_loop wraps its file's [loop_start, loop_end) for the voice's whole life and
//   releases the same way. attack_us is a raised-cosine fade-in from start_frames.
// - align end: the file's end lands on the note's end (NoteOn::lengthSamples): a longer file starts
//   part-way in, a shorter one starts late.
// - Chokes: a note-on of a `chokes` (or `both`) instrument fades every sounding voice of its group's
//   `choked` (or `both`) instruments over that instrument's fade_us, linear or cos^2, from the note-on.
//   (FUT02-04's hat lanes do the same; FUT01's choked_lane ends its 5 ms fade on the closed hat, which
//   would need the next note in advance, so FUT01's open hats ring 5 ms longer here.)
// - mono: one held voice per instrument and source. A note-on while it is held is legato: the voice
//   keeps its zone, layer and file and moves to the new key's pitch, and its velocity gain to the new
//   note's, along the glide (a raised cosine over us, or dip_us for NoteOn::dip; an exponential with time
//   constant us; a straight line over us; none: at once), in semitones. A released voice rings out under
//   the next one.
// - restrike_continues: a note-on on the very sample its own voice was released (the note before ended
//   there), with startFrame 0, takes the voice back instead of starting another: no new attack.
// - layered_with: every note-on also starts the layered instrument on the same key, released with it.
// - Chase (NoteOn::startFrame): the voice starts where its note is now, fading in over at least 5 ms.
// - 64 voices. A note-on with none free steals the oldest released voice (by start; released: its key is
//   up or it is fading out), else the quietest, which fades out over 2 ms in a slot of its own while the
//   new note starts at once.
// - Reads never leave the file: frames before 0 or past the end read as silence (Hermite: the
//   recipe's edge, the first frame, before 0). Mono files feed both sides at unity.
// - Phrase notes (slice >= 0) go to the phrase voices (P6): a voice playing the note's kit, phrase and
//   source, and not stopping, reads on (continueWith); else a free voice starts it. fadeOut () and
//   fadeKit () stop phrase voices too, and a stopping voice takes no more notes, so after a jump the chased
//   note starts afresh while the old one fades. With all 12 busy, a stopping one (the least fade left),
//   else the oldest, is stolen: it fades over 2 ms in a slot of its own while the new note starts; the
//   steal counts in droppedEvents (). Note-offs never reach them: a stem has its own lengths.
// - Interpolation: a copy where the voice's ratio is exactly 1; the 16-tap polyphase sinc
//   (sinc_bank.h), or 4-point Hermite (hermite.h) where the instrument asks for it. The ratio is
//   48000 / rate x 2 ^ (semitones / 12).
//
// Threads: prepare () on the message thread with the audio thread stopped (it allocates); everything
// else on the audio thread, noexcept and real-time safe: no allocation, free, lock, I/O or wait.
// The Kit a voice plays must outlive it: fadeKit () fades a Kit's voices, holdsKit () says when none is
// left (queued events count).

#include "kit/kit.h"

#include <array>
#include <cstdint>
#include <memory>

namespace wubansia::engine
{
    class MixBus;
    class SendBus;

    // Who started a note: the timeline or MIDI in. noteOff (), releaseAll () and fadeOut () act on one.
    enum class Source : std::uint8_t {
        sequencer,
        live
    };

    // One note-on, resolved by the engine down to an instrument.
    struct NoteOn
    {
        std::int16_t instrument = -1; // Kit::instruments index: the KeyTarget's a or b
        std::uint8_t key = 0; // the MIDI key: picks the zone and the pitch
        std::uint8_t velocity = 0; // 1-127: picks the layer and, for pow15, the gain
        std::uint8_t rr = 0; // the round robin: the layer's file rr % its count
        std::uint8_t panIndex = 0; // the pan cycle's entry panIndex % its length
        std::uint8_t character = 0; // phrase notes: the stem, 0 = A, 1 = B
        bool dip = false; // NoteFlag::dip: a legato note gliding over glide.dipUs (never from MIDI in)
        bool continues = false; // NoteFlag::continues (phrase notes)
        Source source = Source::sequencer;
        std::int32_t offset = 0; // the sample within the next render () call
        // align end only: host samples from offset to the note's end at the current tempo (what remains
        // of it when chased); 0 otherwise.
        std::int64_t lengthSamples = 0;
        // Chase: the time since the note's start, in 48 kHz frames before pitch (elapsed host samples x
        // 48000 / rate, rounded); the voice scales it by its own pitch. 0: from the start.
        std::int64_t startFrame = 0;
        // The voice's direct sends, linear: (1 - base) x cc / 127 x the channel's static gain for a
        // thrown note (CC91 Space, CC94 Echo latched at its note-on), else 0. Scaled by setLaneGains ().
        float throwSpace = 0.0f;
        float throwEcho = 0.0f;
        // A phrase note: Kit::phrases index and its slice (Kit::slices), for PhraseVoice; -1 for a
        // multisample note. Notes with slice >= 0 go to the phrase voices.
        std::int16_t phrase = -1;
        std::int16_t slice = -1;
    };

    class Sampler
    {
    public:
        static constexpr int kVoices = 64; // multisample voices sounding at once
        static constexpr int kFadeSlots = 16; // where stolen voices fade out
        static constexpr int kPhraseVoices = 12; // phrase voices sounding at once (P6's are about 2 KB each)
        static constexpr int kPhraseFadeSlots = 4; // where stolen phrase voices fade out
        static constexpr int kMaxEvents = 1024; // queued events per render () call; more are dropped
        static constexpr std::int32_t kStealFadeUs = 2000;
        static constexpr std::int32_t kKitFadeUs = 5000; // fadeKit ()
        static constexpr std::int32_t kChaseFadeUs = 5000; // a chased voice fades in over at least this
        static constexpr std::int32_t kLaneRampUs = 10000; // setLaneGains () moves a gain by 1 over this

        Sampler();
        ~Sampler();
        Sampler (const Sampler&) = delete;
        Sampler& operator= (const Sampler&) = delete;

        // Message thread, the audio thread stopped. Sizes everything for this rate (11025-192000 Hz) and
        // pieces of up to maxBlock samples; every voice stops, lane gains go to 1.
        void prepare (double sampleRate, int maxBlock);

        // Audio thread. Every voice stops at once and the queue empties; the 32-sample count restarts.
        void reset() noexcept;

        // Queue a note-on (NoteOn::offset in the next render ()). The Kit must outlive the voice.
        void noteOn (const Kit& kit, const NoteOn& note) noexcept;

        // Queue a note-off for the instrument the note-on used: it releases that instrument's held voices
        // on key from source (and their layered_with voices). A mono voice holding another key ignores it.
        void noteOff (int instrument, int key, int offset, Source source) noexcept;

        // Queue a note-off for every held note of source (CC123).
        void releaseAll (Source source, int offset) noexcept;

        // Queue a fade of every voice of source, held or not, over fadeSamples from offset (stop, jump,
        // CC120).
        void fadeOut (Source source, int offset, int fadeSamples) noexcept;

        // Queue a fade of every voice playing kit over kKitFadeUs from offset (a kit change).
        void fadeKit (const Kit* kit, int offset) noexcept;

        // Whether a voice, or a queued event, still refers to kit.
        bool holdsKit (const Kit* kit) const noexcept;

        // The lane levels (linear, 0 muted) that scale the voices' direct sends, as the mixer scales the
        // channels' sends. Each moves towards its new value at kLaneRampUs per unit. Default 1.
        void setLaneGains (const std::array<float, kLanes>& gains) noexcept;

        // Applies the queued events at their offsets and adds numSamples (at most prepare's maxBlock) of
        // every voice into bus and sends. Neither is cleared here.
        void render (MixBus& bus, SendBus& sends, int numSamples) noexcept;

        // Sounding voices after the last render (): multisample voices, fading slots and phrase voices.
        int activeVoices() const noexcept;

        // Events dropped because the queue was full, and phrase voices stolen with every phrase slot busy,
        // since prepare ().
        int droppedEvents() const noexcept;

        // A bit per Lane (laneBit) for each phrase lane playing its recording now: a busy phrase voice that
        // isn't stopping (Telemetry::phraseRecorded). After the last render ().
        std::uint32_t phraseLanes() const noexcept;

    private:
        struct Impl;
        std::unique_ptr<Impl> impl;
    };
}
