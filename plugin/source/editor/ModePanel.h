#pragma once

// How the kit plays: the play mode and the vox synth (parameters, bound by the editor), and two
// settings kept per project through the Controller: how MIDI in reaches the lanes, and whether phrase
// lanes may play their recorded lines.

#include "Controls.h"
#include "EditorModel.h"

namespace wubansia::editor
{
    class ModePanel : public juce::Component
    {
    public:
        explicit ModePanel (Controller&);

        void update (const EditorModel&);
        void resized() override;
        void paint (juce::Graphics&) override;

        SegmentedChoice playMode { "Play mode" }; // play_mode: its items come from the parameter
        LedButton vox { "Vox synth" }; // vox_synth
        SegmentedChoice midiIn { "MIDI in", { "Multi", "Lane" } };
        juce::ComboBox midiLane { "MIDI in lane" };
        SegmentedChoice phrases { "Phrase lanes", { "Auto", "Off" } };

        // The notes beside the vox, MIDI-in and phrase switches, which follow their settings.
        const juce::StringArray& hints() const noexcept { return hintTexts; }

    private:
        Controller& controller;
        juce::StringArray laneItems;
        juce::StringArray hintTexts { "", "", "", "" };
        std::array<juce::Rectangle<int>, 4> rows;
    };
}
