// buildTimeline: see timeline_builder.h for the steps and their order.

#include "sched/timeline_builder.h"

#include "midi/reader.h"
#include "sched/phrase_match.h"
#include "util/ints.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

namespace wubansia::engine
{
    namespace
    {
        // What a (channel, key) plays, as instrument indices (KeymapEntry's a and b).
        struct Target
        {
            int a = -1;
            int b = -1;
        };

        using Targets = std::array<std::array<Target, 128>, 16>;

        Targets targetsOf (const KitManifest& kit)
        {
            Targets targets {};
            const auto count = static_cast<int> (kit.instruments.size());
            for (const auto& entry : kit.keymap)
            {
                if (entry.ch < 1 || entry.ch > 16 || entry.a < 0 || entry.a >= count || entry.b >= count)
                    continue;
                for (int key = std::max (0, entry.lo); key <= std::min (127, entry.hi); ++key)
                    targets[static_cast<std::size_t> (entry.ch - 1)][static_cast<std::size_t> (key)] = { entry.a, entry.b };
            }
            return targets;
        }

        // The pre-roll of an instrument's files (every file of one instrument has the same).
        std::int32_t prerollOf (const KitManifest& kit, const InstrumentEntry& instrument)
        {
            for (const auto& zone : instrument.zones)
                for (const auto& layer : zone.layers)
                    for (const auto file : layer.rr)
                        if (file >= 0 && static_cast<std::size_t> (file) < kit.files.size())
                            return kit.files[static_cast<std::size_t> (file)].prerollFrames;
            return 0;
        }

        // The instrument's pre-roll at a key, as 48 kHz frames of time: its files' played at the key's
        // pitch over the root of the zone that holds it (a key no zone holds plays nothing: 0).
        double prerollAt (const KitManifest& kit, const InstrumentEntry& instrument, int key)
        {
            const std::int32_t frames = prerollOf (kit, instrument);
            if (frames <= 0)
                return 0.0;
            for (const auto& zone : instrument.zones)
                if (key >= zone.lo && key <= zone.hi)
                    return prerollTime (frames, key - zone.root);
            return 0.0;
        }

        // Each lane's onset-duck pre_us, from every insert list of the mix (parallel chains included).
        void collectPreUs (const std::vector<Insert>& inserts, std::array<std::int32_t, kLanes>& preUs)
        {
            for (const auto& insert : inserts)
            {
                if (const auto* duck = std::get_if<DuckParams> (&insert.params); duck != nullptr && duck->mode == DuckMode::onset)
                {
                    for (int lane = 0; lane < kLanes; ++lane)
                        if ((duck->keyLanes & (1u << lane)) != 0)
                            preUs[static_cast<std::size_t> (lane)] = std::max (preUs[static_cast<std::size_t> (lane)], duck->preUs);
                }
                else if (const auto* parallel = std::get_if<ParallelParams> (&insert.params))
                    collectPreUs (parallel->chain, preUs);
            }
        }

        std::array<std::int32_t, kLanes> preUsOf (const MixConfig& mix)
        {
            std::array<std::int32_t, kLanes> preUs {};
            for (const auto& channel : mix.channels)
                collectPreUs (channel.inserts, preUs);
            collectPreUs (mix.drums.inserts, preUs);
            collectPreUs (mix.music.inserts, preUs);
            collectPreUs (mix.space.inserts, preUs);
            collectPreUs (mix.echo.inserts, preUs);
            collectPreUs (mix.resonator.inserts, preUs);
            for (const auto& eq : mix.busEq)
                collectPreUs (eq, preUs);
            return preUs;
        }

        struct RawNote
        {
            std::int64_t tick = 0;
            std::int64_t length = 0;
            std::uint8_t channel = 0;
            std::uint8_t key = 0;
            std::uint8_t velocity = 0;
        };

        struct RawCc
        {
            std::int64_t tick = 0;
            std::uint8_t channel = 0;
            std::uint8_t number = 0;
            std::uint8_t value = 0;
        };

        // A throw controller's values on one channel, in the merged file order: the value a note latches
        // is the last one at or before its tick.
        struct Latch
        {
            std::vector<std::pair<std::int64_t, std::uint8_t>> points; // (tick, value), by tick
            std::uint8_t initial = 0;

            std::uint8_t at (std::int64_t tick) const noexcept
            {
                const auto it = std::upper_bound (points.begin(), points.end(), tick,
                    [] (std::int64_t t, const std::pair<std::int64_t, std::uint8_t>& p) { return t < p.first; });
                return it == points.begin() ? initial : std::prev (it)->second;
            }
        };

        constexpr bool isRide (std::uint8_t number) noexcept { return number == 74 || number == 71; }
        constexpr bool isThrow (std::uint8_t number) noexcept { return number == 91 || number == 94; }

        // Step 7b: NoteFlag::dip on both notes of every dip pair of each mono instrument. `group[i]` is
        // the note's mono instrument, or -1; positions are before the loop wrap.
        void flagDips (std::vector<Note>& notes, const std::vector<int>& group, std::size_t instruments, bool loop, std::int64_t length)
        {
            std::vector<std::vector<std::size_t>> byInstrument (instruments);
            for (std::size_t i = 0; i < notes.size(); ++i)
                if (group[i] >= 0)
                    byInstrument[static_cast<std::size_t> (group[i])].push_back (i);

            for (auto& line : byInstrument)
            {
                const std::size_t n = line.size();
                if (n < 2)
                    continue;
                std::stable_sort (line.begin(), line.end(), [&notes] (std::size_t x, std::size_t y) { return notes[x].pos < notes[y].pos; });

                // The note before and after line[i], with its position moved by a cycle across a loop's seam.
                auto at = [&] (std::ptrdiff_t i) -> std::pair<const Note*, std::int64_t> {
                    const auto count = static_cast<std::ptrdiff_t> (n);
                    if (i >= 0 && i < count)
                        return { &notes[line[static_cast<std::size_t> (i)]], 0 };
                    if (! loop)
                        return { nullptr, 0 };
                    const auto wrapped = ((i % count) + count) % count;
                    return { &notes[line[static_cast<std::size_t> (wrapped)]], i < 0 ? -length : length };
                };
                auto legato = [] (const Note& before, std::int64_t beforeShift, const Note& after, std::int64_t afterShift) {
                    const auto start = before.pos + beforeShift;
                    const auto next = after.pos + afterShift;
                    return next > start && next < start + before.len;
                };

                for (std::size_t i = 0; i < n; ++i)
                {
                    const auto [prev, prevShift] = at (static_cast<std::ptrdiff_t> (i) - 1);
                    const auto [next, nextShift] = at (static_cast<std::ptrdiff_t> (i) + 1);
                    if (prev == nullptr || next == nullptr)
                        continue;
                    const Note& cur = notes[line[i]];
                    if (legato (*prev, prevShift, cur, 0) && legato (cur, 0, *next, nextShift) && next->key == prev->key)
                    {
                        notes[line[i]].flags |= NoteFlag::dip;
                        const auto nextIndex = (i + 1) % n;
                        notes[line[nextIndex]].flags |= NoteFlag::dip;
                    }
                }
            }
        }

        bool byPosition (const Note& a, const Note& b) noexcept
        {
            return std::tie (a.pos, a.channel, a.key) < std::tie (b.pos, b.channel, b.key);
        }
    }

    util::Result<Timeline> buildTimeline (std::span<const std::uint8_t> smf, const KitManifest& kit, const MixConfig& mix,
        TimelineOptions options)
    {
        auto parsed = midi::readSmf (smf);
        if (! parsed.ok())
            return util::Result<Timeline>::failure ("smf: " + parsed.error);
        const midi::File& file = parsed.value;
        if (file.ppq != kTicksPerQuarter)
            return util::Result<Timeline>::failure ("smf: " + std::to_string (file.ppq) + " ticks per quarter; the content has "
                                                    + std::to_string (kTicksPerQuarter));

        const bool loop = options.loopTicks > 0;
        std::int64_t endTick = 0;
        for (const auto& track : file.tracks)
            endTick = std::max (endTick, track.endTick);
        const std::int64_t length = (loop ? options.loopTicks : endTick) * kMilliticksPerTick;
        auto wrap = [loop, length] (std::int64_t pos) { return loop ? util::floorMod (pos, length) : pos; };

        const std::int32_t bpm = kit.kit.bpm > 0 ? kit.kit.bpm : 120;
        const std::int32_t swingPm = kit.kit.swingPm;
        const auto targets = targetsOf (kit);
        const auto preUs = preUsOf (mix);

        // The notes something plays, and the controllers the engine uses, from every track.
        std::vector<RawNote> raw;
        std::vector<RawCc> controllers;
        for (const auto& track : file.tracks)
        {
            for (const auto& n : midi::notesOf (track))
            {
                if (n.channel > 15 || n.key > 127 || n.velocity == 0)
                    continue;
                if (targets[n.channel][n.key].a < 0)
                    continue;
                raw.push_back ({ n.tick, n.length, n.channel, n.key, n.velocity });
            }
            for (const auto& c : midi::controllersOf (track))
                if (c.channel <= 15 && (isRide (c.number) || isThrow (c.number)))
                    controllers.push_back ({ c.tick, c.channel, c.number, c.value });
        }
        // By (tick, channel, key); ties keep track order, then file order.
        std::stable_sort (raw.begin(), raw.end(), [] (const RawNote& a, const RawNote& b) {
            return std::tie (a.tick, a.channel, a.key) < std::tie (b.tick, b.channel, b.key);
        });
        std::stable_sort (controllers.begin(), controllers.end(), [] (const RawCc& a, const RawCc& b) { return a.tick < b.tick; });

        // Step 6's latches: per channel, CC91 [0] and CC94 [1].
        std::array<std::array<Latch, 2>, 16> latches {};
        for (const auto& c : controllers)
            if (isThrow (c.number))
                latches[c.channel][c.number == 91 ? 0 : 1].points.emplace_back (c.tick, c.value);
        if (loop)
            for (auto& channel : latches)
                for (auto& latch : channel)
                    if (! latch.points.empty())
                        latch.initial = latch.points.back().second;

        Timeline timeline;
        timeline.length = length;
        timeline.loops = loop;
        timeline.notes.reserve (raw.size());

        const std::size_t instruments = kit.instruments.size();
        std::vector<std::int64_t> rrBlock (instruments, std::numeric_limits<std::int64_t>::min());
        std::vector<std::uint32_t> rrCount (instruments, 0);
        std::vector<int> monoGroup;
        monoGroup.reserve (raw.size());

        for (const auto& r : raw)
        {
            const Target target = targets[r.channel][r.key];
            const auto& a = kit.instruments[static_cast<std::size_t> (target.a)];
            const InstrumentEntry* b = target.b >= 0 ? &kit.instruments[static_cast<std::size_t> (target.b)] : nullptr;

            // 1. Swing snap, starts and ends alike.
            const std::int64_t start = swingSnap (r.tick, swingPm);
            std::int64_t end = swingSnap (r.tick + r.length, swingPm);
            end = std::max (end, start + kMilliticksPerTick);

            // 2. Pre-roll at the note's pitch: the smaller of an A/B pair's.
            double preroll = prerollAt (kit, a, r.key);
            if (b != nullptr)
                preroll = std::min (preroll, prerollAt (kit, *b, r.key));
            const std::int64_t shift = preroll > 0.0 ? timeToMilliticks (preroll, bpm) : 0;

            Note note;
            note.pos = start - shift;
            note.len = end - start + shift;
            note.channel = r.channel;
            note.key = r.key;
            note.velocity = r.velocity;

            // 4. The hit's count in its instrument's rr block, by its written tick.
            const auto ai = static_cast<std::size_t> (target.a);
            const std::int64_t blockTicks = std::int64_t { std::max (1, a.rrBlock16ths) } * (kTicksPerQuarter / 4);
            const std::int64_t block = util::floorDiv (r.tick, blockTicks);
            if (block != rrBlock[ai])
            {
                rrBlock[ai] = block;
                rrCount[ai] = 0;
            }
            const std::uint32_t hit = rrCount[ai]++;
            note.rr = static_cast<std::uint8_t> (hit & 0xFFu);
            note.panIndex = static_cast<std::uint8_t> (hit & 0xFFu);

            if (a.align == Align::end || (b != nullptr && b->align == Align::end))
                note.flags |= NoteFlag::endAligned;
            if (shift > 0 || note.len > kMilliticksPerQuarter)
                note.flags |= NoteFlag::chase;

            // 6. The throws in force at the note-on.
            note.throwSpace = latches[r.channel][0].at (r.tick);
            note.throwEcho = latches[r.channel][1].at (r.tick);

            // 7. A duck key on the onset, pre_us early.
            if (a.duckKey || (b != nullptr && b->duckKey))
            {
                const auto lane = static_cast<std::size_t> (a.lane);
                const std::int64_t early = preUs[lane] > 0 ? microsToMilliticks (preUs[lane], bpm) : 0;
                timeline.keys.push_back ({ start - early, static_cast<std::uint8_t> (a.lane), r.velocity });
            }

            monoGroup.push_back ((a.mono || (b != nullptr && b->mono)) ? target.a : -1);
            timeline.notes.push_back (note);
        }

        // Dips, on positions before the wrap.
        flagDips (timeline.notes, monoGroup, instruments, loop && length > 0, length);

        // 3. The loop wrap; a note never outlasts its loop.
        if (loop)
        {
            for (auto& note : timeline.notes)
            {
                note.pos = wrap (note.pos);
                note.len = std::min (note.len, length);
            }
            for (auto& key : timeline.keys)
                key.pos = wrap (key.pos);
        }
        std::stable_sort (timeline.notes.begin(), timeline.notes.end(), byPosition);
        std::stable_sort (timeline.keys.begin(), timeline.keys.end(), [] (const DuckKey& a, const DuckKey& b) { return a.pos < b.pos; });

        // 5. The rides, per channel and controller, with their ramps. In a loop a point at the end comes
        // first at 0, so the loop's own value at 0 wins.
        {
            struct Point
            {
                std::int64_t pos;
                bool wrapped;
                std::uint8_t value;
            };
            std::array<std::array<std::vector<Point>, 2>, 16> rides {};
            for (const auto& c : controllers)
                if (isRide (c.number))
                {
                    const std::int64_t pos = c.tick * kMilliticksPerTick;
                    const std::int64_t at = wrap (pos);
                    rides[c.channel][c.number == 74 ? 0 : 1].push_back ({ at, at != pos, c.value });
                }
            for (std::size_t ch = 0; ch < 16; ++ch)
                for (std::size_t r = 0; r < 2; ++r)
                {
                    auto& points = rides[ch][r];
                    std::stable_sort (points.begin(), points.end(), [] (const Point& a, const Point& b) {
                        return a.pos != b.pos ? a.pos < b.pos : (a.wrapped && ! b.wrapped);
                    });
                    for (std::size_t i = 0; i < points.size(); ++i)
                    {
                        Cc cc;
                        cc.pos = points[i].pos;
                        cc.channel = static_cast<std::uint8_t> (ch);
                        cc.controller = r == 0 ? 74 : 71;
                        cc.value = points[i].value;
                        cc.rampTo = cc.pos;
                        cc.nextValue = cc.value;
                        const bool last = i + 1 == points.size();
                        if (! last || loop)
                        {
                            const Point& next = last ? points.front() : points[i + 1];
                            const std::int64_t nextPos = next.pos + (last ? length : 0);
                            if (nextPos > cc.pos && nextPos - cc.pos <= kMilliticksPerSixteenth)
                            {
                                cc.rampTo = nextPos;
                                cc.nextValue = next.value;
                            }
                        }
                        timeline.ccs.push_back (cc);
                    }
                }
            std::stable_sort (timeline.ccs.begin(), timeline.ccs.end(), [] (const Cc& a, const Cc& b) { return a.pos < b.pos; });
        }

        // Sections (FF 06) and cue points (FF 07).
        for (const auto& marker : midi::markersOf (file))
            timeline.sections.push_back ({ wrap (marker.tick * kMilliticksPerTick), marker.text });
        for (const auto& track : file.tracks)
            for (const auto& text : midi::textsOf (track))
                if (text.type == midi::meta::cuePoint)
                    timeline.cues.push_back ({ wrap (text.tick * kMilliticksPerTick), text.text });
        auto bySectionPosition = [] (const Section& a, const Section& b) { return a.pos < b.pos; };
        std::stable_sort (timeline.sections.begin(), timeline.sections.end(), bySectionPosition);
        std::stable_sort (timeline.cues.begin(), timeline.cues.end(), bySectionPosition);

        // 8. The phrase lanes (P6), on the finished notes and rides.
        annotatePhrases (timeline, kit);
        std::stable_sort (timeline.notes.begin(), timeline.notes.end(), byPosition);

        // 9. The chase list.
        for (std::size_t i = 0; i < timeline.notes.size(); ++i)
        {
            auto& note = timeline.notes[i];
            if (note.slice >= 0)
                note.flags |= NoteFlag::chase;
            if ((note.flags & NoteFlag::chase) != 0)
                timeline.chase.push_back (static_cast<std::uint32_t> (i));
        }

        return util::Result<Timeline>::success (std::move (timeline));
    }
}
