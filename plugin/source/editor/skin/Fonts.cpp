#include "editor/skin/Fonts.h"

#include "WubansiaFontData.h"

#include <array>

namespace wubansia::editor
{
    namespace
    {
        struct Face
        {
            const char* resource; // juce_add_binary_data's name for plugin/resources/fonts/<file>
            const char* family;
        };

        // In FaceId's order.
        constexpr std::array<Face, kFaces> kFaceTable { {
            { "ArchivoRegular_ttf", "Wubansia Archivo" },
            { "ArchivoExtraBold_ttf", "Wubansia Archivo" },
            { "JetBrainsMonoRegular_ttf", "Wubansia JetBrains Mono" },
            { "ChakraPetchRegular_ttf", "Wubansia Chakra Petch" },
            { "ChakraPetchBold_ttf", "Wubansia Chakra Petch" },
            { "WubansiaHudMonoRegular_ttf", "Wubansia HUD Mono" },
            { "ManropeRegular_ttf", "Wubansia Manrope" },
            { "ManropeSemiBold_ttf", "Wubansia Manrope" },
            { "ManropeExtraBold_ttf", "Wubansia Manrope" },
            { "BarlowCondensedSemiBold_ttf", "Wubansia Barlow Condensed" },
            { "BarlowCondensedBold_ttf", "Wubansia Barlow Condensed" },
            { "BarlowCondensedExtraBold_ttf", "Wubansia Barlow Condensed" },
            { "SpaceMonoRegular_ttf", "Wubansia Space Mono" },
        } };

        // The typefaces, made from the binary data on first use and kept until JUCE shuts down (message thread).
        class Typefaces : public juce::DeletedAtShutdown
        {
        public:
            Typefaces()
            {
                for (std::size_t i = 0; i < kFaceTable.size(); ++i)
                {
                    int size = 0;
                    if (const auto* data = WubansiaFontData::getNamedResource (kFaceTable[i].resource, size); data != nullptr && size > 0)
                        faces[i] = juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
                    jassert (faces[i] != nullptr);
                }
            }

            ~Typefaces() override { clearSingletonInstance(); }

            std::array<juce::Typeface::Ptr, kFaces> faces;

            JUCE_DECLARE_SINGLETON_SINGLETHREADED_MINIMAL_INLINE (Typefaces)
        };
    }

    juce::Typeface::Ptr typefaceFor (FaceId face)
    {
        const auto i = static_cast<std::size_t> (face);
        return i < (std::size_t) kFaces ? Typefaces::getInstance()->faces[i] : nullptr;
    }

    juce::String familyOf (FaceId face)
    {
        const auto i = static_cast<std::size_t> (face);
        return i < (std::size_t) kFaces ? juce::String (kFaceTable[i].family) : juce::String();
    }

    juce::Font fontFor (FaceId face, float px, float letterSpacingPx, bool fallback)
    {
        auto typeface = typefaceFor (face);
        auto options = typeface != nullptr ? juce::FontOptions().withTypeface (typeface) : juce::FontOptions();
        options = options.withPointHeight (px).withFallbackEnabled (fallback);
        juce::Font font (options);
        if (! juce::exactlyEqual (letterSpacingPx, 0.0f))
            font.setExtraKerningFactor (letterSpacingPx / font.getHeight()); // tracking is a multiple of the font's height
        return font;
    }
}
