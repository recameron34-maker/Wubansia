#include "editor/DrawnText.h"

namespace wubansia::editor
{
    DrawnText fitText (const TextStyle&, const juce::String& text, juce::Rectangle<float> area, juce::Justification j)
    {
        DrawnText t;
        t.text = text;
        t.area = area;
        t.justification = j;
        return t;
    }

    void drawText (juce::Graphics&, const DrawnText&, juce::Colour) {}
}
