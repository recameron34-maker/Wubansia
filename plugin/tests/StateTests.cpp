// State version 2 (State.h, ADR 0018, amending ADR 0002): the parameters and the five state-only
// settings, M0's version 1 migrated, newer versions and unknown data kept, and the bytes a host gets
// back right after it sets them are the bytes it set (pluginval's "Plugin state restoration").
//
// The processors here play the synthetic kit under the real kits' ids (FUT01, FUT02), so a state's
// kit loads without samples and without reading the disk.

#include "helpers/test_helpers.h"

#include "content/UserSettings.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <chrono>
#include <cmath>
#include <cstring>

using Catch::Matchers::WithinAbs;
using namespace wubansia;
using test::masterGainDb;
using test::setMasterGainDb;
using test::SyntheticContent;
using test::SyntheticPlugin;

namespace
{
    SyntheticContent::Options twoKits()
    {
        SyntheticContent::Options options;
        options.ids = { "FUT01", "FUT02" };
        return options;
    }

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

    std::unique_ptr<juce::XmlElement> xmlOf (const juce::MemoryBlock& state)
    {
        return juce::AudioProcessor::getXmlFromBinary (state.getData(), (int) state.getSize());
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

    float plain (PluginProcessor& p, const juce::String& id) { return test::parameterValue (p, id); }

    float defaultOf (PluginProcessor& p, const juce::String& id)
    {
        auto& parameter = test::parameter (p, id);
        return parameter.convertFrom0to1 (parameter.getDefaultValue());
    }

    void checkEveryParameterAtItsDefault (PluginProcessor& p, const juce::String& except = {})
    {
        for (const auto* id : ParameterIds::all)
        {
            if (except == id)
                continue;
            INFO (id);
            CHECK_THAT (plain (p, id), WithinAbs (defaultOf (p, id), 1.0e-4));
        }
    }

    // Every parameter set away from its default, the same way each time.
    void setEveryParameter (PluginProcessor& p)
    {
        juce::Random random (7);
        for (auto* base : p.getParameters())
        {
            auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (base);
            REQUIRE (parameter != nullptr);
            auto value = random.nextFloat();
            if (std::abs (value - parameter->getDefaultValue()) < 0.2f)
                value = parameter->getDefaultValue() < 0.5f ? 0.9f : 0.1f;
            parameter->setValueNotifyingHost (value);
        }
    }

    // Records what the processor tells its host.
    struct HostListener final : juce::AudioProcessorListener
    {
        void audioProcessorParameterChanged (juce::AudioProcessor*, int, float) override { ++parameterChanges; }

        void audioProcessorChanged (juce::AudioProcessor*, const ChangeDetails& details) override
        {
            if (details.nonParameterStateChanged)
                ++stateChanges;
        }

        int parameterChanges = 0;
        int stateChanges = 0;
    };

    // A state exactly as M0 saves it, byte for byte. Every later version must keep loading it.
    constexpr auto m0State = R"(<?xml version="1.0" encoding="UTF-8"?> <WubansiaState stateVersion="1"><PARAM id="master_gain" value="-12.5"/></WubansiaState>)";

    // A version 2 state as v0.1 writes one, by hand: later versions must keep loading it too.
    constexpr auto v2State = R"(<?xml version="1.0" encoding="UTF-8"?> <WubansiaState stateVersion="2" kit="FUT02" kit_content="1" standalone_bpm="128.5" midi_in="lane:bass" phrase_lanes="off"><PARAM id="master_gain" value="-3.0"/><PARAM id="play_mode" value="2.0"/><PARAM id="vox_synth" value="0.0"/><PARAM id="kick_ab" value="0.0"/><PARAM id="bass_ab" value="0.0"/><PARAM id="stab_ab" value="1.0"/><PARAM id="kick_level" value="-6.0"/><PARAM id="kick_mute" value="1.0"/><PARAM id="snare_level" value="0.0"/><PARAM id="snare_mute" value="0.0"/><PARAM id="hats_level" value="0.0"/><PARAM id="hats_mute" value="0.0"/><PARAM id="perc_level" value="0.0"/><PARAM id="perc_mute" value="0.0"/><PARAM id="bass_level" value="3.0"/><PARAM id="bass_mute" value="0.0"/><PARAM id="lead_level" value="0.0"/><PARAM id="lead_mute" value="0.0"/><PARAM id="chords_level" value="0.0"/><PARAM id="chords_mute" value="0.0"/><PARAM id="vox_level" value="0.0"/><PARAM id="vox_mute" value="0.0"/><PARAM id="accents_level" value="0.0"/><PARAM id="accents_mute" value="0.0"/><PARAM id="fx_level" value="-60.0"/><PARAM id="fx_mute" value="0.0"/><PARAM id="space" value="150.0"/><PARAM id="echo" value="0.0"/><PARAM id="pump" value="50.0"/><PARAM id="club_master" value="0.0"/></WubansiaState>)";
}

TEST_CASE ("gain survives a round trip", "[state]")
{
    SyntheticPlugin original (twoKits());
    setMasterGainDb (*original, -12.5f);
    REQUIRE_THAT (masterGainDb (*original), WithinAbs (-12.5, 1.0e-4));

    const auto state = saveState (*original);
    REQUIRE (state.getSize() > 0);

    SyntheticPlugin restored (twoKits());
    REQUIRE_THAT (masterGainDb (*restored), WithinAbs (0.0, 1.0e-6));
    loadState (*restored, state);
    CHECK_THAT (masterGainDb (*restored), WithinAbs (-12.5, 1.0e-4));
}

TEST_CASE ("M0's version 1 state loads with every new parameter at its default and kit FUT01, and saves as version 2", "[state]")
{
    SyntheticPlugin plugin (twoKits());

    // Away from every default first, as a host reusing an instance would leave it.
    setEveryParameter (*plugin);
    plugin->controller().selectKit ("FUT02");
    plugin->controller().setMidiInMode (engine::MidiInMode::lane, engine::Lane::vox);

    loadState (*plugin, binaryXml (m0State));
    CHECK_THAT (masterGainDb (*plugin), WithinAbs (-12.5, 1.0e-4));
    checkEveryParameterAtItsDefault (*plugin, ParameterIds::masterGain);

    const auto settings = plugin->settings();
    CHECK (settings.kit == "FUT01");
    CHECK (juce::exactlyEqual (settings.standaloneBpm, 0.0));
    CHECK (settings.midiIn == engine::MidiInMode::multi);
    CHECK (settings.phrases == engine::PhraseMode::automatic);

    const auto xml = xmlOf (saveState (*plugin));
    REQUIRE (xml != nullptr);
    CHECK (xml->getTagName() == "WubansiaState");
    CHECK (xml->getIntAttribute ("stateVersion") == 2);
    CHECK (xml->getStringAttribute ("kit") == "FUT01");
    CHECK (xml->getIntAttribute ("kit_content") == 1);
    REQUIRE (xml->getChildByAttribute ("id", "master_gain") != nullptr);
    CHECK_THAT (xml->getChildByAttribute ("id", "master_gain")->getDoubleAttribute ("value"), WithinAbs (-12.5, 1.0e-6));
}

TEST_CASE ("a saved state is version 2: the root's attributes, then every parameter in the host's order", "[state]")
{
    SyntheticPlugin plugin (twoKits());
    const auto xml = xmlOf (saveState (*plugin));
    REQUIRE (xml != nullptr);

    CHECK (xml->getTagName() == "WubansiaState");
    CHECK (xml->getNumAttributes() == 6);
    const juce::StringArray attributes { "stateVersion", "kit", "kit_content", "standalone_bpm", "midi_in", "phrase_lanes" };
    for (int i = 0; i < attributes.size(); ++i)
        CHECK (xml->getAttributeName (i) == attributes[i]);
    CHECK (xml->getStringAttribute ("stateVersion") == "2");
    CHECK (xml->getStringAttribute ("kit") == "FUT01");
    CHECK (xml->getStringAttribute ("kit_content") == "1");
    CHECK (xml->getStringAttribute ("standalone_bpm") == "0");
    CHECK (xml->getStringAttribute ("midi_in") == "multi");
    CHECK (xml->getStringAttribute ("phrase_lanes") == "auto");

    juce::StringArray ids;
    for (auto* child : xml->getChildIterator())
    {
        CHECK (child->getTagName() == "PARAM");
        ids.add (child->getStringAttribute ("id"));
    }
    juce::StringArray expected;
    for (const auto* id : ParameterIds::all)
        expected.add (id);
    CHECK (ids == expected);
}

TEST_CASE ("a hand-written version 2 state loads", "[state]")
{
    SyntheticPlugin plugin (twoKits());
    loadState (*plugin, binaryXml (v2State));

    CHECK_THAT (masterGainDb (*plugin), WithinAbs (-3.0, 1.0e-4));
    CHECK_THAT (plain (*plugin, ParameterIds::playMode), WithinAbs (2.0, 1.0e-4));
    CHECK_THAT (plain (*plugin, ParameterIds::voxSynth), WithinAbs (0.0, 1.0e-4));
    CHECK_THAT (plain (*plugin, ParameterIds::stabAb), WithinAbs (1.0, 1.0e-4));
    CHECK_THAT (plain (*plugin, ParameterIds::laneLevel[0]), WithinAbs (-6.0, 1.0e-4));
    CHECK_THAT (plain (*plugin, ParameterIds::laneMute[0]), WithinAbs (1.0, 1.0e-4));
    CHECK_THAT (plain (*plugin, ParameterIds::laneLevel[4]), WithinAbs (3.0, 1.0e-4));
    CHECK_THAT (plain (*plugin, ParameterIds::laneLevel[9]), WithinAbs (-60.0, 1.0e-4));
    CHECK_THAT (plain (*plugin, ParameterIds::space), WithinAbs (150.0, 1.0e-3));
    CHECK_THAT (plain (*plugin, ParameterIds::clubMaster), WithinAbs (0.0, 1.0e-4));

    const auto settings = plugin->settings();
    CHECK (settings.kit == "FUT02");
    CHECK (settings.kitContent == 1);
    CHECK (juce::exactlyEqual (settings.standaloneBpm, 128.5));
    CHECK (settings.midiIn == engine::MidiInMode::lane);
    CHECK (settings.midiInLane == engine::Lane::bass);
    CHECK (settings.phrases == engine::PhraseMode::off);

    // The Controller sees the same.
    CHECK (plugin->controller().currentKit() == "FUT02");
    CHECK (plugin->controller().midiInMode() == engine::MidiInMode::lane);
    CHECK (plugin->controller().midiInLane() == engine::Lane::bass);
    CHECK (plugin->controller().phraseMode() == engine::PhraseMode::off);
    CHECK (juce::exactlyEqual (plugin->controller().transport().standaloneBpm, 128.5));
}

TEST_CASE ("a version 2 round trip keeps every parameter and every state-only setting", "[state]")
{
    SyntheticPlugin original (twoKits());
    setEveryParameter (*original);
    auto& controller = original->controller();
    controller.selectKit ("FUT02");
    controller.setTransport (false, 131.25);
    controller.setMidiInMode (engine::MidiInMode::lane, engine::Lane::accents);
    controller.setPhraseMode (engine::PhraseMode::off);

    const auto state = saveState (*original);
    SyntheticPlugin restored (twoKits());
    loadState (*restored, state);

    for (const auto* id : ParameterIds::all)
    {
        INFO (id);
        CHECK_THAT (plain (*restored, id), WithinAbs (plain (*original, id), 1.0e-4));
    }
    CHECK (restored->settings() == original->settings());
    CHECK (restored->settings().kit == "FUT02");
    CHECK (juce::exactlyEqual (restored->settings().standaloneBpm, 131.25));
    CHECK (restored->settings().midiInLane == engine::Lane::accents);
    CHECK (saveState (*restored) == state);
}

TEST_CASE ("the state a host sets is the state it gets back, byte for byte (pluginval's restoration test)", "[state][validator]")
{
    SyntheticPlugin plugin (twoKits());

    SECTION ("a new instance's state") {}

    SECTION ("a state with every parameter and setting changed")
    {
        setEveryParameter (*plugin);
        plugin->controller().setTransport (false, 99.5);
        plugin->controller().setPhraseMode (engine::PhraseMode::off);
        plugin->controller().setMidiInMode (engine::MidiInMode::lane, engine::Lane::fx);
    }

    SECTION ("a migrated version 1 state")
    {
        loadState (*plugin, binaryXml (m0State));
    }

    const auto original = saveState (*plugin);
    juce::Random random (1105);

    // pluginval: each parameter set at random (a host's set reaches the parameter's listeners, as
    // setValueNotifyingHost does), then the original state set, then that parameter checked; here the
    // kit changes too.
    for (auto* base : plugin->getParameters())
    {
        auto* parameter = dynamic_cast<juce::RangedAudioParameter*> (base);
        REQUIRE (parameter != nullptr);
        const auto before = parameter->getValue();
        parameter->setValueNotifyingHost (random.nextFloat());
        plugin->controller().selectKit ("FUT02");
        loadState (*plugin, original);
        INFO (parameter->getParameterID());
        CHECK_THAT (parameter->getValue(), WithinAbs (before, 1.0e-6));
    }

    CHECK (saveState (*plugin) == original);
    CHECK (plugin->settings().kit == xmlOf (original)->getStringAttribute ("kit"));

    // And straight away, with nothing in between.
    loadState (*plugin, original);
    CHECK (saveState (*plugin) == original);
}

TEST_CASE ("a state from a newer version keeps its version, its kit and its unknown data", "[state]")
{
    // Opening a newer project in this build and saving it must not stamp it as an older version, or
    // drop what that version wrote, or a later migration would misread it.
    SyntheticPlugin plugin (twoKits());
    loadState (*plugin, binaryXml (R"(<WubansiaState stateVersion="7" kit="FUT05" kit_content="3" standalone_bpm="0" midi_in="multi" phrase_lanes="auto" future_setting="x"><PARAM id="master_gain" value="-3"/><PARAM id="future_param" value="0.5"/><FutureChild a="1"/></WubansiaState>)"));
    CHECK_THAT (masterGainDb (*plugin), WithinAbs (-3.0, 1.0e-4));
    CHECK (plugin->settings().kit == "FUT05");

    const auto state = saveState (*plugin);
    const auto xml = xmlOf (state);
    REQUIRE (xml != nullptr);
    CHECK (xml->getIntAttribute ("stateVersion") == 7);
    CHECK (xml->getStringAttribute ("kit") == "FUT05");
    CHECK (xml->getIntAttribute ("kit_content") == 3);
    CHECK (xml->getStringAttribute ("future_setting") == "x");
    REQUIRE (xml->getChildByAttribute ("id", "future_param") != nullptr);
    CHECK (xml->getChildByAttribute ("id", "future_param")->getStringAttribute ("value") == "0.5");
    CHECK (xml->getChildByName ("FutureChild") != nullptr);

    // A kit this build doesn't carry plays nothing, and says so.
    test::prepare (*plugin, 48000.0, 512);
    REQUIRE (test::waitForStatus (*plugin, ContentState::failed));
    CHECK (plugin->controller().status().problem.contains ("FUT05"));
    CHECK (test::silent (test::process (*plugin, 512, test::midiAt (juce::MidiMessage::noteOn (10, 36, (juce::uint8) 127), 0))));

    // Saved again, it is the same.
    loadState (*plugin, state);
    CHECK (saveState (*plugin) == state);
}

TEST_CASE ("a value that isn't a finite number falls back to the default", "[state]")
{
    SyntheticPlugin plugin (twoKits());
    setMasterGainDb (*plugin, -6.0f);
    test::setParameter (*plugin, ParameterIds::space, 20.0f);
    loadState (*plugin, binaryXml (R"(<WubansiaState stateVersion="2"><PARAM id="master_gain" value="nan"/><PARAM id="space" value="inf"/><PARAM id="echo" value="50"/></WubansiaState>)"));

    CHECK (std::isfinite (masterGainDb (*plugin)));
    CHECK_THAT (masterGainDb (*plugin), WithinAbs (0.0, 1.0e-4));
    CHECK_THAT (plain (*plugin, ParameterIds::space), WithinAbs (100.0, 1.0e-3));
    CHECK_THAT (plain (*plugin, ParameterIds::echo), WithinAbs (50.0, 1.0e-3));

    const auto xml = xmlOf (saveState (*plugin));
    REQUIRE (xml != nullptr);
    for (auto* child : xml->getChildIterator())
        CHECK (std::isfinite (child->getDoubleAttribute ("value")));
}

TEST_CASE ("a state's missing or unknown settings take their defaults", "[state]")
{
    SyntheticPlugin plugin (twoKits());
    loadState (*plugin, binaryXml (R"(<WubansiaState stateVersion="2" kit="" standalone_bpm="fast" midi_in="omni" phrase_lanes="sometimes"/>)"));
    const auto settings = plugin->settings();
    CHECK (settings.kit == "FUT01");
    CHECK (juce::exactlyEqual (settings.standaloneBpm, 0.0));
    CHECK (settings.midiIn == engine::MidiInMode::multi);
    CHECK (settings.phrases == engine::PhraseMode::automatic);
    checkEveryParameterAtItsDefault (*plugin);

    loadState (*plugin, binaryXml (R"(<WubansiaState stateVersion="2" kit="FUT02" standalone_bpm="1000" midi_in="lane:nope"/>)"));
    CHECK (plugin->settings().kit == "FUT02");
    CHECK (juce::exactlyEqual (plugin->settings().standaloneBpm, 300.0)); // held to the transport's range
    CHECK (plugin->settings().midiIn == engine::MidiInMode::multi);
}

TEST_CASE ("bad state is ignored", "[state]")
{
    SyntheticPlugin plugin (twoKits());
    setMasterGainDb (*plugin, -6.0f);
    plugin->controller().setPhraseMode (engine::PhraseMode::off);
    const auto valid = saveState (*plugin);

    SECTION ("empty")
    {
        plugin->setStateInformation (nullptr, 0);
    }

    SECTION ("junk")
    {
        const char junk[] = "not a state";
        plugin->setStateInformation (junk, (int) sizeof (junk));
    }

    SECTION ("truncated")
    {
        SyntheticPlugin other (twoKits());
        setMasterGainDb (*other, -30.0f);
        const auto full = saveState (*other);
        plugin->setStateInformation (full.getData(), (int) full.getSize() / 2);
    }

    SECTION ("wrong root tag")
    {
        loadState (*plugin, binaryXml (R"(<OtherPlugin stateVersion="2" kit="FUT02"><PARAM id="master_gain" value="-30"/></OtherPlugin>)"));
    }

    CHECK_THAT (masterGainDb (*plugin), WithinAbs (-6.0, 1.0e-4));
    CHECK (plugin->settings().phrases == engine::PhraseMode::off);
    CHECK (saveState (*plugin) == valid);
}

TEST_CASE ("a state-only change tells the host the project changed", "[state]")
{
    SyntheticPlugin plugin (twoKits());
    HostListener host;
    plugin->addListener (&host);
    auto& controller = plugin->controller();

    controller.selectKit ("FUT02");
    CHECK (host.stateChanges == 1);
    controller.setTransport (false, 120.0);
    CHECK (host.stateChanges == 2);
    controller.setMidiInMode (engine::MidiInMode::lane, engine::Lane::bass);
    CHECK (host.stateChanges == 3);
    controller.setPhraseMode (engine::PhraseMode::off);
    CHECK (host.stateChanges == 4);

    // Nothing changed: nothing to tell.
    controller.setPhraseMode (engine::PhraseMode::off);
    controller.selectKit ("NOPE");
    CHECK (host.stateChanges == 4);

    // Playing and stopping the Standalone transport, and the content folder, aren't project state.
    controller.setTransport (true, 120.0);
    controller.setTransport (false, 120.0);
    controller.setContentFolder (juce::File::getSpecialLocation (juce::File::tempDirectory));
    // Nor are the drag strip's drum option (ADR 0025) and For my synths (ADR 0027), the person's (here in the test's own
    // settings file).
    const auto before = saveState (*plugin);
    controller.setDragDrumsOnOneTrack (true);
    CHECK (controller.dragDrumsOnOneTrack());
    controller.setDragForSynths (false);
    CHECK_FALSE (controller.dragForSynths());
    // Nor is the drag strip's Part (ADR 0029), the instance's own.
    controller.setDragPart (DragPart::drums);
    CHECK (controller.dragPart() == DragPart::drums);
    CHECK (saveState (*plugin) == before);
    CHECK (host.stateChanges == 4);

    plugin->removeListener (&host);
}

TEST_CASE ("setStateInformation returns at once, even when the kit changes under a slow load", "[state][service]")
{
    auto options = twoKits();
    options.readDelay = std::chrono::milliseconds (100);
    SyntheticPlugin plugin (options);

    // The two states, from an instance gone before the timing starts (so no Kit of its lives on in the
    // process-wide KitCache for the slow one to share).
    juce::MemoryBlock fut01, fut02;
    {
        SyntheticPlugin source (twoKits());
        fut01 = saveState (*source);
        source->controller().selectKit ("FUT02");
        fut02 = saveState (*source);
    }

    using Clock = std::chrono::steady_clock;
    const auto timed = [&] (const juce::MemoryBlock& state) {
        const auto start = Clock::now();
        loadState (*plugin, state);
        return Clock::now() - start;
    };

    // The first state: it queues the first load, and returns.
    CHECK (timed (fut01) < test::limit (std::chrono::milliseconds (5)));
    REQUIRE (test::waitFor ([&] { return plugin.content->readsStarted.load() > 0; }));

    // Mid-load, another kit.
    CHECK (timed (fut02) < test::limit (std::chrono::milliseconds (5)));
    CHECK (plugin->settings().kit == "FUT02");
    plugin.content->setReadDelay (std::chrono::milliseconds (0));
    CHECK (test::waitForStatus (*plugin, ContentState::ready));
    CHECK (plugin->kitService().status().kit == "FUT02");
}

TEST_CASE ("the constructor reads nothing and starts nothing; the first prepareToPlay or setState queues the load", "[state][service]")
{
    SECTION ("prepareToPlay")
    {
        SyntheticPlugin plugin;
        CHECK_FALSE (plugin->kitService().started());
        CHECK (plugin.content->locates == 0);
        CHECK (plugin->kitService().status().generation == 0);

        test::prepare (*plugin, 48000.0, 512);
        CHECK (plugin->kitService().started());
        CHECK (test::waitForStatus (*plugin, ContentState::ready));
        CHECK (plugin->kitService().status().generation == 1);

        // Later prepares don't load again.
        test::prepare (*plugin, 44100.0, 256);
        CHECK (plugin->kitService().status().generation == 1);
    }

    SECTION ("setStateInformation")
    {
        SyntheticPlugin plugin (twoKits());
        const auto state = saveState (*plugin); // getState starts nothing either
        CHECK_FALSE (plugin->kitService().started());

        loadState (*plugin, state);
        CHECK (plugin->kitService().started());
        CHECK (test::waitForStatus (*plugin, ContentState::ready));
        CHECK (plugin->kitService().status().kit == "FUT01");

        // The same kit again queues nothing; prepareToPlay after it neither.
        loadState (*plugin, state);
        test::prepare (*plugin, 48000.0, 512);
        CHECK (plugin->kitService().status().generation == 1);
    }

    SECTION ("the plugin's own content: no settings file is read")
    {
        const auto reads = UserSettings::instance().fileReads();
        for (int i = 0; i < 10; ++i)
        {
            PluginProcessor plugin;
            CHECK_FALSE (plugin.kitService().started());
        }
        CHECK (UserSettings::instance().fileReads() == reads);
    }
}

TEST_CASE ("100 instances are created and destroyed in under 5 s", "[state][service]")
{
    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < 100; ++i)
    {
        PluginProcessor plugin;
        CHECK (plugin.getParameters().size() == 30);
    }
    CHECK (std::chrono::steady_clock::now() - start < test::limit (std::chrono::seconds (5)));
}

TEST_CASE ("instances are independent", "[state]")
{
    SyntheticPlugin a (twoKits()), b (twoKits());
    setMasterGainDb (*a, -20.0f);
    a->controller().setPhraseMode (engine::PhraseMode::off);
    CHECK_THAT (masterGainDb (*b), WithinAbs (0.0, 1.0e-6));
    CHECK (b->settings().phrases == engine::PhraseMode::automatic);
}
