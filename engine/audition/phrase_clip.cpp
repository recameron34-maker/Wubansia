#include "audition/phrase_clip.h"

#include "kit/kit.h"
#include "midi/reader.h"
#include "sched/timeline_builder.h"

#include <algorithm>

namespace wubansia::engine
{
    std::unique_ptr<PhraseClip> makePhraseClip (std::span<const std::uint8_t> smf, const KitManifest& kit, const MixConfig& mix, std::int32_t kitIndex,
        std::int64_t alignTicks, std::uint32_t ticket)
    {
        if (alignTicks <= 0)
            return nullptr;
        const auto file = midi::readSmf (smf);
        if (!file.ok())
            return nullptr;
        // A loop of the file's whole bars (960 PPQ, 4/4).
        constexpr std::int64_t bar = 4 * kTicksPerQuarter;
        std::int64_t end = 0;
        for (const auto& track : file.value.tracks)
            end = std::max (end, track.endTick);
        TimelineOptions options;
        options.loopTicks = std::max<std::int64_t> (bar, (end + bar - 1) / bar * bar);
        options.phrases = false;
        auto timeline = buildTimeline (smf, kit, mix, options);
        if (!timeline.ok() || timeline.value.notes.empty())
            return nullptr;
        auto clip = std::make_unique<PhraseClip>();
        // The lanes its notes play on, either side of an A/B pair: the groove's notes there sit out while it sounds.
        const auto laneOf = [&kit] (std::int32_t instrument) -> std::uint32_t {
            return instrument >= 0 && static_cast<std::size_t> (instrument) < kit.instruments.size()
                       ? laneBit (kit.instruments[static_cast<std::size_t> (instrument)].lane)
                       : 0u;
        };
        for (const auto& note : timeline.value.notes)
            for (const auto& k : kit.keymap)
                if (k.ch == note.channel + 1 && note.key >= k.lo && note.key <= k.hi)
                    clip->lanes |= laneOf (k.a) | laneOf (k.b);
        clip->timeline = std::move (timeline.value);
        clip->kitIndex = kitIndex;
        clip->alignH = alignTicks * kMilliticksPerTick;
        clip->ticket = ticket;
        return clip;
    }

    void PhraseHandoff::stopUpTo (std::uint32_t ticket) noexcept
    {
        auto current = stopTicket_.load (std::memory_order_relaxed);
        while (ticket > current && !stopTicket_.compare_exchange_weak (current, ticket, std::memory_order_acq_rel)) {}
    }
}
