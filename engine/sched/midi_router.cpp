// The MIDI router's non-template parts: see midi_router.h.

#include "sched/midi_router.h"

namespace wubansia::engine
{
    void MidiRouter::reset() noexcept
    {
        held_ = emptyHeld();
        for (auto& channel : throws_)
            channel = { 0, 0 };
        counts_.fill (0);
    }

    void MidiRouter::forgetNotes() noexcept
    {
        held_ = emptyHeld();
    }

    int MidiRouter::held() const noexcept
    {
        int n = 0;
        for (const auto& channel : held_)
            for (const auto instrument : channel)
                n += instrument >= 0 ? 1 : 0;
        return n;
    }
}
