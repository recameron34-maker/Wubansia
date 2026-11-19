#include "LookAndFeel.h"

#include <cmath>

namespace wubansia::editor
{
    namespace
    {
        namespace lit
        {
            inline const juce::Colour background { 0xff0d0e11 };
            inline const juce::Colour panelTop { 0xff1d2025 };
            inline const juce::Colour panelBottom { 0xff131519 };
            inline const juce::Colour plateTop { 0xff252930 };
            inline const juce::Colour plateBottom { 0xff1b1e23 };
            inline const juce::Colour recess { 0xff08090b };
            inline const juce::Colour text { 0xffeceef2 };
            inline const juce::Colour textDim { 0xffa6acb7 };
            inline const juce::Colour textFaint { 0xff686e79 };
            inline const juce::Colour engraved { 0xff7d838e };
            inline const juce::Colour mute { 0xffeef1f6 }; // the mute pads' light: white, apart from every kit's accent
            inline const juce::Colour busy { 0xffffc23d }; // loading
            inline const juce::Colour warning { 0xffff6a4d }; // problems
            inline const juce::Colour hot { 0xfffff5ec }; // a meter's top segment: white-hot whatever the accent
        }
    }

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

    namespace paint
    {
        void faceplate (juce::Graphics& g, juce::Rectangle<float> area)
        {
            g.setGradientFill (juce::ColourGradient::vertical (lit::panelTop, area.getY(), lit::panelBottom, area.getBottom()));
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

            g.setGradientFill (juce::ColourGradient::vertical (lit::plateTop, area.getY(), lit::plateBottom, area.getBottom()));
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
            g.setColour (lit::recess);
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

                const auto colour = (hotFrom >= 0 && i >= hotFrom) ? lit::hot
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

        void padsGlyph (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour)
        {
            const auto size = juce::jmin (area.getWidth(), area.getHeight());
            const auto square = area.withSizeKeepingCentre (size, size).reduced (size * 0.04f);
            const auto gap = square.getWidth() * 0.16f;
            const auto pad = (square.getWidth() - gap) * 0.5f;
            g.setColour (colour);
            for (int row = 0; row < 2; ++row)
                for (int col = 0; col < 2; ++col)
                    g.fillRoundedRectangle (square.getX() + (float) col * (pad + gap), square.getY() + (float) row * (pad + gap), pad, pad, pad * 0.25f);
        }

        void rubber (juce::Graphics& g, juce::Rectangle<float> area, float corner, bool down, bool over, juce::Colour light, float lit)
        {
            if (down)
                area = area.translated (0.0f, 0.6f);

            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.fillRoundedRectangle (area.translated (0.0f, down ? 0.6f : 1.4f).expanded (0.6f), corner + 0.6f);

            if (lit > 0.0f)
            {
                g.setColour (light.withAlpha (0.22f * lit));
                g.fillRoundedRectangle (area.expanded (2.5f), corner + 2.5f);
                g.setGradientFill (juce::ColourGradient::vertical (light.brighter (0.25f), area.getY(), light.darker (0.35f), area.getBottom()));
            }
            else
            {
                const auto top = juce::Colour (over ? 0xff353a42 : 0xff2d3138);
                g.setGradientFill (juce::ColourGradient::vertical (top, area.getY(), juce::Colour (0xff1b1e22), area.getBottom()));
            }
            g.fillRoundedRectangle (area, corner);

            g.setGradientFill (juce::ColourGradient::vertical (juce::Colours::white.withAlpha (lit > 0.0f ? 0.35f : 0.10f), area.getY(),
                juce::Colours::white.withAlpha (0.0f), area.getCentreY()));
            g.drawRoundedRectangle (area.reduced (0.5f), corner, 1.0f);
            g.setColour (juce::Colours::black.withAlpha (0.6f));
            g.drawRoundedRectangle (area.expanded (0.4f), corner + 0.4f, 0.8f);
        }

        juce::Path playGlyph (juce::Rectangle<float> box)
        {
            const auto size = box.getWidth();
            juce::Path p;
            p.addTriangle (box.getX() + size * 0.1f, box.getY(), box.getX() + size * 0.1f, box.getBottom(), box.getRight(), box.getCentreY());
            return p;
        }

        juce::Path stopGlyph (juce::Rectangle<float> box)
        {
            const auto size = box.getWidth();
            juce::Path p;
            p.addRoundedRectangle (box.reduced (size * 0.06f), size * 0.08f);
            return p;
        }

        juce::Path folderGlyph (juce::Rectangle<float> box)
        {
            const auto size = box.getWidth();
            juce::Path p;
            p.startNewSubPath (box.getX(), box.getY() + size * 0.15f);
            p.lineTo (box.getX() + size * 0.38f, box.getY() + size * 0.15f);
            p.lineTo (box.getX() + size * 0.5f, box.getY() + size * 0.3f);
            p.lineTo (box.getRight(), box.getY() + size * 0.3f);
            p.lineTo (box.getRight(), box.getBottom() - size * 0.05f);
            p.lineTo (box.getX(), box.getBottom() - size * 0.05f);
            p.closeSubPath();
            return p;
        }

        void grip (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour)
        {
            const auto dot = juce::jmin (area.getWidth() / 3.0f, area.getHeight() / 5.0f);
            const auto columns = area.withSizeKeepingCentre (dot * 3.0f, dot * 5.0f);
            g.setColour (colour);
            for (int row = 0; row < 3; ++row)
                for (int column = 0; column < 2; ++column)
                    g.fillEllipse (juce::Rectangle<float> (dot, dot).withPosition (columns.getX() + (float) column * dot * 2.0f,
                        columns.getY() + (float) row * dot * 2.0f));
        }
    }

    Skin litHardwareSkin (const juce::String& kitId)
    {
        Skin s;
        s.id = SkinId::litHardware;
        s.name = "Lit hardware";
        auto& c = s.colours;
        c.face = lit::background;
        c.panel = lit::plateTop;
        c.lane = lit::plateBottom;
        c.panelHighlight = lit::panelTop;
        c.panelEdge = lit::panelBottom;
        c.well = lit::recess;
        c.text = lit::text;
        c.dim = lit::textDim;
        c.faint = lit::textFaint;
        c.label = lit::engraved;
        c.accent = accentForKit (kitId);
        c.accent2 = lit::mute;
        c.faceGlow = lit::hot;
        c.busy = lit::busy;
        c.warning = lit::warning;
        c.focus = c.accent;
        s.type.body = { FaceId::platform, 13.0f };
        s.type.bodyStrong = { FaceId::platform, 13.0f };
        s.type.path = { FaceId::platform, 11.5f, 0.0f, false, 0.0f, true };
        s.display = DisplayKind::lcd;
        s.lane = LaneKind::plate;
        s.texture = Texture::brushed;
        return s;
    }

    LookAndFeel::LookAndFeel()
        : current (litHardwareSkin ({}))
    {
        setColour (juce::ResizableWindow::backgroundColourId, current.colours.face);
        setColour (juce::Label::textColourId, current.colours.text);
        setColour (juce::ComboBox::textColourId, current.colours.text);
        setColour (juce::ComboBox::backgroundColourId, current.colours.well);
        setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff343941));
        setColour (juce::ComboBox::arrowColourId, current.colours.dim);
        setColour (juce::PopupMenu::backgroundColourId, current.colours.lane);
        setColour (juce::PopupMenu::textColourId, current.colours.text);
        setColour (juce::PopupMenu::highlightedTextColourId, current.colours.text);
        setColour (juce::TooltipWindow::textColourId, current.colours.text);
        setColour (juce::TooltipWindow::backgroundColourId, current.colours.lane);
        setColour (juce::TooltipWindow::outlineColourId, juce::Colour (0xff3a3f48));
        // Scroll bars (the Samples drawer's list; a Viewport has no colours of its own): a slim grey thumb on no track.
        setColour (juce::ScrollBar::backgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::ScrollBar::trackColourId, current.colours.well);
        setColour (juce::ScrollBar::thumbColourId, juce::Colour (0xff4b515b));
        setSkin (current);
    }

    void LookAndFeel::setSkin (const Skin& skin)
    {
        current = skin;
        const auto colour = current.colours.accent;
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
        const auto lit = enabled ? current.colours.accent : current.colours.faint;
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
            g.setColour (current.colours.faint);
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
        g.setColour (enabled ? lit.brighter (0.3f) : current.colours.faint);
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
        const auto lit = enabled ? current.colours.accent : current.colours.faint;
        const auto cx = area.getCentreX();

        // Scale marks every 6 dB down to -24, then -48; 0 dB marked longer.
        for (const auto db : { 6.0, 0.0, -6.0, -12.0, -18.0, -24.0, -36.0, -48.0, -60.0 })
        {
            if (db < slider.getMinimum() || db > slider.getMaximum())
                continue;
            const auto py = (float) slider.getPositionOfValue (db);
            const auto zero = juce::exactlyEqual (db, 0.0);
            const auto length = zero ? 9.0f : 5.0f;
            g.setColour (zero ? current.colours.dim : current.colours.faint);
            g.fillRect (juce::Rectangle<float> (cx - 9.0f - length, py - 0.5f, length, 1.0f));
            g.fillRect (juce::Rectangle<float> (cx + 9.0f, py - 0.5f, length, 1.0f));
        }

        const auto slot = juce::Rectangle<float> (cx - 2.5f, area.getY() - 4.0f, 5.0f, area.getHeight() + 8.0f);
        paint::recess (g, slot, 2.5f);
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
        g.setColour (enabled ? lit.brighter (0.25f) : current.colours.faint);
        g.fillRect (juce::Rectangle<float> (cap.getWidth() - 4.0f, 1.6f).withCentre ({ cx, sliderPos }));
    }

    void LookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box)
    {
        const auto area = juce::Rectangle<float> ((float) width, (float) height).reduced (0.5f);
        paint::recess (g, area, 5.0f);
        g.setColour (box.hasKeyboardFocus (true) ? current.colours.accent.withAlpha (0.6f) : juce::Colour (0xff30353d));
        g.drawRoundedRectangle (area, 5.0f, 1.0f);

        const auto arrowArea = area.withLeft (area.getRight() - (float) height).reduced ((float) height * 0.36f);
        juce::Path chevron;
        chevron.startNewSubPath (arrowArea.getX(), arrowArea.getY() + arrowArea.getHeight() * 0.25f);
        chevron.lineTo (arrowArea.getCentreX(), arrowArea.getBottom() - arrowArea.getHeight() * 0.2f);
        chevron.lineTo (arrowArea.getRight(), arrowArea.getY() + arrowArea.getHeight() * 0.25f);
        g.setColour ((box.isEnabled() ? current.colours.dim : current.colours.faint));
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
        label.setColour (juce::Label::textColourId, box.isEnabled() ? current.colours.text : current.colours.faint);
    }

    void LookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
    {
        const auto area = juce::Rectangle<float> ((float) width, (float) height);
        g.setGradientFill (juce::ColourGradient::vertical (current.colours.panel, 0.0f, current.colours.lane, (float) height));
        g.fillRect (area);
        g.setColour (current.colours.accent.withAlpha (0.35f));
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
            g.setColour (current.colours.accent.withAlpha (0.2f));
            g.fillRoundedRectangle (r, 4.0f);
        }
        paint::led (g, { r.getX() + 10.0f, r.getCentreY() }, 3.2f, current.colours.accent, isTicked ? 1.0f : 0.0f);
        g.setColour (isActive ? (textColour != nullptr ? *textColour : current.colours.text) : current.colours.faint);
        g.setFont (getPopupMenuFont());
        g.drawText (text, r.withTrimmedLeft (22.0f), juce::Justification::centredLeft, true);
    }

    juce::Font LookAndFeel::getPopupMenuFont()
    {
        return uiFont (14.0f);
    }

    namespace
    {
        bool isQuiet (const juce::Button& button) { return button.getProperties()["quiet"]; }
    }

    void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&, bool isMouseOverButton, bool isButtonDown)
    {
        if (isQuiet (button))
        {
            const auto area = button.getLocalBounds().toFloat().reduced (1.5f);
            paint::rubber (g, area, juce::jmin (6.0f, area.getHeight() * 0.5f), isButtonDown, isMouseOverButton && button.isEnabled(), current.colours.accent, 0.0f);
            return;
        }

        auto area = button.getLocalBounds().toFloat().reduced (1.5f);
        if (isButtonDown)
            area = area.translated (0.0f, 0.6f);
        const auto corner = area.getHeight() * 0.5f;
        const auto lit = button.isEnabled() ? current.colours.accent : current.colours.faint;

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
        const auto font = getTextButtonFont (button, button.getHeight());
        const auto quiet = isQuiet (button);
        const auto colour = quiet ? (button.isEnabled() ? current.colours.text : current.colours.faint) : juce::Colour (0xff0e1013);
        auto area = button.getLocalBounds().toFloat().translated (0.0f, isButtonDown ? 0.6f : 0.0f);

        // An icon before the text, the pair centred.
        const auto icon = button.getProperties()["icon"].toString();
        if (icon == "play" || icon == "folder")
        {
            const auto size = juce::jmin (11.0f, font.getHeight() * 0.85f);
            const auto gap = 6.0f;
            const auto textWidth = juce::jmin (juce::GlyphArrangement::getStringWidth (font, button.getButtonText()), area.getWidth() - size - gap - 16.0f);
            const auto left = area.getCentreX() - (size + gap + textWidth) * 0.5f;
            const auto box = juce::Rectangle<float> (size, size).withPosition (left, area.getCentreY() - size * 0.5f);
            g.setColour (quiet && button.isEnabled() && icon == "folder" ? current.colours.accent : colour);
            g.fillPath (icon == "play" ? paint::playGlyph (box) : paint::folderGlyph (box));
            area = juce::Rectangle<float> (left + size + gap, area.getY(), textWidth + 1.0f, area.getHeight());
        }

        g.setFont (font);
        g.setColour (colour);
        g.drawText (button.getButtonText(), area, juce::Justification::centred, true);
    }

    juce::Font LookAndFeel::getTextButtonFont (juce::TextButton& button, int buttonHeight)
    {
        if (isQuiet (button))
            return uiFont (juce::jmin (12.5f, (float) buttonHeight * 0.5f), true, 0.02f);
        return uiFont (juce::jmin (13.5f, (float) buttonHeight * 0.45f), true, 0.02f);
    }

    void LookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height)
    {
        const auto area = juce::Rectangle<float> ((float) width, (float) height);
        g.setColour (current.colours.lane);
        g.fillRoundedRectangle (area, 5.0f);
        g.setColour (current.colours.accent.withAlpha (0.45f));
        g.drawRoundedRectangle (area.reduced (0.5f), 5.0f, 1.0f);

        juce::AttributedString s;
        s.setJustification (juce::Justification::centredLeft);
        s.append (text, uiFont (13.0f), current.colours.text);
        juce::TextLayout layout;
        layout.createLayoutWithBalancedLineLengths (s, (float) width - 16.0f);
        layout.draw (g, area.reduced (8.0f, 5.0f));
    }

    juce::Rectangle<int> LookAndFeel::getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos, juce::Rectangle<int> parentArea)
    {
        juce::AttributedString s;
        s.append (tipText, uiFont (13.0f), current.colours.text);
        juce::TextLayout layout;
        layout.createLayoutWithBalancedLineLengths (s, 300.0f);

        const auto w = (int) std::ceil (layout.getWidth()) + 18;
        const auto h = (int) std::ceil (layout.getHeight()) + 12;
        return juce::Rectangle<int> (screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 24,
            screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 6) : screenPos.y + 6,
            w, h)
            .constrainedWithin (parentArea);
    }

    const Skin& skinOf (const juce::Component& component) noexcept
    {
        if (auto* look = dynamic_cast<const LookAndFeel*> (&component.getLookAndFeel()))
            return look->skin();
        static const Skin fallback = litHardwareSkin ({});
        return fallback;
    }
}
