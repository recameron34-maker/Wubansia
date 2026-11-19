// Tests for core/midi/for_synths.h: a file's pitched tracks made for the person's own synths (ADR 0027), for MIDI
// drag-out's "For my synths" option (plugin/source/dragout/DragFiles.h).
//
// The rules on hand-made notes first, then a hand-made file whose rewritten tracks are spelled out byte for byte, then
// every committed content/midi file, then chords as chords (a chord sample's track written as its chords) on
// hand-made files.

#include "midi/for_synths.h"
#include "midi/reader.h"
#include "midi/track_select.h"
#include "midi/writer.h"
#include "util/file_io.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <initializer_list>
#include <set>
#include <string>
#include <tuple>
#include <vector>

using namespace wubansia;
using Catch::Matchers::ContainsSubstring;
using midi::SynthPart;

namespace
{
    using Bytes = std::vector<std::uint8_t>;

    constexpr std::uint16_t ppq = 960; // the kits' files: a 16th is 240 ticks, the gap 10
    constexpr std::int64_t bar = 3840;

    midi::Note note (std::int64_t tick, std::int64_t length, int key, int velocity = 100, int channel = 0)
    {
        midi::Note n;
        n.tick = tick;
        n.length = length;
        n.key = static_cast<std::uint8_t> (key);
        n.velocity = static_cast<std::uint8_t> (velocity);
        n.channel = static_cast<std::uint8_t> (channel);
        return n;
    }

    // (tick, length) of each note, by (tick, channel, key).
    std::vector<std::pair<std::int64_t, std::int64_t>> spans (const std::vector<midi::Note>& notes)
    {
        std::vector<std::pair<std::int64_t, std::int64_t>> out;
        for (const auto& n : notes)
            out.emplace_back (n.tick, n.length);
        return out;
    }

    std::vector<int> keys (const std::vector<midi::Note>& notes)
    {
        std::vector<int> out;
        for (const auto& n : notes)
            out.push_back (n.key);
        return out;
    }

    std::vector<int> velocities (const std::vector<midi::Note>& notes)
    {
        std::vector<int> out;
        for (const auto& n : notes)
            out.push_back (n.velocity);
        return out;
    }

    // ------------------------------------------------------------------ files

    void append (Bytes& out, const Bytes& more) { out.insert (out.end(), more.begin(), more.end()); }

    Bytes chunk (const char (&id)[5], const Bytes& body)
    {
        Bytes out (id, id + 4);
        for (int shift = 24; shift >= 0; shift -= 8)
            out.push_back (static_cast<std::uint8_t> ((body.size() >> shift) & 0xFF));
        append (out, body);
        return out;
    }

    Bytes mtrk (const Bytes& body) { return chunk ("MTrk", body); }

    Bytes mthd (std::uint16_t tracks)
    {
        return chunk ("MThd", { 0x00, 0x01, 0x00, static_cast<std::uint8_t> (tracks), static_cast<std::uint8_t> (ppq >> 8), static_cast<std::uint8_t> (ppq & 0xFF) });
    }

    Bytes concat (std::initializer_list<Bytes> parts)
    {
        Bytes out;
        for (const auto& p : parts)
            append (out, p);
        return out;
    }

    // clang-format off
    // The conductor: a name, 134 BPM, 4/4, F# minor; one bar at 960 PPQ.
    const Bytes conductorBody = {
        0x00, 0xFF, 0x03, 0x04, 'S', 'o', 'n', 'g',
        0x00, 0xFF, 0x51, 0x03, 0x06, 0xD5, 0x11,
        0x00, 0xFF, 0x58, 0x04, 0x04, 0x02, 0x18, 0x08,
        0x00, 0xFF, 0x59, 0x02, 0x03, 0x01,
        0x9E, 0x00, 0xFF, 0x2F, 0x00,
    };
    // Kick on channel 10, 120-tick triggers, with running status: drums stay as they are.
    const Bytes kickBody = {
        0x00, 0xFF, 0x03, 0x04, 'K', 'i', 'c', 'k',
        0x00, 0x99, 0x24, 0x6E,
        0x78, 0x24, 0x00,
        0x8E, 0x08, 0x99, 0x24, 0x64,
        0x78, 0x24, 0x00,
        0x8E, 0x08, 0xFF, 0x2F, 0x00,
    };
    // Bass on channel 1, as the kits write it: a CC74 at 0; F#0 (30) sliding into C#1 (37) at 240, 60 ticks over; C#1 at
    // velocity 44 for a 32nd; A0 (33) for 60 ticks; and F#0 at 3720, 60 ticks before the bar's end.
    const Bytes bassBody = {
        0x00, 0xFF, 0x03, 0x04, 'B', 'a', 's', 's',
        0x00, 0xB0, 0x4A, 0x40,
        0x00, 0x90, 0x1E, 0x64,
        0x81, 0x70, 0x90, 0x25, 0x2C,
        0x3C, 0x80, 0x1E, 0x40,
        0x3C, 0x80, 0x25, 0x40,
        0x84, 0x58, 0x90, 0x21, 0x7F,
        0x3C, 0x80, 0x21, 0x40,
        0x95, 0x0C, 0x90, 0x1E, 0x5A,
        0x3C, 0x80, 0x1E, 0x40,
        0x3C, 0xFF, 0x2F, 0x00,
    };
    // The same, made for synths and laid out by the writer: an octave up (42, 49, 45, 42); the slide cut 10 ticks before
    // C#1 (230); C#1 a 16th long at velocity 72; A0 a 16th; the last note to the bar's end (120).
    const Bytes bassForSynths = {
        0x00, 0xFF, 0x03, 0x04, 'B', 'a', 's', 's',
        0x00, 0xB0, 0x4A, 0x40,
        0x00, 0x90, 0x2A, 0x64,
        0x81, 0x66, 0x80, 0x2A, 0x40,
        0x0A, 0x90, 0x31, 0x48,
        0x81, 0x70, 0x80, 0x31, 0x40,
        0x83, 0x60, 0x90, 0x2D, 0x7F,
        0x81, 0x70, 0x80, 0x2D, 0x40,
        0x93, 0x58, 0x90, 0x2A, 0x5A,
        0x78, 0x80, 0x2A, 0x40,
        0x00, 0xFF, 0x2F, 0x00,
    };
    // Lead on channel 2: program 5; a chord at 0 (C4 for 480 at velocity 70, E4 for 960, G4 for 1000); C5 at 960 for 100
    // ticks, with a text and CC1 on its tick. In file order the chord's E4 lets go after C5 starts.
    const Bytes leadBody = {
        0x00, 0xFF, 0x03, 0x04, 'L', 'e', 'a', 'd',
        0x00, 0xC1, 0x05,
        0x00, 0x91, 0x3C, 0x46,
        0x00, 0x91, 0x40, 0x50,
        0x00, 0x91, 0x43, 0x5A,
        0x83, 0x60, 0x81, 0x3C, 0x40,
        0x83, 0x60, 0x91, 0x48, 0x64,
        0x00, 0x81, 0x40, 0x40,
        0x00, 0xFF, 0x01, 0x02, 'h', 'i',
        0x00, 0xB1, 0x01, 0x20,
        0x28, 0x81, 0x43, 0x40,
        0x3C, 0x81, 0x48, 0x40,
        0x95, 0x5C, 0xFF, 0x2F, 0x00,
    };
    // The same, made for synths: C4 up to velocity 72 and its 480 kept; E4 and G4 end 10 ticks before C5 (950); C5 a 16th;
    // on its tick the text, then CC1, then the note-on.
    const Bytes leadForSynths = {
        0x00, 0xFF, 0x03, 0x04, 'L', 'e', 'a', 'd',
        0x00, 0xC1, 0x05,
        0x00, 0x91, 0x3C, 0x48,
        0x00, 0x91, 0x40, 0x50,
        0x00, 0x91, 0x43, 0x5A,
        0x83, 0x60, 0x81, 0x3C, 0x40,
        0x83, 0x56, 0x81, 0x40, 0x40,
        0x00, 0x81, 0x43, 0x40,
        0x0A, 0xFF, 0x01, 0x02, 'h', 'i',
        0x00, 0xB1, 0x01, 0x20,
        0x00, 0x91, 0x48, 0x64,
        0x81, 0x70, 0x81, 0x48, 0x40,
        0x94, 0x50, 0xFF, 0x2F, 0x00,
    };
    // clang-format on

    // Format 1, 960 PPQ: the conductor, Kick, Bass and Lead.
    Bytes fourTracks() { return concat ({ mthd (4), mtrk (conductorBody), mtrk (kickBody), mtrk (bassBody), mtrk (leadBody) }); }

    Bytes made (const Bytes& smf, std::initializer_list<SynthPart> parts)
    {
        const std::vector<SynthPart> list (parts);
        auto r = midi::forSynths (smf, list);
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

    // Each track's MTrk chunk, whole.
    std::vector<Bytes> chunksOf (const Bytes& smf)
    {
        const auto chunks = midi::trackChunks (smf);
        REQUIRE (chunks.ok());
        std::vector<Bytes> out;
        for (const auto& c : chunks.value)
            out.emplace_back (smf.begin() + static_cast<std::ptrdiff_t> (c.offset), smf.begin() + static_cast<std::ptrdiff_t> (c.offset + c.size));
        return out;
    }

    // Whether every note-off on a tick comes before every note-on on it (docs/midi-map.md, "On a tick").
    bool offsBeforeOns (const midi::Track& track)
    {
        std::int64_t tick = -1;
        bool onSeen = false;
        for (const auto& e : track.events)
        {
            if (e.tick != tick)
            {
                tick = e.tick;
                onSeen = false;
            }
            if (e.isNoteOn())
                onSeen = true;
            else if (e.isNoteOff() && onSeen)
                return false;
        }
        return true;
    }

    // Every committed content/midi file, in path order.
    std::vector<std::filesystem::path> contentMidiFiles()
    {
        const auto root = std::filesystem::path (WUBANSIA_SOURCE_DIR) / "content" / "midi";
        std::vector<std::filesystem::path> files;
        for (const auto& entry : std::filesystem::recursive_directory_iterator (root))
            if (entry.is_regular_file() && entry.path().extension() == ".mid")
                files.push_back (entry.path());
        std::sort (files.begin(), files.end());
        return files;
    }

    // What the drag makes of a kit's track, by name (plugin/source/dragout/DragFiles.cpp): drums and one-key sounds
    // (accents, FX, vox chops) as they are; the bass lane's pitched tracks (Bass, FUT01's Organ Bass, FUT02's Drop
    // Glide) an octave up; every other pitched track (lead, chords, chord voicing, vox, the kits' extras) for synths.
    SynthPart partOf (const std::string& name)
    {
        static const std::set<std::string> asIs { "Kick", "Rumble", "Snare", "Clap", "Snare/Clap", "Hats", "Perc", "Accents", "FX", "Vox chops" };
        static const std::set<std::string> bass { "Bass", "Organ Bass", "Drop Glide" };
        if (asIs.count (name) != 0)
            return SynthPart::asIs;
        return bass.count (name) != 0 ? SynthPart::bass : SynthPart::pitched;
    }

    // ------------------------------------------------------------------ chords as chords

    // FUT01's stabs: F#m9 on F#3 (54) for keys 0-58, Bm9 on B3 (59) above, each root, 5, m7, 9, m3.
    midi::ChordSample stabs()
    {
        return { { 0, 58, { 0, 7, 10, 14, 15 } }, { 59, 127, { 0, 7, 10, 14, 15 } } };
    }

    std::vector<int> keysOf (const std::vector<std::uint8_t>& chord) { return std::vector<int> (chord.begin(), chord.end()); }

    constexpr int chordChannel = 3; // MIDI channel 4

    // Format 1, 960 PPQ, one bar: a conductor and one Chords track on channel 4 holding notes, as the writer lays it out.
    Bytes chordsFile (const std::vector<midi::Note>& notes)
    {
        midi::SongSpec song;
        song.ppq = ppq;
        song.endTick = bar;
        song.name = "Song";
        midi::TrackSpec chords;
        chords.name = "Chords";
        chords.notes = notes;
        song.tracks.push_back (std::move (chords));
        auto r = midi::writeSmf (song);
        INFO (r.error);
        REQUIRE (r.ok());
        return r.value;
    }

    Bytes madeWithChords (const Bytes& smf, std::initializer_list<SynthPart> parts, const std::vector<midi::ChordSample>& chords)
    {
        const std::vector<SynthPart> list (parts);
        auto r = midi::forSynths (smf, list, chords);
        INFO (r.error);
        REQUIRE (r.ok());
        return r.value;
    }

    // Track 1's notes when a chordsFile () is made for synths with track 1 marked chords and played by sample.
    std::vector<midi::Note> chordsMade (const Bytes& smf, const midi::ChordSample& sample)
    {
        const auto file = read (madeWithChords (smf, { SynthPart::asIs, SynthPart::chords }, { {}, sample }));
        REQUIRE (file.tracks.size() == 2);
        return midi::notesOf (file.tracks[1]);
    }
}

TEST_CASE ("synthGap is a 96th of a quarter, at least a tick: 10 at the kits' 960 PPQ", "[midi][for_synths]")
{
    CHECK (midi::synthGap (960) == 10);
    CHECK (midi::synthGap (480) == 5);
    CHECK (midi::synthGap (96) == 1);
    CHECK (midi::synthGap (95) == 1);
    CHECK (midi::synthGap (1) == 1);
    CHECK (midi::kSynthVelocityFloor == 72);
}

TEST_CASE ("synthNotes: a note lasts a 16th at least, and as long as written", "[midi][for_synths]")
{
    CHECK (spans (midi::synthNotes ({ note (0, 120, 40) }, ppq, bar, false)) == spans ({ note (0, 240, 40) }));
    CHECK (spans (midi::synthNotes ({ note (0, 0, 40) }, ppq, bar, false)) == spans ({ note (0, 240, 40) })); // a zero-length note too
    CHECK (spans (midi::synthNotes ({ note (0, 239, 40) }, ppq, bar, false)) == spans ({ note (0, 240, 40) }));
    CHECK (spans (midi::synthNotes ({ note (0, 240, 40) }, ppq, bar, false)) == spans ({ note (0, 240, 40) }));
    CHECK (spans (midi::synthNotes ({ note (0, 1500, 40) }, ppq, bar, false)) == spans ({ note (0, 1500, 40) }));

    // At 480 PPQ a 16th is 120 ticks and the gap 5.
    CHECK (spans (midi::synthNotes ({ note (0, 30, 40), note (100, 30, 41), note (400, 30, 42) }, 480, 1920, false))
           == spans ({ note (0, 95, 40), note (100, 120, 41), note (400, 120, 42) }));
}

TEST_CASE ("synthNotes: a note ends 10 ticks before the next note on its track starts, so a slide is two plain notes", "[midi][for_synths]")
{
    // A slide: the first note held 60 ticks into the next.
    CHECK (spans (midi::synthNotes ({ note (0, 300, 30), note (240, 240, 37) }, ppq, bar, false)) == spans ({ note (0, 230, 30), note (240, 240, 37) }));
    // Notes that touch, or end less than 10 ticks before the next, end 10 ticks before it.
    CHECK (spans (midi::synthNotes ({ note (0, 480, 30), note (480, 240, 37) }, ppq, bar, false)) == spans ({ note (0, 470, 30), note (480, 240, 37) }));
    CHECK (spans (midi::synthNotes ({ note (0, 475, 30), note (480, 240, 37) }, ppq, bar, false)) == spans ({ note (0, 470, 30), note (480, 240, 37) }));
    CHECK (spans (midi::synthNotes ({ note (0, 470, 30), note (480, 240, 37) }, ppq, bar, false)) == spans ({ note (0, 470, 30), note (480, 240, 37) }));
    // A note the next one comes sooner than a 16th after lasts until 10 ticks before it.
    CHECK (spans (midi::synthNotes ({ note (0, 60, 30), note (120, 60, 37) }, ppq, bar, false)) == spans ({ note (0, 110, 30), note (120, 240, 37) }));
    // The next note is the next on the track, whatever its channel or key, the same key included.
    CHECK (spans (midi::synthNotes ({ note (0, 960, 30, 100, 0), note (480, 240, 70, 100, 5) }, ppq, bar, false))
           == spans ({ note (0, 470, 30, 100, 0), note (480, 240, 70, 100, 5) }));
    CHECK (spans (midi::synthNotes ({ note (0, 960, 30), note (480, 960, 30) }, ppq, bar, false)) == spans ({ note (0, 470, 30), note (480, 960, 30) }));
    // Notes 10 ticks apart or closer still last a tick, and never reach the next one.
    CHECK (spans (midi::synthNotes ({ note (0, 120, 30), note (10, 120, 37), note (11, 120, 38), note (14, 120, 39) }, ppq, bar, false))
           == spans ({ note (0, 1, 30), note (10, 1, 37), note (11, 1, 38), note (14, 240, 39) }));
}

TEST_CASE ("synthNotes: notes that start together are a chord, and end 10 ticks before the next chord at most", "[midi][for_synths]")
{
    const auto out = midi::synthNotes ({ note (0, 400, 60), note (0, 960, 64), note (0, 1200, 67), note (960, 960, 62), note (960, 100, 65) }, ppq, bar, false);
    CHECK (spans (out) == spans ({ note (0, 400, 60), note (0, 950, 64), note (0, 950, 67), note (960, 960, 62), note (960, 240, 65) }));
    CHECK (keys (out) == std::vector<int> { 60, 64, 67, 62, 65 });
}

TEST_CASE ("synthNotes: no note ends after its track does", "[midi][for_synths]")
{
    CHECK (spans (midi::synthNotes ({ note (3700, 40, 30) }, ppq, bar, false)) == spans ({ note (3700, 140, 30) }));
    CHECK (spans (midi::synthNotes ({ note (3839, 1, 30) }, ppq, bar, false)) == spans ({ note (3839, 1, 30) }));
    CHECK (spans (midi::synthNotes ({ note (0, 3840, 30) }, ppq, bar, false)) == spans ({ note (0, 3840, 30) }));
}

TEST_CASE ("synthNotes: velocities below 72 come up to it, louder ones stay; release velocities stay", "[midi][for_synths]")
{
    auto soft = note (0, 240, 40, 1);
    soft.releaseVelocity = 0;
    const auto out = midi::synthNotes ({ soft, note (480, 240, 41, 44), note (960, 240, 42, 71), note (1440, 240, 43, 72), note (1920, 240, 44, 73),
                                           note (2400, 240, 45, 127) },
        ppq, bar, false);
    CHECK (velocities (out) == std::vector<int> { 72, 72, 72, 72, 73, 127 });
    REQUIRE (out.size() == 6);
    CHECK (out[0].releaseVelocity == 0);
    CHECK (out[1].releaseVelocity == 64);
}

TEST_CASE ("synthNotes: a bass track goes up an octave, unless a key would pass 127; other tracks keep their keys", "[midi][for_synths]")
{
    const std::vector<midi::Note> line { note (0, 240, 26), note (480, 240, 30), note (960, 240, 43) };
    CHECK (keys (midi::synthNotes (line, ppq, bar, true)) == std::vector<int> { 38, 42, 55 });
    CHECK (keys (midi::synthNotes (line, ppq, bar, false)) == std::vector<int> { 26, 30, 43 });
    CHECK (keys (midi::synthNotes ({ note (0, 240, 30), note (480, 240, 115) }, ppq, bar, true)) == std::vector<int> { 42, 127 });
    CHECK (keys (midi::synthNotes ({ note (0, 240, 30), note (480, 240, 116) }, ppq, bar, true)) == std::vector<int> { 30, 116 });
    CHECK (midi::synthNotes ({}, ppq, bar, true).empty());
}

TEST_CASE ("synthNotes: notes on one tick, channel and key become one, the result sorted by tick, channel and key", "[midi][for_synths]")
{
    const auto out = midi::synthNotes ({ note (480, 240, 50, 90, 1), note (0, 100, 45, 80), note (0, 300, 45, 100), note (480, 240, 40, 90, 2) }, ppq, bar, false);
    REQUIRE (out.size() == 3);
    CHECK (spans (out) == spans ({ note (0, 300, 45), note (480, 240, 50), note (480, 240, 40) }));
    CHECK (keys (out) == std::vector<int> { 45, 50, 40 });
    CHECK (velocities (out) == std::vector<int> { 100, 90, 90 });
    CHECK (out[1].channel == 1);
    CHECK (out[2].channel == 2);
}

TEST_CASE ("forSynths rewrites the pitched and bass tracks in the writer's order, and copies the rest byte for byte", "[midi][for_synths]")
{
    const auto smf = fourTracks();
    const auto out = made (smf, { SynthPart::asIs, SynthPart::asIs, SynthPart::bass, SynthPart::pitched });
    CHECK (out == concat ({ mthd (4), mtrk (conductorBody), mtrk (kickBody), mtrk (bassForSynths), mtrk (leadForSynths) }));

    // Read back: every note as synthNotes () gives it, everything else as it was.
    const auto before = read (smf);
    const auto after = read (out);
    REQUIRE (after.tracks.size() == 4);
    CHECK (after.tracks[0] == before.tracks[0]);
    CHECK (after.tracks[1] == before.tracks[1]);
    for (const std::size_t i : { std::size_t { 2 }, std::size_t { 3 } })
    {
        INFO ("track " << i);
        const auto& a = after.tracks[i];
        const auto& b = before.tracks[i];
        CHECK (midi::notesOf (a) == midi::synthNotes (midi::notesOf (b), ppq, b.endTick, i == 2));
        CHECK (midi::trackName (a) == midi::trackName (b));
        CHECK (midi::controllersOf (a) == midi::controllersOf (b));
        CHECK (midi::programsOf (a) == midi::programsOf (b));
        CHECK (midi::textsOf (a) == midi::textsOf (b));
        CHECK (a.endTick == b.endTick);
        CHECK (offsBeforeOns (a));
    }
}

TEST_CASE ("forSynths leaves the tracks past parts' end, and every asIs track, as they are", "[midi][for_synths]")
{
    const auto smf = fourTracks();
    CHECK (made (smf, {}) == smf);
    CHECK (made (smf, { SynthPart::asIs, SynthPart::asIs, SynthPart::asIs, SynthPart::asIs }) == smf);
    CHECK (made (smf, { SynthPart::asIs, SynthPart::asIs, SynthPart::bass }) == concat ({ mthd (4), mtrk (conductorBody), mtrk (kickBody), mtrk (bassForSynths), mtrk (leadBody) }));

    // As keepTracks () does: chunks that aren't tracks, and bytes after the last track, are dropped.
    const auto extra = concat ({ mthd (4), mtrk (conductorBody), chunk ("XFIH", { 1, 2, 3 }), mtrk (kickBody), mtrk (bassBody), mtrk (leadBody), Bytes { 0xDE, 0xAD } });
    CHECK (made (extra, {}) == smf);
    CHECK (made (extra, { SynthPart::asIs, SynthPart::asIs, SynthPart::bass, SynthPart::pitched })
           == concat ({ mthd (4), mtrk (conductorBody), mtrk (kickBody), mtrk (bassForSynths), mtrk (leadForSynths) }));
}

TEST_CASE ("forSynths leaves a track it can't carry as it is byte for byte", "[midi][for_synths]")
{
    // Each: a Bass track holding one more event the writer can't lay out as it is.
    const std::vector<std::pair<const char*, Bytes>> cases {
        { "a pitch bend", { 0x00, 0xE0, 0x00, 0x40 } },
        { "a channel pressure", { 0x00, 0xD0, 0x30 } },
        { "a key pressure", { 0x00, 0xA0, 0x1E, 0x30 } },
        { "a sysex", { 0x00, 0xF0, 0x02, 0x7E, 0xF7 } },
        { "a tempo", { 0x00, 0xFF, 0x51, 0x03, 0x06, 0xD5, 0x11 } },
        { "a second name", { 0x00, 0xFF, 0x03, 0x02, 'B', '2' } },
        { "two values of CC74 on one tick", { 0x00, 0xB0, 0x4A, 0x50 } },
    };
    for (const auto& [what, event] : cases)
    {
        INFO (what);
        Bytes body (bassBody.begin(), bassBody.begin() + 8); // its name
        append (body, event);
        body.insert (body.end(), bassBody.begin() + 8, bassBody.end());
        const auto smf = concat ({ mthd (4), mtrk (conductorBody), mtrk (kickBody), mtrk (body), mtrk (leadBody) });
        REQUIRE (midi::readSmf (smf).ok());
        CHECK (made (smf, { SynthPart::asIs, SynthPart::asIs, SynthPart::bass, SynthPart::pitched })
               == concat ({ mthd (4), mtrk (conductorBody), mtrk (kickBody), mtrk (body), mtrk (leadForSynths) }));
    }

    // A name after tick 0 can't be carried either: the writer puts it at 0.
    Bytes late = { 0x00, 0xB0, 0x4A, 0x40, 0x10, 0xFF, 0x03, 0x04, 'B', 'a', 's', 's', 0x9D, 0x70, 0xFF, 0x2F, 0x00 };
    const auto smf = concat ({ mthd (2), mtrk (conductorBody), mtrk (late) });
    CHECK (made (smf, { SynthPart::asIs, SynthPart::bass }) == smf);
}

TEST_CASE ("forSynths refuses a file readSmf refuses, and parts for tracks it doesn't have", "[midi][for_synths]")
{
    const Bytes junk { 'M', 'T', 'h', 'd', 0, 0 };
    const std::vector<SynthPart> one { SynthPart::asIs };
    const auto bad = midi::forSynths (junk, one);
    CHECK_FALSE (bad.ok());
    CHECK (bad.value.empty());
    CHECK_FALSE (bad.error.empty());

    const std::vector<SynthPart> five (5, SynthPart::pitched);
    const auto tooMany = midi::forSynths (fourTracks(), five);
    CHECK_FALSE (tooMany.ok());
    CHECK_THAT (tooMany.error, ContainsSubstring ("5") && ContainsSubstring ("4"));
}

TEST_CASE ("every committed content/midi file comes out for synths: pitched tracks by the rules, the rest byte for byte", "[midi][for_synths]")
{
    const auto files = contentMidiFiles();
    REQUIRE (files.size() > 100);
    std::size_t bassFiles = 0, rewritten = 0;
    for (const auto& path : files)
    {
        INFO (path.string());
        const auto bytes = util::readBinaryFile (path);
        REQUIRE (bytes.ok());
        const auto& smf = bytes.value;
        const auto source = read (smf);
        REQUIRE (source.ppq == ppq);

        std::vector<SynthPart> parts { SynthPart::asIs };
        for (std::size_t i = 1; i < source.tracks.size(); ++i)
            parts.push_back (partOf (midi::trackName (source.tracks[i])));
        const auto out = midi::forSynths (smf, parts);
        INFO (out.error);
        REQUIRE (out.ok());
        const auto file = read (out.value);
        REQUIRE (file.tracks.size() == source.tracks.size());
        CHECK (Bytes (out.value.begin(), out.value.begin() + 14) == Bytes (smf.begin(), smf.begin() + 14));
        const auto inChunks = chunksOf (smf);
        const auto outChunks = chunksOf (out.value);

        for (std::size_t i = 0; i < parts.size(); ++i)
        {
            INFO ("track " << i << " " << midi::trackName (source.tracks[i]));
            if (parts[i] == SynthPart::asIs)
            {
                CHECK (outChunks[i] == inChunks[i]);
                continue;
            }
            const auto& a = file.tracks[i];
            const auto& b = source.tracks[i];
            CHECK (midi::trackName (a) == midi::trackName (b));
            CHECK (midi::controllersOf (a) == midi::controllersOf (b));
            CHECK (midi::programsOf (a) == midi::programsOf (b));
            CHECK (midi::textsOf (a) == midi::textsOf (b));
            CHECK (a.endTick == b.endTick);
            CHECK (offsBeforeOns (a));

            // The rules, note by note: no kit track has two notes on one tick and key, so the notes pair up in order.
            const bool bass = parts[i] == SynthPart::bass;
            const auto in = midi::notesOf (b);
            const auto notes = midi::notesOf (a);
            REQUIRE (notes.size() == in.size());
            std::set<std::int64_t> onsets;
            for (const auto& n : in)
                onsets.insert (n.tick);
            bool changed = false;
            for (std::size_t j = 0; j < in.size(); ++j)
            {
                const auto& src = in[j];
                const auto& n = notes[j];
                INFO ("note " << j << " at " << src.tick << " key " << int { src.key });
                CHECK (n.tick == src.tick);
                CHECK (n.channel == src.channel);
                CHECK (int { n.key } == src.key + (bass ? 12 : 0));
                CHECK (n.velocity == std::max (src.velocity, midi::kSynthVelocityFloor));
                CHECK (n.releaseVelocity == src.releaseVelocity);
                auto end = std::max (src.tick + src.length, src.tick + 240);
                if (const auto next = onsets.upper_bound (src.tick); next != onsets.end())
                {
                    end = std::min (end, *next - 10);
                    CHECK (n.tick + n.length <= *next - 10); // nothing sounds into the next note
                }
                end = std::min (end, b.endTick);
                CHECK (n.length == end - src.tick);
                CHECK (n.length >= 1);
                changed = changed || ! (n == src);
            }
            if (changed)
                ++rewritten;

            // The bass lines land on D1-G2 (38-55), from 26-43.
            if (bass && ! notes.empty())
            {
                ++bassFiles;
                CHECK (std::all_of (notes.begin(), notes.end(), [] (const midi::Note& n) { return n.key >= 38 && n.key <= 55; }));
            }
        }
    }
    CHECK (bassFiles > 70); // 77 bass tracks with notes: the Bassline files, the Loops, the mixes and their regions
    CHECK (rewritten > 250); // 267 of the 354 pitched tracks with notes change
}

// ------------------------------------------------------------------ chords as chords (ADR 0027, amended)

TEST_CASE ("chordFor: a key plays its zone's chord, the key plus each interval", "[midi][for_synths]")
{
    CHECK (keysOf (midi::chordFor (stabs(), 59)) == std::vector<int> { 59, 66, 69, 73, 74 });
    CHECK (keysOf (midi::chordFor (stabs(), 54)) == std::vector<int> { 66, 73, 76, 80, 81 }); // F#m9, an octave up
    CHECK (keysOf (midi::chordFor (stabs(), 58)) == std::vector<int> { 58, 65, 68, 72, 73 }); // the low zone's top key

    // A key no zone holds plays nothing.
    const midi::ChordSample narrow { { 50, 60, { 0, 7 } } };
    CHECK (keysOf (midi::chordFor (narrow, 60)) == std::vector<int> { 60, 67 });
    CHECK (midi::chordFor (narrow, 61).empty());
    CHECK (midi::chordFor (narrow, 49).empty());
    CHECK (midi::chordFor (midi::ChordSample {}, 60).empty());
}

TEST_CASE ("chordFor: lifted whole octaves to start on 56 or more, never past 127", "[midi][for_synths]")
{
    CHECK (midi::kChordFloor == 56);
    const midi::ChordSample fifth { { 0, 127, { 0, 7 } } };
    CHECK (keysOf (midi::chordFor (fifth, 40)) == std::vector<int> { 64, 71 }); // two octaves
    CHECK (keysOf (midi::chordFor (fifth, 55)) == std::vector<int> { 67, 74 });
    CHECK (keysOf (midi::chordFor (fifth, 56)) == std::vector<int> { 56, 63 });
    CHECK (keysOf (midi::chordFor (fifth, 0)) == std::vector<int> { 60, 67 });

    // Never past 127: a tone that passes it even unlifted is left out, and a chord whose next octave would pass it
    // stays where it is.
    const midi::ChordSample octave { { 0, 127, { 0, 7, 12 } } };
    CHECK (keysOf (midi::chordFor (octave, 120)) == std::vector<int> { 120, 127 });
    const midi::ChordSample wide { { 0, 127, { 0, 26, 70 } } };
    CHECK (keysOf (midi::chordFor (wide, 50)) == std::vector<int> { 50, 76, 120 });
    CHECK (keysOf (midi::chordFor (wide, 30)) == std::vector<int> { 54, 80, 124 }); // two octaves would start on 54, three pass 127
}

TEST_CASE ("forSynths: a chords track's note becomes its chord, each tone at the note's tick, channel, length and velocity", "[midi][for_synths]")
{
    const auto smf = chordsFile ({ note (0, 480, 54, 100, chordChannel), note (1920, 480, 59, 100, chordChannel) });
    const auto out = chordsMade (smf, stabs());
    REQUIRE (out.size() == 10);
    CHECK (keys (out) == std::vector<int> { 66, 73, 76, 80, 81, 59, 66, 69, 73, 74 });
    for (std::size_t i = 0; i < out.size(); ++i)
    {
        INFO ("tone " << i);
        CHECK (out[i].tick == (i < 5 ? 0 : 1920));
        CHECK (out[i].length == 480);
        CHECK (out[i].velocity == 100);
        CHECK (int { out[i].channel } == chordChannel);
    }

    // The rest of the file as the pitched rule writes it: the conductor byte for byte, the track's name and end.
    const auto made = madeWithChords (smf, { SynthPart::asIs, SynthPart::chords }, { {}, stabs() });
    CHECK (chunksOf (made)[0] == chunksOf (smf)[0]);
    const auto file = read (made);
    CHECK (midi::trackName (file.tracks[1]) == "Chords");
    CHECK (file.tracks[1].endTick == bar);
    CHECK (offsBeforeOns (file.tracks[1]));
}

TEST_CASE ("forSynths: chord tones follow the pitched rules as their root would", "[midi][for_synths]")
{
    // A 32nd at velocity 40: a 16th at 72, every tone.
    const auto shortSoft = chordsMade (chordsFile ({ note (0, 60, 59, 40, chordChannel) }), stabs());
    CHECK (keys (shortSoft) == std::vector<int> { 59, 66, 69, 73, 74 });
    CHECK (spans (shortSoft) == std::vector<std::pair<std::int64_t, std::int64_t>> (5, { 0, 240 }));
    CHECK (velocities (shortSoft) == std::vector<int> (5, 72));

    // A chord held 2000 ticks before the next at 960: its tones end at 950, 10 ticks before it.
    const std::vector<midi::Note> roots { note (0, 2000, 54, 100, chordChannel), note (960, 480, 59, 90, chordChannel) };
    const auto held = chordsMade (chordsFile (roots), stabs());
    REQUIRE (held.size() == 10);
    for (std::size_t i = 0; i < 5; ++i)
        CHECK (held[i].tick + held[i].length == 950);

    // Each tone as synthNotes () makes its root alone.
    for (const auto& file : { std::vector<midi::Note> { note (0, 60, 59, 40, chordChannel) }, roots })
    {
        const auto alone = midi::synthNotes (file, ppq, bar, false);
        const auto tones = chordsMade (chordsFile (file), stabs());
        REQUIRE (tones.size() == 5 * alone.size());
        for (std::size_t i = 0; i < tones.size(); ++i)
        {
            const auto& root = alone[i / 5];
            INFO ("tone " << i << " key " << int { tones[i].key });
            CHECK (tones[i].tick == root.tick);
            CHECK (tones[i].length == root.length);
            CHECK (tones[i].velocity == root.velocity);
            CHECK (tones[i].releaseVelocity == root.releaseVelocity);
        }
    }
}

TEST_CASE ("forSynths: a note no zone holds stays one note, made for synths as a pitched note", "[midi][for_synths]")
{
    // Keys 50-60 play the chord; 61 is no zone's.
    const midi::ChordSample narrow { { 50, 60, { 0, 7 } } };
    const auto out = chordsMade (chordsFile ({ note (0, 60, 61, 40, chordChannel), note (960, 480, 55, 100, chordChannel) }), narrow);
    CHECK (keys (out) == std::vector<int> { 61, 67, 74 });
    CHECK (spans (out) == std::vector<std::pair<std::int64_t, std::int64_t>> { { 0, 240 }, { 960, 480 }, { 960, 480 } });
    CHECK (velocities (out) == std::vector<int> { 72, 100, 100 });
}

TEST_CASE ("forSynths: a chords track without zones is a pitched track, as before", "[midi][for_synths]")
{
    const auto smf = chordsFile ({ note (0, 60, 54, 40, chordChannel), note (1920, 480, 59, 100, chordChannel) });
    const auto pitched = made (smf, { SynthPart::asIs, SynthPart::pitched });
    CHECK (pitched != smf);
    CHECK (made (smf, { SynthPart::asIs, SynthPart::chords }) == pitched); // no chords at all
    CHECK (madeWithChords (smf, { SynthPart::asIs, SynthPart::chords }, {}) == pitched);
    CHECK (madeWithChords (smf, { SynthPart::asIs, SynthPart::chords }, { stabs() }) == pitched); // chords shorter than parts
    CHECK (madeWithChords (smf, { SynthPart::asIs, SynthPart::chords }, { {}, {} }) == pitched); // an empty sample
    CHECK (madeWithChords (smf, { SynthPart::asIs, SynthPart::chords }, { {}, { { 0, 127, {} } } }) == pitched); // a zone without intervals
    // Chords given for a track that isn't marked chords change nothing.
    CHECK (madeWithChords (smf, { SynthPart::asIs, SynthPart::pitched }, { {}, stabs() }) == pitched);
}

TEST_CASE ("forSynths: chords leave every other track as the two-argument forSynths makes it", "[midi][for_synths]")
{
    // clang-format off
    // Chords on channel 4: F#3 (54) at 0 and B3 (59) at 1920, each 480 ticks at velocity 100.
    const Bytes chordsBody = {
        0x00, 0xFF, 0x03, 0x06, 'C', 'h', 'o', 'r', 'd', 's',
        0x00, 0x93, 0x36, 0x64,
        0x83, 0x60, 0x83, 0x36, 0x40,
        0x8B, 0x20, 0x93, 0x3B, 0x64,
        0x83, 0x60, 0x83, 0x3B, 0x40,
        0x8B, 0x20, 0xFF, 0x2F, 0x00,
    };
    // clang-format on
    const auto smf = concat ({ mthd (4), mtrk (conductorBody), mtrk (kickBody), mtrk (bassBody), mtrk (chordsBody) });
    const auto out = chunksOf (madeWithChords (smf, { SynthPart::asIs, SynthPart::asIs, SynthPart::bass, SynthPart::chords }, { {}, {}, {}, stabs() }));
    const auto plain = chunksOf (made (smf, { SynthPart::asIs, SynthPart::asIs, SynthPart::bass, SynthPart::pitched }));
    REQUIRE (out.size() == 4);
    REQUIRE (plain.size() == 4);
    for (std::size_t i = 0; i < 3; ++i)
    {
        INFO ("track " << i);
        CHECK (out[i] == plain[i]);
    }
    CHECK (plain[2] == mtrk (bassForSynths));
    CHECK (out[3] != plain[3]);
    const auto file = read (concat ({ mthd (4), out[0], out[1], out[2], out[3] }));
    CHECK (keys (midi::notesOf (file.tracks[3])) == std::vector<int> { 66, 73, 76, 80, 81, 59, 66, 69, 73, 74 });
}

TEST_CASE ("forSynths: chords come out the same twice, tones ascending on each tick", "[midi][for_synths]")
{
    const midi::ChordSample laser { { 0, 61, { 0, 7, 11, 14, 16 } }, { 62, 127, { 0, 7, 10, 14, 15 } } };
    const auto smf = chordsFile ({ note (0, 240, 58, 90, chordChannel), note (480, 240, 62, 100, chordChannel), note (960, 120, 40, 70, chordChannel),
        note (1440, 960, 70, 127, chordChannel) });
    const auto once = madeWithChords (smf, { SynthPart::asIs, SynthPart::chords }, { {}, laser });
    const auto twice = madeWithChords (smf, { SynthPart::asIs, SynthPart::chords }, { {}, laser });
    CHECK (once == twice);

    // In file order, each tick's note-ons go up by key: five per chord.
    const auto file = read (once);
    std::vector<std::pair<std::int64_t, int>> ons;
    for (const auto& e : file.tracks[1].events)
        if (e.isNoteOn())
            ons.emplace_back (e.tick, e.data1);
    REQUIRE (ons.size() == 20);
    for (std::size_t i = 1; i < ons.size(); ++i)
        if (ons[i].first == ons[i - 1].first)
            CHECK (ons[i].second > ons[i - 1].second);
    CHECK (keys (midi::notesOf (file.tracks[1]))
           == std::vector<int> { 58, 65, 69, 72, 74, 62, 69, 72, 76, 77, 64, 71, 75, 78, 80, 70, 77, 80, 84, 85 });
}
