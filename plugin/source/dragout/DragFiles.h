#pragma once

// MIDI drag-out (P11): the one file a drag chip hands the host, cut from the kit's embedded MIDI
// (docs/midi-map.md, "Files per kit") and written to the drag cache folder.
//
// ControllerImpl::writeDragFile (P9) calls writeDragFile () below with the current kit's info, its
// embedded MIDI files (EmbeddedContent::midiFiles), the play mode, the vox switch, the lane mutes and
// the person's drum option (UserSettings), and returns the file it gives (juce::File () on failure);
// ControllerImpl::phraseFiles () returns phraseFiles (). Message thread only: it reads and writes files.
//
//     const dragout::DragState state { mode, voxSynth ? engine::Variant::vox : engine::Variant::novox, mutedLanes, drumsOnOneTrack, forSynths };
//     const auto file = dragout::writeDragFile (kitInfo, midiFiles, state, request);
//     return file.ok() ? file.value : juce::File(); // file.error says why, for a log
//
// What each DragWhat gives (core/midi/track_select.h cuts the files, byte for byte, and merges drums):
// - loop: the vox switch's Loop file (<KIT>_Loop_Vox_4bars.mid or <KIT>_Loop_NoVox_4bars.mid), less the
//   tracks of muted lanes. With nothing muted it is that file as it is. With drumsOnOneTrack and two or
//   more drum tracks left (Kick, Rumble, Snare, Clap, Snare/Clap, Hats, Perc), those become one track,
//   Drums, where the first was (track_select.h's mergeTracks); every other track stays byte for byte.
// - song: the vox switch's extended mix (<KIT>_ExtendedMix_Vox_224bars.mid or _NoVox_), the same way,
//   drums on one track included.
// - lane: the conductor and that lane's tracks from the current mode's files (Song: the extended
//   mixes; Loop and Off: the Loop files), whatever the mutes. The vox switch picks the file where the
//   lane differs between the two (the accents, the vox); where it doesn't, the vox file gives it, so a
//   name stands for one file; and where the switch's file has none of the lane's tracks (the vox synth
//   with the switch off), the other file does. A lane with tracks in neither can't be dragged.
// - phrase: the named file (one of phraseFiles (); any of the kit's own <KIT>_ files), byte for byte,
//   whatever the mutes.
// Lane and phrase drags never merge drums. Every kept track is kept whole, but drums merged on one
// track, with the conductor (tempo, 4/4, key, and in a Song the section markers and cues), so the
// dragged MIDI is the music Wubansia plays. A track's lane comes from its name
// (engine/kit/channel_map.h's trackLane: channels.py's TRACKS and the kit's extras); a track no lane
// claims stays in Loop and Song drags and is never in a lane drag.
//
// With forSynths (For my synths, ADR 0027), every drag's pitched tracks are made for the person's own
// synths (core/midi/for_synths.h): the bass lane's (Bass and a kit's extras on the bass lane) an octave
// up, and on each of them and every other pitched track (lead, chord voicing, chords, vox, the other
// extras) every note a 16th at least, ending 10 ticks before the next note on its track, at velocity 72
// at least. The conductor, drums, one-key sounds (accents, FX, vox chops), the merged Drums and a track
// no lane claims stay byte for byte; a drag with no pitched track (a drum lane) is the plain one.
//
// Names are "<KIT> <Title> <BPM> <Key> <Camelot> - <what>.mid", for example
//     FUT01 Alloy Signal 134 F#m 11A - Loop.mid
//     FUT01 Alloy Signal 134 F#m 11A - Song NoVox (no Kick, Warp Bass).mid
//     FUT01 Alloy Signal 134 F#m 11A - Song NoVox (1 drum track, no Clap, Warp Bass).mid
//     FUT01 Alloy Signal 134 F#m 11A - Song NoVox (synths, 1 drum track, no Clap, Warp Bass).mid
//     FUT01 Alloy Signal 134 F#m 11A - Warp Bass Song.mid
//     FUT01 Alloy Signal 134 F#m 11A - Warp Bass Song (synths).mid
//     FUT01 Alloy Signal 134 F#m 11A - Phrase Organ Riff 4 bars.mid
// The key is short ("F# minor" is F#m, "Bb major" Bbmaj); a lane is named by the kit's label for it;
// "NoVox" marks the no-vox file. In brackets, "synths" marks a file made for synths, "1 drum track"
// drums merged on one track, and "no ..." lists the muted lanes whose tracks a Loop or Song drag left
// out; with at most one drum track left nothing merges, so the name has no "1 drum track". Only letters, digits, spaces and
// # ( ) , - . _ survive from the kit's strings, so a name is safe on macOS, Windows and Linux. The same
// drag always gives the same name and bytes, and drags that give different files get different names
// (DragOutTests checks it for the four kits; a lane label reading like "Loop" or "Phrase ..." would
// break it).
//
// The cache folder keeps the newest kKeepFiles .mid files: a DAW imports a dropped MIDI file, and
// the newest stay for any drop still in flight.

#include "Controller.h"

#include "util/result.h"

#include <juce_core/juce_core.h>

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace wubansia::dragout
{
    // One embedded MIDI file.
    struct MidiFile
    {
        std::string_view name; // its file name, "FUT01_Loop_Vox_4bars.mid"; folders before a last '/' or '\' are ignored
        std::span<const std::uint8_t> bytes;
    };

    // What the plugin is set to, beside the request.
    struct DragState
    {
        engine::PlayMode mode = engine::PlayMode::loop; // a lane drag takes its tracks from this mode's files
        engine::Variant variant = engine::Variant::vox; // the vox switch
        std::uint32_t mutedLanes = 0; // a bit per engine::Lane (engine::laneBit): left out of Loop and Song
        bool drumsOnOneTrack = false; // Loop and Song: the drum tracks as one track, Drums (ADR 0025)
        bool forSynths = false; // every drag: pitched tracks made for the person's own synths (ADR 0027); off here, so a bare state gives the files as they are
    };

    // A drag's file before it is written.
    struct DragFile
    {
        std::string name; // the file name (UTF-8, but only safe ASCII characters)
        std::vector<std::uint8_t> bytes; // a Standard MIDI File
    };

    // The drag cache folder keeps this many .mid files, the newest.
    inline constexpr int kKeepFiles = 32;

    // text in safe characters: every character but letters, digits, spaces and # ( ) , - . _ made a space,
    // runs of spaces one space, and none at either end, so a file name is safe on macOS, Windows and Linux.
    // The Samples drawer's export names use it too (ADR 0024).
    std::string safeName (std::string_view text);

    // "<note> minor" or "<note> major" (docs/plugin/manifest.md) as F#m or Bbmaj; anything else as it is.
    std::string shortKey (std::string_view key);

    // A kit's Loop and Song files (docs/midi-map.md).
    std::string loopFileName (std::string_view kitId, engine::Variant variant); // "FUT01_Loop_Vox_4bars.mid"
    std::string songFileName (std::string_view kitId, engine::Variant variant); // "FUT01_ExtendedMix_NoVox_224bars.mid"

    // The kit's phrase files among files, "<KIT>_Phrase_*.mid", by file name in byte order.
    juce::StringArray phraseFiles (std::string_view kitId, std::span<const MidiFile> files);

    // The file a request drags, or why there is none. Pure: no file I/O.
    [[nodiscard]] util::Result<DragFile> makeDragFile (const engine::KitInfo& kit, std::span<const MidiFile> files,
        const DragState& state, const DragRequest& request);

    // Where drag files go: <the user's caches>/Wubansia Audio/Wubansia/drag, the caches being
    // ~/Library/Caches on macOS, $XDG_CACHE_HOME or ~/.cache on Linux, and %LOCALAPPDATA% on Windows.
    juce::File cacheFolder();

    // makeDragFile (), written into folder (made if it is missing), replacing a file of the same name;
    // then the folder is trimmed (trimCache) to the newest kKeepFiles. Gives the file, or why it wrote
    // nothing.
    [[nodiscard]] util::Result<juce::File> writeDragFile (const engine::KitInfo& kit, std::span<const MidiFile> files,
        const DragState& state, const DragRequest& request, const juce::File& folder = cacheFolder());

    // Deletes all but the newest `keep` .mid files in folder (by modification time, ties by name),
    // never keepFile, which counts as one of them. Other files are left alone.
    void trimCache (const juce::File& folder, int keep = kKeepFiles, const juce::File& keepFile = {});

    // What an OS drag of file hands to juce::DragAndDropContainer::performExternalDragDropOfFiles: the MIDI chips'
    // and the Samples drawer's (ADR 0024). macOS and Windows take the path as it is (macOS makes the file URL
    // itself, Windows hands paths over). On Linux and the BSDs JUCE writes "file://" and a bare path into the
    // drop's text/uri-list without escaping it, which drop targets cut at '#' (FUT01's F#m) or, in REAPER, at the
    // first space; so there it is the file's own percent-encoded file:// URL, which JUCE hands over as it is.
    juce::String externalDragPath (const juce::File& file);

    // Shows file in the system's file browser: the Samples drawer's tile and Show in Finder, and a MIDI chip's click.
    // On Linux and the BSDs it opens the file's folder with xdg-open, the folder as its one argument and no shell
    // between: File::revealToUser () there runs the path through /bin/sh in double quotes, where $, ` and " in a
    // folder's name act. xdg-open is started by a middle child that exits at once, so it never stays the host's child
    // (a zombie once it ends) and nothing is kept between calls. Elsewhere it is File::revealToUser (). Message thread.
    void revealToUser (const juce::File& file);
}
