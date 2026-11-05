#pragma once

// The macros and the master: Space, Echo and Pump (0-200 %), the club master with its gain reduction
// meter, and the output gain (master_gain).

#include "Controls.h"
#include "EditorModel.h"

namespace wubansia::editor
{
    class FxPanel : public juce::Component
    {
    public:
        FxPanel();

        void update (const EditorModel&);
        void resized() override;
        void paint (juce::Graphics&) override;

        Knob space, echo, pump; // space, echo, pump
        LedButton club { "Club master" }; // club_master
        Knob output; // master_gain

        float shownGainReductionDb() const noexcept { return grDb; }

    private:
        struct Column
        {
            juce::Rectangle<int> label, value;
        };
        std::array<Column, 4> columns; // space, echo, pump, output
        juce::Rectangle<int> clubArea, grArea, grText;
        std::array<juce::String, 4> values;
        float grDb = 0.0f, grHoldDb = 0.0f;
        int grHoldTicks = 0;
        bool clubOn = true;
        juce::Colour accent;
    };
}
