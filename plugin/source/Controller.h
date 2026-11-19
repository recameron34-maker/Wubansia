#pragma once

// The editor's only API (P10), and later the S1f WebView bridge's: what the plugin shows and what a
// person can ask of it beyond the parameters, which the editor binds by parameter ID. ControllerImpl
// (P9) implements it over the processor; plugin/tests/helpers/fake_controller.h fakes it, so the
// editor can be built and tested without an engine.
//
// Every call comes from the message thread and returns at once: a kit load or a content check that
// takes time is queued on the service thread, and status () and snapshot () report on it. Six calls
// touch the disk before they return. writeDragFile () (a MIDI file) and exportSound () (a sample, tens
// of milliseconds for the largest) read and write files, because the OS drag they start must begin
// inside its mouse event; setContentFolder (), setSamplesFolder (), setDragDrumsOnOneTrack () and
// setDragForSynths () write the person's settings.xml at once (setSamplesFolder () also makes the
// folder). That file is read once per process, by the first call that needs it: dragDrumsOnOneTrack ()
// or samplesFolder (), say, unless the service thread's first content check came before. The state-only settings set here (the kit, the
// Standalone tempo, the MIDI-in mode and the phrase mode) mark the host's project as changed; the
// content and samples folders and the drag strip's drum option (drag_drums_one_track) and For my synths
// (drag_for_synths) are the person's, kept in their settings file, and don't. The drag strip's Part is
// the instance's own (ADR 0029): it touches neither the disk nor the project. The looks' pattern () (ADR 0032) parses
// the kit's embedded MIDI, no disk, and their output scope is a lock-free tap on the audio thread's output.

#include "engine.h"

#include <juce_core/juce_core.h>

#include <array>
#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

namespace wubansia
{
    struct PatternView; // services/PatternView.h (ADR 0032)

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
        // Bit per lane with a track in the kit's Loop and Song files, by engine::timelineIndex: Loop vox, Loop no-vox,
        // Song vox, Song no-vox (ADR 0029). Every lane until the catalog knows better.
        std::array<std::uint32_t, 4> fileLanes { 0x3FFu, 0x3FFu, 0x3FFu, 0x3FFu };
        juce::StringArray phraseChords; // the chord loop the kit's phrases are written over (ADR 0031): FUT01's "F#m9", "Bm9"
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
        phrase, // one of the kit's own phrase files
        generatedPhrase // a phrase of the library or More (ADR 0031), by its PhraseInfo id
    };

    struct DragRequest
    {
        DragWhat what = DragWhat::loop;
        engine::Lane lane = engine::Lane::kick; // DragWhat::lane
        juce::String phraseFile; // DragWhat::phrase and generatedPhrase: a PhraseInfo id from phrases () (the kit's own: its file name)

        friend bool operator== (const DragRequest&, const DragRequest&) = default;
    };

    // What the Loop and Song drags bring (ADR 0029): All, the whole kit as before, or one part, its lanes' tracks.
    enum class DragPart : std::uint8_t { all, drums, bass, chords, lead, vox, accents };
    inline constexpr int kDragParts = 7;

    // The drag strip's words, in its order; a part drag's file name carries the part's (never All's).
    inline constexpr std::array<std::string_view, kDragParts> kDragPartNames { "All", "Drums", "Bass", "Chords", "Lead", "Vox", "Accents" };

    // A part's lanes, a bit per engine::Lane: every lane for All; Drums kick, snare, hats and perc; Bass, Chords, Lead and
    // Vox their own lane; Accents the accents and FX lanes. A kit's extras go with their lane (kit.json extras[].lane).
    constexpr std::uint32_t dragPartLanes (DragPart part) noexcept
    {
        using engine::Lane;
        using engine::laneBit;
        switch (part)
        {
            case DragPart::all: return (1u << engine::kLanes) - 1u;
            case DragPart::drums: return laneBit (Lane::kick) | laneBit (Lane::snare) | laneBit (Lane::hats) | laneBit (Lane::perc);
            case DragPart::bass: return laneBit (Lane::bass);
            case DragPart::chords: return laneBit (Lane::chords);
            case DragPart::lead: return laneBit (Lane::lead);
            case DragPart::vox: return laneBit (Lane::vox);
            case DragPart::accents: return laneBit (Lane::accents) | laneBit (Lane::fx);
        }
        return 0u;
    }

    // The Phrases drawer (ADR 0031): where a phrase comes from.
    enum class PhraseOrigin : std::uint8_t { library, more, kit };

    // One note of a phrase's roll: its 16th from bar 1 (0-127), its key and its length in 16ths (at least 1).
    struct PhraseNoteView
    {
        std::uint8_t step = 0, key = 0, len16 = 1;

        friend bool operator== (const PhraseNoteView&, const PhraseNoteView&) = default;
    };

    struct PhraseInfo
    {
        juce::String id; // "hook-5", "arp-1000003"; the kit's own: its file name ("FUT01_Phrase_EPianoStabRhythm_2bars.mid")
        juce::String name; // "Night Bus"; the kit's own: phraseTitle () without the bar count ("E Piano Stab Rhythm", "Lead Chrome Pluck")
        juce::String kind; // a kind id ("hook"... "bounce"); the kit's own: "kit"
        PhraseOrigin origin = PhraseOrigin::kit;
        int bars = 0; // 8; the kit's own 2 or 4
        std::uint64_t seed = 0; // the library's and More's; 0 for the kit's own
        juce::StringArray tags; // "steady", "high", "pedal"...: the library's and More's
        std::vector<PhraseNoteView> roll;
        juce::String sound; // what it plays on: the lane's label for a track of the kit's own lanes ("Pluck"), else the track's name ("Glass Ping")
        std::uint32_t lanes = 0; // bit per engine::Lane it plays on; those lanes of the groove sit out while it sounds
        bool kept = false; // starred: listed under Kept, in every project (settings.xml)
        bool fresh = false; // made by More this session, not yet played or dragged: the tile's New badge

        friend bool operator== (const PhraseInfo&, const PhraseInfo&) = default;
    };

    // Where the Phrases drawer's audition stands (auditionPhrase () gives each request a ticket).
    struct PhraseAuditionState
    {
        std::uint32_t sounding = 0; // the ticket whose phrase plays now; 0 when none
        std::uint32_t latest = 0; // the last ticket auditionPhrase () gave
        juce::String id; // latest's PhraseInfo id
        bool waiting = false; // latest is being made, waits for a block, or waits for its chord line
        int startsAtBar = 0; // 1-based bar of the playing timeline where latest's bar 1 plays; 0 unknown
        bool startedTransport = false; // the audition started Preview (nothing played), and Stop ends both
        bool hostSilent = false; // latest got no block for 300 ms, before or after one took it, and was dropped: the host runs no audio
        juce::String problem; // why latest can't play, for people; empty while it can

        friend bool operator== (const PhraseAuditionState&, const PhraseAuditionState&) = default;
    };

    // The Samples drawer (ADR 0024): every kit's sounds, from its embedded kit.json.
    enum class SoundCategory : std::uint8_t { accents, fx, vox, synths, bass, drums, loops };
    inline constexpr int kSoundCategories = 7;

    // One recorded file of a sound.
    struct SoundFile
    {
        int file = -1; // index into the kit's kit.json files
        juce::String label; // the installed name without folder and ".wav": "Chrome Pluck F#4 v2"
        double seconds = 0.0; // frames / 48000
        int channels = 0; // 1 or 2

        friend bool operator== (const SoundFile& a, const SoundFile& b)
        {
            return a.file == b.file && a.label == b.label && juce::exactlyEqual (a.seconds, b.seconds) && a.channels == b.channels;
        }
    };

    struct SoundInfo
    {
        juce::String id; // "FUT01/rave_stab"; a loop: "FUT01/Loops/Chrome Pluck Phrase.wav"
        juce::String name; // "Rave Stab"; a leading "Accent " dropped: "Dub Chord Cm9"
        SoundCategory category = SoundCategory::accents;
        SoundCategory part = SoundCategory::accents; // what it plays: a loop's lane ("Bass" for a bass line), else its category
        std::vector<SoundFile> files; // zones low to high, then layers soft to hard, then round robins
        int defaultFile = 0; // into files
        bool endAligned = false; // align: end, its last frame is the beat
        int bars = 0; // the default file's whole bars at the kit's tempo (within 1 ms), else 0
        int loopBars = 0; // a loop's cycle (cycle_16ths / 16); 0 for one-shots
        int leadInMs = 0; // the default file's pre-roll, rounded

        friend bool operator== (const SoundInfo&, const SoundInfo&) = default;
    };

    // One file of one kit: what plays, drags and is shown in the Finder.
    struct SoundRef
    {
        juce::String kit; // "FUT01"
        int file = -1; // into that kit's kit.json files

        friend bool operator== (const SoundRef&, const SoundRef&) = default;
    };

    // Where the auditions stand (audition () gives each request a ticket).
    struct AuditionState
    {
        std::uint32_t sounding = 0; // the ticket playing now; 0 when none
        std::uint32_t latest = 0; // the last ticket audition () gave
        bool waiting = false; // latest is being read, or waits for the host's next block
        bool hostSilent = false; // latest got no block for 300 ms, before or after one took it, and was dropped: the host runs no audio
        juce::String problem; // why latest (or a request turned down) can't play, for people; the kit's own file is named in the kit, never by its folder
        juce::String detail; // the problem as the reader gave it, folder and code included; may be empty
        bool sourceProblem = false; // the kit's own file is why (missing, unreadable, not kit.json's)
        bool reinstall = false; // the kit's file isn't the one kit.json lists, and the problem says to reinstall the kit
    };

    struct SoundExport
    {
        juce::File file; // the copy; juce::File () on failure
        juce::String problem; // for people: why there is no copy; the kit's own file is named in the kit, never by its folder
        juce::String detail; // the problem as the disk or the reader gave it, folder and code included; may be empty
        bool sourceProblem = false; // the kit's own file is why (missing, unreadable, not kit.json's), not the samples folder
        bool reinstall = false; // the kit's file isn't the one kit.json lists, and the problem says to reinstall the kit
    };

    // Drum kits (ADR 0030): where the current kit's drum kit stands, as the drum kit thread (DrumKitService) reports it.
    enum class DrumKitStage : std::uint8_t { idle, saving, ready, failed };

    struct DrumKitStatus
    {
        std::uint32_t ticket = 0; // the request it's about; 0 for a refusal or before any
        DrumKitStage stage = DrumKitStage::idle;
        juce::String kit; // "FUT01"
        juce::File folder; // ready: the kit's folder
        juce::File sfz; // ready: the SFZ of the request's kick side
        bool wrote = false; // ready: false when it was there already
        int files = 0; // ready: the plan's files (100 for FUT01)
        juce::String problem, detail; // failed: SoundExport's words
        bool sourceProblem = false, reinstall = false;
    };

    class Controller
    {
    public:
        virtual ~Controller() = default;

        // Kits and content.
        virtual std::vector<KitCard> kits() const = 0; // in the kit picker's order
        virtual juce::String currentKit() const = 0;
        // Queues the load and stops the Samples drawer's audition, even when id is the kit already playing; an
        // unknown id is ignored and stops nothing.
        virtual void selectKit (const juce::String& id) = 0;
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
        // Whether Loop and Song drags give their drum tracks as one track, Drums (ADR 0025). Kept per user in
        // settings.xml, never in project state: setting it doesn't mark the project changed.
        virtual bool dragDrumsOnOneTrack() const = 0;
        virtual void setDragDrumsOnOneTrack (bool on) = 0;
        // Whether every drag gives its pitched tracks made for the person's own synths (ADR 0027): on unless they turn
        // it off. Kept per user in settings.xml like the drum option, never in project state.
        virtual bool dragForSynths() const = 0;
        virtual void setDragForSynths (bool on) = 0;
        // What the Loop and Song drags bring (ADR 0029). The instance's own, All when it starts: never in settings.xml or
        // the project's state, so setting it doesn't mark the project changed.
        virtual DragPart dragPart() const = 0;
        virtual void setDragPart (DragPart part) = 0;
        // Writes the file to drag into the drag cache folder and returns it; juce::File() on failure.
        virtual juce::File writeDragFile (const DragRequest& request) = 0;

        // Samples (ADR 0024). sounds () reads the embedded manifests, so it needs no samples; an unknown kit
        // has none.
        virtual std::vector<SoundInfo> sounds (const juce::String& kit) const = 0;
        // Plays the file dry from its start, on top of whatever plays, replacing the last audition (which fades out
        // over 5 ms). Its ticket, or 0 when it can't (auditionState () says why); a request turned down stops what plays.
        virtual std::uint32_t audition (const SoundRef& sound) = 0;
        virtual void stopAudition() = 0; // every audition so far stops, fading out over 5 ms
        virtual AuditionState auditionState() const = 0;
        // The file's copy in samplesFolder (), made if missing: the installed bytes, checked against kit.json.
        // Synchronous file I/O, like writeDragFile: the OS drag must start inside the mouse event.
        virtual SoundExport exportSound (const SoundRef& sound) = 0;
        virtual juce::File samplesFolder() const = 0; // the chosen folder, else <Music>/Wubansia/Samples
        // Kept per user, never in project state; juce::File () goes back to the default. False, and nothing
        // changes, for a folder inside a content folder or one of Wubansia's own folders (the app data's
        // "Wubansia Audio", which the Uninstall removes, and the caches' around the drag cache), or one that
        // can't be made.
        virtual bool setSamplesFolder (const juce::File& folder) = 0;

        // Drum kits (ADR 0030): the current kit's drums saved as a folder in samplesFolder () on the drum kit thread, at the
        // kick switch's side. Its ticket, 0 when turned down (drumKitStatus () says why). A request equal to the one in
        // hand or the last one saved keeps its ticket; the last saved one stays ready while its folder and SFZ are there.
        // Touches neither the project nor a setting.
        virtual std::uint32_t exportDrumKit() = 0;
        virtual DrumKitStatus drumKitStatus() const = 0;

        // The Phrases drawer (ADR 0031): the current kit's library (its 64 phrases), its kept and More phrases and its own
        // phrase files and lead line. phrases () gives what the phrase thread has made so far for the current kit (the
        // first call starts it, nothing before: an empty list until it is done); phrasesGeneration () moves on whenever
        // phrases () would change (a kit change, a list made, More, Keep, a New badge gone).
        virtual std::vector<PhraseInfo> phrases() const = 0;
        virtual std::uint64_t phrasesGeneration() const = 0;
        // 8 new phrases of kind ("" for one of each kind) after the current kit's list, made on the phrase thread; false
        // while a press is still being made or for an unknown kind.
        virtual bool morePhrases (const juce::String& kind) = 0;
        // Plays the phrase over the groove from the next chord line, on the kit's lead sound, in place of the lanes it
        // plays on; when nothing plays it starts Preview (stopPhraseAudition () ends both). Its ticket, or 0 for an id
        // phrases () doesn't list.
        virtual std::uint32_t auditionPhrase (const juce::String& id) = 0;
        virtual void stopPhraseAudition() = 0;
        virtual PhraseAuditionState phraseAuditionState() const = 0;
        // Stars a phrase (or takes the star off): kept per user in settings.xml, never in project state.
        virtual void setPhraseKept (const juce::String& id, bool kept) = 0;

        // The looks (ADR 0032). The current kit's notes for the display and the step grids: its Loop or Song file for mode
        // (Off: the Loop) and variant, by lane in 16ths, read from the embedded MIDI (no disk). Null for a kit this build
        // doesn't carry. The same object until the kit, mode or variant changes.
        virtual std::shared_ptr<const PatternView> pattern (engine::PlayMode mode, engine::Variant variant) const = 0;
        // The output scope, a lock-free tap after master_gain: on while machine room shows. readScope gives the newest
        // samples (mono, 48 kHz or less), oldest first, and how many.
        virtual void setScopeOn (bool on) = 0;
        virtual int readScope (float* dest, int maxSamples) = 0;
    };
}
