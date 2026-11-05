// The parameters (Parameters.cpp, ADR 0018): the frozen IDs and their order, each parameter's name,
// label, range and default, text that round-trips, and the controls the audio thread reads from them.
//
// The cases run on every processor type in ParameterProcessors: the test processor that holds the
// layout, and PluginProcessor, which builds its APVTS from Parameters::createLayout () (P9).

#include "PluginProcessor.h"
#include "helpers/test_processor.h"

#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <string>

#define ParameterProcessors wubansia::test::TestProcessor, wubansia::PluginProcessor

using Catch::Matchers::WithinAbs;
namespace ids = wubansia::ParameterIds;

namespace
{
    juce::RangedAudioParameter* findParameter (juce::AudioProcessor& processor, const juce::String& id)
    {
        for (auto* parameter : processor.getParameters())
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter))
                if (ranged->getParameterID() == id)
                    return ranged;
        return nullptr;
    }

    void set (juce::AudioProcessor& processor, const juce::String& id, float plainValue)
    {
        auto* parameter = findParameter (processor, id);
        REQUIRE (parameter != nullptr);
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (plainValue));
    }

    enum class Kind { level, percent, toggle, choice };

    struct Expected
    {
        juce::String id, name, label;
        Kind kind;
        float defaultValue; // dB, %, 0 or 1, or the choice index
        juce::StringArray choices;
    };

    // Every v0.1 parameter, as approved: version hint 2, appended after master_gain in this order.
    std::vector<Expected> approved()
    {
        std::vector<Expected> list {
            { "play_mode", "Play Mode", "", Kind::choice, 1.0f, { "Off", "Loop", "Song" } },
            { "vox_synth", "Vox Synth", "", Kind::toggle, 1.0f, {} },
            { "kick_ab", "Kick A/B", "", Kind::choice, 1.0f, { "A", "B" } },
            { "bass_ab", "Bass A/B", "", Kind::choice, 1.0f, { "A", "B" } },
            { "stab_ab", "Stab A/B", "", Kind::choice, 0.0f, { "A", "B" } },
        };

        const char* lanes[] = { "kick", "snare", "hats", "perc", "bass", "lead", "chords", "vox", "accents", "fx" };
        const char* names[] = { "Kick", "Snare", "Hats", "Perc", "Bass", "Lead", "Chords", "Vox", "Accents", "FX" };
        for (int lane = 0; lane < 10; ++lane)
        {
            list.push_back ({ juce::String (lanes[lane]) + "_level", juce::String (names[lane]) + " Level", "dB", Kind::level, 0.0f, {} });
            list.push_back ({ juce::String (lanes[lane]) + "_mute", juce::String (names[lane]) + " Mute", "", Kind::toggle, 0.0f, {} });
        }

        list.push_back ({ "space", "Space", "%", Kind::percent, 100.0f, {} });
        list.push_back ({ "echo", "Echo", "%", Kind::percent, 100.0f, {} });
        list.push_back ({ "pump", "Pump", "%", Kind::percent, 100.0f, {} });
        list.push_back ({ "club_master", "Club Master", "", Kind::toggle, 1.0f, {} });
        return list;
    }
}

TEMPLATE_TEST_CASE ("parameter IDs are frozen", "[parameters]", ParameterProcessors)
{
    TestType plugin;

    // Every parameter ID ever released, in order. IDs are PERMANENT: add new ones at the end, rename
    // labels freely, never change or remove an ID (CLAUDE.md).
    const juce::StringArray releasedIds {
        "master_gain", // M0
        "play_mode", "vox_synth", "kick_ab", "bass_ab", "stab_ab", // v0.1
        "kick_level", "kick_mute", "snare_level", "snare_mute", "hats_level", "hats_mute",
        "perc_level", "perc_mute", "bass_level", "bass_mute", "lead_level", "lead_mute",
        "chords_level", "chords_mute", "vox_level", "vox_mute", "accents_level", "accents_mute",
        "fx_level", "fx_mute",
        "space", "echo", "pump", "club_master"
    };

    juce::StringArray processorIds;
    for (auto* parameter : plugin.getParameters())
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameter))
            processorIds.add (withId->getParameterID());

    juce::StringArray headerIds;
    for (const auto* id : ids::all)
        headerIds.add (id);

    CHECK (releasedIds.size() == 30);
    CHECK (plugin.getParameters().size() == releasedIds.size());
    CHECK (processorIds == releasedIds);
    CHECK (headerIds == releasedIds);
}

TEST_CASE ("parameter IDs follow the engine's lanes and A/B switches", "[parameters]")
{
    for (int lane = 0; lane < wubansia::engine::kLanes; ++lane)
    {
        const std::string laneId (wubansia::engine::kLaneIds[(size_t) lane]);
        CHECK (std::string (ids::laneLevel[(size_t) lane]) == laneId + "_level");
        CHECK (std::string (ids::laneMute[(size_t) lane]) == laneId + "_mute");
    }

    for (int i = 0; i < wubansia::engine::kAbSwitches; ++i)
        CHECK (std::string (ids::ab[(size_t) i]) == std::string (wubansia::engine::kAbSwitchIds[(size_t) i]) + "_ab");
}

TEMPLATE_TEST_CASE ("master gain parameter is M0's, unchanged", "[parameters]", ParameterProcessors)
{
    TestType plugin;
    auto* gain = findParameter (plugin, "master_gain");
    REQUIRE (gain != nullptr);
    REQUIRE (dynamic_cast<juce::AudioParameterFloat*> (gain) != nullptr);

    CHECK (gain->getVersionHint() == 1);
    CHECK (gain->getName (64) == "Gain");
    CHECK (gain->getLabel() == "dB");
    CHECK (gain->isAutomatable());

    const auto& range = gain->getNormalisableRange();
    CHECK (juce::exactlyEqual (range.start, -60.0f));
    CHECK (juce::exactlyEqual (range.end, 6.0f));
    CHECK (juce::exactlyEqual (range.interval, 0.0f));
    CHECK (juce::exactlyEqual (range.skew, 1.0f));
    CHECK_THAT (gain->convertFrom0to1 (gain->getDefaultValue()), WithinAbs (0.0, 1.0e-6));
    CHECK_THAT (gain->convertFrom0to1 (gain->getValue()), WithinAbs (0.0, 1.0e-6));

    // JUCE's default text, as M0 shipped it.
    CHECK (gain->getText (gain->convertTo0to1 (-12.5f), 0) == "-12.5000000");
    CHECK_THAT (gain->convertFrom0to1 (gain->getValueForText ("-12.5")), WithinAbs (-12.5, 1.0e-4));
}

TEMPLATE_TEST_CASE ("every v0.1 parameter has version hint 2, its name, label, range and default", "[parameters]", ParameterProcessors)
{
    TestType plugin;

    for (const auto& expected : approved())
    {
        INFO (expected.id);
        auto* parameter = findParameter (plugin, expected.id);
        REQUIRE (parameter != nullptr);

        CHECK (parameter->getVersionHint() == 2);
        CHECK (parameter->getName (64) == expected.name);
        CHECK (parameter->getLabel() == expected.label);
        CHECK (parameter->isAutomatable());
        CHECK_FALSE (parameter->isMetaParameter());

        const auto& range = parameter->getNormalisableRange();
        const auto defaultValue = parameter->convertFrom0to1 (parameter->getDefaultValue());
        CHECK_THAT (defaultValue, WithinAbs (expected.defaultValue, 1.0e-5));
        CHECK_THAT (parameter->convertFrom0to1 (parameter->getValue()), WithinAbs (expected.defaultValue, 1.0e-5)); // starts at it

        switch (expected.kind)
        {
            case Kind::level:
                CHECK (dynamic_cast<juce::AudioParameterFloat*> (parameter) != nullptr);
                CHECK (juce::exactlyEqual (range.start, -60.0f));
                CHECK (juce::exactlyEqual (range.end, 6.0f));
                CHECK (juce::exactlyEqual (range.interval, 0.0f));
                CHECK (juce::exactlyEqual (range.skew, 1.0f));
                CHECK_FALSE (parameter->isDiscrete());
                break;

            case Kind::percent:
                CHECK (dynamic_cast<juce::AudioParameterFloat*> (parameter) != nullptr);
                CHECK (juce::exactlyEqual (range.start, 0.0f));
                CHECK (juce::exactlyEqual (range.end, 200.0f));
                CHECK (juce::exactlyEqual (range.interval, 0.0f));
                CHECK (juce::exactlyEqual (range.skew, 1.0f));
                CHECK_FALSE (parameter->isDiscrete());
                break;

            case Kind::toggle:
                CHECK (dynamic_cast<juce::AudioParameterBool*> (parameter) != nullptr);
                CHECK (parameter->isBoolean());
                CHECK (parameter->isDiscrete());
                CHECK (parameter->getNumSteps() == 2);
                CHECK (parameter->getText (0.0f, 0) == "Off");
                CHECK (parameter->getText (1.0f, 0) == "On");
                break;

            case Kind::choice:
            {
                auto* choice = dynamic_cast<juce::AudioParameterChoice*> (parameter);
                REQUIRE (choice != nullptr);
                CHECK (choice->choices == expected.choices);
                CHECK (parameter->isDiscrete());
                CHECK (parameter->getNumSteps() == expected.choices.size());
                for (int i = 0; i < expected.choices.size(); ++i)
                    CHECK (parameter->getText (parameter->convertTo0to1 ((float) i), 0) == expected.choices[i]);
                break;
            }
        }
    }
}

TEST_CASE ("level and macro text", "[parameters]")
{
    using namespace wubansia::Parameters;

    CHECK (levelText (-60.0f) == "-inf");
    CHECK (levelText (-59.96f) == "-inf"); // it would read "-60.0", which means silence too
    CHECK (levelText (-59.94f) == "-59.9");
    CHECK (levelText (-6.0f) == "-6.0");
    CHECK (levelText (-0.04f) == "0.0");
    CHECK (levelText (0.0f) == "0.0");
    CHECK (levelText (3.46f) == "+3.5");
    CHECK (levelText (6.0f) == "+6.0");
    CHECK (levelText (-12.25f, 3) == "-12");
    CHECK (levelText (std::nanf ("")) == "-inf");

    CHECK (juce::exactlyEqual (levelFromText ("-inf"), -60.0f));
    CHECK (juce::exactlyEqual (levelFromText (" -6.0 dB"), -6.0f));
    CHECK (juce::exactlyEqual (levelFromText ("+3.5"), 3.5f));
    CHECK (juce::exactlyEqual (levelFromText ("12"), 6.0f)); // clamped to the range
    CHECK (juce::exactlyEqual (levelFromText ("-100"), -60.0f));

    CHECK (percentText (100.0f) == "100");
    CHECK (percentText (99.5f) == "100");
    CHECK (percentText (0.2f) == "0");
    CHECK (percentText (200.0f) == "200");
    CHECK (juce::exactlyEqual (percentFromText ("150 %"), 150.0f));
    CHECK (juce::exactlyEqual (percentFromText ("250"), 200.0f));
    CHECK (juce::exactlyEqual (percentFromText ("-5"), 0.0f));
}

TEMPLATE_TEST_CASE ("getText and getValueForText round-trip", "[parameters]", ParameterProcessors)
{
    TestType plugin;
    juce::Random random (20261105); // fixed: the same values on every run

    for (auto* base : plugin.getParameters())
    {
        auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (base);
        REQUIRE (parameter != nullptr);
        INFO (parameter->getParameterID());

        // The plain value may move by at most what the text rounds away.
        const auto id = parameter->getParameterID();
        const auto tolerance = id == "master_gain" ? 1.0e-4f
                             : id.endsWith ("_level") ? 0.05f + 1.0e-4f
                             : parameter->getLabel() == "%" ? 0.5f + 1.0e-3f
                                                             : 0.0f;

        int failures = 0;
        juce::String firstFailure;
        for (int i = 0; i < 1000; ++i)
        {
            const auto value = random.nextFloat();
            const auto text = parameter->getText (value, 0);
            const auto back = parameter->getValueForText (text);
            const auto plain = parameter->convertFrom0to1 (value);
            const auto plainBack = parameter->convertFrom0to1 (back);

            const bool sameText = parameter->getText (back, 0) == text;
            const bool close = parameter->isDiscrete() ? juce::roundToInt (plain) == juce::roundToInt (plainBack)
                                                       : std::abs (plain - plainBack) <= tolerance
                                                             || (text == "-inf" && juce::exactlyEqual (plainBack, -60.0f));
            if (! (sameText && close) && failures++ == 0)
                firstFailure << value << " -> '" << text << "' -> " << back << " ('" << parameter->getText (back, 0) << "')";
        }

        INFO (firstFailure);
        CHECK (failures == 0);
    }
}

TEMPLATE_TEST_CASE ("the raw parameters fill the engine's controls", "[parameters]", ParameterProcessors)
{
    TestType plugin;
    const wubansia::Parameters::RawParameters raw (plugin.apvts);
    namespace engine = wubansia::engine;

    SECTION ("the defaults are what the club loops play: the engine's own defaults")
    {
        engine::Controls controls;
        controls.mode = engine::PlayMode::off;
        controls.variant = engine::Variant::novox;
        controls.ab = { 0, 0, 1 };
        controls.laneGain.fill (0.25f);
        controls.space = controls.echo = controls.pump = 0.0f;
        controls.club = false;
        raw.read (controls);

        const engine::Controls defaults;
        CHECK (controls.mode == defaults.mode);
        CHECK (controls.variant == defaults.variant);
        CHECK (controls.ab == defaults.ab);
        CHECK (controls.laneGain == defaults.laneGain);
        CHECK (juce::exactlyEqual (controls.space, defaults.space));
        CHECK (juce::exactlyEqual (controls.echo, defaults.echo));
        CHECK (juce::exactlyEqual (controls.pump, defaults.pump));
        CHECK (controls.club == defaults.club);
        CHECK_THAT (raw.masterGainDb(), WithinAbs (0.0, 1.0e-6));
    }

    SECTION ("every parameter reaches its control; the state-only fields stay as they are")
    {
        set (plugin, ids::playMode, 2.0f);
        set (plugin, ids::voxSynth, 0.0f);
        set (plugin, ids::kickAb, 0.0f);
        set (plugin, ids::bassAb, 0.0f);
        set (plugin, ids::stabAb, 1.0f);
        set (plugin, ids::laneLevel[0], -6.0f);
        set (plugin, ids::laneLevel[1], -60.0f); // the bottom of the range is silence
        set (plugin, ids::laneMute[2], 1.0f);
        set (plugin, ids::laneLevel[9], 6.0f);
        set (plugin, ids::space, 200.0f);
        set (plugin, ids::echo, 0.0f);
        set (plugin, ids::pump, 50.0f);
        set (plugin, ids::clubMaster, 0.0f);
        set (plugin, ids::masterGain, -12.5f);

        engine::Controls controls;
        controls.phrases = engine::PhraseMode::off;
        controls.midiIn = engine::MidiInMode::lane;
        controls.midiInLane = 4;
        raw.read (controls);

        CHECK (controls.mode == engine::PlayMode::song);
        CHECK (controls.variant == engine::Variant::novox);
        CHECK (controls.ab == std::array<std::uint8_t, 3> { 0, 0, 1 });
        CHECK_THAT (controls.laneGain[0], WithinAbs (0.501187, 1.0e-5));
        CHECK (juce::exactlyEqual (controls.laneGain[1], 0.0f));
        CHECK (juce::exactlyEqual (controls.laneGain[2], 0.0f));
        CHECK (juce::exactlyEqual (controls.laneGain[3], 1.0f));
        CHECK_THAT (controls.laneGain[9], WithinAbs (1.995262, 1.0e-5));
        CHECK_THAT (controls.space, WithinAbs (2.0, 1.0e-6));
        CHECK_THAT (controls.echo, WithinAbs (0.0, 1.0e-6));
        CHECK_THAT (controls.pump, WithinAbs (0.5, 1.0e-6));
        CHECK_FALSE (controls.club);
        CHECK_THAT (raw.masterGainDb(), WithinAbs (-12.5, 1.0e-4));

        CHECK (controls.phrases == engine::PhraseMode::off);
        CHECK (controls.midiIn == engine::MidiInMode::lane);
        CHECK (controls.midiInLane == 4);

        set (plugin, ids::playMode, 0.0f);
        set (plugin, ids::laneMute[2], 0.0f);
        raw.read (controls);
        CHECK (controls.mode == engine::PlayMode::off);
        CHECK (juce::exactlyEqual (controls.laneGain[2], 1.0f));
    }
}
