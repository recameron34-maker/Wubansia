#pragma once

// Standard MIDI File data shared by the writer (writer.h) and the reader (reader.h).
//
// A File is the exact content of an SMF: tracks of events at absolute ticks, in file order, with
// every status byte explicit (no running status). The writer's SongSpec (notes with lengths,
// controllers, tempo...) becomes a File in one fixed order, encode () turns a File into bytes, and
// readSmf () gives the File back: readSmf (encode (f)).value == f for every File encode () accepts.

#include <cstdint>
#include <string>
#include <vector>

namespace wubansia::midi
{
    inline constexpr std::uint16_t defaultPpq = 480; // ticks per quarter note: 120 per 16th, 1920 per 4/4 bar

    // The largest number a 4-byte variable-length quantity holds, so the largest delta time. The
    // writer keeps every absolute tick at or below it.
    inline constexpr std::int64_t maxTick = 0x0FFFFFFF;

    namespace status
    {
        inline constexpr std::uint8_t noteOff = 0x80;
        inline constexpr std::uint8_t noteOn = 0x90;
        inline constexpr std::uint8_t polyPressure = 0xA0;
        inline constexpr std::uint8_t controlChange = 0xB0;
        inline constexpr std::uint8_t programChange = 0xC0;
        inline constexpr std::uint8_t channelPressure = 0xD0;
        inline constexpr std::uint8_t pitchBend = 0xE0;
        inline constexpr std::uint8_t sysex = 0xF0;
        inline constexpr std::uint8_t sysexEscape = 0xF7;
        inline constexpr std::uint8_t meta = 0xFF;
    }

    namespace meta
    {
        inline constexpr std::uint8_t text = 0x01;
        inline constexpr std::uint8_t copyright = 0x02;
        inline constexpr std::uint8_t trackName = 0x03;
        inline constexpr std::uint8_t instrumentName = 0x04;
        inline constexpr std::uint8_t lyric = 0x05;
        inline constexpr std::uint8_t marker = 0x06;
        inline constexpr std::uint8_t cuePoint = 0x07;
        inline constexpr std::uint8_t channelPrefix = 0x20;
        inline constexpr std::uint8_t endOfTrack = 0x2F;
        inline constexpr std::uint8_t tempo = 0x51;
        inline constexpr std::uint8_t smpteOffset = 0x54;
        inline constexpr std::uint8_t timeSignature = 0x58;
        inline constexpr std::uint8_t keySignature = 0x59;
        inline constexpr std::uint8_t sequencerSpecific = 0x7F;

        // Types 0x01-0x0F carry text (the SMF spec reserves 0x08-0x0F for more text types).
        constexpr bool isText (std::uint8_t type) noexcept { return type >= 0x01 && type <= 0x0F; }
    }

    // One event at an absolute tick.
    struct Event
    {
        std::int64_t tick = 0; // from the start of the track
        std::uint8_t status = 0; // 0x80-0xEF: channel message (kind | channel); 0xFF: meta; 0xF0, 0xF7: sysex
        std::uint8_t data1 = 0; // channel message: its first data byte; meta: its type; sysex: 0
        std::uint8_t data2 = 0; // channel message: its second data byte (0 for program change and channel pressure); else 0
        std::vector<std::uint8_t> payload; // meta and sysex: the bytes after the length; channel messages: empty

        bool isChannel() const noexcept { return status >= 0x80 && status < 0xF0; }
        bool isMeta() const noexcept { return status == status::meta; }
        bool isMeta (std::uint8_t type) const noexcept { return isMeta() && data1 == type; }
        bool isSysex() const noexcept { return status == status::sysex || status == status::sysexEscape; }

        // Channel messages only.
        std::uint8_t kind() const noexcept { return static_cast<std::uint8_t> (status & 0xF0); }
        std::uint8_t channel() const noexcept { return static_cast<std::uint8_t> (status & 0x0F); }

        // A note-on with velocity 0 counts as a note-off, as the MIDI spec says.
        bool isNoteOn() const noexcept { return isChannel() && kind() == status::noteOn && data2 > 0; }
        bool isNoteOff() const noexcept { return isChannel() && (kind() == status::noteOff || (kind() == status::noteOn && data2 == 0)); }
        bool isController() const noexcept { return isChannel() && kind() == status::controlChange; }

        friend bool operator== (const Event&, const Event&) = default;
    };

    struct Track
    {
        std::vector<Event> events; // in file order, ticks never decreasing; the end-of-track event is not in here
        std::int64_t endTick = 0; // the end-of-track event's tick, at or after the last event

        friend bool operator== (const Track&, const Track&) = default;
    };

    struct File
    {
        std::uint16_t format = 1; // 0 (one track) or 1 (tracks played together)
        std::uint16_t ppq = defaultPpq; // ticks per quarter note, 1-0x7FFF
        std::vector<Track> tracks;

        friend bool operator== (const File&, const File&) = default;
    };

    // The typed events the writer takes and the reader's helpers return.

    struct Note
    {
        std::int64_t tick = 0; // note-on
        std::int64_t length = 0; // ticks to the note-off; the writer needs at least 1
        std::uint8_t channel = 0;
        std::uint8_t key = 0;
        std::uint8_t velocity = 0; // 1-127
        std::uint8_t releaseVelocity = 64; // the note-off's velocity; 64 is the MIDI spec's value for "not sensed"

        friend bool operator== (const Note&, const Note&) = default;
    };

    struct Controller
    {
        std::int64_t tick = 0;
        std::uint8_t channel = 0;
        std::uint8_t number = 0; // CC number, e.g. 74
        std::uint8_t value = 0;

        friend bool operator== (const Controller&, const Controller&) = default;
    };

    struct ProgramChange
    {
        std::int64_t tick = 0;
        std::uint8_t channel = 0;
        std::uint8_t program = 0; // 0-127

        friend bool operator== (const ProgramChange&, const ProgramChange&) = default;
    };

    struct Tempo
    {
        std::int64_t tick = 0;
        std::uint32_t microsPerQuarter = 500000; // 1 to 0xFFFFFF; 500000 is 120 BPM, the SMF default

        friend bool operator== (const Tempo&, const Tempo&) = default;
    };

    struct TimeSignature
    {
        std::int64_t tick = 0;
        std::uint8_t numerator = 4;
        std::uint8_t denominatorPow2 = 2; // the denominator as a power of 2: 2 is a quarter note, so 4/4
        std::uint8_t clocksPerClick = 24; // MIDI clocks per metronome click
        std::uint8_t thirtySecondsPerQuarter = 8;

        friend bool operator== (const TimeSignature&, const TimeSignature&) = default;
    };

    struct KeySignature
    {
        std::int64_t tick = 0;
        std::int8_t sharps = 0; // -7 to 7; below 0 counts flats. F# minor: 3, minor
        bool minor = false;

        friend bool operator== (const KeySignature&, const KeySignature&) = default;
    };

    // A marker (FF 06): in the writer's conductor track, one per section start.
    struct Marker
    {
        std::int64_t tick = 0;
        std::string text; // UTF-8

        friend bool operator== (const Marker&, const Marker&) = default;
    };

    // Any text meta event (types 0x01-0x0F): text, lyric, marker, cue point...
    struct TextEvent
    {
        std::int64_t tick = 0;
        std::uint8_t type = meta::text;
        std::string text; // UTF-8

        friend bool operator== (const TextEvent&, const TextEvent&) = default;
    };

    // Microseconds per quarter note for a whole-number tempo, rounded half up like the legacy
    // writer's Math.round (60000000 / bpm): 134 BPM gives 447761, 130 gives 461538. bpm must be 4
    // or more, so the result fits the tempo event's 3 bytes; anything lower gives 0, which the
    // writer rejects.
    constexpr std::uint32_t microsPerQuarter (std::int32_t bpm) noexcept
    {
        if (bpm < 4)
            return 0;
        const auto b = static_cast<std::uint32_t> (bpm);
        return (2u * 60000000u + b) / (2u * b);
    }

    // The key signature's sharps (below 0: flats) for a key, spelled as the legacy pack spells it
    // (legacy/web-pack/core.js:18-29): minor keys C# minor (4 sharps) and Eb minor (6 flats), major
    // keys Db (5 flats) and F# (6 sharps). pitchClass is taken modulo 12 (C = 0).
    // FUT01 F# minor: 3; FUT02 D minor: -1; FUT03 G minor: -2; FUT04 E minor: 1.
    constexpr std::int8_t sharpsFor (int pitchClass, bool minor) noexcept
    {
        constexpr std::int8_t minorSharps[12] = { -3, 4, -1, -6, 1, -4, 3, -2, 5, 0, -5, 2 };
        constexpr std::int8_t majorSharps[12] = { 0, -5, 2, -3, 4, -1, 6, 1, -4, 3, -2, 5 };
        const int pc = ((pitchClass % 12) + 12) % 12;
        return minor ? minorSharps[pc] : majorSharps[pc];
    }
}
