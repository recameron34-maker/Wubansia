#include "editor/skin/Paint.h"

#include "editor/DrawnText.h"

#include <cmath>

namespace wubansia::editor::paint
{
    juce::Path boxPath (juce::Rectangle<float> area, const BoxStyle& style)
    {
        juce::Path p;
        if (style.chamfer > 0.0f)
        {
            const auto c = std::min (style.chamfer, std::min (area.getWidth(), area.getHeight()) * 0.5f);
            p.startNewSubPath (area.getX() + c, area.getY());
            p.lineTo (area.getRight(), area.getY());
            p.lineTo (area.getRight(), area.getBottom() - c);
            p.lineTo (area.getRight() - c, area.getBottom());
            p.lineTo (area.getX(), area.getBottom());
            p.lineTo (area.getX(), area.getY() + c);
            p.closeSubPath();
            return p;
        }
        const auto corner = std::min (style.corner, std::min (area.getWidth(), area.getHeight()) * 0.5f);
        if (corner <= 0.0f)
            p.addRectangle (area);
        else
            p.addRoundedRectangle (area, corner);
        return p;
    }

    void box (juce::Graphics& g, juce::Rectangle<float> area, const BoxStyle& style, juce::Colour fill, juce::Colour edge)
    {
        if (area.isEmpty())
            return;
        const auto path = boxPath (area, style);
        if (! fill.isTransparent())
        {
            if (style.gradient)
                g.setGradientFill (juce::ColourGradient::vertical (fill.brighter (0.06f), area.getY(), fill.darker (0.25f), area.getBottom()));
            else
                g.setColour (fill);
            g.fillPath (path);
        }
        if (style.insetHighlight)
        {
            juce::Graphics::ScopedSaveState state (g);
            g.reduceClipRegion (path);
            g.setGradientFill (juce::ColourGradient::vertical (juce::Colours::white.withAlpha (0.07f), area.getY(), juce::Colours::transparentWhite,
                area.getY() + std::min (14.0f, area.getHeight() * 0.4f)));
            g.fillRect (area);
        }
        if (style.border > 0.0f && ! edge.isTransparent())
        {
            const auto inner = boxPath (area.reduced (style.border * 0.5f), style);
            g.setColour (edge);
            if (style.dashed)
            {
                juce::Path dashed;
                const float dashes[] = { 3.0f, 2.5f };
                juce::PathStrokeType (style.border).createDashedStroke (dashed, inner, dashes, 2);
                g.fillPath (dashed);
            }
            else
                g.strokePath (inner, juce::PathStrokeType (style.border));
        }
        if (style.underline)
        {
            g.setColour (edge);
            g.fillRect (area.withTop (area.getBottom() - 3.0f));
        }
    }

    void glow (juce::Graphics& g, juce::Rectangle<float> area, float corner, juce::Colour colour, float radius)
    {
        for (int i = 3; i >= 1; --i)
        {
            const auto grow = radius * (float) i / 3.0f;
            g.setColour (colour.withMultipliedAlpha (0.10f));
            g.fillRoundedRectangle (area.expanded (grow), corner + grow);
        }
    }

    void focusRing (juce::Graphics& g, const Skin& skin, juce::Rectangle<float> boxArea, float corner)
    {
        const auto w = skin.shape.focusWidth;
        const auto ring = boxArea.expanded (skin.shape.focusGap + w * 0.5f);
        g.setColour (skin.colours.focus);
        BoxStyle style;
        style.corner = corner > 0.0f ? corner + skin.shape.focusGap : 0.0f;
        g.strokePath (boxPath (ring, style), juce::PathStrokeType (w));
    }

    void dot (juce::Graphics& g, juce::Point<float> centre, float radius, juce::Colour colour, bool lit)
    {
        const auto area = juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre);
        if (lit)
        {
            g.setColour (colour.withAlpha (0.25f));
            g.fillEllipse (area.expanded (radius * 0.8f));
            g.setColour (colour);
        }
        else
            g.setColour (colour.withMultipliedAlpha (0.28f));
        g.fillEllipse (area);
    }

    void meter (juce::Graphics& g, juce::Rectangle<float> area, int segments, float gap, float fill, float hold, juce::Colour lit,
        juce::Colour off, bool vertical, int hotFrom, juce::Colour hot)
    {
        if (segments <= 0 || area.isEmpty())
            return;
        const auto length = vertical ? area.getHeight() : area.getWidth();
        const auto size = (length - gap * (float) (segments - 1)) / (float) segments;
        const auto litCount = juce::roundToInt (juce::jlimit (0.0f, 1.0f, fill) * (float) segments);
        const auto holdIndex = hold < 0.0f ? -1 : juce::jlimit (0, segments - 1, (int) std::ceil (hold * (float) segments) - 1);
        for (int i = 0; i < segments; ++i)
        {
            const auto offset = (size + gap) * (float) i;
            const auto cell = vertical ? juce::Rectangle<float> (area.getX(), area.getBottom() - offset - size, area.getWidth(), size)
                                       : juce::Rectangle<float> (area.getX() + offset, area.getY(), size, area.getHeight());
            const auto isHot = hotFrom >= 0 && i >= hotFrom;
            const auto on = i < litCount || i == holdIndex;
            g.setColour (on ? (isHot && ! hot.isTransparent() ? hot : lit) : off);
            g.fillRect (cell);
        }
    }

    namespace
    {
        constexpr float pi = juce::MathConstants<float>::pi;
    }

    void knob (juce::Graphics& g, const Skin& skin, juce::Rectangle<float> area, float value, juce::Colour colour, bool enabled)
    {
        const auto& c = skin.colours;
        const auto size = std::min (area.getWidth(), area.getHeight());
        const auto bounds = area.withSizeKeepingCentre (size, size);
        const auto centre = bounds.getCentre();
        const auto radius = size * 0.5f - 3.0f;
        value = juce::jlimit (0.0f, 1.0f, value);
        const auto lit = enabled ? colour : c.faint;

        // A 270 degree arc from 7.5 o'clock.
        const auto start = pi * 1.25f, end = pi * 2.75f;
        juce::Path track;
        track.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, start, end, true);
        g.setColour (c.controlEdge);
        g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        juce::Path arc;
        arc.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, start, start + (end - start) * value, true);
        g.setColour (lit);
        g.strokePath (arc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // The body and its cap.
        const auto body = bounds.reduced (size * 0.2f);
        g.setColour (c.well);
        g.fillEllipse (body);
        g.setColour (c.controlEdge);
        g.drawEllipse (body, 1.0f);
        const auto cap = body.reduced (body.getWidth() * 0.12f);
        g.setGradientFill (juce::ColourGradient::vertical (c.faderCap, cap.getY(), c.faderCap.darker (0.35f), cap.getBottom()));
        g.fillEllipse (cap);
    }

    void fader (juce::Graphics& g, const Skin& skin, juce::Rectangle<float> area, float value, juce::Colour colour, bool vertical, bool enabled)
    {
        const auto& c = skin.colours;
        value = juce::jlimit (0.0f, 1.0f, value);
        const auto track = vertical ? area.withSizeKeepingCentre (std::min (24.0f, area.getWidth()), area.getHeight())
                                    : area.withSizeKeepingCentre (area.getWidth(), std::min (10.0f, area.getHeight()));
        g.setColour (c.well);
        g.fillRoundedRectangle (track, 5.0f);
        g.setColour (c.faderTrack);
        g.drawRoundedRectangle (track.reduced (0.5f), 5.0f, 1.0f);
        juce::ignoreUnused (colour);
        const auto capSize = 18.0f;
        if (vertical)
        {
            const auto y = track.getBottom() - capSize * 0.5f - (track.getHeight() - capSize) * value;
            const auto cap = juce::Rectangle<float> (track.getWidth() - 4.0f, capSize).withCentre ({ track.getCentreX(), y });
            g.setColour (juce::Colours::black.withAlpha (0.4f));
            g.fillRoundedRectangle (cap.translated (0.0f, 1.5f), 3.0f);
            g.setColour (enabled ? c.faderCap : c.faint);
            g.fillRoundedRectangle (cap, 3.0f);
            g.setColour (c.well.withAlpha (0.6f));
            g.fillRect (cap.withSizeKeepingCentre (cap.getWidth() - 8.0f, 1.0f));
        }
        else
        {
            const auto x = track.getX() + capSize * 0.5f + (track.getWidth() - capSize) * value;
            const auto cap = juce::Rectangle<float> (capSize * 0.34f, area.getHeight() - 4.0f).withCentre ({ x, track.getCentreY() });
            g.setColour (enabled ? c.faderCap : c.faint);
            g.fillRoundedRectangle (cap, 1.5f);
        }
    }

    void label (juce::Graphics& g, const Skin& skin, const juce::String& text, juce::Rectangle<float> area, juce::Justification j,
        float, juce::Colour colour)
    {
        drawText (g, fitText (skin.type.rowLabel, text, area, j), colour.isTransparent() ? skin.colours.label : colour);
    }

    void well (juce::Graphics& g, const Skin& skin, juce::Rectangle<float> area, float corner)
    {
        auto style = skin.shape.well;
        style.corner = std::min (style.corner, corner);
        box (g, area, style, over (skin.colours.well, skin.colours.face), skin.colours.wellEdge);
    }

    void drawer (juce::Graphics& g, const Skin& skin, juce::Rectangle<float> area)
    {
        const auto& c = skin.colours;
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (area.translated (0.0f, 3.0f), skin.shape.drawer.corner);
        box (g, area, skin.shape.drawer, over (c.panel, c.face), c.panelEdge);
    }
}
