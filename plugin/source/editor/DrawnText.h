#pragma once

// What a component draws in text (ADR 0032): each text with the font it was drawn in after fitting and the area it
// was fitted to. A component's paint () draws exactly its drawnText () list, so the tests that walk the lists check
// what the person sees: every label reads in full, in a face that has its characters.

#include "editor/skin/Skin.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <vector>

namespace wubansia::editor
{
    struct DrawnText
    {
        juce::String text; // as drawn (capitals applied)
        juce::Font font { juce::FontOptions() }; // after fitting
        juce::Rectangle<float> area; // in the component's coordinates
        juce::Justification justification = juce::Justification::centredLeft;
        float styledPx = 0.0f; // the style's size; the font's is smaller when it shrank
        float minPx = 0.0f; // the style's floor
        bool cut = false; // still too wide at minPx: drawn cut with "…"
    };

    struct DrawnTextSource
    {
        virtual ~DrawnTextSource() = default;
        virtual std::vector<DrawnText> drawnText() const = 0; // what paint () draws, in order
    };

    // The style's text (capitals applied) in its font at its size, else smaller in 0.5 px steps down to minPx, else
    // minPx cut with "…".
    DrawnText fitText (const TextStyle&, const juce::String& text, juce::Rectangle<float> area,
        juce::Justification = juce::Justification::centredLeft);

    // Draws one in colour.
    void drawText (juce::Graphics&, const DrawnText&, juce::Colour);
}
