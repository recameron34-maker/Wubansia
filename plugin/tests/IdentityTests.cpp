#include "helpers/test_helpers.h"

#include <catch2/catch_test_macros.hpp>

// Frozen copies of the identity in docs/decisions/0002-plugin-identity.md. These values are
// PERMANENT once anyone saves a DAW project with the plugin: if a check here fails, the change
// that broke it is wrong, not the test. The real values come from cmake/WubansiaIdentity.cmake.

#if ! defined(JUCE_VST3_CAN_REPLACE_VST2) || JUCE_VST3_CAN_REPLACE_VST2 != 0
    #error "JUCE_VST3_CAN_REPLACE_VST2 must be defined as 0 (docs/decisions/0002-plugin-identity.md)"
#endif

#if ! defined(JUCE_FORCE_USE_LEGACY_PARAM_IDS) || JUCE_FORCE_USE_LEGACY_PARAM_IDS != 0
    #error "JUCE_FORCE_USE_LEGACY_PARAM_IDS must be defined as 0 (docs/decisions/0002-plugin-identity.md)"
#endif

TEST_CASE ("identity matches ADR 0002", "[identity]")
{
    wubansia::PluginProcessor plugin;

    CHECK (plugin.getName() == "Wubansia");
    CHECK (juce::String (JucePlugin_Manufacturer) == "Wubansia Audio");
    CHECK (JucePlugin_ManufacturerCode == 0x57756261); // 'Wuba'
    CHECK (JucePlugin_PluginCode == 0x57626e73); // 'Wbns'

    // Stringified: the AU type is a multi-character literal and the bundle ID is unquoted.
    CHECK (juce::String (JUCE_STRINGIFY (JucePlugin_AUMainType)) == "'aumu'");
    CHECK (juce::String (JUCE_STRINGIFY (JucePlugin_CFBundleIdentifier)) == "com.wubansiaaudio.wubansia");
    CHECK (juce::String (WUBANSIA_TEST_CLAP_ID) == "com.wubansiaaudio.wubansia");

    CHECK (JucePlugin_IsSynth == 1);
    CHECK (JucePlugin_WantsMidiInput == 1);
    CHECK (JucePlugin_ProducesMidiOutput == 0);
    CHECK (JucePlugin_IsMidiEffect == 0);
}

TEST_CASE ("bus layout", "[identity]")
{
    wubansia::PluginProcessor plugin;

    CHECK (plugin.getBusCount (true) == 0);
    CHECK (plugin.getBusCount (false) == 1);
    CHECK (plugin.getTotalNumOutputChannels() == 2);
    CHECK (plugin.acceptsMidi());
    CHECK_FALSE (plugin.producesMidi());
    CHECK_FALSE (plugin.isMidiEffect());

    juce::AudioProcessor::BusesLayout layout;
    layout.outputBuses.add (juce::AudioChannelSet::mono());
    CHECK (plugin.checkBusesLayoutSupported (layout));
    layout.outputBuses.getReference (0) = juce::AudioChannelSet::stereo();
    CHECK (plugin.checkBusesLayoutSupported (layout));
    layout.outputBuses.getReference (0) = juce::AudioChannelSet::create5point1();
    CHECK_FALSE (plugin.checkBusesLayoutSupported (layout));

    SECTION ("one named program")
    {
        CHECK (plugin.getNumPrograms() == 1);
        CHECK (plugin.getProgramName (0).isNotEmpty());
    }
}
