#pragma once

// The looks' thirteen embedded faces (ADR 0032): plugin/resources/fonts, SIL OFL 1.1, subset by tools/fonts with a
// "Wubansia" prefix on every family, so the process-wide registration can't clash with a font the person has
// installed. IBM Plex Mono is renamed "Wubansia HUD Mono" for its Reserved Font Name.

#include <juce_graphics/juce_graphics.h>

#include <cstdint>

namespace wubansia::editor
{
    enum class FaceId : std::uint8_t {
        archivoRegular, archivoExtraBold, jetBrainsMonoRegular,                 // night bus
        chakraPetchRegular, chakraPetchBold, hudMonoRegular,                     // mission HUD (HUD Mono: IBM Plex Mono, renamed)
        manropeRegular, manropeSemiBold, manropeExtraBold,                       // deep glass
        barlowCondensedSemiBold, barlowCondensedBold, barlowCondensedExtraBold,  // machine room
        spaceMonoRegular,
        platform // the platform's own UI face: lit hardware's, until commit 7
    };
    inline constexpr int kFaces = 13; // the embedded ones: FaceId 0-12

    // Made once per process from the binary data, on the message thread; null for FaceId::platform.
    juce::Typeface::Ptr typefaceFor (FaceId);

    // "Wubansia Archivo" ... as the subset's name table says.
    juce::String familyOf (FaceId);

    // The face at an em size of px logical pixels (CSS's px), letter spacing in px after every character, and
    // fallback to the platform's fonts only where asked (strings the person or the run time supplies).
    juce::Font fontFor (FaceId, float px, float letterSpacingPx = 0.0f, bool fallback = false);
}
