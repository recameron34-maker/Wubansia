#pragma once

// The Standard MIDI File reader, for the metrics (docs/plans/S1-futuristic-slice.md §5.1): the
// metrics read SMF bytes, so the same code scores the legacy baseline and the new generator.
//
// readSmf () takes formats 0 and 1 with a ticks-per-quarter-note division, running status, meta
// and sysex events, and chunks it doesn't know (skipped, as the SMF spec asks). It never reads past
// the input: a truncated or malformed file gives an error saying what is wrong and at which byte.
// Running status is honoured after meta and sysex events too, as many readers do (the spec says
// those cancel it, but a data byte there has no other reading). Bytes after the last track chunk,
// and after a track's end-of-track event, are ignored.
//
// The helpers below turn a File's events into the typed lists the metrics use.

#include "midi/smf.h"
#include "util/result.h"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace wubansia::midi
{
    [[nodiscard]] util::Result<File> readSmf (std::span<const std::uint8_t> bytes);

    // The text of the track's first track-name event (FF 03), or "".
    std::string trackName (const Track& track);

    // The track's notes, each note-on paired with the first note-off (or note-on with velocity 0)
    // on its channel and key that comes after it: the oldest sounding note ends first. A note still
    // sounding at the end of the track ends on its endTick. A note-off with nothing to end is
    // ignored. Lengths can be 0 (a note-off on the note-on's tick). releaseVelocity is the
    // note-off's velocity, 0 for a note-on with velocity 0 and for notes the end of track ends.
    // Sorted by (tick, channel, key); notes that tie keep the order of their note-ons.
    std::vector<Note> notesOf (const Track& track);

    std::vector<Controller> controllersOf (const Track& track); // in file order
    std::vector<ProgramChange> programsOf (const Track& track); // in file order
    std::vector<TextEvent> textsOf (const Track& track); // meta types 0x01-0x0F, in file order

    // Across all tracks, by tick; events on the same tick keep track order, then file order.
    std::vector<Marker> markersOf (const File& file); // FF 06
    std::vector<Tempo> tempoMap (const File& file); // FF 51 with 3 bytes, not 0
    std::vector<TimeSignature> timeSignaturesOf (const File& file); // FF 58 with 4 bytes
    std::vector<KeySignature> keySignaturesOf (const File& file); // FF 59 with 2 bytes

    // The time of a tick in microseconds, rounded down, under a tempo map sorted by tick (as
    // tempoMap () gives it). Before the first tempo event the SMF default, 120 BPM (500000), holds;
    // of several tempos on one tick the last counts. tick must be 0 to 2^28 (maxTick) and ppq
    // 1 to 0x7FFF; anything else gives 0. Exact: integer arithmetic only.
    std::int64_t ticksToMicros (std::span<const Tempo> tempos, std::uint16_t ppq, std::int64_t tick);
}
