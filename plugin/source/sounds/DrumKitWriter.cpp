#include "sounds/DrumKitWriter.h"

namespace wubansia::sounds
{
    DrumKitWrite writeDrumKit (ContentSourceFactory&, const ContentStatus&, const engine::KitManifest&, const DrumKitPlan&,
        const juce::File&, const std::atomic<bool>&, const std::function<void (int)>&)
    {
        return {};
    }

    bool holdsDrumKit (const juce::File&, const engine::KitManifest&, const DrumKitPlan&) { return false; }

    bool renameNoReplace (const juce::File&, const juce::File&) { return false; }
}
