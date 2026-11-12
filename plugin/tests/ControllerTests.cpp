// ControllerImpl (ControllerImpl.h): the editor's API over the real processor, on the synthetic kit.
// The kit cards, kit selection and its status, the snapshot the editor polls, the Song's sections, the
// MIDI-in and phrase settings reaching the engine, drag-out from the kit's embedded MIDI, the content
// folder, the Samples drawer's sounds, copies and samples folder (ADR 0024), and the real editor
// (P10's) on it.

#include "PluginEditor.h"
#include "content/ContentLocator.h"
#include "content/UserSettings.h"
#include "dragout/DragFiles.h"
#include "helpers/test_helpers.h"
#include "sounds/SampleExport.h"

#include "midi/reader.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <utility>

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

    // A fresh temp folder, made now and deleted with this.
    struct TempFolder
    {
        TempFolder() : folder (juce::File::getSpecialLocation (juce::File::tempDirectory).getNonexistentChildFile ("WubansiaControllerTest", "", false))
        {
            folder.createDirectory();
        }
        ~TempFolder() { folder.deleteRecursively(); }
        juce::File folder;
    };

    // What the processor tells its host.
    struct HostListener final : juce::AudioProcessorListener
    {
        void audioProcessorParameterChanged (juce::AudioProcessor*, int, float) override {}

        void audioProcessorChanged (juce::AudioProcessor*, const ChangeDetails& details) override
        {
            if (details.nonParameterStateChanged)
                ++stateChanges;
        }

        int stateChanges = 0;
    };

    // The synthetic kit's index of the file at path.
    int synFile (std::string_view path)
    {
        const auto& files = engine::testing::syntheticContent().kit.files;
        for (std::size_t i = 0; i < files.size(); ++i)
            if (files[i].path == path)
                return (int) i;
        return -1;
    }

    std::vector<std::uint8_t> bytesOf (const juce::File& file)
    {
        juce::MemoryBlock block;
        if (! file.loadFileAsData (block))
            return {};
        const auto* data = static_cast<const std::uint8_t*> (block.getData());
        return std::vector<std::uint8_t> (data, data + block.getSize());
    }
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

TEST_CASE ("sounds: every embedded kit's, with no samples needed", "[controller][sounds]")
{
    // The plugin's own four kits, before any prepare: the manifests it carries are enough.
    PluginProcessor plugin;
    auto& controller = plugin.controller();
    const std::pair<const char*, std::size_t> kits[] { { "FUT01", 35 }, { "FUT02", 37 }, { "FUT03", 36 }, { "FUT04", 32 } };
    for (const auto& [kit, count] : kits)
    {
        CAPTURE (kit);
        const auto sounds = controller.sounds (kit);
        CHECK (sounds.size() == count);
        const auto* entry = plugin.kitService().catalog().find (kit);
        REQUIRE (entry != nullptr);
        CHECK (sounds == entry->sounds);
    }
    CHECK (controller.sounds ("NOPE").empty());
    CHECK_FALSE (plugin.kitService().started()); // no thread, no content check

    // The synthetic kits: SYN01's 13 sounds under each id.
    SyntheticPlugin synthetic (twoKits ("SND01", "SND02"));
    const auto sounds = synthetic->controller().sounds ("SND02");
    REQUIRE (sounds.size() == 13);
    CHECK (sounds.front().id == "SND02/kick_a");
    CHECK (sounds.back().id == "SND02/Loops/Lead Phrase.wav");
    CHECK (synthetic.content->fileReads.load() == 0);
}

TEST_CASE ("exportSound copies the kit's file into the samples folder; a second call writes nothing", "[controller][sounds]")
{
    SyntheticPlugin plugin (twoKits ("XPT01", "XPT02"));
    TempFolder samples;
    auto& controller = static_cast<ControllerImpl&> (plugin->controller());
    controller.setSamplesFolderForTesting (samples.folder);
    CHECK (controller.samplesFolder() == samples.folder);
    test::prepareWithKit (*plugin);
    REQUIRE (controller.status().state == ContentState::ready);

    const auto riser = synFile ("FX/Riser.wav");
    const auto& wav = engine::testing::syntheticContent().wavs[(std::size_t) riser];
    const auto first = controller.exportSound ({ "XPT01", riser });
    INFO (first.problem);
    REQUIRE (first.file.existsAsFile());
    CHECK (first.problem.isEmpty());
    CHECK (first.file == samples.folder.getChildFile ("XPT01 Synthetic 01").getChildFile ("Riser - XPT01 128 Gm 6A.wav"));
    CHECK (bytesOf (first.file) == wav);

    const juce::Time past (2001, 0, 1, 12, 0, 0, 0, false);
    REQUIRE (first.file.setLastModificationTime (past));
    const auto second = controller.exportSound ({ "XPT01", riser });
    CHECK (second.file == first.file);
    CHECK (second.file.getLastModificationTime() == past); // nothing written

    // A kit other than the one playing, once the service has found its samples.
    REQUIRE (test::waitFor ([&] { return controller.kits()[1].content.state == ContentState::ready; }));
    const auto other = controller.exportSound ({ "XPT02", riser });
    INFO (other.problem);
    CHECK (other.file == samples.folder.getChildFile ("XPT02 Synthetic 02").getChildFile ("Riser - XPT02 128 Gm 6A.wav"));
    CHECK (bytesOf (other.file) == wav);

    // Nothing to copy: a kit this build doesn't carry, a file the kit doesn't list.
    const auto unknown = controller.exportSound ({ "NOPE", riser });
    CHECK (unknown.file == juce::File());
    CHECK (unknown.problem.contains ("NOPE"));
    for (const auto file : { -1, 30 })
    {
        const auto none = controller.exportSound ({ "XPT01", file });
        CHECK (none.file == juce::File());
        CHECK (none.problem.isNotEmpty());
    }
    CHECK (samples.folder.getNumberOfChildFiles (juce::File::findFilesAndDirectories) == 2); // the two kits' folders
}

TEST_CASE ("exportSound is refused, with the reason, while the kit's samples aren't there", "[controller][sounds]")
{
    SyntheticContent::Options options;
    options.ids = { "MIS01" };
    options.samples = SyntheticContent::Samples::missing;
    SyntheticPlugin plugin (options);
    TempFolder samples;
    auto& controller = static_cast<ControllerImpl&> (plugin->controller());
    controller.setSamplesFolderForTesting (samples.folder);
    const auto riser = synFile ("FX/Riser.wav");

    // Before the first content check: not found yet.
    const auto early = controller.exportSound ({ "MIS01", riser });
    CHECK (early.file == juce::File());
    CHECK (early.problem.isNotEmpty());

    test::prepare (*plugin, 48000.0, 512);
    REQUIRE (test::waitForStatus (*plugin, ContentState::missing));
    const auto refused = controller.exportSound ({ "MIS01", riser });
    CHECK (refused.file == juce::File());
    CHECK (refused.problem.contains ("no content folder holds MIS01"));
    CHECK (plugin.content->fileReads.load() == 0); // never read
    CHECK (samples.folder.getNumberOfChildFiles (juce::File::findFilesAndDirectories) == 0);
}

TEST_CASE ("setSamplesFolder is kept for the person, never in the project, and refuses the content and drag folders", "[controller][sounds]")
{
    SyntheticPlugin plugin (twoKits ("SMP01", "SMP02"));
    TempFolder temp;
    DragFolder drag;
    UserSettings settings (temp.folder.getChildFile ("settings.xml"));
    auto& controller = static_cast<ControllerImpl&> (plugin->controller());
    controller.setUserSettingsForTesting (settings);
    controller.setDragFolderForTesting (drag.folder);
    HostListener host;
    plugin->addListener (&host);

    CHECK (controller.samplesFolder() == sounds::defaultSamplesFolder()); // nothing chosen yet

    const auto mine = temp.folder.getChildFile ("My Samples"); // not there yet: made
    CHECK (controller.setSamplesFolder (mine));
    CHECK (mine.isDirectory());
    CHECK (controller.samplesFolder() == mine);
    CHECK (UserSettings (settings.file()).samplesFolder() == mine); // in the person's settings file
    CHECK (host.stateChanges == 0); // never the project's

    juce::MemoryBlock state;
    plugin->getStateInformation (state);
    const auto xml = juce::AudioProcessor::getXmlFromBinary (state.getData(), (int) state.getSize());
    REQUIRE (xml != nullptr);
    CHECK_FALSE (xml->hasAttribute ("samples_folder"));
    CHECK_FALSE (xml->toString().contains (mine.getFullPathName()));

    // The content folders (the kit's, and the root that holds it), the installers' content folder and the drag
    // cache, and anything inside them, are refused, and nothing changes.
    test::prepareWithKit (*plugin); // the service has searched SMP01's folder
    const auto kitFolder = SyntheticContent::folderOf ("SMP01");
    const auto contentRoot = kitFolder.getParentDirectory();
    const juce::File refusals[] { kitFolder, kitFolder.getChildFile ("Mine"), contentRoot, contentRoot.getChildFile ("Samples"),
        ContentLocator::platformDefaultFolder(), ContentLocator::platformDefaultFolder().getChildFile ("Samples"), drag.folder,
        drag.folder.getChildFile ("Samples"), dragout::cacheFolder(), dragout::cacheFolder().getChildFile ("Samples") };
    for (const auto& refused : refusals)
    {
        CAPTURE (refused.getFullPathName().toStdString());
        CHECK_FALSE (controller.setSamplesFolder (refused));
        CHECK (controller.samplesFolder() == mine);
    }
    CHECK_FALSE (kitFolder.getChildFile ("Mine").exists());

    // The content folder chosen with "Choose content folder…" too.
    const auto chosenContent = temp.folder.getChildFile ("Chosen Content");
    controller.setContentFolder (chosenContent);
    CHECK_FALSE (controller.setSamplesFolder (chosenContent));
    CHECK_FALSE (controller.setSamplesFolder (chosenContent.getChildFile ("Samples")));

    // A folder that can't be made: a file is in the way.
    const auto blocker = temp.folder.getChildFile ("a file");
    REQUIRE (blocker.replaceWithText ("not a folder"));
    CHECK_FALSE (controller.setSamplesFolder (blocker));
    CHECK_FALSE (controller.setSamplesFolder (blocker.getChildFile ("Samples")));
    CHECK (controller.samplesFolder() == mine);
    CHECK (settings.samplesFolder() == mine);

    // juce::File () goes back to the default.
    CHECK (controller.setSamplesFolder ({}));
    CHECK (controller.samplesFolder() == sounds::defaultSamplesFolder());
    CHECK (UserSettings (settings.file()).samplesFolder() == juce::File());
    CHECK (host.stateChanges == 0);
    plugin->removeListener (&host);
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
