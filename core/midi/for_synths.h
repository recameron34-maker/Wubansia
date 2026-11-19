#pragma once

// MIDI for the person's own synths (ADR 0027): MIDI drag-out's "For my synths" option (plugin/source/dragout/DragFiles.h)
// rewrites a file's pitched tracks so they play well on a synth.
//
// The kits' files are written for Wubansia's samples, which carry their own decay and sit where their zones are rooted:
// the bass an octave below where a synth bass plays (MIDI 26-43), most of its notes shorter than a 16th, its slides
// written as notes that overlap (two voices at once on a polyphonic synth), and soft notes down to velocity 44. On each
// track marked pitched, bass or chords, forSynths () makes every note
// - last a 16th (ppq / 4) at least, or longer as written,
// - but end synthGap () ticks (10 at the kits' 960 PPQ) before the next note on its track starts: no two notes overlap,
//   and a slide is two plain notes. The next note is the next later note-on, so notes that start together (a chord)
//   never shorten each other. No note ends after its track does, and every note lasts a tick at least;
// - play at kSynthVelocityFloor (72) at least: louder notes keep their velocity;
// - and on a bass track, an octave up (MIDI 26-43 becomes 38-55: D1 to G2 in Live's and Logic's names), unless a key
//   would pass 127, when no key moves.
// Everything else on the track stays: its name, controllers, program changes and text events, at their ticks, laid out
// by toFile () (writer.h) in its fixed order. A track holding what that can't carry as it is (pitch bend, key or channel
// pressure, sysex, a meta event other than a text event, a second track name or one after tick 0), or that toFile ()
// refuses, stays as it is. So do the conductor and every track marked asIs, byte for byte, and the header.
//
// A track marked chords (ADR 0027, chords as chords) holds a chord sample's notes: one note per chord, on the key whose
// zone plays it. Each note becomes the chord its sample plays (chordFor () below), every tone at the note's tick and
// with its length and velocity, and the track is then made for synths as a pitched track. The tones start together, so
// they never shorten each other, and each ends where the note alone would have ended.

#include "midi/smf.h"
#include "util/result.h"

#include <cstdint>
#include <span>
#include <vector>

namespace wubansia::midi
{
    // What forSynths () does with a track.
    enum class SynthPart : std::uint8_t {
        asIs, // byte for byte: the conductor, drums, one-key sounds
        pitched, // its notes made for a synth: lengths, no overlaps, velocities
        bass, // pitched, and an octave up
        chords // a chord sample's track: each note its chord, then pitched
    };

    // A chord sample's zones (ADR 0027, chords as chords): a note on a key in [lo, hi] plays the zone's chord, the key
    // plus each interval (semitones over the zone's root, kit.json's zones[].voicing; the first is 0).
    struct ChordZone
    {
        std::uint8_t lo = 0, hi = 127;
        std::vector<std::int8_t> intervals;
    };
    using ChordSample = std::vector<ChordZone>;

    // The lowest key a chord starts on: one above the For my synths bass's top (43 + 12 = 55).
    inline constexpr int kChordFloor = 56;

    // key's chord for a synth: key plus its zone's intervals, lifted whole octaves until the lowest tone is kChordFloor
    // or more; lifted less where the top tone would pass 127; tones past 127 left out when even unlifted they pass it.
    // Ascending. Empty when no zone holds key.
    std::vector<std::uint8_t> chordFor (const ChordSample& sample, std::uint8_t key);

    inline constexpr std::uint8_t kSynthVelocityFloor = 72;

    // How many ticks before the next note on its track a note ends: a 96th of a quarter, at least 1 (10 at 960 PPQ).
    constexpr std::int64_t synthGap (std::uint16_t ppq) noexcept { return ppq >= 192 ? ppq / 96 : 1; }

    // One track's notes as forSynths () plays them (above), the track ending on endTick in a file of ppq ticks a quarter.
    // Sorted by (tick, channel, key); notes on one tick, channel and key become one (trimOverlaps (), writer.h).
    std::vector<Note> synthNotes (std::vector<Note> notes, std::uint16_t ppq, std::int64_t endTick, bool bass);

    // smf with each track i made for synths as parts[i] says; tracks past the end of parts stay as they are. Chunks that
    // aren't tracks, and bytes after the last track, are dropped, as keepTracks () drops them (track_select.h). Fails,
    // saying why, when smf isn't a file readSmf reads, or parts names more tracks than it has. With no chords, a track
    // marked chords is a pitched track.
    [[nodiscard]] util::Result<std::vector<std::uint8_t>> forSynths (std::span<const std::uint8_t> smf, std::span<const SynthPart> parts);

    // forSynths with chord tracks: where parts[i] is chords and chords[i] has zones, each of track i's notes becomes
    // chordFor (chords[i], key), every tone at the note's tick, channel, length and velocity; then the track is made
    // for synths as a pitched track (synthNotes, bass false). A note no zone holds stays one note. A chords track
    // without zones (chords shorter than parts, or empty) is a pitched track, as before.
    [[nodiscard]] util::Result<std::vector<std::uint8_t>> forSynths (std::span<const std::uint8_t> smf,
        std::span<const SynthPart> parts, std::span<const ChordSample> chords);
}
