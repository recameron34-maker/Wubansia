// annotatePhrases (P6): sched/phrase_match.h says what it does. tools/sound/phrase_tables.py's resolve () and the
// continues () inside its play () are its model, step for step.

#include "sched/phrase_match.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <utility>
#include <vector>

namespace wubansia::engine
{
    namespace
    {
        std::int64_t floorDiv (std::int64_t a, std::int64_t b) noexcept
        {
            const auto q = a / b;
            return (a % b != 0 && ((a < 0) != (b < 0))) ? q - 1 : q;
        }

        std::int64_t wrapInto (std::int64_t a, std::int64_t length) noexcept
        {
            const auto r = a % length;
            return r < 0 ? r + length : r;
        }

        // 48 kHz frames as milliticks at bpm, rounded half up: frames x bpm / 3 (the pre-roll step's rule).
        std::int64_t framesToMilliticks (std::int64_t frames, std::int32_t bpm) noexcept
        {
            const std::int64_t x = frames * bpm;
            return x >= 0 ? (2 * x + 3) / 6 : -((-2 * x + 3) / 6);
        }

        // Where a note was written: its onset, pos + preroll (P7's pre-roll step sets preroll).
        std::int64_t onsetOf (const Note& n) noexcept
        {
            return n.pos + n.preroll;
        }

        // One controller's points on a channel, by position: (pos, value).
        using Points = std::vector<std::pair<std::int64_t, int>>;

        // The last value at or before pos; 0 before any.
        int valueAt (const Points& points, std::int64_t pos) noexcept
        {
            const auto it = std::upper_bound (points.begin(), points.end(), pos,
                [] (std::int64_t p, const std::pair<std::int64_t, int>& x) { return p < x.first; });
            return it == points.begin() ? 0 : std::prev (it)->second;
        }

        // Whether the cycle from song 16th `first` rides the phrase's ride: each value within kRideSlack, on the
        // kRideTicks grid from the cycle's start. An empty ride fits any cycle.
        bool rideFits (const PhraseEntry& p, const Points& cc74, const Points& cc71, std::int64_t first) noexcept
        {
            const std::int64_t start = first * kMilliticksPerSixteenth;
            const std::int64_t step = std::int64_t { kRideTicks } * kMilliticksPerTick;
            for (const auto& [ride, points] : { std::pair { &p.ride74, &cc74 }, std::pair { &p.ride71, &cc71 } })
                for (std::size_t i = 0; i < ride->size(); ++i)
                    if (std::abs (valueAt (*points, start + static_cast<std::int64_t> (i) * step) - int { (*ride)[i] }) > kRideSlack)
                        return false;
            return true;
        }

        // A phrase's stem frames: the shorter of its A and B stems.
        std::int64_t stemFrames (const KitManifest& kit, const PhraseEntry& entry) noexcept
        {
            std::int64_t frames = 0;
            for (const auto stem : { entry.stemA, entry.stemB })
                if (stem >= 0 && static_cast<std::size_t> (stem) < kit.files.size())
                {
                    const auto f = kit.files[static_cast<std::size_t> (stem)].frames;
                    frames = frames == 0 ? f : std::min (frames, f);
                }
            return frames;
        }

        // A note on a phrase channel, as the matcher sees it.
        struct Item
        {
            std::size_t index = 0; // into timeline.notes
            std::int64_t onset = 0; // milliticks; within a loop's length
            std::int64_t sixteenth = 0; // the 16th it sounds in, from song 16th 0
            int key = 0;
        };

        // A note that plays a slice: its note, its phrase (the manifest's index), the phrase's own slice, the cycle.
        struct Match
        {
            std::size_t index = 0;
            int phrase = 0;
            int slice = 0;
            std::int64_t cycle = 0;
            std::int64_t onset = 0;
        };

        // The cycle's matches and holds on one channel: resolve ()'s loop over a cycle's notes, in onset order. The
        // cycle plays the phrase only when its matched and held notes are more than half its notes; otherwise every
        // note of the cycle stays on the multisample.
        void matchCycle (Timeline& timeline, const std::vector<PhraseEntry>& phrases, const std::vector<int>& on,
            const Points& cc74, const Points& cc71, std::int64_t c, std::int64_t cycle, const Item* begin,
            const Item* end, std::vector<Match>& matches)
        {
            // Each phrase's score: its distinct cells among the cycle's notes, or -1 off its ride. The first best wins.
            int best = -1, bestScore = 0;
            for (const int p : on)
            {
                const auto& entry = phrases[static_cast<std::size_t> (p)];
                int score = -1;
                if (rideFits (entry, cc74, cc71, c * cycle))
                {
                    score = 0;
                    for (const auto& s : entry.slices)
                        score += std::any_of (begin, end, [&] (const Item& it) {
                            return it.sixteenth - c * cycle == s.step && it.key == s.key;
                        }) ? 1 : 0;
                }
                if (best < 0 || score > bestScore)
                {
                    best = p;
                    bestScore = score;
                }
            }
            if (bestScore <= 0)
                return; // the cycle's notes stay on the multisample

            const auto& entry = phrases[static_cast<std::size_t> (best)];
            auto sliceOf = [&] (std::int64_t step, int key) {
                for (std::size_t j = 0; j < entry.slices.size(); ++j)
                    if (entry.slices[j].step == step && entry.slices[j].key == key)
                        return static_cast<int> (j);
                return -1;
            };
            std::vector<bool> used (entry.slices.size(), false);
            std::vector<std::size_t> holds; // the held notes (indices into timeline.notes)
            const auto firstMatch = matches.size();
            std::array<int, kMaxHolds> pending {};
            int pendingCount = 0, pendingAt = 0;
            std::int64_t ownStep = -1;
            int ownKey = -1;
            for (const Item* it = begin; it != end; ++it)
            {
                const auto step = it->sixteenth - c * cycle;
                const int j = sliceOf (step, it->key);
                // The slice before holds its next held key, on its own cell or on a cell that is no slice's.
                if (pendingAt < pendingCount && it->key == pending[static_cast<std::size_t> (pendingAt)]
                    && ((step == ownStep && it->key == ownKey) || j < 0))
                {
                    ++pendingAt;
                    holds.push_back (it->index);
                    continue;
                }
                if (j < 0 || used[static_cast<std::size_t> (j)])
                    continue; // the multisample
                used[static_cast<std::size_t> (j)] = true;
                matches.push_back ({ it->index, best, j, c, it->onset });
                const auto& s = entry.slices[static_cast<std::size_t> (j)];
                pendingCount = std::clamp (s.holdCount, 0, kMaxHolds);
                for (int h = 0; h < pendingCount; ++h)
                    pending[static_cast<std::size_t> (h)] = s.holds[static_cast<std::size_t> (h)];
                pendingAt = 0;
                ownStep = step;
                ownKey = it->key;
            }

            // Matched and held: more than half the cycle's notes, or the whole cycle plays the multisample (a line
            // that lands on a few of a stem's cells isn't a patchwork of stem and multisample).
            const auto played = matches.size() - firstMatch + holds.size();
            if (2 * played <= static_cast<std::size_t> (end - begin))
            {
                matches.erase (matches.begin() + static_cast<std::ptrdiff_t> (firstMatch), matches.end());
                return;
            }
            for (auto m = firstMatch; m < matches.size(); ++m)
                timeline.notes[matches[m].index].phrase = static_cast<std::int16_t> (best);
            for (const auto i : holds)
            {
                auto& held = timeline.notes[i];
                held.phrase = static_cast<std::int16_t> (best);
                held.flags = static_cast<std::uint8_t> (held.flags | NoteFlag::held);
            }
        }

        // play ()'s continues (): per phrase, its notes in order (by onset, then slice); each reads on into the next
        // when that plays the next slice in the same cycle, or slice 0 in the next cycle after the last (round the loop
        // in a loop), starting where it ends modulo the stem's frames.
        std::vector<bool> readsOn (const Timeline& timeline, const KitManifest& kit, const std::vector<Match>& matches)
        {
            std::vector<bool> out (matches.size(), false);
            for (std::size_t p = 0; p < kit.phrases.size(); ++p)
            {
                const auto& entry = kit.phrases[p];
                std::vector<std::size_t> mine;
                for (std::size_t m = 0; m < matches.size(); ++m)
                    if (matches[m].phrase == static_cast<int> (p))
                        mine.push_back (m);
                if (mine.size() < 2)
                    continue; // a lone note doesn't read on, even into itself
                std::stable_sort (mine.begin(), mine.end(), [&] (std::size_t a, std::size_t b) {
                    return matches[a].onset != matches[b].onset ? matches[a].onset < matches[b].onset : matches[a].slice < matches[b].slice;
                });
                const auto frames = stemFrames (kit, entry);
                const auto& sl = entry.slices;
                const int last = static_cast<int> (sl.size()) - 1;
                // A slice reads on into another that starts where it ends, modulo the stem's frames (a looped stem
                // wraps either way: its last slice through its end, or its slice 0 from before its cycle).
                const auto meets = [frames] (std::int64_t end, std::int64_t start) {
                    return frames > 0 ? (end - start) % frames == 0 : end == start;
                };
                const std::int64_t cycleLength = std::int64_t { entry.cycle16ths } * kMilliticksPerSixteenth;
                const std::int64_t ring = timeline.loops && cycleLength > 0 ? std::max<std::int64_t> (1, timeline.length / cycleLength) : 0;
                const std::size_t pairs = timeline.loops ? mine.size() : mine.size() - 1;
                for (std::size_t i = 0; i < pairs; ++i)
                {
                    const auto& a = matches[mine[i]];
                    const auto& b = matches[mine[(i + 1) % mine.size()]];
                    bool reads = false;
                    const bool next = (b.slice == a.slice + 1 && a.cycle == b.cycle)
                                      || (a.slice == last && b.slice == 0 // into the next cycle (round a loop)
                                          && (ring > 0 ? wrapInto (b.cycle - a.cycle, ring) == 1 % ring : b.cycle - a.cycle == 1));
                    reads = next && meets (sl[static_cast<std::size_t> (a.slice)].end, sl[static_cast<std::size_t> (b.slice)].start);
                    out[mine[i]] = reads;
                }
            }
            return out;
        }
    }

    std::int64_t sixteenthStart (std::int64_t sixteenth, std::int32_t swingPm) noexcept
    {
        return sixteenth * kMilliticksPerSixteenth + (sixteenth % 2 != 0 ? std::int64_t { swingPm - 500 } * 480 : 0);
    }

    std::int64_t sixteenthOf (std::int64_t pos, std::int32_t swingPm) noexcept
    {
        auto s = floorDiv (pos, kMilliticksPerSixteenth);
        if (sixteenthStart (s, swingPm) > pos)
            --s;
        return s;
    }

    void annotatePhrases (Timeline& timeline, const KitManifest& kit)
    {
        const auto& phrases = kit.phrases;
        if (phrases.empty() || timeline.notes.empty())
            return;
        const auto swing = kit.kit.swingPm;
        const bool loops = timeline.loops && timeline.length > 0;

        std::vector<Match> matches;
        for (int channel = 0; channel < 16; ++channel)
        {
            std::vector<int> on; // the channel's phrases, in the manifest's order
            for (std::size_t p = 0; p < phrases.size(); ++p)
                if (phrases[p].ch - 1 == channel && ! phrases[p].slices.empty())
                    on.push_back (static_cast<int> (p));
            if (on.empty())
                continue;
            const std::int64_t cycle = phrases[static_cast<std::size_t> (on.front())].cycle16ths;
            if (cycle <= 0 || std::any_of (on.begin(), on.end(), [&] (int p) { return phrases[static_cast<std::size_t> (p)].cycle16ths != cycle; }))
                continue; // the parser refuses this: one channel, one cycle length

            std::vector<Item> items;
            for (std::size_t i = 0; i < timeline.notes.size(); ++i)
            {
                const auto& n = timeline.notes[i];
                if (n.channel != channel)
                    continue;
                Item it;
                it.index = i;
                it.onset = loops ? wrapInto (onsetOf (n), timeline.length) : onsetOf (n);
                it.sixteenth = sixteenthOf (it.onset, swing);
                it.key = n.key;
                items.push_back (it);
            }
            if (items.empty())
                continue;
            std::stable_sort (items.begin(), items.end(), [] (const Item& a, const Item& b) {
                return a.onset != b.onset ? a.onset < b.onset : a.key < b.key;
            });

            Points cc74, cc71;
            for (const auto& c : timeline.ccs)
                if (c.channel == channel && (c.controller == 74 || c.controller == 71))
                    (c.controller == 74 ? cc74 : cc71).emplace_back (c.pos, c.value);
            const auto byPos = [] (const std::pair<std::int64_t, int>& a, const std::pair<std::int64_t, int>& b) { return a.first < b.first; };
            std::stable_sort (cc74.begin(), cc74.end(), byPos);
            std::stable_sort (cc71.begin(), cc71.end(), byPos);

            for (std::size_t from = 0; from < items.size();) // cycle by cycle: the items come in onset order
            {
                const auto c = floorDiv (items[from].sixteenth, cycle);
                std::size_t to = from;
                while (to < items.size() && floorDiv (items[to].sixteenth, cycle) == c)
                    ++to;
                matchCycle (timeline, phrases, on, cc74, cc71, c, cycle, items.data() + from, items.data() + to, matches);
                from = to;
            }
        }
        if (matches.empty())
            return;

        // The slice notes, beside their notes: from the slice's start frame on its cycle, as long as the slice, with
        // the preroll that takes pos to the onset (negative where the frame rounds past it).
        std::vector<int> firstSlice (phrases.size(), 0); // Kit::slices' layout: each phrase's after the ones before
        for (std::size_t p = 1; p < phrases.size(); ++p)
            firstSlice[p] = firstSlice[p - 1] + static_cast<int> (phrases[p - 1].slices.size());
        const auto continues = readsOn (timeline, kit, matches);
        std::vector<Note> added;
        added.reserve (matches.size());
        for (std::size_t m = 0; m < matches.size(); ++m)
        {
            const auto& match = matches[m];
            const auto& entry = phrases[static_cast<std::size_t> (match.phrase)];
            const auto& s = entry.slices[static_cast<std::size_t> (match.slice)];
            Note n = timeline.notes[match.index];
            const auto cycleLength = std::int64_t { entry.cycle16ths } * kMilliticksPerSixteenth;
            const auto onset = sixteenthStart (match.cycle * entry.cycle16ths + s.step, swing);
            n.pos = match.cycle * cycleLength + framesToMilliticks (s.start, kit.kit.bpm);
            if (s.start + s.lead >= stemFrames (kit, entry)) // a slice 0 whose pre-roll precedes its cycle (it wraps)
                n.pos -= cycleLength; // sits in the cycle before: round a loop to its end, before a song's start
            n.preroll = static_cast<std::int32_t> (onset - n.pos);
            if (loops)
                n.pos = wrapInto (n.pos, timeline.length);
            n.len = std::max (kMilliticksPerTick, framesToMilliticks (s.end - s.start, kit.kit.bpm));
            n.flags = continues[m] ? NoteFlag::continues : std::uint8_t { 0 };
            n.phrase = static_cast<std::int16_t> (match.phrase);
            n.slice = static_cast<std::int16_t> (firstSlice[static_cast<std::size_t> (match.phrase)] + match.slice);
            added.push_back (n);
        }
        timeline.notes.insert (timeline.notes.end(), added.begin(), added.end());
        std::stable_sort (timeline.notes.begin(), timeline.notes.end(), [] (const Note& a, const Note& b) {
            if (a.pos != b.pos)
                return a.pos < b.pos;
            if (a.channel != b.channel)
                return a.channel < b.channel;
            return a.key < b.key;
        });
    }
}
