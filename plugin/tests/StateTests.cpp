#include "helpers/test_helpers.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <cstring>

using Catch::Matchers::WithinAbs;
using wubansia::PluginProcessor;
using wubansia::test::masterGainDb;
using wubansia::test::setMasterGainDb;

namespace
{
    juce::MemoryBlock saveState (PluginProcessor& p)
    {
        juce::MemoryBlock block;
        p.getStateInformation (block);
        return block;
    }

    void loadState (PluginProcessor& p, const juce::MemoryBlock& block)
    {
        p.setStateInformation (block.getData(), (int) block.getSize());
    }

    // JUCE's binary XML format (AudioProcessor::copyXmlToBinary), written out by hand so a format
    // change in JUCE can't hide: magic 0x21324356, the text's byte count (both little-endian),
    // then the UTF-8 text and a terminating zero.
    juce::MemoryBlock binaryXml (const char* xmlText)
    {
        const auto textSize = std::strlen (xmlText);
        juce::MemoryOutputStream out;
        out.writeInt ((int) 0x21324356);
        out.writeInt ((int) textSize);
        out.write (xmlText, textSize);
        out.writeByte (0);
        return out.getMemoryBlock();
    }
}

TEST_CASE ("gain survives a round trip", "[state]")
{
    PluginProcessor original;
    setMasterGainDb (original, -12.5f);
    REQUIRE_THAT (masterGainDb (original), WithinAbs (-12.5, 1.0e-4));

    const auto state = saveState (original);
    REQUIRE (state.getSize() > 0);

    PluginProcessor restored;
    REQUIRE_THAT (masterGainDb (restored), WithinAbs (0.0, 1.0e-6));
    loadState (restored, state);
    CHECK_THAT (masterGainDb (restored), WithinAbs (-12.5, 1.0e-4));
}

TEST_CASE ("bad state is ignored", "[state]")
{
    PluginProcessor plugin;
    setMasterGainDb (plugin, -6.0f);
    const auto valid = saveState (plugin);

    SECTION ("empty")
    {
        plugin.setStateInformation (nullptr, 0);
    }

    SECTION ("junk")
    {
        const char junk[] = "not a state";
        plugin.setStateInformation (junk, (int) sizeof (junk));
    }

    SECTION ("truncated")
    {
        PluginProcessor other;
        setMasterGainDb (other, -30.0f);
        const auto full = saveState (other);
        plugin.setStateInformation (full.getData(), (int) full.getSize() / 2);
    }

    SECTION ("wrong root tag")
    {
        loadState (plugin, binaryXml (R"(<OtherPlugin stateVersion="1"><PARAM id="master_gain" value="-30"/></OtherPlugin>)"));
    }

    CHECK_THAT (masterGainDb (plugin), WithinAbs (-6.0, 1.0e-4));
    CHECK (valid.getSize() > 0);
}

TEST_CASE ("non-finite values fall back to the default", "[state]")
{
    PluginProcessor plugin;
    setMasterGainDb (plugin, -6.0f);
    loadState (plugin, binaryXml (R"(<WubansiaState stateVersion="1"><PARAM id="master_gain" value="nan"/></WubansiaState>)"));

    const auto gain = masterGainDb (plugin);
    CHECK (std::isfinite (gain));
    CHECK_THAT (gain, WithinAbs (0.0, 1.0e-6));
}

// A state exactly as M0 saves it, byte for byte. Future versions must keep loading it: migrate, never break.
constexpr auto m0State = R"(<?xml version="1.0" encoding="UTF-8"?> <WubansiaState stateVersion="1"><PARAM id="master_gain" value="-12.5"/></WubansiaState>)";

TEST_CASE ("M0 v1 state still loads", "[state]")
{
    PluginProcessor plugin;
    loadState (plugin, binaryXml (m0State));
    CHECK_THAT (masterGainDb (plugin), WithinAbs (-12.5, 1.0e-4));
}

TEST_CASE ("M0 saves exactly the v1 fixture", "[state]")
{
    // If this fails after a JUCE bump, JUCE changed its XML or number formatting: the fixture
    // above still has to load, so add the new form as a second fixture rather than replacing it.
    PluginProcessor plugin;
    setMasterGainDb (plugin, -12.5f);
    CHECK (saveState (plugin) == binaryXml (m0State));
}

TEST_CASE ("a state from a newer version keeps its version and data", "[state]")
{
    // Opening a newer project in this build and saving it must not stamp it as an older
    // version, or a later migration would misread it.
    PluginProcessor plugin;
    loadState (plugin, binaryXml (R"(<WubansiaState stateVersion="7"><PARAM id="master_gain" value="-3"/><PARAM id="future_param" value="0.5"/></WubansiaState>)"));
    CHECK_THAT (masterGainDb (plugin), WithinAbs (-3.0, 1.0e-4));

    const auto state = saveState (plugin);
    const auto xml = juce::AudioProcessor::getXmlFromBinary (state.getData(), (int) state.getSize());
    REQUIRE (xml != nullptr);
    CHECK (xml->getIntAttribute ("stateVersion") == 7);
    CHECK (xml->getChildByAttribute ("id", "future_param") != nullptr);
}

TEST_CASE ("saved state is versioned", "[state]")
{
    PluginProcessor plugin;
    const auto state = saveState (plugin);
    const auto xml = juce::AudioProcessor::getXmlFromBinary (state.getData(), (int) state.getSize());

    REQUIRE (xml != nullptr);
    CHECK (xml->getTagName() == "WubansiaState");
    CHECK (xml->getIntAttribute ("stateVersion") == 1);
    CHECK (xml->getChildByAttribute ("id", "master_gain") != nullptr);
}

TEST_CASE ("instances are independent", "[state]")
{
    PluginProcessor a, b;
    setMasterGainDb (a, -20.0f);
    CHECK_THAT (masterGainDb (b), WithinAbs (0.0, 1.0e-6));
}
