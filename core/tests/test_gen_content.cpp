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

TEST_CASE ("the default forms are DJ-friendly extended mixes of 6 to 8 minutes", "[gen][content]")
{
    for (const char* id : { "FUT01", "FUT03" })
    {
        INFO (id);
        const auto b = loadBundle (contentDir, id);
        REQUIRE (b.ok());
        const Style& style = b.value.style;
        std::int32_t bars = 0;
        for (const auto& s : style.form)
        {
            CHECK (s.bars % 8 == 0);
            CHECK (s.blocks.size() == static_cast<std::size_t> (s.bars / 8));
            bars += s.bars;
        }
        // seconds = bars * 4 beats * 60 / bpm
        const std::int32_t seconds = bars * 240 / b.value.kit.bpm;
        CHECK (seconds >= 6 * 60);
        CHECK (seconds <= 8 * 60);
        REQUIRE (style.form.size() >= 3);
        CHECK (style.form.front().kind == "intro");
        CHECK (style.form.front().bars == 32);
        CHECK (style.form.back().kind == "outro");
        CHECK (style.form.back().bars == 32);
        const std::set<std::string> drums = { "kick", "snare", "hats", "shaker", "perc" };
        for (const auto& lane : style.form.front().blocks.front())
            CHECK (drums.contains (lane));
        for (const auto& lane : style.form.back().blocks.back())
            CHECK (drums.contains (lane));
        // the intro adds lanes block by block, the outro takes them away
        for (std::size_t i = 1; i < style.form.front().blocks.size(); ++i)
            CHECK (style.form.front().blocks[i].size() > style.form.front().blocks[i - 1].size());
        for (std::size_t i = 1; i < style.form.back().blocks.size(); ++i)
            CHECK (style.form.back().blocks[i].size() < style.form.back().blocks[i - 1].size());
        std::set<std::string> kinds;
        for (const auto& s : style.form)
            kinds.insert (s.kind);
        CHECK (kinds == std::set<std::string> { "intro", "groove", "build", "drop", "break", "drop2", "outro" });
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
    // a section that isn't on 8-bar lines
    CHECK_THAT (withStyle ("{ \"kind\": \"build\", \"bars\": 8,", "{ \"kind\": \"build\", \"bars\": 12,"), ContainsSubstring ("8"));
    // the intro must start with drums only
    CHECK_THAT (withStyle ("[\"kick\", \"hats\"],\n      [\"kick\", \"hats\", \"shaker\"],",
                    "[\"kick\", \"bass\"],\n      [\"kick\", \"hats\", \"shaker\"],"),
        ContainsSubstring ("drums only"));
    // an unknown lane in a block
    CHECK_THAT (withStyle ("[\"snare\", \"hats\", \"shaker\", \"perc\", \"chords\"] ] },\n    { \"kind\": \"drop\"",
                    "[\"snare\", \"hats\", \"shakr\", \"perc\", \"chords\"] ] },\n    { \"kind\": \"drop\""),
        ContainsSubstring ("shakr"));
    // an unknown section kind
    CHECK_THAT (withStyle ("{ \"kind\": \"build\", \"bars\": 8,", "{ \"kind\": \"bridge\", \"bars\": 8,"), ContainsSubstring ("bridge"));
    // a probability above 100
    CHECK_THAT (withStyle ("\"p\":   [100, 0, 0, 0,   0, 0,  6, 10,", "\"p\":   [101, 0, 0, 0,   0, 0,  6, 10,"), ContainsSubstring ("0 to 100"));
    // a grid of the wrong length
    CHECK_THAT (withStyle ("\"p\":   [0, 0, 0, 0,  100, 0, 0, 0,  0, 0, 0, 0,  100, 0, 0, 0],",
                    "\"p\":   [0, 0, 0, 0,  100, 0, 0, 0,  0, 0, 0, 0,  100, 0, 0],"),
        ContainsSubstring ("16 or 32"));
    // a cell that can hit with no velocity
    CHECK_THAT (withStyle ("\"vel\": [124, 0, 0, 0,   0, 0, 98, 100,", "\"vel\": [124, 0, 0, 0,   0, 0, 0, 100,"),
        ContainsSubstring ("velocity"));
    // a lane active in a section kind its "use" has no grid for (the kick in the break)
    CHECK_THAT (withStyle ("[\"hats\", \"chords\"],\n      [\"hats\", \"perc\", \"chords\"],",
                    "[\"hats\", \"kick\", \"chords\"],\n      [\"hats\", \"perc\", \"chords\"],"),
        ContainsSubstring ("break"));
    // a voicing the palette names but nobody defines
    CHECK_THAT (withStyle ("\"intro\": [\"sus2\", \"fifths\"],", "\"intro\": [\"sus2\", \"fourths\"],"), ContainsSubstring ("fourths"));
    // harmonic rhythm units must sum to 4
    CHECK_THAT (withStyle ("\"units\": [3, 1]", "\"units\": [3, 2]"), ContainsSubstring ("add up to 4"));
    // the bass follows a lane that doesn't exist
    CHECK_THAT (withStyle ("\"follow\": \"kick\",", "\"follow\": \"kik\","), ContainsSubstring ("kik"));
    // a voice exclusive with itself would silence itself on every step it plays
    CHECK_THAT (withStyle ("\"exclusive\": [[\"snare\", \"ghost\"]],", "\"exclusive\": [[\"snare\", \"snare\"]],"), ContainsSubstring ("exclusive with itself"));
}

TEST_CASE ("the style loader keeps every form a DJ-friendly extended mix", "[gen][content]")
{
    // The mix-in: 32 bars from drums only, lanes only joining, one at least every 8 or 16 bars. The
    // mix-out: 32 bars, lanes only leaving, one at least every 8 or 16 bars, ending on drums only.
    const Texts t = texts ("fut01", "garage2step");
    REQUIRE (bundleError (t).empty());
    const auto withStyle = [&t] (std::string_view from, std::string_view to) {
        Texts u = t;
        u.style = replaced (u.style, from, to);
        return bundleError (u);
    };
    const std::string intro = "{ \"kind\": \"intro\", \"bars\": 32, \"blocks\": [\n"
                              "      [\"kick\", \"hats\"],\n"
                              "      [\"kick\", \"hats\", \"shaker\"],\n"
                              "      [\"kick\", \"snare\", \"hats\", \"shaker\", \"perc\"],\n"
                              "      [\"kick\", \"snare\", \"hats\", \"shaker\", \"perc\", \"bass\"] ] },";
    const std::string outro = "{ \"kind\": \"outro\", \"bars\": 32, \"blocks\": [\n"
                              "      [\"kick\", \"snare\", \"hats\", \"shaker\", \"perc\", \"bass\", \"chords\"],\n"
                              "      [\"kick\", \"snare\", \"hats\", \"shaker\", \"perc\", \"bass\"],\n"
                              "      [\"kick\", \"snare\", \"hats\", \"shaker\", \"perc\"],\n"
                              "      [\"kick\", \"hats\", \"shaker\"] ] }";
    // a lane leaving in the mix-in
    const std::string leaves = replaced (intro, "[\"kick\", \"hats\", \"shaker\"],", "[\"kick\", \"shaker\"],");
    CHECK_THAT (withStyle (intro, leaves), ContainsSubstring ("form[0].blocks[1]"));
    CHECK_THAT (withStyle (intro, leaves), ContainsSubstring ("\"hats\""));
    // 24 bars without a lane joining
    const std::string idle = replaced (replaced (intro, "[\"kick\", \"snare\", \"hats\", \"shaker\", \"perc\"],", "[\"kick\", \"hats\", \"shaker\"],"),
        "[\"kick\", \"snare\", \"hats\", \"shaker\", \"perc\", \"bass\"] ]",
        "[\"kick\", \"hats\", \"shaker\"] ]");
    CHECK_THAT (withStyle (intro, idle), ContainsSubstring ("form[0].blocks[3]"));
    // a 16-bar mix-in
    CHECK_THAT (withStyle (intro, "{ \"kind\": \"intro\", \"bars\": 16, \"blocks\": [\n      [\"kick\", \"hats\"],\n      [\"kick\", \"hats\", \"shaker\"] ] },"),
        ContainsSubstring ("32 bars"));
    // a lane joining in the mix-out
    const std::string joins = replaced (outro, "[\"kick\", \"snare\", \"hats\", \"shaker\", \"perc\"],", "[\"kick\", \"snare\", \"hats\", \"shaker\", \"perc\", \"chords\"],");
    CHECK_THAT (withStyle (outro, joins), ContainsSubstring ("form[7].blocks[2]"));
    CHECK_THAT (withStyle (outro, joins), ContainsSubstring ("\"chords\""));
    // 24 bars without a lane leaving
    const std::string stays = replaced (replaced (outro, "[\"kick\", \"snare\", \"hats\", \"shaker\", \"perc\", \"bass\"],", "[\"kick\", \"snare\", \"hats\", \"shaker\", \"perc\", \"bass\", \"chords\"],"),
        "[\"kick\", \"snare\", \"hats\", \"shaker\", \"perc\"],",
        "[\"kick\", \"snare\", \"hats\", \"shaker\", \"perc\", \"bass\", \"chords\"],");
    CHECK_THAT (withStyle (outro, stays), ContainsSubstring ("form[7].blocks[2]"));
    // a 24-bar mix-out
    CHECK_THAT (withStyle (outro, "{ \"kind\": \"outro\", \"bars\": 24, \"blocks\": [\n      [\"kick\", \"snare\", \"hats\", \"shaker\", \"perc\"],\n      [\"kick\", \"hats\", \"shaker\"],\n      [\"kick\", \"hats\"] ] }"),
        ContainsSubstring ("32 bars"));
    // a mix-out whose last block keeps the bass: in every block, so lanes still only leave
    const std::string bassStays = "{ \"kind\": \"outro\", \"bars\": 32, \"blocks\": [\n"
                                  "      [\"kick\", \"snare\", \"hats\", \"shaker\", \"perc\", \"bass\", \"chords\"],\n"
                                  "      [\"kick\", \"snare\", \"hats\", \"shaker\", \"perc\", \"bass\"],\n"
                                  "      [\"kick\", \"hats\", \"shaker\", \"perc\", \"bass\"],\n"
                                  "      [\"kick\", \"hats\", \"bass\"] ] }";
    CHECK_THAT (withStyle (outro, bassStays), ContainsSubstring ("form[7].blocks[3]"));
    CHECK_THAT (withStyle (outro, bassStays), ContainsSubstring ("drums only"));
    // a form that doesn't start with an intro, or doesn't end with an outro
    CHECK_THAT (withStyle (intro, replaced (intro, "\"kind\": \"intro\"", "\"kind\": \"groove\"")), ContainsSubstring ("form[0].kind"));
    CHECK_THAT (withStyle (intro, replaced (intro, "\"kind\": \"intro\"", "\"kind\": \"groove\"")), ContainsSubstring ("starts with an intro"));
    CHECK_THAT (withStyle (outro, replaced (outro, "\"kind\": \"outro\"", "\"kind\": \"groove\"")), ContainsSubstring ("form[7].kind"));
    CHECK_THAT (withStyle (outro, replaced (outro, "\"kind\": \"outro\"", "\"kind\": \"groove\"")), ContainsSubstring ("ends with an outro"));
    // a 40-bar mix-in: longer than 32 is refused too, not only shorter
    const std::string longer = replaced (replaced (intro, "\"bars\": 32", "\"bars\": 40"), "\"bass\"] ] },", "\"bass\"],\n      [\"kick\", \"snare\", \"hats\", \"shaker\", \"perc\", \"bass\"] ] },");
    CHECK_THAT (withStyle (intro, longer), ContainsSubstring ("form[0].bars"));
    CHECK_THAT (withStyle (intro, longer), ContainsSubstring ("32 bars long, not 40"));
    // the shipped forms keep every rule, in all four styles
    for (const auto& [kit, style] : { std::pair { "fut01", "garage2step" }, std::pair { "fut02", "sciclub" }, std::pair { "fut03", "futurehouse" }, std::pair { "fut04", "techno" } })
    {
        INFO (style);
        CHECK (bundleError (texts (kit, style)).empty());
    }
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
