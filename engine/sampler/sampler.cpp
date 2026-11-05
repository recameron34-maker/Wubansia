// The sampler's first commit: the interface (sampler.h) with a silent body, so the engine can build
// against it. The voices follow.

#include "sampler/sampler.h"

#include "mix/mixer.h"

namespace wubansia::engine
{
    struct Sampler::Impl
    {
        double rate = 48000.0;
        int maxBlock = 0;
    };

    Sampler::Sampler() : impl (std::make_unique<Impl>()) {}

    Sampler::~Sampler() = default;

    void Sampler::prepare (double sampleRate, int maxBlock)
    {
        impl->rate = sampleRate;
        impl->maxBlock = maxBlock;
    }

    void Sampler::reset() noexcept {}

    void Sampler::noteOn (const Kit& kit, const NoteOn& note) noexcept
    {
        (void) kit;
        (void) note;
    }

    void Sampler::noteOff (int instrument, int key, int offset, Source source) noexcept
    {
        (void) instrument;
        (void) key;
        (void) offset;
        (void) source;
    }

    void Sampler::releaseAll (Source source, int offset) noexcept
    {
        (void) source;
        (void) offset;
    }

    void Sampler::fadeOut (Source source, int offset, int fadeSamples) noexcept
    {
        (void) source;
        (void) offset;
        (void) fadeSamples;
    }

    void Sampler::fadeKit (const Kit* kit, int offset) noexcept
    {
        (void) kit;
        (void) offset;
    }

    bool Sampler::holdsKit (const Kit* kit) const noexcept
    {
        (void) kit;
        return false;
    }

    void Sampler::setLaneGains (const std::array<float, kLanes>& gains) noexcept
    {
        (void) gains;
    }

    void Sampler::render (MixBus& bus, SendBus& sends, int numSamples) noexcept
    {
        (void) bus;
        (void) sends;
        (void) numSamples;
    }

    int Sampler::activeVoices() const noexcept
    {
        return 0;
    }

    int Sampler::droppedEvents() const noexcept
    {
        return 0;
    }
}
