#pragma once

// The Standard MIDI File writer (docs/plans/S1-futuristic-slice.md §2.3): SMF type 1, a conductor
// track (track 0: song name, tempo, time and key signatures, section markers) and one track per lane.
//
// The bytes are a pure function of the spec's content, never of the order it lists events in:
// toFile () checks the spec and sorts every track into one fixed order, and on a valid spec that
// order is total, so shuffling the input lists gives the same file. Within a track, events sort by
//     tick, then class: track name, tempo, time signature, key signature, text events (markers...),
//         note-offs, program changes, controllers, note-ons,
//     then channel, then key, controller number or text.
// So a note that ends where the next one starts is let go before the new one sounds, and a
// controller set on a note's tick applies to that note.
//
// Running status is never used: every channel message carries its status byte. Every track,
// the conductor included, ends on spec.endTick, so a region file ends on its bar line.
//
// What toFile () refuses, with one line saying what and where (each a bug in the caller):
// - values out of range: channel > 15, key or velocity > 127, velocity 0, length < 1, a tick
//   below 0 or above maxTick, a tempo of 0 or above 0xFFFFFF, a numerator of 0 ...
// - an event after endTick, or a note that ends after it
// - two notes on the same track, channel and key that overlap (trimOverlaps () fixes those)
// - two values at one tick for the same thing: two tempos, time or key signatures, two program
//   changes on a channel, two values of one controller on a channel, the same text twice
// - text that isn't valid UTF-8
// The messages count tracks as the file does: track 0 is the conductor, spec.tracks[i] is
// track i + 1; notes are counted from 0 within their TrackSpec.
//
// core/ is integer only: there is nothing here to round.

#include "midi/smf.h"
#include "util/result.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace wubansia::midi
{
    // One lane: one SMF track.
    struct TrackSpec
    {
        std::string name; // FF 03 at tick 0 (UTF-8); empty: no name event
        std::vector<Note> notes;
        std::vector<Controller> controllers;
        std::vector<ProgramChange> programs;
        std::vector<TextEvent> texts; // types 0x01-0x0F except 0x03 (the name has its own field)
    };

    // A whole file.
    struct SongSpec
    {
        std::uint16_t ppq = defaultPpq; // 1-0x7FFF
        std::int64_t endTick = 0; // every track's end-of-track event: the bar line the file ends on
        std::string name; // the conductor track's name (FF 03); empty: no name event
        std::vector<Tempo> tempos; // written as given: no default is added
        std::vector<TimeSignature> timeSignatures;
        std::vector<KeySignature> keySignatures;
        std::vector<Marker> markers; // FF 06 in the conductor track, e.g. one per section start
        std::vector<TrackSpec> tracks;
    };

    // Checks the spec and lays it out as a File: format 1, the conductor track first, then the
    // lanes in the spec's order, each sorted as above.
    [[nodiscard]] util::Result<File> toFile (const SongSpec& spec);

    // The exact bytes of a File: an MThd chunk of length 6, then one MTrk chunk per track, each
    // ending with FF 2F 00 at its endTick. Events go out in the order given, every status byte
    // explicit. Refuses a File that the reader couldn't give back as it is: format other than 0 or 1
    // (or format 0 without exactly one track), a ppq of 0 or above 0x7FFF, more than 65535 tracks,
    // ticks below 0 or going backwards, a delta above maxTick, an endTick before the last event,
    // a data byte above 0x7F, a meta type above 0x7F, an end-of-track event among the events,
    // unused bytes not 0 (data2 of a program change, data1 and data2 of sysex...) or a payload
    // where none belongs.
    [[nodiscard]] util::Result<std::vector<std::uint8_t>> encode (const File& file);

    // encode (toFile (spec)).
    [[nodiscard]] util::Result<std::vector<std::uint8_t>> writeSmf (const SongSpec& spec);

    // Appends value as an SMF variable-length quantity: 7 bits per byte, most significant first,
    // the top bit set on every byte but the last. 0 -> 00, 0x80 -> 81 00, 0x0FFFFFFF -> FF FF FF 7F.
    // value must be at most maxTick (0x0FFFFFFF); larger values are clamped to it.
    void appendVarLen (std::vector<std::uint8_t>& out, std::uint32_t value);

    // Makes notes of the same channel and key stop overlapping, as a DAW's "fix overlapping
    // notes" does, so a feel pass that pulls a note early can't leave two notes on one key:
    // - notes that start on the same tick on the same channel and key merge into one, with the
    //   highest length, velocity and release velocity among them
    // - a note that is still sounding when the next one on its channel and key starts ends there
    // Returns how many notes it shortened or removed. The notes come back sorted by
    // (tick, channel, key). Applies to one track's notes: overlaps across tracks are allowed.
    std::size_t trimOverlaps (std::vector<Note>& notes);
}
