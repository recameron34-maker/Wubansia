#include "AbPanel.h"
#include "LookAndFeel.h"
#include "ParameterIds.h"

namespace wubansia::editor
{
    namespace
    {
        constexpr std::array<const char*, engine::kAbSwitches> rowNames { "Kick", "Bass", "Stab" };
    }

    AbPanel::AbPanel()
    {
        setComponentID ("ab");
        for (std::size_t i = 0; i < switches.size(); ++i)
        {
            switches[i] = std::make_unique<AbSwitch> (juce::String (rowNames[i]) + " A/B");
            switches[i]->setComponentID (ParameterIds::ab[i]);
            addAndMakeVisible (*switches[i]);
        }
    }

    void AbPanel::update (const EditorModel& model)
    {
        for (std::size_t i = 0; i < switches.size(); ++i)
        {
            auto& s = *switches[i];
            const auto available = model.hasAb ((int) i);
            s.setLabels (model.kit.abA[i], model.kit.abB[i]);
            s.setEnabled (available);
        }
    }

    void AbPanel::resized()
    {
        auto area = getLocalBounds().reduced (12, 8);
        area.removeFromTop (18);
        constexpr auto gap = 5;
        const auto rowHeight = (area.getHeight() - gap * 2) / 3;
        for (std::size_t i = 0; i < rows.size(); ++i)
        {
            rows[i] = area.removeFromTop (rowHeight);
            area.removeFromTop (gap);
            switches[i]->setBounds (rows[i].withTrimmedLeft (44));
        }
    }

    void AbPanel::paint (juce::Graphics& g)
    {
        draw::plate (g, getLocalBounds().toFloat().reduced (0.5f), "A / B sounds");
        for (std::size_t i = 0; i < rows.size(); ++i)
            draw::engraved (g, rowNames[i], rows[i].toFloat().withWidth (42.0f), juce::Justification::centredLeft, 9.5f,
                switches[i]->isEnabled() ? colours::engraved : colours::textFaint);
    }
}
