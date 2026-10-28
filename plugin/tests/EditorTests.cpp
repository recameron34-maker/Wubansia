#include "helpers/test_helpers.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using Catch::Matchers::WithinAbs;
using wubansia::PluginProcessor;

// These open real windows: on Linux without a display, run ctest under `xvfb-run -a`.

TEST_CASE ("editor opens and closes", "[editor]")
{
    PluginProcessor plugin;
    REQUIRE (plugin.hasEditor());

    wubansia::test::withEditor (plugin, [] (juce::AudioProcessorEditor& editor)
        {
            CHECK (editor.getWidth() > 0);
            CHECK (editor.getHeight() > 0);
            CHECK (dynamic_cast<juce::Slider*> (editor.findChildWithID (wubansia::ParameterIds::masterGain)) != nullptr);
        });

    CHECK (plugin.getActiveEditor() == nullptr);
}

TEST_CASE ("gain knob is attached to master_gain", "[editor]")
{
    PluginProcessor plugin;
    wubansia::test::setMasterGainDb (plugin, -6.0f);

    wubansia::test::withEditor (plugin, [&] (juce::AudioProcessorEditor& editor)
        {
            auto* knob = dynamic_cast<juce::Slider*> (editor.findChildWithID (wubansia::ParameterIds::masterGain));
            REQUIRE (knob != nullptr);
            CHECK_THAT (knob->getValue(), WithinAbs (-6.0, 1.0e-3)); // shows the current value

            knob->setValue (-12.0, juce::sendNotificationSync);
            CHECK_THAT (wubansia::test::masterGainDb (plugin), WithinAbs (-12.0, 1.0e-3));
        });
}
