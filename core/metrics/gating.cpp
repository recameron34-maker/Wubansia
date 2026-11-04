// The gating metrics G1-G10 (tools/legacy_audit/README.md, "Gating metrics"), ported from
// tools/legacy_audit/metrics.js function by function. Keys, their order and every number follow the
// audit's result objects, so a result compares key by key with core/tests/legacy_expected.json.

#include "metrics/histogram.h"
#include "metrics/metrics.h"

#include "util/ints.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <iterator>
#include <map>
#include <set>
#include <tuple>
#include <utility>

namespace wubansia::metrics
{
    using detail::numberHistogram;
    using detail::stringHistogram;
    using detail::topOf;
    using util::floorDiv;
    using util::floorMod;

    // ---------------- the bands of G3, G5, G6 and G7 (metrics.h) ----------------

    namespace bands
    {
        bool g3 (std::int64_t drumWindows, std::int64_t drumRepeated, std::int64_t bassWindows, std::int64_t bassRepeated) noexcept
        {
            return drumWindows > 0 && bassWindows > 0 && 10 * drumRepeated <= 4 * drumWindows && 10 * bassRepeated <= 3 * bassWindows;
        }

        bool g5Drums (std::int64_t windows, std::int64_t medianDistinct, std::int64_t medianSdMilli) noexcept
        {
            return windows > 0 && medianDistinct >= 10 && medianSdMilli >= 6000 && medianSdMilli <= 22000;
        }

        bool g5Melodic (std::int64_t windows, std::int64_t medianDistinct, std::int64_t medianSdMilli) noexcept
        {
            return windows > 0 && medianDistinct >= 6 && medianSdMilli >= 8000;
        }

        bool g6 (std::int64_t stacks, std::int64_t twoVelocities, std::int64_t maxSpreadTicks) noexcept
        {
            return stacks > 0 && 10 * twoVelocities >= 7 * stacks && maxSpreadTicks <= 4;
        }

        bool g7 (std::int64_t files, std::int64_t medianSdMilli, std::int64_t medianLag1Pm, std::int64_t medianAlphaMilli) noexcept
        {
            return files > 0 && medianSdMilli >= 1500 && medianSdMilli <= 6000 && medianLag1Pm >= 200 && medianAlphaMilli >= 650 && medianAlphaMilli <= 1100;
        }
    }

    namespace
    {
        // diversity () of metrics.js: how many of the values are measured, distinct, and the most common.
        Value diversity (const std::vector<std::optional<std::string>>& values, std::size_t seeds)
        {
            std::map<std::string, std::int64_t> hist;
            std::int64_t measured = 0;
            for (const auto& v : values)
                if (v)
                {
                    ++measured;
                    ++hist[*v];
                }
            const auto top = topOf (hist);
            const std::int64_t topCount = top ? top->second : 0;
            Value d = Value::object();
            d.set ("seeds", seeds);
            d.set ("measured", measured);
            d.set ("distinct", hist.size());
            d.set ("top", topCount);
            d.set ("distinct_pm", permille (static_cast<std::int64_t> (hist.size()), measured));
            d.set ("top_pm", permille (topCount, measured));
            d.set ("top_shape", top ? top->first : std::string());
            return d;
        }

        // ---- G3 ----

        // The notes of a list sorted by tick in bars [bar0, bar0 + 4), by floorDiv (tick, 1920).
        std::array<std::vector<Note>, 4> fourBars (std::span<const Note> notes, std::int64_t bar0)
        {
            std::array<std::vector<Note>, 4> bars;
            for (const auto& n : between (notes, bar0 * barTicks, (bar0 + 4) * barTicks))
                bars[static_cast<std::size_t> (floorDiv (n.tick, barTicks) - bar0)].push_back (n);
            return bars;
        }

        bool fourOnTheFloor (const std::vector<Note>& bar)
        {
            for (const std::int64_t beat : { 0, 480, 960, 1440 })
            {
                const bool hit = std::any_of (bar.begin(), bar.end(), [beat] (const Note& n) { return std::llabs (floorMod (n.tick, barTicks) - beat) <= 2; });
                if (!hit)
                    return false;
            }
            return true;
        }

        struct G3Lane
        {
            bool drums = false;
            int key = 0;
            std::vector<Note> notes;
        };

        struct G3Counts
        {
            std::int64_t drumWindows = 0, drumRepeated = 0, exempt = 0, bassWindows = 0, bassRepeated = 0;
        };

        G3Counts g3Windows (const Song& song)
        {
            std::vector<G3Lane> lanes;
            for (const Track* t : tracksWithRole (song, Role::drums))
            {
                std::map<int, std::vector<Note>> byKey; // keys in numeric order, notes in track order
                for (const auto& n : t->notes)
                    byKey[n.key].push_back (n);
                for (auto& [key, notes] : byKey)
                    lanes.push_back ({ true, key, std::move (notes) });
            }
            for (const Track* t : tracksWithRole (song, Role::bass))
                lanes.push_back ({ false, 0, t->notes });

            G3Counts out;
            for (const auto& sec : sectionsOf (song))
                for (std::int64_t w = 0; w + 4 <= sec.bars; w += 4)
                {
                    const std::int64_t b0 = sec.bar + w;
                    if ((b0 + 4) * barTicks > song.endTick)
                        continue; // wholly inside the file
                    for (const auto& lane : lanes)
                    {
                        const auto bars = fourBars (lane.notes, b0);
                        if (std::all_of (bars.begin(), bars.end(), [] (const std::vector<Note>& b) { return b.empty(); }))
                            continue;
                        // drums: each bar's multiset of (exact position, velocity); bass: its set of positions
                        std::array<std::vector<std::pair<std::int64_t, int>>, 4> prints;
                        for (std::size_t i = 0; i < 4; ++i)
                        {
                            for (const auto& n : bars[i])
                                prints[i].emplace_back (floorMod (n.tick, barTicks), lane.drums ? n.velocity : 0);
                            std::ranges::sort (prints[i]);
                            if (!lane.drums)
                                prints[i].erase (std::unique (prints[i].begin(), prints[i].end()), prints[i].end());
                        }
                        const bool repeated = !prints[0].empty() && prints[1] == prints[0] && prints[2] == prints[0] && prints[3] == prints[0];
                        if (lane.drums)
                        {
                            if (drumGroup (lane.key) == Group::kick && std::all_of (bars.begin(), bars.end(), fourOnTheFloor))
                            {
                                ++out.exempt;
                                continue;
                            }
                            ++out.drumWindows;
                            out.drumRepeated += repeated ? 1 : 0;
                        }
                        else
                        {
                            ++out.bassWindows;
                            out.bassRepeated += repeated ? 1 : 0;
                        }
                    }
                }
            return out;
        }

        // ---- G5 ----

        bool isBackbeat (std::int64_t tick)
        {
            const std::int64_t p = floorMod (tick, barTicks);
            return (p >= 450 && p <= 510) || (p >= 1410 && p <= 1470);
        }

        struct G5Group
        {
            std::string_view name;
            std::size_t minNotes;
        };
        constexpr std::array<G5Group, 5> g5Groups = { { { "hats", 16 }, { "perc", 16 }, { "ghosts", 10 }, { "bass", 8 }, { "lead", 8 } } };

        std::vector<Note> g5Notes (const Song& song, std::size_t group)
        {
            switch (group)
            {
                case 0:
                    return drumNotesIn (song, hatsGroups);
                case 1:
                    return drumNotesIn (song, percGroups);
                case 2:
                {
                    auto n = drumNotesIn (song, snareFamily);
                    std::erase_if (n, [] (const Note& x) { return isBackbeat (x.tick); });
                    return n;
                }
                case 3:
                    return notesWithRole (song, Role::bass);
                default:
                    return notesWithRole (song, Role::lead);
            }
        }

        struct VelocityWindow
        {
            std::int64_t distinct = 0;
            std::int64_t sd = 0;
        };

        Value medians (const std::vector<VelocityWindow>& windows)
        {
            std::vector<std::int64_t> distinct, sd;
            for (const auto& w : windows)
            {
                distinct.push_back (w.distinct);
                sd.push_back (w.sd);
            }
            Value v = Value::object();
            v.set ("windows", windows.size());
            v.set ("median_distinct", lowerMedian (std::move (distinct)).value_or (-1));
            v.set ("median_sd_milli", lowerMedian (std::move (sd)).value_or (-1));
            return v;
        }

        // ---- G7, G8 ----

        constexpr std::int64_t undefinedValue = -9999; // lag-1 or alpha that can't be computed

        // ---- G9 ----

        struct DrumBar
        {
            std::vector<std::pair<Group, std::int64_t>> add; // (group, step) of snare, clap, rim, tom and perc notes
            std::vector<std::int64_t> kick; // kick steps
            std::vector<std::int64_t> all; // step * 128 + key of every non-hats note
        };

        bool fillGroup (Group g) { return g == Group::snare || g == Group::clap || g == Group::rim || g == Group::tom || g == Group::perc; }

        std::vector<DrumBar> drumStepSets (const Song& song, std::int64_t bar0, std::int64_t nBars)
        {
            std::vector<DrumBar> bars (static_cast<std::size_t> (std::max<std::int64_t> (nBars, 0)));
            for (const auto& n : song.drumNotes)
            {
                const std::int64_t s = step16 (n.tick);
                const std::int64_t b = floorDiv (s, std::int64_t { 16 }) - bar0;
                if (b < 0 || b >= nBars)
                    continue;
                DrumBar& bar = bars[static_cast<std::size_t> (b)];
                const std::int64_t st = floorMod (s, std::int64_t { 16 });
                if (fillGroup (n.group))
                    bar.add.emplace_back (n.group, st);
                if (n.group == Group::kick)
                    bar.kick.push_back (st);
                if (n.group != Group::hats)
                    bar.all.push_back (st * 128 + n.key);
            }
            for (auto& bar : bars)
            {
                std::ranges::sort (bar.add);
                bar.add.erase (std::unique (bar.add.begin(), bar.add.end()), bar.add.end());
                std::ranges::sort (bar.kick);
                bar.kick.erase (std::unique (bar.kick.begin(), bar.kick.end()), bar.kick.end());
                std::ranges::sort (bar.all);
                bar.all.erase (std::unique (bar.all.begin(), bar.all.end()), bar.all.end());
            }
            return bars;
        }

        struct Fill
        {
            std::int64_t index = 0; // section bar
            std::vector<std::int64_t> content; // the sorted distinct step * 128 + key of its non-hats drum notes
        };

        // The fill bars of a section against its own profile (README, "G9").
        std::vector<Fill> fillBarsOf (const Song& song, const Section& sec)
        {
            if (sec.bars < 4)
                return {};
            const auto bars = drumStepSets (song, sec.bar, sec.bars);
            std::map<std::pair<Group, std::int64_t>, std::int64_t> addCount;
            std::map<std::int64_t, std::int64_t> kickCount;
            for (const auto& b : bars)
            {
                for (const auto& x : b.add)
                    ++addCount[x];
                for (const auto s : b.kick)
                    ++kickCount[s];
            }
            std::vector<std::int64_t> commonKicks;
            for (const auto& [s, c] : kickCount)
                if (4 * c >= 3 * sec.bars)
                    commonKicks.push_back (s);
            std::vector<Fill> out;
            for (std::size_t i = 0; i < bars.size(); ++i)
            {
                std::int64_t rare = 0;
                for (const auto& x : bars[i].add)
                    if (8 * addCount[x] <= sec.bars)
                        ++rare;
                std::int64_t missing = 0;
                for (const auto s : commonKicks)
                    if (!std::binary_search (bars[i].kick.begin(), bars[i].kick.end(), s))
                        ++missing;
                if (missing >= 2 || rare + missing >= 3)
                    out.push_back ({ static_cast<std::int64_t> (i), bars[i].all });
            }
            return out;
        }

        struct Gesture
        {
            std::int64_t bar = 0;
            int kinds = 0; // how many of fill, fx, cymbal, gap
        };

        std::vector<Gesture> boundaryGestures (const Song& song, const std::vector<std::vector<Fill>>& fillsBySection)
        {
            const auto fx = notesWithRole (song, Role::fx);
            const auto cymbals = drumNotesIn (song, Group::cymbal);
            std::vector<Gesture> out;
            for (std::size_t k = 1; k < song.sections.size(); ++k)
            {
                const std::int64_t s = song.sections[k].tick;
                const Section& prev = song.sections[k - 1];
                const std::int64_t lastBar = floorDiv (s, barTicks) - 1 - prev.bar;
                Gesture g;
                g.bar = floorDiv (s, barTicks);
                if (std::any_of (fillsBySection[k - 1].begin(), fillsBySection[k - 1].end(), [lastBar] (const Fill& f) { return f.index == lastBar; }))
                    ++g.kinds;
                if (!between (fx, s - 2 * barTicks, s + 480).empty())
                    ++g.kinds;
                if (std::any_of (cymbals.begin(), cymbals.end(), [s] (const Note& n) { return std::llabs (n.tick - s) <= 60; }))
                    ++g.kinds;
                if (between (song.drumNotes, s - 240, s).empty() && !between (song.drumNotes, s - barTicks, s - 240).empty())
                    ++g.kinds;
                out.push_back (g);
            }
            return out;
        }

        // ---- G10 ----

        std::vector<std::int64_t> salientIntervals (const Song& song)
        {
            std::set<std::int64_t> bars;
            for (const auto& s : song.sections)
                bars.insert (s.bar);
            bars.insert (0);
            const auto ch = macroChangeBars (song);
            for (const auto* list : { &ch.bass, &ch.chords, &ch.lead })
                for (const auto& c : *list)
                    bars.insert (c.bar);
            std::vector<std::int64_t> iv;
            for (auto it = bars.begin(); it != bars.end(); ++it)
                if (it != bars.begin())
                    iv.push_back (*it - *std::prev (it));
            return iv;
        }
    }

    // ---------------- G1 ----------------

    std::optional<std::string> progressionShape (const Song& song, std::string_view kind)
    {
        const Section* sec = firstSectionOf (song, kind);
        if (sec == nullptr)
            return {};
        const auto stacks = harmonyStacks (song);
        const auto slots = harmonySlots (stacks, sec->tick, 16);
        if (!slots)
            return {};
        const int r = (*slots)[0]->low % 12;
        std::string shape;
        for (std::size_t j = 0; j < slots->size(); ++j)
        {
            if (j > 0)
                shape += '.';
            shape += maskHex (rotateMask ((*slots)[j]->mask, r));
        }
        return shape;
    }

    Value g1 (Songs songs)
    {
        Value res = Value::object();
        std::vector<std::optional<std::string>> shapes[2];
        bool pass[2] = { false, false };
        const std::array<std::string_view, 2> kinds = { "break", "drop" };
        for (std::size_t k = 0; k < kinds.size(); ++k)
        {
            for (const Song* s : songs)
                shapes[k].push_back (progressionShape (*s, kinds[k]));
            Value d = diversity (shapes[k], songs.size());
            pass[k] = d.num ("measured") == d.num ("seeds") && 2 * d.num ("distinct") >= d.num ("measured") && 10 * d.num ("top") <= d.num ("measured");
            d.set ("pass", pass[k]);
            res.set (kinds[k], std::move (d));
        }
        res.set ("pass", pass[0] && pass[1]);
        Value perFile = Value::object();
        for (std::size_t i = 0; i < songs.size(); ++i)
        {
            Value f = Value::object();
            f.set ("break", shapes[0][i].value_or (""));
            f.set ("drop", shapes[1][i].value_or (""));
            perFile.set (songs[i]->path, std::move (f));
        }
        res.set ("per_file", std::move (perFile));
        return res;
    }

    // ---------------- G2 ----------------

    namespace
    {
        // The run lengths of equal consecutive slot harmonies over the first 4 bars of the first drop
        // (a file without markers: its first 4 bars), in half bars.
        std::optional<std::vector<int>> harmonicRuns (const Song& song)
        {
            const auto all = sectionsOf (song);
            const Section* sec = firstSectionOf (song, "drop");
            if (sec == nullptr && song.sections.empty())
                sec = &all[0];
            if (sec == nullptr)
                return {};
            const auto stacks = harmonyStacks (song);
            const auto slots = harmonySlots (stacks, sec->tick, 8);
            if (!slots)
                return {};
            std::vector<int> runs;
            int run = 1;
            for (std::size_t j = 1; j < 8; ++j)
            {
                if ((*slots)[j]->mask == (*slots)[j - 1]->mask)
                    ++run;
                else
                {
                    runs.push_back (run);
                    run = 1;
                }
            }
            runs.push_back (run);
            return runs;
        }

        std::string joinRuns (const std::vector<int>& runs)
        {
            std::string out;
            for (const int r : runs)
                out += (out.empty() ? "" : ",") + std::to_string (r);
            return out;
        }
    }

    std::optional<std::string> harmonicRhythm (const Song& song)
    {
        const auto runs = harmonicRuns (song);
        if (!runs)
            return {};
        return joinRuns (*runs);
    }

    Value g2 (Songs songs)
    {
        std::vector<std::optional<std::string>> patterns;
        std::map<std::string, std::int64_t> hist;
        std::int64_t measured = 0, wholeBars = 0;
        for (const Song* s : songs)
        {
            const auto runs = harmonicRuns (*s);
            patterns.push_back (runs ? std::optional<std::string> (joinRuns (*runs)) : std::nullopt);
            if (!runs)
                continue;
            ++measured;
            ++hist[*patterns.back()];
            // changes on bar lines only: every run a whole number of bars, an even number of half bars
            wholeBars += std::all_of (runs->begin(), runs->end(), [] (int r) { return r % 2 == 0; }) ? 1 : 0;
        }
        const std::int64_t fourByOne = hist.contains ("2,2,2,2") ? hist["2,2,2,2"] : 0;
        Value res = Value::object();
        res.set ("seeds", songs.size());
        res.set ("measured", measured);
        res.set ("four_by_one_bar", fourByOne);
        res.set ("four_by_one_bar_pm", permille (fourByOne, measured));
        res.set ("patterns", hist.size());
        res.set ("changes_on_bar_lines_only", wholeBars);
        res.set ("pass", measured == static_cast<std::int64_t> (songs.size()) && 10 * fourByOne <= 3 * measured && hist.size() >= 4);
        res.set ("histogram", stringHistogram (hist, "pattern"));
        Value perFile = Value::object();
        for (std::size_t i = 0; i < songs.size(); ++i)
            perFile.set (songs[i]->path, patterns[i].value_or (""));
        res.set ("per_file", std::move (perFile));
        return res;
    }

    // ---------------- G3 ----------------

    Value g3 (Songs songs)
    {
        Value perFile = Value::object();
        G3Counts total;
        for (const Song* s : songs)
        {
            const auto w = g3Windows (*s);
            Value f = Value::object();
            f.set ("drum_windows", w.drumWindows);
            f.set ("drum_repeated", w.drumRepeated);
            f.set ("kick_4x4_exempt", w.exempt);
            f.set ("bass_windows", w.bassWindows);
            f.set ("bass_repeated", w.bassRepeated);
            perFile.set (s->path, std::move (f));
            total.drumWindows += w.drumWindows;
            total.drumRepeated += w.drumRepeated;
            total.exempt += w.exempt;
            total.bassWindows += w.bassWindows;
            total.bassRepeated += w.bassRepeated;
        }
        Value res = Value::object();
        res.set ("drum_windows", total.drumWindows);
        res.set ("drum_repeated", total.drumRepeated);
        res.set ("drum_repeated_pm", permille (total.drumRepeated, total.drumWindows));
        res.set ("kick_4x4_exempt", total.exempt);
        res.set ("bass_windows", total.bassWindows);
        res.set ("bass_repeated", total.bassRepeated);
        res.set ("bass_repeated_pm", permille (total.bassRepeated, total.bassWindows));
        res.set ("pass", bands::g3 (total.drumWindows, total.drumRepeated, total.bassWindows, total.bassRepeated));
        res.set ("per_file", std::move (perFile));
        return res;
    }

    // ---------------- G4 ----------------

    MacroChanges macroChangeBars (const Song& song)
    {
        MacroChanges res;
        const auto sections = sectionsOf (song);
        const std::array<std::pair<Role, std::vector<MacroChange>*>, 3> roles = { { { Role::bass, &res.bass }, { Role::chords, &res.chords }, { Role::lead, &res.lead } } };
        for (const auto& [role, list] : roles)
            for (const Track* t : tracksWithRole (song, role))
                for (const auto& sec : sections)
                {
                    if (sec.bars < 9)
                        continue;
                    const auto prints = stepBars (t->notes, sec.bar, sec.bars);
                    for (const std::int64_t i : macroChanges (prints))
                        list->push_back ({ sec.bar + i, i % 8 });
                }
        return res;
    }

    Value g4 (Songs songs)
    {
        std::int64_t changes[3] = { 0, 0, 0 }, onPhrase[3] = { 0, 0, 0 };
        constexpr std::array<std::string_view, 3> names = { "bass", "chords", "lead" };
        Value perFile = Value::object();
        for (const Song* s : songs)
        {
            const auto ch = macroChangeBars (*s);
            const std::array<const std::vector<MacroChange>*, 3> lists = { &ch.bass, &ch.chords, &ch.lead };
            Value f = Value::object();
            for (std::size_t r = 0; r < 3; ++r)
            {
                const auto all = static_cast<std::int64_t> (lists[r]->size());
                const auto phrase = std::count_if (lists[r]->begin(), lists[r]->end(), [] (const MacroChange& c) { return c.pos == 0 || c.pos == 7; });
                changes[r] += all;
                onPhrase[r] += phrase;
                f.set (std::string (names[r]) + "_changes", all);
                f.set (std::string (names[r]) + "_on_phrase", phrase);
            }
            perFile.set (s->path, std::move (f));
        }
        Value res = Value::object();
        bool pass = true;
        for (std::size_t r = 0; r < 3; ++r)
        {
            Value v = Value::object();
            v.set ("changes", changes[r]);
            v.set ("on_phrase", onPhrase[r]);
            v.set ("on_phrase_pm", permille (onPhrase[r], changes[r]));
            res.set (names[r], std::move (v));
            pass = pass && changes[r] > 0 && 2 * onPhrase[r] >= changes[r];
        }
        res.set ("pass", pass);
        res.set ("per_file", std::move (perFile));
        return res;
    }

    // ---------------- G5 ----------------

    Value g5 (Songs songs)
    {
        std::array<std::vector<VelocityWindow>, 5> all;
        Value perFile = Value::object();
        for (const Song* s : songs)
        {
            Value f = Value::object();
            const auto sections = sectionsOf (*s);
            for (std::size_t g = 0; g < g5Groups.size(); ++g)
            {
                const auto notes = g5Notes (*s, g);
                std::vector<VelocityWindow> windows;
                for (const auto& sec : sections)
                {
                    if (sec.kind == "build")
                        continue; // rolls ramp velocity on purpose
                    for (std::int64_t w = 0; w + 8 <= sec.bars; w += 8)
                    {
                        const std::int64_t t0 = (sec.bar + w) * barTicks;
                        std::vector<std::int64_t> v;
                        for (const auto& n : between (notes, t0, t0 + 8 * barTicks))
                            v.push_back (n.velocity);
                        if (v.size() >= g5Groups[g].minNotes)
                            windows.push_back ({ countDistinct (v), *sdMilli (v) });
                    }
                }
                f.set (g5Groups[g].name, medians (windows));
                all[g].insert (all[g].end(), windows.begin(), windows.end());
            }
            perFile.set (s->path, std::move (f));
        }
        Value res = Value::object();
        for (std::size_t g = 0; g < g5Groups.size(); ++g)
            res.set (g5Groups[g].name, medians (all[g]));
        const auto drumOk = [&res] (std::string_view g) {
            const Value& v = res[g];
            return bands::g5Drums (v.num ("windows"), v.num ("median_distinct"), v.num ("median_sd_milli"));
        };
        const auto melodicOk = [&res] (std::string_view g) {
            const Value& v = res[g];
            return bands::g5Melodic (v.num ("windows"), v.num ("median_distinct"), v.num ("median_sd_milli"));
        };
        res.set ("pass", drumOk ("hats") && drumOk ("perc") && drumOk ("ghosts") && melodicOk ("bass") && melodicOk ("lead"));
        res.set ("per_file", std::move (perFile));
        return res;
    }

    Value partLevels (Songs songs)
    {
        std::set<std::int64_t> hats, perc, snare, kick, bass, chords, lead;
        std::int64_t most = 0;
        for (const Song* s : songs)
        {
            std::set<std::int64_t> file;
            for (const auto& n : s->drumNotes)
            {
                file.insert (n.velocity);
                if (n.group == Group::hats)
                    hats.insert (n.velocity);
                else if (n.group == Group::perc || n.group == Group::tom || n.group == Group::rim)
                    perc.insert (n.velocity);
                else if (n.group == Group::snare || n.group == Group::clap)
                    snare.insert (n.velocity);
                else if (n.group == Group::kick)
                    kick.insert (n.velocity);
            }
            most = std::max (most, static_cast<std::int64_t> (file.size()));
            for (const auto& n : notesWithRole (*s, Role::bass))
                bass.insert (n.velocity);
            for (const auto& n : notesWithRole (*s, Role::chords))
                chords.insert (n.velocity);
            for (const auto& n : notesWithRole (*s, Role::lead))
                lead.insert (n.velocity);
        }
        const auto joined = [] (const std::set<std::int64_t>& v) {
            std::string out;
            for (const auto x : v)
                out += (out.empty() ? "" : ",") + std::to_string (x);
            return out;
        };
        Value res = Value::object();
        res.set ("most_levels_in_one_drum_file", most);
        res.set ("hats", joined (hats));
        res.set ("perc", joined (perc));
        res.set ("snareFamily", joined (snare));
        res.set ("kick", joined (kick));
        res.set ("bass", joined (bass));
        res.set ("chords", joined (chords));
        res.set ("lead", joined (lead));
        return res;
    }

    // ---------------- G6 ----------------

    Value g6 (Songs songs)
    {
        std::int64_t stacks = 0, twoVelocities = 0, maxSpread = 0;
        std::map<std::int64_t, std::int64_t> spreads;
        Value perFile = Value::object();
        for (const Song* s : songs)
        {
            std::int64_t fs = 0, ft = 0;
            for (const Track* t : tracksWithRole (*s, Role::chords))
                for (const auto& st : stacksOf (t->notes))
                {
                    if (st.notes.size() < 2)
                        continue;
                    ++fs;
                    std::vector<std::int64_t> v;
                    for (const auto& n : st.notes)
                        v.push_back (n.velocity);
                    if (countDistinct (v) >= 2)
                        ++ft;
                    maxSpread = std::max (maxSpread, st.spread());
                    ++spreads[st.spread()];
                }
            stacks += fs;
            twoVelocities += ft;
            Value f = Value::object();
            f.set ("stacks", fs);
            f.set ("two_velocities", ft);
            perFile.set (s->path, std::move (f));
        }
        Value res = Value::object();
        res.set ("stacks", stacks);
        res.set ("two_velocities", twoVelocities);
        res.set ("two_velocities_pm", permille (twoVelocities, stacks));
        res.set ("max_spread_ticks", maxSpread);
        res.set ("spread_histogram", numberHistogram (spreads));
        res.set ("pass", bands::g6 (stacks, twoVelocities, maxSpread));
        res.set ("per_file", std::move (perFile));
        return res;
    }

    // ---------------- G7 ----------------

    Value g7 (Songs songs)
    {
        struct Lane
        {
            std::string_view name;
            std::span<const Group> groups;
        };
        const std::array<Lane, 2> lanes = { { { "hats", hatsGroups }, { "perc", percGroups } } };
        std::array<std::vector<Value>, 2> measured; // the files with 64 residuals or more
        Value perFile = Value::object();
        for (const Song* s : songs)
        {
            Value f = Value::object();
            for (std::size_t l = 0; l < lanes.size(); ++l)
            {
                const auto x = residuals (*s, drumNotesIn (*s, lanes[l].groups));
                Value r = Value::object();
                r.set ("n", x.size());
                r.set ("sd_milli", sdMilli (x).value_or (-1));
                r.set ("lag1_pm", x.size() >= 64 ? lag1Permille (x).value_or (undefinedValue) : undefinedValue);
                r.set ("alpha_milli", dfaAlphaMilli (x).value_or (undefinedValue));
                if (x.size() >= 64)
                    measured[l].push_back (r);
                f.set (lanes[l].name, std::move (r));
            }
            perFile.set (s->path, std::move (f));
        }
        Value res = Value::object();
        bool pass = true;
        for (std::size_t l = 0; l < lanes.size(); ++l)
        {
            std::vector<std::int64_t> sd, lag1, alpha;
            std::int64_t alphaUndefined = 0;
            for (const Value& r : measured[l])
            {
                sd.push_back (r.num ("sd_milli"));
                if (r.num ("lag1_pm") != undefinedValue)
                    lag1.push_back (r.num ("lag1_pm"));
                if (r.num ("alpha_milli") != undefinedValue)
                    alpha.push_back (r.num ("alpha_milli"));
                else
                    ++alphaUndefined;
            }
            Value v = Value::object();
            v.set ("files", measured[l].size());
            v.set ("median_sd_milli", lowerMedian (sd).value_or (-1));
            v.set ("median_lag1_pm", lowerMedian (lag1).value_or (undefinedValue));
            v.set ("median_alpha_milli", lowerMedian (alpha).value_or (undefinedValue));
            v.set ("alpha_undefined", alphaUndefined);
            pass = pass && bands::g7 (v.num ("files"), v.num ("median_sd_milli"), v.num ("median_lag1_pm"), v.num ("median_alpha_milli"));
            res.set (lanes[l].name, std::move (v));
        }
        res.set ("pass", pass);
        res.set ("per_file", std::move (perFile));
        return res;
    }

    // ---------------- G8 ----------------

    Value g8 (Songs songs)
    {
        std::int64_t measured = 0, passing = 0;
        std::map<std::int64_t, std::int64_t> hist;
        Value perFile = Value::object();
        for (const Song* s : songs)
        {
            const Section* sec = firstSectionOf (*s, "drop");
            Value f = Value::object();
            if (sec == nullptr)
            {
                f.set ("lanes", "");
                f.set ("distinct", -1);
                perFile.set (s->path, std::move (f));
                continue;
            }
            // kick is the anchor and is left out
            std::vector<std::pair<std::string, std::vector<Note>>> lanes;
            lanes.emplace_back ("snare", drumNotesIn (*s, snareFamily));
            lanes.emplace_back ("hats", drumNotesIn (*s, hatsGroups));
            lanes.emplace_back ("perc", drumNotesIn (*s, percGroups));
            for (const Role role : { Role::bass, Role::chords, Role::lead, Role::vox })
                for (const Track* t : tracksWithRole (*s, role))
                    lanes.emplace_back (std::string (roleName (role)) + ":" + std::to_string (t->index), t->notes);
            std::string text;
            std::vector<std::int64_t> delays;
            for (const auto& [name, notes] : lanes)
                if (const auto d = swingDelay (between (notes, sec->tick, sec->end), 16))
                {
                    text += (text.empty() ? "" : " ") + name + "=" + std::to_string (*d);
                    delays.push_back (*d);
                }
            const std::int64_t k = distinctSwings (delays);
            ++measured;
            passing += k >= 3 ? 1 : 0;
            ++hist[k];
            f.set ("lanes", text);
            f.set ("distinct", k);
            perFile.set (s->path, std::move (f));
        }
        Value res = Value::object();
        res.set ("seeds", songs.size());
        res.set ("measured", measured);
        res.set ("with_3_or_more", passing);
        res.set ("distinct_histogram", numberHistogram (hist));
        res.set ("pass", measured == static_cast<std::int64_t> (songs.size()) && passing == measured);
        res.set ("per_file", std::move (perFile));
        return res;
    }

    // ---------------- G9 ----------------

    Value g9 (Songs songs)
    {
        std::int64_t boundaries = 0, withGesture = 0;
        std::map<std::int64_t, std::int64_t> positions;
        std::vector<std::int64_t> distinctShares;
        Value perFile = Value::object();
        for (const Song* s : songs)
        {
            std::vector<std::vector<Fill>> fills;
            for (const auto& sec : s->sections)
                fills.push_back (fillBarsOf (*s, sec));
            const auto gestures = boundaryGestures (*s, fills);
            const auto gw = std::count_if (gestures.begin(), gestures.end(), [] (const Gesture& g) { return g.kinds > 0; });
            boundaries += static_cast<std::int64_t> (gestures.size());
            withGesture += gw;

            std::vector<const Fill*> songFills;
            for (std::size_t k = 0; k < s->sections.size(); ++k)
                if (s->sections[k].kind != "build") // a build is one long gesture
                    for (const auto& f : fills[k])
                    {
                        songFills.push_back (&f);
                        ++positions[f.index % 8];
                    }
            std::set<std::vector<std::int64_t>> contents;
            for (const Fill* f : songFills)
                contents.insert (f->content);
            const auto distinct = static_cast<std::int64_t> (contents.size());
            if (songFills.size() >= 2)
                distinctShares.push_back (permille (distinct, static_cast<std::int64_t> (songFills.size())));

            std::string fillPositions, missing;
            for (const Fill* f : songFills)
                fillPositions += (fillPositions.empty() ? "" : ",") + std::to_string (f->index % 8);
            for (const auto& g : gestures)
                if (g.kinds == 0)
                    missing += (missing.empty() ? "" : ",") + std::to_string (g.bar);
            Value f = Value::object();
            f.set ("boundaries", gestures.size());
            f.set ("with_gesture", gw);
            f.set ("fills", songFills.size());
            f.set ("distinct_fills", distinct);
            f.set ("fill_positions", fillPositions);
            f.set ("missing_gesture_bars", missing);
            perFile.set (s->path, std::move (f));
        }
        std::int64_t fillsTotal = 0;
        for (const auto& [p, c] : positions)
            fillsTotal += c;
        const auto top = topOf (positions);
        const std::int64_t topCount = top ? top->second : 0;
        const std::int64_t medianDistinct = lowerMedian (distinctShares).value_or (-1);
        Value res = Value::object();
        res.set ("boundaries", boundaries);
        res.set ("with_gesture", withGesture);
        res.set ("with_gesture_pm", permille (withGesture, boundaries));
        res.set ("fills", fillsTotal);
        res.set ("position_histogram", numberHistogram (positions));
        res.set ("top_position", top ? top->first : -1);
        res.set ("top_position_pm", permille (topCount, fillsTotal));
        res.set ("songs_with_2_fills", distinctShares.size());
        res.set ("median_distinct_pm", medianDistinct);
        res.set ("pass", boundaries > 0 && withGesture == boundaries && fillsTotal > 0 && 10 * topCount <= 6 * fillsTotal && medianDistinct >= 700);
        res.set ("per_file", std::move (perFile));
        return res;
    }

    // ---------------- G10 ----------------

    std::string formShape (const Song& song)
    {
        std::string out;
        for (const auto& s : song.sections)
            out += (out.empty() ? "" : " ") + s.kind + std::to_string (s.bars);
        return out;
    }

    Value g10 (Songs songs)
    {
        std::vector<std::optional<std::string>> forms;
        for (const Song* s : songs)
            forms.push_back (s->sections.empty() ? std::optional<std::string> {} : formShape (*s));
        const Value d = diversity (forms, songs.size());
        bool allTwo = true;
        Value perFile = Value::object();
        for (std::size_t i = 0; i < songs.size(); ++i)
        {
            const std::int64_t k = countDistinct (salientIntervals (*songs[i]));
            allTwo = allTwo && k >= 2;
            Value f = Value::object();
            f.set ("form", forms[i].value_or (""));
            f.set ("interval_values", k);
            perFile.set (songs[i]->path, std::move (f));
        }
        Value res = Value::object();
        res.set ("seeds", d.num ("seeds"));
        res.set ("measured", d.num ("measured"));
        res.set ("distinct_forms", d.num ("distinct"));
        res.set ("top_form_count", d.num ("top"));
        res.set ("top_form_pm", d.num ("top_pm"));
        res.set ("top_form", d["top_shape"].string());
        res.set ("every_song_two_interval_values", allTwo);
        res.set ("pass", d.num ("measured") == d.num ("seeds") && d.num ("distinct") >= 8 && 4 * d.num ("top") <= d.num ("measured") && allTwo);
        res.set ("per_file", std::move (perFile));
        return res;
    }
}
