// The hook's material (hook_material.h), moved from hook.cpp without a change.

#include "gen/hook_material.h"

#include "util/ints.h"

#include <algorithm>
#include <array>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <optional>
#include <span>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace wubansia::gen::material
{
    namespace
    {
        constexpr std::int32_t attempts = 32; // per walk, then the attempt that breaks the fewest rules
        constexpr std::int32_t redraws = 8; // of a material that still breaks a rule
    }

    std::int32_t semitones (Mode mode, std::int32_t degree)
    {
        const auto steps = modeSteps (mode);
        return 12 * util::floorDiv (degree, 7) + steps[static_cast<std::size_t> (util::floorMod (degree, 7))];
    }

    // A degree 0-6 by weight; the tonic when none can be drawn.
    std::int32_t pickDegree (rng::Key k, const std::array<std::int32_t, 7>& w)
    {
        const std::size_t i = rng::pickWeighted (k, std::span<const std::int32_t> (w.data(), w.size()));
        return i < 7 ? static_cast<std::int32_t> (i) : 0;
    }

    // The pool's next degree from d in direction dir (+1 or -1), within an octave; d + dir if none.
    std::int32_t nextPool (const HookPitch& p, std::int32_t d, std::int32_t dir)
    {
        for (std::int32_t x = d + dir; std::abs (x - d) <= 7; x += dir)
            if (inPool (p, x))
                return x;
        return d + dir;
    }

    void bySteps (Notes& notes)
    {
        std::stable_sort (notes.begin(), notes.end(), [] (const HookNote& a, const HookNote& b) { return a.step < b.step; });
    }

    // Lengths never run into the next onset nor past the bar line.
    void fitLengths (Notes& bar)
    {
        for (std::size_t i = 0; i < bar.size(); ++i)
        {
            const std::int32_t next = i + 1 < bar.size() ? bar[i + 1].step : 16;
            bar[i].len16 = static_cast<std::uint8_t> (std::max (1, std::min (static_cast<std::int32_t> (bar[i].len16), next - bar[i].step)));
        }
    }

    bool occupied (const Notes& bar, std::int32_t step)
    {
        return std::any_of (bar.begin(), bar.end(), [step] (const HookNote& n) { return n.step == step; });
    }

    // ------------------------------------------------------------------ rhythm first

    // Bar 1 from a cell: per seed at most one non-accent onset out (never the first, its half's only
    // onset or the bar's only non-accent one, and never from a cell of three onsets) and at most one
    // extra onset in, on a 16th of "extra" (never 16th 15 in a pedal style).
    Notes barFromCell (const HookCell& c, const HookMutate& m, bool pedal, rng::Key k)
    {
        Notes bar;
        for (std::size_t i = 0; i < c.on.size(); ++i)
        {
            HookNote n;
            n.step = c.on[i];
            n.len16 = static_cast<std::uint8_t> (c.len.empty() ? 2 : c.len[std::min (i, c.len.size() - 1)]);
            n.accent = std::find (c.acc.begin(), c.acc.end(), c.on[i]) != c.acc.end();
            bar.push_back (n);
        }
        if (rng::chance (k.with (rng::tag ("drop")), p16FromPercent (m.drop)))
        {
            const auto nonAccents = std::count_if (bar.begin(), bar.end(), [] (const HookNote& n) { return !n.accent; });
            std::vector<std::size_t> cand;
            for (std::size_t i = 1; i < bar.size(); ++i)
            {
                if (bar[i].accent)
                    continue;
                const bool firstHalf = bar[i].step < 8;
                const auto sameHalf = std::count_if (bar.begin(), bar.end(), [firstHalf] (const HookNote& n) { return (n.step < 8) == firstHalf; });
                if (sameHalf > 1 && nonAccents > 1)
                    cand.push_back (i);
            }
            if (!cand.empty() && bar.size() > 3)
                bar.erase (bar.begin() + static_cast<std::ptrdiff_t> (cand[rng::pickIndex (k.with (rng::tag ("which")), cand.size())]));
        }
        if (rng::chance (k.with (rng::tag ("add")), p16FromPercent (m.add)))
        {
            std::vector<std::int32_t> cand;
            for (const std::int32_t s : m.extra)
                if (!occupied (bar, s) && !(pedal && s == 15))
                    cand.push_back (s);
            if (!cand.empty())
            {
                HookNote n;
                n.step = cand[rng::pickIndex (k.with (rng::tag ("at")), cand.size())];
                n.len16 = 1;
                bar.push_back (n);
                bySteps (bar);
            }
        }
        fitLengths (bar);
        return bar;
    }

    // Bar 2 answers bar 1: its 16ths 0-7 exactly, and at most one edit among 8-15 by the style's
    // weights: the same, an onset moved by a 16th, one added (not an accent), one dropped (never an
    // accent, the half's only onset or the bar's only non-accent one).
    Notes answerBar (const Notes& bar1, const HookAnswer& a, bool pedal, rng::Key k)
    {
        const std::array<std::int32_t, 4> w { a.same, a.move, a.add, a.drop };
        const std::size_t edit = rng::pickWeighted (k.with (rng::tag ("edit")), std::span<const std::int32_t> (w.data(), w.size()));
        Notes bar2 = bar1;
        std::vector<std::size_t> second;
        for (std::size_t i = 0; i < bar2.size(); ++i)
            if (bar2[i].step >= 8)
                second.push_back (i);
        if (edit == 1)
        {
            std::vector<std::pair<std::size_t, std::int32_t>> cand;
            for (const std::size_t i : second)
                for (const std::int32_t d : { -1, 1 })
                {
                    const std::int32_t s = bar2[i].step + d;
                    if (s >= 8 && s <= 15 && !occupied (bar2, s) && !(pedal && s == 15))
                        cand.emplace_back (i, s);
                }
            if (!cand.empty())
            {
                const auto& [i, s] = cand[rng::pickIndex (k.with (rng::tag ("at")), cand.size())];
                bar2[i].step = s;
                bar2[i].len16 = 2;
            }
        }
        else if (edit == 2)
        {
            std::vector<std::int32_t> cand;
            for (std::int32_t s = 8; s <= 15; ++s)
                if (!occupied (bar2, s) && !(pedal && s == 15))
                    cand.push_back (s);
            if (!cand.empty())
            {
                HookNote n;
                n.step = cand[rng::pickIndex (k.with (rng::tag ("at")), cand.size())];
                n.len16 = 1;
                bar2.push_back (n);
            }
        }
        else if (edit == 3 && second.size() >= 2)
        {
            const auto nonAccents = std::count_if (bar2.begin(), bar2.end(), [] (const HookNote& n) { return !n.accent; });
            std::vector<std::size_t> cand;
            for (const std::size_t i : second)
                if (!bar2[i].accent && nonAccents > 1)
                    cand.push_back (i);
            if (!cand.empty())
                bar2.erase (bar2.begin() + static_cast<std::ptrdiff_t> (cand[rng::pickIndex (k.with (rng::tag ("at")), cand.size())]));
        }
        bySteps (bar2);
        fitLengths (bar2);
        return bar2;
    }

    // A 2-bar riff's rhythm: bar 1 a cell by weight, mutated; bar 2 answering it.
    Notes riffRhythm (const std::vector<HookCell>& cells, const HookSpec& spec, bool pedal, rng::Key k)
    {
        std::vector<std::int32_t> w;
        for (const HookCell& c : cells)
            w.push_back (c.weight);
        const std::size_t ci = rng::pickWeighted (k.with (rng::tag ("cell")), w);
        if (ci >= cells.size())
            return {};
        Notes out = barFromCell (cells[ci], spec.mutate, pedal, k.with (rng::tag ("mutate")));
        for (HookNote n : answerBar (out, spec.answer, pedal, k.with (rng::tag ("bar2"))))
        {
            n.step += 16;
            out.push_back (n);
        }
        return out;
    }

    // Drop 2's period on the hook's own rhythm (the organ and the glass): bar a is A's first half plus
    // a second half of the period's own, its last onset an accent; bar b answers a (the glass: a
    // again); a' has a's rhythm; c keeps a's first half, then the organ's falls on 16ths 8, 10 and 12
    // (held to the bar line) and the glass's keeps a's rhythm.
    Notes periodRhythm (const Notes& aBar1, const HookSpec& spec, bool pedal, rng::Key k)
    {
        const HookPeriod& per = spec.period;
        Notes a;
        for (const HookNote& x : aBar1)
            if (x.step < 8)
                a.push_back (x);
        std::vector<std::int32_t> w;
        for (const auto& half : per.halves)
            w.push_back (half.second);
        const std::size_t hi = rng::pickWeighted (k.with (rng::tag ("half")), w);
        if (hi < per.halves.size())
            for (const std::int32_t s : per.halves[hi].first)
            {
                HookNote n;
                n.step = s;
                n.len16 = 2;
                a.push_back (n);
            }
        if (!a.empty())
            a.back().accent = true;
        fitLengths (a);
        const bool glass = per.idiom == HookIdiom::glass;
        const Notes b = glass ? a : answerBar (a, spec.answer, pedal, k.with (rng::tag ("bar2")));
        Notes c;
        if (glass)
            c = a;
        else
        {
            for (const HookNote& x : a)
                if (x.step < 8)
                    c.push_back (x);
            for (const std::int32_t s : { 8, 10, 12 })
            {
                HookNote n;
                n.step = s;
                n.len16 = static_cast<std::uint8_t> (s == 12 ? 4 : 2);
                n.accent = s == 12;
                c.push_back (n);
            }
            fitLengths (c);
        }
        Notes out;
        for (std::int32_t bar = 0; bar < 4; ++bar)
            for (HookNote n : (bar == 1 ? b : bar == 3 ? c
                                                       : a))
            {
                n.step += bar * 16;
                out.push_back (n);
            }
        return out;
    }

    // ------------------------------------------------------------------ the fit table

    // The degree walk: up to 32 attempts, each a peak drawn among the onsets in the contour's window,
    // then a move per onset by weight: the style's weights by size in scale steps, towards the peak
    // before it and the end after it (a leap of a 4th or more turns back), a strong onset on a chord
    // tone of every chord it sounds over (the fit table), only pool degrees inside the register and
    // within the leap cap; the peak raised over every other note (alone, or struck twice, the notes
    // after a twin top walked again from there); the end degree in the octave nearest the note before
    // it (and the first, where they meet), else the next end by weight. The first attempt that breaks
    // no rule, else the one that breaks the fewest. Sets the notes' degrees and peak flags.
    void walk (const Setting& s, Notes& ns, rng::Key k, std::int32_t startDeg, std::int32_t endDeg, Range window, const WalkOpts& wo)
    {
        const HookPitch& p = s.spec->pitch;
        const auto n = static_cast<std::int32_t> (ns.size());
        if (n == 0)
            return;
        const auto& moves = wo.moves != nullptr ? *wo.moves : p.moves;
        const bool twin = wo.twin && n >= 5;
        const auto note = [&ns] (std::int32_t i) -> HookNote& { return ns[static_cast<std::size_t> (i)]; };
        const auto inRange = [&] (std::int32_t d) {
            const std::int32_t key = wo.base + semitones (s.mode, d);
            return key >= wo.reg.lo && key <= wo.reg.hi;
        };
        const auto apart = [&] (std::int32_t x, std::int32_t y) { return std::abs (semitones (s.mode, x) - semitones (s.mode, y)); };
        const auto strongAt = [&] (std::int32_t i) { return p.beats ? note (i).step % 4 == 0 : note (i).step % 4 == 0 || note (i).accent; };
        if (!inRange (startDeg))
            for (const std::int32_t o : { -7, 7, -14, 14 })
                if (inRange (startDeg + o))
                {
                    startDeg += o;
                    break;
                }
        const std::int32_t lastPeak = twin ? n - 3 : n - 2;
        bool lastMeetsFirst = wo.wrap;
        for (const auto& [ja, jb] : wo.joins)
            lastMeetsFirst = lastMeetsFirst || (static_cast<std::int32_t> (ja) == n - 1 && jb == 0);
        std::vector<std::int32_t> peaks;
        for (std::int32_t i = 1; i <= lastPeak; ++i)
            if (note (i).step >= window.lo && note (i).step <= window.hi)
                peaks.push_back (i);
        if (peaks.empty())
            peaks.push_back (std::max (1, lastPeak));

        std::vector<std::int32_t> best;
        std::int32_t bestBad = INT32_MAX, bestPeak = 0;
        for (std::int32_t attempt = 0; attempt < attempts; ++attempt)
        {
            const rng::Key ka = k.with (rng::tag ("attempt")).with (attempt);
            const std::int32_t pk = std::min (peaks[rng::pickIndex (ka.with (rng::tag ("peak")), peaks.size())], n - 1);
            std::vector<std::int32_t> d (ns.size(), 0);
            const auto D = [&d] (std::int32_t i) -> std::int32_t& { return d[static_cast<std::size_t> (i)]; };
            D (0) = startDeg;
            if (!wo.fixedStart && strongAt (0) && s.fit (note (0).step, startDeg) < 100)
                for (const std::int32_t dd : { -1, 1, -2, 2 })
                    if (inPool (p, startDeg + dd) && inRange (startDeg + dd) && s.fit (note (0).step, startDeg + dd) == 100)
                    {
                        D (0) = startDeg + dd;
                        break;
                    }
            std::int32_t lastMove = 0;
            const auto stepTo = [&] (std::int32_t i, rng::Key kk) {
                const std::int32_t from = D (i - 1);
                const std::int32_t target = i <= pk ? D (0) + 5 : endDeg;
                const bool strong = strongAt (i);
                std::vector<std::int32_t> cand, w;
                for (std::int32_t x = from - 7; x <= from + 7; ++x)
                {
                    if (!inPool (p, x) || !inRange (x) || apart (x, from) > p.leap)
                        continue;
                    const std::int32_t size = std::abs (x - from);
                    std::int32_t wt = moves[static_cast<std::size_t> (size)];
                    const std::int32_t dir = x > from ? 1 : x < from ? -1
                                                                     : 0;
                    const std::int32_t want = target > from ? 1 : target < from ? -1
                                                                                : 0;
                    if (std::abs (lastMove) >= 3)
                        wt *= dir == (lastMove > 0 ? -1 : 1) ? 4 : (dir == 0 ? 1 : 0); // a leap turns back
                    else
                        wt *= dir == want ? 3 : 1;
                    const std::int32_t f = s.fit (note (i).step, x);
                    if (strong)
                        wt *= f == 100 ? 10 : f >= 50 ? 2
                                                      : 0;
                    else
                        wt *= f >= 50 ? 3 : size == 1 ? 2
                                                      : 1;
                    if (wt > 0)
                    {
                        cand.push_back (x);
                        w.push_back (wt);
                    }
                }
                std::int32_t x = from;
                if (!cand.empty())
                {
                    const std::size_t pick = rng::pickWeighted (kk, w);
                    x = cand[pick < cand.size() ? pick : 0];
                }
                D (i) = x;
                lastMove = x - from;
            };
            for (std::int32_t i = 1; i < n; ++i)
                stepTo (i, ka.with (i));
            // the peak: the highest note, alone or struck twice
            std::int32_t maxOther = INT32_MIN / 2;
            for (std::int32_t i = 0; i < n; ++i)
                if (i != pk && !(twin && i == pk + 1))
                    maxOther = std::max (maxOther, D (i));
            if (pk >= 1 && D (pk) <= maxOther)
            {
                std::int32_t x = maxOther + 1;
                for (std::int32_t g = 0; g < 4 && (!inPool (p, x) || (strongAt (pk) && s.fit (note (pk).step, x) < 100)); ++g)
                    ++x;
                if (inRange (x) && apart (x, D (pk - 1)) <= p.leap)
                    D (pk) = x; // else the tie stays, and the rules count it
            }
            if (twin && pk + 1 < n)
            {
                D (pk + 1) = D (pk);
                lastMove = 0;
                for (std::int32_t i = pk + 2; i < n; ++i)
                    stepTo (i, ka.with (rng::tag ("twin")).with (i));
            }
            // the end: the drawn degree in the octave nearest the note before (and the first, where
            // they meet), else the next end by weight
            if (n >= 2)
            {
                const std::int32_t prev = D (n - 2);
                std::int32_t e = endDeg, bestCost = INT32_MAX;
                std::vector<std::int32_t> ends { endDeg };
                ends.insert (ends.end(), wo.endAlts.begin(), wo.endAlts.end());
                for (std::size_t r = 0; r < ends.size(); ++r)
                    for (std::int32_t o = -14; o <= 14; o += 7)
                    {
                        const std::int32_t c = degreeClass (ends[r]) + util::floorDiv (endDeg, 7) * 7 + o;
                        std::int32_t cost = apart (c, prev);
                        if (lastMeetsFirst)
                            cost = std::max (cost, apart (c, D (0)));
                        cost += inRange (c) ? 0 : 1000;
                        cost += cost > p.leap ? 500 : 0;
                        cost += static_cast<std::int32_t> (r) * 50;
                        if (cost < bestCost)
                        {
                            bestCost = cost;
                            e = c;
                        }
                    }
                D (n - 1) = e;
            }
            // the rules
            std::int32_t bad = 0, low = INT32_MAX, high = INT32_MIN;
            for (std::int32_t i = 0; i < n; ++i)
            {
                low = std::min (low, semitones (s.mode, D (i)));
                high = std::max (high, semitones (s.mode, D (i)));
                if (strongAt (i) && !(wo.fixedStart && i == 0) && s.fit (note (i).step, D (i)) < 100)
                    ++bad;
                if (i != pk && !(twin && i == pk + 1) && D (i) >= D (pk))
                    ++bad;
                if (i > 0 && apart (D (i), D (i - 1)) > p.leap)
                    ++bad;
                if (!inRange (D (i)))
                    bad += 2;
            }
            if (wo.wrap && n > 1 && apart (D (n - 1), D (0)) > p.leap)
                ++bad;
            for (const auto& [ja, jb] : wo.joins)
                if (static_cast<std::int32_t> (ja) < n && static_cast<std::int32_t> (jb) < n
                    && apart (D (static_cast<std::int32_t> (ja)), D (static_cast<std::int32_t> (jb))) > p.leap)
                    ++bad;
            if (high - low > s.spec->reg.span)
                bad += 2;
            {
                // never bar 1 moved: the same contour in both bars
                std::vector<std::int32_t> b1, b2;
                for (std::int32_t i = 0; i < n; ++i)
                    (note (i).step % 32 < 16 ? b1 : b2).push_back (D (i));
                if (b1.size() == b2.size() && b1.size() > 1)
                {
                    bool same = true;
                    for (std::size_t j = 1; j < b1.size(); ++j)
                        same = same && (b1[j] - b1[j - 1]) == (b2[j] - b2[j - 1]);
                    bad += same ? 3 : 0;
                }
            }
            if (bad < bestBad)
            {
                bestBad = bad;
                best = d;
                bestPeak = pk;
            }
            if (bad == 0)
                break;
        }
        for (HookNote& x : ns)
            x.peak = false;
        note (bestPeak).peak = true;
        if (twin && bestPeak + 1 < n)
            note (bestPeak + 1).peak = true;
        for (std::int32_t i = 0; i < n; ++i)
            note (i).degree = best[static_cast<std::size_t> (i)];
    }

    // The other end degrees by weight, heaviest first (the lower degree first among equals).
    std::vector<std::int32_t> endAlternatives (const std::array<std::int32_t, 7>& weights, std::int32_t drawn)
    {
        std::vector<std::pair<std::int32_t, std::int32_t>> v;
        for (std::int32_t d = 0; d < 7; ++d)
            if (d != degreeClass (drawn) && weights[static_cast<std::size_t> (d)] > 0)
                v.emplace_back (-weights[static_cast<std::size_t> (d)], d);
        std::stable_sort (v.begin(), v.end());
        std::vector<std::int32_t> out;
        for (const auto& x : v)
            out.push_back (x.second);
        return out;
    }

    // The fixed accent shape: one velocity per onset, drawn once, so bars i and i + 2 of a line agree:
    // the downbeat's accent, beat 3's, the other accents', the rest's; the peak raised; one offset per
    // onset in +-shape.
    void shapeVelocity (const HookVelocity& v, Notes& ns, rng::Key k)
    {
        for (std::size_t i = 0; i < ns.size(); ++i)
        {
            HookNote& x = ns[i];
            const bool down = v.down >= 0 && x.accent && x.step % 16 == 0;
            const bool beat = v.beat >= 0 && x.accent && x.step % 8 == 0;
            const std::int32_t tier = down ? v.down : beat   ? v.beat
                                                  : x.accent ? v.accent
                                                             : v.other;
            const std::int32_t vel = tier + (x.peak ? v.peak : 0) + rng::between (k.with (i), -v.shape, v.shape);
            x.velocity = static_cast<std::uint8_t> (std::clamp (vel, 1, 127));
        }
    }

    bool inside (const SongHook& h, Range reg, std::int32_t degree)
    {
        const std::int32_t key = h.base + semitones (h.mode, degree);
        return key >= reg.lo && key <= reg.hi;
    }

    // Bar 2's first half differs from bar 1's in a pitch (where "headDiffers"): the last note of bar
    // 2's first half that isn't the peak takes a pool step up, else down, where the register, the
    // cap with the notes around it and the contour (no new top) allow; else the note before it.
    void headDiffers (const Setting& s, SongHook& h)
    {
        Notes& a = h.a.notes;
        std::vector<std::pair<std::int32_t, std::int32_t>> head1, head2;
        for (const HookNote& x : a)
        {
            if (x.step < 8)
                head1.emplace_back (x.step, x.degree);
            else if (x.step >= 16 && x.step < 24)
                head2.emplace_back (x.step - 16, x.degree);
        }
        if (head1 != head2 || head2.empty())
            return;
        std::int32_t top = INT32_MIN;
        for (const HookNote& x : a)
            top = std::max (top, x.degree);
        const std::size_t n = a.size();
        const auto fits = [&] (std::size_t j, std::int32_t d) {
            const std::int32_t before = a[(j + n - 1) % n].degree, after = a[(j + 1) % n].degree;
            return inside (h, s.a, d) && d < top && std::abs (semitones (h.mode, d) - semitones (h.mode, before)) <= s.spec->pitch.leap
                   && std::abs (semitones (h.mode, d) - semitones (h.mode, after)) <= s.spec->pitch.leap;
        };
        for (std::size_t j = n; j-- > 0;)
        {
            HookNote& x = a[j];
            if (x.step < 16 || x.step >= 24 || x.peak)
                continue;
            for (const std::int32_t dir : { 1, -1 })
            {
                const std::int32_t d = nextPool (s.spec->pitch, x.degree, dir);
                if (fits (j, d))
                {
                    x.degree = d;
                    return;
                }
            }
        }
    }

    // ------------------------------------------------------------------ the material

    // A: the hook, a 2-bar riff. Sets h.a, h.base, h.contour and h.varyDegree.
    void drawRiff (const Setting& s, SongHook& h, rng::Key kh)
    {
        const HookSpec& spec = *s.spec;
        {
            std::vector<std::int32_t> w;
            for (const auto& c : spec.contours)
                w.push_back (c.second);
            const std::size_t ci = rng::pickWeighted (kh.with (rng::tag ("contour")), w);
            h.contour = ci < spec.contours.size() ? spec.contours[ci].first : HookContour::late;
        }
        h.a.bars = 2;
        h.a.notes = riffRhythm (spec.cells, spec, s.pedal, kh.with (rng::tag ("rhythm")));
        const std::int32_t st = pickDegree (kh.with (rng::tag ("start")), spec.pitch.start);
        const std::int32_t en = pickDegree (kh.with (rng::tag ("end")), spec.pitch.end);
        // the register first: the tonic's octave that puts the start inside the register and the 3rd
        // above the start nearest the center, so the walk draws only degrees inside it
        {
            std::int32_t bestCost = INT32_MAX;
            h.base = s.tonic;
            for (std::int32_t base = s.tonic; base < 128; base += 12)
            {
                const std::int32_t start = base + semitones (h.mode, st);
                const std::int32_t cost = std::abs (base + semitones (h.mode, st + 2) - spec.reg.center) + ((start < s.a.lo || start > s.a.hi) ? 1000 : 0);
                if (cost < bestCost)
                {
                    bestCost = cost;
                    h.base = base;
                }
            }
        }
        if (h.contour == HookContour::twin && h.a.notes.size() < 5)
            h.contour = HookContour::late; // a top struck twice needs five notes at least
        WalkOpts wo;
        wo.base = h.base;
        wo.reg = s.a;
        wo.wrap = true;
        wo.twin = h.contour == HookContour::twin;
        wo.endAlts = endAlternatives (spec.pitch.end, en);
        walk (s, h.a.notes, kh.with (rng::tag ("walk")), st, en, h.contour == HookContour::early ? spec.pitch.early : spec.pitch.peak, wo);
        if (spec.answer.headDiffers)
            headDiffers (s, h);
        shapeVelocity (spec.vel, h.a.notes, kh.with (rng::tag ("vel")));
        std::array<std::int32_t, 7> vary = spec.pitch.end;
        if (!h.a.notes.empty())
            vary[static_cast<std::size_t> (degreeClass (h.a.notes.back().degree))] = 0;
        h.varyDegree = pickDegree (kh.with (rng::tag ("vary")), vary);
    }

    // B's first note: the tonic, or the 5th when A starts on the tonic, in the octave inside B's
    // register that doesn't take A's key at that 16th (the nearest of the walk's order), so no bar of
    // B equals a bar that drop 1 plays.
    std::int32_t newPhraseStart (const Setting& s, const SongHook& h, std::int32_t firstStep, std::int32_t shift)
    {
        const std::int32_t degree = (h.a.notes.empty() || degreeClass (h.a.notes.front().degree) != 0 ? 0 : 4) + shift;
        std::int32_t fallback = degree;
        bool found = false;
        for (const std::int32_t o : { 0, -7, 7, -14, 14 })
        {
            const std::int32_t d = degree + o;
            if (!inside (h, s.b, d))
                continue;
            if (!found)
            {
                fallback = d;
                found = true;
            }
            const std::int32_t key = h.base + semitones (h.mode, d);
            const bool taken = std::any_of (h.a.notes.begin(), h.a.notes.end(), [&] (const HookNote& x) {
                return x.step % 16 == firstStep % 16 && h.base + semitones (h.mode, x.degree) == key;
            });
            if (!taken)
                return d;
        }
        return fallback;
    }

    // B, drop 2's new phrase, in the style's idiom, inside its own register, every join under the cap.
    void drawNewPhrase (const Setting& s, SongHook& h, rng::Key kh)
    {
        const HookSpec& spec = *s.spec;
        const HookPitch& p = spec.pitch;
        const rng::Key kb = kh.with (rng::tag ("b"));
        const std::int32_t shift = 7 * spec.period.octave;
        const auto inB = [&] (std::int32_t d) { return inside (h, s.b, d); };
        const auto apart = [&] (std::int32_t x, std::int32_t y) { return std::abs (semitones (h.mode, x) - semitones (h.mode, y)); };
        WalkOpts wo;
        wo.base = h.base;
        wo.reg = s.b;
        wo.fixedStart = true;
        if (spec.period.idiom == HookIdiom::stab)
        {
            // the acid stab: a second 2-bar riff from the stab cells, on the tonic by repeats and
            // steps, its last note the 5th
            static constexpr std::array<std::int32_t, 8> stabMoves { 30, 50, 15, 5, 0, 0, 0, 0 };
            h.b.bars = 2;
            h.b.notes = riffRhythm (spec.period.cells, spec, s.pedal, kb.with (rng::tag ("rhythm")));
            if (h.b.notes.empty())
                return;
            wo.wrap = true;
            wo.moves = &stabMoves;
            walk (s, h.b.notes, kb.with (rng::tag ("walk")), newPhraseStart (s, h, h.b.notes.front().step, shift), 4 + shift, Range { 26, 31 }, wo);
            shapeVelocity (spec.vel, h.b.notes, kb.with (rng::tag ("vel")));
            return;
        }
        Notes aBar1;
        for (const HookNote& x : h.a.notes)
            if (x.step < 16)
                aBar1.push_back (x);
        const Notes rhythm = periodRhythm (aBar1, spec, s.pedal, kb.with (rng::tag ("rhythm")));
        Notes ab; // bars a and b, walked together
        for (const HookNote& x : rhythm)
            if (x.step < 32)
                ab.push_back (x);
        if (ab.empty())
            return;
        const std::int32_t en = pickDegree (kb.with (rng::tag ("end")), p.end) + shift;
        {
            // the joins the period plays: b's last note into a', a's last note into c (a's first)
            std::size_t aLast = 0;
            for (std::size_t j = 0; j < ab.size(); ++j)
                if (ab[j].step < 16)
                    aLast = j;
            wo.joins = { { ab.size() - 1, 0 }, { aLast, 0 } };
            for (const std::int32_t d : endAlternatives (p.end, en - shift))
                wo.endAlts.push_back (d + shift);
        }
        walk (s, ab, kb.with (rng::tag ("walk")), newPhraseStart (s, h, ab.front().step, shift), en, Range { 22, 26 }, wo);
        h.b.bars = 4;
        h.b.notes = ab;
        const std::int32_t aFirst = ab.front().degree, bLast = ab.back().degree;
        // a': one note moved a pool step, inside the register and under the cap with the notes around
        // it (b's last note before it, c's first, a's first, after it). The organ: the top note up,
        // else the next-highest, else the lowest down. The glass: the last note down, else up.
        {
            Notes a2;
            for (const HookNote& x : ab)
                if (x.step < 16)
                    a2.push_back (x);
            const auto fits = [&] (std::size_t j, std::int32_t d) {
                const std::int32_t before = j > 0 ? a2[j - 1].degree : bLast;
                const std::int32_t after = j + 1 < a2.size() ? a2[j + 1].degree : aFirst;
                return inB (d) && apart (d, before) <= p.leap && apart (d, after) <= p.leap;
            };
            bool changed = false;
            if (!a2.empty() && spec.period.idiom == HookIdiom::glass)
            {
                const std::size_t j = a2.size() - 1;
                for (const std::int32_t d : { nextPool (p, a2[j].degree, -1), nextPool (p, a2[j].degree, 1) })
                    if (!changed && fits (j, d))
                    {
                        a2[j].degree = d;
                        changed = true;
                    }
            }
            else if (!a2.empty())
            {
                std::vector<std::size_t> order (a2.size());
                for (std::size_t j = 0; j < order.size(); ++j)
                    order[j] = j;
                std::stable_sort (order.begin(), order.end(), [&] (std::size_t x, std::size_t y) { return a2[x].degree > a2[y].degree; });
                for (const std::size_t j : order)
                    if (!changed && fits (j, nextPool (p, a2[j].degree, 1)))
                    {
                        a2[j].degree = nextPool (p, a2[j].degree, 1);
                        changed = true;
                    }
                for (auto it = order.rbegin(); it != order.rend() && !changed; ++it)
                    if (fits (*it, nextPool (p, a2[*it].degree, -1)))
                    {
                        a2[*it].degree = nextPool (p, a2[*it].degree, -1);
                        changed = true;
                    }
            }
            for (HookNote x : a2)
            {
                x.step += 32;
                h.b.notes.push_back (x);
            }
        }
        // c
        {
            Notes c;
            std::int32_t last = aFirst;
            for (const HookNote& x : ab)
                if (x.step < 8)
                {
                    c.push_back (x);
                    last = x.degree;
                }
            if (spec.period.idiom == HookIdiom::glass)
            {
                // a's rhythm: the period's top (its highest note before c) struck again on the second
                // half's first onset, or the highest note the cap allows from the note before, then a
                // step under it
                std::int32_t top = INT32_MIN;
                for (const HookNote& x : h.b.notes)
                    top = std::max (top, x.degree);
                std::int32_t reach = top;
                for (std::int32_t g = 0; g < 14 && reach > last && apart (reach, last) > p.leap; ++g)
                    reach = nextPool (p, reach, -1);
                bool first = true;
                for (const HookNote& x : ab)
                    if (x.step >= 8 && x.step < 16)
                    {
                        HookNote y = x;
                        y.degree = first ? reach : (inB (nextPool (p, reach, -1)) ? nextPool (p, reach, -1) : reach);
                        y.accent = first;
                        first = false;
                        c.push_back (y);
                    }
            }
            else
            {
                // the organ: a fall by pool steps on 16ths 8, 10 and 12 to the cadence's end (5, 2 or b3,
                // never the tonic), the last note held to the bar line; it starts within the cap of the
                // note before it and ends within the cap of a's first note, else the next end by weight
                const std::int32_t e = pickDegree (kb.with (rng::tag ("cadence")), spec.period.cadence);
                std::vector<std::int32_t> ends { e };
                for (const std::int32_t d : endAlternatives (spec.period.cadence, e))
                    ends.push_back (d);
                std::int32_t target = e + shift, bestCost = INT32_MAX;
                for (std::size_t r = 0; r < ends.size(); ++r)
                    for (std::int32_t o = -14; o <= 14; o += 7)
                    {
                        const std::int32_t t = ends[r] + shift + o;
                        const std::int32_t d2 = nextPool (p, t, 1), d3 = nextPool (p, d2, 1);
                        const std::int32_t in = apart (d3, last), wrapLeap = apart (t, aFirst);
                        const std::int32_t cost = static_cast<std::int32_t> (r) * 100 + in + ((inB (t) && inB (d2) && inB (d3)) ? 0 : 100000)
                                                  + ((in <= p.leap && wrapLeap <= p.leap) ? 0 : 10000);
                        if (cost < bestCost)
                        {
                            bestCost = cost;
                            target = t;
                        }
                    }
                const std::int32_t d2 = nextPool (p, target, 1), d3 = nextPool (p, d2, 1);
                for (const auto& [step, degree] : { std::pair { 8, d3 }, std::pair { 10, d2 }, std::pair { 12, target } })
                {
                    HookNote n;
                    n.step = step;
                    n.degree = degree;
                    n.len16 = static_cast<std::uint8_t> (step == 12 ? 4 : 2);
                    n.accent = step == 12;
                    c.push_back (n);
                }
            }
            for (HookNote x : c)
            {
                x.step += 48;
                x.peak = false;
                h.b.notes.push_back (x);
            }
        }
        shapeVelocity (spec.vel, h.b.notes, kb.with (rng::tag ("vel")));
    }

    // The response (ANSWER's): A's second-half onsets, walked afresh inside the register from below
    // the call and ending on varyDegree, 6 velocity steps softer; never A's own second half in a bar.
    void drawResponse (const Setting& s, SongHook& h, rng::Key kh)
    {
        h.response.bars = h.a.bars;
        h.response.notes.clear();
        for (const HookNote& x : h.a.notes)
            if (x.step % 16 >= 8)
                h.response.notes.push_back (x);
        if (h.response.notes.empty())
            return;
        WalkOpts wo;
        wo.base = h.base;
        wo.reg = s.a;
        walk (s, h.response.notes, kh.with (rng::tag ("response")).with (rng::tag ("walk")), h.a.notes.front().degree - 2, h.varyDegree, Range { 0, 64 }, wo);
        for (std::int32_t bar = 0; bar < h.a.bars; ++bar)
        {
            std::vector<std::pair<std::int32_t, std::int32_t>> mine, theirs;
            for (const HookNote& x : h.response.notes)
                if (x.step / 16 == bar)
                    mine.emplace_back (x.step, x.degree);
            for (const HookNote& x : h.a.notes)
                if (x.step / 16 == bar && x.step % 16 >= 8)
                    theirs.emplace_back (x.step, x.degree);
            if (mine != theirs)
                continue;
            for (auto it = h.response.notes.rbegin(); it != h.response.notes.rend(); ++it)
                if (it->step / 16 == bar)
                {
                    const std::int32_t down = nextPool (s.spec->pitch, it->degree, -1), up = nextPool (s.spec->pitch, it->degree, 1);
                    it->degree = inside (h, s.a, down) ? down : inside (h, s.a, up) ? up
                                                                                    : it->degree;
                    break;
                }
        }
        for (HookNote& x : h.response.notes)
            x.velocity = static_cast<std::uint8_t> (std::max (1, x.velocity - 6));
    }

    SongHook drawOnce (const Setting& s, rng::Key kh)
    {
        SongHook h;
        h.lane = s.lane;
        h.mode = s.mode;
        drawRiff (s, h, kh);
        drawNewPhrase (s, h, kh);
        drawResponse (s, h, kh);
        return h;
    }

    // The rules a material keeps, counted where it breaks them: every move under the cap (A's and B's,
    // their wraps and B's joins included); A's contour (the late and early peak the highest note
    // alone in its window, the twin top struck on two onsets in a row); every note inside its
    // register; bar 2's first half differing from bar 1's in a pitch (with "headDiffers"); bar 2
    // never bar 1 moved; B's first note the tonic, or the 5th when A starts on it, never A's key at
    // that 16th; the organ's a' one note away from a; the response never A's second half in a bar.
    std::int32_t brokenRules (const Setting& s, const SongHook& h)
    {
        const HookSpec& spec = *s.spec;
        const auto key = [&h] (std::int32_t degree) { return h.base + semitones (h.mode, degree); };
        std::int32_t broken = 0;
        for (const HookPhrase* p : { &h.a, &h.b })
            for (std::size_t i = 0; i < p->notes.size(); ++i)
                broken += std::abs (key (p->notes[(i + 1) % p->notes.size()].degree) - key (p->notes[i].degree)) > spec.pitch.leap ? 1 : 0;
        if (h.a.notes.empty() || h.b.notes.empty())
            return broken + 1;
        // the contour
        std::int32_t top = INT32_MIN;
        for (const HookNote& x : h.a.notes)
            top = std::max (top, x.degree);
        std::vector<std::size_t> tops;
        for (std::size_t i = 0; i < h.a.notes.size(); ++i)
            if (h.a.notes[i].degree == top)
                tops.push_back (i);
        if (h.contour == HookContour::twin)
        {
            bool twice = false;
            for (std::size_t i = 0; i + 1 < tops.size(); ++i)
                twice = twice || tops[i + 1] == tops[i] + 1;
            broken += twice ? 0 : 1;
        }
        else
        {
            const Range window = h.contour == HookContour::early ? spec.pitch.early : spec.pitch.peak;
            const HookNote& peak = h.a.notes[tops.front()];
            broken += tops.size() == 1 && peak.peak && peak.step >= window.lo && peak.step <= window.hi ? 0 : 1;
        }
        // the registers
        for (const HookPhrase* p : { &h.a, &h.response })
            for (const HookNote& x : p->notes)
                broken += inside (h, s.a, x.degree) ? 0 : 1;
        for (const HookNote& x : h.b.notes)
            broken += inside (h, s.b, x.degree) ? 0 : 1;
        // bar 2 against bar 1
        std::vector<std::pair<std::int32_t, std::int32_t>> head1, head2;
        std::vector<std::int32_t> on1, on2, d1, d2;
        for (const HookNote& x : h.a.notes)
        {
            (x.step < 16 ? on1 : on2).push_back (x.step % 16);
            (x.step < 16 ? d1 : d2).push_back (x.degree);
            if (x.step % 16 < 8)
                (x.step < 16 ? head1 : head2).emplace_back (x.step % 16, x.degree);
        }
        if (spec.answer.headDiffers && head1 == head2)
            ++broken;
        if (on1 == on2 && d1.size() > 1)
        {
            bool same = true;
            for (std::size_t j = 1; j < d1.size(); ++j)
                same = same && d1[j] - d1[j - 1] == d2[j] - d2[j - 1];
            broken += same ? 1 : 0;
        }
        // B's first note
        const HookNote& b0 = h.b.notes.front();
        if (degreeClass (b0.degree) != (degreeClass (h.a.notes.front().degree) == 0 ? 4 : 0))
            ++broken;
        for (const HookNote& x : h.a.notes)
            if (x.step % 16 == b0.step % 16 && key (x.degree) == key (b0.degree))
                ++broken;
        // the organ's a'
        if (spec.period.idiom == HookIdiom::organ)
        {
            std::vector<std::int32_t> a, a2;
            for (const HookNote& x : h.b.notes)
            {
                if (x.step < 16)
                    a.push_back (x.degree);
                else if (x.step >= 32 && x.step < 48)
                    a2.push_back (x.degree);
            }
            std::int32_t moved = a.size() == a2.size() ? 0 : 2;
            for (std::size_t j = 0; j < a.size() && j < a2.size(); ++j)
                moved += a[j] != a2[j] ? 1 : 0;
            broken += moved == 1 ? 0 : 1;
        }
        // the response
        for (std::int32_t bar = 0; bar < h.a.bars; ++bar)
        {
            std::vector<std::pair<std::int32_t, std::int32_t>> mine, theirs;
            for (const HookNote& x : h.response.notes)
                if (x.step / 16 == bar)
                    mine.emplace_back (x.step, x.degree);
            for (const HookNote& x : h.a.notes)
                if (x.step / 16 == bar && x.step % 16 >= 8)
                    theirs.emplace_back (x.step, x.degree);
            broken += !mine.empty() && mine == theirs ? 1 : 0;
        }
        return broken;
    }

    // A material drawn below km, drawn again below km.with (tag ("redraw")).with (r), r = 1-8, while
    // it still breaks a rule after its walks; the first that breaks none, else the one breaking fewest.
    std::pair<SongHook, std::int32_t> materialBelow (const Setting& s, rng::Key km)
    {
        SongHook best = drawOnce (s, km);
        std::int32_t bestBroken = brokenRules (s, best);
        for (std::int32_t r = 1; r <= redraws && bestBroken > 0; ++r)
        {
            SongHook h = drawOnce (s, km.with (rng::tag ("redraw")).with (r));
            const std::int32_t broken = brokenRules (s, h);
            if (broken < bestBroken)
            {
                best = std::move (h);
                bestBroken = broken;
            }
        }
        return { std::move (best), bestBroken };
    }

    // Bar `bar` (0-7) of a line of the form on the motif, before the fit and the guard: its notes,
    // each with its 16th in the bar (AUGMENT doubles 16ths and lengths, its lengths to the bar line at
    // most). FULL plays the phrase; FRAGMENT each bar's first half, and its last onset when its second
    // half holds two or more; SKELETON the accents; ANSWER the first half, then the response in the
    // second; AUGMENT the riff at half speed.
    Notes textureNotes (const SongHook& h, HookForm form, std::uint8_t motif, std::int32_t bar)
    {
        Notes out;
        const HookPhrase& p = motif == 1 ? h.b : h.a;
        if (p.bars <= 0 || form == HookForm::out)
            return out;
        const auto add = [&out] (HookNote n, std::int32_t sixteenth) {
            n.step = sixteenth;
            out.push_back (n);
        };
        const auto barOf = [] (const HookPhrase& ph, std::int32_t phraseBar) {
            Notes notes;
            for (const HookNote& n : ph.notes)
                if (n.step / 16 == phraseBar)
                    notes.push_back (n);
            return notes;
        };
        if (form == HookForm::augment)
        {
            const std::int32_t half = bar % 2;
            for (HookNote n : barOf (p, (bar / 2) % p.bars))
                if (n.step % 16 / 8 == half)
                {
                    const std::int32_t at = (n.step % 16) * 2 - half * 16;
                    n.len16 = static_cast<std::uint8_t> (std::max (1, std::min (n.len16 * 2, 16 - at)));
                    add (n, at);
                }
            return out;
        }
        const std::int32_t phraseBar = bar % p.bars;
        const Notes notes = barOf (p, phraseBar);
        switch (form)
        {
            case HookForm::full:
                for (const HookNote& n : notes)
                    add (n, n.step % 16);
                break;
            case HookForm::fragment:
            {
                const auto second = std::count_if (notes.begin(), notes.end(), [] (const HookNote& n) { return n.step % 16 >= 8; });
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
                for (const HookNote& n : barOf (h.response, phraseBar % std::max (1, h.response.bars)))
                    if (n.step % 16 >= 8)
                        add (n, n.step % 16);
                break;
            case HookForm::out:
            case HookForm::augment:
                break;
        }
        return out;
    }

    // ------------------------------------------------------------------ a line, realized

    LineState stateOf (HookForm form, std::uint8_t motif)
    {
        LineState st;
        st.form = form;
        st.motif = motif;
        return st;
    }

    // The written-reference mud rule against the chord-lane keys of a 16th: no minor 9th either way, and a
    // semitone only with the lower note at the kit's floor or above.
    bool rubs (const std::array<std::uint64_t, 2>& keys, std::int32_t key, std::int32_t floor)
    {
        const auto has = [&keys] (std::int32_t k) { return k >= 0 && k < 128 && ((keys[static_cast<std::size_t> (k / 64)] >> (k % 64)) & 1u) != 0; };
        return has (key + 13) || has (key - 13) || (has (key + 1) && key < floor) || (has (key - 1) && key - 1 < floor);
    }

    // The voicing guard's check over every 16th a note can sound in (its length under the lane grid's
    // length scale).
    bool clean (const LineContext& c, std::int32_t floor, std::int32_t key, std::int32_t step, std::int64_t length)
    {
        const std::int64_t span = std::max<std::int64_t> (1, util::ceilDiv (length * c.reach, ticksPerStep * 1000));
        for (std::int64_t q = step; q < std::min<std::int64_t> (128, step + span); ++q)
            if (rubs (c.chord[static_cast<std::size_t> (q)], key, floor))
                return false;
        return true;
    }

    // True if the pedal plays under the line (its "in": FULL, ANSWER, FRAGMENT, NEW).
    bool pedalUnder (const HookPedal& pedal, const LineState& st)
    {
        HookOp op = HookOp::full;
        if (st.form == HookForm::full)
            op = st.motif == 1 ? HookOp::newPhrase : HookOp::full;
        else if (st.form == HookForm::answer)
            op = HookOp::answer;
        else if (st.form == HookForm::fragment)
            op = HookOp::fragment;
        else
            return false;
        return std::find (pedal.in.begin(), pedal.in.end(), op) != pedal.in.end();
    }

    // A line of the state in the section, as its lane plays it (plan, "Realization, per line"): the
    // texture's notes keyed in the section's own mode; a note the mode takes out of the register moves
    // to the nearest pool degree inside it within the cap, else rests (a SEQUENCE line fits whole or
    // not at all); VARY and CADENCE end the line (the end degree in the octave inside the register
    // nearest the note before, within the cap, else the next end by weight; a cadence held to the bar
    // line); the avoid-note fit; the voicing guard (a rubbing note moves to the nearest clean pool
    // degree within two steps that keeps the cap with the note before, as moved, and the one after, as
    // written, else rests, and the notes either side of a rest are checked like a move); FUT02's pedal
    // under the top voice, through the same guard. Lengths from the cell, never into the next note (10
    // ticks short) nor past the line.
    Realized realizeLine (const Setting& s, const LineContext& c, const SongHook& h, const LineState& st)
    {
        Realized out;
        if (!playing (st))
            return out;
        const HookSpec& spec = *s.spec;
        const HookPitch& p = spec.pitch;
        const Range reg = st.motif == 1 ? s.b : s.a;
        const auto keyOf = [&] (std::int32_t d) { return h.base + semitones (c.mode, d); };
        const auto inReg = [&reg] (std::int32_t k) { return k >= reg.lo && k <= reg.hi; };
        std::vector<LineNote> top;
        for (std::int32_t b = 0; b < barsPerBlock; ++b)
        {
            if (b == barsPerBlock - 1 && st.ending == HookEnding::breath)
                continue;
            for (const HookNote& n : textureNotes (h, st.form, st.motif, b))
            {
                LineNote x;
                x.step = b * 16 + n.step;
                x.degree = n.degree + st.shift;
                x.key = keyOf (x.degree);
                x.length = n.len16 * ticksPerStep;
                x.velocity = n.velocity;
                x.accent = n.accent;
                top.push_back (x);
            }
        }
        std::stable_sort (top.begin(), top.end(), [] (const LineNote& a, const LineNote& b) { return a.step < b.step; });
        std::vector<bool> rest (top.size(), false);
        // The cap binds a move where the material's own move keeps it: every move of FULL (the riff,
        // its wrap and the period's joins), but not a texture's own leaps (a SKELETON's or a
        // FRAGMENT's, over the notes it leaves out). A move between notes j and m that sound (the
        // ones between them resting) binds when every material move between them keeps the cap.
        std::vector<bool> held (top.size(), true);
        for (std::size_t i = 1; i < top.size(); ++i)
            held[i] = std::abs (semitones (h.mode, top[i].degree) - semitones (h.mode, top[i - 1].degree)) <= p.leap;
        const auto binds = [&held] (std::size_t j, std::size_t m) {
            for (std::size_t t = j + 1; t <= m; ++t)
                if (!held[t])
                    return false;
            return true;
        };
        // key k for note i keeps the cap with the note that sounds before it (as moved) / after it (as written)
        const auto keepsBefore = [&] (std::size_t i, std::int32_t k) {
            for (std::size_t j = i; j-- > 0;)
                if (!rest[j])
                    return !binds (j, i) || std::abs (k - top[j].key) <= p.leap;
            return true;
        };
        const auto keepsAfter = [&] (std::size_t i, std::int32_t k) {
            for (std::size_t m = i + 1; m < top.size(); ++m)
                if (!rest[m])
                    return !binds (i, m) || std::abs (k - top[m].key) <= p.leap;
            return true;
        };
        const auto keeps = [&] (std::size_t i, std::int32_t k) { return keepsBefore (i, k) && keepsAfter (i, k); };
        // the nearest pool degree that fits: a step down, a step up, two down, two up
        const auto moveTo = [&p] (std::int32_t d, const auto& fits) -> std::optional<std::int32_t> {
            const std::int32_t down = nextPool (p, d, -1), up = nextPool (p, d, 1);
            for (const std::int32_t x : { down, up, nextPool (p, down, -1), nextPool (p, up, 1) })
                if (fits (x))
                    return x;
            return std::nullopt;
        };
        const auto place = [&] (std::size_t i, std::int32_t d) {
            top[i].degree = d;
            top[i].key = keyOf (d);
        };

        // 1. the register
        for (std::size_t i = 0; i < top.size(); ++i)
        {
            if (inReg (top[i].key))
                continue;
            if (st.shift != 0)
            {
                out.ok = false;
                return out;
            }
            if (const auto d = moveTo (top[i].degree, [&] (std::int32_t x) {
                    const std::int32_t k = keyOf (x);
                    return inReg (k) && keeps (i, k);
                }))
                place (i, *d);
            else
                rest[i] = true;
        }

        // 2. VARY and CADENCE: the line's last note
        if (st.ending == HookEnding::vary || st.ending == HookEnding::cadence)
        {
            std::size_t last = top.size();
            for (std::size_t i = top.size(); i-- > 0;)
                if (!rest[i])
                {
                    last = i;
                    break;
                }
            if (last < top.size() && top[last].step >= (barsPerBlock - 1) * 16)
            {
                const bool vary = st.ending == HookEnding::vary;
                std::array<std::int32_t, 7> weights = p.end;
                if (vary && !h.a.notes.empty())
                    weights[static_cast<std::size_t> (degreeClass (h.a.notes.back().degree))] = 0;
                const std::int32_t drawn = vary ? h.varyDegree : 4;
                std::vector<std::int32_t> ends { drawn };
                for (const std::int32_t d : endAlternatives (weights, drawn))
                    ends.push_back (d);
                std::int32_t prev = -1;
                for (std::size_t j = last; j-- > 0 && prev < 0;)
                    if (!rest[j])
                        prev = top[j].key;
                for (const std::int32_t e : ends)
                {
                    if (vary && degreeClass (e) == degreeClass (top[last].degree))
                        continue; // a VARY that varies
                    std::int32_t best = INT32_MIN, bestCost = INT32_MAX;
                    for (std::int32_t o = -21; o <= 21; o += 7)
                    {
                        const std::int32_t d = util::floorDiv (top[last].degree, 7) * 7 + degreeClass (e) + o;
                        const std::int32_t k = keyOf (d);
                        const std::int32_t cost = std::abs (k - (prev >= 0 ? prev : top[last].key));
                        if (inReg (k) && keepsBefore (last, k) && cost < bestCost)
                        {
                            best = d;
                            bestCost = cost;
                        }
                    }
                    if (best != INT32_MIN)
                    {
                        place (last, best);
                        break;
                    }
                }
                if (!vary)
                    top[last].length = (16 - top[last].step % 16) * ticksPerStep;
            }
        }

        // 3. the avoid-note fit: a strong note a semitone over a clash tone of its context
        const auto avoids = [&c] (std::int32_t step, std::int32_t key) { return ((c.avoid[static_cast<std::size_t> (step)] >> pitchClass (key)) & 1u) != 0; };
        for (std::size_t i = 0; i < top.size(); ++i)
        {
            const LineNote x = top[i];
            const bool strong = p.beats ? x.step % 4 == 0 : (x.step % 4 == 0 || x.accent);
            if (rest[i] || !strong || !avoids (x.step, x.key))
                continue;
            if (const auto d = moveTo (x.degree, [&] (std::int32_t y) {
                    const std::int32_t k = keyOf (y);
                    return inReg (k) && !avoids (x.step, k) && keeps (i, k);
                }))
                place (i, *d);
        }

        // 4. the voicing guard
        for (std::size_t i = 0; i < top.size(); ++i)
        {
            if (rest[i])
                continue;
            const LineNote x = top[i];
            // a rubbing note, or one a rest (or the section's mode) leaves over a cap that binds: a move
            if (clean (c, spec.guard, x.key, x.step, x.length) && keepsBefore (i, x.key))
                continue;
            if (const auto d = moveTo (x.degree, [&] (std::int32_t y) {
                    const std::int32_t k = keyOf (y);
                    return inReg (k) && clean (c, spec.guard, k, x.step, x.length) && keeps (i, k);
                }))
                place (i, *d);
            else
                rest[i] = true;
        }
        std::vector<LineNote> notes;
        for (std::size_t i = 0; i < top.size(); ++i)
            if (!rest[i])
                notes.push_back (top[i]);

        // 5. the pedal: after each top note, on the free 16ths before the next one in its bar (never on
        // 16th 15, and under FRAGMENT only in the half it plays), the pedal's degrees in order that sit
        // under it, at most `max`; one the guard finds rubbing rests in its 16th
        if (spec.pedal && pedalUnder (*spec.pedal, st))
        {
            const HookPedal& pd = *spec.pedal;
            std::vector<LineNote> pedal;
            for (std::size_t i = 0; i < notes.size(); ++i)
            {
                const LineNote& t = notes[i];
                const std::int32_t barStart = t.step / 16 * 16;
                std::int32_t limit = barStart + 15;
                if (i + 1 < notes.size() && notes[i + 1].step / 16 == t.step / 16)
                    limit = std::min (limit, notes[i + 1].step);
                if (st.form == HookForm::fragment)
                    limit = std::min (limit, barStart + (t.step % 16 < 8 ? 8 : 16));
                std::int32_t q = t.step + 1, placed = 0;
                for (std::size_t j = 0; j < pd.degrees.size() && j < pd.vel.size() && placed < pd.max && q < limit; ++j)
                {
                    const std::int32_t key = s.pedalTonic + semitones (c.mode, pd.degrees[j]);
                    if (key >= t.key)
                        continue; // only under the top note it follows
                    ++placed;
                    if (clean (c, spec.guard, key, q, pedalLength))
                    {
                        LineNote n;
                        n.step = q;
                        n.key = key;
                        n.degree = pd.degrees[j];
                        n.length = pedalLength;
                        n.velocity = static_cast<std::uint8_t> (std::clamp (pd.vel[j], 1, 127));
                        n.pedal = true;
                        pedal.push_back (n);
                    }
                    ++q;
                }
            }
            notes.insert (notes.end(), pedal.begin(), pedal.end());
            std::stable_sort (notes.begin(), notes.end(), [] (const LineNote& a, const LineNote& b) { return a.step < b.step; });
        }

        // 6. lengths: never into the next note (10 ticks short), nor past the line
        for (std::size_t i = 0; i < notes.size(); ++i)
        {
            std::int64_t limit = (static_cast<std::int64_t> (barsPerBlock) * stepsPerBar - notes[i].step) * ticksPerStep;
            if (i + 1 < notes.size())
                limit = std::min (limit, static_cast<std::int64_t> (notes[i + 1].step - notes[i].step) * ticksPerStep - 10);
            notes[i].length = std::max<std::int64_t> (1, std::min (notes[i].length, limit));
        }

        // 7. its prints, and a note in every bar
        for (const LineNote& n : notes)
            out.prints[static_cast<std::size_t> (n.step / 16)].push_back ((n.step % 16) * 128 + n.key);
        for (std::size_t b = 0; b < out.prints.size(); ++b)
        {
            tidy (out.prints[b]);
            if (out.prints[b].empty() && !(b + 1 == out.prints.size() && st.ending == HookEnding::breath))
                out.ok = false;
        }
        out.notes = std::move (notes);
        return out;
    }
}
