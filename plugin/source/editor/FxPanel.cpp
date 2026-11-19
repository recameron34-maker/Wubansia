#include "FxPanel.h"

#include "LookAndFeel.h"
#include "ParameterIds.h"
#include "Parameters.h"
#include "editor/RepaintLedger.h"

#include <cmath>

namespace wubansia::editor
{
    namespace
    {
        constexpr float grScaleDb = 8.0f; // the club ring's full scale, as the meter's was
        constexpr std::array<const char*, 5> columnNames { "Space", "Echo", "Pump", "Club", "Output" };
        constexpr float column = 146.0f;
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
        club.setRole (LedButton::Role::club);
        output.setComponentID (ParameterIds::masterGain);
        output.setTooltip ("Output gain, after the club master");
        int order = 1;
        for (auto* component : std::initializer_list<juce::Component*> { &space, &echo, &pump, &club, &output })
        {
            component->setExplicitFocusOrder (order++);
            addAndMakeVisible (component);
        }
    }

    void FxPanel::lookAndFeelChanged()
    {
        const auto& skin = skinOf (*this);
        for (auto* knob : { &space, &echo, &pump, &output })
        {
            knob->applySkin (skin);
            knob->getProperties().remove ("colour");
        }
        if (skin.fx == FxKind::bars) // deep glass: Pump's bar in the music's colour, as the board's
            pump.getProperties().set ("colour", (juce::int64) skin.colours.accent2.getARGB());
        resized();
        repaint();
    }

    void FxPanel::update (const EditorModel& model)
    {
        clubOn = club.getToggleState();
        const auto incoming = clubOn ? juce::jmax (0.0f, model.snapshot.gainReductionDb) : 0.0f;
        const auto shownDb = juce::jmax (incoming, grDb - 0.4f);
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
        if (std::abs (shownDb - grDb) > 0.02f || std::abs (hold - grHoldDb) > 0.02f)
        {
            grDb = shownDb;
            grHoldDb = hold;
            club.setRing (grDb / grScaleDb);
            if (skinOf (*this).fx == FxKind::bars)
                repaintArea (*this, { 584, 58, 82, 14 });
        }

        const std::array<juce::String, 5> next {
            space.getTextFromValue (space.getValue()) + " %",
            echo.getTextFromValue (echo.getValue()) + " %",
            pump.getTextFromValue (pump.getValue()) + " %",
            clubOn ? (grDb > 0.05f ? "-" + juce::String (grDb, 1) + " dB" : juce::String ("0.0 dB")) : juce::String ("BYPASS"),
            Parameters::levelText ((float) output.getValue()) + " dB"
        };
        auto shownValues = next;
        if (skinOf (*this).id == SkinId::missionHud) // inside the ring: the number alone
            for (const auto i : { 0, 1, 2 })
                shownValues[(std::size_t) i] = space.getTextFromValue (i == 0 ? space.getValue() : i == 1 ? echo.getValue() : pump.getValue());
        for (std::size_t i = 0; i < next.size(); ++i)
            if (shownValues[i] != values[i])
            {
                values[i] = shownValues[i];
                repaintArea (*this, valueArea ((int) i).toNearestInt().expanded (2));
            }
    }

    namespace
    {
        // Deep glass's rows: Space, Echo and Pump at the left, Output and the club at the right.
        constexpr std::array<float, 5> barRowY { 14.0f, 44.0f, 74.0f, 50.0f, 14.0f };
    }

    juce::Rectangle<float> FxPanel::nameArea (int i) const
    {
        if (skinOf (*this).fx == FxKind::bars)
            return i == 4 ? juce::Rectangle<float> (476.0f, barRowY[4], 64.0f, 22.0f) : i == 3 ? juce::Rectangle<float>() : juce::Rectangle<float> (18.0f, barRowY[(std::size_t) i], 64.0f, 22.0f);
        return { 4.0f + column * (float) i, 74.0f, column - 8.0f, 14.0f };
    }
    juce::Rectangle<float> FxPanel::valueArea (int i) const
    {
        if (skinOf (*this).fx == FxKind::bars)
            return i == 4 ? juce::Rectangle<float> (668.0f, barRowY[4], 44.0f, 22.0f) : i == 3 ? juce::Rectangle<float> (668.0f, 50.0f, 44.0f, 30.0f)
                                                             : juce::Rectangle<float> (414.0f, barRowY[(std::size_t) i], 44.0f, 22.0f);
        // Mission HUD writes a knob's value inside its ring.
        if (skinOf (*this).id == SkinId::missionHud && i != 3)
        {
            const auto centre = juce::Point<float> (column * ((float) i + 0.5f) + 4.0f, 12.0f + skinOf (*this).shape.knob * 0.5f);
            return juce::Rectangle<float> (40.0f, 14.0f).withCentre (centre);
        }
        return { 4.0f + column * (float) i, 90.0f, column - 8.0f, 13.0f };
    }

    void FxPanel::resized()
    {
        const auto& skin = skinOf (*this);
        if (skin.fx == FxKind::bars)
        {
            space.setBounds (88, 14, 320, 22);
            echo.setBounds (88, 44, 320, 22);
            pump.setBounds (88, 74, 320, 22);
            output.setBounds (544, 14, 118, 22);
            club.setBounds (476, 50, 100, 30);
            return;
        }
        const auto size = (int) skin.shape.knob;
        int i = 0;
        for (juce::Component* c : std::initializer_list<juce::Component*> { &space, &echo, &pump, &club, &output })
        {
            const auto centreX = (int) (column * ((float) i + 0.5f)) + 4;
            c->setBounds (juce::Rectangle<int> (size, size).withCentre ({ centreX, 12 + size / 2 }));
            ++i;
        }
    }

    std::vector<DrawnText> FxPanel::drawnText() const
    {
        const auto& skin = skinOf (*this);
        std::vector<DrawnText> out;
        const auto bars = skin.fx == FxKind::bars;
        for (int i = 0; i < 5; ++i)
        {
            if (! (bars && i == 3)) // the club's pill says its own name
                out.push_back (fitText (skin.type.fxName, columnNames[(std::size_t) i], nameArea (i), bars ? juce::Justification::centredLeft : juce::Justification::centred));
            out.push_back (fitText (skin.type.fxValue, values[(std::size_t) i], valueArea (i), bars ? juce::Justification::centredRight : juce::Justification::centred));
        }
        return out;
    }

    void FxPanel::paint (juce::Graphics& g)
    {
        const auto& skin = skinOf (*this);
        const auto& c = skin.colours;
        if (skin.fx == FxKind::bars)
        {
            // The club's gain reduction beside its pill: 0-8 dB.
            const juce::Rectangle<float> gr { 586.0f, 62.0f, 76.0f, 6.0f };
            g.setColour (over (c.meterOff, c.face));
            g.fillRoundedRectangle (gr, 3.0f);
            g.setColour (c.accent2);
            g.fillRoundedRectangle (gr.withWidth (gr.getWidth() * juce::jlimit (0.0f, 1.0f, grDb / grScaleDb)), 3.0f);
        }
        const auto texts = drawnText(); // per column: its name (but deep glass's club), then its value
        std::size_t k = 0;
        for (int i = 0; i < 5; ++i)
        {
            if (! (skin.fx == FxKind::bars && i == 3))
                drawText (g, texts[k++], c.text);
            drawText (g, texts[k++], c.dim);
        }
    }
}
