#pragma once

// The kit's A/B choices: kick, bass and stab, each named the kit's way ("Kick Split" / "Kick Garage").
// A switch the kit has no alternative for says so and is disabled.

#include "Controls.h"
#include "EditorModel.h"
#include "editor/DrawnText.h"

namespace wubansia::editor
{
    // What one switch shows when it's the kit's only one (ADR 0032, section 3.8): when either side's "A · <label>" is
    // too wide for room in style, the words both labels start with move to the title ("ROLL BASS · A/B", "A · CLEAN");
    // otherwise the title is the lane's label. Before the look's capitals.
    struct AbDisplayWords
    {
        juce::String title, a, b;
    };
    AbDisplayWords abDisplay (const juce::String& a, const juce::String& b, const juce::String& laneLabel, const TextStyle& style, float room);

    class AbPanel : public juce::Component, public DrawnTextSource
    {
    public:
        AbPanel();

        void update (const EditorModel&);
        void resized() override;
        void paint (juce::Graphics&) override;
        std::vector<DrawnText> drawnText() const override;

        AbSwitch& abSwitch (int index) noexcept { return *switches[(std::size_t) index]; }

        // What a switch's row shows (ADR 0032), as drawn: its title (or row label) and its two sides.
        struct Display
        {
            juce::String title, a, b;
        };
        Display display (int index) const;

    private:
        std::array<std::unique_ptr<AbSwitch>, engine::kAbSwitches> switches;
        std::array<Display, engine::kAbSwitches> shown;
        std::array<bool, engine::kAbSwitches> available {};
        int count = 0;
    };
}
