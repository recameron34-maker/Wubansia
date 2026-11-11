#include "PollingAttachment.h"

#include <cmath>

namespace wubansia::editor
{
    PollingAttachment::PollingAttachment (juce::Slider& s, juce::RangedAudioParameter& p)
        : parameter (p), control (s), slider (&s)
    {
        // The slider moves in the parameter's own units, through the parameter's own mapping.
        auto range = p.getNormalisableRange();
        juce::NormalisableRange<double> sliderRange (
            (double) range.start,
            (double) range.end,
            [range] (double start, double end, double proportion) mutable
            {
                range.start = (float) start;
                range.end = (float) end;
                return (double) range.convertFrom0to1 ((float) proportion);
            },
            [range] (double start, double end, double value) mutable
            {
                range.start = (float) start;
                range.end = (float) end;
                return (double) range.convertTo0to1 ((float) value);
            },
            [range] (double start, double end, double value) mutable
            {
                range.start = (float) start;
                range.end = (float) end;
                return (double) range.snapToLegalValue ((float) value);
            });
        sliderRange.interval = (double) range.interval;
        sliderRange.skew = (double) range.skew;
        sliderRange.symmetricSkew = range.symmetricSkew;
        s.setNormalisableRange (sliderRange);

        s.textFromValueFunction = [&p] (double value) { return p.getText (p.convertTo0to1 ((float) value), 0); };
        s.valueFromTextFunction = [&p] (const juce::String& text) { return (double) p.convertFrom0to1 (p.getValueForText (text)); };
        s.setDoubleClickReturnValue (true, (double) p.convertFrom0to1 (p.getDefaultValue()));
        if (s.getTitle().isEmpty())
            s.setTitle (p.getName (64));

        s.onDragStart = [this] { beginGesture(); };
        s.onDragEnd = [this] { endGesture(); };
        s.onValueChange = [this]
        {
            if (! updating)
                write (parameter.convertTo0to1 ((float) slider->getValue()));
        };
        poll();
    }

    PollingAttachment::PollingAttachment (juce::Button& b, juce::RangedAudioParameter& p)
        : parameter (p), control (b), button (&b)
    {
        if (b.getTitle().isEmpty())
            b.setTitle (p.getName (64));

        b.onClick = [this]
        {
            if (! updating)
                write (button->getToggleState() ? 1.0f : 0.0f);
        };
        poll();
    }

    PollingAttachment::PollingAttachment (juce::ComboBox& c, juce::RangedAudioParameter& p)
        : parameter (p), control (c), comboBox (&c)
    {
        if (c.getNumItems() == 0)
            c.addItemList (p.getAllValueStrings(), 1);
        if (c.getTitle().isEmpty())
            c.setTitle (p.getName (64));

        c.onChange = [this]
        {
            if (updating)
                return;
            const auto index = comboBox->getSelectedItemIndex();
            if (index >= 0)
                write (parameter.convertTo0to1 (parameter.getNormalisableRange().start + (float) index));
        };
        poll();
    }

    PollingAttachment::~PollingAttachment()
    {
        endGesture(); // the editor closed mid-drag: close the host's gesture

        if (slider != nullptr)
        {
            slider->onDragStart = nullptr;
            slider->onDragEnd = nullptr;
            slider->onValueChange = nullptr;
        }
        if (button != nullptr)
            button->onClick = nullptr;
        if (comboBox != nullptr)
            comboBox->onChange = nullptr;
    }

    void PollingAttachment::poll()
    {
        if (inGesture)
            return; // the person's hand is on it

        const auto value = parameter.getValue();
        if (! std::isfinite (value) || juce::exactlyEqual (value, shown))
            return;
        show (value);
    }

    void PollingAttachment::beginGesture()
    {
        if (! inGesture)
        {
            inGesture = true;
            parameter.beginChangeGesture();
        }
    }

    void PollingAttachment::endGesture()
    {
        if (inGesture)
        {
            inGesture = false;
            parameter.endChangeGesture();
        }
    }

    void PollingAttachment::write (float normalised)
    {
        normalised = juce::jlimit (0.0f, 1.0f, normalised);
        shown = normalised;

        if (inGesture)
        {
            parameter.setValueNotifyingHost (normalised);
            return;
        }

        if (juce::exactlyEqual (parameter.getValue(), normalised))
            return; // a click that changes nothing isn't worth an undo step in the host

        parameter.beginChangeGesture();
        parameter.setValueNotifyingHost (normalised);
        parameter.endChangeGesture();
    }

    void PollingAttachment::show (float normalised)
    {
        shown = normalised;
        const juce::ScopedValueSetter<bool> guard (updating, true);

        if (slider != nullptr)
            slider->setValue ((double) parameter.convertFrom0to1 (normalised), juce::dontSendNotification);
        else if (button != nullptr)
            button->setToggleState (normalised >= 0.5f, juce::dontSendNotification);
        else if (comboBox != nullptr)
            comboBox->setSelectedItemIndex (juce::roundToInt (parameter.convertFrom0to1 (normalised) - parameter.getNormalisableRange().start),
                juce::dontSendNotification);
    }
}
