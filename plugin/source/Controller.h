#pragma once

// The editor's only API (P10), and later the S1f WebView bridge's: what the plugin shows and what a
// person can ask of it beyond the parameters, which the editor binds by parameter ID. ControllerImpl
// (P9) implements it over the processor; plugin/tests/helpers/fake_controller.h fakes it, so the
// editor can be built and tested without an engine.
//
// Every call comes from the message thread and returns at once: a kit load, a content check or a
// drag file that takes time is queued on the service thread, and status () and snapshot () report
// on it. The state-only settings set here (the kit, the content folder, the Standalone tempo, the
// MIDI-in mode and the phrase mode) mark the host's project as changed.

#include "engine.h"

#include <juce_core/juce_core.h>

#include <array>
#include <cstdint>
#include <vector>

namespace wubansia
{
    // Where a kit's samples stand.
    enum class ContentState {
        ready,
        loading,
        missing, // no folder holds the kit
        lfsPointers, // the files are Git LFS pointers, not audio
        mismatch, // a file isn't the listed 48 kHz WAV with the listed frames and channels
        failed // anything else; problem says what
    };

    struct ContentStatus
    {
        ContentState state = ContentState::missing;
        int percent = 0; // while loading, 0-100
        juce::String problem; // the first problem, for people: "Drums/Kick Garage v1 rr1.wav is a Git LFS pointer"
        juce::File folder; // where the kit was found; juce::File() when missing
        juce::StringArray searched; // every folder searched, in search order
    };

    // One embedded kit, for its card.
    struct KitCard
    {
        juce::String id; // "FUT01"
        juce::String title;
        juce::String direction;
        juce::String key; // "F# minor"
        juce::String camelot; // "11A"
        int bpm = 0;
        int contentVersion = 0;
        std::array<juce::String, engine::kLanes> laneLabels; // "" where the kit has no such lane
        std::uint32_t phraseLanes = 0; // bit per lane that has a recorded phrase
        std::array<juce::String, engine::kAbSwitches> abA; // the A/B labels; "" where the kit has no such switch
        std::array<juce::String, engine::kAbSwitches> abB;
        ContentStatus content;
    };

    // A Song-mode section of the current kit.
    struct SectionInfo
    {
        juce::String name; // "Drop 2"
        int startBar = 1; // 1-based
        int bars = 0;
    };

    // What the editor polls on its 30 Hz timer. The hits and peaks are since the previous snapshot ().
    struct Snapshot
    {
        bool playing = false;
        double bpm = 0.0;
        engine::PlayMode mode = engine::PlayMode::loop;
        double songTicks = -1.0; // 960-PPQ ticks into the playing timeline; -1 when not playing
        int section = -1; // index into sections (); -1 before the first, and in Loop mode
        std::uint32_t laneHits = 0; // bit per lane
        std::array<float, engine::kLanes> lanePeak {}; // linear
        std::uint32_t phraseRecorded = 0; // bit per lane playing its recorded phrase
        float gainReductionDb = 0.0f; // the club master's, >= 0
        int voices = 0;
    };

    struct TransportState
    {
        bool internal = false; // the plugin's own transport drives playback (Standalone, or Preview in a host)
        bool playing = false;
        double bpm = 0.0; // the tempo playback runs at
        double standaloneBpm = 0.0; // the setting; 0 follows the kit's tempo
    };

    enum class DragWhat {
        loop, // the 4-bar loop, the vox switch's file, muted lanes left out
        song, // the extended mix, the same way
        lane, // one lane's tracks from the current mode's file
        phrase // one phrase file
    };

    struct DragRequest
    {
        DragWhat what = DragWhat::loop;
        engine::Lane lane = engine::Lane::kick; // DragWhat::lane
        juce::String phraseFile; // DragWhat::phrase: one of phraseFiles ()

        friend bool operator== (const DragRequest&, const DragRequest&) = default;
    };

    class Controller
    {
    public:
        virtual ~Controller() = default;

        // Kits and content.
        virtual std::vector<KitCard> kits() const = 0; // in the kit picker's order
        virtual juce::String currentKit() const = 0;
        virtual void selectKit (const juce::String& id) = 0; // queues the load; an unknown id is ignored
        virtual ContentStatus status() const = 0; // the current kit's
        virtual void setContentFolder (const juce::File& folder) = 0; // kept per user, never in project state

        // Playback.
        virtual Snapshot snapshot() = 0;
        virtual std::vector<SectionInfo> sections() const = 0; // the current kit's Song sections
        virtual TransportState transport() const = 0;
        virtual void setTransport (bool playing, double bpm) = 0; // Standalone; bpm 0 follows the kit
        virtual bool preview() const = 0;
        virtual void setPreview (bool on) = 0; // in a host: plays the internal transport while the host is stopped

        // MIDI in and phrase lanes.
        virtual engine::MidiInMode midiInMode() const = 0;
        virtual engine::Lane midiInLane() const = 0;
        virtual void setMidiInMode (engine::MidiInMode mode, engine::Lane lane) = 0;
        virtual engine::PhraseMode phraseMode() const = 0;
        virtual void setPhraseMode (engine::PhraseMode mode) = 0;

        // MIDI drag-out: one file per drag.
        virtual juce::StringArray phraseFiles() const = 0; // the current kit's phrase files, for their chips
        // Whether Loop and Song drags give their drum tracks as one track, Drums (ADR 0025). Kept per user in
        // settings.xml, never in project state: setting it doesn't mark the project changed.
        virtual bool dragDrumsOnOneTrack() const = 0;
        virtual void setDragDrumsOnOneTrack (bool on) = 0;
        // Writes the file to drag into the drag cache folder and returns it; juce::File() on failure.
        virtual juce::File writeDragFile (const DragRequest& request) = 0;
    };
}
