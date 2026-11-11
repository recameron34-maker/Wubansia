#pragma once

// Where playback is. In Song mode: the extended mix's sections in proportion, the one playing lit,
// with a playhead. In Loop mode: sixteen beat lights across the 4-bar loop. Off: a note that only MIDI
// in plays.

#include "EditorModel.h"

#include <juce_gui_basics/juce_gui_basics.h>

namespace wubansia::editor
{
    class SectionStrip : public juce::Component
    {
    public:
        SectionStrip();

        void update (const EditorModel&);
        void paint (juce::Graphics&) override;

        // For tests: the section lit now (-1 for none), and the beat lit in Loop mode (-1 for none).
        int litSection() const noexcept { return shown.section; }
        int litBeat() const noexcept { return shown.beat; }

    private:
        struct State
        {
            engine::PlayMode mode = engine::PlayMode::loop;
            std::vector<SectionInfo> sections;
            int section = -1;
            int beat = -1;
            int playheadPixel = -1;
            bool playing = false;
            juce::Colour accent;

            bool operator== (const State& other) const;
        };
        State shown;
    };
}
