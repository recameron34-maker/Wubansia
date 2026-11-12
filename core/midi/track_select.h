#pragma once

// Cutting a Standard MIDI File down to some of its tracks, and merging some of them into one track,
// for MIDI drag-out (plugin/source/dragout/DragFiles.h): one file per drag, holding the conductor and
// the tracks of the lanes asked for.
//
// The cut is byte for byte. The header chunk is copied with only its track count rewritten, and each
// kept MTrk chunk is copied whole, so every track reads exactly as it did (readSmf gives the same
// Track) and a cut is as deterministic as its source. Track 0, the conductor (in a format 1 file the
// name, tempo, time and key signatures, markers and cues), is always kept. Chunks that aren't tracks,
// MTrk chunks past the header's track count and bytes after the last track are dropped: readSmf
// ignores them too.
//
// A merge (mergeTracks) is the one exception: the tracks it merges become one new track, written by
// the SMF writer (writer.h) in its fixed order, so the merged bytes depend only on the merged events.
// Every other chunk is still copied byte for byte.
//
// Which tracks to keep is the caller's choice: trackChunks () names them (the drag-out selects by lane
// through the names, engine/kit/channel_map.h's trackLane).

#include "util/result.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace wubansia::midi
{
    // One track's MTrk chunk in a file.
    struct TrackChunk
    {
        std::size_t offset = 0; // where the chunk's "MTrk" id starts in the file
        std::size_t size = 0; // the whole chunk: its 8-byte id and length, then its data
        std::string name; // the track's first track-name event's text (reader.h's trackName), or ""

        friend bool operator== (const TrackChunk&, const TrackChunk&) = default;
    };

    // The file's tracks in file order (index i is readSmf's tracks[i]), or readSmf's error.
    [[nodiscard]] util::Result<std::vector<TrackChunk>> trackChunks (std::span<const std::uint8_t> smf);

    // A new SMF: smf's header chunk with the track count rewritten, then track 0 and the tracks at
    // trackIndices, each chunk byte for byte and in file order, whatever order the indices come in.
    // Index 0 and repeated indices change nothing. Fails, saying why, when smf isn't a file readSmf
    // reads, has no tracks, or an index isn't one of its tracks.
    [[nodiscard]] util::Result<std::vector<std::uint8_t>> keepTracks (std::span<const std::uint8_t> smf, std::span<const int> trackIndices);

    // keepTracks (smf, trackIndices), but with the tracks at mergeIndices (kept too) written as one track named name, in
    // the place of the first of them in file order: their notes, controllers, program changes and text events, each
    // track's own name left out, laid out by toFile () (writer.h): on a tick the name, text events, note-offs, program
    // changes, controllers, note-ons, then channel and key; every status byte explicit; ending on the latest end tick
    // among them. The header (track count rewritten), the conductor and every other kept track are copied byte for byte.
    // Fails, saying why, where keepTracks fails; when mergeIndices names fewer than two distinct tracks, or track 0; when
    // a track to merge holds what can't be carried as it is (pitch bend, key or channel pressure, sysex, a meta event
    // other than a text event, a note-on with no note-off before its track ends, a note-off with nothing to end); and
    // where toFile () refuses the merged track (overlapping notes on one channel and key, two values of one controller or
    // two program changes on one channel and tick, the same text twice on a tick, a note-off on its note-on's tick).
    // Every message starts "can't merge tracks 1, 2, 3, 4: " (the merge indices, ascending); the writer's own text
    // numbers channels from 0.
    [[nodiscard]] util::Result<std::vector<std::uint8_t>> mergeTracks (std::span<const std::uint8_t> smf,
        std::span<const int> trackIndices, std::span<const int> mergeIndices, std::string_view name);
}
