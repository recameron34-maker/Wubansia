#include "sampler/voice_pool.h"

#include <algorithm>

namespace wubansia::engine
{
    void VoicePool::prepare (double sampleRate)
    {
        reset();
        for (auto& p : phrases)
            p.voice.prepare (sampleRate);
        for (auto& p : phraseFading)
            p.voice.prepare (sampleRate);
        phraseSteals = 0;
    }

    void VoicePool::reset() noexcept
    {
        for (auto& v : voices)
            v.stop();
        for (auto& v : fading)
            v.stop();
        forEachPhraseSlot ([] (PhraseSlot& p) {
            p.voice.reset();
            p.kit = nullptr;
            p.phrase = -1;
            p.busy = false;
            p.stopping = false;
            p.fadeLeft = 0;
        });
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
            n += p.busy ? 1 : 0;
        for (const auto& p : phraseFading)
            n += p.busy ? 1 : 0;
        return n;
    }

    void VoicePool::phraseNoteOn (const Kit& kit, const NoteOn& note, int offset, int stealFadeSamples) noexcept
    {
        if (note.phrase < 0 || static_cast<std::size_t> (note.phrase) >= kit.phrases.size())
            return;
        for (auto& p : phrases)
            if (p.busy && ! p.stopping && p.kit == &kit && p.phrase == note.phrase && p.source == note.source)
            {
                p.voice.continueWith (note, offset);
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
            (void) stealFadeSamples; // every slot busy: the first restarts, cut
            slot = &phrases[0];
            slot->voice.reset();
        }

        slot->kit = &kit;
        slot->phrase = note.phrase;
        slot->source = note.source;
        slot->busy = true;
        slot->stopping = false;
        slot->fadeLeft = 0;
        slot->order = ++phraseOrder;
        slot->voice.start (kit, note, offset);
    }

    namespace
    {
        void stopSlot (PhraseSlot& p, int offset, int fadeSamples) noexcept
        {
            const std::int64_t end = static_cast<std::int64_t> (offset) + std::max (0, fadeSamples);
            p.fadeLeft = p.stopping ? std::min (p.fadeLeft, end) : end;
            p.stopping = true;
            p.voice.stop (offset, fadeSamples);
        }
    }

    void VoicePool::stopPhrases (Source source, int offset, int fadeSamples) noexcept
    {
        forEachPhraseSlot ([&] (PhraseSlot& p) {
            if (p.busy && p.source == source)
                stopSlot (p, offset, fadeSamples);
        });
    }

    void VoicePool::stopPhrasesOf (const Kit* kit, int offset, int fadeSamples) noexcept
    {
        forEachPhraseSlot ([&] (PhraseSlot& p) {
            if (p.busy && p.kit == kit)
                stopSlot (p, offset, fadeSamples);
        });
    }

    void VoicePool::renderPhrases (MixBus& bus, SendBus& sends, int numSamples, const float* laneSends, std::size_t stride) noexcept
    {
        forEachPhraseSlot ([&] (PhraseSlot& p) {
            if (! p.busy)
                return;
            const float* laneSend = nullptr;
            if (laneSends != nullptr && p.kit != nullptr && p.phrase >= 0 && static_cast<std::size_t> (p.phrase) < p.kit->phrases.size())
            {
                const auto lane = static_cast<std::size_t> (p.kit->phrases[static_cast<std::size_t> (p.phrase)].lane);
                if (lane < static_cast<std::size_t> (kLanes))
                    laneSend = laneSends + lane * stride;
            }
            p.voice.render (bus, sends, numSamples, laneSend);
            p.busy = p.voice.active();
            p.stopping = p.stopping && p.busy;
            p.fadeLeft = std::max<std::int64_t> (0, p.fadeLeft - numSamples);
        });
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
        for (const auto& p : phraseFading)
            if (p.busy && p.kit == kit)
                return true;
        return false;
    }
}
