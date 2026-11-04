#include "gen/lanes.h"

#include "gen/arrange.h"
#include "gen/mutate.h"
#include "gen/rhythm.h"
#include "util/ints.h"
#include "util/sort.h"

#include <algorithm>
#include <cstdint>
#include <map>
#include <string>
#include <tuple>

namespace wubansia::gen
{
    namespace
    {
        std::uint8_t clampVelocity (std::int64_t v) noexcept { return static_cast<std::uint8_t> (std::clamp<std::int64_t> (v, 1, 127)); }
        std::int64_t scaled (std::int64_t v, std::int32_t permille) noexcept { return util::roundDiv (v * permille, std::int64_t (1000)); }

        // The active range holding tick, or nullptr.
        const TickRange* rangeAt (const std::vector<TickRange>& ranges, std::int64_t tick) noexcept
        {
            const auto it = std::upper_bound (ranges.begin(), ranges.end(), tick, [] (std::int64_t t, const TickRange& r) { return t < r.start; });
            if (it == ranges.begin())
                return nullptr;
            const TickRange& r = *std::prev (it);
            return tick < r.end ? &r : nullptr;
        }

        // Shortens notes so that none rings past the next note on its key.
        void trimSameKey (std::vector<Note>& notes)
        {
            std::map<std::uint8_t, std::size_t> last; // key -> index of the latest note on it
            for (std::size_t i = 0; i < notes.size(); ++i)
            {
                const auto it = last.find (notes[i].key);
                if (it != last.end())
                {
                    Note& prev = notes[it->second];
                    if (prev.tick + prev.length > notes[i].tick)
                        prev.length = std::max<std::int64_t> (1, notes[i].tick - prev.tick);
                }
                last[notes[i].key] = i;
            }
        }

        std::size_t voiceIndex (const DrumLane& lane, std::string_view id)
        {
            for (std::size_t i = 0; i < lane.voices.size(); ++i)
                if (lane.voices[i].id == id)
                    return i;
            return lane.voices.size();
        }

        // The pitch classes the chord lanes voice over each chord span of song.chords, as 12-bit masks
        // (bit pc): every chord-lane score note sounding in the span. 0 where they're silent.
        std::vector<std::uint32_t> voicedMasks (const Song& song)
        {
            std::vector<std::uint32_t> masks (song.chords.size(), 0);
            for (const LaneOut& lane : song.lanes)
            {
                if (lane.kind != LaneKind::chords)
                    continue;
                for (const Note& n : lane.score)
                {
                    // the spans it sounds in, from the one holding its onset (the spans are in time order)
                    auto it = std::upper_bound (song.chords.begin(), song.chords.end(), n.tick, [] (std::int64_t t, const ChordSpan& c) { return t < c.start; });
                    if (it != song.chords.begin())
                        --it;
                    for (; it != song.chords.end() && it->start < n.tick + n.length; ++it)
                        if (it->end > n.tick)
                            masks[static_cast<std::size_t> (it - song.chords.begin())] |= std::uint32_t (1) << pitchClass (n.key);
                }
            }
            return masks;
        }

        // True if pitch class pc agrees with what the chord lanes voice over chord c (voiced, a
        // pitch-class mask; 0 where they're silent): it is voiced itself, or it sits no semitone under
        // a voiced tone (a minor 9th over the bass) and is no other third, seventh or fifth of c than
        // a voiced one.
        bool agreesWith (std::int32_t pc, const ChordSpan& c, std::uint32_t voiced) noexcept
        {
            const std::int32_t iv = pitchClass (pc - c.rootPc);
            const auto on = [&] (std::int32_t x) { return ((voiced >> pitchClass (c.rootPc + x)) & 1u) != 0; };
            if (voiced == 0 || on (iv))
                return true;
            if (on (iv + 1))
                return false;
            if ((iv == 3 || iv == 4) && on (7 - iv))
                return false;
            if ((iv == 10 || iv == 11) && on (21 - iv))
                return false;
            return iv != 7 || !(on (6) || on (8));
        }

        // The bass's note for a chord degree: the root nearest the lane's own root (the tonic in the
        // bass register), then the chord tone above it, folded into the range.
        // - The seventh is the chord's minor 7th, or the fifth where the mode's 7th is major: a major
        //   7th in a sub or rolling bass rubs a minor 9th against the root voiced above it.
        // - The third, seventh and fifth agree with the pitch classes the chord lanes voice over the
        //   chord (voiced; 0 where they're silent): a tone they don't voice may not sit a semitone under
        //   one they do (a minor 9th over the bass), nor be another third, seventh or fifth than theirs,
        //   as under a planed shape, a sus or a quartal stack, which don't voice the mode's own chord
        //   (a planed C-Eb-G-Bb over the IV of Dorian, C-E-G). Where it would, the bass takes the
        //   fifth, and where that disagrees too, the root.
        std::int32_t bassKey (BassDegree d, const ChordSpan& c, const KitLane& lane, std::uint32_t voiced)
        {
            std::int32_t root = -1;
            for (std::int32_t k = lane.range.lo; k <= lane.range.hi; ++k)
                if (pitchClass (k) == c.rootPc && (root < 0 || std::abs (k - lane.root) < std::abs (root - lane.root)))
                    root = k;
            if (root < 0)
                root = lane.root;
            const auto agrees = [&] (std::int32_t iv) { return agreesWith (c.rootPc + iv, c, voiced); };
            const std::int32_t fifth = chordInterval (c.mode, c.degree, 4);
            std::int32_t iv = 0;
            switch (d)
            {
                case BassDegree::root:
                    break;
                case BassDegree::octave:
                    iv = 12;
                    break;
                case BassDegree::fifth:
                    iv = fifth;
                    break;
                case BassDegree::seventh:
                    iv = chordInterval (c.mode, c.degree, 6) == 10 ? 10 : fifth;
                    break;
                case BassDegree::third:
                    iv = chordInterval (c.mode, c.degree, 2);
                    break;
            }
            if (iv != 0 && iv != 12 && !agrees (iv))
                iv = agrees (fifth) ? fifth : 0;
            std::int32_t key = root + iv;
            while (key > lane.range.hi)
                key -= 12;
            while (key < lane.range.lo)
                key += 12;
            return key;
        }

        std::uint8_t keyTags (std::int32_t tonic, std::int32_t key) noexcept { return inMinorKey (tonic, pitchClass (key)) ? 0 : noteTag::borrowed; }

        // A scale step from key, down (dir -1) or up (+1), in the mode on the tonic: 1 or 2 semitones.
        std::int32_t scaleStep (std::int32_t key, std::int32_t dir, Mode mode, std::int32_t tonic) noexcept
        {
            const auto steps = modeSteps (mode);
            for (std::int32_t d = 1; d <= 2; ++d)
            {
                const std::int32_t k = key + dir * d;
                const std::int32_t rel = pitchClass (k - tonic);
                if (std::find (steps.begin(), steps.end(), rel) != steps.end())
                    return k;
            }
            return key + dir;
        }

        bool isMinorChord (Mode mode, std::int32_t degree) noexcept { return chordInterval (mode, degree, 2) == 3; }

        // Where a tick sits in its section's 8-bar phrase. Decisions keyed by it repeat phrase by
        // phrase, so a pitched lane's bars change on phrase lines (plan §2.3's hypermeter tiers, G4).
        std::int64_t phrasePosition (const Song& song, std::int64_t tick) noexcept
        {
            for (const Section& s : song.sections)
                if (tick >= s.startTick() && tick < s.endTick())
                    return util::floorMod (tick - s.startTick(), std::int64_t (barsPerBlock) * ticksPerBar);
            return tick;
        }

        // Ornaments of pitched lanes fall only in the last bar of every `every` bars.
        bool ornamentBar (std::int32_t barInSection, std::int32_t every) noexcept { return every <= 1 || (barInSection + 1) % every == 0; }

        bool isStack (const VoicingSpec& v) noexcept
        {
            const std::size_t tones = v.shape.empty() ? v.degrees.size() : v.shape.size();
            return v.choose == 0 ? tones >= 2 : v.choose >= 2;
        }

        std::int32_t arcTarget (std::string_view arc, Range top, std::int32_t k, std::int32_t n) noexcept
        {
            const std::int32_t span = top.hi - top.lo;
            const std::int32_t last = std::max (1, n - 1);
            if (arc == "rise")
                return top.lo + span * k / last;
            if (arc == "fall")
                return top.hi - span * k / last;
            if (arc == "arch")
                return n <= 2 ? top.lo + span * k / last : top.lo + span * (n - 1 - std::abs (2 * k - (n - 1))) / (n - 1);
            return (top.lo + top.hi) / 2;
        }
    }

    void sortNotes (std::vector<Note>& notes)
    {
        util::stableSortBy (notes, [] (const Note& n) { return std::make_tuple (n.tick, n.key, n.voice, n.length, n.velocity); });
    }

    // ------------------------------------------------------------------ drums

    void scoreDrums (Song& song, const Bundle& bundle, std::size_t li, rng::Key root)
    {
        const Style& style = bundle.style;
        const LaneSpec& lane = style.lanes[li];
        const DrumLane& d = lane.drums;
        const KitLane& kit = *bundle.kit.lane (lane.id);
        LaneOut& out = song.lanes[li];
        out.score.clear();
        const rng::Key kl = root.with (rng::tag ("lane")).with (rng::tag (lane.id));
        const auto ranges = activeRanges (song, li);

        std::vector<std::uint8_t> noteOf;
        for (const auto& v : d.voices)
            for (const auto& n : kit.notes)
                if (n.voice == v.id)
                    noteOf.push_back (n.note);

        const auto gridNamed = [&d] (std::string_view name) -> const DrumGrid& {
            return *std::find_if (d.grids.begin(), d.grids.end(), [&] (const DrumGrid& g) { return g.name == name; });
        };
        std::map<std::pair<std::string, std::int32_t>, Pattern> patterns;
        for (const Section& s : song.sections)
        {
            const std::string_view useName = lane.gridFor (s.kind);
            if (useName.empty())
                continue;
            const DrumGrid& use = gridNamed (useName);
            const std::int32_t velScale = style.section (s.kind)->vel;
            for (std::int32_t bar = 0; bar < s.bars; ++bar)
            {
                if (!s.active (li, bar))
                    continue;
                // the grid this bar plays: the section's, or with stages the one for its part of the
                // section (on the planned length, so a --bars cut is still a prefix of the whole song)
                const auto stageAt = [&] { return static_cast<std::size_t> (static_cast<std::int64_t> (bar) * static_cast<std::int64_t> (use.stages.size()) / s.plannedBars); };
                const DrumGrid& grid = use.stages.empty() ? use : gridNamed (use.stages[stageAt()]);
                const std::string_view gridName = grid.name;
                // and the section grid's crescendo, permille, from its first bar to its last planned one
                const std::int32_t ramp = !use.ramp ? 1000
                                                    : static_cast<std::int32_t> (use.ramp->lo + static_cast<std::int64_t> (use.ramp->hi - use.ramp->lo) * bar / std::max (1, s.plannedBars - 1));
                const std::int32_t variant = variantAt (song, style, li, s.index, bar);
                const auto key = std::make_pair (std::string (gridName), variant);
                auto it = patterns.find (key);
                if (it == patterns.end())
                    it = patterns.emplace (key, drawPattern (grid, kl.with (rng::tag ("pattern")).with (rng::tag (gridName)).with (variant), style.attempts))
                             .first;
                const Pattern& pattern = it->second;
                const std::int32_t absBar = s.startBar + bar;
                const bool turn = isTurnBar (song, absBar);
                const std::int64_t barStep = static_cast<std::int64_t> (absBar) * stepsPerBar;
                std::vector<Note> notes;
                for (std::size_t gi = 0; gi < grid.voices.size(); ++gi)
                {
                    const DrumVoiceGrid& v = grid.voices[gi];
                    const std::size_t vi = voiceIndex (d, v.voice);
                    const rng::Key kv = kl.with (rng::tag (v.voice));
                    for (std::int32_t j = 0; j < 16; ++j)
                    {
                        const auto cell = static_cast<std::size_t> ((bar % 2) * 16 + j);
                        const std::int64_t step = barStep + j;
                        const rng::Key kb = kv.with (s.index).with (bar).with (j);
                        bool hit = false;
                        std::uint16_t flags = 0;
                        std::int32_t vel = 0;
                        if (v.hasCells)
                        {
                            hit = ((pattern.hits[gi] >> cell) & 1u) != 0;
                            const bool anchor = ((pattern.anchors[gi] >> cell) & 1u) != 0;
                            if (hit && !anchor && v.skip > 0 && rng::chance (kb.with (rng::tag ("skip")), p16FromPercent (v.skip)))
                                hit = false;
                            if (hit && !anchor && turn && v.mute[static_cast<std::size_t> (j)] > 0
                                && rng::chance (kb.with (rng::tag ("mute")), p16FromPercent (v.mute[static_cast<std::size_t> (j)])))
                                hit = false;
                            if (hit)
                            {
                                vel = v.vel[cell] + rng::between (kb.with (rng::tag ("vel")), -v.spread, v.spread);
                                flags = anchor ? noteFlag::anchor : 0;
                            }
                            else if (v.orn[cell] > 0 && !((pattern.hits[gi] >> cell) & 1u)
                                     && rng::chance (kb.with (rng::tag ("orn")), p16FromPercent (v.orn[cell])))
                            {
                                hit = true;
                                flags = noteFlag::ornament;
                                vel = rng::between (kb.with (rng::tag ("ornvel")), v.ornVel.lo, v.ornVel.hi);
                            }
                            else if (turn && v.turn[static_cast<std::size_t> (j)] > 0
                                     && rng::chance (kb.with (rng::tag ("turn")), p16FromPercent (v.turn[static_cast<std::size_t> (j)])))
                            {
                                hit = true;
                                flags = noteFlag::turn;
                                vel = rng::between (kb.with (rng::tag ("turnvel")), v.turnVel.lo, v.turnVel.hi);
                            }
                        }
                        else if (v.euclid)
                        {
                            hit = ((pattern.hits[gi] >> cell) & 1u) != 0;
                            if (hit)
                            {
                                const auto n = static_cast<std::size_t> (v.euclid->n);
                                bool first = true;
                                for (std::size_t c = (cell / n) * n; c < cell; ++c)
                                    first = first && ((pattern.hits[gi] >> c) & 1u) == 0;
                                // the hit's velocity belongs to its cell, per variant like the hits: the
                                // cycle's accent shape repeats (the feel's 1/f velocity adds the life)
                                const rng::Key kc = kv.with (rng::tag (gridName)).with (variant).with (static_cast<std::int64_t> (cell));
                                vel = rng::between (kc.with (rng::tag ("evel")), v.euclid->vel.lo, v.euclid->vel.hi) + (first ? v.euclid->accent : 0);
                                flags = noteFlag::euclid;
                            }
                        }
                        else if (v.poly)
                        {
                            const PolyCells& c = *v.poly;
                            const std::int64_t pos = util::floorMod (step, static_cast<std::int64_t> (c.resetBars) * stepsPerBar);
                            const rng::Key kp = kv.with (rng::tag ("poly")).with (rng::tag (gridName)).with (variant);
                            const std::int32_t phase = rng::between (kp.with (rng::tag ("phase")), c.phase.lo, c.phase.hi);
                            if (pos >= phase && (pos - phase) % c.every == 0
                                && (c.p >= 100 || rng::chance (kp.with (rng::tag ("keep")).with (pos), p16FromPercent (c.p))))
                            {
                                hit = true;
                                flags = noteFlag::poly;
                                // per position in the reset cycle and variant, like the keep: the accents repeat
                                vel = rng::between (kp.with (rng::tag ("pvel")).with (pos), c.vel.lo, c.vel.hi) + (pos - phase < c.every ? c.accent : 0);
                            }
                        }
                        if (!hit)
                            continue;
                        Note n;
                        n.step = step;
                        n.tick = step * ticksPerStep;
                        n.length = d.voices[vi].length;
                        n.key = noteOf[vi];
                        n.velocity = clampVelocity (scaled (scaled (vel, velScale), ramp));
                        n.voice = static_cast<std::uint8_t> (vi);
                        n.flags = flags;
                        n.section = static_cast<std::int16_t> (s.index);
                        notes.push_back (n);
                    }
                }
                // the exclusive rule: the winner's step silences the loser there
                for (const auto& [winner, loser] : d.exclusive)
                {
                    const std::size_t w = voiceIndex (d, winner), l = voiceIndex (d, loser);
                    std::erase_if (notes, [&] (const Note& n) {
                        return n.voice == l && std::any_of (notes.begin(), notes.end(), [&] (const Note& o) { return o.voice == w && o.step == n.step; });
                    });
                }
                // one note per key and step (two voices may share a key): the loudest, then the first voice
                sortNotes (notes);
                std::vector<Note> kept;
                for (const Note& n : notes)
                {
                    auto dup = std::find_if (kept.begin(), kept.end(), [&] (const Note& k) { return k.step == n.step && k.key == n.key; });
                    if (dup == kept.end())
                        kept.push_back (n);
                    else if (n.velocity > dup->velocity)
                        *dup = n;
                }
                out.score.insert (out.score.end(), kept.begin(), kept.end());
            }
        }
        sortNotes (out.score);
        for (Note& n : out.score)
        {
            const TickRange* r = rangeAt (ranges, n.tick);
            n.length = std::max<std::int64_t> (1, std::min (n.length, r != nullptr ? r->end - n.tick : n.length));
        }
        // the choke: an open hat ends where a closed one starts
        for (const auto& rule : d.chokes)
        {
            const std::size_t cv = voiceIndex (d, rule.voice);
            std::vector<std::size_t> by;
            for (const auto& b : rule.by)
                by.push_back (voiceIndex (d, b));
            for (std::size_t i = 0; i < out.score.size(); ++i)
            {
                Note& n = out.score[i];
                if (n.voice != cv)
                    continue;
                for (std::size_t k = i + 1; k < out.score.size() && out.score[k].tick < n.tick + n.length; ++k)
                    if (out.score[k].tick > n.tick && std::find (by.begin(), by.end(), out.score[k].voice) != by.end())
                    {
                        n.length = out.score[k].tick - n.tick;
                        break;
                    }
            }
        }
        trimSameKey (out.score);
    }

    // ------------------------------------------------------------------ bass

    void scoreBass (Song& song, const Bundle& bundle, std::size_t li, rng::Key root)
    {
        const Style& style = bundle.style;
        const LaneSpec& lane = style.lanes[li];
        const BassLane& b = lane.bass;
        const KitLane& kit = *bundle.kit.lane (lane.id);
        LaneOut& out = song.lanes[li];
        out.score.clear();
        const rng::Key kb = root.with (rng::tag ("lane")).with (rng::tag (lane.id));
        const auto ranges = activeRanges (song, li);

        // what the chord lanes voice over each chord (they are scored first: generator.cpp)
        const auto voiced = voicedMasks (song);
        const auto voicedAt = [&] (const ChordSpan& c) { return voiced[static_cast<std::size_t> (&c - song.chords.data())]; };

        // the steps where the follow lane plays (its score)
        const std::int64_t steps = static_cast<std::int64_t> (song.totalBars) * stepsPerBar;
        std::vector<std::uint8_t> kickAt (static_cast<std::size_t> (steps), 0);
        if (!b.follow.empty())
            for (const Note& n : song.lanes[*style.laneIndex (b.follow)].score)
                if (n.step >= 0 && n.step < steps && !(n.flags & (noteFlag::ornament | noteFlag::turn)))
                    kickAt[static_cast<std::size_t> (n.step)] = 1; // the pattern's kicks: not its per-bar ornaments or turnarounds

        struct Line
        {
            Note note;
            bool hold = false;
            std::int64_t fixed = 0;
        };
        std::vector<Line> line;
        for (const Section& s : song.sections)
        {
            const std::string_view gridName = lane.gridFor (s.kind);
            if (gridName.empty())
                continue;
            const BassGrid& grid = *std::find_if (b.grids.begin(), b.grids.end(), [&] (const BassGrid& g) { return g.name == gridName; });
            const std::int32_t velScale = style.section (s.kind)->vel;
            for (std::int32_t bar = 0; bar < s.bars; ++bar)
            {
                if (!s.active (li, bar))
                    continue;
                const std::int32_t variant = variantAt (song, style, li, s.index, bar);
                // each cell draws from its variant: the bar's own, or with "mutate" the last that changed it (mutate.h)
                const auto cellVariant = cellVariants (grid, b, kb.with (rng::tag (gridName)), variant);
                const std::int64_t barStep = static_cast<std::int64_t> (s.startBar + bar) * stepsPerBar;
                for (std::int32_t j = 0; j < 16; ++j)
                {
                    const auto cell = static_cast<std::size_t> ((bar % 2) * 16 + j);
                    const rng::Key kv = kb.with (rng::tag (gridName)).with (cellVariant[cell]);
                    const std::size_t dc = drawCell (grid, cell); // the cell itself, or with "mutate" its 16th of the first bar
                    const std::int64_t step = barStep + j;
                    const rng::Key kbar = kb.with (s.index).with (bar).with (j);
                    bool on = false, ornament = false;
                    std::uint16_t flags = 0;
                    if (kickAt[static_cast<std::size_t> (step)] != 0)
                    {
                        // the sub follows the kick: one choice per step and variant, so the motif holds
                        if (grid.follow[cell] > 0 && rng::chance (kv.with (rng::tag ("follow")).with (dc), p16FromPercent (grid.follow[cell])))
                        {
                            on = true;
                            flags = noteFlag::follow;
                        }
                    }
                    else if (grid.p[cell] >= 100 || (grid.p[cell] > 0 && rng::chance (kv.with (rng::tag ("p")).with (dc), p16FromPercent (grid.p[cell]))))
                        on = true;
                    else if (grid.orn[cell] > 0 && ornamentBar (bar, grid.ornBars) && rng::chance (kbar.with (rng::tag ("orn")), p16FromPercent (grid.orn[cell])))
                    {
                        on = true;
                        ornament = true;
                        flags = noteFlag::ornament;
                    }
                    if (!on)
                        continue;
                    const std::int64_t tick = step * ticksPerStep;
                    const ChordSpan* chord = chordAt (song, tick);
                    if (chord == nullptr)
                        continue;
                    const auto& weights = b.degreeSets[static_cast<std::size_t> (ornament ? grid.ornDeg : grid.deg[cell])];
                    const auto degree = static_cast<BassDegree> (std::min<std::size_t> (
                        rng::pickWeighted (ornament ? kbar.with (rng::tag ("deg")) : kv.with (rng::tag ("deg")).with (dc), weights), bassDegreeCount - 1));
                    Line l;
                    l.note.step = step;
                    l.note.tick = tick;
                    l.note.key = static_cast<std::uint8_t> (bassKey (degree, *chord, kit, voicedAt (*chord)));
                    l.note.tags = keyTags (song.tonic, l.note.key);
                    const std::int32_t vel = ornament ? rng::between (kbar.with (rng::tag ("vel")), grid.ornVel.lo, grid.ornVel.hi)
                                                      : grid.vel[cell] + rng::between (kbar.with (rng::tag ("vel")), -grid.spread, grid.spread);
                    l.note.velocity = clampVelocity (scaled (vel, velScale));
                    l.note.flags = flags;
                    l.note.section = static_cast<std::int16_t> (s.index);
                    l.hold = !ornament && grid.len[cell] == 0;
                    l.fixed = ornament ? 90 : grid.len[cell];
                    line.push_back (l);
                }
            }
        }

        // lengths: a held note runs to the next one (across the skipped beat), less the gap
        const auto endOf = [&] (std::size_t i) {
            const TickRange* r = rangeAt (ranges, line[i].note.tick);
            const std::int64_t rangeEnd = r != nullptr ? r->end : line[i].note.tick + 120;
            return (i + 1 < line.size() && line[i + 1].note.tick < rangeEnd) ? line[i + 1].note.tick : rangeEnd;
        };
        for (std::size_t i = 0; i < line.size(); ++i)
        {
            Note& n = line[i].note;
            const std::int64_t next = endOf (i);
            // the gap is before a next note in the note's own range only: one starting on the range's
            // end (the next section's first, on its line) isn't there in a --bars cut at that line
            const TickRange* own = rangeAt (ranges, n.tick);
            const bool toNext = i + 1 < line.size() && line[i + 1].note.tick == next && own != nullptr && next < own->end;
            if (line[i].hold)
                n.length = std::min<std::int64_t> (next - n.tick - (toNext ? b.holdGap : 0), b.holdMax);
            else
                n.length = std::min<std::int64_t> (line[i].fixed, next - n.tick - (toNext ? 10 : 0));
            n.length = std::max<std::int64_t> (1, n.length);
        }

        // A note crossing a chord change: a held one crossing by more than `restrike` is struck again
        // on it, on the new root; any other is kept over the change, unless its pitch disagrees with
        // what the chords voice there, when it ends at the change instead (a sub held on into a chord
        // it would rub against).
        std::vector<Note> notes;
        for (std::size_t i = 0; i < line.size(); ++i)
        {
            Note n = line[i].note;
            for (std::size_t guard = 0; guard < 8; ++guard)
            {
                const ChordSpan* here = chordAt (song, n.tick);
                if (here == nullptr || here->end >= n.tick + n.length)
                    break;
                const ChordSpan* next = chordAt (song, here->end);
                if (next == nullptr)
                    break;
                if (!line[i].hold || n.tick + n.length - here->end <= b.restrike)
                {
                    if (!agreesWith (n.key, *next, voicedAt (*next)))
                        n.length = std::max<std::int64_t> (1, here->end - n.tick - b.holdGap);
                    break;
                }
                Note again = n;
                again.tick = here->end;
                again.step = again.tick / ticksPerStep;
                again.length = n.tick + n.length - again.tick;
                again.key = static_cast<std::uint8_t> (bassKey (BassDegree::root, *next, kit, voicedAt (*next)));
                again.tags = keyTags (song.tonic, again.key);
                again.velocity = clampVelocity (static_cast<std::int64_t> (n.velocity) - 6);
                again.flags = static_cast<std::uint16_t> (next->pushedBy > 0 ? noteFlag::pushed : 0);
                n.length = std::max<std::int64_t> (1, again.tick - n.tick - b.holdGap);
                notes.push_back (n);
                n = again;
            }
            notes.push_back (n);
        }

        // approach notes: a 16th or an 8th before the first note on a chord change, a scale step from it
        for (const ChordSpan& c : song.chords)
        {
            if (c.start == song.sections[static_cast<std::size_t> (c.section)].startTick())
                continue; // a section's first chord isn't a change
            // the note that lands on the change: on the pushed start, or on the line it was pushed from
            const auto target = std::find_if (notes.begin(), notes.end(), [&] (const Note& n) {
                return n.tick == c.start || (c.pushedBy > 0 && n.tick == c.start + c.pushedBy);
            });
            if (target == notes.end() || (target->flags & noteFlag::approach))
                continue;
            const Section& cs = song.sections[static_cast<std::size_t> (c.section)];
            const auto targetBar = static_cast<std::int32_t> (target->tick / ticksPerBar - cs.startBar);
            const rng::Key ka = kb.with (rng::tag ("approach")).with (cs.index).with (variantAt (song, style, li, cs.index, targetBar)).with (phrasePosition (song, c.start));
            if (!rng::chance (ka, p16FromPercent (b.approach)))
                continue;
            std::vector<std::int32_t> byW;
            for (const auto& w : b.approachBy)
                byW.push_back (w.weight);
            const auto pick = rng::pickWeighted (ka.with (rng::tag ("by")), byW);
            const std::int64_t by = b.approachBy[pick < b.approachBy.size() ? pick : 0].value;
            const std::int64_t at = target->tick - by;
            const TickRange* r = rangeAt (ranges, target->tick);
            if (r == nullptr || at < r->start)
                continue;
            const auto prev = std::find_if (notes.rbegin(), notes.rend(), [&] (const Note& n) { return n.tick < target->tick; });
            if (prev != notes.rend() && prev->tick >= at - 30)
                continue; // the slot is taken
            const ChordSpan* chord = chordAt (song, target->tick);
            const bool below = rng::chance (ka.with (rng::tag ("below")), p16FromPercent (b.approachBelow));
            std::int32_t key = scaleStep (target->key, below ? -1 : 1, chord->mode, song.tonic);
            if (key < kit.range.lo || key > kit.range.hi)
                key = scaleStep (target->key, below ? 1 : -1, chord->mode, song.tonic);
            Note a;
            a.tick = at;
            a.step = at / ticksPerStep;
            a.length = std::max<std::int64_t> (1, by - b.holdGap);
            a.key = static_cast<std::uint8_t> (key);
            a.tags = keyTags (song.tonic, key);
            a.velocity = static_cast<std::uint8_t> (rng::between (ka.with (rng::tag ("vel")), b.approachVel.lo, b.approachVel.hi));
            a.flags = noteFlag::approach;
            a.section = target->section;
            if (prev != notes.rend() && prev->tick >= r->start && prev->tick + prev->length > at - b.holdGap)
                prev->length = std::max<std::int64_t> (1, at - prev->tick - b.holdGap); // (a note before the range already ends by its start)
            notes.insert (target, a);
        }

        // slides: a near-legato pair a step or a jump apart may glide; the first note overlaps the next
        for (std::size_t i = 0; i + 1 < notes.size(); ++i)
        {
            Note& a = notes[i];
            const Note& n = notes[i + 1];
            const TickRange* r = rangeAt (ranges, a.tick);
            if (r == nullptr || n.tick >= r->end || a.key == n.key || std::abs (int (a.key) - int (n.key)) > b.slideMaxJump)
                continue;
            if (n.tick - (a.tick + a.length) > 120)
                continue;
            const Section& as = song.sections[static_cast<std::size_t> (a.section)];
            const auto aBar = static_cast<std::int32_t> (a.tick / ticksPerBar - as.startBar);
            const std::string_view aGridName = lane.gridFor (as.kind);
            const BassGrid& ag = *std::find_if (b.grids.begin(), b.grids.end(), [&] (const BassGrid& g) { return g.name == aGridName; });
            if (ag.hasSlide)
            {
                // a grid's per-step slides belong to its pattern: drawn per cell and variant, like the cell
                const auto cell = static_cast<std::size_t> ((aBar % 2) * 16 + a.step % stepsPerBar);
                const rng::Key kg = kb.with (rng::tag (aGridName));
                const auto cv = cellVariants (ag, b, kg, variantAt (song, style, li, as.index, aBar))[cell];
                if (!rng::chance (kg.with (cv).with (rng::tag ("slide")).with (drawCell (ag, cell)), p16FromPercent (ag.slide[cell])))
                    continue;
            }
            else if (!rng::chance (kb.with (rng::tag ("slide")).with (as.index).with (variantAt (song, style, li, as.index, aBar)).with (phrasePosition (song, a.tick)),
                         p16FromPercent (b.slide)))
                continue;
            a.length = std::min<std::int64_t> (n.tick - a.tick + b.slideOverlap, n.tick + n.length - 1 - a.tick);
            a.flags = static_cast<std::uint16_t> (a.flags | noteFlag::slide);
        }
        sortNotes (notes);
        out.score = std::move (notes);
    }

    // ------------------------------------------------------------------ chords

    void scoreChords (Song& song, const Bundle& bundle, std::size_t li, rng::Key root)
    {
        const Style& style = bundle.style;
        const HarmonySpec& spec = style.harmony;
        const LaneSpec& lane = style.lanes[li];
        const ChordLane& cl = lane.chords;
        const KitLane& kit = *bundle.kit.lane (lane.id);
        LaneOut& out = song.lanes[li];
        out.score.clear();
        const rng::Key kc = root.with (rng::tag ("lane")).with (rng::tag (lane.id));
        const auto ranges = activeRanges (song, li);

        std::map<std::tuple<std::int32_t, Mode, std::string>, std::vector<Voiced>> cache;
        const auto candidates = [&] (const VoicingSpec& vs, Mode mode, std::int32_t degree) -> const std::vector<Voiced>& {
            const auto key = std::make_tuple (degree, mode, vs.name);
            auto it = cache.find (key);
            if (it == cache.end())
                it = cache.emplace (key, voicingCandidates (vs, song.tonic, mode, degree, kit.range, spec.mud)).first;
            return it->second;
        };
        Voiced prev;
        bool havePrev = false;
        std::map<std::pair<std::string, std::int32_t>, StepMask> stabMasks; // (grid, variant) -> its pattern

        for (const Section& s : song.sections)
        {
            const std::vector<std::string>& palette = *spec.paletteFor (s.kind);
            // The texture of each variant. The variant the lane enters the section on takes a chord
            // stack, so a section opens on a chord (single-note riffs come later, as developments).
            std::int32_t entryBar = 0;
            while (entryBar < s.bars && !s.active (li, entryBar))
                ++entryBar;
            const std::int32_t entryVariant = variantAt (song, style, li, s.index, std::min (entryBar, s.bars - 1));
            std::vector<std::size_t> stacks;
            for (std::size_t k = 0; k < palette.size(); ++k)
                if (isStack (*spec.voicing (palette[k])))
                    stacks.push_back (k);
            const auto texture = [&] (std::int32_t variant) -> std::size_t {
                const rng::Key kt = kc.with (rng::tag ("texture")).with (s.index).with (variant);
                if (variant == entryVariant && !stacks.empty())
                    return stacks[rng::pickIndex (kt, stacks.size())];
                return rng::pickIndex (kt, palette.size());
            };
            song.harmony[s.index].voicing = palette[texture (entryVariant)];
            const std::string_view gridName = lane.gridFor (s.kind);
            if (gridName.empty())
                continue;
            const ChordGrid& grid = *std::find_if (cl.grids.begin(), cl.grids.end(), [&] (const ChordGrid& g) { return g.name == gridName; });
            const std::int32_t velScale = style.section (s.kind)->vel;
            const std::string& arc = spec.arcs[rng::pickIndex (kc.with (rng::tag ("arc")).with (s.index), spec.arcs.size())];
            const auto chordCount = static_cast<std::int32_t> (song.harmony[s.index].degrees.size());
            std::vector<std::pair<std::int32_t, std::string>> voiced; // (degree, voicing) already played in this phrase
            std::int64_t voicedPhrase = -1;

            for (const ChordSpan& c : song.chords)
            {
                if (static_cast<std::size_t> (c.section) != s.index)
                    continue;
                for (const TickRange& r : ranges)
                {
                    const std::int64_t a = std::max (c.start, r.start), e = std::min (c.end, r.end);
                    if (e - a < (grid.pad ? 480 : 240))
                        continue; // only a sliver of the chord in this range: at a lane's entry or exit
                    const std::int32_t barOf = static_cast<std::int32_t> (std::clamp<std::int64_t> ((c.start + c.pushedBy - s.startTick()) / ticksPerBar, 0, s.bars - 1));
                    const std::int32_t variant = variantAt (song, style, li, s.index, barOf);
                    // the texture, or the next palette entry that suits the chord
                    const std::size_t t0 = texture (variant);
                    const VoicingSpec* vs = nullptr;
                    const std::vector<Voiced>* cands = nullptr;
                    for (std::size_t k = 0; k < palette.size() && vs == nullptr; ++k)
                    {
                        const VoicingSpec* v = spec.voicing (palette[(t0 + k) % palette.size()]);
                        if (v->quality == 1 && !isMinorChord (c.mode, c.degree))
                            continue;
                        if (variant == entryVariant && !isStack (*v) && !stacks.empty())
                            continue;
                        const auto& list = candidates (*v, c.mode, c.degree);
                        if (!list.empty())
                        {
                            vs = v;
                            cands = &list;
                        }
                    }
                    if (vs == nullptr)
                        continue;
                    const std::int32_t target = arcTarget (arc, vs->top, c.index, chordCount);
                    // the chord's phrase: where it sounds from, or with the lane's "fresh" where it is written
                    // (a change pushed early belongs to the phrase it pushes into)
                    const std::int64_t phraseIndex = ((cl.fresh ? c.start + c.pushedBy : a) - s.startTick()) / (std::int64_t (barsPerBlock) * ticksPerBar);
                    // the voicing it leads from; with "fresh", none at a phrase's first chord, so every phrase
                    // is voiced alike
                    const Voiced* lead = (havePrev && !(cl.fresh && phraseIndex != voicedPhrase)) ? &prev : nullptr;
                    // by voice leading, the arc and the register, the top voice inside its window
                    std::size_t best = bestVoicing (*cands, lead, target, *vs);
                    // a phrase starts afresh, so its voicings repeat phrase by phrase
                    if (phraseIndex != voicedPhrase)
                    {
                        voiced.clear();
                        voicedPhrase = phraseIndex;
                    }
                    const auto again = std::make_pair (c.degree, vs->name);
                    if (std::find (voiced.begin(), voiced.end(), again) != voiced.end() && cands->size() > 1
                        && rng::chance (kc.with (rng::tag ("revoice")).with (s.index).with (variant).with (phrasePosition (song, a)), p16FromPercent (spec.revoice)))
                    {
                        // a recurring chord, re-voiced: its second-best placement
                        best = bestVoicing (*cands, lead, target, *vs, best);
                    }
                    else if (std::find (voiced.begin(), voiced.end(), again) == voiced.end())
                        voiced.push_back (again);
                    const Voiced& v = (*cands)[best];
                    prev = v;
                    havePrev = true;

                    // the onsets: one held pad, or the stab grid's hits (plus the pushed change)
                    struct Onset
                    {
                        std::int64_t tick;
                        std::int64_t length;
                        std::int32_t vel;
                    };
                    std::vector<Onset> onsets;
                    if (grid.pad)
                        onsets.push_back ({ a, std::max<std::int64_t> (1, e - a - grid.padGap), grid.padVel });
                    else
                    {
                        for (std::int64_t t = util::ceilDiv (a, ticksPerStep) * ticksPerStep; t < e; t += ticksPerStep)
                        {
                            const std::int64_t step = t / ticksPerStep;
                            const auto barIn = static_cast<std::int32_t> (step / stepsPerBar - s.startBar);
                            const auto cell = static_cast<std::size_t> ((barIn % 2) * 16 + step % stepsPerBar);
                            const std::int32_t vb = variantAt (song, style, li, s.index, barIn);
                            const auto mk = std::make_pair (std::string (gridName), vb);
                            auto mi = stabMasks.find (mk);
                            if (mi == stabMasks.end())
                                mi = stabMasks.emplace (mk, drawCells (grid.p, kc.with (rng::tag ("stab")).with (rng::tag (gridName)).with (vb), style.attempts, grid.minPerBar)).first;
                            bool hit = ((mi->second >> cell) & 1u) != 0
                                       || (grid.orn[cell] > 0 && ornamentBar (barIn, grid.ornBars)
                                           && rng::chance (kc.with (rng::tag ("orn")).with (step), p16FromPercent (grid.orn[cell])));
                            const bool anticipation = t == c.start && c.pushedBy > 0 && grid.anticipate;
                            if (!hit && !anticipation)
                                continue;
                            const std::int32_t base = grid.vel[cell] > 0 ? grid.vel[cell] : 100;
                            const std::int64_t len = grid.len[cell] > 0 ? grid.len[cell] : 180;
                            onsets.push_back ({ t, len, base + rng::between (kc.with (rng::tag ("vel")).with (step), -grid.spread, grid.spread) });
                        }
                        for (std::size_t k = 0; k < onsets.size(); ++k)
                        {
                            const std::int64_t limit = (k + 1 < onsets.size() ? onsets[k + 1].tick - 10 : e) - onsets[k].tick;
                            onsets[k].length = std::max<std::int64_t> (1, std::min (onsets[k].length, limit));
                        }
                    }
                    for (const Onset& o : onsets)
                    {
                        const std::int64_t step = o.tick / ticksPerStep;
                        const std::int64_t base = std::clamp<std::int64_t> (scaled (o.vel, velScale), 1, 127 - cl.top.hi - cl.voice.hi);
                        const std::size_t n = v.keys.size();
                        std::int64_t loudest = 0;
                        std::vector<std::int64_t> vels (n, base);
                        for (std::size_t i = 0; i + 1 < n; ++i)
                        {
                            vels[i] = std::max<std::int64_t> (1, base + rng::between (kc.with (rng::tag ("voice")).with (step).with (i), cl.voice.lo, cl.voice.hi));
                            loudest = std::max (loudest, vels[i]);
                        }
                        if (n >= 2)
                            vels[n - 1] = loudest + rng::between (kc.with (rng::tag ("top")).with (step), cl.top.lo, cl.top.hi);
                        for (std::size_t i = 0; i < n; ++i)
                        {
                            Note note;
                            note.tick = o.tick;
                            note.step = step;
                            note.length = o.length;
                            note.key = static_cast<std::uint8_t> (v.keys[i]);
                            note.tags = v.tags[i];
                            note.velocity = clampVelocity (vels[i]);
                            note.voice = static_cast<std::uint8_t> (i);
                            note.flags = static_cast<std::uint16_t> ((i + 1 == n ? noteFlag::top : 0) | (o.tick == c.start && c.pushedBy > 0 ? noteFlag::pushed : 0));
                            note.section = static_cast<std::int16_t> (s.index);
                            out.score.push_back (note);
                        }
                    }
                }
            }
        }
        sortNotes (out.score);
        trimSameKey (out.score);
    }
}
