#include "services/DrumKitService.h"

namespace wubansia
{
    DrumKitService::DrumKitService (ContentSourceFactory& c) : content (c) {}

    DrumKitService::~DrumKitService() = default;

    std::uint32_t DrumKitService::request (Request) { return 0; }

    void DrumKitService::refuse (const juce::String&, const juce::String&) {}

    DrumKitStatus DrumKitService::status() const { return {}; }

    void DrumKitService::run() {}
}
