#include "FxPanel.h"
#include "LookAndFeel.h"
#include "ParameterIds.h"
#include "Parameters.h"

#include <cmath>

namespace wubansia::editor
{
    namespace
    {
        constexpr float grScaleDb = 8.0f; // the meter's full scale
        constexpr int grSegments = 16;
        constexpr std::array<const char*, 4> columnNames { "Space", "Echo", "Pump", "Output" };
    }

    FxPanel::FxPanel()
    {
        setComponentID ("fx");
        space.setComponentID (ParameterIds::space);
        space.setTooltip ("Space: the kit's reverb return (and its resonator), 0-200 % of the mix the loops were made with");
        echo.setComponentID (ParameterIds::echo);
        echo.setTooltip ("Echo: the kit's dotted-eighth echo return, 0-200 % of the loops' mix");
        pump.setComponentID (ParameterIds::pump);
        pump.setTooltip ("Pump: how deep the kick-keyed ducks breathe, 0-200 % of the loops' mix");
        club.setComponentID (ParameterIds::clubMaster);
        club.setTooltip ("Club master: the kit's glue, drive, clip and true-peak limiter, as on the club loops. "
                         "Off plays the mix unmastered, with the same latency.");
        output.setComponentID (ParameterIds::masterGain);
        output.setTooltip ("Output gain, after the club master");

        for (auto* component : std::initializer_list<juce::Component*> { &space, &echo, &pump, &club, &output })
            addAndMakeVisible (component);
    }

    void FxPanel::update (const EditorModel& model)
    {
        const std::array<juce::String, 4> next {
            space.getTextFromValue (space.getValue()) + " %",
            echo.getTextFromValue (echo.getValue()) + " %",
            pump.getTextFromValue (pump.getValue()) + " %",
            Parameters::levelText ((float) output.getValue()) + " dB"
        };
        for (std::size_t i = 0; i < next.size(); ++i)
            if (next[i] != values[i])
            {
                values[i] = next[i];
                repaint (columns[i].value);
            }

        if (model.accent != accent || club.getToggleState() != clubOn)
        {
            accent = model.accent;
            clubOn = club.getToggleState();
            repaint();
        }

        // The gain reduction falls back at 12 dB/s, and its peak holds for a second.
        const auto incoming = clubOn ? juce::jmax (0.0f, model.snapshot.gainReductionDb) : 0.0f;
        const auto shown = juce::jmax (incoming, grDb - 0.4f);
        auto hold = grHoldDb;
        if (incoming >= grHoldDb)
        {
            hold = incoming;
            grHoldTicks = 30;
        }
        else if (grHoldTicks > 0)
            --grHoldTicks;
        else
            hold = juce::jmax (0.0f, grHoldDb - 0.6f);

        if (std::abs (shown - grDb) > 0.02f || std::abs (hold - grHoldDb) > 0.02f)
        {
            grDb = shown;
            grHoldDb = hold;
            repaint (grArea.getUnion (grText).expanded (3));
        }
    }

    void FxPanel::resized()
    {
        auto area = getLocalBounds().reduced (12, 8);
        area.removeFromTop (16);

        const auto column = [] (juce::Rectangle<int> c, juce::Slider& knob, Column& out)
        {
            out.label = c.removeFromTop (12);
            out.value = c.removeFromBottom (14);
            knob.setBounds (c.withSizeKeepingCentre (juce::jmin (c.getWidth(), c.getHeight()), juce::jmin (c.getWidth(), c.getHeight())));
        };

        column (area.removeFromLeft (88), space, columns[0]);
        column (area.removeFromLeft (88), echo, columns[1]);
        column (area.removeFromLeft (88), pump, columns[2]);
        column (area.removeFromRight (104), output, columns[3]);

        area.reduce (18, 0);
        clubArea = area;
        auto c = area.withTrimmedTop (6);
        club.setBounds (c.removeFromTop (30).withWidth (juce::jmin (176, c.getWidth())));
        c.removeFromTop (12);
        auto meterRow = c.removeFromTop (14);
        grText = meterRow.removeFromRight (64);
        grArea = meterRow.withTrimmedLeft (26).withTrimmedRight (6);
    }

    void FxPanel::paint (juce::Graphics& g)
    {
        paint::plate (g, getLocalBounds().toFloat().reduced (0.5f), "FX" + middleDot() + "Master");

        for (std::size_t i = 0; i < columns.size(); ++i)
        {
            paint::engraved (g, columnNames[i], columns[i].label.toFloat(), juce::Justification::centred, 9.5f);
            g.setFont (bodyFont (skinOf (*this), 11.0f, true, 0.03f));
            g.setColour (skinOf (*this).colours.dim);
            g.drawText (values[i], columns[i].value.toFloat(), juce::Justification::centred, false);
        }

        // A hairline between the macros and the master section.
        const auto x = (float) clubArea.getX() - 9.0f;
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.fillRect (juce::Rectangle<float> (x, (float) clubArea.getY() + 2.0f, 1.0f, (float) clubArea.getHeight() - 4.0f));
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.fillRect (juce::Rectangle<float> (x + 1.0f, (float) clubArea.getY() + 2.0f, 1.0f, (float) clubArea.getHeight() - 4.0f));

        // The club master's gain reduction.
        paint::engraved (g, "GR", grArea.toFloat().withX ((float) grArea.getX() - 26.0f).withWidth (22.0f), juce::Justification::centredLeft, 9.5f);
        paint::recess (g, grArea.toFloat().expanded (3.0f, 2.5f), 3.0f);
        paint::meter (g, grArea.toFloat(), grSegments, clubOn ? grDb / grScaleDb : 0.0f, clubOn && grHoldDb > 0.05f ? grHoldDb / grScaleDb : -1.0f,
            accent, false);
        g.setFont (bodyFont (skinOf (*this), 11.0f, true, 0.03f));
        g.setColour (clubOn ? skinOf (*this).colours.dim : skinOf (*this).colours.faint);
        const auto reduction = grDb >= 0.05f ? "-" + juce::String (grDb, 1) + " dB" : juce::String ("0.0 dB");
        g.drawText (clubOn ? reduction : juce::String ("BYPASS"), grText.toFloat(), juce::Justification::centredRight, false);
    }
}
