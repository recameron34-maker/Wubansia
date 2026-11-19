#include "LaneStrip.h"
#include "LookAndFeel.h"
#include "ParameterIds.h"

#include <cmath>

namespace wubansia::editor
{
    namespace
    {
        constexpr int meterSegments = 18; // 3 dB each, from -54 dBFS
        constexpr float ledDecay = 0.72f; // per 30 Hz tick: a hit glows for about 100 ms
        constexpr float meterFallDb = 1.0f; // per tick: 30 dB/s
        constexpr int holdFor = 30; // ticks: the peak segment holds for a second
    }

    LaneStrip::LaneStrip (engine::Lane lane, Controller& controller)
        : mute ("Mute", "M", colours::mute),
          chip (controller, { DragWhat::lane, lane, {} }, laneName (lane)),
          laneId (lane)
    {
        const auto index = static_cast<std::size_t> (lane);
        setComponentID ("lane_" + juce::String (std::string (engine::laneId (lane))));
        level.setComponentID (ParameterIds::laneLevel[index]);
        mute.setComponentID (ParameterIds::laneMute[index]);
        chip.setComponentID ("drag_" + juce::String (std::string (engine::laneId (lane))));
        chip.setCompact (true);

        addAndMakeVisible (level);
        addAndMakeVisible (mute);
        addAndMakeVisible (chip);
    }

    void LaneStrip::repaintIfChanged (bool changed, juce::Rectangle<int> area)
    {
        if (changed)
            repaint (area);
    }

    void LaneStrip::update (const EditorModel& model, bool isMuted)
    {
        const auto bit = engine::laneBit (laneId);
        const auto& snapshot = model.snapshot;

        // The name, tag and state: repaint the whole strip when one changes.
        const auto newLabel = model.laneLabel (laneId).toUpperCase();
        const auto newTag = model.isPhraseLane (laneId) ? juce::String (model.phrasePlaysRecorded (laneId) ? "recorded" : "multisample") : juce::String();
        const auto newDimmed = ! model.hasLane (laneId);
        const auto newTarget = model.midiInMode == engine::MidiInMode::lane && model.midiInLane == laneId;
        const auto newValue = level.getTextFromValue (level.getValue()) + " dB";
        if (newLabel != label || newTag != tag || newDimmed != dimmed || isMuted != muted || newTarget != midiTarget || model.accent != accent)
        {
            label = newLabel;
            tag = newTag;
            dimmed = newDimmed;
            muted = isMuted;
            midiTarget = newTarget;
            accent = model.accent;
            setAlpha (dimmed ? 0.32f : 1.0f);
            chip.setEnabled (! dimmed);
            chip.setRequest ({ DragWhat::lane, laneId, {} }, model.laneLabel (laneId));
            chip.setTooltip ("Drag the " + model.laneLabel (laneId) + " lane into your DAW as MIDI, from the "
                             + (model.playMode == engine::PlayMode::song ? "extended mix" : "loop") + ". Click to show the file.");
            setTooltip (dimmed ? model.kit.id + " has no " + laneName (laneId).toLowerCase() + " lane" : juce::String());
            repaint();
        }
        if (newValue != valueText)
        {
            valueText = newValue;
            repaint (valueArea);
        }

        // The hit LED and the meter, which animate.
        const auto newLed = (snapshot.laneHits & bit) != 0 ? 1.0f : (led * ledDecay < 0.02f ? 0.0f : led * ledDecay);
        repaintIfChanged (std::abs (newLed - led) > 0.005f, ledArea.expanded (8));
        led = newLed;

        const auto peak = snapshot.lanePeak[static_cast<std::size_t> (laneId)];
        const auto inDb = peak > 0.0f ? juce::jmax (meterFloorDb, 20.0f * std::log10 (peak)) : meterFloorDb;
        const auto newPeak = juce::jmax (inDb, peakDb - meterFallDb);
        auto newHold = holdDb;
        if (inDb >= holdDb)
        {
            newHold = inDb;
            holdTicks = holdFor;
        }
        else if (holdTicks > 0)
            --holdTicks;
        else
            newHold = juce::jmax (meterFloorDb, holdDb - meterFallDb * 1.5f);
        repaintIfChanged (std::abs (newPeak - peakDb) > 0.05f || std::abs (newHold - holdDb) > 0.05f, meterArea.expanded (3));
        peakDb = newPeak;
        holdDb = newHold;
    }

    void LaneStrip::resized()
    {
        auto area = getLocalBounds().reduced (6, 6);
        labelArea = area.removeFromTop (22);
        ledArea = labelArea.withWidth (14).withSizeKeepingCentre (10, 10).translated (2, 0);
        area.removeFromTop (3);
        tagArea = area.removeFromTop (16).reduced (4, 0);
        area.removeFromTop (6);

        auto buttons = area.removeFromBottom (30);
        area.removeFromBottom (4);
        valueArea = area.removeFromBottom (16);
        area.removeFromBottom (2);

        auto faderColumn = area;
        meterArea = faderColumn.removeFromRight (12).reduced (1, 8);
        faderColumn.removeFromRight (4);
        level.setBounds (faderColumn.withTrimmedLeft (4));

        mute.setBounds (buttons.removeFromLeft (31));
        buttons.removeFromLeft (4);
        chip.setBounds (buttons.reduced (0, 2));
    }

    void LaneStrip::paint (juce::Graphics& g)
    {
        const auto bounds = getLocalBounds().toFloat();
        draw::plate (g, bounds.reduced (0.5f));

        // The name and its hit light.
        draw::led (g, ledArea.toFloat().getCentre(), 4.2f, accent, muted ? led * 0.35f : led);
        auto text = labelArea.toFloat().withTrimmedLeft (18.0f);
        if (midiTarget)
        {
            const auto badge = text.removeFromRight (30.0f).reduced (0.0f, 3.0f);
            g.setColour (accent.withAlpha (0.85f));
            g.drawRoundedRectangle (badge, 3.0f, 1.0f);
            g.setFont (uiFont (8.5f, true, 0.06f));
            g.drawText ("MIDI", badge, juce::Justification::centred, false);
        }
        g.setFont (uiFont (12.5f, true, 0.07f));
        g.setColour (muted ? colours::textFaint : colours::text);
        g.drawFittedText (label, text.toNearestInt(), juce::Justification::centredLeft, 1, 0.75f);

        // The phrase tag.
        if (tag.isNotEmpty())
        {
            const auto recorded = tag == "recorded";
            const auto pill = tagArea.toFloat();
            g.setColour (recorded ? accent.withAlpha (0.22f) : juce::Colours::white.withAlpha (0.05f));
            g.fillRoundedRectangle (pill, pill.getHeight() * 0.5f);
            g.setColour (recorded ? accent.withAlpha (0.9f) : colours::textFaint);
            g.drawRoundedRectangle (pill.reduced (0.5f), pill.getHeight() * 0.5f, 1.0f);
            g.setFont (uiFont (9.0f, true, 0.12f));
            g.setColour (recorded ? accent.brighter (0.3f) : colours::textDim);
            g.drawText (tag.toUpperCase(), pill, juce::Justification::centred, false);
        }

        // The meter, in a recess beside the fader.
        draw::recess (g, meterArea.toFloat().expanded (2.5f), 3.0f);
        const auto fill = (peakDb - meterFloorDb) / -meterFloorDb;
        const auto hold = holdDb > meterFloorDb + 0.5f ? (holdDb - meterFloorDb) / -meterFloorDb : -1.0f;
        draw::meter (g, meterArea.toFloat(), meterSegments, muted ? 0.0f : fill, muted ? -1.0f : hold, accent, true, 16, 17);

        // The fader's value.
        g.setFont (uiFont (11.0f, true, 0.03f));
        g.setColour (muted ? colours::textFaint : colours::textDim);
        g.drawText (valueText, valueArea.toFloat(), juce::Justification::centred, false);
    }

    juce::String LaneStrip::secondLine() const { return {}; }
    std::array<StepCell, 16> LaneStrip::stepCells() const { return {}; }
    int LaneStrip::playingCell() const { return -2; }
}
