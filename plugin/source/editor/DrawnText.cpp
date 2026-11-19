#include "editor/DrawnText.h"

namespace wubansia::editor
{
    namespace
    {
        const juce::String ellipsis (juce::CharPointer_UTF8 ("\xe2\x80\xa6"));

        float widthOf (const juce::Font& font, const juce::String& text) { return juce::GlyphArrangement::getStringWidth (font, text); }
    }

    DrawnText fitText (const TextStyle& style, const juce::String& text, juce::Rectangle<float> area, juce::Justification justification)
    {
        DrawnText t;
        t.text = style.apply (text);
        t.area = area;
        t.justification = justification;
        t.styledPx = style.px;
        t.minPx = std::min (style.minPx > 0.0f ? style.minPx : style.px, style.px);
        auto size = style.px;
        t.font = style.fontAt (size);
        while (widthOf (t.font, t.text) > area.getWidth() + 0.01f && size - 0.5f >= t.minPx - 0.001f)
        {
            size -= 0.5f;
            t.font = style.fontAt (size);
        }
        if (widthOf (t.font, t.text) > area.getWidth() + 0.01f)
        {
            // Still too wide at the floor: cut, keeping as much as fits before an ellipsis.
            t.cut = true;
            auto shown = t.text;
            while (shown.isNotEmpty() && widthOf (t.font, shown.trimEnd() + ellipsis) > area.getWidth())
                shown = shown.dropLastCharacters (1);
            t.text = shown.trimEnd() + ellipsis;
        }
        return t;
    }

    void drawText (juce::Graphics& g, const DrawnText& t, juce::Colour colour)
    {
        if (t.text.isEmpty())
            return;
        g.setColour (colour);
        g.setFont (t.font);
        // Single line, no squashing: what was fitted is what is drawn (a little room for antialiasing at the ends).
        g.drawText (t.text, t.area.expanded (1.0f, 0.0f), t.justification, false);
    }
}
