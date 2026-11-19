#pragma once

// The macros and the master: Space, Echo and Pump (0-200 %), the club master with its gain reduction
// meter, and the output gain (master_gain).

#include "Controls.h"
#include "EditorModel.h"
#include "editor/DrawnText.h"

namespace wubansia::editor
{
    class FxPanel : public juce::Component, public DrawnTextSource
    {
    public:
        FxPanel();

        void update (const EditorModel&);
        void resized() override;
        void paint (juce::Graphics&) override;
        std::vector<DrawnText> drawnText() const override;
        void lookAndFeelChanged() override;

        Knob space, echo, pump; // space, echo, pump
        LedButton club { "Club master" }; // club_master
        Knob output; // master_gain

        float shownGainReductionDb() const noexcept { return grDb; }

    private:
        juce::Rectangle<float> nameArea (int column) const;
        juce::Rectangle<float> valueArea (int column) const;

        std::array<juce::String, 5> values; // space, echo, pump, club, output
        float grDb = 0.0f, grHoldDb = 0.0f;
        int grHoldTicks = 0;
        bool clubOn = true;
    };
}
