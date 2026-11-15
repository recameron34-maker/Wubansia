// The hook (S1b2b, docs/plans/S1b2b-plan.md, "Tests first"): one per seed, a 2-bar riff drawn
// rhythm first and pitched key-relative, drop 2's new phrase in each kit's written idiom and
// ANSWER's response; the line plan over each seed's drawn form; the realized lines on the carrier
// lane ('Lead Pluck', 'Lead Keys', 'Lead Blips', and FUT02's arp as the written ChromeArp).
// Numbered as the plan's list, 1-26.
//
// Unless a test names its kits, it runs on the four shipped kits and on the hook fixture
// (core/tests/fixtures/hook: FUT01 with the plan's lead lane and hook block), one section each.
// The shipped kits' sections fail at "the style has a hook block" until the content commit gives
// their styles one; the fixture's fail until the loader reads its block and the hook's code lands.
// "1,024 seeds" are seeds 0-1023 and the snapshot seeds past them (65536, 123456789, 2^64 - 1).
//
// The tests read the songs, not the code: each texture's notes (textureBar), the plan's line rule
// (lineRule) and G4's reading are written out here on their own, from the plan's definitions.

#include "gen/content.h"
#include "gen/export.h"
#include "gen/harmony.h"
#include "gen/hook.h"
#include "gen/json.h"
#include "gen/song.h"
#include "gen/theory.h"
#include "rng/rng.h"
#include "util/file_io.h"
#include "util/ints.h"
#include "util/sha256.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iterator>
#include <map>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace wubansia;
using namespace wubansia::gen;

namespace
{
    namespace fs = std::filesystem;

    const fs::path sourceDir = WUBANSIA_SOURCE_DIR;
    const fs::path contentDir = sourceDir / "content";
    const fs::path fixtureDir = sourceDir / "core" / "tests" / "fixtures";

    constexpr std::uint64_t seeds = 64; // per kit, unless a test says otherwise
    constexpr std::int32_t lineBars = barsPerBlock;

    // ------------------------------------------------------------------ the kits and their songs

    // A kit to test: the shipped content's, or a fixture's.
    struct KitRef
    {
        std::string label; // what a failure names
        fs::path dir; // its content folder
        std::string id;
    };

    const std::vector<KitRef>& shippedKits()
    {
        static const std::vector<KitRef> kits = {
            { "FUT01", contentDir, "FUT01" },
            { "FUT02", contentDir, "FUT02" },
            { "FUT03", contentDir, "FUT03" },
            { "FUT04", contentDir, "FUT04" },
        };
        return kits;
    }

    const KitRef& hookFixture()
    {
        static const KitRef kit { "the hook fixture (FUT01, core/tests/fixtures/hook)", fixtureDir / "hook", "FUT01" };
        return kit;
    }

    // FUT02 before the hook (core/tests/fixtures/nohook): sciclub with its arp lane of kind arp and no
    // hook block.
    const KitRef& arpFixture()
    {
        static const KitRef kit { "FUT02 before the hook (core/tests/fixtures/nohook)", fixtureDir / "nohook", "FUT02" };
        return kit;
    }

    // The kits a test without named kits runs on: the shipped ones, then the hook fixture.
    std::vector<KitRef> hookKits()
    {
        std::vector<KitRef> kits = shippedKits();
        kits.push_back (hookFixture());
        return kits;
    }

    const KitRef& shippedKit (std::string_view id)
    {
        for (const KitRef& k : shippedKits())
            if (k.id == id)
                return k;
        FAIL ("no shipped kit " << id);
        return shippedKits().front();
    }

    const Bundle& bundleOf (const KitRef& k)
    {
        static std::map<std::string, Bundle, std::less<>> cache;
        auto it = cache.find (k.label);
        if (it == cache.end())
        {
            auto b = loadBundle (k.dir, k.id);
            INFO (k.label << ": " << b.error);
            REQUIRE (b.ok());
            it = cache.emplace (k.label, std::move (b.value)).first;
        }
        return it->second;
    }

    // The kit's hook block: every kit these tests run on has one (the shipped ones from the content
    // commit on).
    const HookSpec& hookOf (const KitRef& k)
    {
        const Bundle& b = bundleOf (k);
        INFO (k.label << ": the style has a hook block");
        REQUIRE (b.style.hook.has_value());
        return *b.style.hook;
    }

    // The carrier's kit lane: its track, channel and range.
    const KitLane& carrierOf (const KitRef& k)
    {
        const KitLane* kl = bundleOf (k).kit.lane (hookOf (k).lane);
        INFO (k.label << ": the kit has the carrier's lane");
        REQUIRE (kl != nullptr);
        return *kl;
    }

    Song songOf (const Bundle& b, std::uint64_t seed, std::int32_t bars = 0)
    {
        Options o;
        o.seed = seed;
        o.bars = bars;
        auto s = generate (b, o);
        INFO ("seed " << seed << ": " << s.error);
        REQUIRE (s.ok());
        return std::move (s.value);
    }

    // A song of a kit with a hook block, which has its hook.
    Song hookSong (const KitRef& k, std::uint64_t seed, std::int32_t bars = 0)
    {
        (void) hookOf (k);
        Song s = songOf (bundleOf (k), seed, bars);
        INFO (k.label << " seed " << seed << ": the song has its hook (planHook)");
        REQUIRE (s.hook.has_value());
        return s;
    }

    // The kit's songs of seeds 0-63, generated once.
    const std::vector<Song>& songsOf (const KitRef& k)
    {
        static std::map<std::string, std::vector<Song>, std::less<>> cache;
        auto it = cache.find (k.label);
        if (it == cache.end())
        {
            std::vector<Song> songs;
            for (std::uint64_t seed = 0; seed < seeds; ++seed)
                songs.push_back (hookSong (k, seed));
            it = cache.emplace (k.label, std::move (songs)).first;
        }
        return it->second;
    }

    // Seeds 0-1023 and the snapshot seeds past them.
    std::vector<std::uint64_t> manySeeds()
    {
        std::vector<std::uint64_t> out;
        for (std::uint64_t seed = 0; seed < 1024; ++seed)
            out.push_back (seed);
        for (const std::uint64_t seed : { 65536ull, 123456789ull, 18446744073709551615ull })
            out.push_back (seed);
        return out;
    }

    std::size_t laneIndex (const Song& s, std::string_view id)
    {
        for (std::size_t i = 0; i < s.lanes.size(); ++i)
            if (s.lanes[i].id == id)
                return i;
        FAIL ("no lane " << id);
        return 0;
    }

    const LaneOut& leadOf (const Song& s) { return s.lanes[s.hook->lane]; }

    bool isPedal (const Note& n) { return (n.flags & noteFlag::pedal) != 0; }

    bool isDrop (std::string_view kind) { return kind == "drop" || kind == "drop2"; }

    // The section's index of the first drop, or the number of sections.
    std::size_t firstDropOf (const Song& s)
    {
        for (std::size_t i = 0; i < s.sections.size(); ++i)
            if (s.sections[i].kind == "drop")
                return i;
        return s.sections.size();
    }

    // A note's bar in its section, and its line's first bar there.
    std::int64_t barInSection (const Song& s, const Note& n) { return n.step / stepsPerBar - s.sections[static_cast<std::size_t> (n.section)].startBar; }
    std::int64_t lineOfNote (const Song& s, const Note& n) { return barInSection (s, n) / lineBars; }

    // The hook line a bar of a section is in, or nullptr.
    const HookLine* lineAt (const SongHook& h, std::size_t section, std::int64_t bar)
    {
        for (const HookLine& l : h.lines)
            if (l.section == section && bar >= l.bar && bar < l.bar + lineBars)
                return &l;
        return nullptr;
    }

    std::string_view formName (HookForm f)
    {
        switch (f)
        {
            case HookForm::out: return "out";
            case HookForm::full: return "full";
            case HookForm::fragment: return "fragment";
            case HookForm::skeleton: return "skeleton";
            case HookForm::answer: return "answer";
            case HookForm::augment: return "augment";
        }
        return "?";
    }

    std::string_view endingName (HookEnding e)
    {
        switch (e)
        {
            case HookEnding::none: return "none";
            case HookEnding::vary: return "vary";
            case HookEnding::cadence: return "cadence";
            case HookEnding::breath: return "breath";
        }
        return "?";
    }

    std::string described (const HookLine& l)
    {
        std::string out (formName (l.form));
        if (l.motif == 1)
            out += " of the new phrase";
        if (l.shift != 0)
            out += ", " + std::to_string (l.shift) + " steps up";
        if (l.ending != HookEnding::none)
            out += ", " + std::string (endingName (l.ending));
        return out;
    }

    std::string listed (const std::vector<std::string>& items, std::size_t limit = 10)
    {
        std::string out;
        for (std::size_t i = 0; i < items.size() && i < limit; ++i)
            out += "\n  " + items[i];
        if (items.size() > limit)
            out += "\n  ... and " + std::to_string (items.size() - limit) + " more";
        return out;
    }

    // ------------------------------------------------------------------ the material

    std::int32_t semitones (Mode mode, std::int32_t degree)
    {
        const auto steps = modeSteps (mode);
        return 12 * util::floorDiv (degree, 7) + steps[static_cast<std::size_t> (util::floorMod (degree, 7))];
    }

    std::int32_t keyOf (const SongHook& h, std::int32_t degree) { return h.base + semitones (h.mode, degree); }

    std::vector<HookNote> barNotes (const HookPhrase& p, std::int32_t bar)
    {
        std::vector<HookNote> out;
        for (const HookNote& n : p.notes)
            if (n.step / 16 == bar)
                out.push_back (n);
        return out;
    }

    std::set<std::int32_t> sixteenths (const std::vector<HookNote>& notes, std::int32_t from = 0, std::int32_t to = 16)
    {
        std::set<std::int32_t> out;
        for (const HookNote& n : notes)
            if (n.step % 16 >= from && n.step % 16 < to)
                out.insert (n.step % 16);
        return out;
    }

    // True if bar 1's notes are one of the cells with at most one non-accent onset out and at most one
    // extra onset in (one of `extra`), the accents the cell's.
    bool fromCell (const std::vector<HookNote>& bar, const std::vector<HookCell>& cells, const std::vector<std::int32_t>& extra)
    {
        std::set<std::int32_t> on, acc;
        for (const HookNote& n : bar)
        {
            on.insert (n.step % 16);
            if (n.accent)
                acc.insert (n.step % 16);
        }
        for (const HookCell& c : cells)
        {
            const std::set<std::int32_t> cellOn (c.on.begin(), c.on.end()), cellAcc (c.acc.begin(), c.acc.end());
            if (acc != cellAcc)
                continue;
            std::vector<std::int32_t> out, in;
            std::set_difference (cellOn.begin(), cellOn.end(), on.begin(), on.end(), std::back_inserter (out));
            std::set_difference (on.begin(), on.end(), cellOn.begin(), cellOn.end(), std::back_inserter (in));
            const bool allowed = in.empty() || std::find (extra.begin(), extra.end(), in.front()) != extra.end();
            if (out.size() <= 1 && in.size() <= 1 && allowed)
                return true;
        }
        return false;
    }

    // A note of a texture: its 16th in the bar, its degree, whether it is an accent.
    struct TextureNote
    {
        std::int32_t sixteenth = 0;
        std::int32_t degree = 0;
        bool accent = false;
    };

    // What bar `bar` (0-7) of a line plays of the material, as the plan's operator table defines each
    // texture, before the fit and the guard: FULL the phrase; FRAGMENT each bar's first half, and its
    // last onset when its second half holds two or more; SKELETON the accents; ANSWER the call, then
    // the response in each bar's second half; AUGMENT the riff at half speed; NEW (motif B) the new
    // phrase in full. VARY's and CADENCE's last note aside (bar 7 is free under the line rule);
    // BREATH rests bar 7.
    std::vector<TextureNote> textureBar (const SongHook& h, const HookLine& line, std::int32_t bar)
    {
        std::vector<TextureNote> out;
        if (line.form == HookForm::out || (line.ending == HookEnding::breath && bar == lineBars - 1))
            return out;
        const HookPhrase& p = line.motif == 1 ? h.b : h.a;
        if (p.bars <= 0)
            return out;
        const auto add = [&] (const HookNote& n, std::int32_t sixteenth) { out.push_back ({ sixteenth, n.degree + line.shift, n.accent }); };
        if (line.form == HookForm::augment)
        {
            // half speed: the line's bar b plays half b % 2 of the phrase's bar b / 2, its 16ths doubled
            const std::int32_t phraseBar = (bar / 2) % p.bars, half = bar % 2;
            for (const HookNote& n : barNotes (p, phraseBar))
                if (n.step % 16 / 8 == half)
                    add (n, (n.step % 16) * 2 - half * 16);
            return out;
        }
        const std::int32_t phraseBar = bar % p.bars;
        const auto notes = barNotes (p, phraseBar);
        switch (line.form)
        {
            case HookForm::full:
                for (const HookNote& n : notes)
                    add (n, n.step % 16);
                break;
            case HookForm::fragment:
            {
                std::int32_t second = 0;
                for (const HookNote& n : notes)
                    second += n.step % 16 >= 8 ? 1 : 0;
                for (std::size_t i = 0; i < notes.size(); ++i)
                    if (notes[i].step % 16 < 8 || (second >= 2 && i + 1 == notes.size()))
                        add (notes[i], notes[i].step % 16);
                break;
            }
            case HookForm::skeleton:
                for (const HookNote& n : notes)
                    if (n.accent)
                        add (n, n.step % 16);
                break;
            case HookForm::answer:
                for (const HookNote& n : notes)
                    if (n.step % 16 < 8)
                        add (n, n.step % 16);
                for (const HookNote& n : barNotes (h.response, phraseBar % std::max (1, h.response.bars)))
                    if (n.step % 16 >= 8)
                        add (n, n.step % 16);
                break;
            case HookForm::out:
            case HookForm::augment:
                break;
        }
        return out;
    }

    void tidy (BarPrint& p)
    {
        std::sort (p.begin(), p.end());
        p.erase (std::unique (p.begin(), p.end()), p.end());
    }

    // A line's bar prints of the material (keys from the material's degrees).
    LinePrints materialPrints (const SongHook& h, const HookLine& line)
    {
        LinePrints p;
        for (std::int32_t bar = 0; bar < lineBars; ++bar)
        {
            for (const TextureNote& n : textureBar (h, line, bar))
                p[static_cast<std::size_t> (bar)].push_back (n.sixteenth * 128 + keyOf (h, n.degree));
            tidy (p[static_cast<std::size_t> (bar)]);
        }
        return p;
    }

    // A line's bar prints of the lane's notes: G4 reads every note of the track, FUT02's pedal included.
    LinePrints lanePrints (const LaneOut& lane, const Section& sec, std::int32_t lineBar)
    {
        LinePrints p;
        const std::int64_t first = sec.startBar + lineBar;
        for (const Note& n : lane.score)
        {
            const std::int64_t b = n.step / stepsPerBar - first;
            if (b >= 0 && b < lineBars)
                p[static_cast<std::size_t> (b)].push_back (static_cast<std::int32_t> ((n.step % stepsPerBar) * 128 + n.key));
        }
        for (auto& bar : p)
            tidy (bar);
        return p;
    }

    // The plan's line rule, on its own: `after` differs from `before` in every one of bars 0-6, or in
    // none of them; bar 7 may differ either way.
    bool lineRule (const LinePrints& before, const LinePrints& after)
    {
        const bool first = before[0] != after[0];
        for (std::size_t b = 1; b + 1 < before.size(); ++b)
            if ((before[b] != after[b]) != first)
                return false;
        return true;
    }

    std::string differingBars (const LinePrints& before, const LinePrints& after)
    {
        std::string out;
        for (std::size_t b = 0; b < before.size(); ++b)
            if (before[b] != after[b])
                out += (out.empty() ? "bars " : " ") + std::to_string (b);
        return out.empty() ? "no bar" : out;
    }

    // The half-bars of a line its lane's notes keep busy: bit 2 * bar + half, set where a note starts
    // in the half-bar or sounds into it.
    std::uint16_t busyHalves (const LaneOut& lane, std::int64_t firstBar)
    {
        std::uint16_t mask = 0;
        for (std::int64_t half = 0; half < 2 * lineBars; ++half)
        {
            const std::int64_t from = firstBar * ticksPerBar + half * (ticksPerBar / 2), to = from + ticksPerBar / 2;
            if (std::any_of (lane.score.begin(), lane.score.end(), [&] (const Note& n) { return n.tick < to && n.tick + n.length > from; }))
                mask = static_cast<std::uint16_t> (mask | (1u << half));
        }
        return mask;
    }

    std::string maskString (std::uint16_t mask)
    {
        std::string out;
        for (std::uint32_t i = 0; i < 16; ++i)
            out += ((mask >> i) & 1u) != 0 ? '1' : '0';
        return out;
    }

    // The chord lanes' notes of each section (every lane of kind chords).
    std::map<std::int16_t, std::vector<const Note*>> chordNotesBySection (const Song& s)
    {
        std::map<std::int16_t, std::vector<const Note*>> out;
        for (const LaneOut& lane : s.lanes)
            if (lane.kind == LaneKind::chords)
                for (const Note& c : lane.score)
                    out[c.section].push_back (&c);
        return out;
    }

    double populationSd (const std::vector<std::int32_t>& v)
    {
        if (v.empty())
            return 0.0;
        double sum = 0, squares = 0;
        for (const auto x : v)
        {
            sum += x;
            squares += static_cast<double> (x) * x;
        }
        const double n = static_cast<double> (v.size()), mean = sum / n;
        return std::sqrt (std::max (0.0, squares / n - mean * mean));
    }

    // ------------------------------------------------------------------ fixture texts

    std::string readText (const fs::path& p)
    {
        const auto r = util::readBinaryFile (p);
        INFO (p.string());
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

    Texts hookFixtureTexts()
    {
        const fs::path dir = hookFixture().dir;
        return { readText (dir / "kits" / "fut01.json"), readText (dir / "styles" / "garage2step.json"), readText (dir / "feels" / "garage2step.json") };
    }

    Bundle bundleFromTexts (const Texts& t)
    {
        auto kit = parseKit (t.kit, "kit.json");
        INFO (kit.error);
        REQUIRE (kit.ok());
        auto style = parseStyle (t.style, "style.json");
        INFO (style.error);
        REQUIRE (style.ok());
        auto feel = parseFeel (t.feel, "feel.json");
        INFO (feel.error);
        REQUIRE (feel.ok());
        auto b = makeBundle (std::move (kit.value), std::move (style.value), std::move (feel.value));
        INFO (b.error);
        REQUIRE (b.ok());
        return std::move (b.value);
    }

    // ------------------------------------------------------------------ the 1,024-seed sweep

    // Tests 6, 10, 11 and 12 on 1,024 seeds: each song generated once, checked against all four and
    // dropped. Each broken rule is said in its list.
    struct Sweep
    {
        std::size_t songs = 0;
        std::vector<std::string> leaps, placement, held, rules;
        std::int64_t dropBars = 0, dropBarsWithHook = 0; // drop and drop-2 bars, pooled
        std::size_t r1 = 0, r2 = 0, r3 = 0; // the lines each DJ rule decided
    };

    void sweepLeaps (Sweep& w, const HookSpec& spec, const Song& s)
    {
        const SongHook& h = *s.hook;
        const std::string where = "seed " + std::to_string (s.seed) + ": ";
        // the material: every move, the riff's wrap and the new phrase's joins and wrap included
        for (const auto& [name, phrase] : { std::pair { "A", &h.a }, std::pair { "the new phrase", &h.b } })
            for (std::size_t i = 0; i < phrase->notes.size(); ++i)
            {
                const HookNote& x = phrase->notes[i];
                const HookNote& y = phrase->notes[(i + 1) % phrase->notes.size()];
                const std::int32_t move = std::abs (keyOf (h, y.degree) - keyOf (h, x.degree));
                if (move > spec.pitch.leap)
                    w.leaps.push_back (where + name + " moves " + std::to_string (move) + " semitones from its 16th " + std::to_string (x.step) + " to its 16th "
                                       + std::to_string (y.step));
            }
        // the realized FULL lines: consecutive top notes of one line, but across a rest of a bar or more
        const Note* prev = nullptr;
        for (const Note& n : leadOf (s).score)
        {
            const HookLine* line = lineAt (h, static_cast<std::size_t> (n.section), barInSection (s, n));
            if (isPedal (n) || line == nullptr || line->form != HookForm::full)
                continue;
            if (prev != nullptr && prev->section == n.section && lineOfNote (s, *prev) == lineOfNote (s, n) && n.tick - (prev->tick + prev->length) < ticksPerBar)
            {
                const int move = std::abs (int (n.key) - int (prev->key));
                if (move > spec.pitch.leap)
                    w.leaps.push_back (where + "the lead moves " + std::to_string (move) + " semitones into its note at tick " + std::to_string (n.tick) + " (bar "
                                       + std::to_string (n.step / stepsPerBar + 1) + ")");
            }
            prev = &n;
        }
    }

    void sweepPlacement (Sweep& w, const HookSpec& spec, const Song& s)
    {
        const SongHook& h = *s.hook;
        const std::string where = "seed " + std::to_string (s.seed) + ": ";
        const std::size_t firstDrop = firstDropOf (s);
        const std::int64_t dropTick = firstDrop < s.sections.size() ? s.sections[firstDrop].startTick() : s.endTick();
        std::set<std::int16_t> sections;
        std::set<std::int64_t> bars;
        bool early = false;
        for (const Note& n : leadOf (s).score)
        {
            sections.insert (n.section);
            bars.insert (n.step / stepsPerBar);
            early = early || n.tick < dropTick;
        }
        for (const std::int16_t i : sections)
        {
            const std::string& kind = s.sections[static_cast<std::size_t> (i)].kind;
            if (kind == "intro" || kind == "outro")
                w.placement.push_back (where + "hook notes in the " + kind);
        }
        for (const HookLine& l : h.lines)
        {
            const std::string& kind = s.sections[l.section].kind;
            if (kind == "intro" || kind == "outro")
                w.placement.push_back (where + "a hook line in the " + kind);
        }
        if (sections.size() < 3)
            w.placement.push_back (where + "hook notes in " + std::to_string (sections.size()) + " sections");
        if (!early)
            w.placement.push_back (where + "no hook note before the first drop");
        for (const Section& sec : s.sections)
        {
            if (!isDrop (sec.kind))
                continue;
            for (std::int32_t bar = 0; bar < sec.bars; ++bar)
            {
                ++w.dropBars;
                w.dropBarsWithHook += bars.contains (sec.startBar + bar) ? 1 : 0;
            }
            const HookMenu* menu = spec.menu (sec.kind, false);
            if (menu == nullptr)
            {
                w.placement.push_back (where + "no menu for the " + sec.kind);
                continue;
            }
            const std::int32_t lines = sec.bars / lineBars;
            std::int32_t playing = 0;
            for (const HookLine& l : h.lines)
                playing += l.section == sec.index && l.form != HookForm::out ? 1 : 0;
            const std::int32_t need = (menu->cover * lines + 999) / 1000;
            if (playing < need)
                w.placement.push_back (where + "the " + sec.kind + " at bar " + std::to_string (sec.startBar) + " plays " + std::to_string (playing) + " of its "
                                       + std::to_string (lines) + " lines, under its cover (" + std::to_string (menu->cover) + " permille: " + std::to_string (need) + ")");
        }
    }

    void sweepHeld (Sweep& w, const HookSpec& spec, const Song& s, bool plantsInBuild)
    {
        const SongHook& h = *s.hook;
        const std::string where = "seed " + std::to_string (s.seed) + ": ";
        const std::size_t firstDrop = firstDropOf (s);
        if (firstDrop >= s.sections.size())
        {
            w.held.push_back (where + "no drop");
            return;
        }
        const auto withheld = [&spec] (const std::string& kind) { return std::find (spec.withhold.begin(), spec.withhold.end(), kind) != spec.withhold.end(); };
        for (const HookLine& l : h.lines)
        {
            const Section& sec = s.sections[l.section];
            if (l.section < firstDrop && withheld (sec.kind) && l.form != HookForm::out && (l.form == HookForm::full || l.motif == 1))
                w.held.push_back (where + "the " + sec.kind + " at bar " + std::to_string (sec.startBar) + " plays " + described (l) + " before the first drop, in its line at bar "
                                  + std::to_string (l.bar));
        }
        const HookMenu* drop = spec.menu ("drop", false);
        if (drop != nullptr && drop->holdFull
            && std::none_of (h.lines.begin(), h.lines.end(), [&] (const HookLine& l) { return l.section == firstDrop && l.form == HookForm::full && l.motif == 0; }))
            w.held.push_back (where + "drop 1 holds no FULL line");
        for (const Section& sec : s.sections)
        {
            if (sec.kind != "drop2")
                continue;
            const HookLine* first = nullptr;
            for (const HookLine& l : h.lines)
                if (l.section == sec.index && l.form != HookForm::out && first == nullptr)
                    first = &l;
            if (first == nullptr)
                w.held.push_back (where + "the drop 2 at bar " + std::to_string (sec.startBar) + " plays no line");
            else if (first->motif != 1)
                w.held.push_back (where + "the drop 2 at bar " + std::to_string (sec.startBar) + " opens on " + described (*first) + ", not the new phrase");
        }
        // before drop 1, the hook is planted: by the groove's last line, or (FUT02) by the build
        if (plantsInBuild)
        {
            const bool plays = firstDrop > 0 && s.sections[firstDrop - 1].kind == "build"
                               && std::any_of (h.lines.begin(), h.lines.end(), [&] (const HookLine& l) { return l.section == firstDrop - 1 && l.form != HookForm::out; });
            if (!plays)
                w.held.push_back (where + "the build before drop 1 doesn't play the hook");
        }
        else
        {
            const Section* groove = nullptr;
            for (std::size_t i = 0; i < firstDrop; ++i)
                if (s.sections[i].kind == "groove")
                    groove = &s.sections[i];
            const HookLine* last = groove != nullptr ? lineAt (h, groove->index, groove->bars - 1) : nullptr;
            if (last == nullptr || last->form == HookForm::out)
                w.held.push_back (where + "the groove before drop 1 doesn't play the hook in its last line");
        }
    }

    void sweepRules (Sweep& w, const Song& s)
    {
        const SongHook& h = *s.hook;
        const LaneOut& lane = leadOf (s);
        const std::string where = "seed " + std::to_string (s.seed) + ": ";
        const auto plays = [] (const HookLine* l) { return l != nullptr && l->form != HookForm::out; };
        const auto has = [&] (std::size_t i) { return std::any_of (lane.score.begin(), lane.score.end(), [&] (const Note& n) { return n.section == static_cast<std::int16_t> (i); }); };
        const auto lastBarHas = [&] (std::size_t i) {
            const Section& sec = s.sections[i];
            const std::int64_t bar = sec.startBar + sec.bars - 1;
            return std::any_of (lane.score.begin(), lane.score.end(), [&] (const Note& n) { return n.section == static_cast<std::int16_t> (i) && n.step / stepsPerBar == bar; });
        };
        const std::size_t firstDrop = firstDropOf (s);
        for (std::size_t i = 1; i < s.sections.size(); ++i)
        {
            const Section& sec = s.sections[i];
            if (!isDrop (sec.kind))
                continue;
            // r2 (drop 1 only): a break that ended with the hook, a build without it: the drop opens without it
            const bool r2 = i == firstDrop && i >= 2 && s.sections[i - 1].kind == "build" && s.sections[i - 2].kind == "break" && lastBarHas (i - 2) && !has (i - 1);
            if (r2)
            {
                ++w.r2;
                if (plays (lineAt (h, i, 0)))
                    w.rules.push_back (where + "r2: drop 1 at bar " + std::to_string (sec.startBar) + " opens on the hook after a break that ended on it and a build without it");
            }
            // r1: the hook played the last bar into the drop: the drop's first line plays it
            else if (lastBarHas (i - 1))
            {
                ++w.r1;
                if (!plays (lineAt (h, i, 0)))
                    w.rules.push_back (where + "r1: the " + sec.kind + " at bar " + std::to_string (sec.startBar) + " opens without the hook that played into it");
            }
        }
        // r3: the line straight after a one-bar pull that takes the carrier plays, so the pull lands back
        const std::uint64_t carrier = std::uint64_t (1) << h.lane;
        for (const Section& sec : s.sections)
            for (const Dropout& d : sec.dropouts)
            {
                if ((d.lanes & carrier) == 0 || d.bar + 1 >= sec.bars)
                    continue;
                ++w.r3;
                if (!plays (lineAt (h, sec.index, d.bar + 1)))
                    w.rules.push_back (where + "r3: the pull at bar " + std::to_string (sec.startBar + d.bar) + " takes the lead out, and the line after it doesn't play");
            }
    }

    const Sweep& sweepOf (const KitRef& k)
    {
        static std::map<std::string, Sweep, std::less<>> cache;
        auto it = cache.find (k.label);
        if (it == cache.end())
        {
            const HookSpec& spec = hookOf (k);
            Sweep w;
            for (const std::uint64_t seed : manySeeds())
            {
                const Song s = hookSong (k, seed);
                ++w.songs;
                sweepLeaps (w, spec, s);
                sweepPlacement (w, spec, s);
                sweepHeld (w, spec, s, k.id == "FUT02");
                sweepRules (w, s);
            }
            it = cache.emplace (k.label, std::move (w)).first;
        }
        return it->second;
    }

    // ------------------------------------------------------------------ the bundle without its hook

    // The bundle without its hook block and its carrier (the last lane, so no other lane's index
    // moves), the carrier's place in the plans, and its kit and feel entries.
    Bundle withoutHook (Bundle b)
    {
        REQUIRE (b.style.hook.has_value());
        const std::string lane = b.style.hook->lane;
        REQUIRE_FALSE (b.style.lanes.empty());
        REQUIRE (b.style.lanes.back().id == lane);
        b.style.lanes.pop_back();
        for (KindPlan& p : b.style.form.plans)
        {
            std::erase (p.lanes, lane);
            for (BlockChoice& c : p.choices)
                for (auto& block : c.blocks)
                    std::erase (block, lane);
        }
        std::erase_if (b.kit.lanes, [&] (const KitLane& l) { return l.id == lane; });
        std::erase_if (b.feel.lanes, [&] (const auto& l) { return l.first == lane; });
        for (FeelRow& r : b.feel.rows)
            std::erase_if (r.lanes, [&] (const auto& l) { return l.first == lane; });
        b.style.hook.reset();
        return b;
    }

    // The first difference between two songs' lanes other than `skip` (and their arrangements), or "".
    std::string otherLanesDiffer (const Song& a, const Song& b, std::string_view skip)
    {
        if (a.sections.size() != b.sections.size())
            return "the form";
        for (std::size_t i = 0; i < a.sections.size(); ++i)
            if (a.sections[i].kind != b.sections[i].kind || a.sections[i].startBar != b.sections[i].startBar || a.sections[i].bars != b.sections[i].bars)
                return "section " + std::to_string (i + 1);
        for (const LaneOut& x : b.lanes)
        {
            if (x.id == skip)
                continue;
            const std::size_t ia = laneIndex (a, x.id), ib = laneIndex (b, x.id);
            for (std::size_t i = 0; i < a.sections.size(); ++i)
                for (std::int32_t bar = 0; bar < a.sections[i].bars; ++bar)
                    if (a.sections[i].active (ia, bar) != b.sections[i].active (ib, bar))
                        return x.id + "'s bars in section " + std::to_string (i + 1);
            const LaneOut& y = a.lanes[ia];
            if (y.score != x.score)
                return x.id + "'s score";
            if (y.perf != x.perf)
                return x.id + "'s performance";
            if (y.cc != x.cc)
                return x.id + "'s controllers";
            if (y.swing != x.swing || y.channel != x.channel || y.track != x.track)
                return x.id + "'s swing, channel or track";
        }
        return {};
    }
}

// ------------------------------------------------------------------ 1-5: the material

TEST_CASE ("hook 1: one hook per seed, the same bytes from the same seed", "[gen][hook]")
{
    // Two runs of a seed give equal hooks (the material, the lines, the notes and their masks); the
    // seeds' A phrases (16ths and degrees) are 60 or more distinct ones in 64 (prototype: 64).
    for (const KitRef& k : hookKits())
        DYNAMIC_SECTION (k.label)
        {
            std::vector<std::vector<std::pair<std::int32_t, std::int32_t>>> phrases;
            for (std::uint64_t seed = 0; seed < seeds; ++seed)
            {
                const Song& s = songsOf (k)[seed];
                const Song again = hookSong (k, seed);
                INFO (k.label << " seed " << seed);
                CHECK (*again.hook == *s.hook);
                std::vector<std::pair<std::int32_t, std::int32_t>> a;
                for (const HookNote& n : s.hook->a.notes)
                    a.emplace_back (n.step, n.degree);
                if (std::find (phrases.begin(), phrases.end(), a) == phrases.end())
                    phrases.push_back (a);
            }
            INFO (k.label << ": " << phrases.size() << " distinct A phrases in " << seeds << " seeds");
            CHECK (phrases.size() >= 60);
        }
}

TEST_CASE ("hook 2: rhythm first, bar 1 a style cell with one onset out and one in at most", "[gen][hook]")
{
    // Bar 1 of A is one of the style's cells, at most one non-accent onset out and one extra in (from
    // "mutate.extra"), its accents the cell's; each half of each bar holds an onset; each bar holds a
    // non-accent onset, but in a pedal style (FUT02), whose bars start on 16th 0 instead.
    for (const KitRef& k : hookKits())
        DYNAMIC_SECTION (k.label)
        {
            const HookSpec& spec = hookOf (k);
            for (const Song& s : songsOf (k))
            {
                const SongHook& h = *s.hook;
                INFO (k.label << " seed " << s.seed);
                REQUIRE (h.a.bars == 2);
                CHECK (fromCell (barNotes (h.a, 0), spec.cells, spec.mutate.extra));
                for (std::int32_t bar = 0; bar < 2; ++bar)
                {
                    const auto notes = barNotes (h.a, bar);
                    INFO ("bar " << bar + 1);
                    CHECK_FALSE (sixteenths (notes, 0, 8).empty());
                    CHECK_FALSE (sixteenths (notes, 8, 16).empty());
                    if (spec.pedal)
                        CHECK (sixteenths (notes).contains (0));
                    else
                        CHECK (std::any_of (notes.begin(), notes.end(), [] (const HookNote& n) { return !n.accent; }));
                }
            }
        }
}

TEST_CASE ("hook 3: bar 2 answers bar 1", "[gen][hook]")
{
    // Bar 2 keeps bar 1's 16ths 0-7 exactly and makes at most one edit among 8-15 (the same, an onset
    // moved by a 16th, one added, one dropped); it never transposes bar 1 (the same onsets and the
    // same degree intervals); with "headDiffers", its first half differs from bar 1's in a pitch.
    for (const KitRef& k : hookKits())
        DYNAMIC_SECTION (k.label)
        {
            const HookSpec& spec = hookOf (k);
            for (const Song& s : songsOf (k))
            {
                const SongHook& h = *s.hook;
                INFO (k.label << " seed " << s.seed);
                const auto one = barNotes (h.a, 0), two = barNotes (h.a, 1);
                CHECK (sixteenths (one, 0, 8) == sixteenths (two, 0, 8));
                const auto tail1 = sixteenths (one, 8, 16), tail2 = sixteenths (two, 8, 16);
                std::vector<std::int32_t> dropped, added;
                std::set_difference (tail1.begin(), tail1.end(), tail2.begin(), tail2.end(), std::back_inserter (dropped));
                std::set_difference (tail2.begin(), tail2.end(), tail1.begin(), tail1.end(), std::back_inserter (added));
                const bool oneEdit = dropped.size() + added.size() <= 1 || (dropped.size() == 1 && added.size() == 1 && std::abs (dropped.front() - added.front()) == 1);
                CHECK (oneEdit);
                if (sixteenths (one) == sixteenths (two) && one.size() > 1)
                {
                    std::vector<std::int32_t> i1, i2;
                    for (std::size_t i = 1; i < one.size(); ++i)
                    {
                        i1.push_back (one[i].degree - one[i - 1].degree);
                        i2.push_back (two[i].degree - two[i - 1].degree);
                    }
                    CHECK (i1 != i2); // not bar 1 moved up or down, nor repeated
                }
                if (spec.answer.headDiffers)
                {
                    std::vector<std::int32_t> d1, d2;
                    for (const HookNote& n : one)
                        if (n.step % 16 < 8)
                            d1.push_back (n.degree);
                    for (const HookNote& n : two)
                        if (n.step % 16 < 8)
                            d2.push_back (n.degree);
                    CHECK (d1 != d2);
                }
            }
        }
}

TEST_CASE ("hook 4: the walk", "[gen][hook]")
{
    // Every material note inside its register (A and the response in the carrier's kit range, from
    // the register's floor where it has one; the new phrase in its own register where its idiom has
    // one); the late contour's peak is the highest note alone, in the "peak" window (16ths 18-28 of the
    // phrase), the early one's in the "early" window (4-12), and a twin's top is struck on two onsets
    // in a row; at least 56 of 64 riffs end off the tonic (prototype 58-63); at least 850 permille of
    // the strong lead notes are chord tones (1 3 5 7 9) of the chord sounding, counted as the
    // prototype counted them: every note of the carrier on a beat, FUT02's pedal included (prototype
    // 895, 939, 905, 1000).
    for (const KitRef& k : hookKits())
        DYNAMIC_SECTION (k.label)
        {
            const HookSpec& spec = hookOf (k);
            const Range range = carrierOf (k).range;
            const std::int32_t lo = spec.reg.floor >= 0 ? spec.reg.floor : range.lo, hi = range.hi;
            std::size_t offTonic = 0, strong = 0, tones = 0;
            for (const Song& s : songsOf (k))
            {
                const SongHook& h = *s.hook;
                INFO (k.label << " seed " << s.seed);
                for (const HookPhrase* p : { &h.a, &h.response })
                    for (const HookNote& n : p->notes)
                    {
                        INFO ((p == &h.a ? "A" : "the response") << "'s 16th " << n.step);
                        CHECK (keyOf (h, n.degree) >= lo);
                        CHECK (keyOf (h, n.degree) <= hi);
                    }
                const Range bRange = spec.period.range.value_or (Range { lo, hi });
                for (const HookNote& n : h.b.notes)
                {
                    INFO ("the new phrase's 16th " << n.step);
                    CHECK (keyOf (h, n.degree) >= bRange.lo);
                    CHECK (keyOf (h, n.degree) <= bRange.hi);
                }
                // the contour
                REQUIRE_FALSE (h.a.notes.empty());
                std::int32_t top = h.a.notes.front().degree;
                for (const HookNote& n : h.a.notes)
                    top = std::max (top, n.degree);
                std::vector<std::size_t> tops;
                for (std::size_t i = 0; i < h.a.notes.size(); ++i)
                    if (h.a.notes[i].degree == top)
                        tops.push_back (i);
                switch (h.contour)
                {
                    case HookContour::late:
                    case HookContour::early:
                    {
                        const Range window = h.contour == HookContour::late ? spec.pitch.peak : spec.pitch.early;
                        INFO ((h.contour == HookContour::late ? "late" : "early") << " contour: the top at 16th " << h.a.notes[tops.front()].step);
                        CHECK (tops.size() == 1);
                        CHECK (h.a.notes[tops.front()].peak);
                        CHECK (h.a.notes[tops.front()].step >= window.lo);
                        CHECK (h.a.notes[tops.front()].step <= window.hi);
                        break;
                    }
                    case HookContour::twin:
                    {
                        bool twice = false;
                        for (std::size_t i = 0; i + 1 < tops.size(); ++i)
                            twice = twice || tops[i + 1] == tops[i] + 1;
                        INFO ("twin contour");
                        CHECK (twice);
                        break;
                    }
                }
                offTonic += util::floorMod (h.a.notes.back().degree, 7) != 0 ? 1u : 0u;
                // the realized notes on a beat against the chord sounding
                for (const Note& n : leadOf (s).score)
                {
                    if (n.step % 4 != 0)
                        continue;
                    const ChordSpan* c = chordAt (s, n.tick);
                    REQUIRE (c != nullptr);
                    bool tone = false;
                    for (const int t : { 0, 2, 4, 6, 8 })
                        tone = tone || pitchClass (c->rootPc + chordInterval (c->mode, c->degree, t)) == pitchClass (n.key);
                    ++strong;
                    tones += tone ? 1u : 0u;
                }
            }
            INFO (k.label << ": " << offTonic << " of " << seeds << " riffs end off the tonic; " << tones << " of " << strong << " strong lead notes are chord tones");
            CHECK (offTonic >= 56);
            CHECK (strong > 0);
            CHECK (tones * 1000 >= strong * 850);
        }
}

TEST_CASE ("hook 5: contours vary", "[gen][hook]")
{
    // The peak lands in bar 1 in at least 6 of 64 FUT01 seeds and 16 of 64 FUT03 seeds (prototype 12
    // and 29); a twin top in at least 12 of 64 FUT02 and FUT04 seeds (prototype 25 and 35).
    struct Want
    {
        const KitRef* kit;
        std::size_t barOne, twin;
    };
    for (const Want& w : { Want { &shippedKit ("FUT01"), 6, 0 }, Want { &shippedKit ("FUT02"), 0, 12 }, Want { &shippedKit ("FUT03"), 16, 0 },
             Want { &shippedKit ("FUT04"), 0, 12 }, Want { &hookFixture(), 6, 0 } })
        DYNAMIC_SECTION (w.kit->label)
        {
            std::size_t barOne = 0, twin = 0;
            for (const Song& s : songsOf (*w.kit))
            {
                const auto peak = std::find_if (s.hook->a.notes.begin(), s.hook->a.notes.end(), [] (const HookNote& n) { return n.peak; });
                INFO (w.kit->label << " seed " << s.seed);
                REQUIRE (peak != s.hook->a.notes.end());
                barOne += peak->step < 16 ? 1u : 0u;
                twin += s.hook->contour == HookContour::twin ? 1u : 0u;
            }
            INFO (w.kit->label << ": the peak in bar 1 in " << barOne << " seeds, a twin top in " << twin);
            CHECK (barOne >= w.barOne);
            CHECK (twin >= w.twin);
        }
}

// ------------------------------------------------------------------ 6, 10, 11, 12: on 1,024 seeds

TEST_CASE ("hook 6, 10, 11, 12: on 1,024 seeds, no leap over the cap, the placement, held back then paid off, and the DJ rules", "[gen][hook]")
{
    // 6, "no leap over the cap": the material's moves (A's and the new phrase's, their wraps and the
    //   period's joins included) and consecutive hook notes of a realized FULL line (the riff or the new
    //   phrase played whole, as the prototype counted them; FUT02's pedal aside), never further apart
    //   than the style's "leap", but across a rest of a bar or more (prototype: material clean after
    //   the redraws; FULL-line moves over the cap 16, 45, 11 and 0 in 1,024, which the ends' cap rule
    //   and the exception close).
    // 10, "placement, plan 5.1": hook notes in 3 sections or more and before the first drop; at least
    //   500 permille of the drop and drop-2 bars pooled; each drop at its cover floor, counted on the
    //   realized lines; none in an intro or outro.
    // 11, "held back, then paid off": no FULL or NEW in a withheld kind before the first drop; drop 1
    //   holds a FULL line where "holdFull"; drop 2's first playing line is the new phrase; before drop
    //   1, the groove's last line plays the hook (FUT01, FUT03, FUT04) or the build does (FUT02).
    // 12, "the DJ rules hold on the realized lines": r1 (the hook played the last bar into a drop: the
    //   drop's first line plays it), r2 (drop 1 only: a break ended with the hook and the build after it
    //   had none: drop 1 opens without it) and r3 (the line straight after a one-bar pull that takes
    //   the carrier plays); a line a rule makes play is never OUT. The generic form tests cover the
    //   lead as any lane.
    for (const KitRef& k : hookKits())
        DYNAMIC_SECTION (k.label)
        {
            const Sweep& w = sweepOf (k);
            INFO (k.label << ": " << w.songs << " songs");
            SECTION ("6. no leap over the cap")
            {
                INFO (w.leaps.size() << " moves over the cap:" << listed (w.leaps));
                CHECK (w.leaps.empty());
            }
            SECTION ("10. placement")
            {
                INFO (w.placement.size() << " misplaced:" << listed (w.placement));
                CHECK (w.placement.empty());
                INFO (w.dropBarsWithHook << " of " << w.dropBars << " drop and drop-2 bars hold the hook");
                CHECK (w.dropBarsWithHook * 1000 >= w.dropBars * 500);
            }
            SECTION ("11. held back, then paid off")
            {
                INFO (w.held.size() << " broken:" << listed (w.held));
                CHECK (w.held.empty());
            }
            SECTION ("12. the DJ rules")
            {
                INFO ("r1 decided " << w.r1 << " lines, r2 " << w.r2 << ", r3 " << w.r3 << "; broken " << w.rules.size() << ":" << listed (w.rules));
                CHECK (w.rules.empty());
            }
        }
}

TEST_CASE ("hook 12: fixture forms where each DJ rule decides a line", "[gen][hook]")
{
    // The hook fixture with its form or menus edited so a rule decides a line in (nearly) every seed,
    // 32 seeds each: r1, a build that plays the hook into drop 1, whose first menu is OUT; r2, a break
    // that always ends on the hook before a build without it; r3, a pull that takes the lead before
    // drop 2's last line, whose last menu strips it 3 to 1 (a 40-bar drop 2, a pull before every
    // 32-bar line).
    const Texts base = hookFixtureTexts();
    const auto bundleWith = [&base] (std::initializer_list<std::pair<std::string_view, std::string_view>> edits) {
        Texts t = base;
        for (const auto& [from, to] : edits)
            t.style = replaced (t.style, from, to);
        return bundleFromTexts (t);
    };
    const auto lastBarHas = [] (const Song& s, std::size_t i) {
        const Section& sec = s.sections[i];
        const std::int64_t bar = sec.startBar + sec.bars - 1;
        const auto& notes = leadOf (s).score;
        return std::any_of (notes.begin(), notes.end(), [&] (const Note& n) { return n.section == static_cast<std::int16_t> (i) && n.step / stepsPerBar == bar; });
    };
    std::size_t decided = 0;
    SECTION ("r1: a build that plays the hook into drop 1")
    {
        const Bundle b = bundleWith ({ { "\"build\": { \"first\": [ { \"op\": \"out\", \"w\": 1 } ] },", "\"build\": { \"first\": [ { \"op\": \"skeleton\", \"w\": 1 } ] }," },
            { "\"first\": [ { \"op\": \"skeleton\", \"w\": 2 }, { \"op\": \"full\", \"w\": 1 } ],", "\"first\": [ { \"op\": \"out\", \"w\": 1 } ]," } });
        for (std::uint64_t seed = 0; seed < 32; ++seed)
        {
            const Song s = songOf (b, seed);
            REQUIRE (s.hook.has_value());
            const std::size_t i = firstDropOf (s);
            REQUIRE (i < s.sections.size());
            REQUIRE (i > 0);
            if (!lastBarHas (s, i - 1))
                continue;
            ++decided;
            const HookLine* first = lineAt (*s.hook, i, 0);
            INFO ("seed " << seed << ": drop 1 at bar " << s.sections[i].startBar);
            CHECK ((first != nullptr && first->form != HookForm::out));
        }
        INFO ("r1 decided drop 1's first line in " << decided << " of 32 seeds");
        CHECK (decided >= 24);
    }
    SECTION ("r2: a break that ends on the hook, a build without it")
    {
        const Bundle b = bundleWith ({ { "{ \"kind\": \"break\", \"bars\": [ { \"bars\": 0, \"w\": 3 }, { \"bars\": 8, \"w\": 1 }, { \"bars\": 16, \"w\": 2 } ] }",
                                         "{ \"kind\": \"break\", \"bars\": [ { \"bars\": 16, \"w\": 1 } ] }" },
            { "\"first\": [ { \"op\": \"out\", \"w\": 3 }, { \"op\": \"fragment\", \"w\": 1 } ],", "\"first\": [ { \"op\": \"fragment\", \"w\": 1 } ]," } });
        for (std::uint64_t seed = 0; seed < 32; ++seed)
        {
            const Song s = songOf (b, seed);
            REQUIRE (s.hook.has_value());
            const std::size_t i = firstDropOf (s);
            REQUIRE (i < s.sections.size());
            REQUIRE (i >= 2);
            REQUIRE (s.sections[i - 1].kind == "build");
            REQUIRE (s.sections[i - 2].kind == "break");
            const bool buildPlays = std::any_of (leadOf (s).score.begin(), leadOf (s).score.end(), [&] (const Note& n) { return n.section == static_cast<std::int16_t> (i - 1); });
            if (!lastBarHas (s, i - 2) || buildPlays)
                continue;
            ++decided;
            const HookLine* first = lineAt (*s.hook, i, 0);
            INFO ("seed " << seed << ": drop 1 at bar " << s.sections[i].startBar);
            REQUIRE (first != nullptr);
            CHECK (first->form == HookForm::out);
        }
        INFO ("r2 decided drop 1's first line in " << decided << " of 32 seeds");
        CHECK (decided >= 24);
    }
    SECTION ("r3: a pull that takes the lead before drop 2's last line")
    {
        const Bundle b = bundleWith ({ { "{ \"kind\": \"drop2\", \"bars\": [ { \"bars\": 32, \"w\": 3 }, { \"bars\": 40, \"w\": 1 } ] }", "{ \"kind\": \"drop2\", \"bars\": [ { \"bars\": 40, \"w\": 1 } ] }" },
            { "\"dropout\": { \"p\": 35, \"every\": 16, \"lanes\": [ { \"lanes\": [\"kick\", \"bass\"], \"w\": 3 }, { \"lanes\": [\"kick\", \"bass\", \"hats\"], \"w\": 1 } ] },",
                "\"dropout\": { \"p\": 100, \"every\": 32, \"lanes\": [ { \"lanes\": [\"kick\", \"bass\", \"lead\"], \"w\": 1 } ] }," } });
        for (std::uint64_t seed = 0; seed < 32; ++seed)
        {
            const Song s = songOf (b, seed);
            REQUIRE (s.hook.has_value());
            const std::uint64_t carrier = std::uint64_t (1) << s.hook->lane;
            for (const Section& sec : s.sections)
                for (const Dropout& d : sec.dropouts)
                    if (sec.kind == "drop2" && (d.lanes & carrier) != 0 && d.bar + 1 < sec.bars)
                    {
                        ++decided;
                        const HookLine* after = lineAt (*s.hook, sec.index, d.bar + 1);
                        INFO ("seed " << seed << ": the pull at bar " << sec.startBar + d.bar);
                        CHECK ((after != nullptr && after->form != HookForm::out));
                    }
        }
        INFO ("r3 decided the line after a pull in " << decided << " of 32 seeds");
        CHECK (decided >= 24);
    }
}

// ------------------------------------------------------------------ 7-9: the line rule

TEST_CASE ("hook 7: textures differ in every bar or in none", "[gen][hook]")
{
    // Every texture a line plays holds a note in every bar of the material (bar 7 aside for BREATH);
    // every two lines next to each other in a section differ in every one of bars 0-6 or in none, on
    // the material (each texture before the fit and the guard) and on the lane's final notes (the
    // clash fix, the guard and FUT02's pedal in).
    for (const KitRef& k : hookKits())
        DYNAMIC_SECTION (k.label)
        {
            std::size_t pairs = 0;
            for (const Song& s : songsOf (k))
            {
                const SongHook& h = *s.hook;
                for (std::size_t i = 0; i < h.lines.size(); ++i)
                {
                    const HookLine& l = h.lines[i];
                    const Section& sec = s.sections[l.section];
                    INFO (k.label << " seed " << s.seed << ": the " << sec.kind << " at bar " << sec.startBar << ", its line at bar " << l.bar << " (" << described (l) << ")");
                    if (l.form != HookForm::out)
                        for (std::int32_t bar = 0; bar < lineBars; ++bar)
                            if (!(bar == lineBars - 1 && l.ending == HookEnding::breath))
                            {
                                INFO ("bar " << bar);
                                CHECK_FALSE (textureBar (h, l, bar).empty());
                            }
                    if (i == 0 || h.lines[i - 1].section != l.section)
                        continue;
                    const HookLine& before = h.lines[i - 1];
                    INFO ("after its line at bar " << before.bar << " (" << described (before) << ")");
                    REQUIRE (before.bar + lineBars == l.bar);
                    ++pairs;
                    const LinePrints m0 = materialPrints (h, before), m1 = materialPrints (h, l);
                    INFO ("the material differs in " << differingBars (m0, m1));
                    CHECK (lineRule (m0, m1));
                    const LinePrints r0 = lanePrints (leadOf (s), sec, before.bar), r1 = lanePrints (leadOf (s), sec, l.bar);
                    INFO ("the lane's notes differ in " << differingBars (r0, r1));
                    CHECK (lineRule (r0, r1));
                }
            }
            CHECK (pairs > 0);
        }
}

TEST_CASE ("hook 8: onTheLine on hand-made prints", "[gen][hook]")
{
    // A 2-bar riff's FULL line and lines made from it by hand. Safe: REPEAT; FULL to the half-bar
    // call and back; anything to or from OUT; a change in bar 7 only (BREATH's included). Refused:
    // FULL to the written bar-level call (bar 1 alone, then a silent bar); a change that starts at
    // bar 3.
    const auto print = [] (std::initializer_list<std::pair<std::int32_t, std::int32_t>> onsets) {
        BarPrint p;
        for (const auto& [sixteenth, key] : onsets)
            p.push_back (sixteenth * 128 + key);
        std::sort (p.begin(), p.end());
        return p;
    };
    const BarPrint one = print ({ { 0, 73 }, { 3, 76 }, { 6, 78 }, { 8, 76 }, { 11, 73 }, { 14, 71 } });
    const BarPrint two = print ({ { 0, 73 }, { 3, 76 }, { 6, 80 }, { 8, 81 }, { 11, 78 }, { 14, 76 } });
    LinePrints full, call, barCall, out, breath, vary, fromBar3;
    for (std::size_t b = 0; b < full.size(); ++b)
    {
        full[b] = b % 2 == 0 ? one : two;
        for (const auto x : full[b])
            if (x / 128 < 8)
                call[b].push_back (x);
        barCall[b] = b % 2 == 0 ? one : BarPrint {};
        breath[b] = b + 1 == full.size() ? BarPrint {} : full[b];
        vary[b] = full[b];
        fromBar3[b] = full[b];
        if (b >= 3)
            for (auto& x : fromBar3[b])
                x += 2; // every key a whole tone up from bar 3 on
    }
    vary.back().back() += 1; // the line's last note a semitone up
    CHECK (onTheLine (full, full)); // REPEAT
    CHECK (onTheLine (breath, breath));
    CHECK (onTheLine (full, call)); // the half-bar call: every bar differs
    CHECK (onTheLine (call, full));
    CHECK (onTheLine (full, out));
    CHECK (onTheLine (out, full));
    CHECK (onTheLine (call, out));
    CHECK (onTheLine (out, out));
    CHECK (onTheLine (full, breath)); // bar 7 only
    CHECK (onTheLine (breath, full));
    CHECK (onTheLine (full, vary));
    CHECK_FALSE (onTheLine (full, barCall)); // G4 counts its bar-1 changes off phrase
    CHECK_FALSE (onTheLine (barCall, full));
    CHECK_FALSE (onTheLine (full, fromBar3)); // a run that starts at bar 3
}

TEST_CASE ("hook 9: the lead's changes land on the line, as G4 reads them", "[gen][hook]")
{
    // G4 (tools/legacy_audit/README.md): per section of 9 bars or more, a bar's print is its set of
    // (16th, key) onsets; bar i >= 8 differs if it isn't bar i - 8's; a run of differing bars is one
    // change, on phrase if it starts at bar 0 or 7 of a line. Every change of the lead's track is on
    // phrase (prototype: 259/259, 314/314, 282/282, 241/241).
    for (const KitRef& k : hookKits())
        DYNAMIC_SECTION (k.label)
        {
            std::size_t changes = 0;
            for (const Song& s : songsOf (k))
                for (const Section& sec : s.sections)
                {
                    if (sec.bars < 9)
                        continue;
                    std::vector<BarPrint> bars (static_cast<std::size_t> (sec.bars));
                    for (const Note& n : leadOf (s).score)
                    {
                        const std::int64_t b = n.step / stepsPerBar - sec.startBar;
                        if (b >= 0 && b < sec.bars)
                            bars[static_cast<std::size_t> (b)].push_back (static_cast<std::int32_t> ((n.step % stepsPerBar) * 128 + n.key));
                    }
                    for (auto& b : bars)
                        tidy (b);
                    bool previous = false;
                    for (std::size_t i = 8; i < bars.size(); ++i)
                    {
                        const bool differs = bars[i] != bars[i - 8];
                        if (differs && !previous)
                        {
                            ++changes;
                            INFO (k.label << " seed " << s.seed << ": the " << sec.kind << " at bar " << sec.startBar << " changes at its bar " << i);
                            CHECK ((i % 8 == 0 || i % 8 == 7));
                        }
                        previous = differs;
                    }
                }
            CHECK (changes > 0);
        }
}

// ------------------------------------------------------------------ 13-20: the placement and the kits

TEST_CASE ("hook 13: FUT02's groove keeps the arp", "[gen][hook]")
{
    // FUT02's arp enters its grooves 8 to 32 bars in, or not at all. The hook plays in every groove
    // whose arp (as FUT02 before the hook, the nohook fixture, places it) is active in some bar, and only
    // in its active bars, so the grooves without the arp are exactly the fixture's (prototype: 23 of
    // 64 first grooves, as before the hook).
    const KitRef& k = shippedKit ("FUT02");
    const Bundle& before = bundleOf (arpFixture());
    std::size_t grooves = 0, silent = 0;
    for (const Song& s : songsOf (k))
    {
        const Song was = songOf (before, s.seed);
        REQUIRE (was.sections.size() == s.sections.size());
        const std::size_t arp = laneIndex (was, "arp");
        for (const Section& sec : s.sections)
        {
            if (sec.kind != "groove")
                continue;
            ++grooves;
            std::set<std::int64_t> active;
            for (std::int32_t bar = 0; bar < sec.bars; ++bar)
                if (was.sections[sec.index].active (arp, bar))
                    active.insert (bar);
            std::size_t notes = 0, outside = 0;
            for (const Note& n : leadOf (s).score)
                if (n.section == static_cast<std::int16_t> (sec.index))
                {
                    ++notes;
                    outside += active.contains (barInSection (s, n)) ? 0u : 1u;
                }
            INFO ("seed " << s.seed << ": the groove at bar " << sec.startBar << ", its arp active in " << active.size() << " bars before the hook; " << notes << " hook notes, "
                          << outside << " outside them");
            CHECK ((notes > 0) == !active.empty());
            CHECK (outside == 0);
            silent += active.empty() ? 1u : 0u;
        }
    }
    INFO (silent << " of " << grooves << " grooves without the arp");
    CHECK (grooves > 0);
}

TEST_CASE ("hook 14: drop 2 plays a phrase drop 1 never played", "[gen][hook]")
{
    // The bar prints of drop 2's first line never occur in drop 1 (prototype: none).
    for (const KitRef& k : hookKits())
        DYNAMIC_SECTION (k.label)
        {
            std::size_t compared = 0;
            for (const Song& s : songsOf (k))
            {
                const std::size_t firstDrop = firstDropOf (s);
                REQUIRE (firstDrop < s.sections.size());
                const Section& drop = s.sections[firstDrop];
                std::set<BarPrint> played;
                for (std::int32_t line = 0; line < drop.bars; line += lineBars)
                    for (const BarPrint& p : lanePrints (leadOf (s), drop, line))
                        if (!p.empty())
                            played.insert (p);
                for (const Section& sec : s.sections)
                {
                    if (sec.kind != "drop2")
                        continue;
                    const LinePrints first = lanePrints (leadOf (s), sec, 0);
                    for (std::size_t b = 0; b < first.size(); ++b)
                    {
                        if (first[b].empty())
                            continue;
                        ++compared;
                        INFO (k.label << " seed " << s.seed << ": drop 2's bar " << b << " is a bar of drop 1");
                        CHECK_FALSE (played.contains (first[b]));
                    }
                }
            }
            CHECK (compared > 0);
        }
}

TEST_CASE ("hook 15: the new phrase in its idiom", "[gen][hook]")
{
    // Every idiom: the new phrase starts on the tonic, or on the 5th when A starts on the tonic, and
    // its first onset never takes A's key at that 16th.
    // organ (FUT01, FUT03), a b a' c: bars 1 and 3 share the rhythm and differ in one note; bar 2 keeps
    //   bar 1's first-half rhythm; bar 4 keeps bar 1's first half and falls by scale steps to 5, 2 or
    //   b3, never the tonic, its last note held to the bar line.
    // glass (FUT02): one rhythm in all four bars; bar 4 strikes the period's top again (the highest
    //   note the leap cap allows from the note before it).
    // stab (FUT04): a 2-bar riff from the period's cells, its last note the 5th.
    for (const KitRef& k : hookKits())
        DYNAMIC_SECTION (k.label)
        {
            const HookSpec& spec = hookOf (k);
            for (const Song& s : songsOf (k))
            {
                const SongHook& h = *s.hook;
                INFO (k.label << " seed " << s.seed);
                REQUIRE_FALSE (h.a.notes.empty());
                REQUIRE_FALSE (h.b.notes.empty());
                const HookNote& aFirst = h.a.notes.front();
                const HookNote& bFirst = h.b.notes.front();
                CHECK (util::floorMod (bFirst.degree, 7) == (util::floorMod (aFirst.degree, 7) == 0 ? 4 : 0));
                for (const HookNote& n : h.a.notes)
                    if (n.step % 16 == bFirst.step % 16)
                        CHECK (keyOf (h, n.degree) != keyOf (h, bFirst.degree));
                switch (spec.period.idiom)
                {
                    case HookIdiom::organ:
                    {
                        REQUIRE (h.b.bars == 4);
                        const auto a = barNotes (h.b, 0), b = barNotes (h.b, 1), a2 = barNotes (h.b, 2), c = barNotes (h.b, 3);
                        CHECK (sixteenths (a) == sixteenths (a2));
                        std::size_t moved = 0;
                        for (std::size_t i = 0; i < a.size() && i < a2.size(); ++i)
                            moved += a[i].degree != a2[i].degree ? 1u : 0u;
                        CHECK (moved == 1);
                        CHECK (sixteenths (b, 0, 8) == sixteenths (a, 0, 8));
                        std::vector<std::pair<std::int32_t, std::int32_t>> head, cHead;
                        std::vector<HookNote> fall;
                        for (const HookNote& n : a)
                            if (n.step % 16 < 8)
                                head.emplace_back (n.step % 16, n.degree);
                        for (const HookNote& n : c)
                        {
                            if (n.step % 16 < 8)
                                cHead.emplace_back (n.step % 16, n.degree);
                            else
                                fall.push_back (n);
                        }
                        CHECK (cHead == head);
                        REQUIRE_FALSE (fall.empty());
                        for (std::size_t i = 1; i < fall.size(); ++i)
                        {
                            CHECK (fall[i].degree < fall[i - 1].degree);
                            CHECK (fall[i - 1].degree - fall[i].degree <= 2); // a scale step of the pool
                        }
                        const std::int32_t end = util::floorMod (fall.back().degree, 7);
                        CHECK ((end == 4 || end == 1 || end == 2));
                        CHECK (fall.back().step % 16 + fall.back().len16 == 16);
                        break;
                    }
                    case HookIdiom::glass:
                    {
                        REQUIRE (h.b.bars == 4);
                        for (std::int32_t bar = 1; bar < 4; ++bar)
                            CHECK (sixteenths (barNotes (h.b, bar)) == sixteenths (barNotes (h.b, 0)));
                        std::int32_t top = h.b.notes.front().degree;
                        for (const HookNote& n : h.b.notes)
                            if (n.step < 48)
                                top = std::max (top, n.degree);
                        const auto c = barNotes (h.b, 3);
                        std::size_t strike = 0;
                        for (std::size_t i = 0; i < c.size(); ++i)
                            if (c[i].step % 16 >= 8 && (c[strike].step % 16 < 8 || c[i].degree > c[strike].degree))
                                strike = i;
                        REQUIRE (c[strike].step % 16 >= 8);
                        const std::int32_t before = strike > 0 ? keyOf (h, c[strike - 1].degree) : keyOf (h, c[strike].degree);
                        INFO ("the period's top " << keyOf (h, top) << ", bar 4 strikes " << keyOf (h, c[strike].degree));
                        CHECK ((c[strike].degree == top || std::abs (keyOf (h, top) - before) > spec.pitch.leap));
                        break;
                    }
                    case HookIdiom::stab:
                    {
                        REQUIRE (h.b.bars == 2);
                        CHECK (fromCell (barNotes (h.b, 0), spec.period.cells, spec.mutate.extra));
                        CHECK (util::floorMod (h.b.notes.back().degree, 7) == 4);
                        break;
                    }
                }
            }
        }
}

TEST_CASE ("hook 16: a fixed accent shape", "[gen][hook]")
{
    // The velocities are drawn once per seed, not per bar: in FULL lines of the hook bars i and i + 2
    // carry equal score velocities; the median SD of such a line's velocities (every note of the
    // carrier there, FUT02's pedal included, lines of 8 notes or more, as the prototype counted) is 9
    // or more per kit (prototype 12.9, 21.0, 11.5, 14.5); FUT01's and FUT04's downbeat accents sit
    // above their other accents.
    for (const KitRef& k : hookKits())
        DYNAMIC_SECTION (k.label)
        {
            std::vector<double> sds;
            std::int64_t down = 0, downCount = 0, other = 0, otherCount = 0;
            for (const Song& s : songsOf (k))
            {
                const SongHook& h = *s.hook;
                for (const HookLine& l : h.lines)
                {
                    if (l.form != HookForm::full || l.motif != 0)
                        continue;
                    const Section& sec = s.sections[l.section];
                    std::array<std::map<std::int64_t, std::int32_t>, 8> bars;
                    std::vector<std::int32_t> velocities;
                    for (const Note& n : leadOf (s).score)
                    {
                        const std::int64_t b = n.step / stepsPerBar - (sec.startBar + l.bar);
                        if (n.section != static_cast<std::int16_t> (sec.index) || b < 0 || b >= lineBars)
                            continue;
                        bars[static_cast<std::size_t> (b)][n.step % stepsPerBar] = n.velocity;
                        velocities.push_back (n.velocity);
                    }
                    for (std::size_t b = 0; b + 2 < 7; ++b)
                        for (const auto& [sixteenth, v] : bars[b])
                            if (const auto it = bars[b + 2].find (sixteenth); it != bars[b + 2].end())
                            {
                                INFO (k.label << " seed " << s.seed << ": the full line at bar " << sec.startBar + l.bar << ", 16th " << sixteenth << " of its bars " << b << " and " << b + 2);
                                CHECK (it->second == v);
                            }
                    if (velocities.size() >= 8)
                        sds.push_back (populationSd (velocities));
                }
                for (const HookNote& n : h.a.notes)
                    if (n.accent)
                    {
                        (n.step % 16 == 0 ? down : other) += n.velocity;
                        ++(n.step % 16 == 0 ? downCount : otherCount);
                    }
            }
            REQUIRE_FALSE (sds.empty());
            std::sort (sds.begin(), sds.end());
            const double median = sds[(sds.size() - 1) / 2];
            INFO (k.label << ": the median SD of " << sds.size() << " full lines' velocities is " << median);
            CHECK (median >= 9.0);
            if (k.id == "FUT01" || k.id == "FUT04")
            {
                INFO (downCount << " downbeat accents, " << otherCount << " other accents");
                REQUIRE (downCount > 0);
                REQUIRE (otherCount > 0);
                CHECK (down * otherCount > other * downCount);
            }
        }
}

TEST_CASE ("hook 17: no mud against the chords", "[gen][hook]")
{
    // The written-reference rule (the plan's mud rule): no lead note a minor 9th above or below a
    // chord-lane note that sounds with it, and a semitone only with the lower note at the kit's written
    // floor or above (68, 76, 69 and 78 in FUT01-FUT04), FUT02's pedal included.
    const std::map<std::string, std::int32_t, std::less<>> floors = { { "FUT01", 68 }, { "FUT02", 76 }, { "FUT03", 69 }, { "FUT04", 78 } };
    for (const KitRef& k : hookKits())
        DYNAMIC_SECTION (k.label)
        {
            const HookSpec& spec = hookOf (k);
            const std::int32_t floor = floors.at (k.id);
            CHECK (spec.guard == floor);
            std::size_t pairs = 0;
            std::vector<std::string> mud;
            for (const Song& s : songsOf (k))
            {
                const auto chords = chordNotesBySection (s);
                const std::vector<const Note*> none;
                for (const Note& n : leadOf (s).score)
                    for (const Note* c : chords.contains (n.section) ? chords.at (n.section) : none)
                    {
                        if (c->tick >= n.tick + n.length || n.tick >= c->tick + c->length)
                            continue;
                        ++pairs;
                        const int d = std::abs (int (n.key) - int (c->key));
                        if (d == 13 || (d == 1 && std::min (n.key, c->key) < floor))
                            mud.push_back ("seed " + std::to_string (s.seed) + ": the lead's " + std::to_string (n.key) + " at tick " + std::to_string (n.tick) + " against the chords' "
                                           + std::to_string (c->key));
                    }
            }
            INFO (k.label << ": " << mud.size() << " rubs in " << pairs << " pairs:" << listed (mud));
            CHECK (pairs > 0);
            CHECK (mud.empty());
        }
}

namespace
{
    // The lead notes at or under the top chord-lane note sounding with them (any overlap), and all of
    // them, in permille: the plan's register measure (every lead note counts, FUT02's pedal included).
    std::pair<std::size_t, std::size_t> underTheChords (const std::vector<Song>& songs)
    {
        std::size_t under = 0, all = 0;
        for (const Song& s : songs)
        {
            const auto chords = chordNotesBySection (s);
            for (const Note& n : leadOf (s).score)
            {
                ++all;
                int top = -1;
                if (const auto it = chords.find (n.section); it != chords.end())
                    for (const Note* c : it->second)
                        if (c->tick < n.tick + n.length && n.tick < c->tick + c->length)
                            top = std::max (top, int (c->key));
                under += top >= n.key ? 1u : 0u;
            }
        }
        return { under, all };
    }
}

TEST_CASE ("hook 18: the register", "[gen][hook]")
{
    // Every lead note inside its kit range, which starts at 64 or above; the lead at or under the top
    // sounding chord-lane note in at most 200, 250, 200 and 100 permille of its notes (FUT01-FUT04 in
    // the clearing register; prototype 148, 174, 129 and 17), and 400 for FUT01 and FUT03 in the
    // written one (from F#4 and G4; prototype 345 and 332).
    const std::map<std::string, std::size_t, std::less<>> limits = { { "FUT01", 200 }, { "FUT02", 250 }, { "FUT03", 200 }, { "FUT04", 100 } };
    for (const KitRef& k : hookKits())
        DYNAMIC_SECTION (k.label)
        {
            const Range range = carrierOf (k).range;
            CHECK (range.lo >= 64);
            for (const Song& s : songsOf (k))
                for (const Note& n : leadOf (s).score)
                {
                    INFO (k.label << " seed " << s.seed << ": the lead's " << int (n.key) << " at tick " << n.tick);
                    CHECK (n.key >= range.lo);
                    CHECK (n.key <= range.hi);
                }
            const auto [under, all] = underTheChords (songsOf (k));
            INFO (k.label << ": " << under << " of " << all << " lead notes at or under the top chord note");
            CHECK (all > 0);
            CHECK (under * 1000 <= all * limits.at (k.id));
        }
    for (const auto& [id, low] : { std::pair { "FUT01", 66 }, std::pair { "FUT03", 67 } })
        DYNAMIC_SECTION ("the written register: " << id << " from " << low)
        {
            const KitRef& k = shippedKit (id);
            Bundle written = bundleOf (k);
            const std::string carrier = hookOf (k).lane;
            for (KitLane& kl : written.kit.lanes)
                if (kl.id == carrier)
                    kl.range.lo = low;
            std::vector<Song> songs;
            for (std::uint64_t seed = 0; seed < seeds; ++seed)
            {
                songs.push_back (songOf (written, seed));
                REQUIRE (songs.back().hook.has_value());
            }
            const auto [under, all] = underTheChords (songs);
            INFO (id << " in the written register: " << under << " of " << all << " lead notes at or under the top chord note");
            CHECK (all > 0);
            CHECK (under * 1000 <= all * 400);
        }
}

TEST_CASE ("hook 19: FUT02's arp is the written ChromeArp", "[gen][hook]")
{
    // The top voice on the cell's onsets, at C5 or above, at or under D5 in at most 120 permille of
    // the FULL and ANSWER lines' top notes (prototype 94); after each top note at most two pedal
    // notes, D5 then A4, each under the top note it follows, none before the bar's first top note; no
    // onset on 16th 15; SKELETON plays the top voice alone; R6's median rest at least 200 permille
    // (its lead windows: per 4-bar window with a lead onset, the share of its 64 16ths at whose start
    // no lead note sounds; prototype 250).
    const KitRef& k = shippedKit ("FUT02");
    constexpr int c5 = 72, d5 = 74, a4 = 69;
    std::size_t tops = 0, low = 0;
    std::vector<std::int64_t> rests;
    for (const Song& s : songsOf (k))
    {
        const SongHook& h = *s.hook;
        const LaneOut& lane = leadOf (s);
        INFO ("seed " << s.seed);
        // bar by bar: the top voice and the pedal after each of its notes
        std::map<std::int64_t, std::vector<const Note*>> bars;
        for (const Note& n : lane.score)
        {
            bars[n.step / stepsPerBar].push_back (&n);
            INFO ("the note at tick " << n.tick);
            CHECK (n.step % stepsPerBar != 15);
        }
        for (const auto& [bar, notes] : bars)
        {
            const Section& sec = s.sections[static_cast<std::size_t> (notes.front()->section)];
            const HookLine* line = lineAt (h, sec.index, bar - sec.startBar);
            REQUIRE (line != nullptr);
            INFO ("bar " << bar + 1 << ", a line of " << described (*line));
            const Note* top = nullptr;
            std::vector<int> after;
            for (const Note* n : notes)
            {
                if (!isPedal (*n))
                {
                    top = n;
                    after.clear();
                    CHECK (n->key >= c5);
                    if ((line->form == HookForm::full || line->form == HookForm::answer) && line->motif == 0)
                    {
                        ++tops;
                        low += n->key <= d5 ? 1u : 0u;
                    }
                    continue;
                }
                CHECK (line->form != HookForm::skeleton);
                CHECK (top != nullptr); // none before the bar's first top note
                if (top == nullptr)
                    continue;
                CHECK ((n->key == d5 || n->key == a4));
                CHECK (n->key < top->key);
                after.push_back (n->key);
                CHECK (after.size() <= 2);
                if (after.size() == 2)
                    CHECK ((after[0] == d5 && after[1] == a4));
            }
            if (line->form == HookForm::full && line->motif == 0)
            {
                std::set<std::int64_t> played, written;
                for (const Note* n : notes)
                    if (!isPedal (*n))
                        played.insert (n->step % stepsPerBar);
                for (const TextureNote& t : textureBar (h, *line, static_cast<std::int32_t> (bar - sec.startBar - line->bar)))
                    written.insert (t.sixteenth);
                CHECK (std::includes (written.begin(), written.end(), played.begin(), played.end()));
            }
        }
        // R6's breath, on the score
        for (const Section& sec : s.sections)
            for (std::int32_t w = 0; w + 4 <= sec.bars; w += 4)
            {
                const std::int64_t t0 = (sec.startBar + w) * ticksPerBar, t1 = t0 + 4 * ticksPerBar;
                std::vector<const Note*> sounding; // the lead notes that overlap the window
                bool onset = false;
                for (const Note& n : lane.score)
                {
                    onset = onset || (n.tick >= t0 && n.tick < t1);
                    if (n.tick < t1 && n.tick + n.length > t0)
                        sounding.push_back (&n);
                }
                if (!onset)
                    continue;
                std::int64_t covered = 0;
                for (std::int64_t q = 0; q < 64; ++q)
                {
                    const std::int64_t at = t0 + q * ticksPerStep;
                    covered += std::any_of (sounding.begin(), sounding.end(), [at] (const Note* n) { return n->tick <= at && at < n->tick + n->length; }) ? 1 : 0;
                }
                rests.push_back ((64 - covered) * 1000 / 64);
            }
    }
    INFO (low << " of " << tops << " full and answer top notes at or under D5");
    REQUIRE (tops > 0);
    CHECK (low * 1000 <= tops * 120);
    REQUIRE_FALSE (rests.empty());
    std::sort (rests.begin(), rests.end());
    const std::int64_t median = rests[(rests.size() - 1) / 2];
    INFO ("R6's median rest " << median << " permille over " << rests.size() << " windows");
    CHECK (median >= 200);
}

TEST_CASE ("hook 20: FUT02's drop 2 drives", "[gen][hook]")
{
    // ADR 0023 has FUT02's drop 2 lift on a driving arp: drop 2's notes a bar, every lane pooled over
    // 64 seeds, at least drop 1's (prototype 54.2 against 48.3); drop 2 thinner than drop 1 in at most
    // 12 of 64 seeds (prototype 9).
    const KitRef& k = shippedKit ("FUT02");
    std::int64_t notes1 = 0, bars1 = 0, notes2 = 0, bars2 = 0;
    std::size_t thinner = 0;
    for (const Song& s : songsOf (k))
    {
        std::vector<std::int64_t> notes (s.sections.size(), 0);
        for (const LaneOut& lane : s.lanes)
            for (const Note& n : lane.score)
                ++notes[static_cast<std::size_t> (n.section)];
        std::int64_t n1 = 0, b1 = 0, n2 = 0, b2 = 0;
        const std::size_t firstDrop = firstDropOf (s);
        for (const Section& sec : s.sections)
        {
            if (sec.index == firstDrop)
            {
                n1 += notes[sec.index];
                b1 += sec.bars;
            }
            if (sec.kind == "drop2")
            {
                n2 += notes[sec.index];
                b2 += sec.bars;
            }
        }
        REQUIRE (b1 > 0);
        REQUIRE (b2 > 0);
        notes1 += n1;
        bars1 += b1;
        notes2 += n2;
        bars2 += b2;
        thinner += n2 * b1 < n1 * b2 ? 1u : 0u;
    }
    INFO ("drop 1 " << notes1 * 10 / bars1 << " tenths of a note a bar, drop 2 " << notes2 * 10 / bars2 << "; drop 2 thinner in " << thinner << " of " << seeds << " seeds");
    CHECK (notes2 * bars1 >= notes1 * bars2);
    CHECK (thinner <= 12);
}

// ------------------------------------------------------------------ 21-25: the lane, the export and the cut

TEST_CASE ("hook 21: the hook lane is additive", "[gen][hook]")
{
    // The lead joins the plans of the groove, break, build, drop and drop 2 with no entry, rest,
    // dropout group, roster place or layer, last of the lanes, so every form draw is unchanged; the
    // hook's draws hang below their own tag; and the chords never read the lead. So, seeds 0-15:
    // FUT01, FUT03, FUT04 (and the hook fixture) without the lead lane and the hook block play every
    // other lane note for note, controller for controller, bar for bar; FUT02 against itself before the
    // hook (the nohook fixture: its arp of kind arp, no hook block), every lane but the arp.
    for (const KitRef* k : { &shippedKit ("FUT01"), &shippedKit ("FUT03"), &shippedKit ("FUT04"), &hookFixture() })
        DYNAMIC_SECTION (k->label)
        {
            const Bundle& with = bundleOf (*k);
            const HookSpec& spec = hookOf (*k);
            const Style& style = with.style;
            for (const KindPlan& p : style.form.plans)
            {
                INFO ("the " << p.kind << " plan");
                for (const LaneEntry& e : p.entries)
                    CHECK (std::find (e.lanes.begin(), e.lanes.end(), spec.lane) == e.lanes.end());
                if (p.rest)
                    for (const LaneWeight& lw : p.rest->lanes)
                        CHECK (lw.lane != spec.lane);
                if (p.dropout)
                    for (const LaneGroup& g : p.dropout->lanes)
                        CHECK (std::find (g.lanes.begin(), g.lanes.end(), spec.lane) == g.lanes.end());
                for (const BlockChoice& c : p.choices)
                    for (const auto& block : c.blocks)
                        CHECK (std::find (block.begin(), block.end(), spec.lane) == block.end()); // no intro or outro plays it
            }
            CHECK (std::find (style.changes.roster.begin(), style.changes.roster.end(), spec.lane) == style.changes.roster.end());
            for (const LaneWeight& lw : style.form.layer)
                CHECK (lw.lane != spec.lane);
            const Bundle without = withoutHook (with);
            for (std::uint64_t seed = 0; seed < 16; ++seed)
            {
                const Song a = songOf (with, seed);
                const Song b = songOf (without, seed);
                INFO (k->label << " seed " << seed);
                CHECK (otherLanesDiffer (a, b, spec.lane) == "");
            }
        }
    SECTION ("FUT02 against FUT02 before the hook")
    {
        const Bundle& now = bundleOf (shippedKit ("FUT02"));
        const Bundle& before = bundleOf (arpFixture());
        REQUIRE (now.style.hook.has_value());
        REQUIRE_FALSE (before.style.hook.has_value());
        for (std::uint64_t seed = 0; seed < 16; ++seed)
        {
            const Song a = songOf (now, seed);
            const Song b = songOf (before, seed);
            INFO ("FUT02 seed " << seed);
            CHECK (otherLanesDiffer (a, b, "arp") == "");
        }
    }
}

TEST_CASE ("hook 22: a style without a hook block generates the same bytes", "[gen][hook]")
{
    // FUT02 before the hook (the nohook fixture: no hook block): every file of seeds 0, 1 and 42, both
    // versions, hashes to what the generator wrote before the hook. Each pinned hash is the SHA-256 of
    // the lines "<version>/<path> <the file's SHA-256>\n" in renderFiles' order, feel first: pinned at
    // the hook's tests-first commit, whose generator is the one before the hook, and recomputed then
    // from FUT02's entries in core/tests/snapshots/gen.sha256, which agree. Its regions.json holds no
    // "hook" key, at the top or in a region.
    const Bundle& b = bundleOf (arpFixture());
    REQUIRE_FALSE (b.style.hook.has_value());
    const std::map<std::uint64_t, std::string> pinned = {
        { 0, "bfd4cc43d4b712315ee66db7edee489a4816c30c9c5f68d1e50e111569f2d12b" },
        { 1, "d6393a6adb9409b4c8baf1003221af30d5fd3288991c0258a87a59c35ec53ddf" },
        { 42, "e2332f830ceb63cc7329769377f954aa88f36f4e1b3e93b6aa480e031c399213" },
    };
    for (const auto& [seed, want] : pinned)
    {
        const Song s = songOf (b, seed);
        CHECK_FALSE (s.hook.has_value());
        std::string manifest;
        for (const Version v : { Version::feel, Version::quantized })
        {
            const auto files = renderFiles (s, v);
            REQUIRE (files.ok());
            for (const OutputFile& f : files.value)
                manifest += std::string (versionName (v)) + "/" + f.path + " " + util::sha256Hex (f.bytes) + "\n";
            const auto parsed = json::parse (regionsJson (s, v));
            REQUIRE (parsed.ok());
            CHECK (parsed.value.find ("hook") == nullptr);
            const json::Value* regions = parsed.value.find ("regions");
            REQUIRE (regions != nullptr);
            for (const json::Value& r : regions->items)
                CHECK (r.find ("hook") == nullptr);
        }
        const std::vector<std::uint8_t> bytes (manifest.begin(), manifest.end());
        INFO ("seed " << seed << "'s files:\n" << manifest);
        CHECK (util::sha256Hex (bytes) == want);
    }
}

TEST_CASE ("hook 23: regions.json lists the hook lines", "[gen][hook][export]")
{
    // At the top, "hook": { "lane": the carrier }; per region, "hook": its lines in order, each
    // { "startBar", "dawBar", "bars": 8, "form", "motif" ("a" the hook, "b" the new phrase), "shift",
    // "ending", "mask" }, [] where none; "format" stays 1. A mask is 16 characters of 0 and 1 in time
    // order, character i bar i / 2's first half when i is even: busy where a lead note starts in the
    // half-bar or sounds into it, recomputed here from the lane's final notes.
    for (const KitRef& k : hookKits())
        DYNAMIC_SECTION (k.label)
        {
            std::size_t lines = 0;
            for (std::uint64_t seed = 0; seed < 8; ++seed)
            {
                const Song& s = songsOf (k)[seed];
                const SongHook& h = *s.hook;
                for (const Version v : { Version::feel, Version::quantized })
                {
                    INFO (k.label << " seed " << seed << ", " << versionName (v));
                    const auto parsed = json::parse (regionsJson (s, v));
                    REQUIRE (parsed.ok());
                    const json::Value* format = parsed.value.find ("format");
                    REQUIRE (format != nullptr);
                    CHECK (format->integer == 1);
                    const json::Value* hook = parsed.value.find ("hook");
                    REQUIRE (hook != nullptr);
                    REQUIRE (hook->find ("lane") != nullptr);
                    CHECK (hook->find ("lane")->string == leadOf (s).id);
                    const json::Value* regions = parsed.value.find ("regions");
                    REQUIRE (regions != nullptr);
                    REQUIRE (regions->items.size() == s.sections.size());
                    for (std::size_t i = 0; i < s.sections.size(); ++i)
                    {
                        const Section& sec = s.sections[i];
                        INFO ("region " << i + 1 << ", the " << sec.kind);
                        const json::Value* list = regions->items[i].find ("hook");
                        REQUIRE (list != nullptr);
                        REQUIRE (list->type == json::Value::Type::array);
                        std::vector<const HookLine*> want;
                        for (const HookLine& l : h.lines)
                            if (l.section == i)
                                want.push_back (&l);
                        REQUIRE (list->items.size() == want.size());
                        for (std::size_t j = 0; j < want.size(); ++j)
                        {
                            const json::Value& e = list->items[j];
                            const HookLine& l = *want[j];
                            INFO ("its line at bar " << l.bar);
                            for (const char* key : { "startBar", "dawBar", "bars", "form", "motif", "shift", "ending", "mask" })
                            {
                                INFO (key);
                                REQUIRE (e.find (key) != nullptr);
                            }
                            CHECK (e.find ("startBar")->integer == sec.startBar + l.bar);
                            CHECK (e.find ("dawBar")->integer == sec.startBar + l.bar + 1);
                            CHECK (e.find ("bars")->integer == lineBars);
                            CHECK (e.find ("form")->string == formName (l.form));
                            CHECK (e.find ("motif")->string == (l.motif == 1 ? "b" : "a"));
                            CHECK (e.find ("shift")->integer == l.shift);
                            CHECK (e.find ("ending")->string == endingName (l.ending));
                            CHECK (e.find ("mask")->string == maskString (l.mask));
                            CHECK (maskString (busyHalves (leadOf (s), sec.startBar + l.bar)) == maskString (l.mask));
                            ++lines;
                        }
                    }
                }
            }
            CHECK (lines > 0);
        }
}

TEST_CASE ("hook 24: a --bars cut keeps the whole song's hook", "[gen][hook][cut]")
{
    // A cut is the whole song's head (cut.h): at every 8-bar line of 16 seeds per kit, the cut's hook
    // is the whole song's material, its lines that start before the cut (masks as they are) and its
    // notes that start before it, ended by it.
    for (const KitRef& k : hookKits())
        DYNAMIC_SECTION (k.label)
        {
            std::size_t cuts = 0;
            std::vector<std::string> failed;
            for (std::uint64_t seed = 0; seed < 16; ++seed)
            {
                const Song& whole = songsOf (k)[seed];
                const SongHook& w = *whole.hook;
                for (std::int32_t bars = lineBars; bars < whole.totalBars; bars += lineBars)
                {
                    const Song cut = hookSong (k, seed, bars);
                    const SongHook& c = *cut.hook;
                    ++cuts;
                    const std::int64_t end = static_cast<std::int64_t> (bars) * ticksPerBar;
                    std::vector<HookLine> lines;
                    for (const HookLine& l : w.lines)
                        if (whole.sections[l.section].startBar + l.bar < bars)
                            lines.push_back (l);
                    std::vector<Note> notes;
                    for (Note n : w.notes)
                        if (n.tick < end)
                        {
                            n.length = std::min (n.length, end - n.tick);
                            notes.push_back (n);
                        }
                    const std::string where = "seed " + std::to_string (seed) + " cut at bar " + std::to_string (bars) + ": ";
                    if (c.lane != w.lane || c.mode != w.mode || c.base != w.base || c.contour != w.contour || !(c.a == w.a) || !(c.b == w.b) || !(c.response == w.response)
                        || c.varyDegree != w.varyDegree)
                        failed.push_back (where + "not the whole song's material");
                    if (c.lines != lines)
                        failed.push_back (where + std::to_string (c.lines.size()) + " lines, the whole song's head " + std::to_string (lines.size()));
                    if (c.notes != notes)
                        failed.push_back (where + std::to_string (c.notes.size()) + " notes, or not the whole song's head of " + std::to_string (notes.size()));
                }
            }
            INFO (k.label << ": " << failed.size() << " of " << cuts << " cuts lose the whole song's hook:" << listed (failed));
            CHECK (failed.empty());
            CHECK (cuts > 300);
        }
}

TEST_CASE ("hook 25: an OUT line leaves no dropout and no block bit", "[gen][hook]")
{
    // An OUT line clears the carrier's bit in its block and takes the carrier out of a one-bar dropout
    // inside it (a dropout that then lists no lane goes), so regions.json lists only what plays: a
    // line plays exactly where the carrier's block bit is set, and no dropout lists the carrier inside
    // a line where it is OUT.
    for (const KitRef& k : hookKits())
        DYNAMIC_SECTION (k.label)
        {
            for (const Song& s : songsOf (k))
            {
                const SongHook& h = *s.hook;
                const std::uint64_t carrier = std::uint64_t (1) << h.lane;
                for (const HookLine& l : h.lines)
                {
                    const Section& sec = s.sections[l.section];
                    INFO (k.label << " seed " << s.seed << ": the " << sec.kind << " at bar " << sec.startBar << ", its line at bar " << l.bar << " (" << described (l) << ")");
                    REQUIRE (static_cast<std::size_t> (l.bar / lineBars) < sec.blocks.size());
                    CHECK (((sec.blocks[static_cast<std::size_t> (l.bar / lineBars)] & carrier) != 0) == (l.form != HookForm::out));
                }
                for (const Section& sec : s.sections)
                    for (const Dropout& d : sec.dropouts)
                    {
                        INFO (k.label << " seed " << s.seed << ": the " << sec.kind << " at bar " << sec.startBar << ", the dropout at its bar " << d.bar);
                        CHECK (d.lanes != 0);
                        if ((d.lanes & carrier) == 0)
                            continue;
                        const HookLine* line = lineAt (h, sec.index, d.bar);
                        CHECK ((line != nullptr && line->form != HookForm::out));
                    }
            }
        }
}

// ------------------------------------------------------------------ 26: the keys

TEST_CASE ("hook 26: the hook's keys are its permanent tags", "[gen][hook][rng]")
{
    // A renamed tag fails here, not only in the snapshots: the generator's hook keys against the
    // reference vectors (test_rng.cpp's Python and Node implementations, which agree): FUT01 seed 7's
    // root, root.with (tag ("hook")) and root.with (tag ("hook")).with (tag ("rhythm")).with (tag ("cell")).
    const rng::Key root = songRoot ("FUT01", 7);
    CHECK (root.h == 0x4E503AB4C67A1176ull);
    CHECK (hookKey (root).h == 0x305D75E36F54C6F0ull);
    CHECK (hookKey (root) == root.with (rng::tag ("hook")));
    CHECK (hookCellKey (root).h == 0xC989FB08A5C35F99ull);
    CHECK (hookCellKey (root) == hookKey (root).with (rng::tag ("rhythm")).with (rng::tag ("cell")));
}
