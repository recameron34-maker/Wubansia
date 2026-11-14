// The editor's fake Controller (helpers/fake_controller.h): it compiles against Controller.h inside the
// plugin's build and records what the editor asks, so P10's editor tests can lean on it.

#include "helpers/fake_controller.h"

#include "kit/wav_reader.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <set>
#include <span>

TEST_CASE ("the fake controller shows what a test sets and records what the editor asks", "[controller]")
{
    wubansia::test::FakeController fake;
    wubansia::Controller& controller = fake; // the editor only ever sees the interface

    SECTION ("kits and content")
    {
        REQUIRE (controller.kits().size() == 2);
        CHECK (controller.currentKit() == "FUT01");
        CHECK (controller.status().state == wubansia::ContentState::ready);

        controller.selectKit ("FUT02");
        controller.selectKit ("NOPE"); // an unknown id is ignored
        CHECK (controller.currentKit() == "FUT02");
        CHECK (controller.status().state == wubansia::ContentState::missing);
        CHECK (fake.selectedKits == juce::StringArray { "FUT02", "NOPE" });

        const auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory);
        controller.setContentFolder (folder);
        CHECK (fake.contentFolders.size() == 1);
        CHECK (controller.status().folder == folder);
    }

    SECTION ("snapshots: hits and peaks are since the previous one")
    {
        fake.nextSnapshot.laneHits = 0b101u;
        fake.nextSnapshot.lanePeak[3] = 0.5f;
        fake.nextSnapshot.voices = 7;
        const auto first = controller.snapshot();
        const auto second = controller.snapshot();
        CHECK (first.laneHits == 0b101u);
        CHECK (first.lanePeak[3] > 0.0f);
        CHECK (second.laneHits == 0u);
        CHECK (second.lanePeak[3] <= 0.0f);
        CHECK (second.voices == 7);
        CHECK (fake.snapshotsTaken == 2);
    }

    SECTION ("transport, preview, MIDI in and phrase lanes")
    {
        controller.setTransport (true, 0.0);
        controller.setPreview (true);
        controller.setMidiInMode (wubansia::engine::MidiInMode::lane, wubansia::engine::Lane::bass);
        controller.setPhraseMode (wubansia::engine::PhraseMode::off);
        CHECK (controller.transport().playing);
        CHECK (fake.transportRequests.size() == 1);
        CHECK (controller.preview());
        CHECK (controller.midiInMode() == wubansia::engine::MidiInMode::lane);
        CHECK (controller.midiInLane() == wubansia::engine::Lane::bass);
        CHECK (controller.phraseMode() == wubansia::engine::PhraseMode::off);
    }

    SECTION ("drag-out writes one file per drag")
    {
        wubansia::DragRequest request;
        request.what = wubansia::DragWhat::lane;
        request.lane = wubansia::engine::Lane::bass;
        const auto file = controller.writeDragFile (request);
        CHECK (file.existsAsFile());
        CHECK (file.getFileName() == "FUT01 - bass.mid");
        CHECK (file.getSize() == 26);
        file.deleteFile();
        REQUIRE (fake.dragRequests.size() == 1);
        CHECK (fake.dragRequests[0] == request);

        fake.dragFails = true;
        CHECK (controller.writeDragFile ({}) == juce::File());
        CHECK (controller.phraseFiles().size() == 2);
    }
}

TEST_CASE ("the fake records sounds, auditions, stops and exports, and writes a tiny WAV", "[controller][sounds]")
{
    wubansia::test::FakeController fake;
    wubansia::Controller& controller = fake;
    const auto samples = juce::File::getSpecialLocation (juce::File::tempDirectory).getNonexistentChildFile ("WubansiaFakeSamplesTest", "", false);
    fake.samplesFolderChosen = samples;

    SECTION ("sounds: a slice of FUT01's catalog, every category in it, and counted calls")
    {
        const auto sounds = controller.sounds ("FUT01");
        REQUIRE_FALSE (sounds.empty());
        std::set<wubansia::SoundCategory> categories;
        for (const auto& s : sounds)
        {
            CAPTURE (s.name.toStdString());
            categories.insert (s.category);
            CHECK (s.id.startsWith ("FUT01/"));
            REQUIRE (s.defaultFile >= 0);
            CHECK ((std::size_t) s.defaultFile < s.files.size());
        }
        CHECK ((int) categories.size() == wubansia::kSoundCategories);
        CHECK_FALSE (controller.sounds ("FUT02").empty());
        CHECK (controller.sounds ("NOPE").empty());
        CHECK (fake.soundsCalls == 3);
    }

    SECTION ("exports: recorded, a tiny WAV named for the file, or the problem")
    {
        const auto sounds = controller.sounds ("FUT01");
        const auto& rave = sounds[1];
        REQUIRE (rave.name == "Rave Stab");
        const wubansia::SoundRef ref { "FUT01", rave.files[(std::size_t) rave.defaultFile].file };
        const auto exported = controller.exportSound (ref);
        INFO (exported.problem);
        REQUIRE (exported.file.existsAsFile());
        CHECK (exported.problem.isEmpty());
        CHECK (exported.file == samples.getChildFile ("FUT01").getChildFile ("Rave Stab v2 - FUT01.wav"));
        juce::MemoryBlock block;
        REQUIRE (exported.file.loadFileAsData (block));
        const auto info = wubansia::engine::readWavInfo (std::span<const std::uint8_t> (static_cast<const std::uint8_t*> (block.getData()), block.getSize()));
        REQUIRE (info.ok());
        CHECK (info.value.frames == 4);
        CHECK (info.value.channels == 1);
        CHECK (info.value.bitsPerSample == 24);
        REQUIRE (fake.exportRequests.size() == 1);
        CHECK (fake.exportRequests[0] == ref);

        fake.exportFails = true;
        const auto failed = controller.exportSound ({ "FUT02", 109 });
        CHECK (failed.file == juce::File());
        CHECK (failed.problem.isNotEmpty());
        CHECK (fake.exportRequests.size() == 2);
    }

    SECTION ("auditions and stops: recorded, a ticket each that sounds at once, or the problem")
    {
        const wubansia::SoundRef rave { "FUT01", 166 };
        CHECK (controller.auditionState().latest == 0u);
        const auto first = controller.audition (rave);
        CHECK (first > 0u);
        auto state = controller.auditionState();
        CHECK (state.latest == first);
        CHECK (state.sounding == first);
        CHECK_FALSE (state.waiting);
        CHECK_FALSE (state.hostSilent);
        CHECK (state.problem.isEmpty());

        const auto second = controller.audition ({ "FUT01", 165 });
        CHECK (second > first);
        CHECK (controller.auditionState().sounding == second);
        controller.stopAudition();
        CHECK (controller.auditionState().sounding == 0u);
        CHECK (controller.auditionState().latest == second);
        CHECK (fake.stops == 1);

        fake.auditionFails = true;
        CHECK (controller.audition ({ "FUT02", 109 }) == 0u);
        CHECK (controller.auditionState().problem.isNotEmpty());
        CHECK (controller.auditionState().sounding == 0u);
        REQUIRE (fake.auditions.size() == 3);
        CHECK (fake.auditions[0] == rave);
        CHECK (fake.auditions[2] == wubansia::SoundRef { "FUT02", 109 });

        // What the editor sees is whatever a test sets: a host that runs no blocks, say.
        fake.auditionStatus.hostSilent = true;
        CHECK (controller.auditionState().hostSilent);
    }

    SECTION ("a kit chosen stops the audition, as the Controller's selectKit does; an unknown id stops nothing")
    {
        const auto ticket = controller.audition ({ "FUT01", 166 });
        REQUIRE (controller.auditionState().sounding == ticket);
        controller.selectKit ("NOPE");
        CHECK (controller.auditionState().sounding == ticket);
        controller.selectKit ("FUT01"); // the kit that plays already: it stops all the same
        CHECK (controller.auditionState().sounding == 0u);
        CHECK (controller.auditionState().latest == ticket);
        CHECK_FALSE (controller.auditionState().waiting);
        CHECK (fake.stops == 0); // stops counts the editor's own stopAudition () calls
    }

    SECTION ("the samples folder: chosen, refused, back to the default")
    {
        const auto mine = samples.getChildFile ("Mine");
        CHECK (controller.samplesFolder() == samples);
        CHECK (controller.setSamplesFolder (mine));
        CHECK (controller.samplesFolder() == mine);
        fake.refuseSamplesFolder = true;
        CHECK_FALSE (controller.setSamplesFolder (samples.getChildFile ("Other")));
        CHECK (controller.samplesFolder() == mine);
        fake.refuseSamplesFolder = false;
        CHECK (controller.setSamplesFolder ({}));
        CHECK (controller.samplesFolder() == wubansia::test::FakeController::fakeSamplesFolder());
        CHECK (fake.samplesFolderRequests.size() == 3);
    }
    samples.deleteRecursively();
}
