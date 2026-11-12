#pragma once

// A Controller (plugin/source/Controller.h) for editor tests: it shows whatever a test puts in its
// fields and records every request, so the editor (P10) can be built and tested before ControllerImpl
// (P9) exists, and without an engine, a kit or content.
//
//     wubansia::test::FakeController controller;
//     controller.contentStatus.state = wubansia::ContentState::missing;
//     ... build the editor against it, click ...
//     CHECK (controller.selectedKits == juce::StringArray { "FUT02" });

#include "Controller.h"

#include <juce_core/juce_core.h>

#include <utility>
#include <vector>

namespace wubansia::test
{
    class FakeController : public Controller
    {
    public:
        FakeController()
        {
            KitCard fut01;
            fut01.id = "FUT01";
            fut01.title = "Alloy Signal";
            fut01.direction = "future garage / 2-step";
            fut01.key = "F# minor";
            fut01.camelot = "11A";
            fut01.bpm = 134;
            fut01.contentVersion = 1;
            fut01.laneLabels = { "Kick", "Clap", "Hats", "Perc", "Bass", "Lead", "Chords", "Vox", "Accents", "FX" };
            fut01.phraseLanes = engine::laneBit (engine::Lane::lead) | engine::laneBit (engine::Lane::vox);
            fut01.abA = { "Kick Split", "Warp Clean", "Organ Stab" };
            fut01.abB = { "Kick Garage", "Warp Warm", "E Piano Stab" };
            fut01.content.state = ContentState::ready;

            KitCard fut02;
            fut02.id = "FUT02";
            fut02.title = "Orbit Relay";
            fut02.direction = "sci-fi club";
            fut02.key = "D minor";
            fut02.camelot = "7A";
            fut02.bpm = 130;
            fut02.contentVersion = 1;
            fut02.laneLabels = { "Kick", "Clap", "Hats", "Perc", "Bass", "Arp", "Pad", "Vox", "Accents", "FX" };
            fut02.phraseLanes = engine::laneBit (engine::Lane::bass) | engine::laneBit (engine::Lane::lead) | engine::laneBit (engine::Lane::vox);
            fut02.abA = { "", "Roll Bass Clean", "" };
            fut02.abB = { "", "Roll Bass Warm", "" };
            fut02.content.state = ContentState::missing;
            fut02.content.problem = "no folder holds FUT02";

            kitCards = { fut01, fut02 };
            contentStatus = fut01.content;
            sectionList = { { "Intro", 1, 16 }, { "Groove", 17, 48 }, { "Break", 65, 16 } };
            transportState.bpm = 134.0;
            phraseFileNames = { "FUT01_Phrase_ChromePluck_2bars.mid", "FUT01_Phrase_VoxSynth_2bars.mid" };
            dragFolder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("WubansiaFakeDrag");
        }

        // What the editor sees. Tests set these.
        std::vector<KitCard> kitCards;
        juce::String current = "FUT01";
        ContentStatus contentStatus;
        Snapshot nextSnapshot;
        std::vector<SectionInfo> sectionList;
        TransportState transportState;
        bool previewOn = false;
        engine::MidiInMode midiMode = engine::MidiInMode::multi;
        engine::Lane midiLane = engine::Lane::kick;
        engine::PhraseMode phrases = engine::PhraseMode::automatic;
        bool drumsOneTrack = false; // what dragDrumsOnOneTrack () gives; another instance may change it
        juce::StringArray phraseFileNames;
        juce::File dragFolder; // where writeDragFile writes
        bool dragFails = false;

        // What the editor asked for. Tests read these.
        juce::StringArray selectedKits;
        juce::Array<juce::File> contentFolders;
        std::vector<std::pair<bool, double>> transportRequests; // (playing, bpm)
        std::vector<bool> previewRequests;
        std::vector<bool> drumOptionRequests; // setDragDrumsOnOneTrack () calls, in order
        std::vector<DragRequest> dragRequests;
        int snapshotsTaken = 0;

        std::vector<KitCard> kits() const override { return kitCards; }
        juce::String currentKit() const override { return current; }

        void selectKit (const juce::String& id) override
        {
            selectedKits.add (id);
            for (const auto& card : kitCards)
                if (card.id == id)
                {
                    current = id;
                    contentStatus = card.content;
                }
        }

        ContentStatus status() const override { return contentStatus; }

        void setContentFolder (const juce::File& folder) override
        {
            contentFolders.add (folder);
            contentStatus.folder = folder;
        }

        // Like the real one, hits and peaks are since the previous snapshot.
        Snapshot snapshot() override
        {
            ++snapshotsTaken;
            auto taken = nextSnapshot;
            nextSnapshot.laneHits = 0;
            nextSnapshot.lanePeak = {};
            return taken;
        }

        std::vector<SectionInfo> sections() const override { return sectionList; }
        TransportState transport() const override { return transportState; }

        void setTransport (bool playing, double bpm) override
        {
            transportRequests.emplace_back (playing, bpm);
            transportState.playing = playing;
            transportState.standaloneBpm = bpm;
        }

        bool preview() const override { return previewOn; }

        void setPreview (bool on) override
        {
            previewRequests.push_back (on);
            previewOn = on;
        }

        engine::MidiInMode midiInMode() const override { return midiMode; }
        engine::Lane midiInLane() const override { return midiLane; }

        void setMidiInMode (engine::MidiInMode mode, engine::Lane lane) override
        {
            midiMode = mode;
            midiLane = lane;
        }

        engine::PhraseMode phraseMode() const override { return phrases; }
        void setPhraseMode (engine::PhraseMode mode) override { phrases = mode; }

        bool dragDrumsOnOneTrack() const override { return drumsOneTrack; }

        void setDragDrumsOnOneTrack (bool on) override
        {
            drumOptionRequests.push_back (on);
            drumsOneTrack = on;
        }

        juce::StringArray phraseFiles() const override { return phraseFileNames; }

        // Writes a minimal, valid SMF (one empty track) named for the request.
        juce::File writeDragFile (const DragRequest& request) override
        {
            dragRequests.push_back (request);
            if (dragFails || ! dragFolder.createDirectory())
                return {};

            juce::String name = current + " - ";
            switch (request.what)
            {
                case DragWhat::loop: name << "Loop.mid"; break;
                case DragWhat::song: name << "Song.mid"; break;
                case DragWhat::lane: name << juce::String (std::string (engine::laneId (request.lane))) << ".mid"; break;
                case DragWhat::phrase: name = request.phraseFile; break;
            }
            static constexpr unsigned char smf[] = { 'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 1, 0, 1, 0x03, 0xC0,
                'M', 'T', 'r', 'k', 0, 0, 0, 4, 0, 0xFF, 0x2F, 0 };
            auto file = dragFolder.getChildFile (name);
            return file.replaceWithData (smf, sizeof (smf)) ? file : juce::File();
        }
    };
}
