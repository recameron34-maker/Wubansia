// The hook (S1b2b, docs/plans/S1b2b-plan.md, "What the hook is" and "Where it plays"): its permanent
// keys, the line rule, the material planHook draws per seed from the style's hook block, and the line
// plan that places and realizes it over the drawn form.
//
// The material, in the plan's order: A's rhythm (a style cell, mutated, and bar 2 answering bar 1),
// the register (the tonic's octave), the contour, A's degrees (the walk over the fit table), the
// velocities (a fixed accent shape), varyDegree, B (drop 2's new phrase in the style's idiom: the
// organ or glass period, or the stab riff), the response; then the redraws and the texture check, on
// the material and on each section's realized lines.
// The line plan, per section in song order: the menu's draws, the DJ rules and the held-back rules,
// each line realized (the section's mode, the avoid-note fit, the voicing guard, FUT02's pedal) and
// checked by the line rule against the line before, with its fallbacks; then the carrier's bits and
// dropouts cleared on its OUT lines.
// Every draw hangs below hookKey (root) under the plan's permanent tags:
//   rhythm (cell, mutate with drop, which, add, at; bar2 with edit, at), start, end, contour,
//   walk (attempt, peak, twin), vel, vary, b (rhythm with half, bar2 [the stab's: cell, mutate,
//   bar2]; end, walk, cadence, vel), response (walk), redraw, check (attempt), plan (per section
//   enter; per line op).
// A redraw draws the whole material again below kh.with (tag ("redraw")).with (r), r = 1-8; a texture
// check's attempt below kh.with (tag ("check")).with (tag ("attempt")).with (t), t = 1-8, each with
// its own redraws below it. Integers only; every loop has a cap.

#include "gen/hook.h"
#include "gen/hook_material.h"

#include "gen/theory.h"
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

namespace wubansia::gen
{
    rng::Key hookKey (rng::Key root) noexcept { return root.with (rng::tag ("hook")); }

    rng::Key hookCellKey (rng::Key root) noexcept { return hookKey (root).with (rng::tag ("rhythm")).with (rng::tag ("cell")); }

    bool onTheLine (const LinePrints& before, const LinePrints& after) noexcept
    {
        // bars 0-6 all differ or all agree: a run of differing bars then starts at bar 0 (or there is
        // none); bar 7 is free, a run that starts there is on phrase too
        const bool first = before[0] != after[0];
        for (std::size_t b = 1; b + 1 < before.size(); ++b)
            if ((before[b] != after[b]) != first)
                return false;
        return true;
    }

    namespace
    {
        using namespace material;

        constexpr std::int32_t checks = 8; // of a material whose textures fail the line rule
        // The push-robust context of a tick in a section: the chord its loop position lays out unpushed,
        // plus the next one when the tick is within 240 ticks of that chord's change (a push of 120 or
        // 240 can bring it forward), so every 8-bar line of the section reads the same chords.
        std::vector<Chord> contextAt (const Song& song, const Section& s, std::int64_t tick)
        {
            std::vector<Chord> out;
            const SectionHarmony& h = song.harmony[s.index];
            const std::int64_t loop = h.loopBars * ticksPerBar;
            const std::int64_t unit = loop / 4;
            const std::int64_t pos = util::floorMod (tick - s.startTick(), loop);
            const std::size_t n = std::min (h.degrees.size(), h.units.size());
            std::int64_t at = 0;
            for (std::size_t i = 0; i < n; ++i)
            {
                const std::int64_t end = at + h.units[i] * unit;
                if (pos >= at && pos < end)
                {
                    out.push_back ({ degreePc (song.tonic, h.mode, h.degrees[i]), h.mode, h.degrees[i] });
                    const std::size_t j = (i + 1) % n;
                    if (end - pos <= 240 && j != i)
                        out.push_back ({ degreePc (song.tonic, h.mode, h.degrees[j]), h.mode, h.degrees[j] });
                    break;
                }
                at = end;
            }
            return out;
        }
        Fit fitFor (const Song& song, Mode mode, const HookPitch& pitch)
        {
            Fit f;
            f.tonic = song.tonic;
            f.mode = mode;
            f.tones = pitch.tones;
            for (const Section& s : song.sections)
                if (s.kind == "drop")
                {
                    for (std::int64_t rep = 0; rep * 2 < barsPerBlock; ++rep)
                        for (std::size_t st = 0; st < 32; ++st)
                            for (const Chord& c : contextAt (song, s, s.startTick() + (rep * 32 + static_cast<std::int64_t> (st)) * ticksPerStep))
                                f.at[st].push_back (c);
                    break;
                }
            return f;
        }

        // ------------------------------------------------------------------ the walk

        // ------------------------------------------------------------------ the texture check

        // The textures a line plays of the material: FULL, FRAGMENT (the half-bar call), SKELETON (the
        // accents), ANSWER (the call, then the response), AUGMENT (half speed), NEW (B in full).
        enum class Texture : std::uint8_t {
            full,
            fragment,
            skeleton,
            answer,
            augment,
            newPhrase
        };
        constexpr std::size_t textureCount = 6;

        Texture textureAt (std::size_t i) { return static_cast<Texture> (i); }

        // A texture's form and motif as a line plays it (NEW: the new phrase in full).
        std::pair<HookForm, std::uint8_t> formOf (Texture t)
        {
            switch (t)
            {
                case Texture::full:
                    return { HookForm::full, 0 };
                case Texture::fragment:
                    return { HookForm::fragment, 0 };
                case Texture::skeleton:
                    return { HookForm::skeleton, 0 };
                case Texture::answer:
                    return { HookForm::answer, 0 };
                case Texture::augment:
                    return { HookForm::augment, 0 };
                case Texture::newPhrase:
                    return { HookForm::full, 1 };
            }
            return { HookForm::full, 0 };
        }

        // A texture's bar (0-7 of a line) as G4 prints it: 16th * 128 + key, ascending, each once.
        BarPrint textureBar (const SongHook& h, Texture t, std::int32_t bar)
        {
            BarPrint out;
            const auto [form, motif] = formOf (t);
            for (const HookNote& n : textureNotes (h, form, motif, bar))
                out.push_back (n.step * 128 + h.base + semitones (h.mode, n.degree));
            tidy (out);
            return out;
        }

        LinePrints texturePrints (const SongHook& h, Texture t)
        {
            LinePrints p;
            for (std::size_t bar = 0; bar < p.size(); ++bar)
                p[bar] = textureBar (h, t, static_cast<std::int32_t> (bar));
            return p;
        }

        // The textures the style's menus can play: FULL (the rules place it), SKELETON and FRAGMENT (a
        // marked line falls back to them), and ANSWER, AUGMENT and NEW where a menu draws them.
        std::array<bool, textureCount> texturesOf (const HookSpec& spec)
        {
            std::array<bool, textureCount> plays {};
            plays[static_cast<std::size_t> (Texture::full)] = true;
            plays[static_cast<std::size_t> (Texture::fragment)] = true;
            plays[static_cast<std::size_t> (Texture::skeleton)] = true;
            for (const HookMenu& m : spec.menus)
                for (const auto* list : { &m.first, &m.next, &m.last })
                    for (const HookOpWeight& o : *list)
                    {
                        if (o.weight <= 0)
                            continue;
                        switch (o.op)
                        {
                            case HookOp::answer:
                                plays[static_cast<std::size_t> (Texture::answer)] = true;
                                break;
                            case HookOp::augment:
                                plays[static_cast<std::size_t> (Texture::augment)] = true;
                                break;
                            case HookOp::newPhrase:
                                plays[static_cast<std::size_t> (Texture::newPhrase)] = true;
                                break;
                            case HookOp::out:
                            case HookOp::repeat:
                            case HookOp::full:
                            case HookOp::fragment:
                            case HookOp::skeleton:
                            case HookOp::vary:
                            case HookOp::cadence:
                            case HookOp::breath:
                            case HookOp::sequence:
                                break;
                        }
                    }
            return plays;
        }

        // A texture that fails the check, and the texture it fails against (itself: a bar without a note).
        struct Failure
        {
            Texture texture = Texture::full;
            Texture against = Texture::full;
        };

        // The texture check on the material: every texture the style plays holds a note in every bar of a
        // line, and every two of them differ in every one of bars 0-6 or in none (onTheLine), so whichever
        // two the menus put side by side, the lead changes on the line.
        std::vector<Failure> textureFailures (const SongHook& h, const std::array<bool, textureCount>& plays)
        {
            std::vector<Failure> out;
            std::array<LinePrints, textureCount> prints;
            for (std::size_t i = 0; i < textureCount; ++i)
                if (plays[i])
                {
                    prints[i] = texturePrints (h, textureAt (i));
                    if (std::any_of (prints[i].begin(), prints[i].end(), [] (const BarPrint& b) { return b.empty(); }))
                        out.push_back ({ textureAt (i), textureAt (i) });
                }
            for (std::size_t i = 0; i < textureCount; ++i)
                for (std::size_t j = i + 1; j < textureCount; ++j)
                    if (plays[i] && plays[j] && !onTheLine (prints[i], prints[j]))
                        out.push_back ({ textureAt (i), textureAt (j) });
            return out;
        }

        // The texture a failure takes out of the song's menus: FULL and NEW never (the rules place them),
        // else the later of the pair in the order SKELETON, FRAGMENT, ANSWER, AUGMENT (a marked line falls
        // back to SKELETON, then FRAGMENT).
        std::int32_t keepRank (Texture t)
        {
            switch (t)
            {
                case Texture::full:
                    return 0;
                case Texture::newPhrase:
                    return 1;
                case Texture::skeleton:
                    return 2;
                case Texture::fragment:
                    return 3;
                case Texture::answer:
                    return 4;
                case Texture::augment:
                    return 5;
            }
            return 5;
        }

        // ------------------------------------------------------------------ the sections the lines play in

        // What a section gives its lines: its menu and its own mode, and per 16th of its 8-bar lines (the
        // harmony loops in 2 or 4 bars, and the guard takes the union of the section's lines, so every
        // line of a section reads the same) the pitch classes a semitone over a clash tone of the
        // push-robust context, and the keys the chord lanes sound there in any of its lines.
        struct SectionCtx : LineContext
        {
            const Section* sec = nullptr;
            const HookMenu* menu = nullptr; // the line plan's; nullptr where the carrier's plans don't play it
            bool after = false; // the first drop is behind it
            bool firstDrop = false;
            bool withheld = false; // before the first drop, a "withhold" kind: no FULL or NEW
            bool active = false; // the carrier plays in some block of it
        };

        std::vector<SectionCtx> sectionContexts (const Song& song, const Bundle& bundle, const Setting& s)
        {
            const HookSpec& spec = *s.spec;
            const LaneSpec& lane = bundle.style.lanes[s.lane];
            std::vector<SectionCtx> out (song.sections.size());
            bool after = false;
            for (const Section& sec : song.sections)
            {
                SectionCtx& c = out[sec.index];
                c.sec = &sec;
                c.mode = song.harmony[sec.index].mode;
                c.menu = spec.menu (sec.kind, after);
                c.after = after;
                c.firstDrop = !after && sec.kind == "drop";
                c.withheld = !after && std::find (spec.withhold.begin(), spec.withhold.end(), sec.kind) != spec.withhold.end();
                for (const std::uint64_t b : sec.blocks)
                    c.active = c.active || ((b >> s.lane) & 1u) != 0;
                const std::string_view grid = lane.gridFor (sec.kind);
                for (const LeadGrid& g : lane.lead.grids)
                    if (g.name == grid)
                        c.reach = std::max (1000, g.len);
                for (std::size_t q = 0; q < c.avoid.size(); ++q)
                    for (const Chord& ch : contextAt (song, sec, sec.startTick() + static_cast<std::int64_t> (q) * ticksPerStep))
                        for (const std::int32_t t : spec.pitch.clash)
                            c.avoid[q] = static_cast<std::uint16_t> (c.avoid[q] | (1u << pitchClass (ch.rootPc + chordInterval (ch.mode, ch.degree, t) + 1)));
                after = after || sec.kind == "drop";
            }
            for (const LaneOut& l : song.lanes)
            {
                if (l.kind != LaneKind::chords)
                    continue;
                for (const Note& n : l.score)
                {
                    const auto si = static_cast<std::size_t> (n.section);
                    if (si >= out.size())
                        continue;
                    const Section& sec = song.sections[si];
                    const std::int64_t from = (n.tick - sec.startTick()) / ticksPerStep;
                    const std::int64_t to = (n.tick + std::max<std::int64_t> (1, n.length) - 1 - sec.startTick()) / ticksPerStep;
                    for (std::int64_t q = std::max<std::int64_t> (0, from); q <= to && q < from + 128; ++q)
                    {
                        auto& keys = out[si].chord[static_cast<std::size_t> (q % 128)];
                        keys[static_cast<std::size_t> (n.key / 64)] |= std::uint64_t (1) << (n.key % 64);
                    }
                }
            }
            return out;
        }

        // The texture a line plays (FULL of the new phrase: NEW), if any.
        std::optional<Texture> textureOf (const LineState& st)
        {
            switch (st.form)
            {
                case HookForm::out:
                    return std::nullopt;
                case HookForm::full:
                    return st.motif == 1 ? Texture::newPhrase : Texture::full;
                case HookForm::fragment:
                    return Texture::fragment;
                case HookForm::skeleton:
                    return Texture::skeleton;
                case HookForm::answer:
                    return Texture::answer;
                case HookForm::augment:
                    return Texture::augment;
            }
            return std::nullopt;
        }

        // The texture an op draws of its own (the ones a texture check can bar).
        std::optional<Texture> opTexture (HookOp op)
        {
            switch (op)
            {
                case HookOp::fragment:
                    return Texture::fragment;
                case HookOp::skeleton:
                    return Texture::skeleton;
                case HookOp::answer:
                    return Texture::answer;
                case HookOp::augment:
                    return Texture::augment;
                case HookOp::out:
                case HookOp::repeat:
                case HookOp::full:
                case HookOp::vary:
                case HookOp::cadence:
                case HookOp::breath:
                case HookOp::newPhrase:
                case HookOp::sequence:
                    return std::nullopt;
            }
            return std::nullopt;
        }

        // An op on the line before: REPEAT keeps its texture (not its ending); OUT rests; BREATH rests its
        // last bar (an OUT line stays out); VARY and CADENCE end it, SEQUENCE plays it an octave up (after
        // OUT, all three play FULL); NEW plays the new phrase; FULL, FRAGMENT, SKELETON, ANSWER and AUGMENT
        // play their texture of A.
        LineState applyOp (const LineState& prev, HookOp op)
        {
            LineState st = prev;
            st.ending = HookEnding::none;
            switch (op)
            {
                case HookOp::repeat:
                    return st;
                case HookOp::out:
                    return LineState {};
                case HookOp::breath:
                    if (playing (prev))
                        st.ending = HookEnding::breath;
                    return st;
                case HookOp::vary:
                case HookOp::cadence:
                case HookOp::sequence:
                    if (!playing (prev))
                        st = stateOf (HookForm::full, 0);
                    if (op == HookOp::vary)
                        st.ending = HookEnding::vary;
                    else if (op == HookOp::cadence)
                        st.ending = HookEnding::cadence;
                    else
                        st.shift += 7;
                    return st;
                case HookOp::newPhrase:
                    return stateOf (HookForm::full, 1);
                case HookOp::full:
                    return stateOf (HookForm::full, 0);
                case HookOp::fragment:
                    return stateOf (HookForm::fragment, 0);
                case HookOp::skeleton:
                    return stateOf (HookForm::skeleton, 0);
                case HookOp::answer:
                    return stateOf (HookForm::answer, 0);
                case HookOp::augment:
                    return stateOf (HookForm::augment, 0);
            }
            return st;
        }

        // A line's prints of the material (each texture before the fit and the guard, keyed in the
        // material's mode) as the line rule reads them: VARY's and CADENCE's last note aside (bar 7 is
        // free), BREATH's bar 7 empty.
        LinePrints materialPrints (const SongHook& h, const LineState& st)
        {
            LinePrints out;
            if (!playing (st))
                return out;
            for (std::int32_t b = 0; b < barsPerBlock; ++b)
            {
                if (b == barsPerBlock - 1 && st.ending == HookEnding::breath)
                    continue;
                BarPrint& p = out[static_cast<std::size_t> (b)];
                for (const HookNote& n : textureNotes (h, st.form, st.motif, b))
                    p.push_back (n.step * 128 + h.base + semitones (h.mode, n.degree + st.shift));
                tidy (p);
            }
            return out;
        }

        // ------------------------------------------------------------------ the texture check, realized

        // The textures a section's lines can play: FULL unless the section is withheld, SKELETON and
        // FRAGMENT (a marked line falls back to them), NEW where its menu draws it or in drop 2, ANSWER and
        // AUGMENT where its menu draws them; none where the carrier never plays, nor in a section that
        // continues the one before (all its lines keep one state).
        std::array<bool, textureCount> texturesIn (const SectionCtx& c, const std::array<bool, textureCount>& plays)
        {
            std::array<bool, textureCount> here {};
            if (c.menu == nullptr || c.menu->carry || !c.active)
                return here;
            const auto draws = [&c] (HookOp op) {
                for (const auto* list : { &c.menu->first, &c.menu->next, &c.menu->last })
                    for (const HookOpWeight& o : *list)
                        if (o.op == op && o.weight > 0)
                            return true;
                return false;
            };
            const auto set = [&] (Texture t, bool on) { here[static_cast<std::size_t> (t)] = on && plays[static_cast<std::size_t> (t)]; };
            set (Texture::full, !c.withheld);
            set (Texture::fragment, true);
            set (Texture::skeleton, true);
            set (Texture::answer, draws (HookOp::answer));
            set (Texture::augment, draws (HookOp::augment));
            set (Texture::newPhrase, !c.withheld && (draws (HookOp::newPhrase) || c.sec->kind == "drop2"));
            return here;
        }

        // The texture check on each section's realized lines, the fit, the guard and the pedal in, which
        // can make two textures meet in a bar: every texture the section can play holds a note in every
        // bar, and every two of them meet the line rule.
        std::vector<Failure> realizedFailures (const Setting& s, const std::vector<SectionCtx>& ctxs, const SongHook& h, const std::array<bool, textureCount>& plays)
        {
            std::vector<Failure> out;
            for (const SectionCtx& c : ctxs)
            {
                std::array<bool, textureCount> here = texturesIn (c, plays);
                std::array<LinePrints, textureCount> prints;
                for (std::size_t i = 0; i < textureCount; ++i)
                {
                    if (!here[i])
                        continue;
                    const auto [form, motif] = formOf (textureAt (i));
                    Realized r = realizeLine (s, c, h, stateOf (form, motif));
                    if (!r.ok)
                    {
                        out.push_back ({ textureAt (i), textureAt (i) });
                        here[i] = false;
                    }
                    else
                        prints[i] = std::move (r.prints);
                }
                for (std::size_t i = 0; i < textureCount; ++i)
                    for (std::size_t j = i + 1; j < textureCount; ++j)
                        if (here[i] && here[j] && !onTheLine (prints[i], prints[j]))
                            out.push_back ({ textureAt (i), textureAt (j) });
            }
            return out;
        }

        // The song's material: drawn below kh, its texture check on the material and on each section's
        // realized lines, and up to 8 texture-check redraws below kh.with (tag ("check")).with (tag
        // ("attempt")).with (t), keeping the first that passes, else the one with the fewest broken rules,
        // then the fewest failures; a texture still failing is barred for the song (its menu weight 0).
        struct Material
        {
            SongHook hook;
            std::int32_t broken = 0; // material rules it still breaks (brokenRules)
            std::array<bool, textureCount> barred {}; // textures the line plan may not draw for this song
        };

        Material materialOf (const Setting& s, const std::vector<SectionCtx>& ctxs, rng::Key kh)
        {
            const auto plays = texturesOf (*s.spec);
            const auto failuresOf = [&] (const SongHook& h) {
                std::vector<Failure> f = textureFailures (h, plays);
                for (const Failure& x : realizedFailures (s, ctxs, h, plays))
                    f.push_back (x);
                return f;
            };
            auto [hook, broken] = materialBelow (s, kh);
            std::vector<Failure> failures = failuresOf (hook);
            for (std::int32_t t = 1; t <= checks && (broken > 0 || !failures.empty()); ++t)
            {
                auto [next, nextBroken] = materialBelow (s, kh.with (rng::tag ("check")).with (rng::tag ("attempt")).with (t));
                std::vector<Failure> nextFailures = failuresOf (next);
                if (nextBroken < broken || (nextBroken == broken && nextFailures.size() < failures.size()))
                {
                    hook = std::move (next);
                    broken = nextBroken;
                    failures = std::move (nextFailures);
                }
            }
            Material m;
            m.hook = std::move (hook);
            m.broken = broken;
            for (const Failure& f : failures)
            {
                const auto i = static_cast<std::size_t> (f.texture), j = static_cast<std::size_t> (f.against);
                if (m.barred[i] || m.barred[j])
                    continue;
                const Texture out = keepRank (f.texture) >= keepRank (f.against) ? f.texture : f.against;
                if (keepRank (out) > 1)
                    m.barred[static_cast<std::size_t> (out)] = true;
            }
            return m;
        }

        // ------------------------------------------------------------------ the line plan

        // An op from a menu by weight, a barred texture's weight 0; REPEAT when none can be drawn.
        HookOp pickOp (const std::vector<HookOpWeight>& menu, rng::Key k, const std::array<bool, textureCount>& barred)
        {
            std::vector<std::int32_t> w;
            for (const HookOpWeight& o : menu)
            {
                const auto t = opTexture (o.op);
                w.push_back (t && barred[static_cast<std::size_t> (*t)] ? 0 : o.weight);
            }
            const std::size_t i = rng::pickWeighted (k, w);
            return i < menu.size() ? menu[i].op : HookOp::repeat;
        }

        // The line plan (plan, "Where it plays"), per section in song order: the menu's draws (`enter`'s
        // leading OUT lines, clamped before the first drop so the groove's last line plants the hook;
        // `first` on the carrier's first active line at or after it, `last` on the section's last line,
        // `next` otherwise; an after build's `continue` keeps the section before's last state); the rules
        // in their order (r2 for drop 1 only, r1, drop 2's new phrase, drop 1's FULL line, r3, the cover
        // floor counted on the states the lines will realize, and r3 again for a line the cover made
        // play); then each line realized and checked by the line rule against the line before, on the
        // material and on its realized notes, falling back to REPEAT, then FULL (never in a withheld kind),
        // then OUT, a line a rule makes play to SKELETON, then FRAGMENT, instead. Drop 1's FULL moves to the
        // next line where it can't land; if no later line takes it, the earliest line where it lands does,
        // and the lines after it are realized again. Last, an OUT line clears the carrier's bit in its
        // block and takes the carrier out of a one-bar dropout inside it (a dropout left with no lane goes).
        // Keys: kh.with (tag ("plan")).with (section) with (tag ("enter")), and per line .with (line).with
        // (tag ("op")).
        void planLines (Song& song, const Setting& s, const std::vector<SectionCtx>& ctxs, const Material& m, rng::Key kh)
        {
            SongHook& h = *song.hook;
            const rng::Key kp = kh.with (rng::tag ("plan"));
            const std::uint64_t bit = std::uint64_t (1) << s.lane;
            std::vector<bool> endsActive (song.sections.size(), false), playsAny (song.sections.size(), false);
            LineState carried;
            for (Section& sec : song.sections)
            {
                const std::size_t si = sec.index;
                const SectionCtx& c = ctxs[si];
                const auto lines = static_cast<std::int32_t> (sec.blocks.size());
                if (c.menu == nullptr || lines == 0)
                {
                    // no line plays here, so the carrier doesn't
                    for (std::uint64_t& b : sec.blocks)
                        b &= ~bit;
                    for (Dropout& d : sec.dropouts)
                        d.lanes &= ~bit;
                    std::erase_if (sec.dropouts, [] (const Dropout& d) { return d.lanes == 0; });
                    carried = LineState {};
                    continue;
                }
                const HookMenu& menu = *c.menu;
                const auto L = [] (std::int32_t l) { return static_cast<std::size_t> (l); };
                const auto act = [&] (std::int32_t l) { return (sec.blocks[L (l)] & bit) != 0; };
                const rng::Key ks = kp.with (si);

                // 1. the menu's draws
                std::vector<std::optional<HookOp>> ops (L (lines)); // none: OUT, spending no draw
                std::vector<bool> keep (L (lines), false), must (L (lines), false);
                std::int32_t enter = 0;
                if (!menu.enter.empty())
                {
                    std::vector<std::int32_t> w;
                    for (const Weighted& e : menu.enter)
                        w.push_back (e.weight);
                    const std::size_t pick = rng::pickWeighted (ks.with (rng::tag ("enter")), w);
                    enter = pick < menu.enter.size() ? menu.enter[pick].value : 0;
                    if (sec.kind == "groove" && !c.after)
                        enter = std::min (enter, lines - 1); // the groove's last line plants the hook
                }
                std::int32_t firstLine = -1;
                for (std::int32_t l = 0; l < lines; ++l)
                {
                    if (menu.carry)
                    {
                        keep[L (l)] = playing (carried);
                        if (firstLine < 0 && act (l))
                            firstLine = l;
                        continue;
                    }
                    if (l < enter || !act (l))
                        continue; // an inactive line stays OUT and spends no draw
                    const bool first = firstLine < 0;
                    if (first)
                        firstLine = l;
                    const auto& list = first ? menu.first : (l == lines - 1 && !menu.last.empty() ? menu.last : menu.next);
                    ops[L (l)] = list.empty() ? HookOp::repeat : pickOp (list, ks.with (l).with (rng::tag ("op")), m.barred);
                }
                const auto simulate = [&] {
                    std::vector<LineState> sim (L (lines));
                    LineState st;
                    for (std::int32_t l = 0; l < lines; ++l)
                    {
                        if (!act (l))
                            st = LineState {};
                        else if (keep[L (l)])
                            st = carried;
                        else if (ops[L (l)])
                            st = applyOp (st, *ops[L (l)]);
                        else
                            st = LineState {};
                        sim[L (l)] = st;
                    }
                    return sim;
                };
                const auto set = [&] (std::int32_t l, HookOp op) {
                    ops[L (l)] = op;
                    keep[L (l)] = false;
                };

                // 2. the rules, in their order
                bool forcedOut = false;
                if ((sec.kind == "drop" || sec.kind == "drop2") && si > 0)
                {
                    if (c.firstDrop && si >= 2 && song.sections[si - 1].kind == "build" && !playsAny[si - 1] && song.sections[si - 2].kind == "break" && endsActive[si - 2])
                    {
                        // r2: a break ended with the hook and the build after it had none: drop 1 opens without it
                        set (0, HookOp::out);
                        forcedOut = true;
                    }
                    else if (endsActive[si - 1] && act (0))
                    {
                        // r1: the hook played the last bar into the drop: the drop's first line plays it
                        if (!ops[0] || *ops[0] == HookOp::out)
                            set (0, HookOp::full);
                        keep[0] = false;
                        must[0] = true;
                    }
                }
                if (sec.kind == "drop2" && firstLine >= 0)
                {
                    // drop 2's first playing line is the new phrase, always
                    set (firstLine, HookOp::newPhrase);
                    must[L (firstLine)] = true;
                }
                std::int32_t holdLine = -1;
                const bool holdFull = c.firstDrop && menu.holdFull;
                if (holdFull)
                {
                    // drop 1 holds a FULL line: the first drawn, else the line after the first playing line
                    for (std::int32_t l = 0; l < lines && holdLine < 0; ++l)
                        if (ops[L (l)] && *ops[L (l)] == HookOp::full)
                            holdLine = l;
                    if (holdLine < 0)
                    {
                        std::int32_t p0 = forcedOut ? 1 : 0;
                        while (p0 < lines && (!ops[L (p0)] || *ops[L (p0)] == HookOp::out))
                            ++p0;
                        holdLine = p0 + 1 < lines ? p0 + 1 : std::min (p0, lines - 1);
                        if (forcedOut && holdLine == 0)
                            holdLine = -1;
                        else
                            set (holdLine, HookOp::full);
                    }
                    if (holdLine >= 0)
                        must[L (holdLine)] = true;
                }
                // r3: the line straight after a one-bar pull that takes the carrier inside a playing line plays
                const auto pullsLand = [&] {
                    const auto sim = simulate();
                    for (const Dropout& d : sec.dropouts)
                    {
                        if ((d.lanes & bit) == 0)
                            continue;
                        const std::int32_t own = d.bar / barsPerBlock, land = (d.bar + 1) / barsPerBlock;
                        if (land >= lines || !playing (sim[L (own)]) || !act (land))
                            continue;
                        must[L (land)] = true;
                        if (!playing (sim[L (land)]))
                            set (land, HookOp::repeat);
                    }
                };
                pullsLand();
                // the cover floor, on the states the lines will realize (a REPEAT after OUT doesn't play): the
                // last free OUT line takes FULL (SKELETON in a withheld kind), never r2's line, drop 2's last
                // line last
                const auto need = static_cast<std::int32_t> (menu.cover > 0 ? util::ceilDiv (static_cast<std::int64_t> (menu.cover) * lines, std::int64_t (1000)) : 0);
                for (std::int32_t g = 0; g < lines && need > 0; ++g)
                {
                    const auto sim = simulate();
                    if (std::count_if (sim.begin(), sim.end(), playing) >= need)
                        break;
                    std::int32_t pick = -1;
                    for (std::int32_t pass = 0; pass < 2 && pick < 0; ++pass)
                        for (std::int32_t l = lines - 1; l >= 0 && pick < 0; --l)
                            if (!playing (sim[L (l)]) && act (l) && !(forcedOut && l == 0) && (pass == 1 || !(sec.kind == "drop2" && l == lines - 1)))
                                pick = l;
                    if (pick < 0)
                        break;
                    set (pick, c.withheld ? HookOp::skeleton : HookOp::full);
                    must[L (pick)] = true;
                }
                pullsLand(); // a line the cover made play may hold a pull

                // 3. each line realized and checked by the line rule against the line before
                struct Cached
                {
                    Realized real;
                    LinePrints material;
                };
                std::map<std::tuple<std::uint8_t, std::uint8_t, std::int32_t, std::uint8_t>, Cached> cache;
                const auto realized = [&] (const LineState& st) -> const Cached& {
                    const auto key = std::make_tuple (static_cast<std::uint8_t> (st.form), st.motif, st.shift, static_cast<std::uint8_t> (st.ending));
                    auto it = cache.find (key);
                    if (it == cache.end())
                        it = cache.emplace (key, Cached { realizeLine (s, c, h, st), materialPrints (h, st) }).first;
                    return it->second;
                };
                std::vector<LineState> states (L (lines));
                std::vector<const Cached*> real (L (lines), &realized (LineState {}));
                const auto lands = [&] (std::int32_t l, const Cached& x) {
                    if (l == 0)
                        return true;
                    const Cached& was = *real[L (l - 1)];
                    return onTheLine (was.material, x.material) && onTheLine (was.real.prints, x.real.prints);
                };
                const auto realizeFrom = [&] (std::int32_t from) {
                    for (std::int32_t l = from; l < lines; ++l)
                    {
                        const LineState prev = l > 0 ? states[L (l - 1)] : LineState {};
                        std::vector<LineState> cands;
                        if (act (l))
                        {
                            if (keep[L (l)])
                                cands.push_back (carried);
                            else if (ops[L (l)])
                                cands.push_back (applyOp (prev, *ops[L (l)]));
                            else
                                cands.push_back (LineState {});
                            cands.push_back (applyOp (prev, HookOp::repeat));
                            if (!c.withheld)
                                cands.push_back (stateOf (HookForm::full, 0));
                            if (must[L (l)])
                            {
                                cands.push_back (stateOf (HookForm::skeleton, 0));
                                cands.push_back (stateOf (HookForm::fragment, 0));
                            }
                        }
                        cands.push_back (LineState {});
                        states[L (l)] = LineState {};
                        real[L (l)] = &realized (LineState {});
                        for (const LineState& cand : cands)
                        {
                            const bool plays = playing (cand);
                            if (!plays && must[L (l)] && act (l))
                                continue; // a line a rule makes play never falls back to OUT
                            if (plays && c.withheld && (cand.form == HookForm::full || cand.motif == 1))
                                continue;
                            if (const auto t = textureOf (cand); t && m.barred[static_cast<std::size_t> (*t)])
                                continue;
                            const Cached& x = realized (cand);
                            if ((plays && !x.real.ok) || !lands (l, x))
                                continue;
                            states[L (l)] = cand;
                            real[L (l)] = &x;
                            break;
                        }
                        // drop 1's FULL: where it can't land on its line, the next line takes it
                        if (holdLine == l && !(states[L (l)].form == HookForm::full && states[L (l)].motif == 0))
                        {
                            holdLine = -1;
                            for (std::int32_t next = l + 1; next < lines && holdLine < 0; ++next)
                                if (act (next))
                                {
                                    set (next, HookOp::full);
                                    must[L (next)] = true;
                                    holdLine = next;
                                }
                        }
                    }
                };
                realizeFrom (0);
                if (holdFull && std::none_of (states.begin(), states.end(), [] (const LineState& st) { return st.form == HookForm::full && st.motif == 0; }))
                {
                    // no later line took it: the earliest line where FULL lands does, and the lines after it
                    // are realized again
                    const LineState full = stateOf (HookForm::full, 0);
                    const Cached& x = realized (full);
                    holdLine = -1;
                    for (std::int32_t l = forcedOut ? 1 : 0; l < lines && x.real.ok; ++l)
                        if (act (l) && lands (l, x))
                        {
                            states[L (l)] = full;
                            real[L (l)] = &x;
                            set (l, HookOp::full);
                            must[L (l)] = true;
                            realizeFrom (l + 1);
                            break;
                        }
                }

                // 4. the lines and their notes; an OUT line's block and dropouts lose the carrier
                for (std::int32_t l = 0; l < lines; ++l)
                {
                    const LineState& st = states[L (l)];
                    HookLine line;
                    line.section = si;
                    line.bar = l * barsPerBlock;
                    line.form = st.form;
                    line.motif = st.motif;
                    line.shift = st.shift;
                    line.ending = st.ending;
                    h.lines.push_back (line);
                    if (!playing (st))
                    {
                        sec.blocks[L (l)] &= ~bit;
                        for (Dropout& d : sec.dropouts)
                            if (d.bar / barsPerBlock == l)
                                d.lanes &= ~bit;
                        continue;
                    }
                    playsAny[si] = true;
                    const std::int64_t first = static_cast<std::int64_t> (sec.startBar + line.bar) * stepsPerBar;
                    for (const LineNote& x : real[L (l)]->real.notes)
                    {
                        Note n;
                        n.step = first + x.step;
                        n.tick = n.step * ticksPerStep;
                        n.length = x.length;
                        n.key = static_cast<std::uint8_t> (x.key);
                        n.velocity = x.velocity;
                        n.tags = inMinorKey (song.tonic, pitchClass (x.key)) ? 0 : noteTag::borrowed;
                        n.flags = static_cast<std::uint16_t> (noteFlag::hook | (x.pedal ? noteFlag::pedal : 0));
                        n.section = static_cast<std::int16_t> (si);
                        h.notes.push_back (n);
                    }
                }
                std::erase_if (sec.dropouts, [] (const Dropout& d) { return d.lanes == 0; });
                endsActive[si] = sec.active (s.lane, sec.bars - 1);
                carried = states[L (lines - 1)];
            }
            std::stable_sort (h.notes.begin(), h.notes.end(), [] (const Note& a, const Note& b) { return a.tick < b.tick || (a.tick == b.tick && a.key < b.key); });
        }
    }

    void planHook (Song& song, const Bundle& bundle, rng::Key root)
    {
        if (!bundle.style.hook)
            return;
        const HookSpec& spec = *bundle.style.hook;
        const auto lane = bundle.style.laneIndex (spec.lane);
        const KitLane* kl = bundle.kit.lane (spec.lane);
        if (!lane || kl == nullptr)
            return; // makeBundle refuses such a bundle
        Setting s;
        s.spec = &spec;
        s.lane = *lane;
        s.tonic = song.tonic;
        // the material's mode: the first drop's (the fit table reads its chords)
        s.mode = song.mode;
        for (const Section& sec : song.sections)
            if (sec.kind == "drop")
            {
                s.mode = song.harmony[sec.index].mode;
                break;
            }
        s.fit = fitFor (song, s.mode, spec.pitch);
        s.a = Range { spec.reg.floor >= 0 ? spec.reg.floor : kl->range.lo, kl->range.hi };
        s.b = spec.period.range.value_or (s.a);
        s.pedal = spec.pedal.has_value();
        s.pedalTonic = s.a.lo + util::floorMod (song.tonic - s.a.lo, 12); // the tonic at the register's floor or above (FUT02: D5)
        const std::vector<SectionCtx> ctxs = sectionContexts (song, bundle, s);
        Material m = materialOf (s, ctxs, hookKey (root));
        song.hook = std::move (m.hook);
        planLines (song, s, ctxs, m, hookKey (root));
    }
}
