#include "sampler/voice_pool.h"

namespace wubansia::engine
{
    void VoicePool::prepare (double sampleRate) noexcept
    {
        reset();
        for (auto& p : phrases)
            p.voice.prepare (sampleRate);
    }

    void VoicePool::reset() noexcept
    {
        for (auto& v : voices)
            v.stop();
        for (auto& v : fading)
            v.stop();
        for (auto& p : phrases)
        {
            p.voice.stop (0, 0);
            p.busy = false;
            p.stopping = false;
            p.kit = nullptr;
            p.phrase = -1;
        }
    }

    Voice& VoicePool::take (double stealFadeSamples) noexcept
    {
        for (auto& v : voices)
            if (! v.isActive())
                return v;

        // The oldest released voice (key up, or fading out), else the quietest.
        Voice* victim = nullptr;
        for (auto& v : voices)
            if (v.isReleased() && (victim == nullptr || v.order < victim->order))
                victim = &v;
        if (victim == nullptr)
            for (auto& v : voices)
                if (victim == nullptr || v.level() < victim->level()
                    || (! (v.level() > victim->level()) && v.order < victim->order)) // a tie: the older
                    victim = &v;

        // Its own fading slot: a free one, else the quietest fading voice's.
        Voice* slot = nullptr;
        for (auto& f : fading)
            if (! f.isActive())
            {
                slot = &f;
                break;
            }
        if (slot == nullptr)
            for (auto& f : fading)
                if (slot == nullptr || f.level() < slot->level())
                    slot = &f;

        *slot = *victim;
        slot->fade (stealFadeSamples, FadeCurve::cos2);
        victim->stop();
        return *victim;
    }

    int VoicePool::countActive() const noexcept
    {
        int n = 0;
        forEachActive ([&n] (const Voice&) { ++n; });
        for (const auto& p : phrases)
            if (p.busy)
                ++n;
        return n;
    }

    void VoicePool::phraseNoteOn (const Kit& kit, const NoteOn& note, int offset) noexcept
    {
        if (note.phrase < 0 || static_cast<std::size_t> (note.phrase) >= kit.phrases.size())
            return;
        for (auto& p : phrases)
            if (p.busy && ! p.stopping && p.kit == &kit && p.phrase == note.phrase && p.source == note.source)
            {
                p.voice.continueWith (note.slice, offset);
                return;
            }
        PhraseSlot* slot = nullptr;
        for (auto& p : phrases)
            if (! p.busy)
            {
                slot = &p;
                break;
            }
        if (slot == nullptr)
        {
            slot = &phrases[0];
            slot->voice.stop (offset, 0);
        }
        slot->kit = &kit;
        slot->phrase = note.phrase;
        slot->source = note.source;
        slot->busy = true;
        slot->stopping = false;
        slot->voice.start (kit, note.phrase, note.slice, note.character, offset, note.startFrame);
    }

    void VoicePool::stopPhrases (Source source, int offset, int fadeSamples) noexcept
    {
        for (auto& p : phrases)
            if (p.busy && p.source == source)
            {
                p.voice.stop (offset, fadeSamples);
                p.stopping = true;
            }
    }

    void VoicePool::stopPhrasesOf (const Kit* kit, int offset, int fadeSamples) noexcept
    {
        for (auto& p : phrases)
            if (p.busy && p.kit == kit)
            {
                p.voice.stop (offset, fadeSamples);
                p.stopping = true;
            }
    }

    void VoicePool::renderPhrases (MixBus& bus, SendBus& sends, int numSamples, const float* laneSends, std::size_t stride) noexcept
    {
        (void) laneSends; // for PhraseVoice::render's lane send gains (P6)
        (void) stride;
        for (auto& p : phrases)
            if (p.busy)
            {
                p.voice.render (bus, sends, numSamples);
                p.busy = p.voice.active();
                p.stopping = p.stopping && p.busy;
            }
    }

    std::uint32_t VoicePool::phraseLanes() const noexcept
    {
        std::uint32_t lanes = 0;
        for (const auto& p : phrases)
            if (p.busy && ! p.stopping && p.kit != nullptr && p.phrase >= 0
                && static_cast<std::size_t> (p.phrase) < p.kit->phrases.size())
                lanes |= laneBit (p.kit->phrases[static_cast<std::size_t> (p.phrase)].lane);
        return lanes;
    }

    bool VoicePool::phrasesHold (const Kit* kit) const noexcept
    {
        for (const auto& p : phrases)
            if (p.busy && p.kit == kit)
                return true;
        return false;
    }
}
