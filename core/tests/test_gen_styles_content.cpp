// Tests for the FUT02 "Orbit Relay" (sci-fi club) and FUT04 "Furnace Code" (techno) content, and for
// the loader rules of the engine features those styles use: the arp lane, CC lanes, the bass's duck,
// per-step slides and mutations, the feel's tight rule, and one-chord (pedal) harmony.

#include "gen/content.h"
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
        { "FUT04", { "kick", "rumble", "snare", "hats", "perc", "bass", "chords" } },
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
    CHECK (a.style.lanes[*a.style.laneIndex ("arp")].kind == LaneKind::arp);
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
    // no artist names and no style tags in the names: plain words for what the lane plays
    for (const Bundle* b : { &a, &t })
        for (const auto& lane : b->kit.lanes)
        {
            CHECK_FALSE (lane.track.empty());
            CHECK (lane.track.size() <= 24);
        }
}

TEST_CASE ("styles: the new default forms are DJ-friendly extended mixes of 6 to 8 minutes", "[gen][styles][content]")
{
    for (const char* id : { "FUT02", "FUT04" })
    {
        INFO (id);
        const Bundle b = bundleOf (id);
        const Style& style = b.style;
        std::int32_t bars = 0;
        for (const auto& s : style.form)
        {
            CHECK (s.bars % 8 == 0);
            CHECK (s.blocks.size() == static_cast<std::size_t> (s.bars / 8));
            bars += s.bars;
        }
        const std::int32_t seconds = bars * 240 / b.kit.bpm;
        CHECK (seconds >= 6 * 60);
        CHECK (seconds <= 8 * 60);
        REQUIRE (style.form.size() >= 3);
        CHECK (style.form.front().kind == "intro");
        CHECK (style.form.front().bars == 32);
        CHECK (style.form.back().kind == "outro");
        CHECK (style.form.back().bars == 32);
        const auto isDrums = [&] (const std::string& lane) { return style.lanes[*style.laneIndex (lane)].kind == LaneKind::drums; };
        for (const auto& lane : style.form.front().blocks.front())
            CHECK (isDrums (lane)); // the mix-in starts with drums only...
        for (const auto& lane : style.form.back().blocks.back())
            CHECK (isDrums (lane)); // ...and the mix-out ends with drums only
        for (std::size_t i = 1; i < style.form.front().blocks.size(); ++i)
            CHECK (style.form.front().blocks[i].size() > style.form.front().blocks[i - 1].size()); // the intro adds lanes block by block
        for (std::size_t i = 1; i < style.form.back().blocks.size(); ++i)
            CHECK (style.form.back().blocks[i].size() < style.form.back().blocks[i - 1].size()); // the outro takes them away
        std::set<std::string> kinds;
        for (const auto& s : style.form)
            kinds.insert (s.kind);
        CHECK (kinds == std::set<std::string> { "intro", "groove", "build", "drop", "break", "drop2", "outro" });
        // the kick enters first and leaves last: a DJ mixes on it
        CHECK (std::find (style.form.front().blocks.front().begin(), style.form.front().blocks.front().end(), "kick") != style.form.front().blocks.front().end());
        CHECK (std::find (style.form.back().blocks.back().begin(), style.form.back().blocks.back().end(), "kick") != style.form.back().blocks.back().end());
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
    for (const auto& sec : t.style.form)
        for (const auto& block : sec.blocks)
            if (std::find (block.begin(), block.end(), "bass") != block.end())
                CHECK (bass.cc.front().row (sec.kind) != nullptr);
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
    const LaneSpec& arp = a.style.lanes[*a.style.laneIndex ("arp")];
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
    const Texts sciclub = texts ("fut02", "sciclub");
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
    CHECK_THAT (withKit (sciclub, "\"arp\": { \"track\": \"Arp Chrome\", \"channel\": 3, \"range\": [62, 86] }", "\"arp\": { \"track\": \"Arp Chrome\", \"channel\": 3, \"notes\": { \"zap\": 47 } }"),
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
