#include "editor/skin/Skin.h"

namespace wubansia::editor
{
    juce::Font TextStyle::font() const { return fontFor (face, px, letterSpacingPx, fallback); }
    juce::Font TextStyle::fontAt (float size) const { return fontFor (face, size, letterSpacingPx, fallback); }

    juce::Font bodyFont (const Skin& skin, float juceHeight, bool strong, float kerningFactor)
    {
        const auto& style = strong ? skin.type.bodyStrong : skin.type.body;
        return fontFor (style.face, juceHeight / 1.117f, kerningFactor * juceHeight, style.fallback);
    }

    juce::Font pathFont (const Skin& skin, float juceHeight) { return fontFor (skin.type.path.face, juceHeight / 1.117f, 0.0f, true); }

    const Skin& skinFor (SkinId) noexcept
    {
        static const Skin stub;
        return stub;
    }

    const Skin& skinForKit (const juce::String&) noexcept { return skinFor (SkinId::nightBus); }

    const Skin& skinOf (const juce::Component&) noexcept { return skinFor (SkinId::nightBus); }

    std::vector<std::pair<juce::String, const TextStyle*>> stylesOf (const Skin&) { return {}; }

    float contrastRatio (juce::Colour, juce::Colour) noexcept { return 0.0f; }

    juce::Colour over (juce::Colour top, juce::Colour) noexcept { return top; }
}
