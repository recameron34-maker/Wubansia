#include "editor/skin/Skin.h"


#include <cmath>

namespace wubansia::editor
{
    juce::Font TextStyle::font() const { return fontFor (face, px, letterSpacingPx, fallback); }
    juce::Font TextStyle::fontAt (float size) const { return fontFor (face, size, letterSpacingPx, fallback); }

    juce::Font bodyFont (const Skin& skin, float juceHeight, bool strong, float kerningFactor)
    {
        const auto& style = strong ? skin.type.bodyStrong : skin.type.body;
        return fontFor (style.face, juceHeight / 1.117f, kerningFactor * juceHeight, style.fallback);
    }

    juce::Font pathFont (const Skin& skin, float juceHeight)
    {
        return fontFor (skin.type.path.face, juceHeight / 1.117f, 0.0f, true);
    }

    namespace
    {
        juce::Colour hex (juce::uint32 rgb) { return juce::Colour (0xff000000 | rgb); }
        juce::Colour white (float alpha) { return juce::Colours::white.withAlpha (alpha); }

        TextStyle style (FaceId face, float px, float spacing = 0.0f, bool upper = false, float minPx = 0.0f)
        {
            TextStyle s;
            s.face = face;
            s.px = px;
            s.letterSpacingPx = spacing;
            s.upper = upper;
            s.minPx = minPx;
            return s;
        }

        BoxStyle boxStyle (float corner, float border = 1.0f)
        {
            BoxStyle b;
            b.corner = corner;
            b.border = border;
            return b;
        }

        // Every style that names no floor may shrink 2 px before it cuts; body and path take the platform's glyphs
        // for run-time text.
        void finish (Skin& s)
        {
            for (auto& [role, constStyle] : stylesOf (s))
            {
                auto& style = const_cast<TextStyle&> (*constStyle);
                if (style.minPx <= 0.0f)
                    style.minPx = std::max (7.0f, style.px - 2.0f);
            }
            s.type.body.fallback = s.type.bodyStrong.fallback = s.type.path.fallback = true;
        }

        // FUT01: night-bus chrome (Main.dc.html). Archivo and JetBrains Mono; pink drums, cyan music.
        Skin makeNightBus()
        {
            Skin s;
            s.id = SkinId::nightBus;
            s.name = "Night bus";
            auto& c = s.colours;
            c.face = hex (0x121216);
            c.faceGlow = juce::Colours::transparentBlack;
            c.texture = juce::Colours::transparentBlack;
            c.panel = hex (0x24252b);
            c.panelEdge = hex (0x55575f);
            c.panelHighlight = hex (0x7d808a);
            c.lane = hex (0x1d1e23);
            c.laneEdge = hex (0x3a3b42);
            c.laneHighlight = hex (0x4f515a);
            c.well = hex (0x08080b);
            c.wellEdge = hex (0x3a3b42);
            c.grid = hex (0x2a2b31);
            c.text = hex (0xf2efe9);
            c.dim = hex (0xc9c6bf);
            c.faint = hex (0xa7a4a0);
            c.label = hex (0xb9b6b0);
            c.laneNumber = hex (0xb9b6b0);
            c.accent = hex (0xff4fa3);
            c.accentInk = hex (0x1a0b12);
            c.accentTint = hex (0x2a1520);
            c.accentTintText = hex (0xffd6ea);
            c.accent2 = hex (0x3ee6ff);
            c.accent2Ink = hex (0x121216);
            c.accent2Tint = hex (0x0f2a30);
            c.accent2TintText = hex (0xc8f7ff);
            c.drums = hex (0xff4fa3);
            c.music = hex (0x3ee6ff);
            c.hookLane = hex (0x3ee6ff);
            c.controlEdge = hex (0x6e717b); // the board's #55575f reads 2.31:1 on a lane; 3.41:1 here
            c.faderTrack = hex (0x8b8e98); // the board's #55575f: 2.31:1, now 5.09:1
            c.faderCap = hex (0xc9ccd4);
            c.cellOff = hex (0x26262c);
            c.cellOffEdge = hex (0x26262c);
            c.cellHeld = juce::Colours::transparentBlack; // the lane's colour at 30 %
            c.meterOff = hex (0x0b0b0e);
            c.sectionOff = hex (0x1b1b20);
            c.sectionOffText = hex (0xa7a4a0);
            c.sectionLit = hex (0x2a1520);
            c.sectionLitText = hex (0xffd6ea);
            c.pillOff = hex (0x1b1b20);
            c.pillOffEdge = hex (0x3a3b42);
            c.pillOffText = hex (0xc9c6bf);
            c.pillOn = hex (0x2a1520);
            c.pillOnEdge = hex (0xff4fa3);
            c.pillOnText = hex (0xffd6ea);
            c.busy = hex (0xffc23d);
            c.warning = hex (0xff6a4d);
            c.focus = hex (0x3ee6ff);

            using F = FaceId;
            auto& t = s.type;
            t.wordmark = style (F::archivoExtraBold, 17, 3, true);
            t.subtitle = style (F::archivoRegular, 11, 1.5f, true, 9);
            t.pill = style (F::archivoRegular, 16);
            t.pillSelected = style (F::archivoExtraBold, 16, 0, false, 13);
            t.headerButton = style (F::archivoExtraBold, 16);
            t.displayLine = style (F::jetBrainsMonoRegular, 11, 1);
            t.displayLineStrong = style (F::jetBrainsMonoRegular, 11, 1);
            t.displayLabel = style (F::jetBrainsMonoRegular, 10, 0, false, 8);
            t.transportLine = style (F::jetBrainsMonoRegular, 12, 0, false, 10);
            t.playMode = style (F::archivoExtraBold, 16, 0, true);
            t.playModeOff = style (F::archivoExtraBold, 16, 0, true);
            t.rowLabel = style (F::archivoRegular, 10, 1, true);
            t.rowValue = style (F::archivoExtraBold, 12, 0, true);
            t.section = style (F::archivoRegular, 12, 0, false, 9);
            t.laneName = style (F::archivoExtraBold, 12, 0.5f, false, 10);
            t.laneSub = style (F::jetBrainsMonoRegular, 10, 0, false, 8);
            t.laneButton = style (F::archivoExtraBold, 11);
            t.laneChip = style (F::archivoExtraBold, 10);
            t.abTitle = style (F::archivoRegular, 11, 1.5f, true, 9);
            t.abSide = style (F::archivoRegular, 16, 0, false, 12);
            t.abSideOn = style (F::archivoExtraBold, 16, 0, false, 12);
            t.abRow = style (F::archivoExtraBold, 13, 0, false, 10);
            t.fxName = style (F::archivoExtraBold, 11, 1, true);
            t.fxValue = style (F::jetBrainsMonoRegular, 10);
            t.stripLabel = style (F::archivoRegular, 10, 1.5f, true);
            t.stripChip = style (F::archivoExtraBold, 12);
            t.stripChipOff = style (F::archivoRegular, 12);
            t.stripSegment = style (F::archivoRegular, 12);
            t.stripSegmentOn = style (F::archivoExtraBold, 12);
            t.body = style (F::archivoRegular, 13);
            t.bodyStrong = style (F::archivoExtraBold, 13);
            t.path = style (F::jetBrainsMonoRegular, 11);

            auto& b = s.shape;
            b.panel = boxStyle (12);
            b.panel.insetHighlight = true;
            b.lane = boxStyle (12);
            b.lane.insetHighlight = true;
            b.well = boxStyle (10);
            b.pill = boxStyle (22);
            b.pillSelected = boxStyle (22);
            b.headerButton = boxStyle (10);
            b.segmentTrack = boxStyle (8, 0);
            b.segment = boxStyle (8);
            b.abSide = boxStyle (8);
            b.laneButton = boxStyle (6);
            b.laneChip = boxStyle (6);
            b.laneChip.dashed = true;
            b.stripChip = boxStyle (6);
            b.stripSwitch = boxStyle (6);
            b.section = boxStyle (6);
            b.cell = boxStyle (2, 0);
            b.drawer = boxStyle (12);
            b.drawer.insetHighlight = true;
            b.knob = 56.0f;
            b.playButton = 56.0f;

            s.words = { "WUBANSIA", "Drag MIDI", "Part", "M", "ON", "OFF" };
            s.display = DisplayKind::stepGrid;
            s.lane = LaneKind::faderAndMeter;
            s.fx = FxKind::knobs;
            s.texture = Texture::plain;
            finish (s);
            return s;
        }

        float luminance (juce::Colour c) noexcept
        {
            const auto channel = [] (juce::uint8 v)
            {
                const auto x = (double) v / 255.0;
                return x <= 0.04045 ? x / 12.92 : std::pow ((x + 0.055) / 1.055, 2.4);
            };
            return (float) (0.2126 * channel (c.getRed()) + 0.7152 * channel (c.getGreen()) + 0.0722 * channel (c.getBlue()));
        }
    }

    std::vector<std::pair<juce::String, const TextStyle*>> stylesOf (const Skin& s)
    {
        const auto& t = s.type;
        return { { "wordmark", &t.wordmark }, { "subtitle", &t.subtitle }, { "pill", &t.pill }, { "pillSelected", &t.pillSelected },
            { "headerButton", &t.headerButton }, { "displayLine", &t.displayLine }, { "displayLineStrong", &t.displayLineStrong },
            { "displayLabel", &t.displayLabel }, { "transportLine", &t.transportLine }, { "playMode", &t.playMode },
            { "playModeOff", &t.playModeOff }, { "rowLabel", &t.rowLabel }, { "rowValue", &t.rowValue }, { "section", &t.section },
            { "laneName", &t.laneName }, { "laneSub", &t.laneSub }, { "laneButton", &t.laneButton }, { "laneChip", &t.laneChip },
            { "abTitle", &t.abTitle }, { "abSide", &t.abSide }, { "abSideOn", &t.abSideOn }, { "abRow", &t.abRow },
            { "fxName", &t.fxName }, { "fxValue", &t.fxValue }, { "stripLabel", &t.stripLabel }, { "stripChip", &t.stripChip },
            { "stripChipOff", &t.stripChipOff }, { "stripSegment", &t.stripSegment }, { "stripSegmentOn", &t.stripSegmentOn },
            { "body", &t.body }, { "bodyStrong", &t.bodyStrong }, { "path", &t.path } };
    }

    const Skin& skinFor (SkinId id) noexcept
    {
        static const Skin nightBus = makeNightBus();
        switch (id)
        {
            case SkinId::nightBus:
            case SkinId::missionHud:
            case SkinId::deepGlass:
            case SkinId::machineRoom:
                break;
        }
        return nightBus;
    }

    const Skin& skinForKit (const juce::String&) noexcept
    {
        return skinFor (SkinId::nightBus); // every kit wears night bus until its own look lands
    }

    float contrastRatio (juce::Colour a, juce::Colour b) noexcept
    {
        const auto la = luminance (a), lb = luminance (b);
        return (std::max (la, lb) + 0.05f) / (std::min (la, lb) + 0.05f);
    }

    juce::Colour over (juce::Colour top, juce::Colour background) noexcept
    {
        const auto a = top.getFloatAlpha();
        const auto mix = [a] (juce::uint8 t, juce::uint8 b) { return (juce::uint8) juce::roundToInt ((float) t * a + (float) b * (1.0f - a)); };
        return juce::Colour (mix (top.getRed(), background.getRed()), mix (top.getGreen(), background.getGreen()),
            mix (top.getBlue(), background.getBlue()));
    }
}
