// The engine (engine.h): the block loop (engine_loop.h) on P5's Sampler, with the mixer (P0's
// pass-through until P8's lands). The Kit hand-off (PendingSlot, RetireFifo) and the telemetry are
// engine.h's members; the loop gets them per block.

#include "engine.h"

#include "engine_loop.h"
#include "sampler/sampler.h"

namespace wubansia::engine
{
    struct Engine::Impl
    {
        BlockLoop<Sampler> loop;
    };

    Engine::Engine() : impl (std::make_unique<Impl>()) {}

    Engine::~Engine() = default;

    void Engine::prepare (double sampleRate, int maxBlock, int numOutputs)
    {
        impl->loop.prepare (sampleRate, maxBlock, numOutputs);
    }

    void Engine::reset() noexcept
    {
        impl->loop.reset();
    }

    void Engine::process (float* const* out, int numOutputs, int numSamples, const TransportInfo& transport, const Controls& controls,
        const MidiEvent* midi, int numMidi) noexcept
    {
        impl->loop.process (out, numOutputs, numSamples, transport, controls, midi, numMidi, pending, retired, telemetry_);
    }

    int Engine::latencySamples() const noexcept
    {
        return impl->loop.latencySamples();
    }

    double Engine::tailSeconds() const noexcept
    {
        return impl->loop.tailSeconds();
    }
}
