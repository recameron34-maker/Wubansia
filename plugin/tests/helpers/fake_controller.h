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

#include <functional>
#include <initializer_list>
#include <map>
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
            fut01.phraseChords = { "F#m9", "Bm9" }; // the kit line's "over F#m9 – Bm9"
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
            dragFolder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("WubansiaFakeDrag");

            // A slice of FUT01's and FUT02's catalogs (file indices, names and lengths as their kit.json has them):
            // every category for FUT01, a few sounds for FUT02.
            using C = SoundCategory;
            auto reverseStab = fakeSound ("FUT01/reverse_stab", "Reverse Stab", C::accents, { { 158, "Reverse Stab v1", 1.3, 2 }, { 159, "Reverse Stab v2", 1.3, 2 } }, 1);
            reverseStab.endAligned = true;
            const auto raveStab = fakeSound ("FUT01/rave_stab", "Rave Stab", C::accents, { { 165, "Rave Stab v1", 0.7081, 2 }, { 166, "Rave Stab v2", 0.7081, 2 } }, 1);
            const auto alloyBell = fakeSound ("FUT01/alloy_bell", "Alloy Bell", C::accents, layered (171, "Alloy Bell", 4, 2, 2.49, 2), 6);
            auto shiftRiser = fakeSound ("FUT01/shift_riser", "Shift Riser", C::fx, { { 180, "Shift Riser", 14.3284, 2 } });
            shiftRiser.endAligned = true;
            shiftRiser.bars = 8;
            auto voxSynth = fakeSound ("FUT01/vox_synth", "Vox Synth", C::vox, notes (127, "Vox Synth", { "A4", "C#5", "F#5" }, 1.9, 2), 5);
            voxSynth.leadInMs = 6;
            const auto chromePluck = fakeSound ("FUT01/chrome_pluck", "Chrome Pluck", C::synths,
                notes (101, "Chrome Pluck", { "C4", "D#4", "F#4", "A4", "C5", "D#5", "F#5", "A5", "C6" }, 1.7, 2), 5);
            const auto warpClean = fakeSound ("FUT01/warp_clean", "Warp Clean", C::bass, notes (89, "Warp Clean", { "F#1", "A1", "C#2" }, 4.1, 2), 1);
            const auto kickSplit = fakeSound ("FUT01/kick_split", "Kick Split", C::drums, layered (0, "Kick Split", 5, 2, 0.457, 1), 8);
            auto pluckPhrase = fakeSound ("FUT01/Loops/Chrome Pluck Phrase.wav", "Chrome Pluck Phrase", C::loops, { { 187, "Chrome Pluck Phrase", 3.5821, 2 } });
            pluckPhrase.part = C::synths; // a lead phrase
            pluckPhrase.bars = 2;
            pluckPhrase.loopBars = 2;
            soundLists["FUT01"] = { reverseStab, raveStab, alloyBell, shiftRiser, voxSynth, chromePluck, warpClean, kickSplit, pluckPhrase };

            auto orbitLine = fakeSound ("FUT02/Loops/Orbit Line Clean.wav", "Orbit Line Clean", C::loops, { { 131, "Orbit Line Clean", 3.6923, 1 } });
            orbitLine.part = C::bass; // a bass line
            orbitLine.bars = 2;
            orbitLine.loopBars = 2;
            soundLists["FUT02"] = { fakeSound ("FUT02/glass_ping", "Glass Ping", C::accents, layered (109, "Glass Ping", 2, 2, 2.5, 2), 2),
                fakeSound ("FUT02/kick_split", "Kick Split", C::drums, layered (0, "Kick Split", 5, 2, 0.408, 1), 8), orbitLine };
            samplesFolderChosen = fakeSamplesFolder();
            phraseList = fakePhrases();
        }

        // A library phrase as the Controller lists it: 8 bars, a roll of a few notes, on the lead.
        static PhraseInfo libraryPhrase (const char* id, const char* name, const char* kind, std::uint64_t seed, juce::StringArray tags)
        {
            PhraseInfo p;
            p.id = id;
            p.name = name;
            p.kind = kind;
            p.origin = PhraseOrigin::library;
            p.bars = 8;
            p.seed = seed;
            p.tags = std::move (tags);
            p.roll = { { 0, 66, 2 }, { 4, 69, 2 }, { 8, 73, 4 }, { 64, 71, 8 }, { 120, 66, 8 } };
            p.sound = "Lead";
            p.lanes = engine::laneBit (engine::Lane::lead);
            return p;
        }

        // FUT01's list in the fake: five library phrases (two hooks, a call and answer, a period, an arp), then its own six.
        static std::vector<PhraseInfo> fakePhrases()
        {
            std::vector<PhraseInfo> list { libraryPhrase ("hook-0", "Night Bus", "hook", 0, { "steady", "mid" }),
                libraryPhrase ("hook-3", "Glass Hour", "hook", 3, { "busy", "high" }),
                libraryPhrase ("answer-1", "Low Signal", "answer", 1, { "steady", "mid" }),
                libraryPhrase ("period-0", "Cold Harbour", "period", 0, { "sparse", "mid", "breath" }),
                libraryPhrase ("arp-2", "Neon Stair", "arp", 2, { "busy", "high" }) };
            for (auto own : ownPhrases ("FUT01"))
            {
                own.sound = own.name.startsWith ("Warp") ? "Bass" : "Lead";
                own.lanes = engine::laneBit (own.name.startsWith ("Warp") ? engine::Lane::bass : engine::Lane::lead);
                own.roll = { { 0, 66, 1 }, { 2, 69, 1 } };
                list.push_back (own);
            }
            return list;
        }

        // Each kit's own phrases as the Controller lists them (ADR 0031): its phrase files by name, then its lead line.
        // ControllerTests holds them to the real catalog's.
        static std::vector<PhraseInfo> ownPhrases (const juce::String& kit)
        {
            const auto own = [] (const char* file, const char* name, int bars) { return PhraseInfo { file, name, "kit", PhraseOrigin::kit, bars }; };
            if (kit == "FUT01")
                return { own ("FUT01_Phrase_EPianoStabRhythm_2bars.mid", "E Piano Stab Rhythm", 2), own ("FUT01_Phrase_OrganRiff_4bars.mid", "Organ Riff", 4),
                    own ("FUT01_Phrase_PluckRiff_4bars.mid", "Pluck Riff", 4), own ("FUT01_Phrase_VoxSynthRiff_2bars.mid", "Vox Synth Riff", 2),
                    own ("FUT01_Phrase_WarpRiff_2bars.mid", "Warp Riff", 2), own ("FUT01_Lead_ChromePluck_2bars.mid", "Lead Chrome Pluck", 2) };
            if (kit == "FUT02")
                return { own ("FUT02_Phrase_GlassMelody_4bars.mid", "Glass Melody", 4), own ("FUT02_Phrase_LaserStabRiff_2bars.mid", "Laser Stab Riff", 2),
                    own ("FUT02_Phrase_VoxChoirChords_4bars.mid", "Vox Choir Chords", 4), own ("FUT02_Phrase_VoxSynthRiff_2bars.mid", "Vox Synth Riff", 2),
                    own ("FUT02_Lead_ChromeArp_2bars.mid", "Lead Chrome Arp", 2) };
            if (kit == "FUT03")
                return { own ("FUT03_Phrase_DubChords_4bars.mid", "Dub Chords", 4), own ("FUT03_Phrase_KeysArp_4bars.mid", "Keys Arp", 4),
                    own ("FUT03_Phrase_RubberRiff_2bars.mid", "Rubber Riff", 2), own ("FUT03_Phrase_VoxSynthRiff_2bars.mid", "Vox Synth Riff", 2),
                    own ("FUT03_Lead_KeysEP_2bars.mid", "Lead Keys EP", 2) };
            if (kit == "FUT04")
                return { own ("FUT04_Phrase_AcidStabRiff_2bars.mid", "Acid Stab Riff", 2), own ("FUT04_Phrase_MetalChords_4bars.mid", "Metal Chords", 4),
                    own ("FUT04_Phrase_RaveStabRiff_2bars.mid", "Rave Stab Riff", 2), own ("FUT04_Phrase_VoxSynthRiff_2bars.mid", "Vox Synth Riff", 2),
                    own ("FUT04_Lead_ModBlip_2bars.mid", "Lead Mod Blip", 2) };
            return {};
        }

        // Where samplesFolder () points until a test or the editor chooses another.
        static juce::File fakeSamplesFolder()
        {
            return juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("WubansiaFakeSamples");
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
        engine::PhraseMode phraseLaneMode = engine::PhraseMode::automatic; // what phraseMode () gives
        bool drumsOneTrack = false; // what dragDrumsOnOneTrack () gives; another instance may change it
        bool synthsOn = true; // what dragForSynths () gives; another instance may change it
        DragPart partOn = DragPart::all; // what dragPart () gives (ADR 0029); the Loop and Song files are named for it
        juce::File dragFolder; // where writeDragFile writes
        bool dragFails = false;
        std::map<juce::String, std::vector<SoundInfo>> soundLists; // what sounds () gives, by kit
        juce::File samplesFolderChosen; // what samplesFolder () gives; exportSound writes into <it>/<kit>/
        bool exportFails = false; // the samples folder's fault: "the fake's disk is full"
        juce::String exportSourceProblem; // when set, every export fails with it, the kit's own file's fault
        std::function<SoundExport (const SoundRef&)> exportOutcome; // when set, what exportSound gives (it writes nothing)
        bool refuseSamplesFolder = false; // setSamplesFolder () says no, as for a content or drag folder
        // What auditionState () gives. audition () sets it as a host running blocks would: the next ticket, sounding at
        // once; with auditionFails set, 0 and a problem of its own. stopAudition () and selectKit () of a kit in kitCards clear
        // sounding. Tests may set it directly (waiting, hostSilent...).
        AuditionState auditionStatus;
        bool auditionFails = false;
        // What phrases () and phrasesGeneration () give (ADR 0031): FUT01's own six until a test sets others. A test that
        // changes the list bumps the generation, as a kit change does in the real one; selectKit () changes neither.
        std::vector<PhraseInfo> phraseList;
        std::uint64_t phraseGeneration = 1;
        // Drum kits (ADR 0030): what drumKitStatus () gives. exportDrumKit () counts the request and gives the next ticket,
        // saving for the current kit; with drumKitOutcome set, the status it returns for that request (by count, from 1),
        // whose ticket it gives. Tests may set drumKitState directly (ready, failed...).
        DrumKitStatus drumKitState;
        std::function<DrumKitStatus (int request)> drumKitOutcome;
        // What phraseAuditionState () gives. auditionPhrase () sets it as a playing host would: the next ticket, sounding at
        // once from bar 1; stopPhraseAudition () clears sounding, waiting and startedTransport. Tests may set it directly.
        PhraseAuditionState phraseAudition;
        bool moreBusy = false; // morePhrases () says no, as while a press is in flight

        // What the editor asked for. Tests read these.
        juce::StringArray selectedKits;
        juce::Array<juce::File> contentFolders;
        std::vector<std::pair<bool, double>> transportRequests; // (playing, bpm)
        std::vector<bool> previewRequests;
        std::vector<bool> drumOptionRequests; // setDragDrumsOnOneTrack () calls, in order
        std::vector<bool> synthOptionRequests; // setDragForSynths () calls, in order
        std::vector<DragPart> partRequests; // setDragPart () calls, in order
        std::vector<DragRequest> dragRequests;
        int snapshotsTaken = 0;
        mutable int soundsCalls = 0; // sounds (): the editor should ask once per kit it shows, not every tick
        std::vector<SoundRef> exportRequests;
        juce::Array<juce::File> samplesFolderRequests;
        std::vector<SoundRef> auditions; // audition () requests, in order
        int stops = 0; // stopAudition () calls
        mutable int phrasesCalls = 0; // phrases (): the Phrases drawer should ask once per generation, not every tick
        int drumKitRequests = 0; // exportDrumKit () calls
        juce::StringArray moreRequests; // morePhrases () calls, in order ("" for all kinds)
        juce::StringArray phraseAuditions; // auditionPhrase () ids, in order
        int phraseStops = 0; // stopPhraseAudition () calls
        std::vector<std::pair<juce::String, bool>> keptRequests; // setPhraseKept () calls, in order

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
                    // As ControllerImpl's: a kit chosen stops the audition (stops counts stopAudition () calls only).
                    auditionStatus.sounding = 0;
                    auditionStatus.waiting = false;
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

        engine::PhraseMode phraseMode() const override { return phraseLaneMode; }
        void setPhraseMode (engine::PhraseMode mode) override { phraseLaneMode = mode; }

        bool dragDrumsOnOneTrack() const override { return drumsOneTrack; }

        void setDragDrumsOnOneTrack (bool on) override
        {
            drumOptionRequests.push_back (on);
            drumsOneTrack = on;
        }

        bool dragForSynths() const override { return synthsOn; }

        void setDragForSynths (bool on) override
        {
            synthOptionRequests.push_back (on);
            synthsOn = on;
        }

        DragPart dragPart() const override { return partOn; }

        void setDragPart (DragPart part) override
        {
            partRequests.push_back (part);
            partOn = part;
        }

        // Writes a minimal, valid SMF (one empty track) named for the request: "FUT01 - Loop.mid", or with a part
        // "FUT01 - Song Drums.mid".
        juce::File writeDragFile (const DragRequest& request) override
        {
            dragRequests.push_back (request);
            if (dragFails || ! dragFolder.createDirectory())
                return {};

            juce::String name = current + " - ";
            const auto part = partOn == DragPart::all ? juce::String() : " " + juce::String (std::string (kDragPartNames[static_cast<std::size_t> (partOn)]));
            switch (request.what)
            {
                case DragWhat::loop: name << "Loop" << part << ".mid"; break;
                case DragWhat::song: name << "Song" << part << ".mid"; break;
                case DragWhat::lane: name << juce::String (std::string (engine::laneId (request.lane))) << ".mid"; break;
                case DragWhat::phrase: name = request.phraseFile; break;
                case DragWhat::generatedPhrase: name << "Phrase " << request.phraseFile << ".mid"; break;
            }
            static constexpr unsigned char smf[] = { 'M', 'T', 'h', 'd', 0, 0, 0, 6, 0, 1, 0, 1, 0x03, 0xC0,
                'M', 'T', 'r', 'k', 0, 0, 0, 4, 0, 0xFF, 0x2F, 0 };
            auto file = dragFolder.getChildFile (name);
            return file.replaceWithData (smf, sizeof (smf)) ? file : juce::File();
        }

        std::vector<SoundInfo> sounds (const juce::String& kit) const override
        {
            ++soundsCalls;
            const auto it = soundLists.find (kit);
            return it != soundLists.end() ? it->second : std::vector<SoundInfo> {};
        }

        std::uint32_t audition (const SoundRef& sound) override
        {
            auditions.push_back (sound);
            if (auditionFails)
            {
                auditionStatus.sounding = 0;
                auditionStatus.waiting = false;
                auditionStatus.hostSilent = false;
                auditionStatus.problem = "the fake can't play " + sound.kit + " file " + juce::String (sound.file);
                auditionStatus.detail = {}; // its own words, as the Controller's refusal: nothing of a failure before it
                auditionStatus.sourceProblem = false;
                auditionStatus.reinstall = false;
                return 0;
            }
            const auto ticket = auditionStatus.latest + 1;
            auditionStatus = {};
            auditionStatus.latest = ticket;
            auditionStatus.sounding = ticket;
            return ticket;
        }

        void stopAudition() override
        {
            ++stops;
            auditionStatus.sounding = 0;
            auditionStatus.waiting = false;
        }

        AuditionState auditionState() const override { return auditionStatus; }

        // Writes a tiny WAV (4 frames of 24-bit silence, 48 kHz mono) named for the file, "<label> - <KIT>.wav", into
        // <samplesFolderChosen>/<KIT>/; fails, writing nothing, when exportSourceProblem or exportFails is set. With
        // exportOutcome set, gives what it returns instead.
        SoundExport exportSound (const SoundRef& sound) override
        {
            exportRequests.push_back (sound);
            if (exportOutcome != nullptr)
                return exportOutcome (sound);
            SoundExport result;
            if (exportSourceProblem.isNotEmpty())
            {
                result.problem = exportSourceProblem;
                result.sourceProblem = true;
                return result;
            }
            if (exportFails)
            {
                result.problem = "the fake's disk is full";
                return result;
            }
            juce::String label = "File " + juce::String (sound.file);
            if (const auto it = soundLists.find (sound.kit); it != soundLists.end())
                for (const auto& s : it->second)
                    for (const auto& f : s.files)
                        if (f.file == sound.file)
                            label = f.label;
            static constexpr unsigned char wav[] = { 'R', 'I', 'F', 'F', 48, 0, 0, 0, 'W', 'A', 'V', 'E', 'f', 'm', 't', ' ', 16, 0, 0, 0,
                1, 0, 1, 0, 0x80, 0xBB, 0, 0, 0x80, 0x32, 0x02, 0, 3, 0, 24, 0, 'd', 'a', 't', 'a', 12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0 };
            const auto folder = samplesFolderChosen.getChildFile (sound.kit);
            const auto file = folder.getChildFile (label + " - " + sound.kit + ".wav");
            if (! folder.createDirectory() || ! file.replaceWithData (wav, sizeof (wav)))
            {
                result.problem = "the samples folder can't be written to"; // as exportCopy says it, the path in the detail
                result.detail = "can't write " + file.getFullPathName();
                return result;
            }
            result.file = file;
            return result;
        }

        juce::File samplesFolder() const override { return samplesFolderChosen; }

        bool setSamplesFolder (const juce::File& folder) override
        {
            samplesFolderRequests.add (folder);
            if (refuseSamplesFolder)
                return false;
            samplesFolderChosen = folder != juce::File() ? folder : fakeSamplesFolder();
            return true;
        }

        std::vector<PhraseInfo> phrases() const override
        {
            ++phrasesCalls;
            return phraseList;
        }

        std::uint64_t phrasesGeneration() const override { return phraseGeneration; }

        std::uint32_t exportDrumKit() override
        {
            ++drumKitRequests;
            if (drumKitOutcome != nullptr)
                drumKitState = drumKitOutcome (drumKitRequests);
            else
            {
                drumKitState = {};
                drumKitState.ticket = (std::uint32_t) drumKitRequests;
                drumKitState.stage = DrumKitStage::saving;
                drumKitState.kit = current;
            }
            return drumKitState.ticket;
        }

        DrumKitStatus drumKitStatus() const override { return drumKitState; }

        bool morePhrases (const juce::String& kind) override
        {
            moreRequests.add (kind);
            return ! moreBusy;
        }

        std::uint32_t auditionPhrase (const juce::String& id) override
        {
            phraseAuditions.add (id);
            const auto ticket = phraseAudition.latest + 1;
            phraseAudition = {};
            phraseAudition.latest = ticket;
            phraseAudition.sounding = ticket;
            phraseAudition.id = id;
            phraseAudition.startsAtBar = 1;
            return ticket;
        }

        void stopPhraseAudition() override
        {
            ++phraseStops;
            phraseAudition.sounding = 0;
            phraseAudition.waiting = false;
            phraseAudition.startedTransport = false;
        }

        PhraseAuditionState phraseAuditionState() const override { return phraseAudition; }

        void setPhraseKept (const juce::String& id, bool kept) override
        {
            keptRequests.emplace_back (id, kept);
            for (auto& p : phraseList)
                if (p.id == id)
                    p.kept = kept;
            ++phraseGeneration;
        }

    private:
        static SoundInfo fakeSound (const char* id, const char* name, SoundCategory category, std::vector<SoundFile> files, int defaultFile = 0)
        {
            SoundInfo s;
            s.id = id;
            s.name = name;
            s.category = category;
            s.part = category;
            s.files = std::move (files);
            s.defaultFile = defaultFile;
            return s;
        }

        // "<name> v1 rr1", "<name> v1 rr2" ... from file index first: velocity layers with round robins.
        static std::vector<SoundFile> layered (int first, const char* name, int layers, int roundRobins, double seconds, int channels)
        {
            std::vector<SoundFile> files;
            for (int v = 1; v <= layers; ++v)
                for (int rr = 1; rr <= roundRobins; ++rr)
                    files.push_back ({ first++, juce::String (name) + " v" + juce::String (v) + " rr" + juce::String (rr), seconds, channels });
            return files;
        }

        // "<name> <note> v1", "<name> <note> v2" per note, from file index first: a pitched instrument's soft and loud layers.
        static std::vector<SoundFile> notes (int first, const char* name, std::initializer_list<const char*> keys, double seconds, int channels)
        {
            std::vector<SoundFile> files;
            for (const auto* key : keys)
                for (int v = 1; v <= 2; ++v)
                    files.push_back ({ first++, juce::String (name) + " " + key + " v" + juce::String (v), seconds, channels });
            return files;
        }
    };
}
