// P0's silent engine: it adopts and retires kits as engine.h describes and writes silence through the
// pass-through mixer. P7 replaces this file with the scheduler, the router and the voices.

#include "engine.h"

#include "mix/mixer.h"

#include <algorithm>

namespace wubansia::engine
{
    struct Engine::Impl
    {
        Mixer mixer;
        const Kit* kit = nullptr; // the adopted Kit
        int maxBlock = 0;
        int numOutputs = 2;
    };

    Engine::Engine() : impl (std::make_unique<Impl>()) {}

    Engine::~Engine() = default;

    void Engine::prepare (double sampleRate, int maxBlock, int numOutputs)
    {
        impl->maxBlock = std::max (1, maxBlock);
        impl->numOutputs = std::clamp (numOutputs, 1, 2);
        impl->mixer.prepare (sampleRate, impl->maxBlock, impl->numOutputs);
        impl->mixer.setConfig (impl->kit != nullptr ? &impl->kit->mix : nullptr);
    }

    void Engine::reset() noexcept
    {
        impl->mixer.reset();
    }

    void Engine::process (float* const* out, int numOutputs, int numSamples, const TransportInfo& transport,
        const Controls& controls, const MidiEvent* midi, int numMidi) noexcept
    {
        (void) midi;
        (void) numMidi;

        for (int c = 0; c < numOutputs; ++c)
            std::fill_n (out[c], std::max (0, numSamples), 0.0f);

        // A pending Kit is adopted at block start, while the retire FIFO has room for the one it
        // replaces. With no voices to fade, the old Kit is let go at once.
        if (retired.hasSpace())
        {
            if (const Kit* next = pending.take())
            {
                retired.push (impl->kit);
                impl->kit = next;
                impl->mixer.setConfig (&next->mix);
                telemetry_.kitIndex.store (next->info.index, std::memory_order_relaxed);
            }
        }

        const int n = std::clamp (numSamples, 0, impl->maxBlock);
        impl->mixer.bus().clear (n);
        impl->mixer.sends().clear (n);
        impl->mixer.process (out, std::min (numOutputs, impl->numOutputs), n, controls, transport.bpm);
    }

    int Engine::latencySamples() const noexcept
    {
        return impl->mixer.latencySamples();
    }

    double Engine::tailSeconds() const noexcept
    {
        return impl->mixer.tailSeconds();
    }
}
