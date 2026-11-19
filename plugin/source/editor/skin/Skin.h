#pragma once

// The editor's looks (ADR 0032): one immutable Skin per kit, data the shared painters (Paint.h) read. FUT01 wears
// night bus, FUT02 mission HUD, FUT03 deep glass and FUT04 machine room; any other kit night bus. The editor's
// LookAndFeel holds the current one and swaps it on the message thread when the kit changes; nothing on the audio
// thread reads a skin.

#include "editor/skin/Fonts.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <cstdint>
#include <utility>
#include <vector>

namespace wubansia::editor
{
    enum class SkinId : std::uint8_t { nightBus, missionHud, deepGlass, machineRoom };
    enum class DisplayKind : std::uint8_t { stepGrid, orbit, current, scope };
    enum class LaneKind : std::uint8_t { faderAndMeter, telemetryColumn, glassPill, stepsAndSlide };
    enum class FxKind : std::uint8_t { knobs, bars };
    enum class Texture : std::uint8_t { plain, grid, water, furnace };

    struct TextStyle
    {
        FaceId face = FaceId::archivoRegular;
        float px = 12.0f; // the em size in logical px (FontOptions::withPointHeight): CSS's px
        float letterSpacingPx = 0.0f; // after every character, as CSS letter-spacing
        bool upper = false; // drawn in capitals; titles, tooltips and chord names keep their case
        bool lower = false; // drawn in small letters (deep glass's second lines)
        float minPx = 0.0f; // fitText shrinks to this, in 0.5 px steps, before it cuts; 0: px
        bool fallback = false; // strings the run time builds (paths, messages): the platform's fonts may fill in

        juce::Font font() const; // fontFor (face, px, letterSpacingPx, fallback)
        juce::Font fontAt (float size) const; // the same at another size, the spacing kept in px
        juce::String apply (const juce::String& text) const { return upper ? text.toUpperCase() : lower ? text.toLowerCase() : text; }
    };

    struct BoxStyle
    {
        float corner = 6.0f; // >= height / 2 draws a capsule
        float chamfer = 0.0f; // cut from the top-left and bottom-right corners
        float border = 1.0f;
        bool dashed = false, gradient = false, insetHighlight = false, underline = false;
        float glow = 0.0f; // px of glow when lit; 0: none
    };

    struct Palette
    {
        juce::Colour face, faceGlow, texture;
        juce::Colour panel, panelEdge, panelHighlight, lane, laneEdge, laneHighlight, well, wellEdge, grid;
        juce::Colour text, dim, faint, label, laneNumber;
        juce::Colour accent, accentInk, accentTint, accentTintText;
        juce::Colour accent2, accent2Ink, accent2Tint, accent2TintText;
        juce::Colour drums, music, hookLane; // hookLane: the lead lane's colour where a look marks it (mission HUD)
        juce::Colour controlEdge, faderTrack, faderCap;
        juce::Colour cellOff, cellOffEdge, cellHeld, meterOff;
        juce::Colour sectionOff, sectionOffText, sectionLit, sectionLitText;
        juce::Colour pillOff, pillOffEdge, pillOffText, pillOn, pillOnEdge, pillOnText;
        juce::Colour busy, warning, focus;
    };

    struct Type
    {
        TextStyle wordmark, subtitle, pill, pillSelected, headerButton, displayLine, displayLineStrong, displayLabel,
            transportLine, playMode, playModeOff, rowLabel, rowValue, section, laneName, laneSub, laneButton, laneChip,
            abTitle, abSide, abSideOn, abRow, fxName, fxValue, stripLabel, stripChip, stripChipOff, stripSegment,
            stripSegmentOn, body, bodyStrong, path;
    };

    struct Shape
    {
        BoxStyle panel, lane, well, pill, pillSelected, headerButton, segmentTrack, segment, abSide, laneButton, laneChip,
            stripChip, stripSwitch, section, cell, drawer;
        float knob = 56.0f, playButton = 56.0f, focusGap = 2.0f, focusWidth = 2.0f;
    };

    struct Words
    {
        juce::String wordmark, stripLabel, partLabel, mute, on, off;
    };

    struct Skin
    {
        SkinId id = SkinId::nightBus;
        juce::String name; // "Night bus", "Mission HUD", "Deep glass", "Machine room"
        Palette colours;
        Type type;
        Shape shape;
        Words words;
        DisplayKind display = DisplayKind::stepGrid;
        LaneKind lane = LaneKind::faderAndMeter;
        FxKind fx = FxKind::knobs;
        Texture texture = Texture::plain;
    };

    // The look's body face at a drawer's own size, in the units the drawers were laid out in: juceHeight is a JUCE font
    // height (ascent + descent), made into the em size juceHeight / 1.117 (Liberation Sans' (ascent + descent) / em),
    // with JUCE's kerning factor as letter spacing in px. type.bodyStrong's face when strong, else type.body's; the
    // body style's fallback.
    juce::Font bodyFont (const Skin&, float juceHeight, bool strong = false, float kerningFactor = 0.0f);
    // The same in the path role's face (the person's folder names): fallback on.
    juce::Font pathFont (const Skin&, float juceHeight);

    const Skin& skinFor (SkinId) noexcept; // function-local statics: made once, never changed
    const Skin& skinForKit (const juce::String& kitId) noexcept; // FUT01-FUT04 their own, anything else night bus
    const Skin& skinOf (const juce::Component&) noexcept; // its editor::LookAndFeel's, else night bus

    // Every text style of a look with its role's name ("wordmark", "laneName" ...), in Type's order: what the tests
    // walk.
    std::vector<std::pair<juce::String, const TextStyle*>> stylesOf (const Skin&);

    // WCAG 2.x contrast of two opaque colours (the tests' and a debug assert's).
    float contrastRatio (juce::Colour a, juce::Colour b) noexcept;
    // a over b: a translucent token composited on its background.
    juce::Colour over (juce::Colour top, juce::Colour background) noexcept;
}
