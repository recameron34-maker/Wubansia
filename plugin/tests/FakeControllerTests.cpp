// The editor's fake Controller (helpers/fake_controller.h): it compiles against Controller.h inside the
// plugin's build and records what the editor asks, so P10's editor tests can lean on it.

#include "helpers/fake_controller.h"

#include <catch2/catch_test_macros.hpp>

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
