// The mixer (P8, ADR 0021): the adopted Kit's mix, effects and club master in real time, through mix/graph.h. It
// replaces P0's pass-through mixer.

#include "mix/mixer.h"

#include "fx/denormals.h"
#include "mix/graph.h"

#include <algorithm>

namespace wubansia::engine
{
    struct Mixer::Impl
    {
        mix::MixGraph graph;
        Controls lastControls;
        int maxBlock = 0;
        int numOutputs = 2;
        double sampleRate = 48000.0;
        bool prepared = false;
    };

    Mixer::Mixer() : impl (std::make_unique<Impl>()) {}

    Mixer::~Mixer() = default;

    void Mixer::prepare (double sampleRate, int maxBlock, int numOutputs)
    {
        impl->sampleRate = sampleRate > 0.0 ? sampleRate : 48000.0;
        impl->maxBlock = std::max (1, maxBlock);
        impl->numOutputs = std::clamp (numOutputs, 1, 2);
        bus_.prepare (kMaxMixChannels, impl->maxBlock);
        sends_.prepare (impl->maxBlock);
        impl->graph.prepare (impl->sampleRate, impl->maxBlock);
        impl->prepared = true;
    }

    void Mixer::reset() noexcept
    {
        fx::ScopedFlushDenormals ftz;
        impl->graph.reset (impl->lastControls);
    }

    void Mixer::setConfig (const MixConfig* config) noexcept
    {
        fx::ScopedFlushDenormals ftz;
        impl->graph.setConfig (config, impl->lastControls);
    }

    void Mixer::keyHit (std::uint8_t keyLane, int velocity, int offset) noexcept
    {
        impl->graph.keyHit (keyLane, velocity, offset);
    }

    void Mixer::cc (int mixChannel, int controller, int value, int offset, int rampSamples) noexcept
    {
        impl->graph.cc (mixChannel, controller, value, offset, rampSamples);
    }

    void Mixer::process (float* const* out, int numOutputs, int numSamples, const Controls& controls, double bpm) noexcept
    {
        const int n = std::clamp (numSamples, 0, impl->maxBlock);
        if (numOutputs < 1 || n == 0 || ! impl->prepared)
            return;
        fx::ScopedFlushDenormals ftz;
        impl->lastControls = controls;
        impl->graph.process (bus_, sends_, out[0], numOutputs > 1 ? out[1] : nullptr, n, controls, bpm);
    }

    int Mixer::latencySamples() const noexcept
    {
        return impl->prepared ? impl->graph.latency() : 0;
    }

    double Mixer::tailSeconds() const noexcept
    {
        return 6.0;
    }

    const MixMeters& Mixer::meters() const noexcept
    {
        return impl->graph.meters();
    }
}
