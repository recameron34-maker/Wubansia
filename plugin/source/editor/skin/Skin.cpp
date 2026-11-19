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

        // FUT02: mission-control HUD (OrbitRelay.dc.html). Chakra Petch and Wubansia HUD Mono (IBM Plex Mono, renamed);
        // square boxes, the selected pill chamfered; cyan, the lead lane gold.
        Skin makeMissionHud()
        {
            Skin s;
            s.id = SkinId::missionHud;
            s.name = "Mission HUD";
            auto& c = s.colours;
            c.face = hex (0x050b14);
            c.faceGlow = juce::Colours::transparentBlack;
            c.texture = hex (0x123544);
            c.panel = hex (0x071219);
            c.panelEdge = hex (0x1f4a5a);
            c.panelHighlight = hex (0x1f4a5a);
            c.lane = hex (0x061019);
            c.laneEdge = hex (0x1f4a5a);
            c.laneHighlight = hex (0x1f4a5a);
            c.well = hex (0x03080e);
            c.wellEdge = hex (0x1f4a5a);
            c.grid = hex (0x123544);
            c.text = hex (0xdff8ff);
            c.dim = hex (0x8fd8e8);
            c.faint = hex (0x8fd8e8);
            c.label = hex (0x8fd8e8);
            c.laneNumber = hex (0x8fd8e8);
            c.accent = hex (0x5ee7ff);
            c.accentInk = hex (0x03080e);
            c.accentTint = hex (0x08202b);
            c.accentTintText = hex (0xdff8ff);
            c.accent2 = hex (0xffd166);
            c.accent2Ink = hex (0x03080e);
            c.accent2Tint = hex (0x221b08);
            c.accent2TintText = hex (0xffe7a8);
            c.drums = hex (0x5ee7ff);
            c.music = hex (0x5ee7ff);
            c.hookLane = hex (0xffd166); // the lead carries the hook (ADR 0026)
            c.controlEdge = hex (0x2f6b80); // the board's #1f4a5a reads 2.05:1; 3.32:1 here
            c.faderTrack = hex (0x2f6b80);
            c.faderCap = hex (0xdff8ff);
            c.cellOff = hex (0x0c2230);
            c.cellOffEdge = hex (0x0c2230);
            c.cellHeld = juce::Colours::transparentBlack;
            c.meterOff = hex (0x0c2230);
            c.sectionOff = hex (0x03080e);
            c.sectionOffText = hex (0x8fd8e8);
            c.sectionLit = hex (0x08202b);
            c.sectionLitText = hex (0xdff8ff);
            c.pillOff = hex (0x071219);
            c.pillOffEdge = hex (0x2f6b80);
            c.pillOffText = hex (0x8fd8e8);
            c.pillOn = hex (0x08202b);
            c.pillOnEdge = hex (0x5ee7ff);
            c.pillOnText = hex (0xdff8ff);
            c.busy = hex (0xffd166);
            c.warning = hex (0xff6a4d);
            c.focus = hex (0xffd166);

            using F = FaceId;
            auto& t = s.type;
            t.wordmark = style (F::chakraPetchBold, 17, 4, true);
            t.subtitle = style (F::hudMonoRegular, 10, 1, true, 8);
            t.pill = style (F::chakraPetchRegular, 16);
            t.pillSelected = style (F::chakraPetchBold, 16, 0, true, 13);
            t.headerButton = style (F::chakraPetchBold, 16, 0, true);
            t.displayLine = style (F::hudMonoRegular, 11);
            t.displayLineStrong = style (F::hudMonoRegular, 11);
            t.displayLabel = style (F::hudMonoRegular, 10, 0, false, 8);
            t.transportLine = style (F::hudMonoRegular, 11, 0, false, 9);
            t.playMode = style (F::chakraPetchBold, 16, 0, true);
            t.playModeOff = style (F::chakraPetchBold, 16, 0, true);
            t.rowLabel = style (F::hudMonoRegular, 10, 0, true);
            t.rowValue = style (F::hudMonoRegular, 11, 0, true);
            t.section = style (F::hudMonoRegular, 11, 0, true, 9);
            t.laneName = style (F::chakraPetchBold, 12, 1, true, 9.5f);
            t.laneSub = style (F::hudMonoRegular, 10, 0, false, 8);
            t.laneButton = style (F::chakraPetchBold, 11, 0, true);
            t.laneChip = style (F::chakraPetchBold, 10);
            t.abTitle = style (F::hudMonoRegular, 11, 0, true, 9);
            t.abSide = style (F::chakraPetchRegular, 16, 0, true, 12);
            t.abSideOn = style (F::chakraPetchBold, 16, 0, true, 12);
            t.abRow = style (F::chakraPetchBold, 13, 0, true, 10);
            t.fxName = style (F::hudMonoRegular, 10, 1, true);
            t.fxValue = style (F::hudMonoRegular, 11);
            t.stripLabel = style (F::hudMonoRegular, 10, 0, true);
            t.stripChip = style (F::chakraPetchBold, 12);
            t.stripChipOff = style (F::chakraPetchRegular, 12);
            t.stripSegment = style (F::chakraPetchRegular, 12, 0, true);
            t.stripSegmentOn = style (F::chakraPetchBold, 12, 0, true);
            t.body = style (F::chakraPetchRegular, 13);
            t.bodyStrong = style (F::chakraPetchBold, 13);
            t.path = style (F::hudMonoRegular, 11);

            auto& b = s.shape;
            for (auto* box : { &b.panel, &b.lane, &b.well, &b.pill, &b.pillSelected, &b.headerButton, &b.segment, &b.abSide, &b.laneButton,
                     &b.laneChip, &b.stripChip, &b.stripSwitch, &b.section, &b.drawer })
                *box = boxStyle (0);
            b.segmentTrack = boxStyle (0, 0);
            b.cell = boxStyle (0, 0);
            b.pillSelected.chamfer = 10;
            b.laneChip.dashed = true;
            b.knob = 56.0f;
            b.playButton = 56.0f;

            s.words = { "WUBANSIA", "Uplink MIDI", "PART", "MUTE", "ONLINE", "OFFLINE" };
            s.display = DisplayKind::orbit;
            s.lane = LaneKind::telemetryColumn;
            s.fx = FxKind::knobs;
            s.texture = Texture::grid;
            finish (s);
            return s;
        }

        // FUT03: frosted glass over deep water (GlassCurrent.dc.html). Manrope throughout, in small letters where the
        // board writes them; translucent panels, capsules; aqua drums, coral music.
        Skin makeDeepGlass()
        {
            Skin s;
            s.id = SkinId::deepGlass;
            s.name = "Deep glass";
            auto& c = s.colours;
            const auto aqua = hex (0x7fe8d8), coral = hex (0xff9e7a);
            c.face = hex (0x0a2230);
            c.faceGlow = juce::Colours::transparentBlack;
            c.texture = hex (0x123a4a);
            c.panel = white (0.07f);
            c.panelEdge = white (0.18f);
            c.panelHighlight = white (0.18f);
            c.lane = white (0.06f);
            c.laneEdge = white (0.16f);
            c.laneHighlight = white (0.16f);
            c.well = white (0.07f);
            c.wellEdge = white (0.18f);
            c.grid = white (0.08f);
            c.text = hex (0xeefaf8);
            c.dim = hex (0xcde6e3);
            c.faint = hex (0xb6d6d2);
            c.label = hex (0xcde6e3);
            c.laneNumber = hex (0xcde6e3);
            c.accent = aqua;
            c.accentInk = hex (0x0a2230);
            c.accentTint = aqua.withAlpha (0.2f);
            c.accentTintText = hex (0xffffff);
            c.accent2 = coral;
            c.accent2Ink = hex (0x0a2230);
            c.accent2Tint = coral.withAlpha (0.16f);
            c.accent2TintText = hex (0xffe1d4);
            c.drums = aqua;
            c.music = coral;
            c.hookLane = coral;
            c.controlEdge = white (0.35f); // the board's 25 % reads 2.22:1; 3.03:1 here
            c.faderTrack = white (0.35f);
            c.faderCap = hex (0xffffff);
            c.cellOff = white (0.06f);
            c.cellOffEdge = white (0.06f);
            c.cellHeld = juce::Colours::transparentBlack;
            c.meterOff = juce::Colours::black.withAlpha (0.25f);
            c.sectionOff = white (0.05f);
            c.sectionOffText = hex (0xb6d6d2);
            c.sectionLit = aqua.withAlpha (0.22f);
            c.sectionLitText = hex (0xffffff);
            c.pillOff = white (0.05f);
            c.pillOffEdge = white (0.16f);
            c.pillOffText = hex (0xcde6e3);
            c.pillOn = white (0.16f);
            c.pillOnEdge = white (0.35f);
            c.pillOnText = hex (0xffffff);
            c.busy = hex (0xffd166);
            c.warning = hex (0xff8a6a);
            c.focus = hex (0xffffff);

            using F = FaceId;
            auto& t = s.type;
            t.wordmark = style (F::manropeExtraBold, 18, 1);
            t.subtitle = style (F::manropeRegular, 11, 0, false, 9);
            t.pill = style (F::manropeRegular, 16);
            t.pillSelected = style (F::manropeExtraBold, 16, 0, false, 13);
            t.headerButton = style (F::manropeExtraBold, 16);
            t.displayLine = style (F::manropeRegular, 12);
            t.displayLineStrong = style (F::manropeExtraBold, 12);
            t.displayLabel = style (F::manropeRegular, 10, 0, false, 8);
            t.transportLine = style (F::manropeRegular, 12, 0, false, 10);
            t.playMode = style (F::manropeExtraBold, 16);
            t.playModeOff = style (F::manropeSemiBold, 16);
            t.rowLabel = style (F::manropeRegular, 11);
            t.rowValue = style (F::manropeExtraBold, 12);
            t.section = style (F::manropeRegular, 12, 0, false, 9);
            t.laneName = style (F::manropeExtraBold, 12, 0, false, 10);
            t.laneSub = style (F::manropeRegular, 10, 0, false, 8);
            t.laneSub.lower = true;
            t.laneButton = style (F::manropeExtraBold, 11);
            t.laneChip = style (F::manropeExtraBold, 10);
            t.abTitle = style (F::manropeRegular, 12, 0, false, 9);
            t.abSide = style (F::manropeRegular, 16, 0, false, 12);
            t.abSideOn = style (F::manropeExtraBold, 16, 0, false, 12);
            t.abRow = style (F::manropeExtraBold, 13, 0, false, 10);
            t.fxName = style (F::manropeExtraBold, 12);
            t.fxValue = style (F::manropeRegular, 12);
            t.stripLabel = style (F::manropeRegular, 11);
            t.stripChip = style (F::manropeExtraBold, 12);
            t.stripChipOff = style (F::manropeRegular, 12);
            t.stripSegment = style (F::manropeRegular, 12);
            t.stripSegmentOn = style (F::manropeExtraBold, 12);
            t.body = style (F::manropeRegular, 13);
            t.bodyStrong = style (F::manropeExtraBold, 13);
            t.path = style (F::manropeRegular, 12);

            auto& b = s.shape;
            b.panel = boxStyle (18);
            b.lane = boxStyle (18);
            b.well = boxStyle (18);
            b.drawer = boxStyle (18);
            for (auto* box : { &b.pill, &b.pillSelected, &b.headerButton, &b.segmentTrack, &b.segment, &b.abSide, &b.laneButton, &b.laneChip,
                     &b.stripChip, &b.stripSwitch, &b.section })
                *box = boxStyle (99);
            b.laneChip.dashed = true;
            b.cell = boxStyle (2, 0);
            b.knob = 56.0f;
            b.playButton = 60.0f;

            s.words = { "wubansia", "drag midi", "part", "Mute", "on", "off" };
            s.display = DisplayKind::current;
            s.lane = LaneKind::glassPill;
            s.fx = FxKind::bars;
            s.texture = Texture::water;
            finish (s);
            return s;
        }

        // FUT04: the machine room (FurnaceCode.dc.html). Barlow Condensed and Space Mono in capitals, near-square
        // boxes, amber on black with molten for the drums and accents, scanlines and a furnace glow.
        Skin makeMachineRoom()
        {
            Skin s;
            s.id = SkinId::machineRoom;
            s.name = "Machine room";
            auto& c = s.colours;
            const auto amber = hex (0xffb000), molten = hex (0xff4a1c);
            c.face = hex (0x0c0c0d);
            c.faceGlow = molten.withAlpha (0.16f);
            c.texture = white (0.014f);
            c.panel = hex (0x1a1b1e);
            c.panelEdge = hex (0x2c2d31);
            c.panelHighlight = hex (0x111214);
            c.lane = hex (0x18191c);
            c.laneEdge = hex (0x2c2d31);
            c.laneHighlight = hex (0x111214);
            c.well = hex (0x070502);
            c.wellEdge = hex (0x3a2a00);
            c.grid = amber.withAlpha (0.11f);
            c.text = hex (0xece6da);
            c.dim = hex (0xc9c2b4);
            c.faint = hex (0x8d877c);
            c.label = hex (0xc99a3a);
            c.laneNumber = hex (0x948d82); // the board's #6f6a61 reads 3.37:1; 5.51:1 here
            c.accent = amber;
            c.accentInk = hex (0x0c0c0d);
            c.accentTint = hex (0x241a00);
            c.accentTintText = hex (0xffe2a6);
            c.accent2 = molten;
            c.accent2Ink = hex (0x0c0c0d);
            c.accent2Tint = hex (0x2a0f08);
            c.accent2TintText = hex (0xffd2c4);
            c.drums = molten;
            c.music = amber;
            c.hookLane = amber;
            c.controlEdge = hex (0x6a6c73); // the board's #3a3b40 reads 1.62:1; 3.45:1 here
            c.faderTrack = hex (0x6a6c73);
            c.faderCap = amber;
            c.cellOff = hex (0x1c1d21);
            c.cellOffEdge = hex (0x2a2b30);
            c.cellHeld = hex (0x8a6000); // the board's #4a3300 reads 1.42:1 against an empty cell; 3.01:1 here
            c.meterOff = hex (0x24252a);
            c.sectionOff = hex (0x17181b);
            c.sectionOffText = hex (0x8d877c);
            c.sectionLit = molten;
            c.sectionLitText = hex (0x0c0c0d);
            c.pillOff = hex (0x141518);
            c.pillOffEdge = hex (0x2c2d31);
            c.pillOffText = hex (0x8d877c);
            c.pillOn = hex (0x24252a);
            c.pillOnEdge = hex (0x3a3b40);
            c.pillOnText = hex (0xf6f0e4);
            c.busy = amber;
            c.warning = molten;
            c.focus = amber;

            using F = FaceId;
            auto& t = s.type;
            t.wordmark = style (F::barlowCondensedExtraBold, 22, 3, true);
            t.subtitle = style (F::spaceMonoRegular, 10, 0, true, 8);
            t.pill = style (F::barlowCondensedSemiBold, 16, 1.5f, true);
            t.pillSelected = style (F::barlowCondensedBold, 17, 1.5f, true, 13);
            t.headerButton = style (F::barlowCondensedBold, 16, 2, true);
            t.displayLine = style (F::spaceMonoRegular, 11);
            t.displayLineStrong = style (F::spaceMonoRegular, 11);
            t.displayLabel = style (F::spaceMonoRegular, 9, 0, false, 8);
            t.transportLine = style (F::spaceMonoRegular, 10, 0, true, 8);
            t.playMode = style (F::barlowCondensedExtraBold, 17, 3, true);
            t.playModeOff = style (F::barlowCondensedExtraBold, 17, 3, true);
            t.rowLabel = style (F::spaceMonoRegular, 10, 0, true);
            t.rowValue = style (F::spaceMonoRegular, 11, 0, true);
            t.section = style (F::barlowCondensedBold, 14, 1.5f, true, 9);
            t.laneName = style (F::barlowCondensedBold, 15, 1, true, 10);
            t.laneSub = style (F::spaceMonoRegular, 9, 0, false, 7);
            t.laneButton = style (F::barlowCondensedBold, 13, 2, true);
            t.laneChip = style (F::barlowCondensedBold, 12, 2);
            t.abTitle = style (F::spaceMonoRegular, 10, 0, true, 8);
            t.abSide = style (F::barlowCondensedSemiBold, 16, 1, true, 12);
            t.abSideOn = style (F::barlowCondensedExtraBold, 16, 1, true, 12);
            t.abRow = style (F::barlowCondensedBold, 13, 1, true, 10);
            t.fxName = style (F::barlowCondensedBold, 13, 1.5f, true);
            t.fxValue = style (F::spaceMonoRegular, 10);
            t.stripLabel = style (F::spaceMonoRegular, 10, 0, true);
            t.stripChip = style (F::barlowCondensedBold, 14, 1.5f, true);
            t.stripChipOff = style (F::barlowCondensedSemiBold, 14, 1.5f, true);
            t.stripSegment = style (F::barlowCondensedSemiBold, 14, 1.5f, true);
            t.stripSegmentOn = style (F::barlowCondensedExtraBold, 14, 1.5f, true);
            t.body = style (F::barlowCondensedSemiBold, 14);
            t.bodyStrong = style (F::barlowCondensedBold, 14);
            t.path = style (F::spaceMonoRegular, 11);

            auto& b = s.shape;
            b.panel = boxStyle (4);
            b.panel.gradient = true;
            b.lane = boxStyle (4);
            b.lane.gradient = true;
            b.well = boxStyle (4);
            b.drawer = boxStyle (4);
            for (auto* box : { &b.pill, &b.pillSelected, &b.headerButton })
                *box = boxStyle (3);
            for (auto* box : { &b.segmentTrack, &b.segment, &b.abSide, &b.laneButton, &b.laneChip, &b.stripChip, &b.stripSwitch, &b.section })
                *box = boxStyle (2);
            b.pillSelected.gradient = true;
            b.pillSelected.underline = true; // a molten bar under the selected pill
            b.laneChip.dashed = true;
            b.cell = boxStyle (2, 0);
            b.knob = 58.0f;
            b.playButton = 64.0f;

            s.words = { "WUBANSIA", "DRAG MIDI", "PART", "MUTE", "ON", "OFF" };
            s.display = DisplayKind::scope;
            s.lane = LaneKind::stepsAndSlide;
            s.fx = FxKind::knobs;
            s.texture = Texture::furnace;
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
        static const Skin missionHud = makeMissionHud();
        static const Skin deepGlass = makeDeepGlass();
        static const Skin machineRoom = makeMachineRoom();
        switch (id)
        {
            case SkinId::missionHud: return missionHud;
            case SkinId::deepGlass: return deepGlass;
            case SkinId::machineRoom: return machineRoom;
            case SkinId::nightBus:
                break;
        }
        return nightBus;
    }

    const Skin& skinForKit (const juce::String& kitId) noexcept
    {
        if (kitId == "FUT02")
            return skinFor (SkinId::missionHud);
        if (kitId == "FUT03")
            return skinFor (SkinId::deepGlass);
        if (kitId == "FUT04")
            return skinFor (SkinId::machineRoom);
        return skinFor (SkinId::nightBus); // FUT01, and any kit without a look of its own
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
