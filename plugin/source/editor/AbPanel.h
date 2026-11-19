#pragma once

// The kit's A/B choices: kick, bass and stab, each named the kit's way ("Kick Split" / "Kick Garage").
// A switch the kit has no alternative for says so and is disabled.

#include "Controls.h"
#include "EditorModel.h"

namespace wubansia::editor
{
    class AbPanel : public juce::Component
    {
    public:
        AbPanel();

        void update (const EditorModel&);
        void resized() override;
        void paint (juce::Graphics&) override;

        AbSwitch& abSwitch (int index) noexcept { return *switches[(std::size_t) index]; }

        // What a switch's row shows (ADR 0032), as drawn: its title (or row label) and its two sides.
        struct Display
        {
            juce::String title, a, b;
        };
        Display display (int index) const;

    private:
        std::array<std::unique_ptr<AbSwitch>, engine::kAbSwitches> switches;
        std::array<juce::Rectangle<int>, engine::kAbSwitches> rows;
    };
}
