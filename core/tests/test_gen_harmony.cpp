// Tests for core/gen/harmony: the anti-cliche memory, the progression grammar, pushed changes and
// voicings, on the real styles and on whole songs.

#include "gen/content.h"
#include "gen/harmony.h"
#include "gen/song.h"
#include "gen/theory.h"
#include "util/file_io.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

using namespace wubansia;
using namespace wubansia::gen;
using Catch::Matchers::ContainsSubstring;

namespace
{
    std::string styleText (std::string_view style)
    {
        const auto r = util::readBinaryFile (std::filesystem::path (WUBANSIA_SOURCE_DIR) / "content" / "styles" / (std::string (style) + ".json"));
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

    const Bundle& bundleFor (std::string_view id)
    {
        static std::map<std::string, Bundle, std::less<>> cache;
        auto it = cache.find (id);
        if (it == cache.end())
        {
            auto b = loadBundle (std::filesystem::path (WUBANSIA_SOURCE_DIR) / "content", id);
            REQUIRE (b.ok());
            it = cache.emplace (std::string (id), std::move (b.value)).first;
        }
        return it->second;
    }

    Song songFor (const std::string& id, std::uint64_t seed)
    {
        Options o;
        o.seed = seed;
        auto s = generate (bundleFor (id), o);
        REQUIRE (s.ok());
        return std::move (s.value);
    }

    const LaneOut& laneOf (const Song& s, std::string_view id)
    {
        for (const auto& l : s.lanes)
            if (l.id == id)
                return l;
        FAIL ("no lane " << id);
        return s.lanes.front();
    }

    using Degs = std::vector<std::int32_t>;
}

TEST_CASE ("sameShape and containsRun read progressions cyclically", "[gen][harmony]")
{
    const Degs fam = { 0, 5, 3, 4 };
    CHECK (sameShape (fam, Degs { 3, 4, 0, 5 }));
    CHECK (sameShape (fam, fam));
    CHECK_FALSE (sameShape (fam, Degs { 0, 3, 5, 4 }));
    CHECK_FALSE (sameShape (fam, Degs { 0, 5, 3 }));
    CHECK (containsRun (fam, Degs { 4, 0 })); // wraps
    CHECK (containsRun (fam, Degs { 0, 5, 3 }));
    CHECK (containsRun (fam, Degs { 4, 0, 5 }));
    CHECK_FALSE (containsRun (fam, Degs { 5, 0 }));
    CHECK_FALSE (containsRun (fam, Degs { 0, 3 }));
}

TEST_CASE ("the anti-cliche memory holds the legacy shapes and their runs", "[gen][harmony]")
{
    const HarmonySpec& h = bundleFor ("FUT01").style.harmony;
    REQUIRE (h.memory.size() >= 2);
    CHECK (h.memory[0].shape == Degs { 0, 5, 3, 4 }); // i-bVI-iv-v
    CHECK (h.memory[1].shape == Degs { 5, 6, 4, 0 }); // bVI-bVII-v-i, the legacy breakdown shape
    CHECK (clichePenalty (h, Degs { 0, 5, 3, 4 }) == 1000);
    CHECK (clichePenalty (h, Degs { 3, 4, 0, 5 }) == 1000); // a rotation
    CHECK (clichePenalty (h, Degs { 6, 4, 0, 5 }) == 1000);
    CHECK (clichePenalty (h, Degs { 0, 5, 3 }) == 900);
    CHECK (clichePenalty (h, Degs { 5, 6, 4 }) == 900);
    CHECK (clichePenalty (h, Degs { 4, 0, 5 }) == 900); // runs wrap round the memory shape
    CHECK (clichePenalty (h, Degs { 0, 5 }) == 300);
    CHECK (clichePenalty (h, Degs { 6, 4 }) == 300);
    CHECK (clichePenalty (h, Degs { 0, 6 }) == 0); // i-bVII is fine
    CHECK (clichePenalty (h, Degs { 0, 3 }) == 0); // i-iv
    CHECK (clichePenalty (h, Degs { 0, 6, 3 }) == 0);
    // a 3-chord progression whose wrap makes a memory run: 3 -> 4 is in i-bVI-iv-v
    CHECK (clichePenalty (h, Degs { 4, 2, 3 }) == 300);
}

TEST_CASE ("progression candidates follow the slot's rules", "[gen][harmony]")
{
    for (const char* id : { "FUT01", "FUT03" })
    {
        INFO (id);
        const HarmonySpec& h = bundleFor (id).style.harmony;
        const ProgressionSlot* main = h.slot ("main");
        const ProgressionSlot* brk = h.slot ("break");
        REQUIRE (main != nullptr);
        REQUIRE (brk != nullptr);
        for (const std::size_t count : { 2u, 3u })
            for (const Mode mode : { Mode::aeolian, Mode::dorian, Mode::phrygian })
            {
                const auto cands = progressionCandidates (h, *main, mode, count, {});
                std::size_t live = 0;
                for (const auto& c : cands)
                {
                    REQUIRE (c.degrees.size() == count);
                    for (std::size_t i = 0; i < count; ++i)
                        CHECK (c.degrees[i] != c.degrees[(i + 1) % count]);
                    if (c.weight <= 0)
                        continue;
                    ++live;
                    CHECK (std::count (c.degrees.begin(), c.degrees.end(), 0) > 0); // tonic present
                    CHECK (clichePenalty (h, c.degrees) < 1000);
                    for (const auto d : c.degrees)
                        CHECK (h.degrees[static_cast<std::size_t> (mode)][static_cast<std::size_t> (d)] > 0);
                    const auto& colour = h.colour[static_cast<std::size_t> (mode)];
                    if (!colour.empty())
                        CHECK (std::any_of (c.degrees.begin(), c.degrees.end(), [&] (std::int32_t d) {
                            return std::find (colour.begin(), colour.end(), d) != colour.end();
                        }));
                }
                CHECK (live > 0);

                const std::vector<Degs> taken = { Degs { 0, 6 }, Degs { 0, 3, 6 } };
                for (const auto& c : progressionCandidates (h, *brk, mode, count, taken))
                {
                    if (c.weight <= 0)
                        continue;
                    CHECK (c.degrees.front() != 0); // emergent: not on the tonic first...
                    CHECK (std::count (c.degrees.begin(), c.degrees.end(), 0) > 0); // ...but it comes
                    for (const auto& t : taken)
                        CHECK_FALSE (sameShape (c.degrees, t));
                }
            }
    }
}

TEST_CASE ("a harmonic rhythm holds at most 3 chords, so the progression weights fit in 64 bits", "[gen][harmony]")
{
    // The plan's grammar is 2-3 chords over a loop (plus the one-chord pedal, [4]). A weight is the
    // product of a start weight, a move per chord and a degree weight per chord, each at most 100,
    // times (1000 - penalty): for 3 chords at most 100^7 * 1000 = 1e17, and the 7^3 candidates sum
    // to at most 3.43e16. Four chords would reach 100^9 = 1e18 before the penalty: signed overflow.
    constexpr std::int64_t worst3 = std::int64_t (100) * 100 * 100 * 100 * 100 * 100 * 100;
    static_assert (worst3 <= std::numeric_limits<std::int64_t>::max() / 1000); // the penalty's multiply
    static_assert (worst3 <= std::numeric_limits<std::int64_t>::max() / 343); // pickScaled's total

    // the loader refuses a rhythm of 4 units, with the reason
    const std::string four = replaced (styleText ("garage2step"), "{ \"units\": [2, 2], \"w\": 30 }", "{ \"units\": [1, 1, 1, 1], \"w\": 30 }");
    const auto refused = parseStyle (four, "style.json");
    REQUIRE_FALSE (refused.ok());
    CHECK_THAT (refused.error, ContainsSubstring ("harmony.rhythms[0].units"));
    CHECK_THAT (refused.error, ContainsSubstring ("at most 3"));

    // and the grammar itself offers nothing for 4 chords
    const HarmonySpec& h = bundleFor ("FUT01").style.harmony;
    CHECK (progressionCandidates (h, *h.slot ("main"), Mode::aeolian, 4, {}).empty());

    // every candidate the real styles can draw has a weight that didn't wrap
    for (const char* id : { "FUT01", "FUT02", "FUT03", "FUT04" })
    {
        const HarmonySpec& spec = bundleFor (id).style.harmony;
        for (const auto& slot : spec.slots)
            for (const std::size_t count : { 1u, 2u, 3u })
                for (const Mode mode : { Mode::aeolian, Mode::dorian, Mode::phrygian })
                {
                    std::int64_t total = 0;
                    for (const auto& c : progressionCandidates (spec, slot, mode, count, {}))
                    {
                        CHECK (c.weight >= 0);
                        CHECK (c.weight <= worst3);
                        total += c.weight;
                    }
                    CHECK (total >= 0);
                }
    }
}

TEST_CASE ("planHarmony draws varied progressions and harmonic rhythms over 64 seeds", "[gen][harmony]")
{
    for (const char* id : { "FUT01", "FUT03" })
    {
        INFO (id);
        const Style& style = bundleFor (id).style;
        std::map<std::string, int> shapes, rhythms, chords;
        for (std::uint64_t seed = 0; seed < 64; ++seed)
        {
            const HarmonyPlan plan = planHarmony (style, rng::seedKey (seed));
            const Progression* main = plan.slot ("main");
            const Progression* brk = plan.slot ("break");
            REQUIRE (main != nullptr);
            REQUIRE (brk != nullptr);
            for (const Progression* p : { main, brk })
            {
                CHECK (p->degrees.size() >= 2);
                CHECK (p->degrees.size() <= 3);
                CHECK (p->degrees.size() == p->units.size());
                std::int32_t sum = 0;
                for (const auto u : p->units)
                    sum += u;
                CHECK (sum == 4);
                CHECK ((p->loopBars == 2 || p->loopBars == 4));
                CHECK (clichePenalty (style.harmony, p->degrees) < 1000);
            }
            CHECK_FALSE (sameShape (main->degrees, brk->degrees)); // no repeated shape inside a song
            CHECK ((plan.mode == Mode::aeolian || plan.mode == Mode::dorian));
            CHECK (main->mode == plan.mode);
            CHECK ((brk->mode == plan.mode || brk->mode == Mode::phrygian));
            // G1's shape is the pitch-class masks of 16 half-bar slots (tools/legacy_audit), so it
            // sees the mode, the chords, the loop and the harmonic rhythm (and the voicings, which
            // only make shapes more distinct): this key is that shape short of the voicings.
            std::string degrees;
            for (const auto d : main->degrees)
                degrees += std::to_string (d) + "-";
            ++chords[degrees];
            std::string rh = std::to_string (main->loopBars) + ":";
            for (const auto u : main->units)
                rh += std::to_string (u);
            ++rhythms[rh];
            ++shapes[std::string (modeName (main->mode)) + " " + degrees + " " + rh];
            // never 4 x 1-bar slots (G2): 4 chords a bar each can't be drawn at all
            CHECK_FALSE ((main->loopBars == 4 && main->units.size() == 4));
        }
        int top = 0, topChords = 0;
        for (const auto& [k, n] : shapes)
            top = std::max (top, n);
        for (const auto& [k, n] : chords)
            topChords = std::max (topChords, n);
        INFO ("distinct shapes " << shapes.size() << ", top " << top << "; distinct chord sequences " << chords.size() << ", top " << topChords);
        CHECK (shapes.size() >= 32); // G1: distinct shapes on at least half of 64 seeds...
        CHECK (top <= 6); // ...and no shape on more than 10% of them
        CHECK (chords.size() >= 16); // and by chords alone, no rut either
        CHECK (topChords <= 13); // (20%: i-iv and i-IV share their degrees but not their chords)
        CHECK (rhythms.size() >= 4); // G2: at least 4 harmonic-rhythm patterns
    }
}

TEST_CASE ("chord spans tile each section and 40-60% of changes are pushed", "[gen][harmony]")
{
    for (const char* id : { "FUT01", "FUT03" })
        for (std::uint64_t seed = 1; seed <= 32; ++seed)
        {
            INFO (id << " seed " << seed);
            const Song s = songFor (id, seed);
            std::size_t changes = 0, pushed = 0;
            for (const Section& sec : s.sections)
            {
                std::vector<const ChordSpan*> spans;
                for (const auto& c : s.chords)
                    if (static_cast<std::size_t> (c.section) == sec.index)
                        spans.push_back (&c);
                REQUIRE_FALSE (spans.empty());
                CHECK (spans.front()->start == sec.startTick());
                CHECK (spans.front()->pushedBy == 0);
                CHECK (spans.back()->end == sec.endTick());
                for (std::size_t i = 0; i < spans.size(); ++i)
                {
                    CHECK (spans[i]->end > spans[i]->start);
                    if (i + 1 < spans.size())
                        CHECK (spans[i]->end == spans[i + 1]->start);
                    if (i > 0)
                    {
                        ++changes;
                        if (spans[i]->pushedBy > 0)
                        {
                            ++pushed;
                            CHECK ((spans[i]->pushedBy == 120 || spans[i]->pushedBy == 240));
                            CHECK ((spans[i]->start + spans[i]->pushedBy) % (ticksPerBar / 2) == 0); // the unpushed change is on a half-bar
                        }
                    }
                }
                const SectionHarmony& h = s.harmony[sec.index];
                CHECK (h.slot == (sec.kind == "break" ? "break" : "main"));
                CHECK (h.pushPermille >= 400);
                CHECK (h.pushPermille <= 600);
            }
            REQUIRE (changes > 20);
            CHECK (pushed * 100 >= changes * 40);
            CHECK (pushed * 100 <= changes * 60);
        }
}

TEST_CASE ("voicing candidates keep the register span and mud rules", "[gen][harmony]")
{
    const HarmonySpec& h = bundleFor ("FUT01").style.harmony;
    const Range reg { 42, 86 };
    for (const auto& spec : h.voicings)
        for (std::int32_t degree = 0; degree < 7; ++degree)
        {
            INFO (spec.name << " degree " << degree);
            const auto cands = voicingCandidates (spec, 6, Mode::aeolian, degree, reg, h.mud);
            if (degree == 1)
                continue; // ii° may have no placements: it never plays
            CHECK_FALSE (cands.empty());
            for (const auto& v : cands)
            {
                REQUIRE_FALSE (v.keys.empty());
                REQUIRE (v.keys.size() == v.tags.size());
                CHECK (std::is_sorted (v.keys.begin(), v.keys.end()));
                CHECK (std::adjacent_find (v.keys.begin(), v.keys.end()) == v.keys.end());
                CHECK (v.keys.front() >= reg.lo);
                CHECK (v.keys.back() <= reg.hi);
                if (spec.choose == 0)
                {
                    CHECK (v.keys.back() - v.keys.front() >= spec.span.lo);
                    CHECK (v.keys.back() - v.keys.front() <= spec.span.hi);
                    const std::size_t tones = spec.shape.empty() ? spec.degrees.size() : spec.shape.size();
                    CHECK (v.keys.size() == tones);
                }
                else
                    CHECK (v.keys.size() == static_cast<std::size_t> (spec.choose));
                for (std::size_t i = 1; i < v.keys.size(); ++i)
                    if (v.keys[i - 1] < h.mud.below)
                        CHECK (v.keys[i] - v.keys[i - 1] >= h.mud.gap);
                for (std::size_t i = 0; i < v.keys.size(); ++i)
                {
                    const bool inKey = inMinorKey (6, pitchClass (v.keys[i]));
                    CHECK (inKey == (v.tags[i] == 0));
                    if (!spec.shape.empty() && !inKey)
                        CHECK (v.tags[i] == noteTag::planed);
                }
            }
        }
    // Dorian's raised 6th is tagged borrowed
    const VoicingSpec* m7 = h.voicing ("m7");
    REQUIRE (m7 != nullptr);
    bool sawBorrowed = false;
    for (const auto& v : voicingCandidates (*m7, 6, Mode::dorian, 3, reg, h.mud)) // IV: B D# F# A
        for (std::size_t i = 0; i < v.keys.size(); ++i)
            if (pitchClass (v.keys[i]) == 3)
            {
                CHECK (v.tags[i] == noteTag::borrowed);
                sawBorrowed = true;
            }
    CHECK (sawBorrowed);
}

namespace
{
    // The low rubs a chord voicing may not hold: two keys a semitone apart with the lower one under
    // MIDI 64 (E4), or a minor ninth apart with the lower one under 60 (C4). Whole-step seconds are
    // the palette's colour (m7, add9, close sus2) and stay. "" if none, else the first, as text.
    std::string lowRub (const std::vector<std::int32_t>& keys)
    {
        for (std::size_t i = 0; i < keys.size(); ++i)
            for (std::size_t j = 0; j < keys.size(); ++j)
            {
                const std::int32_t lo = keys[i], hi = keys[j];
                if ((hi - lo == 1 && lo < 64) || (hi - lo == 13 && lo < 60))
                    return std::to_string (lo) + " and " + std::to_string (hi);
            }
        return {};
    }
}

TEST_CASE ("no voicing of any style rubs a minor second or ninth low in the stack", "[gen][harmony]")
{
    // Every placement of every voicing in each style, on every degree of every mode, in the kit's
    // chord register: no semitone between voices with the lower under 64, no minor ninth with the
    // lower under 60, and the mud rule's 5 semitones between neighbours below 52 still.
    std::size_t placements = 0;
    for (const char* id : { "FUT01", "FUT02", "FUT03", "FUT04" })
    {
        const Bundle& b = bundleFor (id);
        const HarmonySpec& h = b.style.harmony;
        const Range reg = b.kit.lane ("chords")->range;
        for (const auto& spec : h.voicings)
            for (const Mode mode : { Mode::aeolian, Mode::dorian, Mode::phrygian })
                for (std::int32_t degree = 0; degree < 7; ++degree)
                    for (const auto& v : voicingCandidates (spec, b.kit.tonic, mode, degree, reg, h.mud))
                    {
                        INFO (id << " " << spec.name << " on degree " << degree << " of " << modeName (mode) << ": " << lowRub (v.keys));
                        CHECK (lowRub (v.keys).empty());
                        for (std::size_t i = 1; i < v.keys.size(); ++i)
                            if (v.keys[i - 1] < 52)
                                CHECK (v.keys[i] - v.keys[i - 1] >= 5);
                        ++placements;
                    }
    }
    CHECK (placements > 5000);
}

TEST_CASE ("no chord lane sounds a low minor second or ninth, held pads included", "[gen][harmony]")
{
    // On 64 seeds per kit, at every chord-lane onset, the notes sounding then (a held pad's drone
    // included) hold no low rub.
    for (const char* id : { "FUT01", "FUT02", "FUT03", "FUT04" })
    {
        std::size_t onsets = 0, rubs = 0;
        std::string first;
        for (std::uint64_t seed = 0; seed < 64; ++seed)
        {
            const Song s = songFor (id, seed);
            for (const auto& lane : s.lanes)
            {
                if (lane.kind != LaneKind::chords)
                    continue;
                std::vector<const Note*> active; // the notes sounding at the onset in hand (the score is in tick order)
                for (std::size_t i = 0; i < lane.score.size();)
                {
                    const std::int64_t t = lane.score[i].tick;
                    std::erase_if (active, [t] (const Note* n) { return n->tick + n->length <= t; });
                    for (; i < lane.score.size() && lane.score[i].tick == t; ++i)
                        active.push_back (&lane.score[i]);
                    std::vector<std::int32_t> sounding;
                    for (const Note* n : active)
                        sounding.push_back (n->key);
                    ++onsets;
                    if (const std::string r = lowRub (sounding); !r.empty())
                    {
                        if (first.empty())
                            first = "seed " + std::to_string (seed) + " tick " + std::to_string (t) + ": " + r;
                        ++rubs;
                    }
                }
            }
        }
        INFO (id << ": " << rubs << " of " << onsets << " chord onsets with a low rub, the first " << first);
        CHECK (rubs == 0);
        CHECK (onsets > 1000);
    }
}

TEST_CASE ("voicingCost rewards smooth voice leading and the arc", "[gen][harmony]")
{
    const Voiced a { { 54, 57, 61, 64 }, { 0, 0, 0, 0 } };
    const Voiced near { { 52, 57, 61, 64 }, { 0, 0, 0, 0 } };
    const Voiced far { { 64, 69, 73, 76 }, { 0, 0, 0, 0 } };
    CHECK (voicingCost (near, &a, 64, 59) < voicingCost (far, &a, 64, 59));
    CHECK (voicingCost (a, &a, 64, 59) < voicingCost (near, &a, 64, 59)); // common tones
    // with no previous voicing only the arc and the register count
    CHECK (voicingCost (a, nullptr, 64, 59) == 0 * 4 + 2 * 0 + 0);
    CHECK (voicingCost (far, nullptr, 64, 59) == 2 * 12 + (71 - 59)); // the centroid 70.5 rounds half up to 71
}

TEST_CASE ("chord stacks have a louder top voice and stay in the register", "[gen][harmony]")
{
    for (const char* id : { "FUT01", "FUT03" })
        for (std::uint64_t seed = 1; seed <= 16; ++seed)
        {
            INFO (id << " seed " << seed);
            const Song s = songFor (id, seed);
            const LaneOut& ch = laneOf (s, "chords");
            const Range reg = bundleFor (id).kit.lane ("chords")->range;
            std::map<std::int64_t, std::vector<const Note*>> stacks;
            for (const Note& n : ch.score)
            {
                stacks[n.tick].push_back (&n);
                CHECK (n.key >= reg.lo);
                CHECK (n.key <= reg.hi);
            }
            std::size_t multi = 0;
            for (const auto& [tick, notes] : stacks)
            {
                if (notes.size() < 2)
                    continue;
                ++multi;
                const Note* top = *std::max_element (notes.begin(), notes.end(), [] (const Note* x, const Note* y) { return x->key < y->key; });
                CHECK ((top->flags & noteFlag::top) != 0);
                std::int32_t others = 0;
                for (const Note* n : notes)
                    if (n != top)
                        others = std::max<std::int32_t> (others, n->velocity);
                const std::int32_t lift = top->velocity - others;
                CHECK (lift >= 4); // per-voice velocity: the top voice 4 to 8 over the loudest other voice
                CHECK (lift <= 8);
            }
            CHECK (multi > 20);
            // each section's texture comes from its kind's palette
            for (const Section& sec : s.sections)
            {
                const auto* palette = bundleFor (id).style.harmony.paletteFor (sec.kind);
                REQUIRE (palette != nullptr);
                CHECK (std::find (palette->begin(), palette->end(), s.harmony[sec.index].voicing) != palette->end());
            }
        }
}

TEST_CASE ("every pitched note is in key unless tagged borrowed or planed", "[gen][harmony]")
{
    std::size_t borrowed = 0, planed = 0;
    for (const char* id : { "FUT01", "FUT03" })
        for (std::uint64_t seed = 0; seed < 64; ++seed)
        {
            INFO (id << " seed " << seed);
            const Song s = songFor (id, seed);
            for (const auto& lane : s.lanes)
            {
                if (lane.kind == LaneKind::drums)
                    continue;
                for (const auto* notes : { &lane.score, &lane.perf })
                    for (const Note& n : *notes)
                    {
                        const bool inKey = inMinorKey (s.tonic, pitchClass (n.key));
                        if (!inKey)
                        {
                            INFO (lane.id << " key " << int (n.key) << " tick " << n.tick);
                            CHECK ((n.tags & (noteTag::borrowed | noteTag::planed)) != 0);
                        }
                        borrowed += (n.tags & noteTag::borrowed) ? 1u : 0u;
                        planed += (n.tags & noteTag::planed) ? 1u : 0u;
                    }
            }
        }
    CHECK (borrowed > 0); // Dorian and Phrygian colour do turn up
    CHECK (planed > 0); // and planing
}

namespace
{
    // The pitch classes (a 12-bit mask) the chord lanes voice over each chord span: every chord-lane
    // score note sounding in it.
    std::vector<std::uint32_t> voicedOver (const Song& s)
    {
        std::vector<std::uint32_t> mask (s.chords.size(), 0);
        for (const auto& lane : s.lanes)
            if (lane.kind == LaneKind::chords)
                for (const Note& n : lane.score)
                    for (std::size_t c = 0; c < s.chords.size(); ++c)
                        if (n.tick < s.chords[c].end && n.tick + n.length > s.chords[c].start)
                            mask[c] |= std::uint32_t (1) << pitchClass (n.key);
        return mask;
    }

    std::size_t spanAt (const Song& s, std::int64_t tick)
    {
        const ChordSpan* c = chordAt (s, tick);
        REQUIRE (c != nullptr);
        return static_cast<std::size_t> (c - s.chords.data());
    }

    bool has (std::uint32_t mask, std::int32_t pc) { return ((mask >> pitchClass (pc)) & 1u) != 0; }
}

TEST_CASE ("the bass agrees with the chords it plays under", "[gen][harmony]")
{
    // A bass note the chord lanes don't voice is never a semitone below a voiced tone (a minor 9th
    // over the bass) and never another third, seventh or fifth than the voiced one: planed shapes,
    // sus and quartal stacks don't voice the mode's own chord (a planed C-Eb-G-Bb over an E, a
    // quartal C-F-B over an E). And the bass never plays a major 7th: its seventh is the minor one.
    for (const char* id : { "FUT01", "FUT02", "FUT03", "FUT04" })
    {
        std::size_t checked = 0, unvoiced = 0;
        for (std::uint64_t seed = 0; seed < 32; ++seed)
        {
            const Song s = songFor (id, seed);
            const auto voiced = voicedOver (s);
            for (const auto& lane : s.lanes)
            {
                if (lane.kind != LaneKind::bass)
                    continue;
                for (const Note& n : lane.score)
                {
                    if (n.flags & noteFlag::approach)
                        continue; // a passing tone into the next chord, by design
                    const std::size_t first = spanAt (s, n.tick);
                    CHECK (pitchClass (n.key - s.chords[first].rootPc) != 11);
                    ++checked;
                    // every chord it sounds over, a note held into the next chord included; a slide's
                    // overlap is the glide into its next note, so only its own chord counts
                    for (std::size_t ci = first; ci < s.chords.size() && s.chords[ci].start < n.tick + n.length; ++ci)
                    {
                        if (ci > first && (n.flags & noteFlag::slide))
                            break;
                        const ChordSpan& c = s.chords[ci];
                        const std::int32_t iv = pitchClass (n.key - c.rootPc);
                        INFO (id << " seed " << seed << " " << lane.id << " key " << int (n.key) << " tick " << n.tick << " length " << n.length << " over the chord at " << c.start
                                 << ", root pc " << c.rootPc << ", interval " << iv << ", voiced mask " << voiced[ci]);
                        const std::uint32_t m = voiced[ci];
                        if (m == 0 || has (m, n.key))
                            continue;
                        ++unvoiced;
                        CHECK_FALSE (has (m, n.key + 1));
                        if (iv == 3 || iv == 4)
                            CHECK_FALSE (has (m, c.rootPc + 7 - iv));
                        if (iv == 10)
                            CHECK_FALSE (has (m, c.rootPc + 11));
                        if (iv == 7)
                        {
                            CHECK_FALSE (has (m, c.rootPc + 6));
                            CHECK_FALSE (has (m, c.rootPc + 8));
                        }
                    }
                }
            }
        }
        INFO (id << ": " << checked << " bass notes, " << unvoiced << " of them not voiced by the chords");
        CHECK (checked > 1000);
    }
}

TEST_CASE ("no chord stack voices a minor 9th over its root", "[gen][harmony]")
{
    // A 9th or 2nd a semitone above the root (the Phrygian i's b2, the Aeolian v's, the Dorian ii's)
    // moves up two scale steps, to the 11th or 4th: m9 and add9 become m11 and add11 there, sus2 sus4.
    for (const char* id : { "FUT01", "FUT02", "FUT03", "FUT04" })
        for (std::uint64_t seed = 0; seed < 32; ++seed)
        {
            const Song s = songFor (id, seed);
            for (const auto& lane : s.lanes)
                if (lane.kind == LaneKind::chords)
                    for (const Note& n : lane.score)
                    {
                        const ChordSpan* c = chordAt (s, n.tick);
                        REQUIRE (c != nullptr);
                        INFO (id << " seed " << seed << " key " << int (n.key) << " tick " << n.tick << " over root pc " << c->rootPc);
                        CHECK (pitchClass (n.key - c->rootPc) != 1);
                    }
        }
}

TEST_CASE ("the arp's chord tones agree with the chord lane's stacks", "[gen][harmony]")
{
    // FUT02's arp climbs the mode's own chord tones, so the stacks under it voice the same third
    // and fifth: no 4th over a major third (a quartal stack on a major chord), no other third, no
    // b5 or #5 against its fifth.
    std::size_t checked = 0;
    for (std::uint64_t seed = 0; seed < 32; ++seed)
    {
        const Song s = songFor ("FUT02", seed);
        const auto voiced = voicedOver (s);
        for (const Note& n : laneOf (s, "arp").score)
        {
            const std::size_t ci = spanAt (s, n.tick);
            const std::uint32_t m = voiced[ci];
            const std::int32_t root = s.chords[ci].rootPc;
            const std::int32_t iv = pitchClass (n.key - root);
            if (m == 0 || has (m, n.key))
                continue;
            ++checked;
            INFO ("seed " << seed << " arp key " << int (n.key) << " tick " << n.tick << " over root pc " << root << ", voiced mask " << m);
            if (iv == 3 || iv == 4)
                CHECK_FALSE (has (m, root + 7 - iv));
            if (iv == 4)
                CHECK_FALSE (has (m, root + 5));
            if (iv == 7)
            {
                CHECK_FALSE (has (m, root + 6));
                CHECK_FALSE (has (m, root + 8));
            }
        }
    }
    CHECK (checked > 100);
}

TEST_CASE ("bestVoicing keeps the top voice inside the voicing's top window", "[gen][harmony]")
{
    VoicingSpec vs;
    vs.top = { 65, 78 };
    vs.center = 66;
    const std::vector<Voiced> cands = {
        { { 40, 45, 50 }, { 0, 0, 0 } }, // top 50: far below the window
        { { 52, 57, 62 }, { 0, 0, 0 } }, // top 62: just below it
        { { 59, 64, 69 }, { 0, 0, 0 } }, // top 69: inside
        { { 64, 69, 74 }, { 0, 0, 0 } }, // top 74: inside
    };
    // a previous voicing deep down pulls the voice leading below the window: the window wins
    const Voiced low { { 41, 46, 51 }, { 0, 0, 0 } };
    CHECK (voicingCost (cands[0], &low, 70, 66) < voicingCost (cands[2], &low, 70, 66)); // by cost alone, the low one
    CHECK (bestVoicing (cands, &low, 70, vs) == 2);
    // the second best (a re-voicing) is the next inside the window
    CHECK (bestVoicing (cands, &low, 70, vs, 2) == 3);
    // with nothing inside the window, the cheapest of all
    VoicingSpec high = vs;
    high.top = { 90, 100 };
    CHECK (bestVoicing (cands, &low, 70, high) == 0);
    CHECK (bestVoicing (cands, nullptr, 70, high) == 2); // no lead: the arc and the register decide
    // and only one candidate inside: a re-voicing doesn't leave the window, it keeps that one
    VoicingSpec narrow = vs;
    narrow.top = { 68, 70 };
    CHECK (bestVoicing (cands, &low, 70, narrow) == 2);
    CHECK (bestVoicing (cands, &low, 70, narrow, 2) == 2);
    // outside any window, the re-voicing is the second cheapest of all
    CHECK (bestVoicing (cands, &low, 70, high, 0) == 1);
}

TEST_CASE ("bestVoicing's top window holds both its edges and nothing past them", "[gen][harmony]")
{
    VoicingSpec vs;
    vs.top = { 65, 78 };
    vs.center = 66;
    const Voiced below { { 54, 59, 64 }, { 0, 0, 0 } }; // top 64: lo - 1
    const Voiced atLo { { 55, 60, 65 }, { 0, 0, 0 } }; // top 65: lo
    const Voiced atHi { { 68, 73, 78 }, { 0, 0, 0 } }; // top 78: hi
    const Voiced above { { 69, 74, 79 }, { 0, 0, 0 } }; // top 79: hi + 1
    // led from the outside candidate itself, the voice leading prefers it: the window decides
    CHECK (voicingCost (below, &below, 70, 66) < voicingCost (atLo, &below, 70, 66));
    CHECK (bestVoicing (std::vector<Voiced> { below, atLo }, &below, 70, vs) == 1);
    CHECK (bestVoicing (std::vector<Voiced> { atLo, below }, &below, 70, vs) == 0);
    CHECK (voicingCost (above, &above, 70, 66) < voicingCost (atHi, &above, 70, 66));
    CHECK (bestVoicing (std::vector<Voiced> { above, atHi }, &above, 70, vs) == 1);
    CHECK (bestVoicing (std::vector<Voiced> { atHi, above }, &above, 70, vs) == 0);
}

TEST_CASE ("a b9 over the root moves up to the 11th or 4th in every style's voicings", "[gen][harmony]")
{
    // ADR 0013 §2: a scale step a minor 9th (or 2nd) over the root moves up two steps, to the 11th
    // (or 4th), so m9 and add9 become m11 and add11 there, and sus2 sus4. On the three chords where
    // the 9th is flat (the Phrygian i, the Aeolian v, the Dorian ii), every placement of every
    // voicing with a 2nd or 9th has a 4th over the root and nothing a semitone over it.
    std::size_t checked = 0;
    for (const char* id : { "FUT01", "FUT02", "FUT03", "FUT04" })
    {
        const Bundle& b = bundleFor (id);
        const HarmonySpec& h = b.style.harmony;
        const KitLane& chords = *b.kit.lane ("chords");
        for (const auto& spec : h.voicings)
        {
            const bool ninth = std::any_of (spec.degrees.begin(), spec.degrees.end(), [] (std::int32_t d) { return d % 7 == 1; });
            if (!spec.shape.empty() || spec.choose != 0 || !ninth)
                continue;
            for (const auto& [mode, degree] : { std::pair { Mode::phrygian, 0 }, std::pair { Mode::aeolian, 4 }, std::pair { Mode::dorian, 1 } })
            {
                const std::int32_t rootPc = degreePc (b.kit.tonic, mode, degree);
                REQUIRE (pitchClass (degreePc (b.kit.tonic, mode, degree + 1) - rootPc) == 1); // the flat 9th
                for (const auto& v : voicingCandidates (spec, b.kit.tonic, mode, degree, chords.range, h.mud))
                {
                    INFO (id << " " << spec.name << " on degree " << degree << " of " << modeName (mode));
                    CHECK (std::none_of (v.keys.begin(), v.keys.end(), [&] (std::int32_t k) { return pitchClass (k - rootPc) == 1; }));
                    CHECK (std::any_of (v.keys.begin(), v.keys.end(), [&] (std::int32_t k) { return pitchClass (k - rootPc) == 5; }));
                    ++checked;
                }
            }
        }
    }
    CHECK (checked > 100);
}

TEST_CASE ("drop stabs stay in their register instead of drifting down with the voice leading", "[gen][harmony]")
{
    // Planed and quartal stabs led by nearest motion over a falling loop used to sink an octave per
    // cycle, into the sub's register. Each voicing's top window holds them: FUT01's drop voicings put
    // their top note in 62-79, FUT03's in 65-79, so no stack bottoms out under the pads' high-pass
    // (about 100 Hz: G2, 55), and a drop's lowest notes span about an octave.
    for (const auto& [id, lowTop, highTop] : { std::tuple { "FUT01", 62, 79 }, std::tuple { "FUT03", 65, 79 } })
    {
        std::size_t stacks = 0, under = 0;
        std::int32_t widest = 0;
        for (std::uint64_t seed = 0; seed < 32; ++seed)
        {
            const Song s = songFor (id, seed);
            std::map<std::int64_t, std::vector<std::int32_t>> byTick;
            for (const Note& n : laneOf (s, "chords").score)
                if (s.sections[static_cast<std::size_t> (n.section)].kind == "drop")
                    byTick[n.tick].push_back (n.key);
            std::int32_t lo = 127, hi = 0;
            for (const auto& [tick, keys] : byTick)
            {
                const auto [mn, mx] = std::minmax_element (keys.begin(), keys.end());
                INFO (id << " seed " << seed << " drop stack at " << tick << ": " << *mn << " to " << *mx);
                CHECK (*mx >= lowTop);
                CHECK (*mx <= highTop);
                ++stacks;
                under += *mn < 55 ? 1u : 0u;
                lo = std::min (lo, *mn);
                hi = std::max (hi, *mn);
            }
            widest = std::max (widest, hi - lo);
        }
        INFO (id << ": " << stacks << " drop stacks, " << under << " under G2; the widest span of a drop's lowest notes " << widest);
        CHECK (stacks > 1000);
        CHECK (under * 100 <= stacks); // at most 1%
        CHECK (widest <= 17);
    }
}
