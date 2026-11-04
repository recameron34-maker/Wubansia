// The reported metrics R1-R11 and the invariants I1-I5 that MIDI can show
// (tools/legacy_audit/README.md, "Reported metrics", "Invariants measured on legacy"), ported from
// tools/legacy_audit/metrics.js function by function, with the same keys in the same order.

#include "metrics/histogram.h"
#include "metrics/metrics.h"

#include "util/ints.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <map>
#include <set>
#include <utility>

namespace wubansia::metrics
{
    using detail::numberHistogram;
    using detail::stringHistogram;
    using util::floorDiv;
    using util::floorMod;

    namespace
    {
        // The lanes of R1 and R2: the kick group, and every bass, chords and lead track.
        struct ReportLane
        {
            std::size_t role = 0; // index into reportRoles
            std::vector<Note> notes;
        };
        constexpr std::array<std::string_view, 4> reportRoles = { "kick", "bass", "chords", "lead" };

        std::vector<ReportLane> reportLanes (const Song& song)
        {
            std::vector<ReportLane> lanes;
            auto kick = drumNotesIn (song, Group::kick);
            if (!kick.empty())
                lanes.push_back ({ 0, std::move (kick) });
            const std::array<Role, 3> roles = { Role::bass, Role::chords, Role::lead };
            for (std::size_t r = 0; r < roles.size(); ++r)
                for (const Track* t : tracksWithRole (song, roles[r]))
                    lanes.push_back ({ r + 1, t->notes });
            return lanes;
        }

        Value counts (std::initializer_list<std::pair<std::string_view, std::int64_t>> fields)
        {
            Value v = Value::object();
            for (const auto& [k, n] : fields)
                v.set (k, n);
            return v;
        }

        // The onset steps (0-15) of each bar that has notes, by bar, from step attribution.
        std::map<std::int64_t, std::vector<std::int32_t>> stepsByBar (std::span<const Note> notes)
        {
            std::map<std::int64_t, std::vector<std::int32_t>> out;
            for (const auto& n : notes)
            {
                const std::int64_t st = step16 (n.tick);
                out[floorDiv (st, std::int64_t { 16 })].push_back (static_cast<std::int32_t> (floorMod (st, std::int64_t { 16 })));
            }
            for (auto& [b, steps] : out)
            {
                std::ranges::sort (steps);
                steps.erase (std::unique (steps.begin(), steps.end()), steps.end());
            }
            return out;
        }

        constexpr std::array<int, 7> minorScale = { 0, 2, 3, 5, 7, 8, 10 };
        constexpr std::array<int, 7> majorScale = { 0, 2, 4, 5, 7, 9, 11 };
    }

    // ---------------- R1 ----------------

    Value rRepetition (Songs songs)
    {
        std::array<std::int64_t, 4> pairs {}, equal {};
        for (const Song* s : songs)
        {
            const auto sections = sectionsOf (*s);
            for (const auto& lane : reportLanes (*s))
                for (const auto& sec : sections)
                {
                    const auto prints = stepBars (lane.notes, sec.bar, sec.bars);
                    for (std::size_t i = 4; i < prints.size(); ++i)
                        if (!prints[i].empty() && !prints[i - 4].empty())
                        {
                            ++pairs[lane.role];
                            equal[lane.role] += prints[i] == prints[i - 4] ? 1 : 0;
                        }
                }
        }
        Value res = Value::object();
        for (std::size_t r = 0; r < reportRoles.size(); ++r)
            res.set (reportRoles[r], counts ({ { "pairs", pairs[r] }, { "equal", equal[r] }, { "equal_pm", permille (equal[r], pairs[r]) } }));
        return res;
    }

    // ---------------- R2 ----------------

    Value rLoopTiling (Songs songs)
    {
        std::array<std::vector<std::int64_t>, 4> runs;
        for (const Song* s : songs)
        {
            std::array<std::optional<std::int64_t>, 4> best;
            for (const auto& lane : reportLanes (*s))
            {
                const auto prints = stepBars (lane.notes, 0, s->bars);
                std::int64_t longest = 0, run = 0;
                bool havePrevious = false;
                std::size_t previous = 0; // the first bar of the previous non-empty block
                for (std::int64_t b = 0; b + 4 <= s->bars; b += 4)
                {
                    const auto at = static_cast<std::size_t> (b);
                    const bool empty = std::all_of (prints.begin() + static_cast<std::ptrdiff_t> (at), prints.begin() + static_cast<std::ptrdiff_t> (at + 4), [] (const StepPrint& p) { return p.empty(); });
                    const bool same = havePrevious && std::equal (prints.begin() + static_cast<std::ptrdiff_t> (at), prints.begin() + static_cast<std::ptrdiff_t> (at + 4), prints.begin() + static_cast<std::ptrdiff_t> (previous));
                    if (!empty && same)
                        run += 4;
                    else
                        run = empty ? 0 : 4;
                    havePrevious = !empty;
                    previous = at;
                    longest = std::max (longest, run);
                }
                best[lane.role] = std::max (best[lane.role].value_or (0), longest);
            }
            for (std::size_t r = 0; r < 4; ++r)
                if (best[r])
                    runs[r].push_back (*best[r]);
        }
        Value res = Value::object();
        for (std::size_t r = 0; r < reportRoles.size(); ++r)
        {
            const std::int64_t most = runs[r].empty() ? -1 : *std::max_element (runs[r].begin(), runs[r].end());
            res.set (reportRoles[r], counts ({ { "files", static_cast<std::int64_t> (runs[r].size()) }, { "median_bars", lowerMedian (runs[r]).value_or (-1) }, { "max_bars", most } }));
        }
        return res;
    }

    // ---------------- R3 ----------------

    std::int64_t accentPeriod (std::span<const std::int32_t, 16> v) noexcept
    {
        for (const std::size_t p : { 1u, 2u, 4u, 8u })
        {
            bool ok = true;
            for (std::size_t s = p; s < 16 && ok; ++s)
                ok = v[s] == v[s - p];
            if (ok)
                return static_cast<std::int64_t> (p);
        }
        return 16;
    }

    Value rHatAccent (Songs songs)
    {
        std::int64_t bars = 0, shortPeriod = 0;
        std::map<std::int64_t, std::int64_t> periods;
        for (const Song* s : songs)
        {
            // closed hats only (42, 44): per bar the highest velocity on each step, 0 for a rest
            std::map<std::int64_t, std::array<std::int32_t, 16>> byBar;
            for (const auto& n : s->drumNotes)
            {
                if (n.key != 42 && n.key != 44)
                    continue;
                const std::int64_t st = step16 (n.tick);
                auto [it, fresh] = byBar.try_emplace (floorDiv (st, std::int64_t { 16 }));
                if (fresh)
                    it->second.fill (0);
                auto& cell = it->second[static_cast<std::size_t> (floorMod (st, std::int64_t { 16 }))];
                cell = std::max<std::int32_t> (cell, n.velocity);
            }
            for (const auto& [b, v] : byBar)
            {
                if (std::count_if (v.begin(), v.end(), [] (std::int32_t x) { return x > 0; }) < 4)
                    continue;
                const std::int64_t p = accentPeriod (v);
                ++bars;
                shortPeriod += p <= 4 ? 1 : 0;
                ++periods[p];
            }
        }
        Value res = Value::object();
        res.set ("bars", bars);
        res.set ("period_4_or_less", shortPeriod);
        res.set ("period_4_or_less_pm", permille (shortPeriod, bars));
        res.set ("period_histogram", numberHistogram (periods));
        return res;
    }

    // ---------------- R4 ----------------

    Value rGateRule (Songs songs)
    {
        std::int64_t notes = 0, rule = 0;
        for (const Song* s : songs)
        {
            const std::int64_t fileDelay = fileSwing (*s);
            for (const Role role : { Role::bass, Role::chords, Role::lead })
                for (const Track* t : tracksWithRole (*s, role))
                {
                    const std::int64_t d = swingDelay (t->notes, 16).value_or (fileDelay);
                    for (const auto& n : t->notes)
                    {
                        const std::int64_t e = n.tick + n.length;
                        ++notes;
                        if (e == s->endTick || onSwungGrid (e + 14, d, 0) || onSwungGrid (e - 30, d, 0))
                            ++rule;
                    }
                }
        }
        return counts ({ { "notes", notes }, { "gate_rule", rule }, { "gate_rule_pm", permille (rule, notes) } });
    }

    // ---------------- R5 ----------------

    Value rChordTones (Songs songs)
    {
        // per role: strong n, strong in chord, weak n, weak in chord
        std::array<std::array<std::int64_t, 4>, 2> t {};
        const std::array<Role, 2> roles = { Role::lead, Role::bass };
        for (const Song* s : songs)
        {
            const auto stacks = harmonyStacks (*s);
            if (stacks.empty())
                continue;
            for (std::size_t r = 0; r < roles.size(); ++r)
                for (const auto& n : notesWithRole (*s, roles[r]))
                {
                    const HarmonyStack* h = harmonyAt (stacks, n.tick);
                    const bool strong = floorMod (step16 (n.tick), std::int64_t { 4 }) == 0;
                    const std::int64_t inChord = (h->mask >> (n.key % 12)) & 1;
                    t[r][strong ? 0 : 2] += 1;
                    t[r][strong ? 1 : 3] += inChord;
                }
        }
        Value res = Value::object();
        for (std::size_t r = 0; r < roles.size(); ++r)
        {
            const auto& a = t[r];
            res.set (roleName (roles[r]), counts ({ { "strong", a[0] }, { "strong_chord_tones", a[1] }, { "strong_pm", permille (a[1], a[0]) }, { "weak", a[2] }, { "weak_chord_tones", a[3] }, { "weak_pm", permille (a[3], a[2]) } }));
        }
        return res;
    }

    // ---------------- R6 ----------------

    Value rBreath (Songs songs)
    {
        std::vector<std::int64_t> rests;
        std::int64_t units = 0, oneOnly = 0;
        for (const Song* s : songs)
        {
            const auto lead = notesWithRole (*s, Role::lead);
            const auto vox = notesWithRole (*s, Role::vox);
            for (const auto& sec : sectionsOf (*s))
                for (std::int64_t w = 0; w + 4 <= sec.bars; w += 4)
                {
                    const std::int64_t t0 = (sec.bar + w) * barTicks, t1 = t0 + 4 * barTicks;
                    if (between (lead, t0, t1).empty())
                        continue;
                    std::vector<const Note*> sounding; // the lead notes that overlap the window
                    for (const auto& n : lead)
                        if (n.tick < t1 && n.tick + n.length > t0)
                            sounding.push_back (&n);
                    std::int64_t covered = 0;
                    for (std::int64_t k = 0; k < 64; ++k)
                    {
                        const std::int64_t at = t0 + k * stepTicks;
                        if (std::any_of (sounding.begin(), sounding.end(), [at] (const Note* n) { return n->tick <= at && at < n->tick + n->length; }))
                            ++covered;
                    }
                    rests.push_back (permille (64 - covered, 64));
                }
            if (vox.empty() || lead.empty())
                continue;
            for (std::int64_t h = 0; h < 2 * s->bars; ++h)
            {
                const bool a = !between (lead, h * 960, h * 960 + 960).empty();
                const bool b = !between (vox, h * 960, h * 960 + 960).empty();
                if (a || b)
                {
                    ++units;
                    oneOnly += a != b ? 1 : 0;
                }
            }
        }
        return counts ({ { "lead_windows", static_cast<std::int64_t> (rests.size()) }, { "median_rest_pm", lowerMedian (rests).value_or (-1) }, { "half_bars_with_lead_or_vox", units }, { "only_one_pm", permille (oneOnly, units) } });
    }

    // ---------------- R7 ----------------

    Value rLeadShape (Songs songs)
    {
        std::int64_t intervals = 0, steps = 0, repeats = 0, windows = 0, same13 = 0;
        for (const Song* s : songs)
        {
            const auto sections = sectionsOf (*s);
            for (const Track* t : tracksWithRole (*s, Role::lead))
            {
                std::vector<int> tops; // the top note of each stack
                for (const auto& st : stacksOf (t->notes))
                {
                    int top = 0;
                    for (const auto& n : st.notes)
                        top = std::max<int> (top, n.key);
                    tops.push_back (top);
                }
                for (std::size_t i = 1; i < tops.size(); ++i)
                {
                    const int d = std::abs (tops[i] - tops[i - 1]);
                    ++intervals;
                    steps += d == 1 || d == 2 ? 1 : 0;
                    repeats += d == 0 ? 1 : 0;
                }
                for (const auto& sec : sections)
                    for (std::int64_t w = 0; w + 4 <= sec.bars; w += 4)
                    {
                        const auto prints = stepBars (t->notes, sec.bar + w, 4);
                        // each bar's onset steps (ascending, since a print sorts by step * 128 + key)
                        std::array<std::vector<std::int32_t>, 4> onsets;
                        for (std::size_t b = 0; b < 4; ++b)
                            for (const auto x : prints[b])
                                if (onsets[b].empty() || onsets[b].back() != x / 128)
                                    onsets[b].push_back (x / 128);
                        if (onsets[0].empty() || onsets[2].empty())
                            continue;
                        ++windows;
                        same13 += onsets[0] == onsets[2] ? 1 : 0;
                    }
            }
        }
        Value res = Value::object();
        res.set ("intervals", intervals);
        res.set ("steps", steps);
        res.set ("step_pm", permille (steps, intervals));
        res.set ("repeats_pm", permille (repeats, intervals));
        res.set ("windows", windows);
        res.set ("bar1_eq_bar3", same13);
        res.set ("bar1_eq_bar3_pm", permille (same13, windows));
        return res;
    }

    // ---------------- R8 ----------------

    std::string voicingType (std::vector<int> keys)
    {
        std::set<int> classes;
        for (const int k : keys)
            classes.insert (k % 12);
        std::ranges::sort (keys);
        keys.erase (std::unique (keys.begin(), keys.end()), keys.end());
        bool fourths = keys.size() >= 3;
        for (std::size_t i = 1; i < keys.size() && fourths; ++i)
            fourths = keys[i] - keys[i - 1] == 5;
        if (fourths)
            return "quartal";
        if (classes.size() <= 1)
            return "octave";
        if (classes.size() == 2)
            return "dyad";
        if (classes.size() == 3)
            return "triad";
        return "four_plus";
    }

    Value rVoicing (Songs songs)
    {
        std::vector<std::int64_t> typesPerSong;
        std::map<std::string, std::int64_t> types;
        std::int64_t stacks = 0, fourPlus = 0, recurring = 0, revoiced = 0;
        for (const Song* s : songs)
        {
            std::set<std::string> seen;
            std::map<int, std::vector<int>> last; // pitch-class mask -> the sorted keys of its latest stack
            for (const Track* t : tracksWithRole (*s, Role::chords))
                for (const auto& st : stacksOf (t->notes))
                {
                    if (st.notes.size() < 2)
                        continue;
                    std::vector<int> keys;
                    for (const auto& n : st.notes)
                        keys.push_back (n.key);
                    std::ranges::sort (keys);
                    const std::string type = voicingType (keys);
                    const int mask = pitchClassMask (st.notes);
                    ++stacks;
                    fourPlus += type == "four_plus" ? 1 : 0;
                    seen.insert (type);
                    ++types[type];
                    if (const auto it = last.find (mask); it != last.end())
                    {
                        ++recurring;
                        revoiced += it->second != keys ? 1 : 0;
                    }
                    last[mask] = keys;
                }
            if (!seen.empty())
                typesPerSong.push_back (static_cast<std::int64_t> (seen.size()));
        }
        Value res = Value::object();
        res.set ("stacks", stacks);
        res.set ("type_histogram", stringHistogram (types, "type"));
        res.set ("median_types_per_song", lowerMedian (typesPerSong).value_or (-1));
        res.set ("four_plus_pm", permille (fourPlus, stacks));
        res.set ("recurring", recurring);
        res.set ("revoiced", revoiced);
        res.set ("revoiced_pm", permille (revoiced, recurring));
        return res;
    }

    // ---------------- R9 ----------------

    Value rOddCycles (Songs songs)
    {
        std::int64_t windows = 0, odd = 0;
        for (const Song* s : songs)
        {
            const auto sections = sectionsOf (*s);
            for (const Track* t : tracksWithRole (*s, Role::drums))
            {
                std::map<int, std::vector<std::int64_t>> stepsByKey; // keys in numeric order
                for (const auto& n : t->notes)
                    stepsByKey[n.key].push_back (step16 (n.tick));
                for (const auto& [key, steps] : stepsByKey)
                    for (const auto& sec : sections)
                        for (std::int64_t w = 0; w + 8 <= sec.bars; w += 8)
                        {
                            const std::int64_t b0 = sec.bar + w;
                            std::array<bool, 128> seq {};
                            bool any = false;
                            for (const std::int64_t st : steps)
                            {
                                const std::int64_t i = st - 16 * b0;
                                if (i >= 0 && i < 128)
                                {
                                    seq[static_cast<std::size_t> (i)] = true;
                                    any = true;
                                }
                            }
                            if (!any)
                                continue;
                            ++windows;
                            std::size_t period = 0;
                            for (std::size_t p = 1; p <= 64 && period == 0; ++p)
                            {
                                bool ok = true;
                                for (std::size_t i = p; i < 128 && ok; ++i)
                                    ok = seq[i] == seq[i - p];
                                if (ok)
                                    period = p;
                            }
                            if (period != 0 && 16 % period != 0 && period % 16 != 0)
                                ++odd;
                        }
            }
        }
        return counts ({ { "lane_windows", windows }, { "odd_cycles", odd }, { "odd_cycles_pm", permille (odd, windows) } });
    }

    // ---------------- R10 ----------------

    Value rDuplicates (Songs songs, Role role)
    {
        using Motif = std::vector<std::pair<std::int64_t, std::int64_t>>; // (step from the window start, key - first key)
        std::map<Motif, std::set<std::string>> owners; // a motif -> the files it occurs in
        std::vector<Motif> motifs; // every window, in order
        for (const Song* s : songs)
        {
            const auto notes = notesWithRole (*s, role);
            for (const auto& sec : sectionsOf (*s))
                for (std::int64_t w = 0; w + 2 <= sec.bars; w += 2)
                {
                    const std::int64_t b0 = sec.bar + w;
                    const std::int64_t s0 = floorDiv (b0 * barTicks, stepTicks);
                    Motif m;
                    int first = -1;
                    for (const auto& n : notes)
                    {
                        const std::int64_t st = step16 (n.tick);
                        const std::int64_t b = floorDiv (st, std::int64_t { 16 });
                        if (b < b0 || b >= b0 + 2)
                            continue;
                        if (first < 0)
                            first = n.key; // the window's first note, by onset, then track, then key
                        m.emplace_back (st - s0, n.key - first);
                    }
                    if (first < 0)
                        continue;
                    std::ranges::sort (m);
                    m.erase (std::unique (m.begin(), m.end()), m.end());
                    owners[m].insert (s->path);
                    motifs.push_back (std::move (m));
                }
        }
        std::int64_t duplicated = 0;
        for (const auto& m : motifs)
            duplicated += owners[m].size() > 1 ? 1 : 0;
        const auto windows = static_cast<std::int64_t> (motifs.size());
        return counts ({ { "windows", windows }, { "duplicated", duplicated }, { "duplicated_pm", permille (duplicated, windows) } });
    }

    // ---------------- R11 ----------------

    Value rSyncopation (Songs songs)
    {
        constexpr std::array<std::string_view, 6> names = { "kick", "snare", "hats", "bass", "chords", "lead" };
        std::array<std::vector<std::int64_t>, 6> values;
        for (const Song* s : songs)
        {
            const std::array<std::vector<Note>, 6> lanes = { drumNotesIn (*s, Group::kick), drumNotesIn (*s, snareFamily), drumNotesIn (*s, hatsGroups), notesWithRole (*s, Role::bass), notesWithRole (*s, Role::chords), notesWithRole (*s, Role::lead) };
            for (std::size_t l = 0; l < lanes.size(); ++l)
                for (const auto& [bar, steps] : stepsByBar (lanes[l]))
                    values[l].push_back (lhl (steps));
        }
        Value res = Value::object();
        for (std::size_t l = 0; l < names.size(); ++l)
        {
            const auto& v = values[l];
            std::int64_t sum = 0;
            for (const auto x : v)
                sum += x;
            const auto n = static_cast<std::int64_t> (v.size());
            Value e = Value::object();
            e.set ("bars", n);
            e.set ("mean_milli", n > 0 ? floorDiv (2000 * sum + n, 2 * n) : std::int64_t { -1 });
            e.set ("sd_milli", sdMilli (v).value_or (-1));
            e.set ("distinct", countDistinct (v));
            res.set (names[l], std::move (e));
        }
        return res;
    }

    // ---------------- invariants ----------------

    Value iInKey (Songs songs)
    {
        std::map<std::string, std::pair<std::int64_t, std::int64_t>> byRole; // role name in byte order -> notes, in key
        for (const Song* s : songs)
        {
            if (s->keySignatures.empty())
                continue;
            for (const auto& t : s->tracks)
            {
                if (t.role == Role::drums || t.role == Role::ignore)
                    continue;
                auto& a = byRole[std::string (roleName (t.role))];
                for (const auto& n : t.notes)
                {
                    const midi::KeySignature* ks = &s->keySignatures[0];
                    for (const auto& k : s->keySignatures)
                        if (k.tick <= n.tick)
                            ks = &k;
                    const int tonic = floorMod ((ks->minor ? 9 : 0) + 7 * ks->sharps, 12);
                    const auto& scale = ks->minor ? minorScale : majorScale;
                    ++a.first;
                    if (std::find (scale.begin(), scale.end(), floorMod (n.key - tonic, 12)) != scale.end())
                        ++a.second;
                }
            }
        }
        Value res = Value::object();
        for (const auto& [role, a] : byRole)
            res.set (role, counts ({ { "notes", a.first }, { "in_key", a.second }, { "in_key_pm", permille (a.second, a.first) } }));
        return res;
    }

    Value iSwungGrid (Songs songs)
    {
        std::array<std::int64_t, 3> riser {}, roll {}; // notes, on the swung grid, straight only
        const auto classify = [] (std::array<std::int64_t, 3>& a, std::int64_t tick, std::int64_t d) {
            ++a[0];
            if (onSwungGrid (tick, d, 2))
                ++a[1];
            else if (onStraightGrid (tick, 2))
                ++a[2];
        };
        for (const Song* s : songs)
        {
            const std::int64_t d = fileSwing (*s);
            for (const auto& n : notesWithRole (*s, Role::fx))
                classify (riser, n.tick, d);
            const auto snares = drumNotesIn (*s, snareFamily);
            for (const auto& sec : s->sections)
                if (sec.kind == "build")
                    for (const auto& n : between (snares, sec.tick, sec.end))
                        classify (roll, n.tick, d);
        }
        const auto out = [] (const std::array<std::int64_t, 3>& a) {
            return counts ({ { "notes", a[0] }, { "swung", a[1] }, { "straight_only", a[2] }, { "straight_only_pm", permille (a[2], a[0]) } });
        };
        Value res = Value::object();
        res.set ("riser", out (riser));
        res.set ("build_snare", out (roll));
        return res;
    }

    Value iKickBass (Songs songs)
    {
        std::int64_t pairs = 0, tight = 0;
        for (const Song* s : songs)
        {
            const auto kick = drumNotesIn (*s, Group::kick);
            for (const auto& n : notesWithRole (*s, Role::bass))
            {
                const auto near = between (kick, n.tick - 30, n.tick + 31);
                if (near.empty())
                    continue;
                std::int64_t d = std::llabs (near[0].tick - n.tick);
                for (const auto& k : near)
                    d = std::min<std::int64_t> (d, std::llabs (k.tick - n.tick));
                ++pairs;
                tight += d <= 2 ? 1 : 0;
            }
        }
        return counts ({ { "pairs", pairs }, { "within_2_ticks", tight }, { "within_2_ticks_pm", permille (tight, pairs) } });
    }

    Value iSections (Songs songs)
    {
        std::int64_t n = 0, onLine = 0;
        for (const Song* s : songs)
            for (const auto& sec : s->sections)
            {
                ++n;
                onLine += floorMod (sec.tick, 8 * barTicks) == 0 ? 1 : 0;
            }
        return counts ({ { "sections", n }, { "on_8_bar_lines", onLine }, { "on_8_bar_lines_pm", permille (onLine, n) } });
    }

    Value iAnchors (Songs songs)
    {
        std::int64_t n = 0, tight = 0;
        for (const Song* s : songs)
            for (const auto& k : s->drumNotes)
            {
                if (k.group != Group::kick)
                    continue;
                const std::int64_t r = floorMod (k.tick, beatTicks);
                const std::int64_t d = std::min (r, beatTicks - r);
                if (d > 30)
                    continue;
                ++n;
                tight += d <= 2 ? 1 : 0;
            }
        return counts ({ { "kicks_near_beats", n }, { "within_2_ticks", tight }, { "within_2_ticks_pm", permille (tight, n) } });
    }
}
