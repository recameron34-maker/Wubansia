#include "Controls.h"
#include "LookAndFeel.h"

#include <cmath>
#include <vector>

namespace wubansia::editor
{
    Knob::Knob()
        : juce::Slider (juce::Slider::RotaryVerticalDrag, juce::Slider::NoTextBox)
    {
        setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        setMouseDragSensitivity (220);
        setPopupMenuEnabled (false);
        setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    }

    void Knob::mouseDown (const juce::MouseEvent& e)
    {
        if (! takeContextClick (e))
            juce::Slider::mouseDown (e);
    }

    void Knob::mouseDrag (const juce::MouseEvent& e)
    {
        if (! e.mods.isPopupMenu())
            juce::Slider::mouseDrag (e);
    }

    void Knob::mouseUp (const juce::MouseEvent& e)
    {
        if (! e.mods.isPopupMenu())
            juce::Slider::mouseUp (e);
    }

    Fader::Fader()
        : juce::Slider (juce::Slider::LinearVertical, juce::Slider::NoTextBox)
    {
        setSliderSnapsToMousePosition (false); // grab the cap anywhere and move it relative
        setPopupMenuEnabled (false);
    }

    void Fader::mouseDown (const juce::MouseEvent& e)
    {
        if (! takeContextClick (e))
            juce::Slider::mouseDown (e);
    }

    void Fader::mouseDrag (const juce::MouseEvent& e)
    {
        if (! e.mods.isPopupMenu())
            juce::Slider::mouseDrag (e);
    }

    void Fader::mouseUp (const juce::MouseEvent& e)
    {
        if (! e.mods.isPopupMenu())
            juce::Slider::mouseUp (e);
    }

    PadButton::PadButton (const juce::String& name, const juce::String& letterToShow)
        : juce::Button (name), letter (letterToShow)
    {
        setClickingTogglesState (true);
    }

    void PadButton::paintButton (juce::Graphics& g, bool isMouseOver, bool isButtonDown)
    {
        const auto on = getToggleState();
        const auto light = skinOf (*this).colours.accent2;
        const auto area = getLocalBounds().toFloat().reduced (2.5f);
        paint::rubber (g, area, 5.0f, isButtonDown, isMouseOver && isEnabled(), light, on ? 1.0f : 0.0f);

        g.setFont (bodyFont (skinOf (*this), juce::jmin (13.0f, area.getHeight() * 0.55f), true, 0.06f));
        g.setColour (on ? juce::Colour (0xff1d1606) : (isEnabled() ? skinOf (*this).colours.dim : skinOf (*this).colours.faint));
        g.drawText (letter, area.translated (0.0f, isButtonDown ? 0.6f : 0.0f), juce::Justification::centred, false);
    }

    void PadButton::mouseDown (const juce::MouseEvent& e)
    {
        if (! takeContextClick (e))
            juce::Button::mouseDown (e);
    }

    void PadButton::mouseUp (const juce::MouseEvent& e)
    {
        if (! e.mods.isPopupMenu())
            juce::Button::mouseUp (e);
    }

    void LedButton::mouseDown (const juce::MouseEvent& e)
    {
        if (! takeContextClick (e))
            juce::Button::mouseDown (e);
    }

    void LedButton::mouseUp (const juce::MouseEvent& e)
    {
        if (! e.mods.isPopupMenu())
            juce::Button::mouseUp (e);
    }

    LedButton::LedButton (const juce::String& label)
        : juce::Button (label)
    {
        setClickingTogglesState (true);
    }

    void LedButton::paintButton (juce::Graphics& g, bool isMouseOver, bool isButtonDown)
    {
        const auto on = getToggleState() && isEnabled();
        const auto accent = skinOf (*this).colours.accent;
        auto area = getLocalBounds().toFloat().reduced (2.0f);
        const auto corner = area.getHeight() * 0.5f;
        paint::rubber (g, area, corner, isButtonDown, isMouseOver && isEnabled(), accent, 0.0f);
        if (isButtonDown)
            area = area.translated (0.0f, 0.6f);

        // On: a lit rim and a tint, so the state reads from across the room.
        if (on)
        {
            g.setGradientFill (juce::ColourGradient::vertical (accent.withAlpha (0.24f), area.getY(), accent.withAlpha (0.08f), area.getBottom()));
            g.fillRoundedRectangle (area, corner);
            g.setColour (accent.withAlpha (0.75f));
            g.drawRoundedRectangle (area.reduced (0.5f), corner, 1.0f);
        }

        const auto ledRadius = juce::jmin (4.6f, area.getHeight() * 0.19f);
        paint::led (g, { area.getX() + area.getHeight() * 0.5f + 1.0f, area.getCentreY() }, ledRadius, accent, on ? 1.0f : 0.0f);

        g.setFont (labelFont());
        g.setColour (! isEnabled() ? skinOf (*this).colours.faint : (on ? skinOf (*this).colours.text : skinOf (*this).colours.dim));
        g.drawText (getButtonText().toUpperCase(), labelArea().translated (0.0f, isButtonDown ? 0.6f : 0.0f), juce::Justification::centred, true);
    }

    juce::Font LedButton::labelFont() const
    {
        return bodyFont (skinOf (*this), juce::jmin (12.0f, getLocalBounds().toFloat().reduced (2.0f).getHeight() * 0.42f), true, 0.08f);
    }

    juce::Rectangle<float> LedButton::labelArea() const
    {
        // Right of the LED, as paintButton () draws it.
        const auto area = getLocalBounds().toFloat().reduced (2.0f);
        const auto ledRadius = juce::jmin (4.6f, area.getHeight() * 0.19f);
        return area.withTrimmedLeft (area.getHeight() * 0.5f + ledRadius + 7.0f).withTrimmedRight (6.0f);
    }

    AbSwitch::AbSwitch (const juce::String& name)
        : juce::Button (name)
    {
        setClickingTogglesState (false); // clicked () picks the side that was clicked
        setToggleable (true); // accessibility can still flip it
    }

    void AbSwitch::setLabels (const juce::String& newA, const juce::String& newB)
    {
        if (newA == a && newB == b)
            return;
        a = newA;
        b = newB;
        setTooltip (a.isNotEmpty() && b.isNotEmpty() ? "A: " + a + "   B: " + b : "This kit has no alternative here");
        repaint();
    }

    void AbSwitch::mouseDown (const juce::MouseEvent& e)
    {
        if (takeContextClick (e))
            return;
        pressedSide = e.x < getWidth() / 2 ? 0 : 1;
        juce::Button::mouseDown (e);
    }

    void AbSwitch::mouseUp (const juce::MouseEvent& e)
    {
        if (! e.mods.isPopupMenu())
            juce::Button::mouseUp (e);
    }

    void AbSwitch::clicked()
    {
        // A mouse click picks the side it landed on; anything else (keyboard, accessibility,
        // setToggleState with a notification) keeps the toggle as it was given.
        if (pressedSide >= 0)
            setToggleState (pressedSide == 1, juce::dontSendNotification);
        pressedSide = -1;
    }

    void AbSwitch::paintButton (juce::Graphics& g, bool isMouseOver, bool isButtonDown)
    {
        juce::ignoreUnused (isButtonDown);
        const auto accent = skinOf (*this).colours.accent;
        const auto area = getLocalBounds().toFloat().reduced (1.5f);
        paint::recess (g, area, 6.0f);

        if (! isEnabled() || a.isEmpty() || b.isEmpty())
        {
            g.setFont (bodyFont (skinOf (*this), 11.0f, true, 0.06f));
            g.setColour (skinOf (*this).colours.faint);
            g.drawText (a.isNotEmpty() ? a + juce::String (juce::CharPointer_UTF8 ("  \xc2\xb7  one sound")) : juce::String ("one sound in this kit"),
                area, juce::Justification::centred, true);
            return;
        }

        const auto selected = getToggleState() ? 1 : 0;
        for (int side = 0; side < 2; ++side)
        {
            auto half = (side == 0 ? area.withWidth (area.getWidth() * 0.5f) : area.withLeft (area.getCentreX())).reduced (2.5f);
            const auto on = side == selected;
            if (on)
            {
                g.setColour (accent.withAlpha (0.16f));
                g.fillRoundedRectangle (half.expanded (1.5f), 6.0f);
                g.setGradientFill (juce::ColourGradient::vertical (accent.withAlpha (0.34f), half.getY(), accent.withAlpha (0.14f), half.getBottom()));
                g.fillRoundedRectangle (half, 4.5f);
                g.setColour (accent.withAlpha (0.85f));
                g.drawRoundedRectangle (half, 4.5f, 1.0f);
            }
            else if (isMouseOver)
            {
                g.setColour (juce::Colours::white.withAlpha (0.04f));
                g.fillRoundedRectangle (half, 4.5f);
            }

            auto badge = half.removeFromLeft (18.0f).withSizeKeepingCentre (14.0f, 14.0f).translated (4.0f, 0.0f);
            g.setColour (on ? accent : skinOf (*this).colours.faint.withAlpha (0.5f));
            g.drawRoundedRectangle (badge, 3.0f, 1.0f);
            g.setFont (bodyFont (skinOf (*this), 9.5f, true));
            g.drawText (side == 0 ? "A" : "B", badge, juce::Justification::centred, false);

            g.setFont (bodyFont (skinOf (*this), 11.5f, true, 0.02f));
            g.setColour (on ? skinOf (*this).colours.text : skinOf (*this).colours.dim);
            g.drawFittedText (side == 0 ? a : b, half.withTrimmedLeft (6.0f).withTrimmedRight (3.0f).toNearestInt(),
                juce::Justification::centredLeft, 1, 0.8f);
        }
    }

    SegmentedChoice::SegmentedChoice (const juce::String& name, const juce::StringArray& items)
        : juce::ComboBox (name)
    {
        getProperties().set ("segmented", true); // LookAndFeel::positionComboBoxText hides the text box
        addItemList (items, 1);
    }

    void SegmentedChoice::paint (juce::Graphics& g)
    {
        const auto accent = skinOf (*this).colours.accent;
        const auto area = getLocalBounds().toFloat().reduced (0.5f);
        paint::recess (g, area, 6.0f);

        const auto count = getNumItems();
        if (count <= 0)
            return;

        const auto selected = getSelectedItemIndex();
        const auto font = labelFont();
        for (int i = 0; i < count; ++i)
        {
            const auto cell = segmentArea (i).reduced (2.5f);
            const auto on = i == selected && isEnabled();
            if (on)
            {
                g.setColour (accent.withAlpha (0.16f));
                g.fillRoundedRectangle (cell.expanded (1.5f), 6.0f);
                g.setGradientFill (juce::ColourGradient::vertical (accent.withAlpha (0.36f), cell.getY(), accent.withAlpha (0.14f), cell.getBottom()));
                g.fillRoundedRectangle (cell, 4.5f);
                g.setColour (accent.withAlpha (0.9f));
                g.drawRoundedRectangle (cell, 4.5f, 1.0f);
            }
            else if (i > 0 && i != selected + 1)
            {
                g.setColour (juce::Colours::white.withAlpha (0.06f));
                g.fillRect (juce::Rectangle<float> (1.0f, cell.getHeight() * 0.5f).withCentre ({ cell.getX() - 2.5f, cell.getCentreY() }));
            }

            g.setFont (font);
            g.setColour (! isEnabled() ? skinOf (*this).colours.faint : (on ? skinOf (*this).colours.text : skinOf (*this).colours.dim));
            g.drawFittedText (getItemText (i).toUpperCase(), cell.toNearestInt(), juce::Justification::centred, 1, 0.75f);
        }

        // Keyboard focus: the outline LookAndFeel draws round a focused combo box.
        if (hasKeyboardFocus (false))
        {
            g.setColour (accent.withAlpha (0.6f));
            g.drawRoundedRectangle (area, 6.0f, 1.0f);
        }
    }

    void SegmentedChoice::mouseDown (const juce::MouseEvent& e)
    {
        if (takeContextClick (e))
            return;
        const auto count = getNumItems();
        if (! isEnabled() || count <= 0 || getWidth() <= 0)
            return;
        const auto x = (float) e.x;
        int index = x < segmentArea (0).getX() ? 0 : count - 1;
        for (int i = 0; i < count; ++i)
            if (const auto area = segmentArea (i); x >= area.getX() && x < area.getRight())
                index = i;
        setSelectedItemIndex (index, juce::sendNotificationSync);
    }

    void SegmentedChoice::showPopup()
    {
        // No menu: the keyboard's "open" (Return) steps to the next segment. ComboBox marks its popup active before it
        // asks for it and only hidePopup clears that, so without it Return would work once and a screen reader would
        // hear "expanded" from then on.
        if (const auto count = getNumItems(); count > 0)
            setSelectedItemIndex ((getSelectedItemIndex() + 1) % count, juce::sendNotificationSync);
        hidePopup();
    }

    bool SegmentedChoice::keyPressed (const juce::KeyPress& key)
    {
        // The arrows tell onChange at once: ComboBox's own would tell it later, and a tick showing the caller's value in
        // between would take the key back. Return goes ComboBox's way, to showPopup; every other key, Space included,
        // goes on to the host.
        const auto step = key == juce::KeyPress::upKey || key == juce::KeyPress::leftKey ? -1
                          : key == juce::KeyPress::downKey || key == juce::KeyPress::rightKey ? 1
                                                                                                : 0;
        if (step == 0)
            return key == juce::KeyPress::returnKey && juce::ComboBox::keyPressed (key);
        if (const auto count = getNumItems(); isEnabled() && count > 0)
        {
            const auto index = juce::jlimit (0, count - 1, getSelectedItemIndex() + step);
            if (index != getSelectedItemIndex())
                setSelectedItemIndex (index, juce::sendNotificationSync);
        }
        return true;
    }

    void SegmentedChoice::setSegmentsFitLabels (bool shouldFit)
    {
        if (fitLabels != shouldFit)
        {
            fitLabels = shouldFit;
            repaint();
        }
    }

    namespace
    {
        // The widths a segment's label needs at this font, kLabelPad each side.
        std::vector<float> labelWidths (const SegmentedChoice& choice, const juce::Font& font)
        {
            std::vector<float> widths;
            for (int i = 0; i < choice.getNumItems(); ++i)
                widths.push_back (juce::GlyphArrangement::getStringWidth (font, choice.getItemText (i).toUpperCase())
                                  + 2.0f * SegmentedChoice::kLabelPad);
            return widths;
        }
    }

    int SegmentedChoice::fitWidth (int height) const
    {
        const auto font = bodyFont (skinOf (*this), juce::jmin (11.5f, ((float) height - 6.0f) * 0.5f), true, 0.1f);
        float total = 1.0f; // the 0.5 px border on each side
        for (const auto w : labelWidths (*this, font))
            total += w;
        return (int) std::ceil (total);
    }

    juce::Rectangle<float> SegmentedChoice::segmentArea (int index) const
    {
        const auto count = getNumItems();
        if (index < 0 || index >= count)
            return {};
        const auto area = getLocalBounds().toFloat().reduced (0.5f);
        if (! fitLabels)
        {
            const auto width = area.getWidth() / (float) count;
            return { area.getX() + width * (float) index, area.getY(), width, area.getHeight() };
        }

        // Each label's need, then the room left over shared equally, or every segment squeezed alike when short.
        const auto widths = labelWidths (*this, labelFont());
        float need = 0.0f;
        for (const auto w : widths)
            need += w;
        const auto extra = (area.getWidth() - need) / (float) count;
        const auto scale = need > 0.0f && extra < 0.0f ? area.getWidth() / need : 1.0f;
        const auto widthOf = [&] (int i) { return extra < 0.0f ? widths[(std::size_t) i] * scale : widths[(std::size_t) i] + extra; };
        float x = area.getX();
        for (int i = 0; i < index; ++i)
            x += widthOf (i);
        const auto right = index == count - 1 ? area.getRight() : x + widthOf (index);
        return { x, area.getY(), right - x, area.getHeight() };
    }

    juce::Font SegmentedChoice::labelFont() const
    {
        return bodyFont (skinOf (*this), juce::jmin (11.5f, ((float) getHeight() - 6.0f) * 0.5f), true, 0.1f);
    }

    IconButton::IconButton (const juce::String& name, Icon iconToShow)
        : juce::Button (name), icon (iconToShow)
    {
    }

    void IconButton::setIcon (Icon newIcon)
    {
        if (icon != newIcon)
        {
            icon = newIcon;
            repaint();
        }
    }

    void IconButton::paintButton (juce::Graphics& g, bool isMouseOver, bool isButtonDown)
    {
        const auto accent = skinOf (*this).colours.accent;
        const auto on = getToggleState();
        auto area = getLocalBounds().toFloat().reduced (2.0f);
        paint::rubber (g, area, 5.0f, isButtonDown, isMouseOver && isEnabled(), accent, on ? 0.85f : 0.0f);
        if (isButtonDown)
            area = area.translated (0.0f, 0.6f);

        const auto size = juce::jmin (area.getWidth(), area.getHeight()) * 0.42f;
        const auto box = area.withSizeKeepingCentre (size, size);
        const auto colour = ! isEnabled() ? skinOf (*this).colours.faint : (on ? juce::Colour (0xff10141a) : skinOf (*this).colours.text);

        juce::Path p;
        switch (icon)
        {
            case Icon::play:
                p = paint::playGlyph (box);
                break;
            case Icon::stop:
                p = paint::stopGlyph (box);
                break;
            case Icon::folder:
                p = paint::folderGlyph (box);
                break;
            case Icon::close:
            case Icon::plus:
            {
                const auto t = size * 0.16f;
                juce::Path bar;
                bar.addRoundedRectangle (-size * 0.5f, -t * 0.5f, size, t, t * 0.5f);
                const auto centre = box.getCentre();
                const auto base = icon == Icon::close ? juce::MathConstants<float>::pi * 0.25f : 0.0f;
                p.addPath (bar, juce::AffineTransform::rotation (base).translated (centre));
                p.addPath (bar, juce::AffineTransform::rotation (base + juce::MathConstants<float>::halfPi).translated (centre));
                break;
            }
            case Icon::minus:
            {
                const auto t = size * 0.16f;
                p.addRoundedRectangle (box.getX(), box.getCentreY() - t * 0.5f, size, t, t * 0.5f);
                break;
            }
            case Icon::reset:
            {
                const auto centre = box.getCentre();
                juce::Path arc;
                arc.addCentredArc (centre.x, centre.y, size * 0.45f, size * 0.45f, 0.0f, 0.5f, juce::MathConstants<float>::twoPi - 0.3f, true);
                g.setColour (colour);
                g.strokePath (arc, juce::PathStrokeType (size * 0.13f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                const auto tip = centre.getPointOnCircumference (size * 0.45f, 0.5f);
                p.addTriangle (tip.x - size * 0.22f, tip.y - size * 0.05f, tip.x + size * 0.12f, tip.y - size * 0.28f, tip.x + size * 0.1f, tip.y + size * 0.14f);
                break;
            }
        }
        g.setColour (colour);
        g.fillPath (p);
    }
}
