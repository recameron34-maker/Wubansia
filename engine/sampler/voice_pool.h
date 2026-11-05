#pragma once

// The sampler's voices: 64 multisample voices, 16 slots where stolen voices fade out, and the phrase
// voices (P6's PhraseVoice). Fixed arrays: nothing here allocates.
//
// Stealing (a note-on with no voice free): the oldest released voice (by start; key up, or fading
// out), else the quietest (by its gain now, a rising attack counting as its end; the older on a tie, so
// a chord's notes never steal each other). It moves to a fading slot and fades over the steal fade while the new note takes its
// place at once, so neither has to wait and nothing clicks. With every fading slot busy, the quietest
// fading voice gives up its slot.

#include "sampler/phrase_voice.h"
#include "sampler/sampler.h"
#include "sampler/voice.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace wubansia::engine
{
    // A phrase voice and what the sampler knows of it.
    struct PhraseSlot
    {
        PhraseVoice voice;
        const Kit* kit = nullptr;
        std::int16_t phrase = -1;
        Source source = Source::sequencer;
        bool busy = false; // started, and not yet seen silent after a render
        bool stopping = false; // fading out after a stop: no note routes to it any more
        std::uint64_t order = 0; // start order: with every slot busy, the oldest is stolen
        std::int64_t fadeLeft = 0; // stopping: samples until its fade ends, from the next piece's start
    };

    class VoicePool
    {
    public:
        // Every voice silent at once; the phrase voices learn the rate. Not noexcept: PhraseVoice::prepare
        // may build the shared sinc bank. The sampler builds it first, so here nothing allocates.
        void prepare (double sampleRate);
        // Every voice silent at once (PhraseVoice::reset), every slot free.
        void reset() noexcept;

        // A voice for a new note: a free one, else a stolen one (the old note fading in a slot of its own
        // over stealFadeSamples).
        Voice& take (double stealFadeSamples) noexcept;

        // f (Voice&) for every active voice, the fading slots included.
        template <class F>
        void forEachActive (F&& f) noexcept
        {
            for (auto& v : voices)
                if (v.isActive())
                    f (v);
            for (auto& v : fading)
                if (v.isActive())
                    f (v);
        }

        template <class F>
        void forEachActive (F&& f) const noexcept
        {
            for (const auto& v : voices)
                if (v.isActive())
                    f (v);
            for (const auto& v : fading)
                if (v.isActive())
                    f (v);
        }

        int countActive() const noexcept;

        // The phrase voices. Every call into PhraseVoice is in these (and prepare and reset), so a change to
        // its interface is an edit here.
        // A phrase note: a busy slot playing the same kit, phrase and source, and not stopping, reads on
        // (continueWith); else a free slot starts it (start), and with every slot busy the first restarts.
        // So after a jump the chased note starts in a fresh slot while the old one fades, and a stop then a
        // start within the fade never cut it.
        void phraseNoteOn (const Kit& kit, const NoteOn& note, int offset, int stealFadeSamples) noexcept;
        // Fades the phrase voices of a source, or of a kit, over fadeSamples from offset (stop). They are
        // stopping from then on.
        void stopPhrases (Source source, int offset, int fadeSamples) noexcept;
        void stopPhrasesOf (const Kit* kit, int offset, int fadeSamples) noexcept;
        // Renders the busy phrase voices over the whole piece. laneSends: per lane, the piece's send gains
        // (stride floats apart); each voice gets its phrase's lane's.
        void renderPhrases (MixBus& bus, SendBus& sends, int numSamples, const float* laneSends, std::size_t stride) noexcept;
        bool phrasesHold (const Kit* kit) const noexcept;
        // A bit per Lane for each busy phrase voice that isn't stopping: the lanes playing their recordings.
        std::uint32_t phraseLanes() const noexcept;

        std::array<Voice, Sampler::kVoices> voices {};
        std::array<Voice, Sampler::kFadeSlots> fading {};
        std::array<PhraseSlot, Sampler::kPhraseVoices> phrases {};
        std::array<PhraseSlot, Sampler::kPhraseFadeSlots> phraseFading {}; // (unused until the steal policy)
        int phraseSteals = 0; // since prepare ()

    private:
        template <class F>
        void forEachPhraseSlot (F&& f) noexcept
        {
            for (auto& p : phrases)
                f (p);
            for (auto& p : phraseFading)
                f (p);
        }

        std::uint64_t phraseOrder = 0;
    };
}
