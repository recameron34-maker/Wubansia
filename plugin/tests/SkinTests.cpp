// The looks' data (ADR 0032): which kit wears which look, the embedded faces and what they cover, every text style's
// font, and the colours' contrast. The looks in the editor are LooksTests.cpp's.

#include "content/EmbeddedContent.h"
#include "editor/EditorModel.h"
#include "editor/skin/Skin.h"
#include "services/KitCatalog.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <map>
#include <set>

using namespace wubansia;
using namespace wubansia::editor;

namespace
{
    constexpr std::array<SkinId, 4> kLooks { SkinId::nightBus, SkinId::missionHud, SkinId::deepGlass, SkinId::machineRoom };

    // The faces each look draws with (Skin.h, ADR 0032's table).
    const std::map<SkinId, std::vector<FaceId>>& facesOfLook()
    {
        static const std::map<SkinId, std::vector<FaceId>> faces {
            { SkinId::nightBus, { FaceId::archivoRegular, FaceId::archivoExtraBold, FaceId::jetBrainsMonoRegular } },
            { SkinId::missionHud, { FaceId::chakraPetchRegular, FaceId::chakraPetchBold, FaceId::hudMonoRegular } },
            { SkinId::deepGlass, { FaceId::manropeRegular, FaceId::manropeSemiBold, FaceId::manropeExtraBold } },
            { SkinId::machineRoom, { FaceId::barlowCondensedSemiBold, FaceId::barlowCondensedBold, FaceId::barlowCondensedExtraBold, FaceId::spaceMonoRegular } },
        };
        return faces;
    }

    bool covers (FaceId face, juce::juce_wchar c)
    {
        const auto typeface = typefaceFor (face);
        return typeface != nullptr && typeface->getNominalGlyphForCodepoint (c).has_value();
    }

    // The characters the editor's own words use: printable ASCII, the middle dot and the ellipsis.
    juce::String uiCharacters()
    {
        juce::String s;
        for (juce::juce_wchar c = 0x20; c < 0x7f; ++c)
            s += juce::String::charToString (c);
        return s + utf8 ("\xc2\xb7\xe2\x80\xa6");
    }
}

TEST_CASE ("each kit has its look, and any other kit night bus", "[looks]")
{
    CHECK (skinForKit ("FUT01").id == SkinId::nightBus);
    CHECK (skinForKit ("FUT02").id == SkinId::missionHud);
    CHECK (skinForKit ("FUT03").id == SkinId::deepGlass);
    CHECK (skinForKit ("FUT04").id == SkinId::machineRoom);
    CHECK (skinForKit ("").id == SkinId::nightBus);
    CHECK (skinForKit ("SYN01").id == SkinId::nightBus);

    for (const auto id : kLooks)
    {
        CHECK (&skinFor (id) == &skinFor (id)); // the same object every call
        CHECK (skinFor (id).id == id);
        CHECK (skinFor (id).name.isNotEmpty());
    }
    CHECK (&skinForKit ("FUT03") == &skinFor (SkinId::deepGlass));
    CHECK (skinFor (SkinId::nightBus).name == "Night bus");
    CHECK (skinFor (SkinId::missionHud).name == "Mission HUD");
    CHECK (skinFor (SkinId::deepGlass).name == "Deep glass");
    CHECK (skinFor (SkinId::machineRoom).name == "Machine room");
}

TEST_CASE ("every face is embedded and has every character the looks draw", "[looks]")
{
    const std::map<FaceId, juce::String> families {
        { FaceId::archivoRegular, "Wubansia Archivo" }, { FaceId::archivoExtraBold, "Wubansia Archivo" },
        { FaceId::jetBrainsMonoRegular, "Wubansia JetBrains Mono" }, { FaceId::chakraPetchRegular, "Wubansia Chakra Petch" },
        { FaceId::chakraPetchBold, "Wubansia Chakra Petch" }, { FaceId::hudMonoRegular, "Wubansia HUD Mono" },
        { FaceId::manropeRegular, "Wubansia Manrope" }, { FaceId::manropeSemiBold, "Wubansia Manrope" },
        { FaceId::manropeExtraBold, "Wubansia Manrope" }, { FaceId::barlowCondensedSemiBold, "Wubansia Barlow Condensed" },
        { FaceId::barlowCondensedBold, "Wubansia Barlow Condensed" }, { FaceId::barlowCondensedExtraBold, "Wubansia Barlow Condensed" },
        { FaceId::spaceMonoRegular, "Wubansia Space Mono" },
    };
    REQUIRE ((int) families.size() == kFaces);

    const auto ui = uiCharacters();
    REQUIRE (ui.length() == 97);
    for (const auto& [face, family] : families)
    {
        INFO ("face " << (int) face);
        const auto typeface = typefaceFor (face);
        REQUIRE (typeface != nullptr);
        CHECK (typeface == typefaceFor (face)); // made once
        CHECK (familyOf (face) == family);
        CHECK (typeface->getName() == family);
        for (int i = 0; i < ui.length(); ++i)
        {
            INFO ("U+" << juce::String::toHexString ((int) ui[i]));
            CHECK (covers (face, ui[i]));
        }
    }

    // Every string a kit brings, in every face its own look draws with.
    const KitCatalog catalog (EmbeddedContent::sources());
    REQUIRE (catalog.entries().size() == 4);
    for (const auto& entry : catalog.entries())
    {
        INFO (entry.id);
        REQUIRE (entry.ok());
        const auto& card = entry.card;
        juce::StringArray strings { card.id, card.title, card.direction, card.key, card.camelot };
        for (const auto& label : card.laneLabels)
            strings.add (label);
        for (std::size_t i = 0; i < card.abA.size(); ++i)
            strings.addArray (juce::StringArray { card.abA[i], card.abB[i] });
        for (const auto& variant : entry.sections)
            for (const auto& section : variant)
                strings.add (section.name);
        for (const auto& row : entry.phraseRows)
            strings.add (juce::String (row.name));
        for (const auto& sound : entry.sounds)
            strings.add (sound.name);
        strings.add (card.phraseChords.joinIntoString (" "));
        REQUIRE (strings.size() > 30);

        const auto look = skinForKit (card.id).id;
        for (const auto face : facesOfLook().at (look))
            for (const auto& s : strings)
                for (int i = 0; i < s.length(); ++i)
                {
                    INFO (s << ": U+" << juce::String::toHexString ((int) s[i]) << " in face " << (int) face);
                    CHECK (covers (face, s[i]));
                }
    }
}

TEST_CASE ("every text style is its look's embedded face at its size, with fallback off but for run-time text", "[looks]")
{
    for (const auto id : kLooks)
    {
        const auto& skin = skinFor (id);
        INFO (skin.name);
        const auto styles = stylesOf (skin);
        REQUIRE (styles.size() == 32);
        const auto& faces = facesOfLook().at (id);
        for (const auto& [role, style] : styles)
        {
            INFO (role);
            const auto font = style->font();
            CHECK (std::find (faces.begin(), faces.end(), style->face) != faces.end());
            CHECK (font.getTypefacePtr() == typefaceFor (style->face));
            CHECK (font.getHeightInPoints() == Catch::Approx (style->px).margin (0.01));
            // Strings the run time builds (tooltips, menus, the drawers' messages, folder paths) may borrow a platform
            // glyph; every fixed word and kit text is drawn in the face alone.
            const auto runTime = role == "body" || role == "bodyStrong" || role == "path";
            CHECK (style->fallback == runTime);
            CHECK (font.getFallbackEnabled() == style->fallback);
            CHECK (style->minPx <= style->px);
        }
    }
}

TEST_CASE ("the looks' text and controls meet WCAG AA contrast", "[looks]")
{
    for (const auto id : kLooks)
    {
        const auto& skin = skinFor (id);
        const auto& c = skin.colours;
        INFO (skin.name);
        const auto face = c.face;
        const auto panel = over (c.panel, face);
        const auto lane = over (c.lane, face);
        const auto well = over (c.well, face);

        for (const auto background : { panel, lane, well })
        {
            CHECK (contrastRatio (c.text, background) >= 4.5f);
            CHECK (contrastRatio (c.dim, background) >= 4.5f);
            CHECK (contrastRatio (c.label, background) >= 4.5f);
        }
        CHECK (contrastRatio (c.laneNumber, lane) >= 4.5f);
        CHECK (contrastRatio (c.accentInk, c.accent) >= 4.5f);
        CHECK (contrastRatio (c.accent2Ink, c.accent2) >= 4.5f);
        CHECK (contrastRatio (c.accentTintText, over (c.accentTint, panel)) >= 4.5f);
        CHECK (contrastRatio (c.accent2TintText, over (c.accent2Tint, panel)) >= 4.5f);
        CHECK (contrastRatio (c.drums, lane) >= 4.5f);
        CHECK (contrastRatio (c.music, lane) >= 4.5f);
        CHECK (contrastRatio (c.hookLane, lane) >= 4.5f);
        CHECK (contrastRatio (c.pillOffText, over (c.pillOff, face)) >= 4.5f);
        CHECK (contrastRatio (c.pillOnText, over (c.pillOn, face)) >= 4.5f);
        CHECK (contrastRatio (c.sectionOffText, over (c.sectionOff, face)) >= 4.5f);
        CHECK (contrastRatio (c.sectionLitText, over (c.sectionLit, face)) >= 4.5f);

        // Control boundaries and states: 3:1.
        CHECK (contrastRatio (over (c.controlEdge, lane), lane) >= 3.0f);
        CHECK (contrastRatio (over (c.controlEdge, panel), panel) >= 3.0f);
        CHECK (contrastRatio (over (c.faderTrack, lane), lane) >= 3.0f);
        CHECK (contrastRatio (c.focus, face) >= 3.0f);
        CHECK (contrastRatio (c.focus, panel) >= 3.0f);
        const auto cellOff = over (c.cellOff, lane);
        CHECK (contrastRatio (over (c.drums, cellOff), cellOff) >= 3.0f);
        CHECK (contrastRatio (over (c.music, cellOff), cellOff) >= 3.0f);
        if (id == SkinId::machineRoom) // the one look with held cells of their own colour
        {
            CHECK (contrastRatio (c.cellHeld, cellOff) >= 3.0f);
            CHECK (contrastRatio (c.music, c.cellHeld) >= 3.0f);
            CHECK (c.laneNumber == juce::Colour (0xff948d82)); // the board's #6f6a61 read 3.37:1
        }
    }
}

TEST_CASE ("contrastRatio is WCAG's", "[looks]")
{
    CHECK (contrastRatio (juce::Colours::white, juce::Colours::black) == Catch::Approx (21.0f).margin (0.01));
    CHECK (contrastRatio (juce::Colours::black, juce::Colours::white) == Catch::Approx (21.0f).margin (0.01));
    CHECK (contrastRatio (juce::Colour (0xff3ee6ff), juce::Colour (0xff3ee6ff)) == Catch::Approx (1.0f));
    CHECK (contrastRatio (juce::Colour (0xff767676), juce::Colours::white) == Catch::Approx (4.54f).margin (0.01));
    CHECK (over (juce::Colours::white.withAlpha (0.5f), juce::Colours::black).getRed() == 128);
}
