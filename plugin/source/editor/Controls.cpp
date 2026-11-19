#include "Controls.h"

#include "EditorModel.h"
#include "LookAndFeel.h"
#include "editor/skin/Paint.h"

#include <cmath>
#include <vector>

namespace wubansia::editor
{
    bool sliderKey (juce::Slider& slider, const juce::KeyPress& key, double step, double fine)
    {
        const auto code = key.getKeyCode();
        double target = 0.0;
        if (code == juce::KeyPress::homeKey)
            target = slider.getDoubleClickReturnValue();
        else
        {
            const auto up = code == juce::KeyPress::upKey || code == juce::KeyPress::rightKey;
            const auto down = code == juce::KeyPress::downKey || code == juce::KeyPress::leftKey;
            if (! up && ! down)
                return false;
            const auto by = key.getModifiers().isShiftDown() ? fine : step;
            target = slider.getValue() + (up ? by : -by);
        }
        if (! slider.isEnabled())
            return true;
        target = juce::jlimit (slider.getMinimum(), slider.getMaximum(), target);
        // One gesture per key, as one drag is: the attachment begins it, writes the value inside it and ends it.
        if (slider.onDragStart != nullptr)
            slider.onDragStart();
        slider.setValue (target, juce::sendNotificationSync);
        if (slider.onDragEnd != nullptr)
            slider.onDragEnd();
        return true;
    }

    //==============================================================================================================
    Knob::Knob()
        : juce::Slider (juce::Slider::RotaryVerticalDrag, juce::Slider::NoTextBox)
    {
        setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        setMouseDragSensitivity (220);
        setPopupMenuEnabled (false);
        setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
        setWantsKeyboardFocus (true);
        setMouseClickGrabsKeyboardFocus (false);
    }

    void Knob::applySkin (const Skin& skin)
    {
        const auto style = skin.fx == FxKind::bars ? juce::Slider::LinearBar : juce::Slider::RotaryVerticalDrag;
        if (getSliderStyle() != style)
        {
            setSliderStyle (style);
            setSliderSnapsToMousePosition (style != juce::Slider::LinearBar);
            setMouseCursor (style == juce::Slider::LinearBar ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::UpDownResizeCursor);
        }
    }

    bool Knob::keyPressed (const juce::KeyPress& key) { return sliderKey (*this, key, 5.0, 1.0); }

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

    //==============================================================================================================
    Fader::Fader()
        : juce::Slider (juce::Slider::LinearVertical, juce::Slider::NoTextBox)
    {
        setSliderSnapsToMousePosition (false); // grab the cap anywhere and move it relative
        setPopupMenuEnabled (false);
        setWantsKeyboardFocus (true);
        setMouseClickGrabsKeyboardFocus (false);
    }

    void Fader::applySkin (const Skin& skin)
    {
        const auto style = skin.lane == LaneKind::stepsAndSlide ? juce::Slider::LinearHorizontal : juce::Slider::LinearVertical;
        if (getSliderStyle() != style)
            setSliderStyle (style);
    }

    bool Fader::keyPressed (const juce::KeyPress& key) { return sliderKey (*this, key, 1.0, 0.1); }

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

    //==============================================================================================================
    PadButton::PadButton (const juce::String& name, const juce::String& letterToShow)
        : juce::Button (name)
    {
        juce::ignoreUnused (letterToShow);
        setClickingTogglesState (true);
    }

    std::vector<DrawnText> PadButton::drawnText() const
    {
        const auto& skin = skinOf (*this);
        return { fitText (skin.type.laneButton, skin.words.mute, getLocalBounds().toFloat().reduced (4.0f, 2.0f), juce::Justification::centred) };
    }

    void PadButton::paintButton (juce::Graphics& g, bool isMouseOver, bool isButtonDown)
    {
        const auto& skin = skinOf (*this);
        const auto& c = skin.colours;
        const auto on = getToggleState();
        const auto area = getLocalBounds().toFloat().reduced (0.5f).translated (0.0f, isButtonDown ? 0.5f : 0.0f);
        const auto fill = on ? c.text : (isMouseOver && isEnabled() ? over (c.lane, c.face).brighter (0.1f) : over (c.lane, c.face));
        paint::box (g, area, skin.shape.laneButton, fill, on ? c.text : c.controlEdge);
        for (const auto& t : drawnText())
            drawText (g, t, on ? over (c.lane, c.face) : (isEnabled() ? c.text : c.faint));
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

    //==============================================================================================================
    LedButton::LedButton (const juce::String& label)
        : juce::Button (label)
    {
        setClickingTogglesState (true);
    }

    void LedButton::setRole (Role r)
    {
        if (kind != r)
        {
            kind = r;
            repaint();
        }
    }

    void LedButton::setRing (float amount)
    {
        amount = juce::jlimit (0.0f, 1.0f, amount);
        if (std::abs (amount - ring) > 0.002f)
        {
            ring = amount;
            repaint();
        }
    }

    namespace
    {
        const TextStyle& labelStyle (const Skin& skin, LedButton::Role role, bool on)
        {
            switch (role)
            {
                case LedButton::Role::headerButton: return skin.type.headerButton;
                case LedButton::Role::modeRow: return skin.type.rowLabel;
                case LedButton::Role::club: return skin.type.fxName;
                case LedButton::Role::transport: return skin.type.transportLine;
                case LedButton::Role::stripSwitch: break;
            }
            return on ? skin.type.stripChip : skin.type.stripChipOff;
        }
    }

    juce::Font LedButton::labelFont() const
    {
        const auto& skin = skinOf (*this);
        const auto& style = labelStyle (skin, kind, true); // the wider of on and off
        return fitText (style, getButtonText(), labelArea()).font;
    }

    juce::Rectangle<float> LedButton::labelArea() const
    {
        const auto area = getLocalBounds().toFloat();
        switch (kind)
        {
            case Role::headerButton: return area.reduced (12.0f, 2.0f);
            case Role::modeRow: return area.reduced (8.0f, 0.0f).withTrimmedRight (area.getWidth() * 0.3f);
            case Role::club: return area.withSizeKeepingCentre (area.getHeight() - 14.0f, 14.0f);
            case Role::stripSwitch: return area.reduced (8.0f, 2.0f);
            case Role::transport: return {};
        }
        return area;
    }

    int LedButton::fitWidth (int height) const
    {
        juce::ignoreUnused (height);
        const auto& skin = skinOf (*this);
        const auto& style = labelStyle (skin, kind, true);
        const auto pad = kind == Role::headerButton ? 24.0f : 16.0f;
        return (int) std::ceil (juce::GlyphArrangement::getStringWidth (style.font(), style.apply (getButtonText())) + pad + 2.0f);
    }

    std::vector<DrawnText> LedButton::drawnText() const
    {
        const auto& skin = skinOf (*this);
        const auto on = getToggleState() && isEnabled();
        std::vector<DrawnText> out;
        switch (kind)
        {
            case Role::transport:
                break;
            case Role::modeRow:
            {
                const auto area = getLocalBounds().toFloat().reduced (8.0f, 0.0f);
                out.push_back (fitText (skin.type.rowLabel, getButtonText(), area.withTrimmedRight (area.getWidth() * 0.32f)));
                out.push_back (fitText (skin.type.rowValue, on ? skin.words.on : skin.words.off, area.withTrimmedLeft (area.getWidth() * 0.68f),
                    juce::Justification::centredRight));
                break;
            }
            case Role::club:
                if (skin.fx == FxKind::bars) // deep glass: a pill toggle, "Club · on"
                    out.push_back (fitText (skin.type.fxName, "Club" + middleDot() + (on ? skin.words.on : skin.words.off),
                        getLocalBounds().toFloat().reduced (10.0f, 0.0f), juce::Justification::centred));
                else
                    out.push_back (fitText (skin.type.fxName, on ? skin.words.on : skin.words.off, labelArea(), juce::Justification::centred));
                break;
            case Role::headerButton:
            case Role::stripSwitch:
                out.push_back (fitText (labelStyle (skin, kind, on), getButtonText(), labelArea(), juce::Justification::centred));
                break;
        }
        return out;
    }

    void LedButton::paintButton (juce::Graphics& g, bool isMouseOver, bool isButtonDown)
    {
        const auto& skin = skinOf (*this);
        const auto& c = skin.colours;
        const auto on = getToggleState() && isEnabled();
        const auto bounds = getLocalBounds().toFloat().reduced (0.5f).translated (0.0f, isButtonDown ? 0.5f : 0.0f);
        const auto hover = isMouseOver && isEnabled();
        switch (kind)
        {
            case Role::headerButton:
            {
                const auto fill = on ? over (c.accent2Tint, c.face) : over (c.panel, c.face).brighter (hover ? 0.06f : 0.0f);
                paint::box (g, bounds, skin.shape.headerButton, fill, on ? c.accent2 : c.controlEdge);
                for (const auto& t : drawnText())
                    drawText (g, t, on ? c.accent2TintText : c.text);
                return;
            }
            case Role::stripSwitch:
            {
                const auto fill = on ? over (c.accent2Tint, c.face) : juce::Colours::transparentBlack;
                paint::box (g, bounds, skin.shape.stripSwitch, fill, on ? c.accent2 : c.controlEdge);
                for (const auto& t : drawnText())
                    drawText (g, t, ! isEnabled() ? c.faint : on ? c.accent2TintText : c.dim);
                return;
            }
            case Role::modeRow:
            {
                if (hover)
                {
                    g.setColour (juce::Colours::white.withAlpha (0.04f));
                    g.fillRoundedRectangle (bounds, 4.0f);
                }
                const auto texts = drawnText();
                drawText (g, texts[0], c.dim);
                drawText (g, texts[1], on ? c.accent2 : c.faint);
                return;
            }
            case Role::club:
            {
                if (skin.fx == FxKind::bars)
                {
                    paint::box (g, bounds, skin.shape.stripSwitch, on ? over (c.accentTint, c.face) : over (c.panel, c.face), on ? c.accent : c.controlEdge);
                    for (const auto& t : drawnText())
                        drawText (g, t, on ? c.accentTintText : c.dim);
                    return;
                }
                const auto size = std::min (bounds.getWidth(), bounds.getHeight());
                const auto circle = bounds.withSizeKeepingCentre (size, size).reduced (3.0f);
                const auto centre = circle.getCentre();
                const auto radius = circle.getWidth() * 0.5f;
                constexpr auto pi = juce::MathConstants<float>::pi;
                juce::Path track;
                track.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, pi * 1.25f, pi * 2.75f, true);
                g.setColour (c.controlEdge);
                g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                if (on && ring > 0.0f)
                {
                    juce::Path arc;
                    arc.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, pi * 1.25f, pi * 1.25f + pi * 1.5f * ring, true);
                    g.setColour (c.accent);
                    g.strokePath (arc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                }
                const auto body = circle.reduced (size * 0.17f);
                g.setColour (on ? over (c.accentTint, c.face) : over (c.well, c.face));
                g.fillEllipse (body);
                g.setColour (on ? c.accent : c.controlEdge);
                g.drawEllipse (body, 1.0f);
                for (const auto& t : drawnText())
                    drawText (g, t, on ? c.accentTintText : c.dim);
                return;
            }
            case Role::transport:
                paintRoundButton (g, skin, bounds, on, on, hover, isEnabled());
                return;
        }
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

    //==============================================================================================================
    AbSwitch::AbSwitch (const juce::String& name)
        : juce::Button (name)
    {
        setClickingTogglesState (false); // clicked () picks the side that was clicked
        setToggleable (true); // accessibility can still flip it
        setWantsKeyboardFocus (true);
        setMouseClickGrabsKeyboardFocus (false);
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

    void AbSwitch::setDisplay (const juce::String& sideA, const juce::String& sideB, bool row)
    {
        if (sideA == shownA && sideB == shownB && row == asRow)
            return;
        shownA = sideA;
        shownB = sideB;
        asRow = row;
        repaint();
    }

    juce::Rectangle<float> AbSwitch::sideArea (int side) const
    {
        const auto area = getLocalBounds().toFloat();
        const auto half = (area.getWidth() - 6.0f) * 0.5f;
        return side == 0 ? area.withWidth (half) : area.withLeft (area.getRight() - half);
    }

    std::vector<DrawnText> AbSwitch::drawnText() const
    {
        const auto& skin = skinOf (*this);
        const auto selected = getToggleState() ? 1 : 0;
        std::vector<DrawnText> out;
        for (int side = 0; side < 2; ++side)
        {
            const auto& style = asRow ? skin.type.abRow : (side == selected ? skin.type.abSideOn : skin.type.abSide);
            out.push_back (fitText (style, side == 0 ? shownA : shownB, sideArea (side).reduced (8.0f, 0.0f), juce::Justification::centred));
        }
        return out;
    }

    void AbSwitch::paintButton (juce::Graphics& g, bool isMouseOver, bool)
    {
        const auto& skin = skinOf (*this);
        const auto& c = skin.colours;
        const auto selected = getToggleState() ? 1 : 0;
        const auto texts = drawnText();
        for (int side = 0; side < 2; ++side)
        {
            const auto area = sideArea (side).reduced (0.5f);
            const auto on = side == selected && isEnabled();
            const auto fill = on ? over (c.accent2Tint, c.face) : over (c.panel, c.face).brighter (isMouseOver ? 0.04f : 0.0f);
            paint::box (g, area, skin.shape.abSide, fill, on ? c.accent2 : c.controlEdge);
            drawText (g, texts[(std::size_t) side], ! isEnabled() ? c.faint : on ? c.accent2TintText : c.dim);
        }
    }

    void AbSwitch::pick (int side)
    {
        if (! isEnabled())
            return;
        setToggleState (side == 1, juce::dontSendNotification);
        if (onClick != nullptr)
            onClick(); // the attachment writes it as a gesture
    }

    bool AbSwitch::keyPressed (const juce::KeyPress& key)
    {
        if (key == juce::KeyPress::returnKey)
            pick (getToggleState() ? 0 : 1);
        else if (key == juce::KeyPress::leftKey)
            pick (0);
        else if (key == juce::KeyPress::rightKey)
            pick (1);
        else
            return false; // Space and the rest are the host's
        return true;
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
        if (pressedSide >= 0)
            setToggleState (pressedSide == 1, juce::dontSendNotification);
        pressedSide = -1;
    }

    //==============================================================================================================
    SegmentedChoice::SegmentedChoice (const juce::String& name, const juce::StringArray& items)
        : juce::ComboBox (name)
    {
        getProperties().set ("segmented", true); // LookAndFeel::positionComboBoxText hides the text box
        addItemList (items, 1);
    }

    void SegmentedChoice::setRole (Role r)
    {
        if (kind != r)
        {
            kind = r;
            repaint();
        }
    }

    const TextStyle& SegmentedChoice::styleFor (bool on) const
    {
        const auto& t = skinOf (*this).type;
        switch (kind)
        {
            case Role::playMode: return on ? t.playMode : t.playModeOff;
            case Role::strip: return on ? t.stripSegmentOn : t.stripSegment;
            case Role::row: break;
        }
        return on ? t.rowValue : t.rowLabel;
    }

    std::vector<DrawnText> SegmentedChoice::drawnText() const
    {
        std::vector<DrawnText> out;
        const auto selected = getSelectedItemIndex();
        for (int i = 0; i < getNumItems(); ++i)
            out.push_back (fitText (styleFor (i == selected), getItemText (i), segmentArea (i).reduced (4.0f, 0.0f), juce::Justification::centred));
        return out;
    }

    void SegmentedChoice::paint (juce::Graphics& g)
    {
        const auto& skin = skinOf (*this);
        const auto& c = skin.colours;
        const auto area = getLocalBounds().toFloat().reduced (0.5f);
        const auto count = getNumItems();
        const auto separate = kind == Role::playMode && skin.shape.segmentTrack.border <= 0.0f;
        if (! separate)
            paint::box (g, area, skin.shape.segmentTrack, over (c.well, c.face), c.controlEdge);
        const auto selected = getSelectedItemIndex();
        const auto texts = drawnText();
        for (int i = 0; i < count; ++i)
        {
            const auto on = i == selected && isEnabled();
            auto cell = segmentArea (i);
            if (separate)
                cell = cell.reduced (3.0f, 0.0f);
            else
                cell = cell.reduced (2.0f);
            if (on)
                paint::box (g, cell, skin.shape.segment, kind == Role::playMode ? c.accent : over (c.accent2Tint, c.face),
                    kind == Role::playMode ? c.accent : c.accent2);
            else if (separate)
                paint::box (g, cell, skin.shape.segment, over (c.pillOff, c.face), c.controlEdge);
            const auto ink = ! isEnabled() ? c.faint : on ? (kind == Role::playMode ? c.accentInk : c.accent2TintText) : c.dim;
            drawText (g, texts[(std::size_t) i], ink);
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
        // The accessibility "press" action and a keyboard Return land here: step to the next item.
        if (const auto count = getNumItems(); count > 0)
            setSelectedItemIndex ((getSelectedItemIndex() + 1) % count, juce::sendNotificationSync);
        hidePopup();
    }

    bool SegmentedChoice::keyPressed (const juce::KeyPress& key)
    {
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
        // Each label's width in the wider of the selected and unselected styles, plus the pad each side.
        std::vector<float> labelWidths (const SegmentedChoice& choice, const TextStyle& on, const TextStyle& offStyle)
        {
            std::vector<float> widths;
            for (int i = 0; i < choice.getNumItems(); ++i)
            {
                const auto text = choice.getItemText (i);
                widths.push_back (std::max (juce::GlyphArrangement::getStringWidth (on.font(), on.apply (text)),
                                      juce::GlyphArrangement::getStringWidth (offStyle.font(), offStyle.apply (text)))
                                  + 2.0f * SegmentedChoice::kLabelPad + 8.0f);
            }
            return widths;
        }
    }

    int SegmentedChoice::fitWidth (int) const
    {
        float total = 1.0f;
        for (const auto w : labelWidths (*this, styleFor (true), styleFor (false)))
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
        const auto widths = labelWidths (*this, styleFor (true), styleFor (false));
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
        return styleFor (true).font();
    }

    //==============================================================================================================
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

    void paintRoundButton (juce::Graphics& g, const Skin& skin, juce::Rectangle<float> area, bool on, bool showStop, bool over, bool enabled)
    {
        const auto& c = skin.colours;
        const auto size = std::min (area.getWidth(), area.getHeight());
        const auto circle = area.withSizeKeepingCentre (size, size).reduced (1.5f);
        if (on)
            paint::glow (g, circle, size * 0.5f, c.accent, 5.0f);
        g.setColour (wubansia::editor::over (c.well, c.face).brighter (over ? 0.08f : 0.0f));
        g.fillEllipse (circle);
        g.setColour (enabled ? c.accent : c.controlEdge);
        g.drawEllipse (circle, 2.0f);
        const auto glyphSize = size * 0.3f;
        const auto glyphBox = circle.withSizeKeepingCentre (glyphSize, glyphSize);
        g.setColour (enabled ? c.accent : c.faint);
        g.fillPath (showStop ? paint::stopGlyph (glyphBox) : paint::playGlyph (glyphBox.translated (glyphSize * 0.08f, 0.0f)));
    }

    void IconButton::paintButton (juce::Graphics& g, bool isMouseOver, bool isButtonDown)
    {
        const auto& skin = skinOf (*this);
        const auto& c = skin.colours;
        const auto on = getToggleState();
        auto area = getLocalBounds().toFloat().reduced (1.0f).translated (0.0f, isButtonDown ? 0.5f : 0.0f);
        if (roundButton)
        {
            paintRoundButton (g, skin, area, on, icon == Icon::stop, isMouseOver, isEnabled());
            return;
        }
        paint::box (g, area, skin.shape.laneButton, on ? c.accent : over (c.panel, c.face).brighter (isMouseOver && isEnabled() ? 0.06f : 0.0f),
            on ? c.accent : c.controlEdge);

        const auto size = juce::jmin (area.getWidth(), area.getHeight()) * 0.42f;
        const auto box = area.withSizeKeepingCentre (size, size);
        const auto colour = ! isEnabled() ? c.faint : (on ? c.accentInk : c.text);
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
