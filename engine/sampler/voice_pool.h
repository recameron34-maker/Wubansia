#pragma once

// The sampler's voices: 64 multisample voices, 16 slots where stolen voices fade out, and the phrase
// voices (P6's PhraseVoice). Fixed arrays: nothing here allocates.
//
// Stealing (a note-on with no voice free): the oldest released voice (by start), else the quietest
// (by its last gain). It moves to a fading slot and fades over the steal fade while the new note takes its
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
    };

    class VoicePool
    {
    public:
        // Every voice silent at once; the phrase voices learn the rate.
        void prepare (double sampleRate) noexcept;
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

        // The phrase voices. Every call into PhraseVoice is in these four (and prepare and reset), so a
        // change to its interface is an edit here.
        // A phrase note: a busy slot playing the same kit, phrase and source reads on (continueWith), else a
        // free slot starts it (the first slot, stopped, when none is free).
        void phraseNoteOn (const Kit& kit, const NoteOn& note, int offset) noexcept;
        // Fades the phrase voices of a source, or of a kit, over fadeSamples from offset.
        void stopPhrases (Source source, int offset, int fadeSamples) noexcept;
        void stopPhrasesOf (const Kit* kit, int offset, int fadeSamples) noexcept;
        // Renders the busy phrase voices over the whole piece; laneSends: per lane, the piece's send gains
        // (stride floats apart).
        void renderPhrases (MixBus& bus, SendBus& sends, int numSamples, const float* laneSends, std::size_t stride) noexcept;
        bool phrasesHold (const Kit* kit) const noexcept;

        std::array<Voice, Sampler::kVoices> voices {};
        std::array<Voice, Sampler::kFadeSlots> fading {};
        std::array<PhraseSlot, Sampler::kPhraseVoices> phrases {};
    };
}
