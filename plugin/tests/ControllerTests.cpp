// ControllerImpl (ControllerImpl.h): the editor's API over the real processor, on the synthetic kit.
// The kit cards, kit selection and its status, the snapshot the editor polls, the Song's sections, the
// MIDI-in and phrase settings reaching the engine, drag-out from the kit's embedded MIDI, the content
// folder, and the real editor (P10's) on it.

#include "PluginEditor.h"
#include "helpers/test_helpers.h"

#include "midi/reader.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using Catch::Matchers::WithinAbs;
using namespace wubansia;
using test::SyntheticContent;
using test::SyntheticPlugin;

namespace
{
    SyntheticContent::Options twoKits (const char* a, const char* b)
    {
        SyntheticContent::Options options;
        options.ids = { a, b };
        return options;
    }

    const auto kick = juce::MidiMessage::noteOn (10, 36, (juce::uint8) 127);

    // A temp folder for drag files, deleted with this.
    struct DragFolder
    {
        DragFolder() : folder (juce::File::getSpecialLocation (juce::File::tempDirectory).getNonexistentChildFile ("WubansiaDragTest", "", false)) {}
        ~DragFolder() { folder.deleteRecursively(); }
        juce::File folder;
    };
}

TEST_CASE ("the kit cards come from the kits' manifests, with their content status", "[controller]")
{
    SyntheticPlugin plugin (twoKits ("CTL01", "CTL02"));
    auto& controller = plugin->controller();

    auto cards = controller.kits();
    REQUIRE (cards.size() == 2);
    const auto& manifest = engine::testing::syntheticContent().kit.kit;
    CHECK (cards[0].id == "CTL01");
    CHECK (cards[1].id == "CTL02");
    CHECK (cards[0].direction == juce::String (manifest.direction)); // exactly as the manifest has it
    CHECK (cards[0].key == juce::String (manifest.key));
    CHECK (cards[0].camelot == juce::String (manifest.camelot));
    CHECK (cards[0].bpm == manifest.bpm);
    CHECK (cards[0].contentVersion == manifest.contentVersion);
    for (std::size_t lane = 0; lane < cards[0].laneLabels.size(); ++lane)
        CHECK (cards[0].laneLabels[lane] == juce::String (manifest.lanes[lane]));
    CHECK (cards[0].phraseLanes == engine::laneBit (engine::Lane::lead));
    CHECK (cards[0].abA[0] == juce::String (manifest.ab[0].a));
    CHECK (cards[0].abB[1] == juce::String (manifest.ab[1].b));
    CHECK (cards[0].abA[2].isEmpty()); // no stab A/B

    test::prepare (*plugin, 48000.0, 512);
    REQUIRE (test::waitFor ([&] {
        cards = controller.kits();
        return cards[0].content.state == ContentState::ready && cards[1].content.state == ContentState::ready;
    }));
}

TEST_CASE ("selectKit: a known kit loads and becomes the project's, an unknown one is ignored", "[controller]")
{
    SyntheticPlugin plugin (twoKits ("SEL01", "SEL02"));
    auto& controller = plugin->controller();
    CHECK (controller.currentKit() == "SEL01");
    test::prepareWithKit (*plugin);
    CHECK (controller.status().state == ContentState::ready);

    controller.selectKit ("SEL02");
    CHECK (controller.currentKit() == "SEL02");
    CHECK (controller.status().state == ContentState::loading);
    REQUIRE (test::waitForStatus (*plugin, ContentState::ready));
    CHECK (controller.status().folder == SyntheticContent::folderOf ("SEL02"));
    CHECK (controller.status().percent == 100);
    CHECK (controller.status().problem.isEmpty());

    const auto generation = plugin->kitService().status().generation;
    controller.selectKit ("SEL02"); // already there
    controller.selectKit ("NOPE");
    CHECK (plugin->kitService().status().generation == generation);
    CHECK (controller.currentKit() == "SEL02");
}

TEST_CASE ("the snapshot: hits and peaks since the last one, voices, the mode, -1 ticks while stopped", "[controller]")
{
    SyntheticPlugin plugin (twoKits ("SNP01", "SNP02"));
    test::prepareWithKit (*plugin);
    auto& controller = plugin->controller();

    auto s = controller.snapshot();
    CHECK_FALSE (s.playing);
    CHECK (s.songTicks < 0.0);
    CHECK (s.section == -1);
    CHECK (s.mode == engine::PlayMode::loop);
    CHECK (s.voices == 0);

    test::render (*plugin, 4800, 512, test::midiAt (kick, 0));
    s = controller.snapshot();
    CHECK ((s.laneHits & engine::laneBit (engine::Lane::kick)) != 0u);
    CHECK (s.lanePeak[(std::size_t) engine::Lane::kick] > 0.0f);
    CHECK (s.voices > 0);
    CHECK (s.gainReductionDb >= 0.0f);

    s = controller.snapshot(); // taken: nothing new since
    CHECK (s.laneHits == 0u);
    CHECK (s.lanePeak[(std::size_t) engine::Lane::kick] <= 0.0f);

    // The loop on the plugin's own clock, at the kit's tempo: the lead lane plays its recorded phrase.
    controller.setTransport (true, 0.0);
    const auto recordedOver = [&] (int blocks) {
        std::uint32_t lanes = 0;
        for (int i = 0; i < blocks; ++i)
        {
            test::process (*plugin, 512);
            lanes |= controller.snapshot().phraseRecorded;
        }
        return lanes;
    };
    CHECK (recordedOver (400) == engine::laneBit (engine::Lane::lead));
    controller.setPhraseMode (engine::PhraseMode::off);
    test::render (*plugin, 48000 * 4, 512); // the stem's slice in flight ends (a 2-bar cycle at most)
    CHECK (recordedOver (200) == 0u);

    // Song mode: ticks into the song and its section.
    test::setParameter (*plugin, ParameterIds::playMode, 2.0f);
    test::render (*plugin, 48000 * 3, 512);
    s = controller.snapshot();
    CHECK (s.playing);
    CHECK (s.mode == engine::PlayMode::song);
    CHECK (s.songTicks > 0.0);
    CHECK (s.section >= 0);
    CHECK (s.section < (int) controller.sections().size());
    CHECK_THAT (s.bpm, WithinAbs (128.0, 1.0e-9));
}

TEST_CASE ("the Song's sections, from its markers", "[controller]")
{
    SyntheticPlugin plugin (twoKits ("SEC01", "SEC02"));
    const auto sections = plugin->controller().sections();
    REQUIRE (sections.size() == 3);
    CHECK (sections[0].name == "Intro");
    CHECK (sections[0].startBar == 1);
    CHECK (sections[1].name == "Drop");
    CHECK (sections[2].name == "Outro");
    int bars = 0;
    for (const auto& s : sections)
        bars += s.bars;
    CHECK (bars == 16);
    test::setParameter (*plugin, ParameterIds::voxSynth, 0.0f);
    CHECK (plugin->controller().sections().size() == 3);
}

TEST_CASE ("MIDI in: Lane mode plays every channel on one lane; the settings reach the engine", "[controller]")
{
    SyntheticPlugin plugin (twoKits ("MDI01", "MDI02"));
    test::prepareWithKit (*plugin);
    auto& controller = plugin->controller();
    const auto onChannel2 = test::midiAt (juce::MidiMessage::noteOn (2, 36, (juce::uint8) 127), 0);

    // Multi: channel 2 is the lead, whose keys start at 48: nothing.
    CHECK (test::silent (test::render (*plugin, 9600, 512, onChannel2)));

    controller.setMidiInMode (engine::MidiInMode::lane, engine::Lane::kick);
    CHECK (controller.midiInMode() == engine::MidiInMode::lane);
    CHECK (controller.midiInLane() == engine::Lane::kick);
    CHECK_FALSE (test::silent (test::render (*plugin, 9600, 512, onChannel2)));
}

TEST_CASE ("drag-out: the kit's own MIDI, cut to the request, into the drag folder", "[controller][dragout]")
{
    SyntheticPlugin plugin (twoKits ("DRG01", "DRG02"));
    DragFolder drag;
    auto& controller = static_cast<ControllerImpl&> (plugin->controller());
    controller.setDragFolderForTesting (drag.folder);

    CHECK (controller.phraseFiles() == juce::StringArray { "DRG01_Phrase_Lead_2bars.mid" });

    DragRequest loop;
    const auto file = controller.writeDragFile (loop);
    REQUIRE (file.existsAsFile());
    CHECK (file.getParentDirectory() == drag.folder);
    CHECK (file.getFileName().startsWith ("DRG01 "));
    CHECK (file.getFileName().endsWith (".mid"));
    juce::MemoryBlock bytes;
    REQUIRE (file.loadFileAsData (bytes));
    const auto& smf = engine::testing::syntheticContent().smfs[0];
    CHECK (bytes.getSize() == smf.size()); // nothing muted: the vox Loop file as it is

    // A muted lane is left out.
    test::setParameter (*plugin, ParameterIds::laneMute[0], 1.0f);
    const auto muted = controller.writeDragFile (loop);
    REQUIRE (muted.existsAsFile());
    CHECK (muted != file);
    CHECK (muted.getSize() < file.getSize());

    DragRequest lane;
    lane.what = DragWhat::lane;
    lane.lane = engine::Lane::bass;
    const auto bass = controller.writeDragFile (lane);
    REQUIRE (bass.existsAsFile());
    juce::MemoryBlock bassBytes;
    REQUIRE (bass.loadFileAsData (bassBytes));
    CHECK (midi::readSmf (std::span<const std::uint8_t> (static_cast<const std::uint8_t*> (bassBytes.getData()), bassBytes.getSize())).ok());

    DragRequest phrase;
    phrase.what = DragWhat::phrase;
    phrase.phraseFile = "DRG01_Phrase_Lead_2bars.mid";
    CHECK (controller.writeDragFile (phrase).existsAsFile());
    phrase.phraseFile = "DRG02_Phrase_Lead_2bars.mid"; // another kit's
    CHECK (controller.writeDragFile (phrase) == juce::File());
}

TEST_CASE ("the chosen content folder goes to the person's settings and reloads the kit", "[controller][content]")
{
    SyntheticContent::Options options;
    options.ids = { "FLD01" };
    options.samples = SyntheticContent::Samples::missing;
    SyntheticPlugin plugin (options);
    test::prepare (*plugin, 48000.0, 512);
    REQUIRE (test::waitForStatus (*plugin, ContentState::missing));

    plugin.content->setSamples (SyntheticContent::Samples::ready);
    const auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Chosen");
    plugin->controller().setContentFolder (folder);
    CHECK (plugin.content->userFolder() == folder);
    CHECK (test::waitForStatus (*plugin, ContentState::ready));
}

TEST_CASE ("the editor on the real processor: it opens, shows the kits, and closes", "[controller][editor]")
{
    SyntheticPlugin plugin (twoKits ("EDT01", "EDT02"));
    test::prepareWithKit (*plugin);
    REQUIRE (plugin->hasEditor());

    test::withEditor (*plugin, [&] (juce::AudioProcessorEditor& base) {
        auto* editor = dynamic_cast<PluginEditor*> (&base);
        REQUIRE (editor != nullptr);
        editor->tick();
        CHECK (editor->model().kits.size() == 2);
        CHECK (editor->model().hasKit());
        CHECK (editor->model().kit.id == "EDT01");
        CHECK (editor->getWidth() > 0);
        juce::Image image (juce::Image::ARGB, editor->getWidth(), editor->getHeight(), true, juce::SoftwareImageType());
        juce::Graphics g (image);
        editor->paintEntireComponent (g, false);
    });
    CHECK (plugin->getActiveEditor() == nullptr);
}
