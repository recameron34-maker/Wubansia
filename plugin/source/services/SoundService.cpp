#include "services/SoundService.h"

namespace wubansia
{
    SoundService::SoundService (ContentSourceFactory& c, engine::AuditionPlayer& p) : content (c), player (p) {}

    SoundService::~SoundService() = default;

    std::uint32_t SoundService::audition (Request) { return 0; } // not yet

    void SoundService::stop() {}

    void SoundService::refuse (const juce::String&) {}

    AuditionState SoundService::status() const { return {}; }

    void SoundService::run() {}

    void SoundService::work (const Job&) {}

    bool SoundService::superseded (std::uint32_t) const { return true; }

    void SoundService::fail (const Job&, const std::string&) {}

    void SoundService::offer (const Job&, std::unique_ptr<engine::AuditionClip>) {}

    void SoundService::checkOffer() {}

    void SoundService::drain() {}

    void SoundService::back (const engine::AuditionClip* clip) { delete clip; }

    void SoundService::discard (std::unique_ptr<engine::AuditionClip>) {}

    void SoundService::stopAll (const juce::String*) {}
}
