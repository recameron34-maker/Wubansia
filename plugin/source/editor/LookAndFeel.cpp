#include "LookAndFeel.h"

#include "editor/skin/Paint.h"

#include <cmath>

namespace wubansia::editor
{

    namespace paint
    {
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

    LookAndFeel::LookAndFeel()
        : current (skinFor (SkinId::nightBus))
    {
        setSkin (current);
    }

    void LookAndFeel::setSkin (const Skin& skin)
    {
        current = skin;
        const auto& c = current.colours;
        const auto panel = over (c.panel, c.face);
        setColour (juce::ResizableWindow::backgroundColourId, c.face);
        setColour (juce::Label::textColourId, c.text);
        setColour (juce::ComboBox::textColourId, c.text);
        setColour (juce::ComboBox::backgroundColourId, over (c.well, c.face));
        setColour (juce::ComboBox::outlineColourId, c.controlEdge);
        setColour (juce::ComboBox::arrowColourId, c.dim);
        setColour (juce::PopupMenu::backgroundColourId, panel);
        setColour (juce::PopupMenu::textColourId, c.text);
        setColour (juce::PopupMenu::highlightedTextColourId, c.text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, c.accentTint);
        setColour (juce::TooltipWindow::textColourId, c.text);
        setColour (juce::TooltipWindow::backgroundColourId, panel);
        setColour (juce::TooltipWindow::outlineColourId, c.panelEdge);
        // Scroll bars (the drawers' lists; a Viewport has no colours of its own): a slim thumb on no track.
        setColour (juce::ScrollBar::backgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::ScrollBar::trackColourId, over (c.well, c.face));
        setColour (juce::ScrollBar::thumbColourId, c.controlEdge);
        setColour (juce::Slider::thumbColourId, c.accent);
        setColour (juce::Slider::rotarySliderFillColourId, c.accent);
        setColour (juce::TextEditor::backgroundColourId, over (c.well, c.face));
        setColour (juce::TextEditor::textColourId, c.text);
        setColour (juce::TextEditor::outlineColourId, c.controlEdge);
    }

    juce::Slider::SliderLayout LookAndFeel::getSliderLayout (juce::Slider& slider)
    {
        juce::Slider::SliderLayout layout;
        const auto bounds = slider.getLocalBounds();
        if (slider.isRotary() || slider.getSliderStyle() == juce::Slider::LinearBar)
            layout.sliderBounds = bounds;
        else if (slider.isVertical())
            layout.sliderBounds = bounds.reduced (0, getSliderThumbRadius (slider));
        else
            layout.sliderBounds = bounds.reduced (getSliderThumbRadius (slider), 0);
        return layout;
    }

    int LookAndFeel::getSliderThumbRadius (juce::Slider& slider)
    {
        return slider.isRotary() || slider.getSliderStyle() == juce::Slider::LinearBar ? 0 : 9;
    }

    namespace
    {
        // The colour a slider lights in: its "colour" property where a panel set one (a lane's), else the accent.
        juce::Colour lightOf (const juce::Slider& slider, const Skin& skin)
        {
            const auto v = slider.getProperties()["colour"];
            return v.isVoid() ? skin.colours.accent : juce::Colour ((juce::uint32) (juce::int64) v);
        }
    }

    void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos, float, float, juce::Slider& slider)
    {
        paint::knob (g, current, juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height), sliderPos,
            lightOf (slider, current), slider.isEnabled());
    }

    void LookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float, float, float,
        juce::Slider::SliderStyle, juce::Slider& slider)
    {
        const auto area = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height);
        const auto value = (float) slider.valueToProportionOfLength (slider.getValue());
        paint::fader (g, current, area, value, lightOf (slider, current), slider.isVertical(), slider.isEnabled());
    }

    void LookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box)
    {
        const auto area = juce::Rectangle<float> ((float) width, (float) height);
        paint::box (g, area, current.shape.segment, over (current.colours.well, current.colours.face), current.colours.controlEdge);
        const auto arrowArea = area.withLeft (area.getRight() - (float) height).reduced ((float) height * 0.36f);
        juce::Path chevron;
        chevron.startNewSubPath (arrowArea.getX(), arrowArea.getY() + arrowArea.getHeight() * 0.25f);
        chevron.lineTo (arrowArea.getCentreX(), arrowArea.getBottom() - arrowArea.getHeight() * 0.2f);
        chevron.lineTo (arrowArea.getRight(), arrowArea.getY() + arrowArea.getHeight() * 0.25f);
        g.setColour (box.isEnabled() ? current.colours.dim : current.colours.faint);
        g.strokePath (chevron, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    juce::Font LookAndFeel::getComboBoxFont (juce::ComboBox&)
    {
        return current.type.rowValue.fontAt (std::min (current.type.rowValue.px, 13.0f));
    }

    void LookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
    {
        if (box.getProperties()["segmented"])
        {
            label.setBounds ({}); // SegmentedChoice draws its own items
            return;
        }
        label.setBounds (6, 1, box.getWidth() - box.getHeight() - 4, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
        label.setColour (juce::Label::textColourId, box.isEnabled() ? current.colours.text : current.colours.faint);
    }

    void LookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
    {
        const auto area = juce::Rectangle<float> ((float) width, (float) height);
        g.setColour (over (current.colours.panel, current.colours.face));
        g.fillRect (area);
        g.setColour (current.colours.panelEdge);
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
            g.setColour (current.colours.accentTint);
            g.fillRoundedRectangle (r, 4.0f);
        }
        paint::dot (g, { r.getX() + 10.0f, r.getCentreY() }, 3.2f, current.colours.accent, isTicked);
        g.setColour (isActive ? (textColour != nullptr ? *textColour : current.colours.text) : current.colours.faint);
        g.setFont (getPopupMenuFont());
        g.drawText (text, r.withTrimmedLeft (22.0f), juce::Justification::centredLeft, true);
    }

    juce::Font LookAndFeel::getPopupMenuFont()
    {
        return bodyFont (current, 14.0f); // run-time words: fallback on
    }

    namespace
    {
        bool isQuiet (const juce::Button& button) { return button.getProperties()["quiet"]; }
    }

    void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&, bool isMouseOverButton, bool isButtonDown)
    {
        const auto& c = current.colours;
        const auto area = button.getLocalBounds().toFloat().reduced (1.0f).translated (0.0f, isButtonDown ? 0.5f : 0.0f);
        auto style = current.shape.headerButton;
        style.corner = std::min (style.corner, area.getHeight() * 0.5f);
        if (isQuiet (button))
        {
            paint::box (g, area, style, over (c.panel, c.face).brighter (isMouseOverButton ? 0.08f : 0.0f), c.controlEdge);
            return;
        }
        const auto lit = button.isEnabled() ? c.accent : c.faint;
        paint::box (g, area, style, isMouseOverButton ? lit.brighter (0.15f) : lit, lit);
    }

    void LookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button, bool, bool isButtonDown)
    {
        const auto font = getTextButtonFont (button, button.getHeight());
        const auto quiet = isQuiet (button);
        const auto colour = quiet ? (button.isEnabled() ? current.colours.text : current.colours.faint) : current.colours.accentInk;
        auto area = button.getLocalBounds().toFloat().translated (0.0f, isButtonDown ? 0.5f : 0.0f);
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
            return bodyFont (current, juce::jmin (12.5f, (float) buttonHeight * 0.5f), true, 0.02f);
        return bodyFont (current, juce::jmin (13.5f, (float) buttonHeight * 0.45f), true, 0.02f);
    }

    void LookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height)
    {
        const auto area = juce::Rectangle<float> ((float) width, (float) height);
        g.setColour (over (current.colours.panel, current.colours.face));
        g.fillRoundedRectangle (area, 5.0f);
        g.setColour (current.colours.panelEdge);
        g.drawRoundedRectangle (area.reduced (0.5f), 5.0f, 1.0f);
        juce::AttributedString s;
        s.setJustification (juce::Justification::centredLeft);
        s.append (text, bodyFont (current, 13.0f), current.colours.text);
        juce::TextLayout layout;
        layout.createLayoutWithBalancedLineLengths (s, (float) width - 16.0f);
        layout.draw (g, area.reduced (8.0f, 5.0f));
    }

    juce::Rectangle<int> LookAndFeel::getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos, juce::Rectangle<int> parentArea)
    {
        juce::AttributedString s;
        s.append (tipText, bodyFont (current, 13.0f), current.colours.text);
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
        return skinFor (SkinId::nightBus);
    }
}
