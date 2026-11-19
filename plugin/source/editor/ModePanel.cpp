#include "ModePanel.h"
#include "LookAndFeel.h"
#include "ParameterIds.h"

namespace wubansia::editor
{
    ModePanel::ModePanel (Controller& c)
        : controller (c)
    {
        setComponentID ("mode");

        playMode.setComponentID (ParameterIds::playMode);
        playMode.setTooltip ("Off: only MIDI in plays the kit. Loop: the kit's 4-bar loop, in time with the host. "
                             "Song: the 224-bar extended mix, bar 1 at the start of the host's timeline.");
        vox.setComponentID (ParameterIds::voxSynth);
        vox.setTooltip ("On: the vox synth and its accents. Off: the no-vox arrangement.");

        midiIn.setComponentID ("midi_in_mode");
        midiIn.setTooltip ("Multi: each MIDI channel plays its own lane (the kit's MIDI map). Lane: every channel plays the lane you pick.");
        midiIn.onChange = [this]
        {
            const auto lane = static_cast<engine::Lane> (juce::jmax (0, midiLane.getSelectedItemIndex()));
            controller.setMidiInMode (midiIn.getSelectedItemIndex() == 1 ? engine::MidiInMode::lane : engine::MidiInMode::multi, lane);
        };

        midiLane.setComponentID ("midi_in_lane");
        midiLane.setTooltip ("The lane every incoming MIDI channel plays in Lane mode");
        midiLane.onChange = [this]
        {
            if (const auto index = midiLane.getSelectedItemIndex(); index >= 0)
                controller.setMidiInMode (engine::MidiInMode::lane, static_cast<engine::Lane> (index));
        };

        phrases.setComponentID ("phrase_mode");
        phrases.setTooltip ("Auto: phrase lanes play their recorded lines while the tempo is within 2 % of the kit's, "
                            "and the multisample otherwise. Off: always the multisample.");
        phrases.onChange = [this]
        {
            controller.setPhraseMode (phrases.getSelectedItemIndex() == 1 ? engine::PhraseMode::off : engine::PhraseMode::automatic);
        };

        playMode.setRole (SegmentedChoice::Role::playMode);
        vox.setRole (LedButton::Role::modeRow);
        midiIn.setRole (SegmentedChoice::Role::row);
        phrases.setRole (SegmentedChoice::Role::row);
        int order = 1;
        for (auto* component : std::initializer_list<juce::Component*> { &playMode, &vox, &midiIn, &midiLane, &phrases })
        {
            component->setExplicitFocusOrder (order++);
            addAndMakeVisible (component);
        }
        for (auto* c : std::initializer_list<juce::SettableTooltipClient*> { &playMode, &vox, &midiIn, &phrases })
            baseTips.add (c->getTooltip());
    }

    void ModePanel::update (const EditorModel& model)
    {
        juce::StringArray items;
        for (int lane = 0; lane < engine::kLanes; ++lane)
            items.add (model.laneLabel (static_cast<engine::Lane> (lane)));
        if (items != laneItems)
        {
            laneItems = items;
            midiLane.clear (juce::dontSendNotification);
            midiLane.addItemList (items, 1);
        }

        const auto laneMode = model.midiInMode == engine::MidiInMode::lane;
        midiIn.setSelectedItemIndex (laneMode ? 1 : 0, juce::dontSendNotification);
        midiLane.setSelectedItemIndex (static_cast<int> (model.midiInLane), juce::dontSendNotification);
        midiLane.setEnabled (laneMode);
        midiLane.setVisible (laneMode);
        phrases.setSelectedItemIndex (model.phraseMode == engine::PhraseMode::off ? 1 : 0, juce::dontSendNotification);

        juce::StringArray next {
            "",
            model.vox ? "vox synth and its accents" : "the no-vox arrangement",
            laneMode ? "" : "lanes by MIDI channel",
            model.phraseMode == engine::PhraseMode::off ? juce::String ("always the multisample")
                                                        : "recorded lines near " + juce::String (juce::jmax (0, model.kit.bpm)) + " BPM"
        };
        if (next != hintTexts)
        {
            hintTexts = next;
            // The hints drawn beside the switches before the looks now end their tooltips (ADR 0032).
            int i = 0;
            for (auto* c : std::initializer_list<juce::SettableTooltipClient*> { &playMode, &vox, &midiIn, &phrases })
            {
                const auto& hint = hintTexts[i];
                c->setTooltip (baseTips[i] + (hint.isNotEmpty() ? " Now: " + hint + "." : juce::String()));
                ++i;
            }
        }
    }

    void ModePanel::resized()
    {
        playMode.setBounds (10, 10, 322, 40);
        vox.setBounds (10, 58, 164, 24);
        midiIn.setBounds (228, 58, 104, 24);
        phrases.setBounds (80, 90, 94, 24);
        midiLane.setBounds (182, 90, 150, 24);
    }

    std::vector<DrawnText> ModePanel::drawnText() const
    {
        // "Recorded", not "Phrases": the header's Phrases button is ADR 0031's; the switch keeps its name and ID.
        const auto& skin = skinOf (*this);
        return { fitText (skin.type.rowLabel, "MIDI in", { 182.0f, 58.0f, 44.0f, 24.0f }),
            fitText (skin.type.rowLabel, "Recorded", { 10.0f, 90.0f, 68.0f, 24.0f }) };
    }

    void ModePanel::paint (juce::Graphics& g)
    {
        const auto& c = skinOf (*this).colours;
        for (const auto& t : drawnText())
            drawText (g, t, c.label);
    }
}
