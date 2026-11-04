// Tests for core/metrics/model.h: reading files into sections, lanes and drum groups, as
// tools/legacy_audit/README.md ("Reading the files", "The model") defines it. The answers come from
// tools/legacy_audit/fixtures/expected.json, worked out by hand in make_fixtures.js and checked
// against the Node audit by its own tests, so the C++ and the audit are held to the same answers.

#include "metrics/model.h"
#include "metrics/value.h"
#include "midi/reader.h"
#include "midi/writer.h"
#include "util/file_io.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

using namespace wubansia;
using namespace wubansia::metrics;
using Catch::Matchers::ContainsSubstring;

namespace
{
    const std::filesystem::path auditDir = std::filesystem::path (WUBANSIA_SOURCE_DIR) / "tools" / "legacy_audit";
    const std::filesystem::path fixtureDir = auditDir / "fixtures";

    const Value& fixtureAnswers()
    {
        static const Value answers = [] {
            auto r = readJsonFile (fixtureDir / "expected.json");
            INFO (r.error);
            REQUIRE (r.ok());
            return r.value;
        }();
        return answers;
    }

    Song fixture (const std::string& name)
    {
        auto r = loadSongFile (fixtureDir / name, "fixtures/" + name);
        INFO (r.error);
        REQUIRE (r.ok());
        return r.value;
    }
}

TEST_CASE ("section kinds, lane roles and drum groups follow the audit's tables", "[metrics][model]")
{
    const Value& names = fixtureAnswers()["names"];
    REQUIRE (names["section_kinds"].size() == 10);
    for (const Value& c : names["section_kinds"].items())
    {
        INFO (c["text"].string());
        CHECK (sectionKind (c["text"].string()) == c["kind"].string());
    }
    REQUIRE (names["roles"].size() == 16);
    for (const Value& c : names["roles"].items())
    {
        INFO (c["name"].string() << " slot " << c["slot"].string());
        CHECK (roleName (roleOf (c["name"].string(), c.num ("channel9") == 1, c["slot"].string())) == c["role"].string());
    }
    REQUIRE (names["drum_groups"].size() == 16);
    for (const Value& c : names["drum_groups"].items())
    {
        INFO (c.num ("key"));
        CHECK (groupName (drumGroup (static_cast<int> (c.num ("key")))) == c["group"].string());
    }
}

TEST_CASE ("section kinds: the first word for generator v2, the whole name for legacy", "[metrics][model]")
{
    CHECK (sectionKind ("drop2") == "drop2");
    CHECK (sectionKind ("drop2 four to the floor") == "drop2");
    CHECK (sectionKind ("intro 01 (32 bars)") == "intro");
    CHECK (sectionKind ("outro") == "outro");
    CHECK (sectionKind ("groove") == "groove");
    CHECK (sectionKind ("build") == "build");
    CHECK (sectionKind ("Drop") == "drop"); // legacy: the table
    CHECK (sectionKind ("Breakdown (organ)") == "break");
    CHECK (sectionKind ("4x4 switch") == "switch");
    CHECK (sectionKind ("Drop 2 (big bass)") == "drop");
    CHECK (sectionKind ("drops") == "unknown");
    CHECK (sectionKind ("Intro") == "intro");
    CHECK (sectionKind ("intro\tdj") == "unknown"); // only a space ends the first word
    CHECK (sectionKind ("01 drop (32 bars)") == "unknown"); // the kind word must come first
    CHECK (sectionKind ("") == "unknown");
}

TEST_CASE ("track roles by first word, slot and channel", "[metrics][model]")
{
    CHECK (firstWord ("Riser (builds)") == "riser");
    CHECK (firstWord ("REESE bass") == "reese");
    CHECK (firstWord ("808") == "");
    CHECK (firstWord ("") == "");
    CHECK (roleOf ("Sub", false, "") == Role::bass);
    CHECK (roleOf ("Acid line", false, "") == Role::bass);
    CHECK (roleOf ("Organ stabs", false, "") == Role::chords);
    CHECK (roleOf ("Arp", false, "") == Role::lead);
    CHECK (roleOf ("Chops", false, "") == Role::vox);
    CHECK (roleOf ("Impacts", false, "") == Role::fx);
    CHECK (roleOf ("Kick", true, "") == Role::drums);
    CHECK (roleOf ("Perc", false, "") == Role::other); // a perc lane must be on channel 10
    CHECK (roleOf ("sidechain", true, "") == Role::ignore);
    CHECK (roleOf ("Constructor", false, "") == Role::other); // no JavaScript prototype names
    CHECK (roleOf ("Kit name", false, "Chords-B") == Role::chords);
    CHECK (roleOf ("Kit name", false, "Melody-A") == Role::lead);
    CHECK (roleOf ("Kit name", false, "Drums-Main") == Role::drums);
    CHECK (roleOf ("Lead", false, "FullSong") == Role::lead); // the full files use their track names
    CHECK (roleOf ("Lead", false, "ExtendedMix") == Role::lead);
    CHECK (roleOf ("Kit name", false, "FullArrangement") == Role::other);
}

TEST_CASE ("legacy baseline paths give the kit and the slot", "[metrics][model]")
{
    std::string kit, slot;
    CHECK (splitLegacyPath ("UKG01/UKG01_Bass-A.mid", kit, slot));
    CHECK (kit == "UKG01");
    CHECK (slot == "Bass-A");
    CHECK (splitLegacyPath ("MTH08/MTH08_FullSong.mid", kit, slot));
    CHECK (slot == "FullSong");
    CHECK (splitLegacyPath ("CH03/CH03_Drums-Fill.mid", kit, slot));
    CHECK (slot == "Drums-Fill");
    for (const char* other : { "UKG01/UKG02_Bass-A.mid", "fixtures/harmony_a.mid", "ukg01/ukg01_Bass-A.mid", "UKG01/UKG01_.mid", "x/UKG01/UKG01_FullSong.mid", "UKG01/UKG01_FullSong.midi", "UKG/UKG_FullSong.mid", "01/01_FullSong.mid", "UKG01_FullSong.mid" })
    {
        INFO (other);
        CHECK_FALSE (splitLegacyPath (other, kit, slot));
        CHECK (kit.empty());
        CHECK (slot.empty());
    }
}

TEST_CASE ("the legacy marker table is legacy_sections.json and legacy_expected.json's copy of it", "[metrics][model]")
{
    const auto table = readJsonFile (auditDir / "legacy_sections.json");
    REQUIRE (table.ok());
    const Value& kinds = table.value["kinds"];
    REQUIRE (kinds.isObject());
    const auto& mine = legacySections();
    REQUIRE (mine.size() == kinds.size());
    REQUIRE (mine.size() == 105);
    for (std::size_t i = 0; i < mine.size(); ++i)
    {
        INFO (mine[i].text);
        CHECK (mine[i].text == kinds.members()[i].first); // same names, same (byte) order
        CHECK (mine[i].kind == kinds.members()[i].second.string());
        CHECK (legacySectionKind (mine[i].text) == mine[i].kind);
        if (i > 0)
            CHECK (mine[i - 1].text < mine[i].text);
    }
    CHECK (legacySectionKind ("Not a section") == "");

    const auto expected = readJsonFile (std::filesystem::path (WUBANSIA_SOURCE_DIR) / "core" / "tests" / "legacy_expected.json");
    REQUIRE (expected.ok());
    CHECK (compare (expected.value["section_kinds"], kinds).ok());
}

TEST_CASE ("the reader gives the audit's answers on reader.mid", "[metrics][model]")
{
    // The same rules as tools/legacy_audit/smf.js, held to the same hand-worked answers.
    const Value& e = fixtureAnswers()["reader"];
    const auto bytes = util::readBinaryFile (fixtureDir / "reader.mid");
    REQUIRE (bytes.ok());
    const auto file = midi::readSmf (bytes.value);
    REQUIRE (file.ok());
    CHECK (file.value.format == e.num ("format"));
    CHECK (file.value.ppq == e.num ("ppq"));
    REQUIRE (static_cast<std::int64_t> (file.value.tracks.size()) == e.num ("tracks"));
    for (std::size_t i = 0; i < file.value.tracks.size(); ++i)
        CHECK (file.value.tracks[i].endTick == e["track_end_ticks"][i].integer());
    CHECK (midi::trackName (file.value.tracks[1]) == e["track1_name"].string());

    const auto notes = midi::notesOf (file.value.tracks[1]);
    REQUIRE (notes.size() == e["notes"].size());
    for (std::size_t i = 0; i < notes.size(); ++i)
    {
        const Value& n = e["notes"][i];
        INFO ("note " << i);
        CHECK (notes[i].tick == n.num ("tick"));
        CHECK (notes[i].length == n.num ("length"));
        CHECK (notes[i].channel == n.num ("channel"));
        CHECK (notes[i].key == n.num ("key"));
        CHECK (notes[i].velocity == n.num ("velocity"));
    }
    const auto markers = midi::markersOf (file.value);
    REQUIRE (markers.size() == e["markers"].size());
    for (std::size_t i = 0; i < markers.size(); ++i)
    {
        CHECK (markers[i].tick == e["markers"][i].num ("tick"));
        CHECK (markers[i].text == e["markers"][i]["text"].string());
    }
    const auto keys = midi::keySignaturesOf (file.value);
    REQUIRE (keys.size() == 1);
    CHECK (keys[0].tick == e["key_signatures"][0].num ("tick"));
    CHECK (keys[0].sharps == e["key_signatures"][0].num ("sharps"));
    CHECK ((keys[0].minor ? 1 : 0) == e["key_signatures"][0].num ("minor"));
}

TEST_CASE ("loadSong builds sections, lanes and drum notes", "[metrics][model]")
{
    const Song s = fixture ("reader.mid");
    CHECK (s.path == "fixtures/reader.mid");
    CHECK (s.kit.empty());
    CHECK (s.slot.empty());
    CHECK (s.format == 1);
    CHECK (s.endTick == 3840);
    CHECK (s.bars == 2);
    REQUIRE (s.sections.size() == 2);
    CHECK (s.sections[0].tick == 0);
    CHECK (s.sections[0].end == 1920);
    CHECK (s.sections[0].kind == "drop");
    CHECK (s.sections[0].bar == 0);
    CHECK (s.sections[0].bars == 1);
    CHECK (s.sections[1].kind == "break");
    CHECK (s.sections[1].bar == 1);
    CHECK (s.sections[1].end == 3840);
    REQUIRE (s.tracks.size() == 1); // the conductor has no notes
    CHECK (s.tracks[0].index == 1);
    CHECK (s.tracks[0].name == "Bass");
    CHECK (s.tracks[0].role == Role::bass);
    CHECK (s.tracks[0].notes.size() == 5);
    CHECK (s.tracks[0].notes[4].track == 1);
    CHECK (s.drumNotes.empty());
    CHECK (s.noteCount() == 5);
    REQUIRE (s.keySignatures.size() == 1);
    CHECK (s.keySignatures[0].sharps == -3);

    // A file without markers is one section of kind "none" over all its bars.
    const Song lead = fixture ("lead.mid");
    CHECK (lead.sections.empty());
    const auto secs = sectionsOf (lead);
    REQUIRE (secs.size() == 1);
    CHECK (secs[0].kind == "none");
    CHECK (secs[0].tick == 0);
    CHECK (secs[0].bar == 0);
    CHECK (secs[0].bars == lead.bars);
    CHECK (secs[0].end == lead.bars * barTicks);
}

TEST_CASE ("loadSong drops empty sections and sorts drum notes by tick, track and key", "[metrics][model]")
{
    // Markers on one tick: the first is dropped. A marker on the last tick has no section.
    midi::SongSpec spec;
    spec.endTick = 2 * 1920 + 960;
    spec.markers = { { 0, "intro" }, { 0, "drop" }, { 1920, "Breakdown" }, { 2 * 1920 + 960, "outro" } };
    midi::TrackSpec hats { "Hats", { { 0, 60, 9, 42, 90 }, { 480, 60, 9, 42, 91 } }, {}, {}, {} };
    midi::TrackSpec kick { "Kick", { { 0, 60, 9, 36, 100 }, { 480, 60, 9, 36, 101 } }, {}, {}, {} };
    midi::TrackSpec perc { "Perc", { { 0, 60, 9, 38, 80 }, { 0, 60, 9, 37, 81 } }, {}, {}, {} };
    midi::TrackSpec ghost { "Sidechain trigger", { { 0, 60, 9, 35, 1 } }, {}, {}, {} };
    midi::TrackSpec mixed { "Bass", { { 0, 60, 9, 36, 100 }, { 960, 60, 1, 40, 100 } }, {}, {}, {} };
    spec.tracks = { hats, kick, perc, ghost, mixed };
    const auto bytes = midi::writeSmf (spec);
    REQUIRE (bytes.ok());
    const auto r = loadSong (bytes.value, "built.mid");
    INFO (r.error);
    REQUIRE (r.ok());
    const Song& s = r.value;

    // The writer puts the two tick-0 markers in its own fixed order; the second in the file is kept.
    const auto file = midi::readSmf (bytes.value);
    REQUIRE (file.ok());
    const auto markers = midi::markersOf (file.value);
    REQUIRE (markers.size() == 4);
    REQUIRE (markers[1].tick == 0);
    REQUIRE (s.sections.size() == 2);
    CHECK (s.sections[0].text == markers[1].text);
    CHECK (s.sections[0].kind == sectionKind (markers[1].text));
    CHECK (s.sections[0].tick == 0);
    CHECK (s.sections[0].bars == 1);
    CHECK (s.sections[1].kind == "break");
    CHECK (s.sections[1].bars == 2); // 1.5 bars, rounded up
    CHECK (s.bars == 3);

    REQUIRE (s.tracks.size() == 5);
    CHECK (s.tracks[0].role == Role::drums);
    CHECK (s.tracks[3].role == Role::ignore);
    CHECK (s.tracks[4].role == Role::bass); // one note off channel 10: not a drum track

    // by (tick, track, key); the ignored and the bass track's notes aren't drum notes
    REQUIRE (s.drumNotes.size() == 6);
    const std::vector<std::pair<std::int64_t, int>> order = { { 0, 42 }, { 0, 36 }, { 0, 37 }, { 0, 38 }, { 480, 42 }, { 480, 36 } };
    for (std::size_t i = 0; i < order.size(); ++i)
    {
        INFO ("drum note " << i);
        CHECK (s.drumNotes[i].tick == order[i].first);
        CHECK (s.drumNotes[i].key == order[i].second);
    }
    CHECK (s.drumNotes[0].group == Group::hats);
    CHECK (s.drumNotes[1].group == Group::kick);
    CHECK (s.drumNotes[2].group == Group::rim);
    CHECK (s.drumNotes[3].group == Group::snare);
    CHECK (drumNotesIn (s, percGroups).size() == 1);
    CHECK (drumNotesIn (s, snareFamily).size() == 1);
    CHECK (drumNotesIn (s, Group::kick).size() == 2);
    CHECK (notesWithRole (s, Role::bass).size() == 2);
    CHECK (tracksWithRole (s, Role::drums).size() == 3);
    CHECK (firstSectionOf (s, "break") == &s.sections[1]);
    CHECK (firstSectionOf (s, "build") == nullptr);
}

TEST_CASE ("loadSong reads 480 ticks per quarter note only and names the file in errors", "[metrics][model]")
{
    auto bytes = util::readBinaryFile (fixtureDir / "lead.mid");
    REQUIRE (bytes.ok());
    bytes.value[12] = 0x03; // 960 ticks per quarter note
    bytes.value[13] = 0xC0;
    const auto r = loadSong (bytes.value, "x.mid");
    REQUIRE_FALSE (r.ok());
    CHECK_THAT (r.error, ContainsSubstring ("x.mid"));
    CHECK_THAT (r.error, ContainsSubstring ("480"));

    const std::vector<std::uint8_t> junk = { 'R', 'I', 'F', 'F', 0, 0, 0, 6 };
    const auto j = loadSong (junk, "junk.mid");
    REQUIRE_FALSE (j.ok());
    CHECK_THAT (j.error, ContainsSubstring ("junk.mid"));

    const auto missing = loadSongFile (fixtureDir / "no-such-file.mid", "no-such-file.mid");
    CHECK_FALSE (missing.ok());

    std::string kit, slot;
    REQUIRE (splitLegacyPath ("UKG01/UKG01_Bass-A.mid", kit, slot));
    const auto part = loadSongFile (fixtureDir / "lead.mid", "UKG01/UKG01_Bass-A.mid");
    REQUIRE (part.ok());
    CHECK (part.value.kit == "UKG01");
    CHECK (part.value.slot == "Bass-A");
}

TEST_CASE ("step attribution: the nearest 16th, a tie to the later step", "[metrics][model]")
{
    CHECK (step16 (59) == 0);
    CHECK (step16 (60) == 1);
    CHECK (step16 (179) == 1);
    CHECK (step16 (180) == 2);
    CHECK (step16 (1859) == 15);
    CHECK (step16 (1860) == 16); // the next bar's downbeat
    CHECK (step16 (-60) == 0);
    CHECK (step16 (-61) == -1);
}
