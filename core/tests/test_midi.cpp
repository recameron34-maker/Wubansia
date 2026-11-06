// Tests for core/midi: the SMF writer (writer.h) and reader (reader.h).
//
// The two hand-made files below were written out from the SMF 1.0 spec and then decoded with an
// independent parser (Python mido 1.3.3) before being pasted here, to check they say what the
// comments say.

#include "midi/reader.h"
#include "midi/writer.h"
#include "rng/rng.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <vector>

using namespace wubansia;
using Catch::Matchers::ContainsSubstring;

namespace
{
    using Bytes = std::vector<std::uint8_t>;

    // A 2-bar song at 480 PPQ: a conductor track and two lanes. At tick 480 the bass lets go of
    // F#2, moves CC74 and plays F#2 again: note-off, controller, note-on, in that order.
    // clang-format off
    const Bytes expectedSmall = {
        // MThd: length 6, format 1, 3 tracks, 480 ticks per quarter note
        0x4D, 0x54, 0x68, 0x64, 0x00, 0x00, 0x00, 0x06, 0x00, 0x01, 0x00, 0x03, 0x01, 0xE0,
        // MTrk 0, 57 bytes: the conductor
        0x4D, 0x54, 0x72, 0x6B, 0x00, 0x00, 0x00, 0x39,
        0x00, 0xFF, 0x03, 0x09, 0x54, 0x65, 0x73, 0x74, 0x20, 0x73, 0x6F, 0x6E, 0x67, // name "Test song"
        0x00, 0xFF, 0x51, 0x03, 0x06, 0xD5, 0x11,                                     // tempo 447761 us: 134 BPM
        0x00, 0xFF, 0x58, 0x04, 0x04, 0x02, 0x18, 0x08,                               // 4/4, 24 clocks, 8 32nds
        0x00, 0xFF, 0x59, 0x02, 0x03, 0x01,                                           // 3 sharps, minor: F# minor
        0x00, 0xFF, 0x06, 0x05, 0x69, 0x6E, 0x74, 0x72, 0x6F,                         // marker "intro"
        0x8F, 0x00, 0xFF, 0x06, 0x04, 0x64, 0x72, 0x6F, 0x70,                         // +1920: marker "drop"
        0x8F, 0x00, 0xFF, 0x2F, 0x00,                                                 // +1920: end of track at 3840
        // MTrk 1, 30 bytes: "Kick", channel 10 (9 from 0)
        0x4D, 0x54, 0x72, 0x6B, 0x00, 0x00, 0x00, 0x1E,
        0x00, 0xFF, 0x03, 0x04, 0x4B, 0x69, 0x63, 0x6B,
        0x00, 0x99, 0x24, 0x6E, // C1 velocity 110
        0x78, 0x89, 0x24, 0x40, // +120: note-off, release velocity 64
        0x8E, 0x08, 0x99, 0x24, 0x64, // +1800: tick 1920, velocity 100
        0x78, 0x89, 0x24, 0x40,
        0x8E, 0x08, 0xFF, 0x2F, 0x00, // +1800: end of track at 3840
        // MTrk 2, 39 bytes: "Bass", channel 2 (1 from 0)
        0x4D, 0x54, 0x72, 0x6B, 0x00, 0x00, 0x00, 0x27,
        0x00, 0xFF, 0x03, 0x04, 0x42, 0x61, 0x73, 0x73,
        0x00, 0xB1, 0x4A, 0x40, // CC74 = 64 before the note it shapes
        0x00, 0x91, 0x2A, 0x60, // F#2 velocity 96
        0x83, 0x60, 0x81, 0x2A, 0x40, // +480: note-off first...
        0x00, 0xB1, 0x4A, 0x50, // ...then CC74 = 80...
        0x00, 0x91, 0x2A, 0x5A, // ...then the new note, velocity 90
        0x81, 0x70, 0x81, 0x2A, 0x40, // +240: note-off at 720
        0x98, 0x30, 0xFF, 0x2F, 0x00, // +3120: end of track at 3840
    };
    // clang-format on

    midi::SongSpec smallSpec()
    {
        midi::SongSpec s;
        s.ppq = 480;
        s.endTick = 3840;
        s.name = "Test song";
        s.tempos = { { 0, midi::microsPerQuarter (134) } };
        s.timeSignatures = { { 0, 4, 2, 24, 8 } };
        s.keySignatures = { { 0, midi::sharpsFor (6, true), true } };
        s.markers = { { 0, "intro" }, { 1920, "drop" } };

        midi::TrackSpec kick;
        kick.name = "Kick";
        kick.notes = { { 0, 120, 9, 36, 110 }, { 1920, 120, 9, 36, 100 } };

        midi::TrackSpec bass;
        bass.name = "Bass";
        bass.notes = { { 0, 480, 1, 42, 96 }, { 480, 240, 1, 42, 90 } };
        bass.controllers = { { 0, 1, 74, 64 }, { 480, 1, 74, 80 } };

        s.tracks = { kick, bass };
        return s;
    }

    // A type-0 file at 96 PPQ that uses running status for notes and controllers, ends notes with
    // note-ons of velocity 0, and has a program change, a marker, a sysex event and a pitch bend.
    // clang-format off
    const Bytes runningStatusType0 = {
        0x4D, 0x54, 0x68, 0x64, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x01, 0x00, 0x60,
        0x4D, 0x54, 0x72, 0x6B, 0x00, 0x00, 0x00, 0x34,
        0x00, 0xFF, 0x51, 0x03, 0x07, 0xA1, 0x20, // tempo 500000
        0x00, 0xC0, 0x05,                         // program 5: one data byte
        0x00, 0x90, 0x3C, 0x64,                   // note-on C4 100
        0x00, 0x40, 0x50,                         // running status: note-on E4 80
        0x60, 0x3C, 0x00,                         // +96: note-on C4 velocity 0, a note-off
        0x00, 0x40, 0x00,                         // note-on E4 velocity 0
        0x00, 0xB0, 0x07, 0x64,                   // CC7 = 100
        0x10, 0x0A, 0x40,                         // +16: running status: CC10 = 64
        0x00, 0xFF, 0x06, 0x03, 0x65, 0x6E, 0x64, // marker "end"
        0x00, 0xF0, 0x03, 0x7E, 0x7F, 0xF7,       // sysex, 3 bytes
        0x81, 0x00, 0xE0, 0x00, 0x40,             // +128: pitch bend, centre
        0x00, 0xFF, 0x2F, 0x00,                   // end of track at 240
    };
    // clang-format on

    // An SMF from an MThd body and track bodies, with the chunk lengths filled in.
    Bytes smf (std::uint16_t format, std::uint16_t ppq, std::initializer_list<Bytes> tracks)
    {
        Bytes out = { 0x4D, 0x54, 0x68, 0x64, 0x00, 0x00, 0x00, 0x06 };
        const auto u16 = [&out] (std::size_t v) {
            out.push_back (static_cast<std::uint8_t> ((v >> 8) & 0xFF));
            out.push_back (static_cast<std::uint8_t> (v & 0xFF));
        };
        u16 (format);
        u16 (tracks.size());
        u16 (ppq);
        for (const auto& t : tracks)
        {
            out.insert (out.end(), { 0x4D, 0x54, 0x72, 0x6B });
            for (int shift = 24; shift >= 0; shift -= 8)
                out.push_back (static_cast<std::uint8_t> ((t.size() >> shift) & 0xFF));
            out.insert (out.end(), t.begin(), t.end());
        }
        return out;
    }

    Bytes write (const midi::SongSpec& spec)
    {
        auto r = midi::writeSmf (spec);
        INFO (r.error);
        REQUIRE (r.ok());
        return r.value;
    }

    midi::File read (const Bytes& bytes)
    {
        auto r = midi::readSmf (bytes);
        INFO (r.error);
        REQUIRE (r.ok());
        return r.value;
    }

    std::string writeError (const midi::SongSpec& spec)
    {
        const auto r = midi::writeSmf (spec);
        CHECK_FALSE (r.ok());
        CHECK (r.value.empty());
        return r.error;
    }

    std::string readError (const Bytes& bytes)
    {
        const auto r = midi::readSmf (bytes);
        CHECK_FALSE (r.ok());
        return r.error;
    }

    midi::Event channelEvent (std::int64_t tick, std::uint8_t status, std::uint8_t d1, std::uint8_t d2 = 0)
    {
        return { tick, status, d1, d2, {} };
    }

    midi::Event metaEvent (std::int64_t tick, std::uint8_t type, Bytes payload)
    {
        return { tick, midi::status::meta, type, 0, std::move (payload) };
    }
}

TEST_CASE ("appendVarLen writes the SMF spec's examples", "[midi]")
{
    struct Case
    {
        std::uint32_t value;
        Bytes bytes;
    };
    // clang-format off
    const std::vector<Case> spec = {
        { 0x00000000, { 0x00 } }, { 0x00000040, { 0x40 } }, { 0x0000007F, { 0x7F } },
        { 0x00000080, { 0x81, 0x00 } }, { 0x00002000, { 0xC0, 0x00 } }, { 0x00003FFF, { 0xFF, 0x7F } },
        { 0x00004000, { 0x81, 0x80, 0x00 } }, { 0x00100000, { 0xC0, 0x80, 0x00 } }, { 0x001FFFFF, { 0xFF, 0xFF, 0x7F } },
        { 0x00200000, { 0x81, 0x80, 0x80, 0x00 } }, { 0x08000000, { 0xC0, 0x80, 0x80, 0x00 } }, { 0x0FFFFFFF, { 0xFF, 0xFF, 0xFF, 0x7F } },
    };
    // clang-format on
    for (const auto& c : spec)
    {
        Bytes out = { 0xAA }; // appends
        midi::appendVarLen (out, c.value);
        Bytes expected = { 0xAA };
        expected.insert (expected.end(), c.bytes.begin(), c.bytes.end());
        INFO (c.value);
        CHECK (out == expected);
    }

    Bytes clamped;
    midi::appendVarLen (clamped, 0xFFFFFFFFu);
    CHECK (clamped == Bytes { 0xFF, 0xFF, 0xFF, 0x7F });
}

TEST_CASE ("microsPerQuarter rounds like the legacy writer's Math.round", "[midi]")
{
    CHECK (midi::microsPerQuarter (134) == 447761u); // 447761.19
    CHECK (midi::microsPerQuarter (130) == 461538u); // 461538.46
    CHECK (midi::microsPerQuarter (128) == 468750u);
    CHECK (midi::microsPerQuarter (136) == 441176u); // 441176.47
    CHECK (midi::microsPerQuarter (120) == 500000u);
    CHECK (midi::microsPerQuarter (512) == 117188u); // exactly 117187.5: the half goes up
    CHECK (midi::microsPerQuarter (7) == 8571429u);
    CHECK (midi::microsPerQuarter (4) == 15000000u); // still fits 3 bytes
    CHECK (midi::microsPerQuarter (3) == 0u);
    CHECK (midi::microsPerQuarter (0) == 0u);
    CHECK (midi::microsPerQuarter (-134) == 0u);
}

TEST_CASE ("sharpsFor spells keys as the legacy pack does", "[midi]")
{
    CHECK (midi::sharpsFor (6, true) == 3); // FUT01 F# minor, 11A
    CHECK (midi::sharpsFor (2, true) == -1); // FUT02 D minor, 7A
    CHECK (midi::sharpsFor (7, true) == -2); // FUT03 G minor, 6A
    CHECK (midi::sharpsFor (4, true) == 1); // FUT04 E minor, 9A
    CHECK (midi::sharpsFor (9, true) == 0); // A minor
    CHECK (midi::sharpsFor (1, true) == 4); // C# minor
    CHECK (midi::sharpsFor (3, true) == -6); // Eb minor
    CHECK (midi::sharpsFor (0, false) == 0); // C major
    CHECK (midi::sharpsFor (6, false) == 6); // F# major
    CHECK (midi::sharpsFor (1, false) == -5); // Db major
    CHECK (midi::sharpsFor (-6, true) == 3); // taken modulo 12
    CHECK (midi::sharpsFor (18, true) == 3);
}

TEST_CASE ("the writer's bytes for a small song match the SMF spec byte for byte", "[midi]")
{
    CHECK (write (smallSpec()) == expectedSmall);

    SECTION ("writing twice gives the same bytes")
    {
        CHECK (write (smallSpec()) == write (smallSpec()));
    }
}

TEST_CASE ("the bytes don't depend on the order the spec lists events in", "[midi]")
{
    // A busier spec: chords, controllers on two channels, texts and programs.
    midi::SongSpec base = smallSpec();
    midi::TrackSpec chords;
    chords.name = "Chords";
    for (std::int64_t bar = 0; bar < 2; ++bar)
        for (const int key : { 54, 57, 61, 64 })
            chords.notes.push_back ({ bar * 1920, 960, 2, static_cast<std::uint8_t> (key), static_cast<std::uint8_t> (70 + key % 7) });
    chords.controllers = { { 0, 2, 74, 30 }, { 0, 2, 1, 10 }, { 960, 2, 74, 50 }, { 0, 3, 74, 99 } };
    chords.programs = { { 0, 2, 4 }, { 0, 3, 81 } };
    chords.texts = { { 0, midi::meta::lyric, "la" }, { 0, midi::meta::cuePoint, "chop" }, { 960, midi::meta::lyric, "la" } };
    base.tracks.push_back (chords);
    base.markers.push_back ({ 1920, "build" }); // two markers on one tick: ordered by their text
    const Bytes reference = write (base);

    for (std::uint32_t attempt = 0; attempt < 24; ++attempt)
    {
        midi::SongSpec shuffled = base;
        const rng::Key k = rng::seedKey (attempt).with (rng::tag ("shuffle-spec"));
        rng::shuffle (k.with (0), shuffled.markers);
        for (std::size_t t = 0; t < shuffled.tracks.size(); ++t)
        {
            auto& track = shuffled.tracks[t];
            rng::shuffle (k.with (1).with (t), track.notes);
            rng::shuffle (k.with (2).with (t), track.controllers);
            rng::shuffle (k.with (3).with (t), track.programs);
            rng::shuffle (k.with (4).with (t), track.texts);
        }
        CHECK (write (shuffled) == reference);
    }
}

TEST_CASE ("events on one tick go out in the fixed order", "[midi]")
{
    midi::SongSpec s;
    s.endTick = 1920;
    midi::TrackSpec t;
    t.name = "Lane";
    // All on tick 480 except the notes ending there.
    t.notes = { { 480, 10, 1, 60, 100 }, { 0, 480, 1, 64, 90 }, { 480, 10, 0, 62, 80 }, { 0, 480, 0, 67, 70 }, { 480, 10, 0, 50, 75 } };
    t.controllers = { { 480, 1, 7, 100 }, { 480, 0, 74, 20 }, { 480, 0, 1, 5 } };
    t.programs = { { 480, 1, 33 } };
    t.texts = { { 480, midi::meta::marker, "b" }, { 480, midi::meta::lyric, "z" }, { 480, midi::meta::marker, "a" } };
    s.tracks = { t };

    const auto file = toFile (s);
    REQUIRE (file.ok());
    const auto& events = file.value.tracks.at (1).events;

    std::vector<std::string> order;
    for (const auto& e : events)
    {
        if (e.tick != 480)
            continue;
        std::string what;
        if (e.isMeta())
            what = "text" + std::to_string (e.data1) + ":" + std::string (e.payload.begin(), e.payload.end());
        else if (e.isNoteOff())
            what = "off" + std::to_string (e.channel()) + ":" + std::to_string (e.data1);
        else if (e.kind() == midi::status::programChange)
            what = "prog" + std::to_string (e.channel());
        else if (e.isController())
            what = "cc" + std::to_string (e.channel()) + ":" + std::to_string (e.data1);
        else if (e.isNoteOn())
            what = "on" + std::to_string (e.channel()) + ":" + std::to_string (e.data1);
        order.push_back (what);
    }
    CHECK (order == std::vector<std::string> {
               "text5:z", "text6:a", "text6:b", // texts by type, then text
               "off0:67",
               "off1:64", // note-offs by channel, then key
               "prog1",
               "cc0:1",
               "cc0:74",
               "cc1:7", // controllers by channel, then number
               "on0:50",
               "on0:62",
               "on1:60", // note-ons by channel, then key
           });
}

TEST_CASE ("every track ends on endTick", "[midi]")
{
    auto s = smallSpec();
    s.endTick = 7680; // 4 bars, past the last event
    const auto file = read (write (s));
    REQUIRE (file.tracks.size() == 3);
    for (const auto& t : file.tracks)
        CHECK (t.endTick == 7680);

    SECTION ("a note may end exactly on endTick")
    {
        auto tight = smallSpec();
        tight.tracks[0].notes.push_back ({ 3720, 120, 9, 36, 90 });
        for (const auto& t : read (write (tight)).tracks)
            CHECK (t.endTick == 3840);
    }

    SECTION ("an empty song is a conductor track that ends on endTick")
    {
        midi::SongSpec empty;
        empty.endTick = 1920;
        const auto f = read (write (empty));
        REQUIRE (f.tracks.size() == 1);
        CHECK (f.tracks[0].events.empty());
        CHECK (f.tracks[0].endTick == 1920);
        CHECK (f.format == 1);
    }
}

TEST_CASE ("toFile refuses specs with mistakes and says where", "[midi]")
{
    SECTION ("note values out of range")
    {
        auto s = smallSpec();
        s.tracks[1].notes[1].velocity = 0;
        CHECK_THAT (writeError (s), ContainsSubstring ("velocity") && ContainsSubstring ("Bass"));

        s = smallSpec();
        s.tracks[1].notes[1].velocity = 128;
        CHECK_THAT (writeError (s), ContainsSubstring ("velocity"));

        s = smallSpec();
        s.tracks[0].notes[0].key = 128;
        CHECK_THAT (writeError (s), ContainsSubstring ("key") && ContainsSubstring ("Kick"));

        s = smallSpec();
        s.tracks[0].notes[0].channel = 16;
        CHECK_THAT (writeError (s), ContainsSubstring ("channel"));

        s = smallSpec();
        s.tracks[0].notes[0].length = 0;
        CHECK_THAT (writeError (s), ContainsSubstring ("length"));

        s = smallSpec();
        s.tracks[0].notes[0].releaseVelocity = 200;
        CHECK_THAT (writeError (s), ContainsSubstring ("release velocity"));

        s = smallSpec();
        s.tracks[0].notes[0].tick = -1;
        CHECK_THAT (writeError (s), ContainsSubstring ("tick"));
    }

    SECTION ("anything after endTick")
    {
        auto s = smallSpec();
        s.tracks[0].notes.push_back ({ 3780, 120, 9, 36, 90 }); // ends at 3900
        CHECK_THAT (writeError (s), ContainsSubstring ("3900") && ContainsSubstring ("3840"));

        s = smallSpec();
        s.markers.push_back ({ 3841, "late" });
        CHECK_THAT (writeError (s), ContainsSubstring ("marker"));

        s = smallSpec();
        s.endTick = -1;
        CHECK_THAT (writeError (s), ContainsSubstring ("end tick"));

        s = smallSpec();
        s.endTick = midi::maxTick + 1;
        CHECK_THAT (writeError (s), ContainsSubstring ("end tick"));
    }

    SECTION ("overlapping notes on one channel and key")
    {
        auto s = smallSpec();
        s.tracks[1].notes.push_back ({ 400, 120, 1, 42, 80 }); // inside the first F#2
        CHECK_THAT (writeError (s), ContainsSubstring ("overlap") && ContainsSubstring ("42"));

        s = smallSpec();
        s.tracks[1].notes.push_back ({ 0, 60, 1, 42, 80 }); // same tick, same key
        CHECK_THAT (writeError (s), ContainsSubstring ("overlap"));

        // the edge: smallSpec's bass notes {0, 480} and {480, ...} on F#2 abut and are fine; one
        // tick longer and they overlap
        s = smallSpec();
        REQUIRE (s.tracks[1].notes[0].tick + s.tracks[1].notes[0].length == s.tracks[1].notes[1].tick);
        CHECK (midi::writeSmf (s).ok());
        s.tracks[1].notes[0].length += 1;
        CHECK_THAT (writeError (s), ContainsSubstring ("overlap") && ContainsSubstring ("length 481"));

        // Another channel, another key or another track is fine.
        s = smallSpec();
        s.tracks[1].notes.push_back ({ 400, 120, 2, 42, 80 });
        s.tracks[1].notes.push_back ({ 400, 120, 1, 43, 80 });
        s.tracks[0].notes.push_back ({ 400, 120, 1, 42, 80 });
        CHECK (midi::writeSmf (s).ok());
    }

    SECTION ("two values for one thing on one tick")
    {
        auto s = smallSpec();
        s.tracks[1].controllers.push_back ({ 480, 1, 74, 81 });
        CHECK_THAT (writeError (s), ContainsSubstring ("controller 74"));

        s = smallSpec();
        s.tempos.push_back ({ 0, 500000 });
        CHECK_THAT (writeError (s), ContainsSubstring ("tempo"));

        s = smallSpec();
        s.timeSignatures.push_back ({ 0, 3, 2, 24, 8 });
        CHECK_THAT (writeError (s), ContainsSubstring ("time signature"));

        s = smallSpec();
        s.keySignatures.push_back ({ 0, 0, false });
        CHECK_THAT (writeError (s), ContainsSubstring ("key signature"));

        s = smallSpec();
        s.tracks[1].programs = { { 0, 1, 38 }, { 0, 1, 39 } };
        CHECK_THAT (writeError (s), ContainsSubstring ("program"));

        s = smallSpec();
        s.markers.push_back ({ 1920, "drop" });
        CHECK_THAT (writeError (s), ContainsSubstring ("marker"));
    }

    SECTION ("conductor values out of range")
    {
        auto s = smallSpec();
        s.ppq = 0;
        CHECK_THAT (writeError (s), ContainsSubstring ("ppq"));
        s.ppq = 0x8000;
        CHECK_THAT (writeError (s), ContainsSubstring ("ppq"));

        s = smallSpec();
        s.tempos[0].microsPerQuarter = 0;
        CHECK_THAT (writeError (s), ContainsSubstring ("tempo"));
        s.tempos[0].microsPerQuarter = 0x1000000;
        CHECK_THAT (writeError (s), ContainsSubstring ("tempo"));

        s = smallSpec();
        s.keySignatures[0].sharps = 8;
        CHECK_THAT (writeError (s), ContainsSubstring ("key signature"));

        s = smallSpec();
        s.timeSignatures[0].numerator = 0;
        CHECK_THAT (writeError (s), ContainsSubstring ("time signature"));

        s = smallSpec();
        s.timeSignatures[0].denominatorPow2 = 8;
        CHECK_THAT (writeError (s), ContainsSubstring ("time signature"));
    }

    SECTION ("text that isn't UTF-8, or isn't text")
    {
        auto s = smallSpec();
        s.tracks[0].name = "Caf\xE9"; // Latin-1
        CHECK_THAT (writeError (s), ContainsSubstring ("UTF-8"));

        s = smallSpec();
        s.markers[0].text = "\xC3";
        CHECK_THAT (writeError (s), ContainsSubstring ("UTF-8"));

        s = smallSpec();
        s.tracks[0].texts = { { 0, midi::meta::trackName, "second name" } };
        CHECK_THAT (writeError (s), ContainsSubstring ("type"));

        s = smallSpec();
        s.tracks[0].texts = { { 0, midi::meta::tempo, "x" } };
        CHECK_THAT (writeError (s), ContainsSubstring ("type"));
    }

    SECTION ("controllers and programs out of range")
    {
        auto s = smallSpec();
        s.tracks[1].controllers[0].value = 128;
        CHECK_THAT (writeError (s), ContainsSubstring ("value"));

        s = smallSpec();
        s.tracks[1].controllers[0].number = 128;
        CHECK_THAT (writeError (s), ContainsSubstring ("controller"));

        s = smallSpec();
        s.tracks[1].programs = { { 0, 1, 128 } };
        CHECK_THAT (writeError (s), ContainsSubstring ("program"));
    }

    SECTION ("too many tracks for an SMF")
    {
        midi::SongSpec s;
        s.tracks.resize (65535); // with the conductor, one more than the header's 16 bits hold
        CHECK_THAT (writeError (s), ContainsSubstring ("tracks"));
        s.tracks.resize (65534);
        CHECK (midi::writeSmf (s).ok());
    }
}

TEST_CASE ("trimOverlaps ends each note where the next one on its key starts", "[midi]")
{
    std::vector<midi::Note> notes = {
        { 100, 200, 0, 42, 90 }, // overlaps the next F#2 by 80 ticks: ends at 220
        { 220, 100, 0, 42, 80 },
        { 0, 50, 0, 42, 70 }, // touches nothing
        { 500, 30, 0, 50, 60, 20 }, // two notes starting together merge: the highest of each value
        { 500, 90, 0, 50, 40, 10 },
        { 520, 10, 1, 50, 99 }, // another channel: untouched
    };
    const auto changed = midi::trimOverlaps (notes);
    CHECK (changed == 2u); // one shortened, one removed

    const std::vector<midi::Note> expected = {
        { 0, 50, 0, 42, 70 },
        { 100, 120, 0, 42, 90 },
        { 220, 100, 0, 42, 80 },
        { 500, 90, 0, 50, 60, 20 },
        { 520, 10, 1, 50, 99 },
    };
    CHECK (notes == expected);

    // The result is something the writer takes, and trimming again changes nothing.
    midi::SongSpec s;
    s.endTick = 1920;
    s.tracks = { { "Lane", notes, {}, {}, {} } };
    CHECK (midi::writeSmf (s).ok());
    CHECK (midi::trimOverlaps (notes) == 0u);

    SECTION ("the result doesn't depend on the input order")
    {
        std::vector<midi::Note> a = { { 0, 400, 3, 60, 10 }, { 100, 400, 3, 60, 20 }, { 100, 50, 3, 60, 30 }, { 300, 10, 3, 60, 40 } };
        std::vector<midi::Note> b (a.rbegin(), a.rend());
        midi::trimOverlaps (a);
        midi::trimOverlaps (b);
        CHECK (a == b);
        // 0-100, then the two at 100 merged (length 400, velocity 30) cut at 300, then 300-310.
        CHECK (a == std::vector<midi::Note> { { 0, 100, 3, 60, 10 }, { 100, 200, 3, 60, 30 }, { 300, 10, 3, 60, 40 } });
    }
}

TEST_CASE ("readSmf gives back what encode wrote", "[midi]")
{
    SECTION ("a song")
    {
        const auto file = toFile (smallSpec());
        REQUIRE (file.ok());
        const auto bytes = encode (file.value);
        REQUIRE (bytes.ok());
        CHECK (bytes.value == expectedSmall);
        CHECK (read (bytes.value) == file.value);
    }

    SECTION ("every kind of event")
    {
        midi::File f;
        f.format = 1;
        f.ppq = 96;
        midi::Track t;
        t.events = {
            metaEvent (0, midi::meta::trackName, { 'X' }),
            metaEvent (0, midi::meta::smpteOffset, { 0x60, 0x00, 0x00, 0x00, 0x00 }),
            metaEvent (0, midi::meta::sequencerSpecific, { 0x00, 0x00, 0x41 }),
            metaEvent (0, midi::meta::channelPrefix, { 0x05 }),
            metaEvent (0, 0x7E, {}), // an unknown meta type, kept as it is
            channelEvent (0, 0xC3, 12),
            channelEvent (1, 0xA3, 60, 33),
            channelEvent (2, 0xD3, 99),
            channelEvent (3, 0xE3, 0x7F, 0x7F),
            channelEvent (3, 0x93, 60, 0), // a note-on with velocity 0 stays one
            { 4, midi::status::sysex, 0, 0, { 0x7E, 0x7F, 0x09, 0x01, 0xF7 } },
            { 4, midi::status::sysexEscape, 0, 0, { 0xF8 } },
            channelEvent (midi::maxTick + 4, 0x83, 60, 0), // the largest delta
        };
        t.endTick = midi::maxTick + 10;
        f.tracks = { t, midi::Track {} };
        const auto bytes = encode (f);
        INFO (bytes.error);
        REQUIRE (bytes.ok());
        CHECK (read (bytes.value) == f);
    }

    SECTION ("format 0")
    {
        const midi::File f0 = read (runningStatusType0);
        CHECK (f0.format == 0);
        const auto bytes = encode (f0);
        REQUIRE (bytes.ok());
        CHECK (bytes.value.size() == runningStatusType0.size() + 4); // the 4 running-status events get their status bytes back
        CHECK (read (bytes.value) == f0);
    }
}

TEST_CASE ("the reader's helpers give the song back", "[midi]")
{
    const auto spec = smallSpec();
    const auto file = read (expectedSmall);
    REQUIRE (file.format == 1);
    REQUIRE (file.ppq == 480);
    REQUIRE (file.tracks.size() == 3);

    CHECK (midi::trackName (file.tracks[0]) == "Test song");
    CHECK (midi::trackName (file.tracks[1]) == "Kick");
    CHECK (midi::trackName (file.tracks[2]) == "Bass");
    CHECK (midi::tempoMap (file) == spec.tempos);
    CHECK (midi::timeSignaturesOf (file) == spec.timeSignatures);
    CHECK (midi::keySignaturesOf (file) == spec.keySignatures);
    CHECK (midi::markersOf (file) == spec.markers);
    CHECK (midi::notesOf (file.tracks[0]).empty());
    CHECK (midi::notesOf (file.tracks[1]) == spec.tracks[0].notes);
    CHECK (midi::notesOf (file.tracks[2]) == spec.tracks[1].notes);
    CHECK (midi::controllersOf (file.tracks[2]) == spec.tracks[1].controllers);
    CHECK (midi::textsOf (file.tracks[0]) == std::vector<midi::TextEvent> { { 0, midi::meta::trackName, "Test song" }, { 0, midi::meta::marker, "intro" }, { 1920, midi::meta::marker, "drop" } });

    // 3840 ticks at 134 BPM: 8 quarters of 447761 us.
    const auto tempos = midi::tempoMap (file);
    CHECK (midi::ticksToMicros (tempos, file.ppq, 3840) == 8 * 447761);
}

TEST_CASE ("readSmf reads a type-0 file with running status", "[midi]")
{
    const auto f = read (runningStatusType0);
    CHECK (f.format == 0);
    CHECK (f.ppq == 96);
    REQUIRE (f.tracks.size() == 1);
    const auto& t = f.tracks[0];
    CHECK (t.endTick == 240);

    const std::vector<midi::Event> expected = {
        metaEvent (0, midi::meta::tempo, { 0x07, 0xA1, 0x20 }),
        channelEvent (0, 0xC0, 5),
        channelEvent (0, 0x90, 0x3C, 0x64),
        channelEvent (0, 0x90, 0x40, 0x50),
        channelEvent (96, 0x90, 0x3C, 0x00),
        channelEvent (96, 0x90, 0x40, 0x00),
        channelEvent (96, 0xB0, 0x07, 0x64),
        channelEvent (112, 0xB0, 0x0A, 0x40),
        metaEvent (112, midi::meta::marker, { 'e', 'n', 'd' }),
        { 112, midi::status::sysex, 0, 0, { 0x7E, 0x7F, 0xF7 } },
        channelEvent (240, 0xE0, 0x00, 0x40),
    };
    CHECK (t.events == expected);

    // Note-ons with velocity 0 end the notes; the release velocity is 0.
    CHECK (midi::notesOf (t) == std::vector<midi::Note> { { 0, 96, 0, 60, 100, 0 }, { 0, 96, 0, 64, 80, 0 } });
    CHECK (midi::controllersOf (t) == std::vector<midi::Controller> { { 96, 0, 7, 100 }, { 112, 0, 10, 64 } });
    CHECK (midi::programsOf (t) == std::vector<midi::ProgramChange> { { 0, 0, 5 } });
    CHECK (midi::markersOf (f) == std::vector<midi::Marker> { { 112, "end" } });
    CHECK (midi::tempoMap (f) == std::vector<midi::Tempo> { { 0, 500000 } });
    CHECK (midi::ticksToMicros (midi::tempoMap (f), f.ppq, 240) == 1250000); // 2.5 quarters at 120 BPM
}

TEST_CASE ("running status carries across meta and sysex events", "[midi]")
{
    // clang-format off
    const auto f = read (smf (0, 96, { { 0x00, 0x90, 0x3C, 0x64, // note-on
                                         0x00, 0xFF, 0x01, 0x01, 0x78, // a text event
                                         0x10, 0x3C, 0x00, // running status after the meta
                                         0x00, 0xF0, 0x01, 0xF7, // sysex
                                         0x00, 0x3E, 0x20, // running status after the sysex
                                         0x10, 0x3E, 0x00,
                                         0x00, 0xFF, 0x2F, 0x00 } }));
    // clang-format on
    const auto notes = midi::notesOf (f.tracks.at (0));
    CHECK (notes == std::vector<midi::Note> { { 0, 16, 0, 60, 100, 0 }, { 16, 16, 0, 62, 32, 0 } });
}

TEST_CASE ("notesOf pairs each note-off with the oldest sounding note", "[midi]")
{
    midi::Track t;
    t.events = {
        channelEvent (0, 0x90, 60, 100),
        channelEvent (10, 0x90, 60, 90), // the same key again, still sounding
        channelEvent (20, 0x80, 60, 5), // ends the first
        channelEvent (30, 0x80, 60, 6), // ends the second
        channelEvent (35, 0x80, 61, 7), // nothing to end: ignored
        channelEvent (40, 0x91, 60, 80), // channel 2: its own note
        channelEvent (40, 0x81, 60, 0), // ends on the tick it started: length 0
        channelEvent (50, 0x90, 72, 70), // never ended: ends on endTick
        channelEvent (60, 0x90, 200, 9), // a hand-built bad key (readSmf never gives one): skipped
        channelEvent (61, 0x80, 255, 9),
    };
    t.endTick = 100;
    CHECK (midi::notesOf (t) == std::vector<midi::Note> {
               { 0, 20, 0, 60, 100, 5 },
               { 10, 20, 0, 60, 90, 6 },
               { 40, 0, 1, 60, 80, 0 },
               { 50, 50, 0, 72, 70, 0 },
           });
}

TEST_CASE ("tempoMap and ticksToMicros follow tempo changes exactly", "[midi]")
{
    midi::File f;
    f.ppq = 480;
    midi::Track conductor;
    conductor.events = {
        metaEvent (960, midi::meta::tempo, { 0x07, 0xA1, 0x20 }), // 120 BPM from tick 960
        metaEvent (1920, midi::meta::tempo, { 0x06, 0xD5, 0x11 }), // 134 BPM from tick 1920
        metaEvent (1920, midi::meta::tempo, { 0x0F, 0x42, 0x40 }), // then 60 BPM on the same tick: the last counts
        metaEvent (2000, midi::meta::tempo, { 0x00, 0x00 }), // malformed: skipped
        metaEvent (2100, midi::meta::tempo, { 0x00, 0x00, 0x00 }), // 0: skipped
    };
    conductor.endTick = 3840;
    midi::Track lane;
    lane.events = { metaEvent (2880, midi::meta::tempo, { 0x07, 0xA1, 0x20 }) }; // tempo in another track still counts
    lane.endTick = 3840;
    f.tracks = { conductor, lane };

    const auto tempos = midi::tempoMap (f);
    CHECK (tempos == std::vector<midi::Tempo> { { 960, 500000 }, { 1920, 447761 }, { 1920, 1000000 }, { 2880, 500000 } });

    CHECK (midi::ticksToMicros (tempos, 480, 0) == 0);
    CHECK (midi::ticksToMicros (tempos, 480, 960) == 1000000); // the default 120 BPM before the first event
    CHECK (midi::ticksToMicros (tempos, 480, 1920) == 2000000);
    CHECK (midi::ticksToMicros (tempos, 480, 2880) == 4000000); // 2 quarters at 60 BPM
    CHECK (midi::ticksToMicros (tempos, 480, 3000) == 4125000);
    CHECK (midi::ticksToMicros (tempos, 480, 1) == 1041); // 500000 / 480 = 1041.67, rounded down
    CHECK (midi::ticksToMicros ({}, 480, 480) == 500000);
    CHECK (midi::ticksToMicros (tempos, 0, 480) == 0); // nonsense in, 0 out
    CHECK (midi::ticksToMicros (tempos, 480, -1) == 0);
}

TEST_CASE ("readSmf refuses malformed files", "[midi]")
{
    const Bytes eot = { 0x00, 0xFF, 0x2F, 0x00 };

    CHECK_THAT (readError ({}), ContainsSubstring ("MThd"));
    CHECK_THAT (readError ({ 'R', 'I', 'F', 'F', 0, 0, 0, 6, 0, 0, 0, 1, 0, 96 }), ContainsSubstring ("MThd"));

    Bytes shortHeader = smf (0, 96, { eot });
    shortHeader[7] = 5;
    CHECK_THAT (readError (shortHeader), ContainsSubstring ("header"));

    CHECK_THAT (readError (smf (2, 96, { eot })), ContainsSubstring ("format 2"));
    CHECK_THAT (readError (smf (0, 96, { eot, eot })), ContainsSubstring ("format 0"));
    CHECK_THAT (readError (smf (1, 0, { eot })), ContainsSubstring ("ticks per quarter"));
    CHECK_THAT (readError (smf (1, 0xE728, { eot })), ContainsSubstring ("SMPTE"));

    CHECK_THAT (readError (smf (1, 96, { { 0x00, 0x3C, 0x64 } })), ContainsSubstring ("running status"));
    CHECK_THAT (readError (smf (1, 96, { { 0x80, 0x80, 0x80, 0x80, 0x00, 0xFF, 0x2F, 0x00 } })), ContainsSubstring ("4 bytes"));
    CHECK_THAT (readError (smf (1, 96, { { 0x00, 0xF2, 0x00, 0x00 } })), ContainsSubstring ("0xF2"));
    CHECK_THAT (readError (smf (1, 96, { { 0x00, 0x90, 0x3C, 0x90 } })), ContainsSubstring ("data byte"));
    CHECK_THAT (readError (smf (1, 96, { { 0x00, 0xFF, 0x80, 0x00 } })), ContainsSubstring ("meta"));
    CHECK_THAT (readError (smf (1, 96, { { 0x00, 0x90, 0x3C, 0x64 } })), ContainsSubstring ("end-of-track"));
    CHECK_THAT (readError (smf (1, 96, { {} })), ContainsSubstring ("end-of-track"));
    CHECK_THAT (readError (smf (1, 96, { { 0x00, 0xFF, 0x01, 0x05, 0x41 } })), ContainsSubstring ("track 0"));

    Bytes missingTrack = smf (1, 96, { eot, eot });
    missingTrack.resize (missingTrack.size() - 12); // drop the second MTrk chunk
    CHECK_THAT (readError (missingTrack), ContainsSubstring ("1 of 2"));

    Bytes longChunk = smf (1, 96, { eot });
    longChunk[21] = 0x05; // the MTrk says 5 bytes; 4 follow
    CHECK_THAT (readError (longChunk), ContainsSubstring ("truncated"));
}

TEST_CASE ("readSmf skips what the SMF spec says to skip", "[midi]")
{
    const Bytes body = { 0x00, 0x90, 0x3C, 0x64, 0x60, 0x80, 0x3C, 0x40, 0x00, 0xFF, 0x2F, 0x00 };
    const midi::File reference = read (smf (1, 96, { body }));
    REQUIRE (midi::notesOf (reference.tracks.at (0)) == std::vector<midi::Note> { { 0, 96, 0, 60, 100, 64 } });

    SECTION ("a longer header")
    {
        Bytes f = smf (1, 96, { body });
        f[7] = 8;
        f.insert (f.begin() + 14, { 0xAB, 0xCD });
        CHECK (read (f) == reference);
    }

    SECTION ("an unknown chunk")
    {
        Bytes f = smf (1, 96, { body });
        f.insert (f.begin() + 14, { 'X', 'F', 'I', 'H', 0, 0, 0, 3, 1, 2, 3 });
        CHECK (read (f) == reference);
    }

    SECTION ("bytes after the last track chunk")
    {
        Bytes f = smf (1, 96, { body });
        f.insert (f.end(), { 0x00, 0x00, 0x13 });
        CHECK (read (f) == reference);
    }

    SECTION ("bytes after the end-of-track event")
    {
        Bytes padded = body;
        padded.insert (padded.end(), { 0x00, 0x90, 0x3C });
        CHECK (read (smf (1, 96, { padded })) == reference);
    }
}

TEST_CASE ("every truncation of a valid file fails cleanly", "[midi]")
{
    for (const Bytes* file : { &expectedSmall, &runningStatusType0 })
    {
        REQUIRE (midi::readSmf (*file).ok());
        for (std::size_t n = 0; n < file->size(); ++n)
        {
            const Bytes prefix (file->begin(), file->begin() + static_cast<std::ptrdiff_t> (n));
            const auto r = midi::readSmf (prefix);
            INFO ("prefix of " << n << " bytes");
            CHECK_FALSE (r.ok());
            CHECK_FALSE (r.error.empty());
        }
    }
}

TEST_CASE ("mutated files never crash the reader and what it reads re-encodes as it was", "[midi]")
{
    // Deterministic mutations of the two valid files: flipped and special bytes, truncations,
    // inserted and deleted bytes, and data bytes swapped for others, which keep the structure so
    // the reader accepts more of them. Under the sanitizers this also shows the reader stays in
    // bounds.
    int accepted = 0;
    int refused = 0;
    for (std::uint32_t i = 0; i < 24000; ++i)
    {
        const rng::Key k = rng::seedKey (i).with (rng::tag ("smf-fuzz"));
        Bytes bytes = (i % 2 == 0) ? expectedSmall : runningStatusType0;
        const std::uint32_t ops = 1 + rng::below (k, 3);
        for (std::uint32_t op = 0; op < ops && !bytes.empty(); ++op)
        {
            const rng::Key opKey = k.with (op);
            const auto pos = static_cast<std::ptrdiff_t> (rng::below (opKey.with (1), static_cast<std::uint32_t> (bytes.size())));
            const auto value = static_cast<std::uint8_t> (rng::below (opKey.with (2), 256));
            constexpr std::array<std::uint8_t, 8> special = { 0x00, 0x01, 0x7F, 0x80, 0x81, 0xF0, 0xFF, 0x2F };
            auto& at = bytes[static_cast<std::size_t> (pos)];
            switch (rng::below (opKey.with (0), 6))
            {
                case 0:
                    at ^= static_cast<std::uint8_t> (value | 1);
                    break;
                case 1:
                    at = special[value % special.size()];
                    break;
                case 2:
                    bytes.resize (static_cast<std::size_t> (pos));
                    break;
                case 3:
                    bytes.insert (bytes.begin() + pos, value);
                    break;
                case 4:
                    bytes.erase (bytes.begin() + pos);
                    break;
                default:
                    if (at < 0x80 && pos >= 22) // a data byte in a track: another data byte
                        at = static_cast<std::uint8_t> (value & 0x7F);
                    break;
            }
        }

        const auto r = midi::readSmf (bytes);
        if (!r.ok())
        {
            ++refused;
            REQUIRE_FALSE (r.error.empty());
            continue;
        }
        ++accepted;
        const auto again = encode (r.value);
        INFO ("mutation " << i << ": " << again.error);
        REQUIRE (again.ok());
        const auto reread = midi::readSmf (again.value);
        REQUIRE (reread.ok());
        REQUIRE (reread.value == r.value);
    }
    // Both paths ran many times.
    INFO ("accepted " << accepted << ", refused " << refused);
    CHECK (accepted > 3000);
    CHECK (refused > 3000);
}

TEST_CASE ("encode refuses Files the reader couldn't give back", "[midi]")
{
    const auto oneTrack = [] (std::vector<midi::Event> events, std::int64_t endTick = 100) {
        midi::File f;
        f.tracks = { midi::Track { std::move (events), endTick } };
        return f;
    };
    const auto encodeError = [] (const midi::File& f) {
        const auto r = encode (f);
        CHECK_FALSE (r.ok());
        return r.error;
    };

    CHECK (encode (oneTrack ({ channelEvent (0, 0x90, 60, 100) })).ok());

    midi::File f = oneTrack ({});
    f.format = 2;
    CHECK_THAT (encodeError (f), ContainsSubstring ("format"));
    f.format = 0;
    f.tracks.push_back ({});
    CHECK_THAT (encodeError (f), ContainsSubstring ("format 0"));

    f = oneTrack ({});
    f.ppq = 0;
    CHECK_THAT (encodeError (f), ContainsSubstring ("ppq"));
    f.ppq = 0x8000;
    CHECK_THAT (encodeError (f), ContainsSubstring ("ppq"));

    CHECK_THAT (encodeError (oneTrack ({ channelEvent (10, 0x90, 60, 100), channelEvent (5, 0x80, 60, 0) })), ContainsSubstring ("backwards"));
    CHECK_THAT (encodeError (oneTrack ({ channelEvent (-1, 0x90, 60, 100) })), ContainsSubstring ("tick"));
    CHECK_THAT (encodeError (oneTrack ({ channelEvent (200, 0x90, 60, 100) })), ContainsSubstring ("end"));
    CHECK_THAT (encodeError (oneTrack ({ channelEvent (0, 0x90, 0x80, 100) })), ContainsSubstring ("data byte"));
    CHECK_THAT (encodeError (oneTrack ({ channelEvent (0, 0xC0, 5, 1) })), ContainsSubstring ("data2"));
    CHECK_THAT (encodeError (oneTrack ({ { 0, 0x90, 60, 100, { 1 } } })), ContainsSubstring ("payload"));
    CHECK_THAT (encodeError (oneTrack ({ channelEvent (0, 0x7F, 0) })), ContainsSubstring ("status"));
    CHECK_THAT (encodeError (oneTrack ({ channelEvent (0, 0xF1, 0) })), ContainsSubstring ("status"));
    CHECK_THAT (encodeError (oneTrack ({ metaEvent (0, midi::meta::endOfTrack, {}) })), ContainsSubstring ("end-of-track"));
    CHECK_THAT (encodeError (oneTrack ({ metaEvent (0, 0x80, {}) })), ContainsSubstring ("meta type"));
    CHECK_THAT (encodeError (oneTrack ({ { 0, midi::status::meta, midi::meta::text, 1, {} } })), ContainsSubstring ("data2"));
    CHECK_THAT (encodeError (oneTrack ({ { 0, midi::status::sysex, 3, 0, {} } })), ContainsSubstring ("data1"));
    CHECK_THAT (encodeError (oneTrack ({ channelEvent (midi::maxTick + 1, 0x90, 60, 100) }, midi::maxTick + 1)), ContainsSubstring ("delta"));
    CHECK_THAT (encodeError (oneTrack ({}, midi::maxTick + 1)), ContainsSubstring ("delta"));
}
