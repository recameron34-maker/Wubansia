#pragma once

// A Timeline: one embedded MIDI file (a 4-bar loop or a whole song, vox or no-vox) as the scheduler
// plays it. P7's buildTimeline makes it on the service thread from the SMF and the kit manifest:
// swing snap, pre-roll, the loop wrap, round robins and pans by position, CC ramps, throw latches,
// duck keys, then annotatePhrases (P6). It never changes afterwards; the Kit owns four of them.
//
// Positions are int64 milliticks: 960 PPQ x 1000, so the kits' swing (28.8, 43.2 ticks...) is held
// exactly. A 16th is 240000 milliticks, a 4/4 bar 3840000. Channels are 0-15 (the status byte's low
// nibble), not the 1-based numbers of docs/midi-map.md.

#include <cstdint>
#include <string>
#include <vector>

namespace wubansia::engine
{
    inline constexpr std::int64_t kTicksPerQuarter = 960; // the content's PPQ
    inline constexpr std::int64_t kMilliticksPerTick = 1000;
    inline constexpr std::int64_t kMilliticksPerQuarter = kTicksPerQuarter * kMilliticksPerTick;
    inline constexpr std::int64_t kMilliticksPerSixteenth = kMilliticksPerQuarter / 4;
    inline constexpr std::int64_t kMilliticksPerBar = kMilliticksPerQuarter * 4; // 4/4, the only metre the content has
    inline constexpr std::int64_t kLoopLength = 4 * kMilliticksPerBar; // 15360 ticks
    inline constexpr std::int64_t kSongLength = 224 * kMilliticksPerBar; // 860160 ticks: every extended mix

    // Note::flags.
    namespace NoteFlag
    {
        inline constexpr std::uint8_t endAligned = 1 << 0; // the sample's end lands on the note's end at the current tempo

        // A note a start or seek must chase, listed in Timeline::chase: (i) a note longer than a beat;
        // (ii) a note pre-roll moved earlier, so starting at its onset still plays it instead of dropping
        // it; (iii) a phrase-matched note (slice >= 0), which the phrase voice chases mid-slice.
        inline constexpr std::uint8_t chase = 1 << 1;

        inline constexpr std::uint8_t continues = 1 << 2; // a phrase slice whose next slice follows contiguously

        // A legato note that glides over its instrument's glide.dip_us: a dip (a legato note whose next
        // legato note returns to the pitch before it) and that returning note. The builder sets it on
        // both notes of the pair; live MIDI never does.
        inline constexpr std::uint8_t dip = 1 << 3;
    }

    struct Note
    {
        std::int64_t pos = 0; // milliticks; already moved by swing snap and pre-roll
        std::int64_t len = 0; // milliticks, >= 1 tick
        std::uint8_t channel = 0; // 0-15
        std::uint8_t key = 0;
        std::uint8_t velocity = 0; // 1-127
        std::uint8_t rr = 0; // round robin, counted by position in the instrument's rr block
        std::uint8_t panIndex = 0; // index into the instrument's pan cycle, counted the same way
        std::uint8_t flags = 0; // NoteFlag bits
        std::uint8_t throwSpace = 0; // CC91 latched at the note-on, 0-127 (0: no throw)
        std::uint8_t throwEcho = 0; // CC94, the same
        std::int16_t phrase = -1; // Kit::phrases index when the note can play a recorded slice
        std::int16_t slice = -1; // Kit::slices index (the phrase's own slice), with phrase

        friend bool operator== (const Note&, const Note&) = default;
    };

    // A sequenced controller. A CC whose next value on the same channel and controller comes within a
    // 16th ramps to it linearly: rampTo is that next point's position and nextValue its value.
    // Otherwise rampTo == pos and nextValue == value (a step).
    struct Cc
    {
        std::int64_t pos = 0;
        std::int64_t rampTo = 0;
        std::uint8_t channel = 0;
        std::uint8_t controller = 0;
        std::uint8_t value = 0;
        std::uint8_t nextValue = 0;

        friend bool operator== (const Cc&, const Cc&) = default;
    };

    // An onset the onset ducks key on: a note of an instrument whose duck_key is set, placed the
    // lane's preUs early (mix/mix_config.h, DuckParams).
    struct DuckKey
    {
        std::int64_t pos = 0;
        std::uint8_t keyLane = 0; // a Lane value
        std::uint8_t velocity = 0;

        friend bool operator== (const DuckKey&, const DuckKey&) = default;
    };

    // A section marker (FF 06) or a cue point (FF 07) from the file's conductor track.
    struct Section
    {
        std::int64_t pos = 0;
        std::string name; // UTF-8, as in the file

        friend bool operator== (const Section&, const Section&) = default;
    };

    struct Timeline
    {
        std::int64_t length = 0; // milliticks: the file's end, a bar line (kLoopLength for a loop)
        bool loops = false; // a loop: positions wrap at length, and notes cross the wrap
        std::vector<Note> notes; // by pos, then channel, then key
        std::vector<Cc> ccs; // by pos
        std::vector<DuckKey> keys; // by pos
        std::vector<Section> sections; // FF 06, by pos
        std::vector<Section> cues; // FF 07, by pos
        // Indices into notes of every note with NoteFlag::chase (longer than a beat, moved earlier by
        // pre-roll, or phrase-matched), by pos. The builder makes it after annotatePhrases.
        std::vector<std::uint32_t> chase;

        friend bool operator== (const Timeline&, const Timeline&) = default;
    };
}
