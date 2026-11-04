#include "gen/feel.h"

#include "gen/arrange.h"
#include "util/ints.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <map>
#include <string>

namespace wubansia::gen
{
    std::int32_t vossSum (rng::Key key, std::int64_t n) noexcept
    {
        const auto u = static_cast<std::uint64_t> (n);
        std::int32_t sum = rng::between (key.with (rng::tag ("white")).with (u), -1023, 1023);
        for (std::int32_t r = 0; r < vossRows; ++r)
        {
            // row r has been updated once per step whose lowest set bit is bit r, up to n
            const std::uint64_t updates = (u + (std::uint64_t (1) << r)) >> (r + 1);
            sum += rng::between (key.with (r).with (updates), -1023, 1023);
        }
        return sum;
    }

    std::int32_t vossScaled (rng::Key key, std::int64_t n, std::int32_t sdTenths) noexcept
    {
        if (sdTenths <= 0)
            return 0;
        return static_cast<std::int32_t> (util::roundDiv (static_cast<std::int64_t> (vossSum (key, n)) * sdTenths, std::int64_t (vossSdTimes10)));
    }

    namespace
    {
        // One lane's feel in one section kind, with the values drawn for the song.
        struct Row
        {
            FeelLane f;
            std::int32_t swing = 500;
            std::int32_t sd = 0;
            std::int32_t vsd = 0;
            std::int32_t clamp = 0;
            bool anchor = false;
        };

        bool rowSets (const Feel& feel, std::string_view kind, std::string_view laneId, bool (*has) (const FeelLane&))
        {
            for (const auto& row : feel.rows)
                if (row.kind == kind)
                    for (const auto& [id, f] : row.lanes)
                        if (id == laneId && has (f))
                            return true;
            return false;
        }

        std::int64_t scale (std::int64_t v, std::int32_t amount) noexcept { return util::roundDiv (v * amount, std::int64_t (1000)); }
    }

    void perform (Song& song, const Bundle& bundle)
    {
        const Style& style = bundle.style;
        const Feel& feel = bundle.feel;
        const std::int32_t amount = song.feelAmount;
        const rng::Key root = songRoot (song.kitId, song.seed).with (rng::tag ("feel"));

        // lanes that lock to another come after it: the lock of the lane's base, as lockAt below reads
        // it (makeBundle allows no lock in a row, and no lock to a lane that locks itself)
        std::vector<std::size_t> order;
        std::vector<bool> done (style.lanes.size(), false);
        for (std::size_t pass = 0; pass <= style.lanes.size() && order.size() < style.lanes.size(); ++pass)
            for (std::size_t li = 0; li < style.lanes.size(); ++li)
            {
                if (done[li])
                    continue;
                const FeelLane f = feel.resolve (style.lanes[li].id, "");
                const auto target = f.lock ? style.laneIndex (*f.lock) : std::nullopt;
                if (target && !done[*target])
                    continue;
                done[li] = true;
                order.push_back (li);
            }
        assert (order.size() == style.lanes.size()); // every lane is performed

        for (const std::size_t li : order)
        {
            const LaneSpec& lane = style.lanes[li];
            LaneOut& out = song.lanes[li];
            const rng::Key kf = root.with (rng::tag (lane.id));
            const auto ranges = activeRanges (song, li);

            // the draws per section kind: a field a row doesn't set is drawn once, for the lane's default
            std::map<std::string, Row> rows;
            const auto rowFor = [&] (const std::string& kind) -> const Row& {
                auto it = rows.find (kind);
                if (it != rows.end())
                    return it->second;
                Row r;
                r.f = feel.resolve (lane.id, kind);
                const auto keyOf = [&] (bool (*has) (const FeelLane&)) {
                    return rng::tag (rowSets (feel, kind, lane.id, has) ? kind : std::string ("default"));
                };
                r.swing = bundle.kit.swing + rng::between (kf.with (rng::tag ("swing")).with (keyOf ([] (const FeelLane& x) { return x.swing.has_value(); })), r.f.swing->lo, r.f.swing->hi);
                r.sd = rng::between (kf.with (rng::tag ("sd")).with (keyOf ([] (const FeelLane& x) { return x.sd.has_value(); })), r.f.sd->lo, r.f.sd->hi);
                if (r.f.vsd)
                    r.vsd = rng::between (kf.with (rng::tag ("vsd")).with (keyOf ([] (const FeelLane& x) { return x.vsd.has_value(); })), r.f.vsd->lo, r.f.vsd->hi);
                r.clamp = *r.f.clamp;
                r.anchor = r.f.anchor.value_or (false);
                return rows.emplace (kind, std::move (r)).first->second;
            };
            {
                // the lane's own swing, drawn for its default row: what the song reports
                const FeelLane f = feel.resolve (lane.id, "");
                out.swing = bundle.kit.swing + rng::between (kf.with (rng::tag ("swing")).with (rng::tag ("default")), f.swing->lo, f.swing->hi);
            }

            out.perf = out.score;
            const std::size_t n = out.score.size();
            const FeelLane laneDefault = feel.resolve (lane.id, "");
            const std::optional<std::size_t> lockLane = laneDefault.lock ? style.laneIndex (*laneDefault.lock) : std::nullopt;
            std::map<std::int64_t, std::int64_t> lockAt; // step -> the lock lane's performed tick
            if (lockLane)
            {
                const LaneOut& target = song.lanes[*lockLane];
                for (std::size_t i = 0; i < target.score.size(); ++i)
                    lockAt.emplace (target.score[i].step, target.perf[i].tick);
            }

            // a tight note (the row's "tight": a score velocity at or above it) plays as written: no
            // swing, offset, 1/f, spread, velocity feel or articulation; only the lock moves it
            const auto isTight = [] (const Row& r, const Note& s) { return r.f.tight && s.velocity >= *r.f.tight; };

            // timing, pass 1: swing, the offset map, 1/f, clamps; the lock
            for (std::size_t i = 0; i < n; ++i)
            {
                const Note& s = out.score[i];
                Note& p = out.perf[i];
                const Section& sec = song.sections[static_cast<std::size_t> (s.section)];
                const Row& r = rowFor (sec.kind);
                if (lockAt.contains (s.step) && r.f.lock)
                {
                    p.tick = lockAt[s.step]; // the low-end lock: exactly the kick's tick
                    continue;
                }
                if (isTight (r, s))
                {
                    p.tick = s.tick;
                    continue;
                }
                const std::int64_t swingTicks = (s.step % 2 != 0) ? swingDelay (r.swing) : 0;
                const std::int64_t map = r.f.offset ? (*r.f.offset)[static_cast<std::size_t> (s.step % 32)] : 0;
                const std::int64_t flux = vossScaled (kf.with (rng::tag ("flux")), s.step, r.sd);
                const std::int64_t limit = (r.anchor && (s.flags & noteFlag::anchor)) ? feel.anchorClamp : r.clamp;
                const std::int64_t dev = std::clamp<std::int64_t> (scale (map + flux, amount), -limit, limit);
                p.tick = s.tick + scale (swingTicks, amount) + dev;
            }
            // pass 2: voices that play a set distance after another on the same step
            for (std::size_t i = 0; i < n; ++i)
            {
                const Note& s = out.score[i];
                const Section& sec = song.sections[static_cast<std::size_t> (s.section)];
                const Row& r = rowFor (sec.kind);
                if (!r.f.after || lane.kind != LaneKind::drums || isTight (r, s))
                    continue;
                for (const AfterRule& rule : *r.f.after)
                {
                    const auto voiceOf = [&] (std::string_view id) {
                        for (std::size_t v = 0; v < lane.drums.voices.size(); ++v)
                            if (lane.drums.voices[v].id == id)
                                return v;
                        return lane.drums.voices.size();
                    };
                    if (s.voice != voiceOf (rule.voice))
                        continue;
                    const std::size_t lead = voiceOf (rule.after);
                    // notes on one step share its score tick, so they sit next to each other
                    std::size_t from = i;
                    while (from > 0 && out.score[from - 1].tick == s.tick)
                        --from;
                    for (std::size_t k = from; k < n && out.score[k].tick == s.tick; ++k)
                        if (out.score[k].step == s.step && out.score[k].voice == lead)
                        {
                            const std::int64_t by = rng::between (kf.with (rng::tag ("after")).with (s.step), rule.by.lo, rule.by.hi);
                            std::int64_t t = out.perf[k].tick + scale (by, amount);
                            if (r.anchor && (s.flags & noteFlag::anchor))
                            {
                                // within the clamp of its place on the swung grid, as in pass 1
                                const std::int64_t place = s.tick + ((s.step % 2 != 0) ? scale (swingDelay (r.swing), amount) : 0);
                                t = std::clamp<std::int64_t> (t, place - feel.anchorClamp, place + feel.anchorClamp);
                            }
                            out.perf[i].tick = t;
                            break;
                        }
                }
            }
            // chord stacks: the voices spread upwards over a few ticks
            if (lane.kind == LaneKind::chords)
                for (std::size_t i = 0; i < n;)
                {
                    std::size_t j = i;
                    while (j < n && out.score[j].tick == out.score[i].tick)
                        ++j;
                    const Section& sec = song.sections[static_cast<std::size_t> (out.score[i].section)];
                    const Row& r = rowFor (sec.kind);
                    if (r.f.spread && j - i > 1)
                    {
                        const std::int64_t spread = rng::between (kf.with (rng::tag ("spread")).with (out.score[i].step), r.f.spread->lo, r.f.spread->hi);
                        const auto top = static_cast<std::int64_t> (j - i - 1);
                        for (std::size_t k = i; k < j; ++k)
                            if (!isTight (r, out.score[k]))
                                out.perf[k].tick += scale (util::roundDiv (spread * static_cast<std::int64_t> (out.score[k].voice), top), amount);
                    }
                    i = j;
                }

            // velocity: the map, 1/f, and a dip before an accent
            for (std::size_t i = 0; i < n; ++i)
            {
                const Note& s = out.score[i];
                const Section& sec = song.sections[static_cast<std::size_t> (s.section)];
                const Row& r = rowFor (sec.kind);
                if (isTight (r, s))
                    continue; // the score's velocity, as it was copied
                std::int64_t dv = (r.f.vel ? (*r.f.vel)[static_cast<std::size_t> (s.step % 32)] : 0)
                                  + vossScaled (kf.with (rng::tag ("vflux")), s.step, r.vsd);
                if (r.f.dip)
                    for (std::size_t k = i + 1; k < n && out.score[k].step <= s.step + 2 && out.score[k].section == s.section; ++k)
                        if (out.score[k].voice == s.voice && out.score[k].step > s.step && out.score[k].velocity >= r.f.dip->hi)
                        {
                            dv -= r.f.dip->lo;
                            break;
                        }
                // with "tight", a ghost stays below it: the file splits the notes as the score does
                const std::int64_t top = r.f.tight ? *r.f.tight - 1 : 127;
                out.perf[i].velocity = static_cast<std::uint8_t> (std::clamp<std::int64_t> (s.velocity + scale (dv, amount), 1, top));
            }

            // lengths: articulation, slides kept, the mono bass kept apart, inside the lane's range
            for (std::size_t i = 0; i < n; ++i)
            {
                const Note& s = out.score[i];
                Note& p = out.perf[i];
                const Section& sec = song.sections[static_cast<std::size_t> (s.section)];
                const Row& r = rowFor (sec.kind);
                std::int64_t len = s.length;
                if (r.f.artic && *r.f.artic > 0 && !isTight (r, s))
                {
                    const std::int64_t delta = util::roundDiv (s.length * rng::between (kf.with (rng::tag ("artic")).with (s.step).with (s.voice), -*r.f.artic, *r.f.artic),
                        std::int64_t (1000));
                    len += scale (delta, amount);
                }
                // the mono bass: a slide keeps the score's overlap, other notes stay clear of the next
                // (only inside a section: no note's performance depends on another section)
                if (lane.kind == LaneKind::bass && i + 1 < n && out.score[i + 1].section == s.section)
                {
                    const Note& ns = out.score[i + 1];
                    const std::int64_t nextTick = out.perf[i + 1].tick;
                    if (s.flags & noteFlag::slide)
                        len = nextTick - p.tick + (s.tick + s.length - ns.tick);
                    else if (s.tick + s.length <= ns.tick)
                        len = std::min (len, nextTick - p.tick - std::min<std::int64_t> (20, std::max<std::int64_t> (1, ns.tick - (s.tick + s.length))));
                }
                // inside its own section, whatever the feel's values: no note crosses a section line,
                // so the regions tile the song (a late note on a section's last 16th plays on)
                p.tick = std::clamp (p.tick, sec.startTick(), sec.endTick() - 1);
                std::int64_t end = p.tick + std::max<std::int64_t> (1, len);
                for (const TickRange& range : ranges)
                    if (s.tick >= range.start && s.tick < range.end)
                        end = std::min (end, range.end);
                end = std::min (end, sec.endTick());
                p.length = std::max<std::int64_t> (1, end - p.tick);
            }

            // the choke, on performed ticks
            if (lane.kind == LaneKind::drums)
                for (const auto& rule : lane.drums.chokes)
                {
                    std::size_t cv = 0;
                    std::vector<std::size_t> by;
                    for (std::size_t v = 0; v < lane.drums.voices.size(); ++v)
                    {
                        if (lane.drums.voices[v].id == rule.voice)
                            cv = v;
                        if (std::find (rule.by.begin(), rule.by.end(), lane.drums.voices[v].id) != rule.by.end())
                            by.push_back (v);
                    }
                    for (std::size_t i = 0; i < n; ++i)
                    {
                        Note& p = out.perf[i];
                        if (p.voice != cv)
                            continue;
                        for (std::size_t k = i + 1; k < n && out.score[k].tick < out.score[i].tick + out.score[i].length + 240 && out.score[k].section == out.score[i].section; ++k)
                            if (std::find (by.begin(), by.end(), out.perf[k].voice) != by.end() && out.perf[k].tick > p.tick)
                            {
                                p.length = std::max<std::int64_t> (1, std::min (p.length, out.perf[k].tick - p.tick));
                                break;
                            }
                    }
                }

            // no note rings into the next one on its key
            std::map<std::uint8_t, std::vector<std::size_t>> byKey;
            for (std::size_t i = 0; i < n; ++i)
                byKey[out.perf[i].key].push_back (i);
            for (auto& [key, idx] : byKey)
            {
                std::stable_sort (idx.begin(), idx.end(), [&] (std::size_t a, std::size_t b) { return out.perf[a].tick < out.perf[b].tick; });
                for (std::size_t k = 0; k + 1 < idx.size(); ++k)
                {
                    Note& a = out.perf[idx[k]];
                    const Note& b = out.perf[idx[k + 1]];
                    if (a.tick + a.length > b.tick)
                        a.length = std::max<std::int64_t> (1, b.tick - a.tick);
                }
            }
        }
    }
}
