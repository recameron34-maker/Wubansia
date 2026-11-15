// Tests for the FUT02 "Orbit Relay" (sci-fi club) and FUT04 "Furnace Code" (techno) content, and for
// the loader rules of the engine features those styles use: the arp lane (on FUT02 as it was before
// the hook, core/tests/fixtures/nohook), CC lanes, the bass's duck, per-step slides and mutations,
// the feel's tight rule, one-chord (pedal) harmony, and the hook block (S1b2b, on the hook fixture).

#include "gen/content.h"
#include "gen/song.h"
#include "gen/theory.h"
#include "util/file_io.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <string_view>

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
        INFO ("looking for: " << from);
        REQUIRE (at != std::string::npos);
        text.replace (at, from.size(), to);
        return text;
    }

    struct Texts
    {
        std::string kit, style, feel;
    };

    Texts texts (std::string_view kit, std::string_view style, const std::filesystem::path& dir = contentDir)
    {
        return { readText (dir / "kits" / (std::string (kit) + ".json")),
            readText (dir / "styles" / (std::string (style) + ".json")),
            readText (dir / "feels" / (std::string (style) + ".json")) };
    }

    const std::filesystem::path fixtureDir = std::filesystem::path (WUBANSIA_SOURCE_DIR) / "core" / "tests" / "fixtures";

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

    Bundle bundleOf (std::string_view id)
    {
        auto b = loadBundle (contentDir, id);
        INFO (b.error);
        REQUIRE (b.ok());
        return std::move (b.value);
    }

    // The role the metrics give a track (tools/legacy_audit/README.md, "Lanes and roles"): from the
    // first word of its name, letters only, lower-cased; drum tracks are found by their channel.
    std::string roleOf (std::string_view track)
    {
        std::string word;
        for (const char c : track)
        {
            if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')))
                break;
            word += static_cast<char> (c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
        }
        // clang-format off
        static const std::map<std::string, std::string> roles = {
            { "bass", "bass" }, { "sub", "bass" }, { "reese", "bass" }, { "acid", "bass" }, { "donk", "bass" },
            { "chords", "chords" }, { "stabs", "chords" }, { "stab", "chords" }, { "pad", "chords" }, { "organ", "chords" },
            { "strings", "chords" }, { "piano", "chords" }, { "brass", "chords" }, { "orchestra", "chords" }, { "choir", "chords" },
            { "melody", "lead" }, { "lead", "lead" }, { "pluck", "lead" }, { "arp", "lead" }, { "counter", "lead" },
            { "talkbox", "lead" }, { "hoover", "lead" },
            { "vocal", "vox" }, { "vox", "vox" }, { "chops", "vox" }, { "shouts", "vox" },
            { "riser", "fx" }, { "fx", "fx" }, { "impact", "fx" }, { "impacts", "fx" }, { "dub", "fx" },
        };
        // clang-format on
        const auto it = roles.find (word);
        return it == roles.end() ? "other" : it->second;
    }

    int noteOf (const Kit& kit, std::string_view lane, std::string_view voice)
    {
        const KitLane* l = kit.lane (lane);
        if (l == nullptr)
            return -1;
        for (const auto& n : l->notes)
            if (n.voice == voice)
                return n.note;
        return -1;
    }
}

TEST_CASE ("styles: the FUT02 and FUT04 bundles load from content", "[gen][styles][content]")
{
    const Bundle a = bundleOf ("FUT02");
    CHECK (a.kit.id == "FUT02");
    CHECK (a.kit.name == "Orbit Relay");
    CHECK (a.kit.bpm == 130);
    CHECK (a.kit.tonic == 2);
    CHECK (a.kit.keyName == "D minor");
    CHECK (a.kit.camelot == "7A");
    CHECK (a.kit.swing == 520);
    CHECK (a.style.id == "sciclub");
    CHECK (a.feel.id == "sciclub");

    const Bundle b = bundleOf ("fut04"); // the id is case-insensitive for the file name
    CHECK (b.kit.id == "FUT04");
    CHECK (b.kit.name == "Furnace Code");
    CHECK (b.kit.bpm == 136);
    CHECK (b.kit.tonic == 4);
    CHECK (b.kit.keyName == "E minor");
    CHECK (b.kit.camelot == "9A");
    CHECK (b.kit.swing == 540);
    CHECK (b.style.id == "techno");
    CHECK (b.feel.id == "techno");
}

TEST_CASE ("styles: the new kits keep drums on GM notes inside 35-59 and have the lanes of their briefs", "[gen][styles][content]")
{
    const std::map<std::string, std::set<std::string>> expected = {
        { "FUT02", { "kick", "snare", "hats", "perc", "bass", "chords", "arp" } },
        { "FUT04", { "kick", "rumble", "snare", "hats", "perc", "bass", "chords", "lead" } }, // the lead: the hook's carrier (S1b2b)
    };
    for (const auto& [id, lanes] : expected)
    {
        INFO (id);
        const Bundle b = bundleOf (id);
        std::set<std::string> got;
        std::set<std::uint8_t> pitchedChannels;
        for (const auto& lane : b.style.lanes)
        {
            got.insert (lane.id);
            const KitLane* kl = b.kit.lane (lane.id);
            REQUIRE (kl != nullptr);
            if (lane.kind == LaneKind::drums)
            {
                CHECK (kl->channel == 9);
                for (const auto& n : kl->notes)
                {
                    INFO (lane.id << " " << n.voice);
                    CHECK (n.note >= 35);
                    CHECK (n.note <= 59);
                }
            }
            else
            {
                CHECK (kl->channel != 9);
                CHECK (pitchedChannels.insert (kl->channel).second); // one channel per pitched lane
            }
        }
        CHECK (got == lanes);
        CHECK (noteOf (b.kit, "kick", "kick") == 36);
        CHECK (noteOf (b.kit, "snare", "snare") == 38);
        CHECK (noteOf (b.kit, "snare", "clap") == 39);
        CHECK (noteOf (b.kit, "hats", "closed") == 42);
        CHECK (noteOf (b.kit, "hats", "open") == 46);
        CHECK (b.style.lanes[*b.style.laneIndex ("bass")].kind == LaneKind::bass);
        CHECK (b.style.lanes[*b.style.laneIndex ("bass")].bass.follow == "kick");
        CHECK (b.style.lanes[*b.style.laneIndex ("chords")].kind == LaneKind::chords);
    }
    const Bundle a = bundleOf ("FUT02");
    CHECK (a.style.lanes[*a.style.laneIndex ("arp")].kind == LaneKind::lead); // the arp carries the hook (S1b2b) as the written ChromeArp
    const Bundle t = bundleOf ("FUT04");
    CHECK (noteOf (t.kit, "hats", "ride") == 51); // the offbeat ride
    const LaneSpec& rumble = t.style.lanes[*t.style.laneIndex ("rumble")];
    CHECK (rumble.kind == LaneKind::bass); // low notes on the kicks, for the kit's rumble sample
    CHECK (rumble.bass.follow == "kick");
    CHECK (t.kit.lane ("rumble")->root == 28); // E1: the kit's kick note, so kick, sub and rumble are one note
}

TEST_CASE ("styles: track names give each lane its role in the metrics", "[gen][styles][content]")
{
    // The metrics read roles from track names (tools/legacy_audit/README.md): the arp is a lead, the
    // acid and the rolling bass are basses, the pads are chords. The rumble is a kick texture (a
    // four-on-the-floor kick's tail, like the exempt kick in G3), so it takes no bass, chords or lead role.
    const Bundle a = bundleOf ("FUT02");
    CHECK (roleOf (a.kit.lane ("bass")->track) == "bass");
    CHECK (roleOf (a.kit.lane ("chords")->track) == "chords");
    CHECK (roleOf (a.kit.lane ("arp")->track) == "lead");
    const Bundle t = bundleOf ("FUT04");
    CHECK (roleOf (t.kit.lane ("bass")->track) == "bass");
    CHECK (roleOf (t.kit.lane ("chords")->track) == "chords");
    CHECK (roleOf (t.kit.lane ("rumble")->track) == "other");
    REQUIRE (t.kit.lane ("lead") != nullptr);
    CHECK (roleOf (t.kit.lane ("lead")->track) == "lead"); // 'Lead Blips', the hook's carrier (S1b2b)
    // no artist names and no style tags in the names: plain words for what the lane plays
    for (const Bundle* b : { &a, &t })
        for (const auto& lane : b->kit.lanes)
        {
            CHECK_FALSE (lane.track.empty());
            CHECK (lane.track.size() <= 24);
        }
}

TEST_CASE ("styles: the loader reads the new engine fields", "[gen][styles][content]")
{
    const Bundle t = bundleOf ("FUT04");
    const LaneSpec& bass = t.style.lanes[*t.style.laneIndex ("bass")];
    const LaneSpec& rumble = t.style.lanes[*t.style.laneIndex ("rumble")];
    CHECK (bass.bass.duck == 48);
    CHECK (rumble.bass.duck == 60);
    const auto grid = [] (const LaneSpec& l, std::string_view name) -> const BassGrid& {
        for (const auto& g : l.bass.grids)
            if (g.name == name)
                return g;
        FAIL ("no grid " << name);
        return l.bass.grids.front();
    };
    CHECK (grid (bass, "main").mutate == 3);
    CHECK (grid (bass, "main").hasSlide);
    CHECK (std::any_of (grid (bass, "main").slide.begin(), grid (bass, "main").slide.end(), [] (std::int32_t p) { return p > 0; }));
    CHECK (t.feel.resolve ("bass", "drop").tight == std::optional<std::int32_t> (64));
    CHECK (t.feel.resolve ("rumble", "drop").tight == std::optional<std::int32_t> (1));
    CHECK_FALSE (t.feel.resolve ("hats", "drop").tight.has_value());
    // a pedal: one chord over the whole loop
    REQUIRE (t.style.harmony.rhythms.size() == 1);
    CHECK (t.style.harmony.rhythms.front() == std::vector<std::int32_t> { 4 });
    // the acid's filter: CC74, with a row for every section kind the bass plays in
    REQUIRE (bass.cc.size() == 1);
    CHECK (bass.cc.front().number == 74);
    CHECK (bass.cc.front().step == 120);
    const std::size_t bassLane = *t.style.laneIndex ("bass");
    for (std::uint64_t seed = 0; seed < 16; ++seed)
    {
        Options o;
        o.seed = seed;
        const auto song = generate (t, o);
        REQUIRE (song.ok());
        for (const auto& sec : song.value.sections)
            for (std::int32_t bar = 0; bar < sec.bars; ++bar)
                if (sec.active (bassLane, bar))
                    CHECK (bass.cc.front().row (sec.kind) != nullptr);
    }
    const CcRow* build = bass.cc.front().row ("build");
    REQUIRE (build != nullptr);
    CHECK (build->to.lo > build->from.hi); // the build opens the filter
    CHECK (rumble.cc.empty());
    // the drone: voicings start afresh every phrase and its pad holds common tones over
    const LaneSpec& drone = t.style.lanes[*t.style.laneIndex ("chords")];
    CHECK (drone.chords.fresh);
    const auto padGrid = std::find_if (drone.chords.grids.begin(), drone.chords.grids.end(), [] (const ChordGrid& g) { return g.name == "pad"; });
    REQUIRE (padGrid != drone.chords.grids.end());
    CHECK (padGrid->pad);
    CHECK (padGrid->padHold);
    CHECK (padGrid->padGap == 60);
    CHECK_FALSE (t.style.harmony.pushAlike); // the pedal pushes nothing

    const Bundle a = bundleOf ("FUT02");
    // the arp lane (arp.h), on FUT02 as it was before the hook: the shipped arp carries the hook (S1b2b)
    const auto before = loadBundle (fixtureDir / "nohook", "FUT02");
    INFO (before.error);
    REQUIRE (before.ok());
    const LaneSpec& arp = before.value.style.lanes[*before.value.style.laneIndex ("arp")];
    REQUIRE (arp.kind == LaneKind::arp);
    CHECK (arp.arp.center == 68);
    REQUIRE_FALSE (arp.arp.grids.empty());
    for (const auto& g : arp.arp.grids)
    {
        INFO (g.name);
        CHECK_FALSE (g.cells.empty());
        CHECK_FALSE (g.tones.empty());
        CHECK (g.tones.front() == 0); // the ladder starts on the root
        CHECK (std::is_sorted (g.tones.begin(), g.tones.end()));
        CHECK (g.tones.back() <= 6); // one octave's worth: the ladder repeats it
        CHECK (g.octave % 12 == 0);
    }
    CHECK (a.style.lanes[*a.style.laneIndex ("bass")].bass.duck == 44);
    // the starfield pad's slow filter movement
    const LaneSpec& pad = a.style.lanes[*a.style.laneIndex ("chords")];
    REQUIRE (pad.cc.size() == 1);
    CHECK (pad.cc.front().number == 74);
    CHECK (pad.chords.fresh);
    for (const auto& g : pad.chords.grids)
        CHECK_FALSE (g.padHold); // its chords change, so it strikes each one
    CHECK (a.style.harmony.pushAlike);
}

TEST_CASE ("styles: the loader refuses broken new fields and says where", "[gen][styles][content]")
{
    const Texts techno = texts ("fut04", "techno");
    const Texts sciclub = texts ("fut02", "sciclub", fixtureDir / "nohook"); // the arp lane: FUT02 before the hook
    REQUIRE (bundleError (techno).empty());
    REQUIRE (bundleError (sciclub).empty());
    const auto withStyle = [] (const Texts& t, std::string_view from, std::string_view to) {
        Texts u = t;
        u.style = replaced (u.style, from, to);
        return bundleError (u);
    };
    const auto withFeel = [] (const Texts& t, std::string_view from, std::string_view to) {
        Texts u = t;
        u.feel = replaced (u.feel, from, to);
        return bundleError (u);
    };
    const auto withKit = [] (const Texts& t, std::string_view from, std::string_view to) {
        Texts u = t;
        u.kit = replaced (u.kit, from, to);
        return bundleError (u);
    };

    // the duck: 1-90 ticks, so a note on the 16th grid keeps 30
    CHECK_THAT (withStyle (techno, "\"duck\": 48,", "\"duck\": 91,"), ContainsSubstring ("duck"));
    CHECK_THAT (withStyle (techno, "\"duck\": 48,", "\"duck\": 0,"), ContainsSubstring ("duck"));
    // a duck needs a lane to follow
    CHECK_THAT (withStyle (techno, "\"follow\": \"kick\", \"duck\": 48,", "\"duck\": 48,"), ContainsSubstring ("follow"));
    // mutations: 1-8 cells
    CHECK_THAT (withStyle (techno, "\"mutate\": 2,", "\"mutate\": 9,"), ContainsSubstring ("mutate"));
    // a per-step slide is a percentage
    CHECK_THAT (withStyle (techno, "\"slide\": [0, 12, 30, 0,", "\"slide\": [0, 12, 130, 0,"), ContainsSubstring ("0 to 100"));
    // tight is a score velocity, 1-127
    CHECK_THAT (withFeel (techno, "\"tight\": 64", "\"tight\": 0"), ContainsSubstring ("tight"));
    CHECK_THAT (withFeel (techno, "\"tight\": 64", "\"tight\": 128"), ContainsSubstring ("tight"));
    // harmonic rhythm units still add up to 4 when there is only one
    CHECK_THAT (withStyle (techno, "\"units\": [4]", "\"units\": [3]"), ContainsSubstring ("add up to 4"));
    // CC lanes
    CHECK_THAT (withStyle (techno, "{ \"number\": 74, \"step\": 120,", "{ \"number\": 120, \"step\": 120,"), ContainsSubstring ("number"));
    CHECK_THAT (withStyle (techno, "{ \"number\": 74, \"step\": 120,", "{ \"number\": 74, \"step\": 100,"), ContainsSubstring ("step"));
    CHECK_THAT (withStyle (techno, "\"build\": { \"from\": [58, 66], \"to\": [100, 112] }", "\"bridge\": { \"from\": [58, 66], \"to\": [100, 112] }"),
        ContainsSubstring ("bridge"));
    CHECK_THAT (withStyle (techno, "\"build\": { \"from\": [58, 66], \"to\": [100, 112] },", ""), ContainsSubstring ("build"));
    CHECK_THAT (withStyle (techno, "\"wave\": [8, 16]", "\"wave\": [8, 12]"), ContainsSubstring ("4, 8, 16 or 32"));
    CHECK_THAT (withStyle (techno, "\"from\": [58, 66]", "\"from\": [58, 166]"), ContainsSubstring ("127"));
    CHECK_THAT (withStyle (techno, "\"wave\": [8, 16]", "\"wav\": [8, 16]"), ContainsSubstring ("unknown key \"wav\""));
    // the arp lane
    CHECK_THAT (withStyle (sciclub, "\"id\": \"arp\", \"kind\": \"arp\", \"center\": 68,", "\"id\": \"arp\", \"kind\": \"arp\", \"center\": 68, \"centre\": 1,"),
        ContainsSubstring ("unknown key \"centre\""));
    CHECK_THAT (withStyle (sciclub, "\"center\": 68,", "\"center\": 40,"), ContainsSubstring ("center"));
    CHECK_THAT (withStyle (sciclub, "\"tones\": [0, 2, 4, 6]", "\"tones\": [0, 4, 2, 6]"), ContainsSubstring ("ascending"));
    CHECK_THAT (withStyle (sciclub, "\"tones\": [0, 2, 4, 6]", "\"tones\": [2, 4, 6]"), ContainsSubstring ("root"));
    CHECK_THAT (withStyle (sciclub, "\"tones\": [0, 2, 4, 6]", "\"tones\": [0, 2, 4, 7]"), ContainsSubstring ("0 to 6"));
    CHECK_THAT (withStyle (sciclub, "\"restart\": [0, 8]", "\"restart\": [0, 16]"), ContainsSubstring ("0 to 15"));
    CHECK_THAT (withStyle (sciclub, "\"octave\": 0", "\"octave\": 7"), ContainsSubstring ("multiple of 12"));
    CHECK_THAT (withStyle (sciclub, "{ \"idx\": [0, 4, 2], \"w\": 12 }", "{ \"idx\": [], \"w\": 12 }"), ContainsSubstring ("idx"));
    CHECK_THAT (withStyle (sciclub, "\"kind\": \"arp\"", "\"kind\": \"arpeggio\""), ContainsSubstring ("arpeggio"));
    // an arp needs a register: a range in the kit, not drum notes
    CHECK_THAT (withKit (sciclub, "\"arp\": { \"track\": \"Arp Chrome\", \"channel\": 1, \"range\": [62, 86] }", "\"arp\": { \"track\": \"Arp Chrome\", \"channel\": 1, \"notes\": { \"zap\": 47 } }"),
        ContainsSubstring ("arp"));
    // the feel can't spread or follow an arp's notes like a chord's or a drum's
    CHECK_THAT (withFeel (sciclub, "\"arp\": {", "\"arp\": { \"spread\": [0, 2],"), ContainsSubstring ("spread"));
    // fresh voicings, alike pushes and held pads are switches, 0 or 1; a hold is a pad's
    CHECK_THAT (withStyle (techno, "\"fresh\": 1,", "\"fresh\": 2,"), ContainsSubstring ("fresh"));
    CHECK_THAT (withStyle (techno, "\"gap\": 60, \"hold\": 1 }", "\"gap\": 60, \"hold\": 2 }"), ContainsSubstring ("hold"));
    CHECK_THAT (withStyle (techno, "\"min\": 1,\n          \"anticipate\": 0", "\"min\": 1,\n          \"anticipate\": 0, \"hold\": 1"),
        ContainsSubstring ("unknown key \"hold\""));
    CHECK_THAT (withStyle (sciclub, "\"alike\": 1 }", "\"alike\": -1 }"), ContainsSubstring ("alike"));
}

TEST_CASE ("styles: the garage and house styles use only the engine fields their briefs need", "[gen][styles][content]")
{
    // The engine fields are opt-in: FUT01 and FUT03 use none of the arp, the duck, mutations,
    // per-step slides, fresh voicings, held pads or tight notes. Their one controller lane is the
    // chords' CC74 filter, which sweeps their builds up into the drops.
    for (const char* id : { "FUT01", "FUT03" })
    {
        INFO (id);
        const Bundle b = bundleOf (id);
        for (const auto& lane : b.style.lanes)
        {
            CHECK (lane.kind != LaneKind::arp);
            CHECK (lane.cc.size() == (lane.id == "chords" ? 1u : 0u));
            for (const auto& cc : lane.cc)
                CHECK (cc.number == 74);
            CHECK (lane.bass.duck == 0);
            for (const auto& g : lane.bass.grids)
            {
                CHECK (g.mutate == 0);
                CHECK_FALSE (g.hasSlide);
            }
            CHECK_FALSE (lane.chords.fresh);
            for (const auto& g : lane.chords.grids)
                CHECK_FALSE (g.padHold);
            for (const auto& kind : sectionKinds)
                CHECK_FALSE (b.feel.resolve (lane.id, kind).tight.has_value());
        }
        for (const auto& r : b.style.harmony.rhythms)
            CHECK (r.size() >= 2);
        CHECK_FALSE (b.style.harmony.pushAlike);
    }
}

TEST_CASE ("styles: the loader refuses broken hook blocks and says where", "[gen][styles][content][hook]")
{
    // The hook fixture (core/tests/fixtures/hook: FUT01 with the plan's hook block) loads, and so does
    // a pedal style made from it (FUT02's: all-accent cells and cells from 16th 0 allowed); each broken
    // copy is refused at the hook block's path (docs/plans/S1b2b-plan.md, "The loader's refusals").
    const Texts hook = texts ("fut01", "garage2step", fixtureDir / "hook");
    REQUIRE (bundleError (hook).empty());
    const auto refused = [] (const Texts& t, std::string_view from, std::string_view to) {
        Texts u = t;
        u.style = replaced (u.style, from, to);
        return bundleError (u);
    };
    const auto hookRefused = [&] (std::string_view from, std::string_view to) { return refused (hook, from, to); };

    // contours: a known shape, a weight that can be drawn, windows inside the 2-bar phrase
    CHECK_THAT (hookRefused ("{ \"shape\": \"early\", \"w\": 1 }", "{ \"shape\": \"middle\", \"w\": 1 }"), ContainsSubstring ("hook.contours[1].shape"));
    CHECK_THAT (hookRefused ("[ { \"shape\": \"late\", \"w\": 3 }, { \"shape\": \"early\", \"w\": 1 } ]", "[ { \"shape\": \"late\", \"w\": 0 }, { \"shape\": \"early\", \"w\": 0 } ]"),
        ContainsSubstring ("hook.contours"));
    CHECK_THAT (hookRefused ("\"peak\": [18, 28]", "\"peak\": [18, 32]"), ContainsSubstring ("hook.pitch.peak"));
    CHECK_THAT (hookRefused ("\"early\": [4, 12]", "\"early\": [-1, 12]"), ContainsSubstring ("hook.pitch.early"));

    // cells: 16ths 0-15 ascending; a length per onset, 1-16; accents among the onsets; an onset in each
    // half; a non-accent onset (in a style without a pedal); a weight that can be drawn
    const std::string cell0 = "{ \"on\": [0, 3, 6, 8, 11, 14], \"len\": [2, 2, 3, 2, 2, 2], \"acc\": [0, 6, 8], \"w\": 30 }";
    CHECK_THAT (hookRefused (cell0, "{ \"on\": [0, 3, 6, 8, 11, 16], \"len\": [2, 2, 3, 2, 2, 2], \"acc\": [0, 6, 8], \"w\": 30 }"), ContainsSubstring ("hook.cells[0].on"));
    CHECK_THAT (hookRefused (cell0, "{ \"on\": [0, 3, 6, 8, 14, 11], \"len\": [2, 2, 3, 2, 2, 2], \"acc\": [0, 6, 8], \"w\": 30 }"), ContainsSubstring ("hook.cells[0].on"));
    CHECK_THAT (hookRefused (cell0, "{ \"on\": [0, 3, 6, 8, 11, 14], \"len\": [2, 2, 3, 2, 2], \"acc\": [0, 6, 8], \"w\": 30 }"), ContainsSubstring ("hook.cells[0].len"));
    CHECK_THAT (hookRefused (cell0, "{ \"on\": [0, 3, 6, 8, 11, 14], \"len\": [2, 2, 3, 2, 2, 17], \"acc\": [0, 6, 8], \"w\": 30 }"), ContainsSubstring ("hook.cells[0].len"));
    CHECK_THAT (hookRefused (cell0, "{ \"on\": [0, 3, 6, 8, 11, 14], \"len\": [2, 2, 3, 2, 2, 2], \"acc\": [0, 6, 9], \"w\": 30 }"), ContainsSubstring ("hook.cells[0].acc"));
    const std::string cell3 = "{ \"on\": [2, 6, 10, 13], \"len\": [2, 2, 2, 2], \"acc\": [6, 10], \"w\": 14 }";
    CHECK_THAT (hookRefused (cell3, "{ \"on\": [2, 4, 5, 6], \"len\": [2, 1, 1, 2], \"acc\": [6], \"w\": 14 }"), ContainsSubstring ("hook.cells[3]")); // nothing in 8-15
    CHECK_THAT (hookRefused (cell3, "{ \"on\": [2, 6, 10, 13], \"len\": [2, 2, 2, 2], \"acc\": [2, 6, 10, 13], \"w\": 14 }"), ContainsSubstring ("hook.cells[3]")); // all accents
    const std::string cells = "\"cells\": [\n"
                              "      { \"on\": [0, 3, 6, 8, 11, 14], \"len\": [2, 2, 3, 2, 2, 2], \"acc\": [0, 6, 8], \"w\": 30 },\n"
                              "      { \"on\": [0, 3, 6, 10, 12], \"len\": [2, 2, 3, 2, 2], \"acc\": [0, 6, 10], \"w\": 16 },\n"
                              "      { \"on\": [6, 9, 10, 15], \"len\": [2, 1, 3, 1], \"acc\": [6, 10], \"w\": 14 },\n"
                              "      { \"on\": [2, 6, 10, 13], \"len\": [2, 2, 2, 2], \"acc\": [6, 10], \"w\": 14 },\n"
                              "      { \"on\": [0, 3, 7, 8, 11, 14], \"len\": [2, 2, 1, 2, 2, 2], \"acc\": [0, 8, 14], \"w\": 12 },\n"
                              "      { \"on\": [0, 3, 6, 9, 12], \"len\": [2, 2, 2, 2, 3], \"acc\": [0, 6, 12], \"w\": 14 }\n"
                              "    ],";
    std::string zeroCells = cells;
    for (const std::string w : { "\"w\": 30 }", "\"w\": 16 }", "\"w\": 14 }", "\"w\": 14 }", "\"w\": 12 }", "\"w\": 14 }" })
        zeroCells = replaced (zeroCells, w, "\"w\": 0 }");
    CHECK_THAT (hookRefused (cells, zeroCells), ContainsSubstring ("hook.cells"));

    // mutate: percentages, extra onsets on 16ths 0-15; answer: a weight that can be drawn
    CHECK_THAT (hookRefused ("\"drop\": 25, \"add\": 25", "\"drop\": 125, \"add\": 25"), ContainsSubstring ("hook.mutate.drop"));
    CHECK_THAT (hookRefused ("\"extra\": [2, 7, 10, 13, 15]", "\"extra\": [2, 7, 10, 13, 16]"), ContainsSubstring ("hook.mutate.extra"));
    CHECK_THAT (hookRefused ("\"same\": 4, \"move\": 3, \"add\": 2, \"drop\": 1", "\"same\": 0, \"move\": 0, \"add\": 0, \"drop\": 0"), ContainsSubstring ("hook.answer"));

    // pitch: pool degrees 0-6, once each, holding every degree start and end can draw; 7 weights
    // each for start and end, 8 for moves; a leap of 1-12 semitones; strong onsets "accents" or
    // "beats"; tones and clash scale steps 0-8
    CHECK_THAT (hookRefused ("\"pool\": [0, 1, 2, 3, 4, 6]", "\"pool\": [0, 1, 2, 3, 4, 7]"), ContainsSubstring ("hook.pitch.pool"));
    CHECK_THAT (hookRefused ("\"pool\": [0, 1, 2, 3, 4, 6]", "\"pool\": [0, 1, 2, 3, 4, 4]"), ContainsSubstring ("hook.pitch.pool"));
    CHECK_THAT (hookRefused ("\"pool\": [0, 1, 2, 3, 4, 6]", "\"pool\": [0, 1, 2, 3, 4]"), ContainsSubstring ("hook.pitch")); // start and end can draw the b7
    CHECK_THAT (hookRefused ("\"start\": [40, 0, 20, 0, 30, 0, 10]", "\"start\": [40, 0, 20, 0, 30, 0]"), ContainsSubstring ("hook.pitch.start"));
    CHECK_THAT (hookRefused ("\"start\": [40, 0, 20, 0, 30, 0, 10]", "\"start\": [0, 0, 0, 0, 0, 0, 0]"), ContainsSubstring ("hook.pitch.start"));
    CHECK_THAT (hookRefused ("\"end\": [4, 25, 20, 0, 35, 0, 20]", "\"end\": [0, 0, 0, 0, 0, 0, 0]"), ContainsSubstring ("hook.pitch.end"));
    CHECK_THAT (hookRefused ("\"moves\": [10, 45, 27, 9, 9, 0, 0, 0]", "\"moves\": [10, 45, 27, 9, 9, 0, 0]"), ContainsSubstring ("hook.pitch.moves"));
    CHECK_THAT (hookRefused ("\"leap\": 9,", "\"leap\": 13,"), ContainsSubstring ("hook.pitch.leap"));
    CHECK_THAT (hookRefused ("\"leap\": 9,", "\"leap\": 0,"), ContainsSubstring ("hook.pitch.leap"));
    CHECK_THAT (hookRefused ("\"strong\": \"accents\"", "\"strong\": \"loud\""), ContainsSubstring ("hook.pitch.strong"));
    CHECK_THAT (hookRefused ("\"tones\": [0, 2, 4, 6, 8]", "\"tones\": [0, 2, 4, 6, 9]"), ContainsSubstring ("hook.pitch.tones"));
    CHECK_THAT (hookRefused ("\"clash\": [0, 2, 4, 6]", "\"clash\": [0, 2, 4, 9]"), ContainsSubstring ("hook.pitch.clash"));

    // vel: 1-127, a peak raise of 20 at most, a shape of 12 at most; guard.second 0-127
    CHECK_THAT (hookRefused ("\"down\": 124,", "\"down\": 128,"), ContainsSubstring ("hook.vel.down"));
    CHECK_THAT (hookRefused ("\"other\": 88,", "\"other\": 0,"), ContainsSubstring ("hook.vel.other"));
    CHECK_THAT (hookRefused ("\"peak\": 6,", "\"peak\": 21,"), ContainsSubstring ("hook.vel.peak"));
    CHECK_THAT (hookRefused ("\"shape\": 4 }", "\"shape\": 13 }"), ContainsSubstring ("hook.vel.shape"));
    CHECK_THAT (hookRefused ("\"second\": 68", "\"second\": 128"), ContainsSubstring ("hook.guard.second"));

    // period: organ, glass or stab; halves in 8-15, ascending; an octave of -1 to 1; no cadence on the
    // tonic; a glass register inside the kit range (68-86 here); a stab idiom with its cells
    CHECK_THAT (hookRefused ("\"idiom\": \"organ\"", "\"idiom\": \"piano\""), ContainsSubstring ("hook.period.idiom"));
    CHECK_THAT (hookRefused ("{ \"on\": [8, 10, 13], \"w\": 2 }", "{ \"on\": [7, 10, 13], \"w\": 2 }"), ContainsSubstring ("hook.period.halves[0].on"));
    CHECK_THAT (hookRefused ("{ \"on\": [8, 10, 13], \"w\": 2 }", "{ \"on\": [8, 13, 10], \"w\": 2 }"), ContainsSubstring ("hook.period.halves[0].on"));
    CHECK_THAT (hookRefused ("\"octave\": 0,", "\"octave\": 2,"), ContainsSubstring ("hook.period.octave"));
    CHECK_THAT (hookRefused ("\"cadence\": [0, 2, 1, 0, 2, 0, 0]", "\"cadence\": [1, 2, 1, 0, 2, 0, 0]"), ContainsSubstring ("hook.period.cadence"));
    const std::string period = "\"period\": {\n"
                               "      \"idiom\": \"organ\",\n"
                               "      \"halves\": [ { \"on\": [8, 10, 13], \"w\": 2 }, { \"on\": [8, 10], \"w\": 1 }, { \"on\": [8, 11, 13], \"w\": 1 } ],\n"
                               "      \"octave\": 0,\n"
                               "      \"cadence\": [0, 2, 1, 0, 2, 0, 0]\n"
                               "    },";
    CHECK_THAT (hookRefused (period, "\"period\": { \"idiom\": \"glass\", \"register\": [76, 89], \"halves\": [ { \"on\": [10, 14], \"w\": 1 } ] },"), ContainsSubstring ("register"));
    CHECK_THAT (hookRefused (period, "\"period\": { \"idiom\": \"stab\" },"), ContainsSubstring ("hook.period"));

    // a pedal (FUT02's ChromeArp): its cells start on 16th 0 and may be all accents; no extra onset on
    // 16th 15; degrees -7 to 7, velocities 1-127, under full, answer, fragment and new only, 1 or 2
    // pedal notes after a top note
    const std::string pedalCells = "\"cells\": [\n"
                                   "      { \"on\": [0, 3, 6, 8, 11, 14], \"len\": [2, 2, 2, 2, 2, 2], \"acc\": [0, 3, 6, 8, 11, 14], \"w\": 30 },\n"
                                   "      { \"on\": [0, 3, 6, 10, 14], \"len\": [2, 2, 3, 2, 2], \"acc\": [0, 6, 10], \"w\": 25 }\n"
                                   "    ],";
    Texts pedal = hook;
    pedal.style = replaced (pedal.style, cells, pedalCells);
    pedal.style = replaced (pedal.style, "\"extra\": [2, 7, 10, 13, 15]", "\"extra\": [2, 7, 10, 13]");
    pedal.style = replaced (pedal.style, "\"guard\": { \"second\": 68 },",
        "\"guard\": { \"second\": 68 },\n    \"pedal\": { \"degrees\": [0, -3], \"vel\": [80, 66], \"in\": [\"full\", \"answer\", \"fragment\", \"new\"], \"max\": 2 },");
    INFO (bundleError (pedal));
    CHECK (bundleError (pedal).empty());
    CHECK_THAT (refused (pedal, "\"degrees\": [0, -3]", "\"degrees\": [0, -8]"), ContainsSubstring ("hook.pedal.degrees"));
    CHECK_THAT (refused (pedal, "\"vel\": [80, 66]", "\"vel\": [80, 0]"), ContainsSubstring ("hook.pedal.vel"));
    CHECK_THAT (refused (pedal, "\"fragment\", \"new\"]", "\"fragment\", \"skeleton\"]"), ContainsSubstring ("hook.pedal.in"));
    CHECK_THAT (refused (pedal, "\"max\": 2 }", "\"max\": 3 }"), ContainsSubstring ("hook.pedal.max"));
    CHECK_THAT (refused (pedal, "\"extra\": [2, 7, 10, 13]", "\"extra\": [2, 7, 10, 13, 15]"), ContainsSubstring ("hook.mutate.extra"));
    CHECK_THAT (refused (pedal, "{ \"on\": [0, 3, 6, 10, 14], \"len\": [2, 2, 3, 2, 2], \"acc\": [0, 6, 10], \"w\": 25 }",
                    "{ \"on\": [2, 3, 6, 10, 14], \"len\": [2, 2, 3, 2, 2], \"acc\": [6, 10], \"w\": 25 }"),
        ContainsSubstring ("hook.cells[1]"));

    // plan: known ops; a menu for each kind the carrier's plans play it in and for no other; no VARY,
    // CADENCE or BREATH in a first menu; no FULL or NEW in a withheld kind before the first drop;
    // "continue" only in an after build; a drop's and drop 2's cover, 500 or more and reachable at
    // every length they take (900 of drop 1's 3 lines: 2,700 > 2,000); drop 2's first menu the new
    // phrase; a kind before the first drop that always plants the hook (here the groove)
    const std::string grooveFirst = "\"first\": [ { \"op\": \"fragment\", \"w\": 3 }, { \"op\": \"skeleton\", \"w\": 1 } ],";
    CHECK_THAT (hookRefused (grooveFirst, "\"first\": [ { \"op\": \"fragment\", \"w\": 3 }, { \"op\": \"skeletal\", \"w\": 1 } ],"), ContainsSubstring ("hook.plan.groove.first[1].op"));
    CHECK_THAT (hookRefused ("\"plan\": {\n      \"groove\": {", "\"plan\": {\n      \"intro\": { \"first\": [ { \"op\": \"out\", \"w\": 1 } ] },\n      \"groove\": {"),
        ContainsSubstring ("intro"));
    CHECK_THAT (hookRefused ("      \"build\": { \"first\": [ { \"op\": \"out\", \"w\": 1 } ] },\n", ""), ContainsSubstring ("build"));
    const std::string dropFirst = "\"first\": [ { \"op\": \"skeleton\", \"w\": 2 }, { \"op\": \"full\", \"w\": 1 } ],";
    CHECK_THAT (hookRefused (dropFirst, "\"first\": [ { \"op\": \"skeleton\", \"w\": 2 }, { \"op\": \"vary\", \"w\": 1 } ],"), ContainsSubstring ("hook.plan.drop.first"));
    CHECK_THAT (hookRefused (dropFirst, "\"first\": [ { \"op\": \"breath\", \"w\": 1 } ],"), ContainsSubstring ("hook.plan.drop.first"));
    CHECK_THAT (hookRefused ("{ \"op\": \"answer\", \"w\": 1 }, { \"op\": \"skeleton\", \"w\": 1 } ] },", "{ \"op\": \"full\", \"w\": 1 }, { \"op\": \"skeleton\", \"w\": 1 } ] },"),
        ContainsSubstring ("hook.plan.groove.next"));
    CHECK_THAT (hookRefused ("\"next\": [ { \"op\": \"fragment\", \"w\": 1 } ],", "\"next\": [ { \"op\": \"new\", \"w\": 1 } ],"), ContainsSubstring ("hook.plan.break.next"));
    CHECK_THAT (hookRefused ("\"build\": { \"first\": [ { \"op\": \"out\", \"w\": 1 } ] },", "\"build\": { \"continue\": 1 },"), ContainsSubstring ("continue"));
    CHECK_THAT (hookRefused ("\"holdFull\": 1,\n        \"cover\": 600 },", "\"holdFull\": 1 },"), ContainsSubstring ("hook.plan.drop"));
    CHECK_THAT (hookRefused ("\"cover\": 600 },", "\"cover\": 400 },"), ContainsSubstring ("hook.plan.drop.cover"));
    CHECK_THAT (hookRefused ("\"cover\": 600 },", "\"cover\": 900 },"), ContainsSubstring ("hook.plan.drop.cover"));
    CHECK_THAT (hookRefused ("\"first\": [ { \"op\": \"new\", \"w\": 1 } ],", "\"first\": [ { \"op\": \"full\", \"w\": 1 } ],"), ContainsSubstring ("hook.plan.drop2.first"));
    CHECK_THAT (hookRefused (grooveFirst, "\"first\": [ { \"op\": \"out\", \"w\": 1 }, { \"op\": \"fragment\", \"w\": 3 } ],"), ContainsSubstring ("hook.plan"));
    // and a misspelt key anywhere in the block is an unknown key, never a silent default
    CHECK_THAT (hookRefused ("\"headDiffers\": 1", "\"headDifers\": 1"), ContainsSubstring ("unknown key \"headDifers\""));
}
