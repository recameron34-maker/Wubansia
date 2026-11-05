#pragma once

// Cutting a Standard MIDI File down to some of its tracks, for MIDI drag-out
// (plugin/source/dragout/DragFiles.h): one file per drag, holding the conductor and the tracks of the
// lanes asked for.
//
// The cut is byte for byte. The header chunk is copied with only its track count rewritten, and each
// kept MTrk chunk is copied whole, so every track reads exactly as it did (readSmf gives the same
// Track) and a cut is as deterministic as its source. Track 0, the conductor (in a format 1 file the
// name, tempo, time and key signatures, markers and cues), is always kept. Chunks that aren't tracks,
// MTrk chunks past the header's track count and bytes after the last track are dropped: readSmf
// ignores them too.
//
// Which tracks to keep is the caller's choice: trackChunks () names them (the drag-out selects by lane
// through the names, engine/kit/channel_map.h's trackLane).

#include "util/result.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
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
}
