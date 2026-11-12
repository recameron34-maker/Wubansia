#include "Controls.h"
#include "LookAndFeel.h"

namespace wubansia::editor
{
    namespace
    {
        // The dark rubber body every button shares; lit buttons glow in their colour.
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
    }

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

    PadButton::PadButton (const juce::String& name, const juce::String& letterToShow, juce::Colour lightColour)
        : juce::Button (name), letter (letterToShow), light (lightColour)
    {
        setClickingTogglesState (true);
    }

    void PadButton::paintButton (juce::Graphics& g, bool isMouseOver, bool isButtonDown)
    {
        const auto on = getToggleState();
        const auto area = getLocalBounds().toFloat().reduced (2.5f);
        rubber (g, area, 5.0f, isButtonDown, isMouseOver && isEnabled(), light, on ? 1.0f : 0.0f);

        g.setFont (uiFont (juce::jmin (13.0f, area.getHeight() * 0.55f), true, 0.06f));
        g.setColour (on ? juce::Colour (0xff1d1606) : (isEnabled() ? colours::textDim : colours::textFaint));
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
        const auto accent = accentOf (*this);
        auto area = getLocalBounds().toFloat().reduced (2.0f);
        const auto corner = area.getHeight() * 0.5f;
        rubber (g, area, corner, isButtonDown, isMouseOver && isEnabled(), accent, 0.0f);
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
        draw::led (g, { area.getX() + area.getHeight() * 0.5f + 1.0f, area.getCentreY() }, ledRadius, accent, on ? 1.0f : 0.0f);

        g.setFont (labelFont());
        g.setColour (! isEnabled() ? colours::textFaint : (on ? colours::text : colours::textDim));
        g.drawText (getButtonText().toUpperCase(), labelArea().translated (0.0f, isButtonDown ? 0.6f : 0.0f), juce::Justification::centred, true);
    }

    juce::Font LedButton::labelFont() const
    {
        return uiFont (juce::jmin (12.0f, getLocalBounds().toFloat().reduced (2.0f).getHeight() * 0.42f), true, 0.08f);
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
        const auto accent = accentOf (*this);
        const auto area = getLocalBounds().toFloat().reduced (1.5f);
        draw::recess (g, area, 6.0f);

        if (! isEnabled() || a.isEmpty() || b.isEmpty())
        {
            g.setFont (uiFont (11.0f, true, 0.06f));
            g.setColour (colours::textFaint);
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
            g.setColour (on ? accent : colours::textFaint.withAlpha (0.5f));
            g.drawRoundedRectangle (badge, 3.0f, 1.0f);
            g.setFont (uiFont (9.5f, true));
            g.drawText (side == 0 ? "A" : "B", badge, juce::Justification::centred, false);

            g.setFont (uiFont (11.5f, true, 0.02f));
            g.setColour (on ? colours::text : colours::textDim);
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
        const auto accent = accentOf (*this);
        const auto area = getLocalBounds().toFloat().reduced (0.5f);
        draw::recess (g, area, 6.0f);

        const auto count = getNumItems();
        if (count <= 0)
            return;

        const auto width = area.getWidth() / (float) count;
        const auto selected = getSelectedItemIndex();
        for (int i = 0; i < count; ++i)
        {
            const auto cell = juce::Rectangle<float> (area.getX() + width * (float) i, area.getY(), width, area.getHeight()).reduced (2.5f);
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

            g.setFont (uiFont (juce::jmin (11.5f, cell.getHeight() * 0.5f), true, 0.1f));
            g.setColour (! isEnabled() ? colours::textFaint : (on ? colours::text : colours::textDim));
            g.drawFittedText (getItemText (i).toUpperCase(), cell.toNearestInt(), juce::Justification::centred, 1, 0.75f);
        }
    }

    void SegmentedChoice::mouseDown (const juce::MouseEvent& e)
    {
        if (takeContextClick (e))
            return;
        const auto count = getNumItems();
        if (! isEnabled() || count <= 0 || getWidth() <= 0)
            return;
        const auto index = juce::jlimit (0, count - 1, e.x * count / getWidth());
        setSelectedItemIndex (index, juce::sendNotificationSync);
    }

    void SegmentedChoice::showPopup()
    {
        // No menu: the keyboard's "open" steps to the next segment.
        if (const auto count = getNumItems(); count > 0)
            setSelectedItemIndex ((getSelectedItemIndex() + 1) % count, juce::sendNotificationSync);
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
        const auto accent = accentOf (*this);
        const auto on = getToggleState();
        auto area = getLocalBounds().toFloat().reduced (2.0f);
        rubber (g, area, 5.0f, isButtonDown, isMouseOver && isEnabled(), accent, on ? 0.85f : 0.0f);
        if (isButtonDown)
            area = area.translated (0.0f, 0.6f);

        const auto size = juce::jmin (area.getWidth(), area.getHeight()) * 0.42f;
        const auto box = area.withSizeKeepingCentre (size, size);
        const auto colour = ! isEnabled() ? colours::textFaint : (on ? juce::Colour (0xff10141a) : colours::text);

        juce::Path p;
        switch (icon)
        {
            case Icon::play:
                p.addTriangle (box.getX() + size * 0.1f, box.getY(), box.getX() + size * 0.1f, box.getBottom(), box.getRight(), box.getCentreY());
                break;
            case Icon::stop:
                p.addRoundedRectangle (box.reduced (size * 0.06f), size * 0.08f);
                break;
            case Icon::folder:
                p.startNewSubPath (box.getX(), box.getY() + size * 0.15f);
                p.lineTo (box.getX() + size * 0.38f, box.getY() + size * 0.15f);
                p.lineTo (box.getX() + size * 0.5f, box.getY() + size * 0.3f);
                p.lineTo (box.getRight(), box.getY() + size * 0.3f);
                p.lineTo (box.getRight(), box.getBottom() - size * 0.05f);
                p.lineTo (box.getX(), box.getBottom() - size * 0.05f);
                p.closeSubPath();
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
