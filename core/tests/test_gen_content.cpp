// Tests for core/gen/content (the kit, style and feel loaders) and core/gen/theory, on the real
// content files and on broken copies of them.

#include "gen/content.h"
#include "gen/theory.h"
#include "util/file_io.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <cstdint>
#include <filesystem>
#include <set>
#include <string>
#include <string_view>
#include <utility>

using namespace wubansia;
using namespace wubansia::gen;
using Catch::Matchers::ContainsSubstring;

namespace
{
    const std::filesystem::path contentDir = std::filesystem::path (WUBANSIA_SOURCE_DIR) / "content";

    std::string readText (const std::filesystem::path& p)
    {
        const auto r = util::readBinaryFile (p);
        REQUIRE (r.ok());
        return std::string (r.value.begin(), r.value.end());
    }

    // text with the first `from` replaced by `to` (which must be there).
    std::string replaced (std::string text, std::string_view from, std::string_view to)
    {
        const auto at = text.find (from);
        REQUIRE (at != std::string::npos);
        text.replace (at, from.size(), to);
        return text;
    }

    struct Texts
    {
        std::string kit, style, feel;
    };

    Texts texts (std::string_view kit, std::string_view style)
    {
        return { readText (contentDir / "kits" / (std::string (kit) + ".json")),
            readText (contentDir / "styles" / (std::string (style) + ".json")),
            readText (contentDir / "feels" / (std::string (style) + ".json")) };
    }

    // The error makeBundle gives for these texts, or "" if they load.
    std::string bundleError (const Texts& t)
    {
        auto kit = parseKit (t.kit, "kit.json");
        if (!kit.ok())
            return kit.error;
        auto style = parseStyle (t.style, "style.json");
        if (!style.ok())
            return style.error;
        auto feel = parseFeel (t.feel, "feel.json");
        if (!feel.ok())
            return feel.error;
        auto b = makeBundle (std::move (kit.value), std::move (style.value), std::move (feel.value));
        return b.ok() ? std::string() : b.error;
    }
}

TEST_CASE ("theory spells keys and Camelot codes for the four kits", "[gen][theory]")
{
    CHECK (keyName (6, true) == "F# minor");
    CHECK (keyName (2, true) == "D minor");
    CHECK (keyName (7, true) == "G minor");
    CHECK (keyName (4, true) == "E minor");
    CHECK (keyName (10, true) == "Bb minor");
    CHECK (keyName (1, true) == "C# minor");
    CHECK (keyName (3, true) == "Eb minor");
    CHECK (keyName (0, false) == "C major");
    CHECK (keyName (1, false) == "Db major");
    CHECK (camelot (6, true) == "11A");
    CHECK (camelot (2, true) == "7A");
    CHECK (camelot (7, true) == "6A");
    CHECK (camelot (4, true) == "9A");
    CHECK (camelot (8, true) == "1A");
    CHECK (camelot (9, true) == "8A");
    CHECK (camelot (0, false) == "8B");
    CHECK (camelot (11, false) == "1B");
}

TEST_CASE ("theory builds chords on the modes' degrees", "[gen][theory]")
{
    CHECK (modeSteps (Mode::aeolian) == std::array<int, 7> { 0, 2, 3, 5, 7, 8, 10 });
    CHECK (modeSteps (Mode::dorian) == std::array<int, 7> { 0, 2, 3, 5, 7, 9, 10 });
    CHECK (modeSteps (Mode::phrygian) == std::array<int, 7> { 0, 1, 3, 5, 7, 8, 10 });
    // F# minor: F# G# A B C# D E
    CHECK (degreePc (6, Mode::aeolian, 0) == 6);
    CHECK (degreePc (6, Mode::aeolian, 5) == 2); // bVI = D
    CHECK (degreePc (6, Mode::aeolian, 7) == 6); // the octave
    CHECK (degreePc (6, Mode::aeolian, -1) == 4); // below the tonic: E
    CHECK (degreePc (6, Mode::dorian, 5) == 3); // Dorian's 6th: D#
    CHECK (degreePc (6, Mode::phrygian, 1) == 7); // Phrygian's b2: G
    CHECK (chordInterval (Mode::aeolian, 0, 2) == 3); // i is minor
    CHECK (chordInterval (Mode::aeolian, 5, 2) == 4); // bVI is major
    CHECK (chordInterval (Mode::aeolian, 6, 6) == 10); // bVII has a minor seventh
    CHECK (chordInterval (Mode::dorian, 3, 2) == 4); // Dorian IV is major
    CHECK (chordInterval (Mode::aeolian, 0, 8) == 14); // the ninth
    CHECK (inMinorKey (6, 6));
    CHECK (inMinorKey (6, 4));
    CHECK_FALSE (inMinorKey (6, 3)); // D# is Dorian colour
    CHECK_FALSE (inMinorKey (6, 7)); // G is Phrygian colour
    CHECK (modeFromName ("dorian") == Mode::dorian);
    CHECK_FALSE (modeFromName ("lydian").has_value());
    CHECK (modeName (Mode::phrygian) == "phrygian");
}

TEST_CASE ("p16FromPercent converts exactly", "[gen][content]")
{
    CHECK (p16FromPercent (0) == 0);
    CHECK (p16FromPercent (-5) == 0);
    CHECK (p16FromPercent (50) == 32768);
    CHECK (p16FromPercent (25) == 16384);
    CHECK (p16FromPercent (1) == 655); // 655.36
    CHECK (p16FromPercent (33) == 21627); // 21626.88
    CHECK (p16FromPercent (99) == 64881); // 64880.64
    CHECK (p16FromPercent (100) == 65536);
    CHECK (p16FromPercent (150) == 65536);
}

TEST_CASE ("the FUT01 and FUT03 bundles load from content", "[gen][content]")
{
    const auto a = loadBundle (contentDir, "FUT01");
    REQUIRE (a.ok());
    CHECK (a.value.kit.id == "FUT01");
    CHECK (a.value.kit.name == "Alloy Signal");
    CHECK (a.value.kit.bpm == 134);
    CHECK (a.value.kit.tonic == 6);
    CHECK (a.value.kit.minor);
    CHECK (a.value.kit.camelot == "11A");
    CHECK (a.value.kit.swing == 570);
    CHECK (a.value.style.id == "garage2step");
    CHECK (a.value.feel.id == "garage2step");

    const auto b = loadBundle (contentDir, "fut03"); // the id is case-insensitive for the file name
    REQUIRE (b.ok());
    CHECK (b.value.kit.id == "FUT03");
    CHECK (b.value.kit.bpm == 128);
    CHECK (b.value.kit.tonic == 7);
    CHECK (b.value.kit.camelot == "6A");
    CHECK (b.value.kit.swing == 530);
    CHECK (b.value.style.id == "futurehouse");

    const auto missing = loadBundle (contentDir, "FUT99");
    CHECK_FALSE (missing.ok());
    CHECK_THAT (missing.error, ContainsSubstring ("fut99.json"));
}

TEST_CASE ("both bundles use the GM drum notes inside 35-59 and the lanes of the brief", "[gen][content]")
{
    for (const char* id : { "FUT01", "FUT03" })
    {
        INFO (id);
        const auto b = loadBundle (contentDir, id);
        REQUIRE (b.ok());
        std::set<std::string> lanes;
        for (const auto& lane : b.value.style.lanes)
            lanes.insert (lane.id);
        CHECK (lanes == std::set<std::string> { "kick", "snare", "hats", "shaker", "perc", "bass", "chords" });
        for (const auto& lane : b.value.kit.lanes)
            for (const auto& n : lane.notes)
            {
                INFO (lane.id << " " << n.voice);
                CHECK (n.note >= 35);
                CHECK (n.note <= 59);
            }
        const auto note = [&] (std::string_view lane, std::string_view voice) -> int {
            for (const auto& n : b.value.kit.lane (lane)->notes)
                if (n.voice == voice)
                    return n.note;
            return -1;
        };
        CHECK (note ("kick", "kick") == 36);
        CHECK (note ("snare", "snare") == 38);
        CHECK (note ("snare", "clap") == 39);
        CHECK (note ("hats", "closed") == 42);
        CHECK (note ("hats", "open") == 46);
        CHECK (b.value.kit.lane ("kick")->channel == 9);
    }
}

TEST_CASE ("generator kits follow the MIDI v2 channels", "[gen][content]")
{
    // docs/midi-map.md's channels, 1-based as there (a kit's "channel" counts from 0): the bass on 1
    // (Wubansia's bass multisample), a lead on 2 (its lead multisample: FUT02's arp), the voiced
    // chords on 3 (the chord voicing, which Wubansia ignores: on 4 it would trigger the kit's chord
    // sample at every note), the drums on 10. FUT04's rumble, the one bass-kind lane that isn't the
    // bass, sits on 9, reserved, where Wubansia plays nothing, until it becomes drum note 35.
    const auto midiV2 = [] (std::string_view kit, const LaneSpec& lane) -> int {
        if (kit == "FUT04" && lane.id == "rumble")
            return 9;
        switch (lane.kind)
        {
            case LaneKind::drums: return 10;
            case LaneKind::bass: return 1;
            case LaneKind::chords: return 3;
            case LaneKind::arp: return 2;
        }
        return 0; // a new kind: give it its channel from the table first
    };
    for (const char* id : { "FUT01", "FUT02", "FUT03", "FUT04" })
    {
        const auto b = loadBundle (contentDir, id);
        REQUIRE (b.ok());
        std::set<int> pitched;
        for (const auto& lane : b.value.style.lanes)
        {
            INFO (id << " " << lane.id);
            const KitLane* kl = b.value.kit.lane (lane.id);
            REQUIRE (kl != nullptr);
            CHECK (kl->channel + 1 == midiV2 (id, lane));
            if (lane.kind != LaneKind::drums)
                CHECK (pitched.insert (kl->channel + 1).second); // one instrument per channel
        }
    }
}

TEST_CASE ("the kit loader refuses broken kits and says where", "[gen][content]")
{
    const Texts t = texts ("fut01", "garage2step");
    REQUIRE (bundleError (t).empty());

    const auto withKit = [&t] (std::string_view from, std::string_view to) {
        Texts u = t;
        u.kit = replaced (u.kit, from, to);
        return bundleError (u);
    };
    CHECK_THAT (withKit ("\"bpm\": 134", "\"bpm\": 150"), ContainsSubstring ("bpm"));
    CHECK_THAT (withKit ("\"bpm\": 134", "\"bpm\": 134.0"), ContainsSubstring ("integer"));
    CHECK_THAT (withKit ("\"camelot\": \"11A\"", "\"camelot\": \"12A\""), ContainsSubstring ("11A"));
    CHECK_THAT (withKit ("\"name\": \"F# minor\"", "\"name\": \"G minor\""), ContainsSubstring ("F# minor"));
    CHECK_THAT (withKit ("\"kick\": 36", "\"kick\": 70"), ContainsSubstring ("35"));
    CHECK_THAT (withKit ("\"swing\": 570", "\"swing\": 570, \"swng\": 1"), ContainsSubstring ("unknown key \"swng\""));
    CHECK_THAT (withKit ("\"channel\": 9, \"notes\": { \"kick\": 36 }", "\"channel\": 16, \"notes\": { \"kick\": 36 }"),
        ContainsSubstring ("channel"));
    CHECK_THAT (withKit ("\"notes\": { \"kick\": 36 }", "\"notes\": { \"kik\": 36 }"), ContainsSubstring ("kick"));
    CHECK_THAT (withKit ("\"range\": [28, 52], \"root\": 30", "\"range\": [28, 52], \"root\": 20"), ContainsSubstring ("root"));
    CHECK_THAT (withKit ("\"style\": \"garage2step\"", "\"style\": \"garage2step\", \"style\": \"x\""), ContainsSubstring ("twice"));
    CHECK_THAT (withKit ("\"format\": 1", "\"format\": 2"), ContainsSubstring ("format"));
}

TEST_CASE ("the style loader refuses broken styles and says where", "[gen][content]")
{
    const Texts t = texts ("fut01", "garage2step");
    const auto withStyle = [&t] (std::string_view from, std::string_view to) {
        Texts u = t;
        u.style = replaced (u.style, from, to);
        return bundleError (u);
    };
    // (the form grammar's refusals: test_gen_form.cpp)
    // an unknown section kind
    CHECK_THAT (withStyle ("\"build\": { \"energy\": 3,", "\"bridge\": { \"energy\": 3,"), ContainsSubstring ("bridge"));
    // a probability above 100
    CHECK_THAT (withStyle ("\"p\":   [100, 0, 0, 0,   0, 0,  6, 10,", "\"p\":   [101, 0, 0, 0,   0, 0,  6, 10,"), ContainsSubstring ("0 to 100"));
    // a grid of the wrong length
    CHECK_THAT (withStyle ("\"p\":   [0, 0, 0, 0,  100, 0, 0, 0,  0, 0, 0, 0,  100, 0, 0, 0],",
                    "\"p\":   [0, 0, 0, 0,  100, 0, 0, 0,  0, 0, 0, 0,  100, 0, 0],"),
        ContainsSubstring ("16 or 32"));
    // a cell that can hit with no velocity
    CHECK_THAT (withStyle ("\"vel\": [124, 0, 0, 0,   0, 0, 98, 100,", "\"vel\": [124, 0, 0, 0,   0, 0, 0, 100,"),
        ContainsSubstring ("velocity"));
    // a lane the form plays in a section kind its "use" has no grid for (the kick in the break)
    CHECK_THAT (withStyle ("\"break\": {\n        \"lanes\": [\"hats\",", "\"break\": {\n        \"lanes\": [\"kick\", \"hats\","), ContainsSubstring ("no grid for break"));
    // a voicing the palette names but nobody defines
    CHECK_THAT (withStyle ("\"intro\": [\"sus2\", \"fifths\"],", "\"intro\": [\"sus2\", \"fourths\"],"), ContainsSubstring ("fourths"));
    // harmonic rhythm units must sum to 4
    CHECK_THAT (withStyle ("\"units\": [3, 1]", "\"units\": [3, 2]"), ContainsSubstring ("add up to 4"));
    // the bass follows a lane that doesn't exist
    CHECK_THAT (withStyle ("\"follow\": \"kick\",", "\"follow\": \"kik\","), ContainsSubstring ("kik"));
    // a voice exclusive with itself would silence itself on every step it plays
    CHECK_THAT (withStyle ("\"exclusive\": [[\"snare\", \"ghost\"]],", "\"exclusive\": [[\"snare\", \"snare\"]],"), ContainsSubstring ("exclusive with itself"));
}

TEST_CASE ("the feel loader refuses broken feels and says where", "[gen][content]")
{
    const Texts t = texts ("fut01", "garage2step");
    const auto withFeel = [&t] (std::string_view from, std::string_view to) {
        Texts u = t;
        u.feel = replaced (u.feel, from, to);
        return bundleError (u);
    };
    CHECK_THAT (withFeel ("\"amount\": 1000", "\"amount\": 1000.5"), ContainsSubstring ("integer"));
    CHECK_THAT (withFeel ("\"lock\": \"kick\"", "\"lock\": \"kik\""), ContainsSubstring ("kik"));
    CHECK_THAT (withFeel ("\"voice\": \"clap\", \"after\": \"snare\"", "\"voice\": \"clapp\", \"after\": \"snare\""),
        ContainsSubstring ("clapp"));
    CHECK_THAT (withFeel ("\"outro\": {}", "\"outtro\": {}"), ContainsSubstring ("outtro"));
    CHECK_THAT (withFeel ("\"drop2\": { \"hats\": { \"swing\": [30, 70] }",
                    "\"drop2\": { \"hatz\": { \"swing\": [30, 70] }"),
        ContainsSubstring ("hatz"));
    CHECK_THAT (withFeel ("\"swing\": [10, 60]", "\"swing\": [60, 10]"), ContainsSubstring ("swing"));
    // every section kind needs a row
    CHECK_THAT (withFeel (",\n    \"outro\": {}", ""), ContainsSubstring ("outro"));
}

TEST_CASE ("the feel loader needs swing, sd and clamp at a lane's base, not only in a row", "[gen][content]")
{
    // A row only overrides: perform() reads all three for every section kind (and the lane's
    // reported swing from its base), so a lane that sets one only in rows.drop has none elsewhere.
    const Texts t = texts ("fut01", "garage2step");
    REQUIRE (bundleError (t).empty());
    const auto moved = [&t] (std::string_view base, std::string_view drop) {
        Texts u = t;
        u.feel = replaced (u.feel, "\"kick\": { \"swing\": [0, 0], \"sd\": [8, 12], \"clamp\": 4,", base);
        u.feel = replaced (u.feel, "\"drop\": {},", "\"drop\": { \"kick\": { " + std::string (drop) + " } },");
        return bundleError (u);
    };
    for (const auto& [base, drop] : { std::pair { "\"kick\": { \"sd\": [8, 12], \"clamp\": 4,", "\"swing\": [0, 0]" },
             std::pair { "\"kick\": { \"swing\": [0, 0], \"clamp\": 4,", "\"sd\": [8, 12]" },
             std::pair { "\"kick\": { \"swing\": [0, 0], \"sd\": [8, 12],", "\"clamp\": 4" } })
    {
        INFO (drop);
        const std::string e = moved (base, drop);
        CHECK_THAT (e, ContainsSubstring ("needs swing, sd and clamp"));
        CHECK_THAT (e, ContainsSubstring ("\"kick\""));
    }
}

TEST_CASE ("the feel loader allows one lock per lane, at its base, to a lane that locks to nothing", "[gen][content]")
{
    // perform() orders the lanes and takes every lock's target from the lane's base: a row's lock
    // would be read in one place and not the other, and a chain or a cycle can't be ordered.
    const Texts t = texts ("fut01", "garage2step");
    REQUIRE (bundleError (t).empty());
    const auto withFeel = [&t] (std::string_view from, std::string_view to) {
        Texts u = t;
        u.feel = replaced (u.feel, from, to);
        return bundleError (u);
    };
    // a lock in a row: the drop's (which ordered the lanes) or any other
    CHECK_THAT (withFeel ("\"drop\": {},", "\"drop\": { \"bass\": { \"lock\": \"snare\" } },"), ContainsSubstring ("rows.drop.bass.lock"));
    CHECK_THAT (withFeel ("\"groove\": {},", "\"groove\": { \"bass\": { \"lock\": \"kick\" } },"), ContainsSubstring ("rows.groove.bass.lock"));
    // a cycle of two lanes, and a chain
    Texts cycle = t;
    cycle.feel = replaced (cycle.feel, "\"snare\": {\n      \"swing\": [0, 0],", "\"snare\": {\n      \"lock\": \"hats\", \"swing\": [0, 0],");
    cycle.feel = replaced (cycle.feel, "\"hats\": {\n      \"swing\": [10, 60],", "\"hats\": {\n      \"lock\": \"snare\", \"swing\": [10, 60],");
    CHECK_THAT (bundleError (cycle), ContainsSubstring ("locks to another lane itself"));
    CHECK_THAT (withFeel ("\"kick\": { \"swing\": [0, 0],", "\"kick\": { \"lock\": \"snare\", \"swing\": [0, 0],"),
        ContainsSubstring ("lanes.bass.lock: \"kick\" locks to another lane itself"));
}

TEST_CASE ("Feel::resolve lays a section row over the lane defaults", "[gen][content]")
{
    const auto b = loadBundle (contentDir, "FUT01");
    REQUIRE (b.ok());
    const Feel& feel = b.value.feel;
    const FeelLane drop = feel.resolve ("hats", "drop");
    const FeelLane drop2 = feel.resolve ("hats", "drop2");
    const FeelLane brk = feel.resolve ("hats", "break");
    REQUIRE (drop.swing.has_value());
    CHECK (drop.swing->lo == 10);
    CHECK (drop.swing->hi == 60);
    REQUIRE (drop2.swing.has_value());
    CHECK (drop2.swing->lo == 30); // the row
    CHECK (drop2.sd->lo == drop.sd->lo); // not in the row: the lane's
    CHECK (brk.sd->lo == 40);
    CHECK (brk.swing->lo == 10);
    CHECK (feel.resolve ("bass", "drop").lock == std::optional<std::string> ("kick"));
}
