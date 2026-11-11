// Tests for core/midi/track_select.h: cutting an SMF down to its conductor and some of its tracks,
// byte for byte, for MIDI drag-out (plugin/source/dragout/DragFiles.h).
//
// The hand-made files are built chunk by chunk here, so each expected cut is spelled out from the
// same chunk bodies. The last tests cut every committed content/midi file.

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
#include <string>
#include <vector>

using namespace wubansia;
using Catch::Matchers::ContainsSubstring;

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
