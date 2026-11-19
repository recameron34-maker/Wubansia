#pragma once

// The looks' shared painters (ADR 0032): free functions that draw boxes, rings, meters, knobs and faders the way the
// skin says. Most of what tells the four looks apart is data (corners, borders, fills, type); the genuinely different
// drawings switch on the skin's kinds here and in the display and lane painters.

#include "editor/skin/Skin.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace wubansia::editor::paint
{
    // The outline of a box in its style: a capsule when the corner reaches half the height, chamfered corners where
    // the style cuts them.
    juce::Path boxPath (juce::Rectangle<float> area, const BoxStyle& style);

    // A box: its fill, its edge (dashed where the style says) and the look's inset highlight along the top.
    void box (juce::Graphics&, juce::Rectangle<float> area, const BoxStyle& style, juce::Colour fill, juce::Colour edge);

    // A soft glow around a box: two or three alpha fills, never an image effect.
    void glow (juce::Graphics&, juce::Rectangle<float> area, float corner, juce::Colour colour, float radius);

    // The focus ring: the look's focus colour, focusWidth px, focusGap px outside the box, with the box's corner.
    void focusRing (juce::Graphics&, const Skin&, juce::Rectangle<float> box, float corner);

    // A round light: off is a dark lens, on is lit with a glow.
    void dot (juce::Graphics&, juce::Point<float> centre, float radius, juce::Colour colour, bool lit);

    // A segmented level meter, bottom up (or left to right): fill and hold are 0..1 of the scale, hold < 0 draws none.
    void meter (juce::Graphics&, juce::Rectangle<float> area, int segments, float gap, float fill, float hold, juce::Colour lit,
        juce::Colour off, bool vertical, int hotFrom = -1, juce::Colour hot = {});

    // A knob in the look's kind: value is 0..1 of its range, colour lights its ring.
    void knob (juce::Graphics&, const Skin&, juce::Rectangle<float> area, float value, juce::Colour colour, bool enabled);

    // A fader: the track and the cap at value (0..1), vertical (bottom up) or horizontal (left to right).
    void fader (juce::Graphics&, const Skin&, juce::Rectangle<float> area, float value, juce::Colour colour, bool vertical, bool enabled);

    // A small label in the look's row-label style (the drawers' headings); height is the old engraved size, unused.
    void label (juce::Graphics&, const Skin&, const juce::String& text, juce::Rectangle<float> area, juce::Justification,
        float height = 0.0f, juce::Colour colour = {});

    // A recessed well in the look (the drawers' lists).
    void well (juce::Graphics&, const Skin&, juce::Rectangle<float> area, float corner);

    // The drawers' and the content panel's frame.
    void drawer (juce::Graphics&, const Skin&, juce::Rectangle<float> area);

    // A colour at a fraction of its alpha.
    inline juce::Colour alpha (juce::Colour c, float a) { return c.withMultipliedAlpha (a); }
}
