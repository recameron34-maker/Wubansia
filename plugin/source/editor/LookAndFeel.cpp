#include "LookAndFeel.h"

#include <cmath>

namespace wubansia::editor
{
    juce::Colour accentForKit (const juce::String& kitId)
    {
        if (kitId == "FUT01")
            return juce::Colour (0xffb67bff); // UK garage: chrome violet
        if (kitId == "FUT02")
            return juce::Colour (0xff35e2ff); // sci-fi club: laser cyan
        if (kitId == "FUT03")
            return juce::Colour (0xffffad42); // deep tech house: warm amber
        if (kitId == "FUT04")
            return juce::Colour (0xffff4255); // dark techno: furnace red
        if (kitId.isEmpty())
            return juce::Colour (0xff8fd3c8);

        const auto hue = (float) (static_cast<juce::uint32> (kitId.hashCode()) % 360u) / 360.0f;
        return juce::Colour::fromHSV (hue, 0.62f, 1.0f, 1.0f);
    }

    juce::Font uiFont (float height, bool bold, float tracking)
    {
        auto options = juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain);
        if (! juce::exactlyEqual (tracking, 0.0f))
            options = options.withKerningFactor (tracking);
        return juce::Font (options);
    }

    juce::Font monoFont (float height, bool bold)
    {
        return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), height, bold ? juce::Font::bold : juce::Font::plain));
    }

    namespace draw
    {
        void faceplate (juce::Graphics& g, juce::Rectangle<float> area)
        {
            g.setGradientFill (juce::ColourGradient::vertical (colours::panelTop, area.getY(), colours::panelBottom, area.getBottom()));
            g.fillRect (area);

            // Brushed grain: hairlines of slightly varying brightness, the same on every run.
            juce::Random grain (0x5eed);
            for (auto y = area.getY(); y < area.getBottom(); y += 1.0f)
            {
                const auto alpha = grain.nextFloat() * 0.032f;
                g.setColour ((grain.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (alpha));
                g.fillRect (area.getX(), y, area.getWidth(), 0.6f);
            }

            // A soft vignette, and a faint sheen across the top.
            const auto centre = area.getCentre();
            g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack, centre.x, centre.y,
                juce::Colours::black.withAlpha (0.38f), area.getX(), area.getY(), true));
            g.fillRect (area);
            g.setGradientFill (juce::ColourGradient::vertical (juce::Colours::white.withAlpha (0.045f), area.getY(),
                juce::Colours::transparentWhite, area.getY() + area.getHeight() * 0.25f));
            g.fillRect (area);

            const auto inset = 9.0f;
            screw (g, { area.getX() + inset, area.getY() + inset }, 4.2f, 0.6f);
            screw (g, { area.getRight() - inset, area.getY() + inset }, 4.2f, 2.1f);
            screw (g, { area.getX() + inset, area.getBottom() - inset }, 4.2f, 1.2f);
            screw (g, { area.getRight() - inset, area.getBottom() - inset }, 4.2f, 2.7f);
        }

        void plate (juce::Graphics& g, juce::Rectangle<float> area, const juce::String& title)
        {
            constexpr auto corner = 7.0f;

            g.setColour (juce::Colours::black.withAlpha (0.45f));
            g.fillRoundedRectangle (area.translated (0.0f, 1.5f).expanded (0.5f), corner + 0.5f);

            g.setGradientFill (juce::ColourGradient::vertical (colours::plateTop, area.getY(), colours::plateBottom, area.getBottom()));
            g.fillRoundedRectangle (area, corner);

            // The bevel: light along the top edge, fading out by the middle.
            g.setGradientFill (juce::ColourGradient::vertical (juce::Colours::white.withAlpha (0.10f), area.getY(),
                juce::Colours::white.withAlpha (0.0f), area.getCentreY()));
            g.drawRoundedRectangle (area.reduced (0.5f), corner, 1.0f);
            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.drawRoundedRectangle (area.expanded (0.5f), corner + 0.5f, 1.0f);

            if (title.isNotEmpty())
                engraved (g, title, area.reduced (12.0f, 7.0f).removeFromTop (12.0f), juce::Justification::centredLeft, 9.5f);
        }

        void recess (juce::Graphics& g, juce::Rectangle<float> area, float corner)
        {
            g.setColour (colours::recess);
            g.fillRoundedRectangle (area, corner);

            juce::Path outline;
            outline.addRoundedRectangle (area, corner);
            juce::Graphics::ScopedSaveState state (g);
            g.reduceClipRegion (outline);
            g.setGradientFill (juce::ColourGradient::vertical (juce::Colours::black.withAlpha (0.7f), area.getY(),
                juce::Colours::black.withAlpha (0.0f), area.getY() + juce::jmin (8.0f, area.getHeight() * 0.5f)));
            g.fillRect (area);
            g.setGradientFill (juce::ColourGradient::vertical (juce::Colours::white.withAlpha (0.0f), area.getCentreY(),
                juce::Colours::white.withAlpha (0.09f), area.getBottom()));
            g.drawRoundedRectangle (area.reduced (0.5f), corner, 1.0f);
        }

        void screw (juce::Graphics& g, juce::Point<float> centre, float radius, float angle)
        {
            g.setColour (juce::Colours::black.withAlpha (0.65f));
            g.fillEllipse (juce::Rectangle<float> (radius * 2.0f + 2.0f, radius * 2.0f + 2.0f).withCentre (centre.translated (0.0f, 0.6f)));

            g.setGradientFill (juce::ColourGradient (juce::Colour (0xffa2a8b1), centre.x - radius * 0.5f, centre.y - radius * 0.6f,
                juce::Colour (0xff353940), centre.x + radius * 0.7f, centre.y + radius * 0.8f, true));
            g.fillEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre));

            // A cross head.
            for (const auto turn : { 0.0f, juce::MathConstants<float>::halfPi })
            {
                juce::Path slot;
                slot.addRoundedRectangle (-radius * 0.72f, -radius * 0.13f, radius * 1.44f, radius * 0.26f, radius * 0.1f);
                g.setColour (juce::Colour (0xff17191d));
                g.fillPath (slot, juce::AffineTransform::rotation (angle + turn).translated (centre));
            }
        }

        void engraved (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area,
            juce::Justification justification, float height, juce::Colour colour)
        {
            const auto upper = text.toUpperCase();
            g.setFont (uiFont (height, true, 0.12f));
            g.setColour (juce::Colours::black.withAlpha (0.6f));
            g.drawText (upper, area.translated (0.0f, 1.0f), justification, false);
            g.setColour (colour);
            g.drawText (upper, area, justification, false);
        }

        void led (juce::Graphics& g, juce::Point<float> centre, float radius, juce::Colour colour, float level)
        {
            level = juce::jlimit (0.0f, 1.0f, level);
            const auto circle = [&centre] (float r) { return juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (centre); };

            if (level > 0.01f)
            {
                const auto glowRadius = radius * 3.4f;
                g.setGradientFill (juce::ColourGradient (colour.withAlpha (0.42f * level), centre.x, centre.y,
                    colour.withAlpha (0.0f), centre.x + glowRadius, centre.y, true));
                g.fillEllipse (circle (glowRadius));
            }

            g.setColour (juce::Colours::black.withAlpha (0.75f));
            g.fillEllipse (circle (radius + 1.3f));
            g.setColour (juce::Colour (0xff30343b));
            g.drawEllipse (circle (radius + 0.7f), 0.7f);

            const auto off = colour.withMultipliedBrightness (0.2f).withMultipliedSaturation (0.75f);
            const auto body = off.interpolatedWith (colour.brighter (0.2f), level);
            g.setGradientFill (juce::ColourGradient (body.brighter (0.15f + 0.6f * level), centre.x - radius * 0.3f, centre.y - radius * 0.35f,
                body.darker (0.5f), centre.x + radius * 0.8f, centre.y + radius * 0.9f, true));
            g.fillEllipse (circle (radius));

            if (level > 0.25f)
            {
                g.setColour (juce::Colours::white.withAlpha (0.6f * level));
                g.fillEllipse (juce::Rectangle<float> (radius * 0.8f, radius * 0.8f).withCentre (centre.translated (-radius * 0.12f, -radius * 0.15f)));
            }

            g.setColour (juce::Colours::white.withAlpha (0.22f));
            g.fillEllipse (juce::Rectangle<float> (radius * 0.75f, radius * 0.45f).withCentre (centre.translated (-radius * 0.3f, -radius * 0.5f)));
        }

        void meter (juce::Graphics& g, juce::Rectangle<float> area, int segments, float fill, float hold,
            juce::Colour lit, bool vertical, int warmFrom, int hotFrom)
        {
            if (segments <= 0)
                return;

            constexpr auto gap = 1.5f;
            const auto length = vertical ? area.getHeight() : area.getWidth();
            const auto segment = (length - gap * (float) (segments - 1)) / (float) segments;
            const auto holdIndex = hold >= 0.0f ? juce::jlimit (0, segments - 1, (int) std::floor (hold * (float) segments - 0.001f)) : -1;

            for (int i = 0; i < segments; ++i)
            {
                const auto offset = (float) i * (segment + gap);
                const auto rect = vertical ? juce::Rectangle<float> (area.getX(), area.getBottom() - offset - segment, area.getWidth(), segment)
                                           : juce::Rectangle<float> (area.getX() + offset, area.getY(), segment, area.getHeight());

                const auto colour = (hotFrom >= 0 && i >= hotFrom) ? colours::hot
                                  : (warmFrom >= 0 && i >= warmFrom) ? lit.interpolatedWith (juce::Colours::white, 0.5f)
                                                                     : lit;
                const auto start = (float) i / (float) segments;
                auto level = juce::jlimit (0.0f, 1.0f, (fill - start) * (float) segments);
                if (i == holdIndex && fill < start + 1.0f / (float) segments)
                    level = juce::jmax (level, 0.85f);

                g.setColour (colour.withAlpha (0.08f));
                g.fillRoundedRectangle (rect, 1.0f);
                if (level > 0.0f)
                {
                    g.setColour (colour.withAlpha (0.22f * level));
                    g.fillRoundedRectangle (rect.expanded (1.2f), 2.0f);
                    g.setColour (colour.withAlpha (0.25f + 0.75f * level));
                    g.fillRoundedRectangle (rect, 1.0f);
                }
            }
        }

        namespace
        {
            // Segments a..g in bits 0..6.
            int segmentsFor (juce::juce_wchar c)
            {
                switch (c)
                {
                    case '0': case 'O': return 0x3f;
                    case '1': return 0x06;
                    case '2': return 0x5b;
                    case '3': return 0x4f;
                    case '4': return 0x66;
                    case '5': case 'S': return 0x6d;
                    case '6': return 0x7d;
                    case '7': return 0x07;
                    case '8': return 0x7f;
                    case '9': return 0x6f;
                    case '-': return 0x40;
                    case 'A': return 0x77;
                    case 'b': return 0x7c;
                    case 'C': return 0x39;
                    case 'd': return 0x5e;
                    case 'E': return 0x79;
                    case 'F': return 0x71;
                    case 'H': return 0x76;
                    case 'L': return 0x38;
                    case 'n': return 0x54;
                    case 'o': return 0x5c;
                    case 'P': return 0x73;
                    case 'r': return 0x50;
                    case 't': return 0x78;
                    case 'U': return 0x3e;
                    default: return 0;
                }
            }

            juce::Path segmentPath (int index, float w, float h, float t)
            {
                const auto half = t * 0.5f;
                const auto gap = t * 0.16f;
                const auto left = half, right = w - half, top = half, middle = h * 0.5f, bottom = h - half;

                juce::Path p;
                const auto horizontal = [&] (float y)
                {
                    const auto x0 = left + gap, x1 = right - gap;
                    p.startNewSubPath (x0, y);
                    p.lineTo (x0 + half, y - half);
                    p.lineTo (x1 - half, y - half);
                    p.lineTo (x1, y);
                    p.lineTo (x1 - half, y + half);
                    p.lineTo (x0 + half, y + half);
                    p.closeSubPath();
                };
                const auto vertical = [&] (float x, float y0, float y1)
                {
                    y0 += gap;
                    y1 -= gap;
                    p.startNewSubPath (x, y0);
                    p.lineTo (x + half, y0 + half);
                    p.lineTo (x + half, y1 - half);
                    p.lineTo (x, y1);
                    p.lineTo (x - half, y1 - half);
                    p.lineTo (x - half, y0 + half);
                    p.closeSubPath();
                };

                switch (index)
                {
                    case 0: horizontal (top); break;
                    case 1: vertical (right, top, middle); break;
                    case 2: vertical (right, middle, bottom); break;
                    case 3: horizontal (bottom); break;
                    case 4: vertical (left, middle, bottom); break;
                    case 5: vertical (left, top, middle); break;
                    default: horizontal (middle); break;
                }
                return p;
            }
        }

        void sevenSegment (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area, juce::Colour lit, float ghostAlpha)
        {
            struct Cell
            {
                int segments = 0;
                bool point = false;
            };
            juce::Array<Cell> cells;
            for (auto p = text.getCharPointer(); ! p.isEmpty(); ++p)
            {
                const auto c = *p;
                if (c == '.' && ! cells.isEmpty() && ! cells.getReference (cells.size() - 1).point)
                    cells.getReference (cells.size() - 1).point = true;
                else
                    cells.add ({ segmentsFor (c), false });
            }
            if (cells.isEmpty())
                return;

            // Size the cells to the area's height, shrinking them if the text wouldn't fit its width.
            auto h = area.getHeight();
            auto w = h * 0.52f;
            auto pitch = w + h * 0.2f;
            const auto needed = pitch * (float) cells.size() - h * 0.2f;
            if (needed > area.getWidth())
            {
                const auto scale = area.getWidth() / needed;
                h *= scale;
                w *= scale;
                pitch *= scale;
            }
            const auto t = h * 0.13f;
            const auto slant = 0.09f;
            auto x = area.getRight() - (pitch * (float) cells.size() - h * 0.2f); // right-aligned, like a calculator
            const auto y = area.getCentreY() - h * 0.5f;

            for (const auto& cell : cells)
            {
                const auto transform = juce::AffineTransform::shear (-slant, 0.0f).translated (x + slant * h, y);
                for (int s = 0; s < 7; ++s)
                {
                    const auto path = segmentPath (s, w, h, t);
                    if ((cell.segments & (1 << s)) != 0)
                    {
                        g.setColour (lit.withAlpha (0.16f));
                        g.strokePath (path, juce::PathStrokeType (t * 0.9f, juce::PathStrokeType::curved), transform);
                        g.setColour (lit);
                        g.fillPath (path, transform);
                    }
                    else
                    {
                        g.setColour (lit.withAlpha (ghostAlpha));
                        g.fillPath (path, transform);
                    }
                }

                const auto dot = juce::Rectangle<float> (t * 1.05f, t * 1.05f).withCentre ({ x + w + (pitch - w) * 0.45f, y + h - t * 0.5f });
                g.setColour (cell.point ? lit : lit.withAlpha (ghostAlpha));
                g.fillEllipse (dot);
                x += pitch;
            }
        }

        void midiGlyph (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour)
        {
            // A five-pin connector face.
            const auto size = juce::jmin (area.getWidth(), area.getHeight());
            const auto circle = area.withSizeKeepingCentre (size, size).reduced (size * 0.06f);
            const auto centre = circle.getCentre();
            const auto r = circle.getWidth() * 0.5f;

            g.setColour (colour);
            g.drawEllipse (circle, juce::jmax (1.0f, size * 0.09f));
            g.fillRect (juce::Rectangle<float> (r * 0.42f, r * 0.24f).withCentre ({ centre.x, circle.getY() + r * 0.12f }));

            // Five pins on the lower half, from 3 o'clock round to 9 o'clock.
            for (int pin = 0; pin < 5; ++pin)
            {
                const auto angle = juce::MathConstants<float>::halfPi + (float) pin * juce::MathConstants<float>::pi * 0.25f;
                const auto p = centre.getPointOnCircumference (r * 0.56f, angle);
                g.fillEllipse (juce::Rectangle<float> (r * 0.24f, r * 0.24f).withCentre (p));
            }
        }
    }

    LookAndFeel::LookAndFeel()
        : accentColour (accentForKit ({}))
    {
        setColour (juce::ResizableWindow::backgroundColourId, colours::background);
        setColour (juce::Label::textColourId, colours::text);
        setColour (juce::ComboBox::textColourId, colours::text);
        setColour (juce::ComboBox::backgroundColourId, colours::recess);
        setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff343941));
        setColour (juce::ComboBox::arrowColourId, colours::textDim);
        setColour (juce::PopupMenu::backgroundColourId, colours::plateBottom);
        setColour (juce::PopupMenu::textColourId, colours::text);
        setColour (juce::PopupMenu::highlightedTextColourId, colours::text);
        setColour (juce::TooltipWindow::textColourId, colours::text);
        setColour (juce::TooltipWindow::backgroundColourId, colours::plateBottom);
        setColour (juce::TooltipWindow::outlineColourId, juce::Colour (0xff3a3f48));
        setAccent (accentColour);
    }

    void LookAndFeel::setAccent (juce::Colour colour)
    {
        accentColour = colour;
        setColour (juce::PopupMenu::highlightedBackgroundColourId, colour.withAlpha (0.22f));
        setColour (juce::Slider::thumbColourId, colour);
        setColour (juce::Slider::rotarySliderFillColourId, colour);
    }

    juce::Slider::SliderLayout LookAndFeel::getSliderLayout (juce::Slider& slider)
    {
        juce::Slider::SliderLayout layout;
        const auto bounds = slider.getLocalBounds();
        if (slider.isRotary())
            layout.sliderBounds = bounds;
        else if (slider.isVertical())
            layout.sliderBounds = bounds.reduced (0, getSliderThumbRadius (slider));
        else
            layout.sliderBounds = bounds.reduced (getSliderThumbRadius (slider), 0);
        return layout;
    }

    int LookAndFeel::getSliderThumbRadius (juce::Slider& slider)
    {
        return slider.isRotary() ? 0 : 10;
    }

    void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
        float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider)
    {
        const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (2.0f);
        const auto size = juce::jmin (bounds.getWidth(), bounds.getHeight());
        const auto centre = bounds.getCentre();
        const auto ringRadius = size * 0.5f - 2.5f;
        const auto knobRadius = ringRadius * 0.74f;
        const auto enabled = slider.isEnabled();
        const auto lit = enabled ? accentColour : colours::textFaint;
        const auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        // The ring: a dark groove and the lit arc, with a soft glow.
        juce::Path groove;
        groove.addCentredArc (centre.x, centre.y, ringRadius, ringRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour (juce::Colours::black.withAlpha (0.65f));
        g.strokePath (groove, juce::PathStrokeType (4.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (lit.withAlpha (0.10f));
        g.strokePath (groove, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        if (sliderPos > 0.001f)
        {
            juce::Path arc;
            arc.addCentredArc (centre.x, centre.y, ringRadius, ringRadius, 0.0f, rotaryStartAngle, angle, true);
            g.setColour (lit.withAlpha (0.20f));
            g.strokePath (arc, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (lit);
            g.strokePath (arc, juce::PathStrokeType (2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Scale marks at the ends and the middle.
        for (const auto mark : { rotaryStartAngle, (rotaryStartAngle + rotaryEndAngle) * 0.5f, rotaryEndAngle })
        {
            const auto inner = centre.getPointOnCircumference (ringRadius + 3.5f, mark);
            const auto outer = centre.getPointOnCircumference (ringRadius + 6.0f, mark);
            g.setColour (colours::textFaint);
            g.drawLine ({ inner, outer }, 1.0f);
        }

        // The knob: a shadow, a dark skirt and a machined cap.
        const auto circle = [&centre] (float r) { return juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (centre); };
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillEllipse (circle (knobRadius + 1.5f).translated (0.0f, 2.0f));
        g.setGradientFill (juce::ColourGradient::vertical (juce::Colour (0xff30343b), centre.y - knobRadius, juce::Colour (0xff131518), centre.y + knobRadius));
        g.fillEllipse (circle (knobRadius));

        const auto capRadius = knobRadius * 0.8f;
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff5a606a), centre.x - capRadius * 0.6f, centre.y - capRadius * 0.8f,
            juce::Colour (0xff23262b), centre.x + capRadius * 0.6f, centre.y + capRadius, false));
        g.fillEllipse (circle (capRadius));
        for (auto r = capRadius * 0.3f; r < capRadius; r += capRadius * 0.17f)
        {
            g.setColour (juce::Colours::white.withAlpha (0.035f));
            g.drawEllipse (circle (r), 0.6f);
        }
        g.setGradientFill (juce::ColourGradient::vertical (juce::Colours::white.withAlpha (0.16f), centre.y - capRadius,
            juce::Colours::white.withAlpha (0.0f), centre.y));
        g.drawEllipse (circle (capRadius - 0.5f), 1.0f);

        // The pointer, lit.
        const auto from = centre.getPointOnCircumference (knobRadius * 0.28f, angle);
        const auto to = centre.getPointOnCircumference (knobRadius * 0.9f, angle);
        g.setColour (lit.withAlpha (0.25f));
        g.drawLine ({ from, to }, 5.0f);
        g.setColour (enabled ? lit.brighter (0.3f) : colours::textFaint);
        g.drawLine ({ from, to }, 2.2f);
    }

    void LookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
        float minSliderPos, float maxSliderPos, juce::Slider::SliderStyle style, juce::Slider& slider)
    {
        if (! slider.isVertical())
        {
            LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, slider);
            return;
        }

        const auto area = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height);
        const auto enabled = slider.isEnabled();
        const auto lit = enabled ? accentColour : colours::textFaint;
        const auto cx = area.getCentreX();

        // Scale marks every 6 dB down to -24, then -48; 0 dB marked longer.
        for (const auto db : { 6.0, 0.0, -6.0, -12.0, -18.0, -24.0, -36.0, -48.0, -60.0 })
        {
            if (db < slider.getMinimum() || db > slider.getMaximum())
                continue;
            const auto py = (float) slider.getPositionOfValue (db);
            const auto zero = juce::exactlyEqual (db, 0.0);
            const auto length = zero ? 9.0f : 5.0f;
            g.setColour (zero ? colours::textDim : colours::textFaint);
            g.fillRect (juce::Rectangle<float> (cx - 9.0f - length, py - 0.5f, length, 1.0f));
            g.fillRect (juce::Rectangle<float> (cx + 9.0f, py - 0.5f, length, 1.0f));
        }

        const auto slot = juce::Rectangle<float> (cx - 2.5f, area.getY() - 4.0f, 5.0f, area.getHeight() + 8.0f);
        draw::recess (g, slot, 2.5f);
        const auto filled = slot.withTop (sliderPos).reduced (1.2f, 0.0f).withTrimmedBottom (1.0f);
        g.setColour (lit.withAlpha (enabled ? 0.55f : 0.25f));
        g.fillRoundedRectangle (filled, 1.5f);

        // The cap: brushed metal with grip lines and a lit centre line.
        const auto cap = juce::Rectangle<float> (juce::jmin (34.0f, area.getWidth() - 2.0f), 20.0f).withCentre ({ cx, sliderPos });
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (cap.translated (0.0f, 2.0f).expanded (1.0f), 3.5f);
        g.setGradientFill (juce::ColourGradient::vertical (juce::Colour (0xffd6dae0), cap.getY(), juce::Colour (0xff6f757e), cap.getBottom()));
        g.fillRoundedRectangle (cap, 3.0f);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.drawRoundedRectangle (cap, 3.0f, 0.8f);
        for (const auto dy : { -6.0f, -3.5f, 3.5f, 6.0f })
        {
            g.setColour (juce::Colours::black.withAlpha (0.28f));
            g.fillRect (juce::Rectangle<float> (cap.getWidth() - 10.0f, 0.8f).withCentre ({ cx, sliderPos + dy }));
        }
        g.setColour (lit.withAlpha (0.35f));
        g.fillRect (juce::Rectangle<float> (cap.getWidth() - 4.0f, 4.0f).withCentre ({ cx, sliderPos }));
        g.setColour (enabled ? lit.brighter (0.25f) : colours::textFaint);
        g.fillRect (juce::Rectangle<float> (cap.getWidth() - 4.0f, 1.6f).withCentre ({ cx, sliderPos }));
    }

    void LookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box)
    {
        const auto area = juce::Rectangle<float> ((float) width, (float) height).reduced (0.5f);
        draw::recess (g, area, 5.0f);
        g.setColour (box.hasKeyboardFocus (true) ? accentColour.withAlpha (0.6f) : juce::Colour (0xff30353d));
        g.drawRoundedRectangle (area, 5.0f, 1.0f);

        const auto arrowArea = area.withLeft (area.getRight() - (float) height).reduced ((float) height * 0.36f);
        juce::Path chevron;
        chevron.startNewSubPath (arrowArea.getX(), arrowArea.getY() + arrowArea.getHeight() * 0.25f);
        chevron.lineTo (arrowArea.getCentreX(), arrowArea.getBottom() - arrowArea.getHeight() * 0.2f);
        chevron.lineTo (arrowArea.getRight(), arrowArea.getY() + arrowArea.getHeight() * 0.25f);
        g.setColour ((box.isEnabled() ? colours::textDim : colours::textFaint));
        g.strokePath (chevron, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    juce::Font LookAndFeel::getComboBoxFont (juce::ComboBox& box)
    {
        return uiFont (juce::jmin (13.0f, (float) box.getHeight() * 0.5f), true, 0.04f);
    }

    void LookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
    {
        if (box.getProperties()["segmented"])
        {
            label.setBounds ({}); // SegmentedChoice draws its own items
            return;
        }

        label.setBounds (8, 1, box.getWidth() - box.getHeight() - 6, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
        label.setColour (juce::Label::textColourId, box.isEnabled() ? colours::text : colours::textFaint);
    }

    void LookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
    {
        const auto area = juce::Rectangle<float> ((float) width, (float) height);
        g.setGradientFill (juce::ColourGradient::vertical (colours::plateTop, 0.0f, colours::plateBottom, (float) height));
        g.fillRect (area);
        g.setColour (accentColour.withAlpha (0.35f));
        g.drawRect (area, 1.0f);
    }

    void LookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, bool isSeparator, bool isActive,
        bool isHighlighted, bool isTicked, bool hasSubMenu, const juce::String& text, const juce::String& shortcutKeyText,
        const juce::Drawable* icon, const juce::Colour* textColour)
    {
        if (isSeparator || hasSubMenu || icon != nullptr || shortcutKeyText.isNotEmpty())
        {
            LookAndFeel_V4::drawPopupMenuItem (g, area, isSeparator, isActive, isHighlighted, isTicked, hasSubMenu, text,
                shortcutKeyText, icon, textColour);
            return;
        }

        auto r = area.toFloat().reduced (3.0f, 1.0f);
        if (isHighlighted && isActive)
        {
            g.setColour (accentColour.withAlpha (0.2f));
            g.fillRoundedRectangle (r, 4.0f);
        }
        draw::led (g, { r.getX() + 10.0f, r.getCentreY() }, 3.2f, accentColour, isTicked ? 1.0f : 0.0f);
        g.setColour (isActive ? (textColour != nullptr ? *textColour : colours::text) : colours::textFaint);
        g.setFont (getPopupMenuFont());
        g.drawText (text, r.withTrimmedLeft (22.0f), juce::Justification::centredLeft, true);
    }

    juce::Font LookAndFeel::getPopupMenuFont()
    {
        return uiFont (14.0f);
    }

    void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&, bool isMouseOverButton, bool isButtonDown)
    {
        auto area = button.getLocalBounds().toFloat().reduced (1.5f);
        if (isButtonDown)
            area = area.translated (0.0f, 0.6f);
        const auto corner = area.getHeight() * 0.5f;
        const auto lit = button.isEnabled() ? accentColour : colours::textFaint;

        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillRoundedRectangle (area.translated (0.0f, isButtonDown ? 0.6f : 1.5f), corner);
        g.setColour (lit.withAlpha (isMouseOverButton ? 0.3f : 0.18f));
        g.fillRoundedRectangle (area.expanded (2.0f), corner + 2.0f);
        g.setGradientFill (juce::ColourGradient::vertical (lit.brighter (isMouseOverButton ? 0.35f : 0.2f), area.getY(), lit.darker (0.25f), area.getBottom()));
        g.fillRoundedRectangle (area, corner);
        g.setGradientFill (juce::ColourGradient::vertical (juce::Colours::white.withAlpha (0.45f), area.getY(), juce::Colours::white.withAlpha (0.0f), area.getCentreY()));
        g.drawRoundedRectangle (area.reduced (0.5f), corner, 1.0f);
    }

    void LookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button, bool, bool isButtonDown)
    {
        g.setFont (getTextButtonFont (button, button.getHeight()));
        g.setColour (juce::Colour (0xff0e1013));
        g.drawText (button.getButtonText(), button.getLocalBounds().toFloat().translated (0.0f, isButtonDown ? 0.6f : 0.0f),
            juce::Justification::centred, true);
    }

    juce::Font LookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
    {
        return uiFont (juce::jmin (13.5f, (float) buttonHeight * 0.45f), true, 0.02f);
    }

    void LookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height)
    {
        const auto area = juce::Rectangle<float> ((float) width, (float) height);
        g.setColour (colours::plateBottom);
        g.fillRoundedRectangle (area, 5.0f);
        g.setColour (accentColour.withAlpha (0.45f));
        g.drawRoundedRectangle (area.reduced (0.5f), 5.0f, 1.0f);

        juce::AttributedString s;
        s.setJustification (juce::Justification::centredLeft);
        s.append (text, uiFont (13.0f), colours::text);
        juce::TextLayout layout;
        layout.createLayoutWithBalancedLineLengths (s, (float) width - 16.0f);
        layout.draw (g, area.reduced (8.0f, 5.0f));
    }

    juce::Rectangle<int> LookAndFeel::getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos, juce::Rectangle<int> parentArea)
    {
        juce::AttributedString s;
        s.append (tipText, uiFont (13.0f), colours::text);
        juce::TextLayout layout;
        layout.createLayoutWithBalancedLineLengths (s, 300.0f);

        const auto w = (int) std::ceil (layout.getWidth()) + 18;
        const auto h = (int) std::ceil (layout.getHeight()) + 12;
        return juce::Rectangle<int> (screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 24,
            screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 6) : screenPos.y + 6,
            w, h)
            .constrainedWithin (parentArea);
    }

    juce::Colour accentOf (const juce::Component& component)
    {
        if (auto* look = dynamic_cast<const LookAndFeel*> (&component.getLookAndFeel()))
            return look->accent();
        return accentForKit ({});
    }
}
