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

        for (auto* component : std::initializer_list<juce::Component*> { &playMode, &vox, &midiIn, &midiLane, &phrases })
            addAndMakeVisible (component);
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
            repaint();
        }
    }

    void ModePanel::resized()
    {
        auto area = getLocalBounds().reduced (10, 9);
        constexpr auto gap = 5;
        const auto rowHeight = (area.getHeight() - gap * 3) / 4;
        for (auto& row : rows)
        {
            row = area.removeFromTop (rowHeight);
            area.removeFromTop (gap);
        }

        const auto control = [] (juce::Rectangle<int> row) { return row.withTrimmedLeft (66); };
        playMode.setBounds (control (rows[0]));
        vox.setBounds (control (rows[1]).withWidth (124));
        auto midi = control (rows[2]);
        midiIn.setBounds (midi.removeFromLeft (124));
        midiLane.setBounds (midi.withTrimmedLeft (6));
        phrases.setBounds (control (rows[3]).withWidth (124));
    }

    void ModePanel::paint (juce::Graphics& g)
    {
        const char* labels[] = { "Play", "Vox", "MIDI in", "Phrases" };
        for (std::size_t i = 0; i < rows.size(); ++i)
            paint::engraved (g, labels[i], rows[i].toFloat().withWidth (62.0f), juce::Justification::centredLeft, 9.5f);

        // The notes beside the switches.
        g.setFont (bodyFont (skinOf (*this), 10.5f));
        g.setColour (skinOf (*this).colours.faint);
        for (std::size_t i = 1; i < rows.size(); ++i)
            g.drawText (hintTexts[(int) i], rows[i].withTrimmedLeft (66 + 132).toFloat(), juce::Justification::centredLeft, true);
    }
}
