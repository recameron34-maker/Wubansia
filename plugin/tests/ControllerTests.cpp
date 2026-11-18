// ControllerImpl (ControllerImpl.h): the editor's API over the real processor, on the synthetic kit.
// The kit cards, kit selection and its status, the snapshot the editor polls, the Song's sections, the
// MIDI-in and phrase settings reaching the engine, drag-out from the kit's embedded MIDI, the content
// folder, the Samples drawer's sounds, auditions, copies and samples folder (ADR 0024), and the real editor
// (P10's) on it.

#include "PluginEditor.h"
#include "content/ContentLocator.h"
#include "content/UserSettings.h"
#include "dragout/DragFiles.h"
#include "helpers/test_helpers.h"
#include "sounds/SampleExport.h"

#include "midi/for_synths.h"
#include "midi/reader.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <functional>
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

    // The editor's one component with this ID, as the editor tests find them.
    template <class T>
    T& component (juce::Component& root, const juce::String& id)
    {
        std::vector<juce::Component*> found;
        std::function<void (juce::Component&)> walk = [&] (juce::Component& parent)
        {
            for (auto* child : parent.getChildren())
            {
                if (child->getComponentID() == id)
                    found.push_back (child);
                walk (*child);
            }
        };
        walk (root);
        INFO ("component " << id);
        REQUIRE (found.size() == 1);
        auto* typed = dynamic_cast<T*> (found.front());
        REQUIRE (typed != nullptr);
        return *typed;
    }

    // A left click at a component's middle.
    void clickOn (juce::Component& c)
    {
        const auto at = c.getLocalBounds().toFloat().getCentre();
        const auto now = juce::Time::getCurrentTime();
        const juce::MouseEvent event (juce::Desktop::getInstance().getMainMouseSource(), at, juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier),
            juce::MouseInputSource::defaultPressure, juce::MouseInputSource::defaultOrientation, juce::MouseInputSource::defaultRotation,
            juce::MouseInputSource::defaultTiltX, juce::MouseInputSource::defaultTiltY, &c, &c, now, at, now, 1, false);
        c.mouseDown (event);
        c.mouseUp (event);
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
    controller.setDragForSynths (false); // the kit's files as they are (For my synths has a test of its own)

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

TEST_CASE ("drag-out: 1 drum track gives the Loop's and Song's drums as one track, kept in the person's settings", "[controller][dragout]")
{
    SyntheticPlugin plugin (twoKits ("DRM01", "DRM02"));
    DragFolder drag;
    auto& controller = static_cast<ControllerImpl&> (plugin->controller());
    controller.setDragFolderForTesting (drag.folder);
    controller.setDragForSynths (false); // every other track byte for byte (For my synths has a test of its own)
    const auto& syn = engine::testing::syntheticContent();

    using Bytes = std::vector<std::uint8_t>;
    const auto bytesOf = [] (const juce::File& file) {
        juce::MemoryBlock block;
        REQUIRE (file.loadFileAsData (block));
        const auto* p = static_cast<const std::uint8_t*> (block.getData());
        return Bytes (p, p + block.getSize());
    };
    const auto read = [] (const Bytes& bytes) {
        auto r = midi::readSmf (bytes);
        INFO (r.error);
        REQUIRE (r.ok());
        return r.value;
    };
    const auto namesOf = [] (const midi::File& file) {
        std::vector<std::string> names;
        for (const auto& t : file.tracks)
            names.push_back (midi::trackName (t));
        return names;
    };

    DragRequest loop;
    DragRequest song;
    song.what = DragWhat::song;
    DragRequest kickLane;
    kickLane.what = DragWhat::lane;
    DragRequest phrase;
    phrase.what = DragWhat::phrase;
    phrase.phraseFile = "DRM01_Phrase_Lead_2bars.mid"; // the synthetic kit's phrase file holds the loop, drums and all

    // Off, the default: the vox Loop file as it is.
    CHECK_FALSE (controller.dragDrumsOnOneTrack());
    CHECK (bytesOf (controller.writeDragFile (loop)) == syn.smfs[0]);
    const auto laneOff = bytesOf (controller.writeDragFile (kickLane));
    const auto phraseOff = bytesOf (controller.writeDragFile (phrase));

    // Lit: kept in the person's settings file (here the test's own), never in the project.
    controller.setDragDrumsOnOneTrack (true);
    CHECK (controller.dragDrumsOnOneTrack());
    CHECK (plugin.settings->file().loadFileAsString().contains ("drag_drums_one_track=\"1\""));

    // Kick, Snare and Hats become one track, Drums, after the conductor; every other track stays.
    const auto checkMerged = [&] (const DragRequest& request, const Bytes& source, const juce::String& what, std::size_t markers) {
        const auto file = controller.writeDragFile (request);
        REQUIRE (file.existsAsFile());
        CHECK (file.getFileName().endsWith (" - " + what + " (1 drum track).mid"));
        const auto bytes = bytesOf (file);
        const auto merged = read (bytes);
        const auto original = read (source);
        const auto originalNames = namesOf (original);
        REQUIRE (originalNames.size() == 11);
        CHECK (std::vector<std::string> (originalNames.begin() + 1, originalNames.begin() + 4) == std::vector<std::string> { "Kick", "Snare", "Hats" });
        CHECK (namesOf (merged) == std::vector<std::string> { originalNames[0], "Drums", "Bass", "Lead", "Chord voicing", "Chords", "Pad", "Vox", "FX" });
        REQUIRE (merged.tracks.size() == 9);

        // The three tracks' notes, by tick, channel and key.
        std::vector<midi::Note> drums;
        for (std::size_t i = 1; i <= 3; ++i)
        {
            const auto notes = midi::notesOf (original.tracks[i]);
            drums.insert (drums.end(), notes.begin(), notes.end());
        }
        std::stable_sort (drums.begin(), drums.end(), [] (const midi::Note& a, const midi::Note& b) {
            if (a.tick != b.tick)
                return a.tick < b.tick;
            return a.channel != b.channel ? a.channel < b.channel : a.key < b.key;
        });
        CHECK (midi::notesOf (merged.tracks[1]) == drums);
        for (const auto& e : merged.tracks[1].events)
            if (e.isChannel())
                CHECK (e.channel() == 9); // channel 10
        for (std::size_t i = 2; i < merged.tracks.size(); ++i)
            CHECK (merged.tracks[i] == original.tracks[i + 2]);
        CHECK (midi::markersOf (merged).size() == markers);
        return bytes;
    };
    const auto mergedLoop = checkMerged (loop, syn.smfs[0], "Loop", 0);
    checkMerged (song, syn.smfs[2], "Song", 3);

    // Lane and phrase drags don't change.
    CHECK (bytesOf (controller.writeDragFile (kickLane)) == laneOff);
    CHECK (bytesOf (controller.writeDragFile (phrase)) == phraseOff);
    CHECK (phraseOff == syn.smfs[0]);

    // Snare and hats muted: one drum track is left, so the drag is the plain one.
    test::setParameter (*plugin, ParameterIds::laneMute[1], 1.0f);
    test::setParameter (*plugin, ParameterIds::laneMute[2], 1.0f);
    const auto lone = controller.writeDragFile (loop);
    CHECK (lone.getFileName().endsWith (" - Loop (no Snare, Hats).mid"));
    CHECK (namesOf (read (bytesOf (lone))) == std::vector<std::string> { namesOf (read (syn.smfs[0]))[0], "Kick", "Bass", "Lead", "Chord voicing", "Chords", "Pad", "Vox", "FX" });
    test::setParameter (*plugin, ParameterIds::laneMute[1], 0.0f);
    test::setParameter (*plugin, ParameterIds::laneMute[2], 0.0f);

    // Every instance shares the person's settings: a second plugin on the same ones sees the option.
    SyntheticPlugin other (twoKits ("DRM01", "DRM02"));
    auto& otherController = static_cast<ControllerImpl&> (other->controller());
    otherController.setUserSettingsForTesting (*plugin.settings);
    otherController.setDragFolderForTesting (drag.folder);
    CHECK (otherController.dragDrumsOnOneTrack());
    CHECK (bytesOf (otherController.writeDragFile (loop)) == mergedLoop);

    // Off again: the plain file.
    controller.setDragDrumsOnOneTrack (false);
    CHECK_FALSE (controller.dragDrumsOnOneTrack());
    CHECK_FALSE (otherController.dragDrumsOnOneTrack());
    CHECK_FALSE (plugin.settings->file().loadFileAsString().contains ("drag_drums_one_track"));
    CHECK (bytesOf (controller.writeDragFile (loop)) == syn.smfs[0]);
}

TEST_CASE ("drag-out: For my synths is on by default and kept in the person's settings; off, the drags are as before", "[controller][dragout]")
{
    SyntheticPlugin plugin (twoKits ("SNT01", "SNT02"));
    DragFolder drag;
    auto& controller = static_cast<ControllerImpl&> (plugin->controller());
    controller.setDragFolderForTesting (drag.folder);
    const auto& syn = engine::testing::syntheticContent();
    const auto attribute = [&plugin] (const char* name)
    {
        const auto xml = juce::XmlDocument::parse (plugin.settings->file());
        return xml != nullptr && xml->hasAttribute (name) ? xml->getStringAttribute (name) : juce::String ("(none)");
    };

    DragRequest loop;
    DragRequest kickLane;
    kickLane.what = DragWhat::lane;
    DragRequest bassLane;
    bassLane.what = DragWhat::lane;
    bassLane.lane = engine::Lane::bass;

    // On by default, with nothing in the settings file.
    CHECK (controller.dragForSynths());
    CHECK (attribute ("drag_for_synths") == "(none)");

    // The Loop: the conductor, drums and FX as they are, the bass an octave up, the other pitched tracks (Lead, Chord
    // voicing, Chords, the Pad extra and Vox) made for synths. The name says so.
    using midi::SynthPart;
    const std::vector<SynthPart> parts { SynthPart::asIs, SynthPart::asIs, SynthPart::asIs, SynthPart::asIs, SynthPart::bass, SynthPart::pitched,
        SynthPart::pitched, SynthPart::pitched, SynthPart::pitched, SynthPart::pitched, SynthPart::asIs };
    const auto expected = midi::forSynths (syn.smfs[0], parts);
    REQUIRE (expected.ok());
    const auto file = controller.writeDragFile (loop);
    CHECK (file.getFileName().endsWith (" - Loop (synths).mid"));
    CHECK (bytesOf (file) == expected.value);
    CHECK (expected.value != syn.smfs[0]);
    const auto bass = controller.writeDragFile (bassLane);
    CHECK (bass.getFileName().contains (" - Bass"));
    CHECK (bass.getFileName().endsWith (" (synths).mid"));
    const auto kickFile = controller.writeDragFile (kickLane);
    CHECK_FALSE (kickFile.getFileName().contains ("synths")); // drums alone: nothing for a synth
    controller.setDragDrumsOnOneTrack (true);
    CHECK (controller.writeDragFile (loop).getFileName().endsWith (" - Loop (synths, 1 drum track).mid"));
    controller.setDragDrumsOnOneTrack (false);

    // Off: kept in the person's settings file, never in the project, and the drags are the files as they are.
    controller.setDragForSynths (false);
    CHECK_FALSE (controller.dragForSynths());
    CHECK (attribute ("drag_for_synths") == "0");
    const auto plain = controller.writeDragFile (loop);
    CHECK (plain.getFileName().endsWith (" - Loop.mid"));
    CHECK (bytesOf (plain) == syn.smfs[0]);
    CHECK (bytesOf (controller.writeDragFile (kickLane)) == bytesOf (kickFile));

    // Every instance shares it.
    SyntheticPlugin other (twoKits ("SNT01", "SNT02"));
    auto& otherController = static_cast<ControllerImpl&> (other->controller());
    otherController.setUserSettingsForTesting (*plugin.settings);
    otherController.setDragFolderForTesting (drag.folder);
    CHECK_FALSE (otherController.dragForSynths());
    CHECK (bytesOf (otherController.writeDragFile (loop)) == syn.smfs[0]);

    // On again: the attribute is gone.
    controller.setDragForSynths (true);
    CHECK (otherController.dragForSynths());
    CHECK (attribute ("drag_for_synths") == "(none)");
    CHECK (bytesOf (controller.writeDragFile (loop)) == expected.value);
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
    plugin.content->clearFoldersRead();
    const auto first = controller.exportSound ({ "XPT01", riser });
    INFO (first.problem);
    REQUIRE (first.file.existsAsFile());
    CHECK (first.problem.isEmpty());
    CHECK (first.file == samples.folder.getChildFile ("XPT01 Synthetic 01").getChildFile ("Riser - XPT01 128 Gm 6A.wav"));
    CHECK (bytesOf (first.file) == wav);
    CHECK (plugin.content->foldersRead() == std::vector<juce::File> { SyntheticContent::folderOf ("XPT01") });

    const juce::Time past (2001, 0, 1, 12, 0, 0, 0, false);
    REQUIRE (first.file.setLastModificationTime (past));
    const auto second = controller.exportSound ({ "XPT01", riser });
    CHECK (second.file == first.file);
    CHECK (second.file.getLastModificationTime() == past); // nothing written

    // A kit other than the one playing, once the service has found its samples: read from its own folder (every
    // synthetic kit serves the same bytes, so only the folder tells).
    REQUIRE (test::waitFor ([&] { return controller.kits()[1].content.state == ContentState::ready; }));
    plugin.content->clearFoldersRead();
    const auto other = controller.exportSound ({ "XPT02", riser });
    INFO (other.problem);
    CHECK (other.file == samples.folder.getChildFile ("XPT02 Synthetic 02").getChildFile ("Riser - XPT02 128 Gm 6A.wav"));
    CHECK (bytesOf (other.file) == wav);
    CHECK (plugin.content->foldersRead() == std::vector<juce::File> { SyntheticContent::folderOf ("XPT02") });

    // Nothing to copy: a kit this build doesn't carry, a file the kit doesn't list.
    const auto unknown = controller.exportSound ({ "NOPE", riser });
    CHECK (unknown.file == juce::File());
    CHECK (unknown.problem.contains ("NOPE"));
    CHECK (unknown.sourceProblem);
    for (const auto file : { -1, 30 })
    {
        const auto none = controller.exportSound ({ "XPT01", file });
        CHECK (none.file == juce::File());
        CHECK (none.problem.isNotEmpty());
        CHECK (none.sourceProblem);
    }
    CHECK (samples.folder.getNumberOfChildFiles (juce::File::findFilesAndDirectories) == 2); // the two kits' folders
}

TEST_CASE ("exportSound says whose fault a failed copy is, as exportCopy found it: the kit's file or the samples folder", "[controller][sounds]")
{
    // The Controller's own checks pass (the kit is found ready), so exportCopy is what refuses: its flags and detail must
    // reach the drawer as they are, true and false alike, or the drawer gives the wrong advice.
    const auto riser = synFile ("FX/Riser.wav");
    TempFolder samples;

    SECTION ("installed bytes that aren't kit.json's: the kit's file, and the problem says to reinstall")
    {
        SyntheticContent::Options options;
        options.ids = { "SPF01" };
        options.alteredPath = "FX/Riser.wav";
        SyntheticPlugin plugin (options);
        auto& controller = static_cast<ControllerImpl&> (plugin->controller());
        controller.setSamplesFolderForTesting (samples.folder);
        test::prepareWithKit (*plugin);
        REQUIRE (controller.status().state == ContentState::ready);

        const auto refused = controller.exportSound ({ "SPF01", riser });
        INFO (refused.problem);
        CHECK (refused.file == juce::File());
        CHECK (refused.problem == "Riser.wav isn't the file SPF01's content v1 lists: reinstall SPF01");
        CHECK (refused.sourceProblem);
        CHECK (refused.reinstall);
        CHECK (refused.detail.isEmpty());
        CHECK (samples.folder.getNumberOfChildFiles (juce::File::findFilesAndDirectories) == 0);
    }

    SECTION ("the installed file gone since the kit loaded: the kit's file, named short, the folder in the detail")
    {
        SyntheticContent::Options options;
        options.ids = { "SPF02" };
        SyntheticPlugin plugin (options);
        auto& controller = static_cast<ControllerImpl&> (plugin->controller());
        controller.setSamplesFolderForTesting (samples.folder);
        test::prepareWithKit (*plugin);
        REQUIRE (controller.status().state == ContentState::ready);
        plugin.content->setSamples (SyntheticContent::Samples::missing); // the drive unplugged, the file deleted
        REQUIRE (controller.status().state == ContentState::ready); // the Controller still sees the kit ready

        const auto refused = controller.exportSound ({ "SPF02", riser });
        INFO (refused.problem);
        CHECK (refused.file == juce::File());
        CHECK (refused.problem == "SPF02's FX/Riser.wav is missing");
        CHECK (refused.detail == "missing: FX/Riser.wav: no such file in " + SyntheticContent::folderOf ("SPF02").getFullPathName());
        CHECK (refused.sourceProblem);
        CHECK_FALSE (refused.reinstall);
    }

    SECTION ("a samples folder that can't be written: the folder's fault, not the kit's")
    {
        SyntheticContent::Options options;
        options.ids = { "SPF03" };
        SyntheticPlugin plugin (options);
        const auto blocker = samples.folder.getChildFile ("Samples");
        REQUIRE (samples.folder.createDirectory().wasOk());
        REQUIRE (blocker.replaceWithText ("a file, not a folder"));
        auto& controller = static_cast<ControllerImpl&> (plugin->controller());
        controller.setSamplesFolderForTesting (blocker);
        test::prepareWithKit (*plugin);
        REQUIRE (controller.status().state == ContentState::ready);

        const auto refused = controller.exportSound ({ "SPF03", riser });
        INFO (refused.problem);
        CHECK (refused.file == juce::File());
        CHECK (refused.problem == "the samples folder can't be written to"); // no path: the drawer's line stays short
        CHECK (refused.detail == "can't make the folder " + blocker.getChildFile ("SPF03 Synthetic 03").getFullPathName());
        CHECK_FALSE (refused.sourceProblem);
        CHECK_FALSE (refused.reinstall);
    }

    SECTION ("a copy made: nobody's fault")
    {
        SyntheticContent::Options options;
        options.ids = { "SPF04" };
        SyntheticPlugin plugin (options);
        auto& controller = static_cast<ControllerImpl&> (plugin->controller());
        controller.setSamplesFolderForTesting (samples.folder);
        test::prepareWithKit (*plugin);
        REQUIRE (controller.status().state == ContentState::ready);

        const auto copy = controller.exportSound ({ "SPF04", riser });
        INFO (copy.problem);
        CHECK (copy.file.existsAsFile());
        CHECK (copy.problem.isEmpty());
        CHECK (copy.detail.isEmpty());
        CHECK_FALSE (copy.sourceProblem);
        CHECK_FALSE (copy.reinstall);
    }
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
    CHECK (early.sourceProblem);

    test::prepare (*plugin, 48000.0, 512);
    REQUIRE (test::waitForStatus (*plugin, ContentState::missing));
    const auto refused = controller.exportSound ({ "MIS01", riser });
    CHECK (refused.file == juce::File());
    CHECK (refused.problem.contains ("no content folder holds MIS01"));
    CHECK (refused.sourceProblem); // the kit's samples, not the samples folder
    CHECK (plugin.content->fileReads.load() == 0); // never read
    CHECK (samples.folder.getNumberOfChildFiles (juce::File::findFilesAndDirectories) == 0);
}

TEST_CASE ("audition is refused, with the reason, while the kit's samples aren't there", "[controller][sounds][audition]")
{
    SyntheticContent::Options options;
    options.ids = { "MIS02" };
    options.samples = SyntheticContent::Samples::missing;
    SyntheticPlugin plugin (options);
    auto& controller = plugin->controller();
    const auto riser = synFile ("FX/Riser.wav");

    // Before the first content check: not found yet.
    CHECK (controller.audition ({ "MIS02", riser }) == 0u);
    CHECK (controller.auditionState().problem.isNotEmpty());

    test::prepare (*plugin, 48000.0, 512);
    REQUIRE (test::waitForStatus (*plugin, ContentState::missing));
    CHECK (controller.audition ({ "MIS02", riser }) == 0u);
    const auto state = controller.auditionState();
    CHECK (state.problem.contains ("no content folder holds MIS02"));
    CHECK (state.latest == 0u);
    CHECK (state.sounding == 0u);
    CHECK_FALSE (state.waiting);
    CHECK_FALSE (state.hostSilent);

    // Nothing to play: a kit this build doesn't carry, a file the kit doesn't list.
    CHECK (controller.audition ({ "NOPE", riser }) == 0u);
    CHECK (controller.auditionState().problem.contains ("NOPE"));
    for (const auto file : { -1, 30 })
    {
        CHECK (controller.audition ({ "MIS02", file }) == 0u);
        CHECK (controller.auditionState().problem.contains ("MIS02"));
    }

    CHECK (plugin.content->fileReads.load() == 0); // never read
    CHECK_FALSE (plugin->auditionPlayer().offered());
    CHECK_FALSE (plugin->soundService().started()); // nothing to read, so no thread
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
    // cache, and anything inside them, are refused, and nothing changes. So are Wubansia's own folders: the app
    // data's "Wubansia Audio", which holds Content and settings.xml and which INSTALL.md's Uninstall deletes, and the
    // caches' "Wubansia Audio" around the drag cache, which cleaners empty.
    test::prepareWithKit (*plugin); // the service has searched SMP01's folder
    const auto kitFolder = SyntheticContent::folderOf ("SMP01");
    const auto contentRoot = kitFolder.getParentDirectory();
    const auto appData = ContentLocator::appDataFolder(); // <app data>/Wubansia Audio/Wubansia
    const auto ownCaches = dragout::cacheFolder().getParentDirectory().getParentDirectory(); // <caches>/Wubansia Audio
    REQUIRE (appData.getParentDirectory().getFileName() == "Wubansia Audio");
    REQUIRE (ownCaches.getFileName() == "Wubansia Audio");
    const juce::File refusals[] { kitFolder, kitFolder.getChildFile ("Mine"), contentRoot, contentRoot.getChildFile ("Samples"),
        ContentLocator::platformDefaultFolder(), ContentLocator::platformDefaultFolder().getChildFile ("Samples"), drag.folder,
        drag.folder.getChildFile ("Samples"), dragout::cacheFolder(), dragout::cacheFolder().getChildFile ("Samples"), appData,
        appData.getChildFile ("Samples"), appData.getParentDirectory(), appData.getParentDirectory().getChildFile ("Samples"), ownCaches,
        ownCaches.getChildFile ("Samples"), ownCaches.getChildFile ("Wubansia") };
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

TEST_CASE ("the drum option and the samples folder share one settings file, a test processor's own, and keep each other", "[controller][dragout][sounds]")
{
    // A processor test's settings are a file of its own (test::SyntheticPlugin): the Controller keeps both of its
    // settings there, so no test reads or writes the person's settings.xml for either, and neither setter drops the other.
    SyntheticPlugin plugin (twoKits ("SET01", "SET02"));
    auto& controller = static_cast<ControllerImpl&> (plugin->controller());
    TempFolder temp;
    const auto file = plugin.settings->file();
    const auto attribute = [&file] (const char* name)
    {
        const auto xml = juce::XmlDocument::parse (file);
        REQUIRE (xml != nullptr);
        return xml->hasAttribute (name) ? xml->getStringAttribute (name) : juce::String ("(none)");
    };

    // A samples folder in the test's own file is the Controller's. Required: otherwise the calls below would write the
    // person's settings.xml.
    const auto mine = temp.folder.getChildFile ("My Samples");
    REQUIRE (plugin.settings->setSamplesFolder (mine));
    REQUIRE (controller.samplesFolder().getFullPathName() == mine.getFullPathName());

    controller.setDragDrumsOnOneTrack (true);
    CHECK (attribute ("drag_drums_one_track") == "1");
    CHECK (attribute ("samples_folder") == mine.getFullPathName());
    controller.setDragForSynths (false);
    CHECK (attribute ("drag_for_synths") == "0");
    CHECK (attribute ("drag_drums_one_track") == "1");
    CHECK (attribute ("samples_folder") == mine.getFullPathName());

    const auto other = temp.folder.getChildFile ("Other Samples");
    CHECK (controller.setSamplesFolder (other));
    CHECK (attribute ("samples_folder") == other.getFullPathName());
    CHECK (attribute ("drag_drums_one_track") == "1");

    // Another instance on the same settings sees both, and so does a fresh reader of the file.
    SyntheticPlugin second (twoKits ("SET01", "SET02"));
    auto& secondController = static_cast<ControllerImpl&> (second->controller());
    secondController.setUserSettingsForTesting (*plugin.settings);
    CHECK (secondController.samplesFolder() == other);
    CHECK (secondController.dragDrumsOnOneTrack());
    CHECK_FALSE (secondController.dragForSynths());
    CHECK (UserSettings (file).samplesFolder() == other);
    CHECK (UserSettings (file).dragDrumsOnOneTrack());
    CHECK_FALSE (UserSettings (file).dragForSynths());
    CHECK (attribute ("drag_for_synths") == "0");

    // Each, cleared, removes its own attribute and nothing else.
    controller.setDragDrumsOnOneTrack (false);
    CHECK (attribute ("drag_drums_one_track") == "(none)");
    CHECK (attribute ("samples_folder") == other.getFullPathName());
    CHECK_FALSE (secondController.dragDrumsOnOneTrack());
    controller.setDragDrumsOnOneTrack (true);
    CHECK (controller.setSamplesFolder ({}));
    CHECK (attribute ("samples_folder") == "(none)");
    CHECK (attribute ("drag_drums_one_track") == "1");
    CHECK (controller.samplesFolder() == sounds::defaultSamplesFolder());
    CHECK (secondController.samplesFolder() == sounds::defaultSamplesFolder());
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

TEST_CASE ("the drawer on the real processor: it lists the kit's sounds, plays one and copies its file", "[controller][editor][sounds]")
{
    SyntheticPlugin plugin (twoKits ("DRW01", "DRW02"));
    TempFolder samples;
    auto& controller = static_cast<ControllerImpl&> (plugin->controller());
    controller.setSamplesFolderForTesting (samples.folder);
    test::prepareWithKit (*plugin);

    test::withEditor (*plugin, [&] (juce::AudioProcessorEditor& base) {
        auto* editor = dynamic_cast<PluginEditor*> (&base);
        REQUIRE (editor != nullptr);
        editor->tick();
        clickOn (component<juce::Button> (*editor, "samples"));
        editor->tick();
        REQUIRE (editor->isSamplesPanelShown());
        auto& drawer = component<editor::SamplesPanel> (*editor, "samples_drawer");
        juce::Array<juce::File> revealed;
        drawer.revealFile = [&revealed] (const juce::File& file) { revealed.add (file); };
        drawer.startDrag = [] (const juce::File&, juce::Component&) { return true; };

        // The synthetic kit has no Accents: the drawer opens on its first category, FX, which holds the riser.
        CHECK (drawer.browsedKit() == "DRW01");
        CHECK (drawer.category() == SoundCategory::fx);
        REQUIRE (drawer.numRows() == 1);
        auto& riser = component<editor::SoundRow> (*editor, "sound_row_0");
        CHECK (riser.look().name == "Riser");
        CHECK_FALSE (riser.look().dimmed);

        // A press plays it, through the sound thread.
        clickOn (riser);
        CHECK (controller.auditionState().latest != 0u);
        CHECK (controller.auditionState().problem.isEmpty());
        CHECK (plugin->soundService().started());

        // The tile names the copy; a click makes it, byte for byte, and shows it.
        auto& tile = component<editor::SoundDragTile> (*editor, "sound_drag");
        CHECK (tile.fileName() == "Riser - DRW01 128 Gm 6A.wav");
        clickOn (tile);
        REQUIRE (revealed.size() == 1);
        CHECK (revealed[0] == samples.folder.getChildFile ("DRW01 Synthetic 01").getChildFile ("Riser - DRW01 128 Gm 6A.wav"));
        CHECK (bytesOf (revealed[0]) == engine::testing::syntheticContent().wavs[(std::size_t) synFile ("FX/Riser.wav")]);

        // Closing the drawer stops it.
        clickOn (component<juce::Button> (*editor, "samples_close"));
        CHECK_FALSE (editor->isSamplesPanelShown());
        CHECK (controller.auditionState().sounding == 0u);
        CHECK_FALSE (controller.auditionState().waiting);
    });
    CHECK (plugin->getActiveEditor() == nullptr);
}
