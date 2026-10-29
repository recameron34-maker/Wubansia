#include "helpers/test_helpers.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using Catch::Matchers::WithinAbs;

TEST_CASE ("parameter IDs are frozen", "[parameters]")
{
    wubansia::PluginProcessor plugin;

    // Every parameter ID ever released, in order. IDs are PERMANENT: add new ones at the
    // end, rename labels freely, never change or remove an ID (CLAUDE.md).
    const juce::StringArray releasedIds { "master_gain" };

    juce::StringArray ids;
    for (auto* parameter : plugin.getParameters())
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameter))
            ids.add (withId->getParameterID());

    CHECK (plugin.getParameters().size() == releasedIds.size());
    CHECK (ids == releasedIds);
}

TEST_CASE ("master gain parameter", "[parameters]")
{
    wubansia::PluginProcessor plugin;
    auto& gain = wubansia::test::masterGainParameter (plugin);

    CHECK (gain.getVersionHint() == 1);
    CHECK (gain.getName (64) == "Gain");
    CHECK (gain.getLabel() == "dB");
    CHECK (gain.isAutomatable());

    const auto& range = gain.getNormalisableRange();
    CHECK (juce::exactlyEqual (range.start, -60.0f));
    CHECK (juce::exactlyEqual (range.end, 6.0f));
    CHECK_THAT (gain.convertFrom0to1 (gain.getDefaultValue()), WithinAbs (0.0, 1.0e-6));
    CHECK_THAT (wubansia::test::masterGainDb (plugin), WithinAbs (0.0, 1.0e-6));
}
