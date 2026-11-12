// Tests for core/midi/track_select.h: cutting an SMF down to its conductor and some of its tracks,
// byte for byte, and merging some of them into one track, for MIDI drag-out
// (plugin/source/dragout/DragFiles.h).
//
// The hand-made files are built chunk by chunk here, so each expected cut or merge is spelled out
// from the same chunk bodies. The last tests cut and merge every committed content/midi file.

#include "midi/reader.h"
#include "midi/track_select.h"
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
#include <string_view>
#include <tuple>
#include <vector>

using namespace wubansia;
using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::StartsWith;

namespace
{
    using Bytes = std::vector<std::uint8_t>;

    void append (Bytes& out, const Bytes& more) { out.insert (out.end(), more.begin(), more.end()); }

    void appendU32 (Bytes& out, std::size_t v)
    {
        for (int shift = 24; shift >= 0; shift -= 8)
            out.push_back (static_cast<std::uint8_t> ((v >> shift) & 0xFF));
    }

    // A chunk: its 4-character id, its length, then its body.
    Bytes chunk (const char (&id)[5], const Bytes& body)
    {
        Bytes out (id, id + 4);
        appendU32 (out, body.size());
        append (out, body);
        return out;
    }

    Bytes mtrk (const Bytes& body) { return chunk ("MTrk", body); }

    // An MThd chunk: format, track count and ppq, then any extra header bytes (a longer header).
    Bytes mthd (std::uint16_t format, std::uint16_t tracks, std::uint16_t ppq, const Bytes& extra = {})
    {
        Bytes body = { static_cast<std::uint8_t> (format >> 8), static_cast<std::uint8_t> (format & 0xFF),
            static_cast<std::uint8_t> (tracks >> 8), static_cast<std::uint8_t> (tracks & 0xFF),
            static_cast<std::uint8_t> (ppq >> 8), static_cast<std::uint8_t> (ppq & 0xFF) };
        append (body, extra);
        return chunk ("MThd", body);
    }

    Bytes concat (std::initializer_list<Bytes> parts)
    {
        Bytes out;
        for (const auto& p : parts)
            append (out, p);
        return out;
    }

    // clang-format off
    // The conductor: a name, 134 BPM, 4/4, F# minor, a marker; ends at 3840.
    const Bytes conductorBody = {
        0x00, 0xFF, 0x03, 0x04, 'S', 'o', 'n', 'g',
        0x00, 0xFF, 0x51, 0x03, 0x06, 0xD5, 0x11,
        0x00, 0xFF, 0x58, 0x04, 0x04, 0x02, 0x18, 0x08,
        0x00, 0xFF, 0x59, 0x02, 0x03, 0x01,
        0x00, 0xFF, 0x06, 0x05, 'i', 'n', 't', 'r', 'o',
        0x9E, 0x00, 0xFF, 0x2F, 0x00,
    };
    // "Kick" on channel 10, written with running status.
    const Bytes kickBody = {
        0x00, 0xFF, 0x03, 0x04, 'K', 'i', 'c', 'k',
        0x00, 0x99, 0x24, 0x6E,
        0x78, 0x24, 0x00, // running status: note-on velocity 0 ends it
        0x8E, 0x08, 0x24, 0x64,
        0x78, 0x24, 0x00,
        0x8E, 0x08, 0xFF, 0x2F, 0x00,
    };
    // "Bass" on channel 2 with a CC74 ride.
    const Bytes bassBody = {
        0x00, 0xFF, 0x03, 0x04, 'B', 'a', 's', 's',
        0x00, 0xB1, 0x4A, 0x40,
        0x00, 0x91, 0x2A, 0x60,
        0x83, 0x60, 0x81, 0x2A, 0x40,
        0xAA, 0x20, 0xFF, 0x2F, 0x00,
    };
    // A track with no name: channel 5 notes, and bytes after its end-of-track event (readers ignore them).
    const Bytes unnamedBody = {
        0x00, 0x94, 0x3C, 0x50,
        0x83, 0x60, 0x84, 0x3C, 0x40,
        0x00, 0xFF, 0x2F, 0x00,
        0xDE, 0xAD,
    };
    // clang-format on

    // Format 1, 480 PPQ: the conductor, Kick, Bass and the unnamed track.
    Bytes fourTracks()
    {
        return concat ({ mthd (1, 4, 480), mtrk (conductorBody), mtrk (kickBody), mtrk (bassBody), mtrk (unnamedBody) });
    }

    midi::File read (const Bytes& bytes)
    {
        auto r = midi::readSmf (bytes);
        INFO (r.error);
        REQUIRE (r.ok());
        return r.value;
    }

    Bytes keep (const Bytes& smf, std::initializer_list<int> indices)
    {
        const std::vector<int> v (indices);
        auto r = midi::keepTracks (smf, v);
        INFO (r.error);
        REQUIRE (r.ok());
        return r.value;
    }

    std::string keepError (const Bytes& smf, std::initializer_list<int> indices)
    {
        const std::vector<int> v (indices);
        const auto r = midi::keepTracks (smf, v);
        CHECK_FALSE (r.ok());
        CHECK (r.value.empty());
        return r.error;
    }

    std::uint16_t headerTrackCount (const Bytes& smf)
    {
        REQUIRE (smf.size() >= 14);
        return static_cast<std::uint16_t> ((smf[10] << 8) | smf[11]);
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

    // ------------------------------------------------------------------ merges

    // clang-format off
    // Two drum tracks on channel 10 to merge, both ending at 3840 like the conductor. Kick: 36 at 0 and at 240. Clap: a
    // rim (37) at 0, and a clap (38) at 120 that lets go at 240 with release velocity 64. On tick 120 Kick's and the
    // rim's note-offs meet the clap's note-on; on tick 240 the clap's note-off meets Kick's note-on, which file order
    // would put first.
    const Bytes mergeKickBody = {
        0x00, 0xFF, 0x03, 0x04, 'K', 'i', 'c', 'k',
        0x00, 0x99, 0x24, 0x64,
        0x78, 0x89, 0x24, 0x00,
        0x78, 0x99, 0x24, 0x50,
        0x78, 0x89, 0x24, 0x00,
        0x9B, 0x18, 0xFF, 0x2F, 0x00,
    };
    const Bytes mergeClapBody = {
        0x00, 0xFF, 0x03, 0x04, 'C', 'l', 'a', 'p',
        0x00, 0x99, 0x25, 0x46,
        0x78, 0x89, 0x25, 0x00,
        0x00, 0x99, 0x26, 0x5A,
        0x78, 0x89, 0x26, 0x40,
        0x9C, 0x10, 0xFF, 0x2F, 0x00,
    };
    // The two merged as the writer lays a track out: its name, then on each tick the note-offs by key, then the
    // note-ons by key, every status byte written; ending at 3840.
    const Bytes drumsBody = {
        0x00, 0xFF, 0x03, 0x05, 'D', 'r', 'u', 'm', 's',
        0x00, 0x99, 0x24, 0x64,
        0x00, 0x99, 0x25, 0x46,
        0x78, 0x89, 0x24, 0x00,
        0x00, 0x89, 0x25, 0x00,
        0x00, 0x99, 0x26, 0x5A,
        0x78, 0x89, 0x26, 0x40,
        0x00, 0x99, 0x24, 0x50,
        0x78, 0x89, 0x24, 0x00,
        0x9B, 0x18, 0xFF, 0x2F, 0x00,
    };
    // clang-format on

    // A track's body: its name at tick 0, then the events as given (delta times included), then the end of track
    // endDelta ticks after the last of them.
    Bytes body (std::string_view name, const Bytes& events, std::uint8_t endDelta = 0)
    {
        Bytes out = { 0x00, 0xFF, 0x03, static_cast<std::uint8_t> (name.size()) };
        out.insert (out.end(), name.begin(), name.end());
        append (out, events);
        append (out, Bytes { endDelta, 0xFF, 0x2F, 0x00 });
        return out;
    }

    // Format 1, 480 PPQ: the conductor, then a and b (tracks 1 and 2).
    Bytes twoTracks (const Bytes& a, const Bytes& b)
    {
        return concat ({ mthd (1, 3, 480), mtrk (conductorBody), mtrk (a), mtrk (b) });
    }

    Bytes merge (const Bytes& smf, std::initializer_list<int> keepIndices, std::initializer_list<int> mergeIndices)
    {
        const std::vector<int> keepList (keepIndices), mergeList (mergeIndices);
        auto r = midi::mergeTracks (smf, keepList, mergeList, "Drums");
        INFO (r.error);
        REQUIRE (r.ok());
        return r.value;
    }

    std::string mergeError (const Bytes& smf, std::initializer_list<int> keepIndices, std::initializer_list<int> mergeIndices)
    {
        const std::vector<int> keepList (keepIndices), mergeList (mergeIndices);
        const auto r = midi::mergeTracks (smf, keepList, mergeList, "Drums");
        CHECK_FALSE (r.ok());
        CHECK (r.value.empty());
        return r.error;
    }

    // A chunk's bytes in its file.
    Bytes chunkOf (const Bytes& smf, const midi::TrackChunk& c)
    {
        return Bytes (smf.begin() + static_cast<std::ptrdiff_t> (c.offset), smf.begin() + static_cast<std::ptrdiff_t> (c.offset + c.size));
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

    // Lists in one order, so two tracks' events can be compared whatever order they come in.
    std::vector<midi::Note> sorted (std::vector<midi::Note> v)
    {
        std::stable_sort (v.begin(), v.end(), [] (const midi::Note& a, const midi::Note& b) {
            return std::tie (a.tick, a.channel, a.key) < std::tie (b.tick, b.channel, b.key);
        });
        return v;
    }

    std::vector<midi::Controller> sorted (std::vector<midi::Controller> v)
    {
        std::sort (v.begin(), v.end(), [] (const midi::Controller& a, const midi::Controller& b) {
            return std::tie (a.tick, a.channel, a.number, a.value) < std::tie (b.tick, b.channel, b.number, b.value);
        });
        return v;
    }

    std::vector<midi::ProgramChange> sorted (std::vector<midi::ProgramChange> v)
    {
        std::sort (v.begin(), v.end(), [] (const midi::ProgramChange& a, const midi::ProgramChange& b) {
            return std::tie (a.tick, a.channel, a.program) < std::tie (b.tick, b.channel, b.program);
        });
        return v;
    }

    std::vector<midi::TextEvent> sorted (std::vector<midi::TextEvent> v)
    {
        std::sort (v.begin(), v.end(), [] (const midi::TextEvent& a, const midi::TextEvent& b) {
            return std::tie (a.tick, a.type, a.text) < std::tie (b.tick, b.type, b.text);
        });
        return v;
    }

    // A track's text events but its names (FF 03).
    std::vector<midi::TextEvent> textsButNames (const midi::Track& track)
    {
        std::vector<midi::TextEvent> out;
        for (auto& t : midi::textsOf (track))
            if (t.type != midi::meta::trackName)
                out.push_back (std::move (t));
        return out;
    }
}

TEST_CASE ("keepTracks copies the header, the conductor and the chosen tracks byte for byte", "[midi][track_select]")
{
    const auto smf = fourTracks();

    SECTION ("one track")
    {
        CHECK (keep (smf, { 2 }) == concat ({ mthd (1, 2, 480), mtrk (conductorBody), mtrk (bassBody) }));
    }

    SECTION ("two tracks, the last one with bytes after its end of track")
    {
        CHECK (keep (smf, { 1, 3 }) == concat ({ mthd (1, 3, 480), mtrk (conductorBody), mtrk (kickBody), mtrk (unnamedBody) }));
    }

    SECTION ("every track gives the file back")
    {
        CHECK (keep (smf, { 0, 1, 2, 3 }) == smf);
        CHECK (keep (smf, { 1, 2, 3 }) == smf);
    }

    SECTION ("the cut reads as the source's tracks, running status and all")
    {
        const auto source = read (smf);
        const auto cut = read (keep (smf, { 1, 3 }));
        CHECK (cut.format == 1);
        CHECK (cut.ppq == 480);
        REQUIRE (cut.tracks.size() == 3);
        CHECK (cut.tracks[0] == source.tracks[0]);
        CHECK (cut.tracks[1] == source.tracks[1]);
        CHECK (cut.tracks[2] == source.tracks[3]);
        CHECK (midi::notesOf (cut.tracks[1]).size() == 2);
    }
}

TEST_CASE ("keepTracks always keeps the conductor, once, and keeps the file's track order", "[midi][track_select]")
{
    const auto smf = fourTracks();
    const auto expected = concat ({ mthd (1, 3, 480), mtrk (conductorBody), mtrk (kickBody), mtrk (bassBody) });

    CHECK (keep (smf, { 2, 1 }) == expected);
    CHECK (keep (smf, { 2, 0, 1, 2, 1 }) == expected);
    CHECK (headerTrackCount (expected) == 3);

    SECTION ("no tracks chosen leaves the conductor alone")
    {
        const auto alone = keep (smf, {});
        CHECK (alone == concat ({ mthd (1, 1, 480), mtrk (conductorBody) }));
        CHECK (keep (smf, { 0 }) == alone);
        CHECK (read (alone).tracks.size() == 1);
    }
}

TEST_CASE ("keepTracks refuses an index that isn't a track, and a file readSmf refuses", "[midi][track_select]")
{
    const auto smf = fourTracks();
    CHECK_THAT (keepError (smf, { 4 }), ContainsSubstring ("track 4") && ContainsSubstring ("4 tracks"));
    CHECK_THAT (keepError (smf, { 1, -1 }), ContainsSubstring ("track -1"));

    const Bytes notMidi = { 'R', 'I', 'F', 'F', 0, 0, 0, 4, 'W', 'A', 'V', 'E' };
    CHECK_THAT (keepError (notMidi, { 1 }), ContainsSubstring ("not a MIDI file"));

    Bytes truncated = smf;
    truncated.resize (truncated.size() - 20);
    CHECK_THAT (keepError (truncated, { 1 }), ContainsSubstring ("truncated"));

    CHECK_THAT (keepError (mthd (1, 0, 480), {}), ContainsSubstring ("no tracks"));
}

TEST_CASE ("keepTracks keeps a longer header, and drops other chunks and what follows the last track", "[midi][track_select]")
{
    const Bytes extra = { 0xAB, 0xCD };
    const auto smf = concat ({ mthd (1, 3, 960, extra), mtrk (conductorBody), chunk ("XFIH", { 1, 2, 3 }), mtrk (kickBody),
        mtrk (bassBody), mtrk (unnamedBody), Bytes { 0x00, 0x01, 0x02 } });
    REQUIRE (read (smf).tracks.size() == 3); // the header counts 3: the fourth MTrk chunk is past the last track

    CHECK (keep (smf, { 2 }) == concat ({ mthd (1, 2, 960, extra), mtrk (conductorBody), mtrk (bassBody) }));
    CHECK (keep (smf, { 1, 2 }) == concat ({ mthd (1, 3, 960, extra), mtrk (conductorBody), mtrk (kickBody), mtrk (bassBody) }));
    CHECK_THAT (keepError (smf, { 3 }), ContainsSubstring ("3 tracks"));
}

TEST_CASE ("keepTracks takes a format 0 file's one track", "[midi][track_select]")
{
    const auto smf = concat ({ mthd (0, 1, 96), mtrk (kickBody) });
    CHECK (keep (smf, {}) == smf);
    CHECK (keep (smf, { 0 }) == smf);
    CHECK_THAT (keepError (smf, { 1 }), ContainsSubstring ("track 1"));
}

TEST_CASE ("trackChunks gives each track's place, size and name", "[midi][track_select]")
{
    const auto smf = fourTracks();
    const auto r = midi::trackChunks (smf);
    INFO (r.error);
    REQUIRE (r.ok());
    const auto& chunks = r.value;
    REQUIRE (chunks.size() == 4);

    const std::size_t sizes[] = { 8 + conductorBody.size(), 8 + kickBody.size(), 8 + bassBody.size(), 8 + unnamedBody.size() };
    const char* names[] = { "Song", "Kick", "Bass", "" };
    std::size_t at = 14;
    for (std::size_t i = 0; i < 4; ++i)
    {
        CHECK (chunks[i].offset == at);
        CHECK (chunks[i].size == sizes[i]);
        CHECK (chunks[i].name == names[i]);
        at += sizes[i];
    }

    SECTION ("other chunks are skipped, as readSmf skips them")
    {
        const auto withOther = concat ({ mthd (1, 2, 480), chunk ("XFIH", { 9 }), mtrk (conductorBody), mtrk (bassBody) });
        const auto c = midi::trackChunks (withOther);
        REQUIRE (c.ok());
        REQUIRE (c.value.size() == 2);
        CHECK (c.value[0].offset == 14 + 9);
        CHECK (c.value[1].name == "Bass");
    }

    SECTION ("a file readSmf refuses")
    {
        Bytes truncated = smf;
        truncated.resize (30);
        CHECK_FALSE (midi::trackChunks (truncated).ok());
    }
}

TEST_CASE ("every committed content/midi file cuts into its conductor and any one track", "[midi][track_select]")
{
    const auto files = contentMidiFiles();
    REQUIRE (files.size() > 100);

    for (const auto& path : files)
    {
        INFO (path.string());
        const auto bytes = util::readBinaryFile (path);
        REQUIRE (bytes.ok());
        const auto source = read (bytes.value);
        REQUIRE (source.format == 1);
        REQUIRE (!source.tracks.empty());

        const auto chunks = midi::trackChunks (bytes.value);
        REQUIRE (chunks.ok());
        REQUIRE (chunks.value.size() == source.tracks.size());
        for (std::size_t i = 0; i < source.tracks.size(); ++i)
            CHECK (chunks.value[i].name == midi::trackName (source.tracks[i]));

        // Every track at once is the file itself: it has a plain header and no other chunks.
        std::vector<int> all;
        for (std::size_t i = 0; i < source.tracks.size(); ++i)
            all.push_back (static_cast<int> (i));
        const auto whole = midi::keepTracks (bytes.value, all);
        REQUIRE (whole.ok());
        CHECK (whole.value == bytes.value);

        for (std::size_t i = 1; i < source.tracks.size(); ++i)
        {
            INFO ("track " << i);
            const int index[] = { static_cast<int> (i) };
            const auto cut = midi::keepTracks (bytes.value, index);
            REQUIRE (cut.ok());
            CHECK (headerTrackCount (cut.value) == 2);

            // The header, then the two chunks as they are in the source.
            const auto& c0 = chunks.value[0];
            const auto& ci = chunks.value[i];
            Bytes expected (bytes.value.begin(), bytes.value.begin() + 14);
            expected[10] = 0;
            expected[11] = 2;
            expected.insert (expected.end(), bytes.value.begin() + static_cast<std::ptrdiff_t> (c0.offset),
                bytes.value.begin() + static_cast<std::ptrdiff_t> (c0.offset + c0.size));
            expected.insert (expected.end(), bytes.value.begin() + static_cast<std::ptrdiff_t> (ci.offset),
                bytes.value.begin() + static_cast<std::ptrdiff_t> (ci.offset + ci.size));
            CHECK (cut.value == expected);

            const auto file = read (cut.value);
            REQUIRE (file.tracks.size() == 2);
            CHECK (file.ppq == source.ppq);
            CHECK (file.tracks[0] == source.tracks[0]);
            CHECK (file.tracks[1] == source.tracks[i]);
        }
    }
}

TEST_CASE ("mergeTracks writes the merged track in the writer's order: on a tick its note-offs, then its note-ons", "[midi][track_select]")
{
    const auto smf = concat ({ mthd (1, 4, 480), mtrk (conductorBody), mtrk (mergeKickBody), mtrk (mergeClapBody), mtrk (bassBody) });
    const auto merged = merge (smf, { 1, 2, 3 }, { 1, 2 });
    CHECK (merged == concat ({ mthd (1, 3, 480), mtrk (conductorBody), mtrk (drumsBody), mtrk (bassBody) }));

    // The notes are the two tracks' own, ticks, lengths, velocities and release velocities.
    const auto source = read (smf);
    const auto out = read (merged);
    REQUIRE (out.tracks.size() == 3);
    auto notes = midi::notesOf (source.tracks[1]);
    const auto clap = midi::notesOf (source.tracks[2]);
    notes.insert (notes.end(), clap.begin(), clap.end());
    CHECK (midi::notesOf (out.tracks[1]) == sorted (notes));
    CHECK (midi::trackName (out.tracks[1]) == "Drums");
    CHECK (offsBeforeOns (out.tracks[1]));
    CHECK (out.tracks[2] == source.tracks[3]);

    SECTION ("notes on one key from two tracks merge while they don't overlap")
    {
        const auto first = body ("Kick", { 0x00, 0x99, 0x24, 0x64, 0x78, 0x89, 0x24, 0x00 }); // 36 from 0 to 120
        const auto second = body ("Clap", { 0x78, 0x99, 0x24, 0x50, 0x78, 0x89, 0x24, 0x00 }); // 36 from 120 to 240
        const auto expected = body ("Drums", { 0x00, 0x99, 0x24, 0x64, 0x78, 0x89, 0x24, 0x00, 0x00, 0x99, 0x24, 0x50, 0x78, 0x89, 0x24, 0x00 });
        CHECK (merge (twoTracks (first, second), {}, { 1, 2 }) == concat ({ mthd (1, 2, 480), mtrk (conductorBody), mtrk (expected) }));
    }
}

TEST_CASE ("mergeTracks puts the merged track where the first of its tracks was, and copies every other chunk", "[midi][track_select]")
{
    // The conductor, Bass, Kick, the unnamed track (bytes after its end of track), Clap.
    const auto smf = concat ({ mthd (1, 5, 480), mtrk (conductorBody), mtrk (bassBody), mtrk (mergeKickBody), mtrk (unnamedBody), mtrk (mergeClapBody) });

    SECTION ("every track kept")
    {
        CHECK (merge (smf, { 1, 2, 3, 4 }, { 2, 4 })
               == concat ({ mthd (1, 4, 480), mtrk (conductorBody), mtrk (bassBody), mtrk (drumsBody), mtrk (unnamedBody) }));
    }

    SECTION ("some tracks kept")
    {
        CHECK (merge (smf, { 3 }, { 2, 4 }) == concat ({ mthd (1, 3, 480), mtrk (conductorBody), mtrk (drumsBody), mtrk (unnamedBody) }));
        CHECK (merge (smf, { 1 }, { 2, 4 }) == concat ({ mthd (1, 3, 480), mtrk (conductorBody), mtrk (bassBody), mtrk (drumsBody) }));
    }

    SECTION ("a longer header keeps its extra bytes; other chunks and what follows the last track are dropped")
    {
        const Bytes extra = { 0xAB, 0xCD };
        const auto longer = concat ({ mthd (1, 3, 480, extra), mtrk (conductorBody), chunk ("XFIH", { 1, 2, 3 }), mtrk (mergeKickBody),
            mtrk (mergeClapBody), mtrk (bassBody), Bytes { 0x00, 0x01 } });
        CHECK (merge (longer, {}, { 1, 2 }) == concat ({ mthd (1, 2, 480, extra), mtrk (conductorBody), mtrk (drumsBody) }));
    }
}

TEST_CASE ("mergeTracks ends the merged track on the latest end among its tracks", "[midi][track_select]")
{
    // Clap ends at 4000, past Kick and the conductor (3840).
    auto lateClap = mergeClapBody;
    lateClap.resize (lateClap.size() - 5);
    append (lateClap, Bytes { 0x9D, 0x30, 0xFF, 0x2F, 0x00 }); // 4000 - 240
    auto lateDrums = drumsBody;
    lateDrums.resize (lateDrums.size() - 5);
    append (lateDrums, Bytes { 0x9C, 0x38, 0xFF, 0x2F, 0x00 }); // 4000 - 360

    const auto merged = merge (twoTracks (mergeKickBody, lateClap), {}, { 1, 2 });
    CHECK (merged == concat ({ mthd (1, 2, 480), mtrk (conductorBody), mtrk (lateDrums) }));
    const auto out = read (merged);
    CHECK (out.tracks[0].endTick == 3840);
    CHECK (out.tracks[1].endTick == 4000);
}

TEST_CASE ("mergeTracks takes its tracks in any order, and counts them as kept", "[midi][track_select]")
{
    const auto smf = concat ({ mthd (1, 4, 480), mtrk (conductorBody), mtrk (mergeKickBody), mtrk (mergeClapBody), mtrk (bassBody) });
    const auto expected = concat ({ mthd (1, 3, 480), mtrk (conductorBody), mtrk (drumsBody), mtrk (bassBody) });
    CHECK (merge (smf, { 1, 2, 3 }, { 2, 1 }) == expected);
    CHECK (merge (smf, { 3, 0, 3 }, { 2, 1, 2, 1 }) == expected);
    CHECK (merge (smf, {}, { 2, 1 }) == concat ({ mthd (1, 2, 480), mtrk (conductorBody), mtrk (drumsBody) }));
}

TEST_CASE ("mergeTracks reads running status, and writes a note-on with velocity 0 as a note-off", "[midi][track_select]")
{
    // kickBody: running status throughout, its notes ended by note-ons with velocity 0.
    const auto merged = merge (twoTracks (kickBody, mergeClapBody), {}, { 1, 2 });
    // clang-format off
    const Bytes expected = {
        0x00, 0xFF, 0x03, 0x05, 'D', 'r', 'u', 'm', 's',
        0x00, 0x99, 0x24, 0x6E,
        0x00, 0x99, 0x25, 0x46,
        0x78, 0x89, 0x24, 0x00,
        0x00, 0x89, 0x25, 0x00,
        0x00, 0x99, 0x26, 0x5A,
        0x78, 0x89, 0x26, 0x40,
        0x8D, 0x10, 0x99, 0x24, 0x64, // 1920
        0x78, 0x89, 0x24, 0x00,
        0x8E, 0x08, 0xFF, 0x2F, 0x00, // 3840
    };
    // clang-format on
    CHECK (merged == concat ({ mthd (1, 2, 480), mtrk (conductorBody), mtrk (expected) }));
}

TEST_CASE ("mergeTracks carries controllers, program changes and text events, and drops each track's name", "[midi][track_select]")
{
    // Kick: a text event and a note at 0. Perc: CC 7, a program change, a second name at 120, CC 7 again, a note.
    const auto kick = body ("Kick", { 0x00, 0xFF, 0x01, 0x03, 'h', 'i', 't', 0x00, 0x99, 0x24, 0x64, 0x78, 0x89, 0x24, 0x00 });
    const auto perc = body ("Perc", { 0x00, 0xB9, 0x07, 0x64, 0x00, 0xC9, 0x05, 0x78, 0xFF, 0x03, 0x01, 'x', 0x00, 0xB9, 0x07, 0x50,
                                        0x00, 0x99, 0x26, 0x5A, 0x78, 0x89, 0x26, 0x00 });
    // On a tick: the name, text events, note-offs, program changes, controllers, note-ons.
    const auto expected = body ("Drums", { 0x00, 0xFF, 0x01, 0x03, 'h', 'i', 't', 0x00, 0xC9, 0x05, 0x00, 0xB9, 0x07, 0x64,
                                             0x00, 0x99, 0x24, 0x64, 0x78, 0x89, 0x24, 0x00, 0x00, 0xB9, 0x07, 0x50, 0x00, 0x99, 0x26, 0x5A,
                                             0x78, 0x89, 0x26, 0x00 });
    const auto merged = merge (twoTracks (kick, perc), {}, { 1, 2 });
    CHECK (merged == concat ({ mthd (1, 2, 480), mtrk (conductorBody), mtrk (expected) }));

    const auto out = read (merged);
    CHECK (midi::controllersOf (out.tracks[1]) == std::vector<midi::Controller> { { 0, 9, 7, 100 }, { 120, 9, 7, 80 } });
    CHECK (midi::programsOf (out.tracks[1]) == std::vector<midi::ProgramChange> { { 0, 9, 5 } });
    CHECK (textsButNames (out.tracks[1]) == std::vector<midi::TextEvent> { { 0, midi::meta::text, "hit" } });
}

TEST_CASE ("mergeTracks refuses what it can't carry as it is, saying why", "[midi][track_select]")
{
    const auto kick = body ("Kick", { 0x00, 0x99, 0x24, 0x64, 0x78, 0x89, 0x24, 0x00 });
    const auto clapWith = [] (const Bytes& events) { return body ("Clap", events); };

    SECTION ("overlapping notes on one key, from two tracks")
    {
        const auto late = body ("Clap", { 0x3C, 0x99, 0x24, 0x50, 0x78, 0x89, 0x24, 0x00 }); // 36 from 60, while Kick's still sounds
        CHECK_THAT (mergeError (twoTracks (kick, late), {}, { 1, 2 }),
            StartsWith ("can't merge tracks 1, 2: ") && ContainsSubstring ("notes on key 36, channel 9 overlap: one at tick 0 (length 120) and one at tick 60"));
    }

    SECTION ("two values of one controller on one channel and tick, even equal ones")
    {
        const auto a = body ("Kick", { 0x00, 0xB9, 0x07, 0x64, 0x00, 0x99, 0x24, 0x64, 0x78, 0x89, 0x24, 0x00 });
        const auto b = body ("Clap", { 0x00, 0xB9, 0x07, 0x64, 0x00, 0x99, 0x26, 0x64, 0x78, 0x89, 0x26, 0x00 });
        CHECK_THAT (mergeError (twoTracks (a, b), {}, { 1, 2 }),
            StartsWith ("can't merge tracks 1, 2: ") && ContainsSubstring ("two values of controller 7 on channel 9 at tick 0"));
    }

    SECTION ("two program changes on one channel and tick")
    {
        const auto a = body ("Kick", { 0x00, 0xC9, 0x05, 0x00, 0x99, 0x24, 0x64, 0x78, 0x89, 0x24, 0x00 });
        const auto b = body ("Clap", { 0x00, 0xC9, 0x06 });
        CHECK_THAT (mergeError (twoTracks (a, b), {}, { 1, 2 }), ContainsSubstring ("two program changes on channel 9 at tick 0"));
    }

    SECTION ("the same text twice on a tick")
    {
        const auto a = body ("Kick", { 0x00, 0xFF, 0x01, 0x01, 'x' });
        const auto b = body ("Clap", { 0x00, 0xFF, 0x01, 0x01, 'x' });
        CHECK_THAT (mergeError (twoTracks (a, b), {}, { 1, 2 }), ContainsSubstring ("the same text event twice at tick 0"));
    }

    SECTION ("pitch bend, key or channel pressure, sysex, and meta events that aren't text")
    {
        const std::vector<std::pair<Bytes, std::string>> refused {
            { { 0x00, 0xE9, 0x00, 0x40 }, "track 2 \"Clap\" has a pitch bend at tick 0" },
            { { 0x78, 0xA9, 0x24, 0x40 }, "track 2 \"Clap\" has a key pressure at tick 120" },
            { { 0x00, 0xD9, 0x40 }, "track 2 \"Clap\" has a channel pressure at tick 0" },
            { { 0x00, 0xF0, 0x03, 0x7E, 0x7F, 0xF7 }, "track 2 \"Clap\" has a sysex event at tick 0" },
            { { 0x00, 0xFF, 0x51, 0x03, 0x07, 0xA1, 0x20 }, "track 2 \"Clap\" has a meta event of type 0x51 at tick 0" },
            { { 0x00, 0xFF, 0x59, 0x02, 0x00, 0x00 }, "track 2 \"Clap\" has a meta event of type 0x59 at tick 0" },
            { { 0x00, 0xFF, 0x7F, 0x01, 0x00 }, "track 2 \"Clap\" has a meta event of type 0x7F at tick 0" },
        };
        for (const auto& [events, message] : refused)
        {
            INFO (message);
            CHECK_THAT (mergeError (twoTracks (kick, clapWith (events)), {}, { 1, 2 }), StartsWith ("can't merge tracks 1, 2: " + message));
        }
    }

    SECTION ("a note-on with no note-off, a note-off with nothing to end, a note-off on its note-on's tick")
    {
        const auto unended = body ("Kick", { 0x00, 0x99, 0x24, 0x64 }, 0x78);
        CHECK (mergeError (twoTracks (unended, clapWith ({})), {}, { 1, 2 })
               == "can't merge tracks 1, 2: track 1 \"Kick\": the note-on at tick 0 (key 36, channel 9) has no note-off before the track ends");

        CHECK (mergeError (twoTracks (kick, clapWith ({ 0x00, 0x89, 0x26, 0x00 })), {}, { 1, 2 })
               == "can't merge tracks 1, 2: track 2 \"Clap\": the note-off at tick 0 (key 38, channel 9) has no note to end");
        CHECK_THAT (mergeError (twoTracks (kick, clapWith ({ 0x78, 0x99, 0x26, 0x00 })), {}, { 1, 2 }), ContainsSubstring ("has no note to end"));

        const auto zero = body ("Kick", { 0x00, 0x99, 0x24, 0x64, 0x00, 0x89, 0x24, 0x00 });
        CHECK_THAT (mergeError (twoTracks (zero, clapWith ({})), {}, { 1, 2 }),
            StartsWith ("can't merge tracks 1, 2: ") && ContainsSubstring ("(key 36, channel 9, tick 0) has length 0"));
    }

    SECTION ("fewer than two tracks to merge, the conductor, an index that isn't a track")
    {
        const auto smf = twoTracks (kick, clapWith ({}));
        CHECK (mergeError (smf, {}, { 1 }) == "can't merge tracks 1: it takes two tracks or more");
        CHECK (mergeError (smf, { 2 }, { 2, 2 }) == "can't merge tracks 2: it takes two tracks or more");
        CHECK (mergeError (smf, { 1, 2 }, {}) == "can't merge tracks: it takes two tracks or more");
        CHECK (mergeError (smf, {}, { 0, 1 }) == "can't merge tracks 0, 1: track 0, the conductor, is never merged");
        CHECK (mergeError (smf, {}, { 1, 3 }) == "can't merge tracks 1, 3: track 3 isn't in the file (3 tracks)");
        CHECK (mergeError (smf, {}, { -1, 1 }) == "can't merge tracks -1, 1: track -1 isn't in the file (3 tracks)");
        CHECK (mergeError (smf, { 5 }, { 1, 2 }) == "can't merge tracks 1, 2: track 5 isn't in the file (3 tracks)");
    }

    SECTION ("a file readSmf refuses")
    {
        const Bytes notMidi = { 'R', 'I', 'F', 'F', 0, 0, 0, 4, 'W', 'A', 'V', 'E' };
        CHECK_THAT (mergeError (notMidi, {}, { 1, 2 }), StartsWith ("can't merge tracks 1, 2: not a MIDI file"));
        auto truncated = twoTracks (kick, clapWith ({}));
        truncated.resize (truncated.size() - 6);
        CHECK_THAT (mergeError (truncated, {}, { 1, 2 }), StartsWith ("can't merge tracks 1, 2: ") && ContainsSubstring ("truncated"));
        CHECK (mergeError (mthd (1, 0, 480), {}, { 1, 2 }) == "can't merge tracks 1, 2: the file has no tracks, so no conductor to keep");
    }
}

TEST_CASE ("every committed content/midi file's drum tracks merge into one track, the rest byte for byte", "[midi][track_select]")
{
    // The drag's drum tracks (plugin/source/dragout/DragFiles.cpp): the names with the drums role in
    // engine/kit/channel_map.h's kTracks.
    const std::set<std::string> drumNames { "Kick", "Rumble", "Snare", "Clap", "Snare/Clap", "Hats", "Perc" };

    const auto files = contentMidiFiles();
    REQUIRE (files.size() > 100);
    std::size_t mergedFiles = 0;
    for (const auto& path : files)
    {
        INFO (path.string());
        const auto bytes = util::readBinaryFile (path);
        REQUIRE (bytes.ok());
        const auto& smf = bytes.value;
        const auto source = read (smf);

        std::vector<int> all, drums;
        for (std::size_t i = 1; i < source.tracks.size(); ++i)
        {
            all.push_back (static_cast<int> (i));
            if (drumNames.count (midi::trackName (source.tracks[i])) != 0)
                drums.push_back (static_cast<int> (i));
        }
        if (drums.size() < 2)
            continue;
        ++mergedFiles;

        const auto merged = midi::mergeTracks (smf, all, drums, "Drums");
        INFO (merged.error);
        REQUIRE (merged.ok());
        const auto out = read (merged.value);
        CHECK (out.format == source.format);
        CHECK (out.ppq == source.ppq);
        REQUIRE (out.tracks.size() == source.tracks.size() - drums.size() + 1);

        // What the drum tracks hold between them.
        std::vector<midi::Note> notes;
        std::vector<midi::Controller> controllers;
        std::vector<midi::ProgramChange> programs;
        std::vector<midi::TextEvent> texts;
        std::int64_t end = 0;
        for (const int d : drums)
        {
            const auto& t = source.tracks[static_cast<std::size_t> (d)];
            const auto n = midi::notesOf (t);
            const auto c = midi::controllersOf (t);
            const auto p = midi::programsOf (t);
            const auto x = textsButNames (t);
            notes.insert (notes.end(), n.begin(), n.end());
            controllers.insert (controllers.end(), c.begin(), c.end());
            programs.insert (programs.end(), p.begin(), p.end());
            texts.insert (texts.end(), x.begin(), x.end());
            end = std::max (end, t.endTick);
        }

        // The merged track, where the first drum track was.
        const auto& m = out.tracks[static_cast<std::size_t> (drums.front())];
        CHECK (midi::trackName (m) == "Drums");
        CHECK (midi::notesOf (m) == sorted (notes));
        CHECK (sorted (midi::controllersOf (m)) == sorted (controllers));
        CHECK (sorted (midi::programsOf (m)) == sorted (programs));
        CHECK (sorted (textsButNames (m)) == sorted (texts));
        bool channel10 = true;
        for (const auto& e : m.events)
            channel10 = channel10 && (!e.isChannel() || e.channel() == 9);
        CHECK (channel10);
        CHECK (offsBeforeOns (m));
        CHECK (m.endTick == end);

        // The header but its count, then the conductor and every other chunk as they are, in file order.
        Bytes header (merged.value.begin(), merged.value.begin() + 14);
        CHECK (static_cast<std::size_t> (headerTrackCount (header)) == out.tracks.size());
        header[10] = smf[10];
        header[11] = smf[11];
        CHECK (header == Bytes (smf.begin(), smf.begin() + 14));
        const auto in = midi::trackChunks (smf);
        const auto outChunks = midi::trackChunks (merged.value);
        REQUIRE (in.ok());
        REQUIRE (outChunks.ok());
        std::size_t o = 0;
        for (std::size_t i = 0; i < in.value.size(); ++i)
        {
            const bool drum = std::find (drums.begin(), drums.end(), static_cast<int> (i)) != drums.end();
            if (drum && static_cast<int> (i) != drums.front())
                continue;
            REQUIRE (o < outChunks.value.size());
            if (!drum)
                CHECK (chunkOf (merged.value, outChunks.value[o]) == chunkOf (smf, in.value[i]));
            ++o;
        }
        CHECK (o == outChunks.value.size());
    }
    CHECK (mergedFiles >= 16); // the 16 Loop and Song files at least
}
