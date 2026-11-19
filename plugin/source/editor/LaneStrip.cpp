#include "LaneStrip.h"

#include "LookAndFeel.h"
#include "ParameterIds.h"
#include "editor/RepaintLedger.h"
#include "editor/skin/Paint.h"
#include "kit/channel_map.h"
#include "services/PatternView.h"

#include <cmath>

namespace wubansia::editor
{
    namespace
    {
        constexpr float ledDecay = 0.72f; // per 30 Hz tick: a hit glows for about 100 ms
        constexpr float meterFallDb = 1.0f; // per tick: 30 dB/s
        constexpr int holdFor = 30; // ticks: the peak segment holds for a second

        bool isDrum (engine::Lane lane) noexcept { return static_cast<int> (lane) < static_cast<int> (engine::Lane::bass); }

        // The lane's MIDI channel in the kit's files (kit/channel_map.h): the drums on 10, the rest one each.
        int channelOf (engine::Lane lane) noexcept
        {
            using engine::ChannelRole;
            switch (lane)
            {
                case engine::Lane::bass: return engine::channelOf (ChannelRole::bass);
                case engine::Lane::lead: return engine::channelOf (ChannelRole::lead);
                case engine::Lane::chords: return engine::channelOf (ChannelRole::chords);
                case engine::Lane::vox: return engine::channelOf (ChannelRole::vox);
                case engine::Lane::accents: return engine::channelOf (ChannelRole::accents);
                case engine::Lane::fx: return engine::channelOf (ChannelRole::fx);
                default: break;
            }
            return engine::channelOf (ChannelRole::drums);
        }

        // Which A/B switch shows on the lane: kick, bass and the chords' stab.
        int abOf (engine::Lane lane) noexcept
        {
            return lane == engine::Lane::kick ? 0 : lane == engine::Lane::bass ? 1 : lane == engine::Lane::chords ? 2 : -1;
        }
    }

    LaneStrip::LaneStrip (engine::Lane lane, Controller& controller)
        : mute ("Mute", "M"),
          chip (controller, { DragWhat::lane, lane, {} }, laneName (lane)),
          laneId (lane)
    {
        const auto index = static_cast<std::size_t> (lane);
        setComponentID ("lane_" + juce::String (std::string (engine::laneId (lane))));
        level.setComponentID (ParameterIds::laneLevel[index]);
        mute.setComponentID (ParameterIds::laneMute[index]);
        chip.setComponentID ("drag_" + juce::String (std::string (engine::laneId (lane))));
        chip.setCompact (true);
        level.setExplicitFocusOrder (1);
        mute.setExplicitFocusOrder (2);
        addAndMakeVisible (level);
        addAndMakeVisible (mute);
        addAndMakeVisible (chip);
    }

    void LaneStrip::repaintIfChanged (bool changed, juce::Rectangle<int> area)
    {
        if (changed)
            repaintArea (*this, area);
    }

    juce::String LaneStrip::secondLine() const { return skinOf (*this).type.laneSub.apply (sub); }
    std::array<StepCell, 16> LaneStrip::stepCells() const { return cells; }
    int LaneStrip::playingCell() const { return playing; }

    juce::Colour LaneStrip::laneColour (const Skin& skin) const
    {
        if (laneId == engine::Lane::lead && skin.colours.hookLane != skin.colours.music)
            return skin.colours.hookLane;
        return isDrum (laneId) ? skin.colours.drums : skin.colours.music;
    }

    void LaneStrip::lookAndFeelChanged()
    {
        kind = skinOf (*this).lane;
        level.applySkin (skinOf (*this));
        chip.setTint (laneColour (skinOf (*this)));
        resized();
        repaint();
    }

    void LaneStrip::update (const EditorModel& model, bool isMuted)
    {
        const auto& skin = skinOf (*this);
        const auto bit = engine::laneBit (laneId);
        const auto& snapshot = model.snapshot;
        const auto newLabel = model.laneLabel (laneId);
        const auto newTag = model.isPhraseLane (laneId) ? juce::String (model.phrasePlaysRecorded (laneId) ? "recorded" : "multisample") : juce::String();
        const auto newDimmed = ! model.hasLane (laneId);
        const auto newTarget = model.midiInMode == engine::MidiInMode::lane && model.midiInLane == laneId;

        // The second line: mission HUD's channel, the A/B side, REC or MULTI, MIDI IN; machine room leads with the number.
        juce::StringArray parts;
        if (skin.lane == LaneKind::stepsAndSlide)
            parts.add (juce::String (static_cast<int> (laneId) + 1).paddedLeft ('0', 2));
        if (skin.lane == LaneKind::telemetryColumn)
            parts.add ("CH " + juce::String (channelOf (laneId)));
        else if (const auto ab = abOf (laneId); ab >= 0 && model.hasAb (ab))
            parts.add (model.abB[(std::size_t) ab] ? "B" : "A");
        if (newTag.isNotEmpty())
            parts.add (newTag == "recorded" ? "REC" : "MULTI");
        if (newTarget)
            parts.add ("MIDI IN");
        const auto newSub = parts.joinIntoString (middleDot());

        if (newLabel != label || newTag != tag || newDimmed != dimmed || isMuted != muted || newTarget != midiTarget || newSub != sub)
        {
            label = newLabel;
            tag = newTag;
            dimmed = newDimmed;
            muted = isMuted;
            midiTarget = newTarget;
            sub = newSub;
            setAlpha (dimmed ? 0.32f : 1.0f);
            chip.setEnabled (! dimmed);
            chip.setRequest ({ DragWhat::lane, laneId, {} }, model.laneLabel (laneId));
            chip.setTooltip ("Drag the " + model.laneLabel (laneId) + " lane into your DAW as MIDI, from the "
                             + (model.playMode == engine::PlayMode::song ? "extended mix" : "loop") + ". Click to show the file.");
            setTooltip (dimmed ? model.kit.id + " has no " + laneName (laneId).toLowerCase() + " lane" : juce::String());
            repaintArea (*this);
        }

        // The fader's value shows while it's hovered or dragged.
        const auto newValue = level.getTextFromValue (level.getValue()) + " dB";
        const auto show = level.isMouseOverOrDragging();
        if (newValue != valueText || show != showValue)
        {
            valueText = newValue;
            showValue = show;
            repaintIfChanged (true, subArea().toNearestInt().expanded (2));
        }

        const auto newLed = (snapshot.laneHits & bit) != 0 ? 1.0f : (led * ledDecay < 0.02f ? 0.0f : led * ledDecay);
        repaintIfChanged (std::abs (newLed - led) > 0.005f, nameArea().toNearestInt().expanded (4));
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
        repaintIfChanged (std::abs (newPeak - peakDb) > 0.05f || std::abs (newHold - holdDb) > 0.05f, meterRect().expanded (3));
        peakDb = newPeak;
        holdDb = newHold;

        // The playing bar's 16 cells (machine room draws them; the others keep them for when it does).
        std::array<StepCell, 16> nextCells {};
        const auto step = model.patternStep;
        const auto bar = step >= 0 ? step / 16 * 16 : 0;
        if (const auto& p = model.pattern; p != nullptr && p->steps > 0)
            for (const auto& n : p->notesIn (laneId, bar, bar + 16))
                for (auto s = std::max (n.step, bar); s < std::min (n.end(), bar + 16); ++s)
                {
                    auto& cell = nextCells[(std::size_t) (s - bar)];
                    cell = std::max (cell, s == n.step ? (n.velocity >= 100 ? StepCell::accent : StepCell::hit) : StepCell::held);
                }
        const auto nextPlaying = step >= 0 ? step - bar : -1;
        if (nextCells != cells || nextPlaying != playing)
        {
            cells = nextCells;
            playing = nextPlaying;
            if (kind == LaneKind::stepsAndSlide)
                repaintArea (*this, gridRect().expanded (2));
        }
    }

    juce::Rectangle<float> LaneStrip::nameArea() const
    {
        const auto x = (float) (getWidth() - 84) * 0.5f;
        return { x, 10.0f, 84.0f, 16.0f };
    }

    juce::Rectangle<float> LaneStrip::subArea() const
    {
        const auto x = (float) (getWidth() - 84) * 0.5f;
        return { x, 28.0f, 84.0f, 12.0f };
    }

    juce::Rectangle<int> LaneStrip::meterRect() const
    {
        const auto x = (getWidth() - 100) / 2;
        return { x + 26, 48, 10, 148 };
    }

    juce::Rectangle<int> LaneStrip::gridRect() const
    {
        const auto x = (getWidth() - 100) / 2;
        return { x + 10, 44, 80, 80 };
    }

    void LaneStrip::resized()
    {
        const auto x = (getWidth() - 100) / 2;
        level.setBounds (x + 40, 48, 36, 148);
        mute.setBounds (x + 8, 204, 84, 30);
        chip.setBounds (x + 8, 242, 84, 26);
    }

    std::vector<DrawnText> LaneStrip::drawnText() const
    {
        const auto& skin = skinOf (*this);
        std::vector<DrawnText> out;
        auto name = fitText (skin.type.laneName, label, nameArea(), juce::Justification::centred);
        out.push_back (name);
        out.push_back (fitText (skin.type.laneSub, showValue ? valueText : sub, subArea(), juce::Justification::centred));
        return out;
    }

    void LaneStrip::paint (juce::Graphics& g)
    {
        const auto& skin = skinOf (*this);
        const auto& c = skin.colours;
        const auto colour = laneColour (skin);
        const auto texts = drawnText();

        // The name with its hit light left of it.
        const auto& name = texts[0];
        const auto nameWidth = juce::GlyphArrangement::getStringWidth (name.font, name.text);
        const auto dotX = name.area.getCentreX() - nameWidth * 0.5f - 7.0f;
        paint::dot (g, { dotX, name.area.getCentreY() }, 3.0f, colour, led > 0.05f);
        drawText (g, name, c.text);
        drawText (g, texts[1], showValue ? c.text : (midiTarget ? c.accent2 : c.label));

        // The meter beside the fader: 18 segments of 3 dB, the top two hot.
        const auto meter = meterRect().toFloat();
        const auto fill = (peakDb - meterFloorDb) / -meterFloorDb;
        const auto hold = holdDb > meterFloorDb + 0.5f ? (holdDb - meterFloorDb) / -meterFloorDb : -1.0f;
        const auto meterAlpha = muted ? 0.4f : 1.0f;
        paint::meter (g, meter, 18, 1.0f, fill, hold, colour.withMultipliedAlpha (meterAlpha), over (c.meterOff, c.face), true, 16, c.text.withMultipliedAlpha (meterAlpha));
    }
}
