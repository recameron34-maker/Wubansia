// Feel tests for the sci-fi club (FUT02) and techno (FUT04) feels: the invariants of feel.h on the new
// lanes (the arp, the rumble, the acid), per-lane swing and the 1/f microtiming. Statistics use
// doubles: they only report on what the generator wrote.

#include "gen/content.h"
#include "gen/feel.h"
#include "gen/song.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

using namespace wubansia;
using namespace wubansia::gen;

namespace
{
    constexpr std::array<const char*, 2> kits = { "FUT02", "FUT04" };

    const Bundle& bundleFor (std::string_view id)
    {
        static std::map<std::string, Bundle, std::less<>> cache;
        auto it = cache.find (id);
        if (it == cache.end())
        {
            auto b = loadBundle (std::filesystem::path (WUBANSIA_SOURCE_DIR) / "content", id);
            INFO (b.error);
            REQUIRE (b.ok());
            it = cache.emplace (std::string (id), std::move (b.value)).first;
        }
        return it->second;
    }

    Song songFor (std::string_view id, std::uint64_t seed, std::int32_t amount = -1)
    {
        Options o;
        o.seed = seed;
        o.feelAmount = amount;
        auto s = generate (bundleFor (id), o);
        INFO (s.error);
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

    double mean (const std::vector<double>& v)
    {
        double sum = 0;
        for (const double x : v)
            sum += x;
        return v.empty() ? 0.0 : sum / static_cast<double> (v.size());
    }

    double sd (const std::vector<double>& v)
    {
        const double m = mean (v);
        double ss = 0;
        for (const double x : v)
            ss += (x - m) * (x - m);
        return v.size() < 2 ? 0.0 : std::sqrt (ss / static_cast<double> (v.size() - 1));
    }

    double autocorr (const std::vector<double>& v, std::size_t lag)
    {
        const double m = mean (v);
        double num = 0, den = 0;
        for (std::size_t i = 0; i < v.size(); ++i)
        {
            den += (v[i] - m) * (v[i] - m);
            if (i + lag < v.size())
                num += (v[i] - m) * (v[i + lag] - m);
        }
        return den <= 0.0 ? 0.0 : num / den;
    }
}

TEST_CASE ("styles: feel amount 0 gives the score back exactly", "[gen][styles][feel]")
{
    for (const char* id : kits)
        for (std::uint64_t seed : { 1u, 2u })
        {
            INFO (id << " seed " << seed);
            const Song s = songFor (id, seed, 0);
            for (const auto& lane : s.lanes)
            {
                INFO (lane.id);
                CHECK (lane.perf == lane.score);
            }
        }
}

TEST_CASE ("styles: anchors stay within 2 ticks and nothing leaves its section", "[gen][styles][feel]")
{
    for (const char* id : kits)
        for (std::uint64_t seed = 1; seed <= 8; ++seed)
        {
            INFO (id << " seed " << seed);
            const Bundle& b = bundleFor (id);
            const Song s = songFor (id, seed);
            std::size_t anchors = 0;
            for (const auto& lane : s.lanes)
            {
                REQUIRE (lane.perf.size() == lane.score.size());
                for (const Note& n : lane.perf)
                {
                    const Section& sec = s.sections[static_cast<std::size_t> (n.section)];
                    CHECK (n.tick >= sec.startTick());
                    CHECK (n.tick + n.length <= sec.endTick());
                    CHECK (n.length >= 1);
                    // within the clamp of their place on the lane's swung grid (an odd 16th's swung)
                    const FeelLane f = b.feel.resolve (lane.id, sec.kind);
                    if ((n.flags & noteFlag::anchor) && f.anchor.value_or (false))
                    {
                        ++anchors;
                        REQUIRE (f.swing->lo == f.swing->hi);
                        const std::int64_t place = n.step * ticksPerStep + (n.step % 2 != 0 ? swingDelay (b.kit.swing + f.swing->lo) : 0);
                        CHECK (std::abs (n.tick - place) <= b.feel.anchorClamp);
                    }
                }
            }
            CHECK (anchors > 500);
            CHECK (b.feel.anchorClamp == 2); // plan §4.1: anchors within +-2 ticks
        }
}

TEST_CASE ("styles: the low-end lock puts bass and rumble onsets on kick steps exactly on the kick", "[gen][styles][feel]")
{
    for (const char* id : kits)
    {
        std::size_t coincident = 0;
        for (std::uint64_t seed = 1; seed <= 16; ++seed)
        {
            INFO (id << " seed " << seed);
            const Song s = songFor (id, seed);
            std::map<std::int64_t, std::int64_t> kickAt;
            for (const Note& n : laneOf (s, "kick").perf)
                kickAt.emplace (n.step, n.tick);
            for (const auto& lane : s.lanes)
            {
                if (lane.kind != LaneKind::bass)
                    continue;
                for (const Note& n : lane.perf)
                {
                    const auto it = kickAt.find (n.step);
                    if (it == kickAt.end())
                        continue;
                    ++coincident;
                    INFO (lane.id << " step " << n.step);
                    CHECK (n.tick == it->second);
                }
            }
        }
        INFO (id << ": " << coincident << " bass or rumble onsets on a kick step");
        // the techno rumble sits on every kick; the sci-fi bass rests on the kicks, so only an approach note lands on one
        CHECK (coincident > (std::string (id) == "FUT04" ? 1000u : 0u));
    }
}

TEST_CASE ("styles: chord stacks spread over at most 4 ticks and the arp plays one note at a time", "[gen][styles][feel]")
{
    for (const char* id : kits)
        for (std::uint64_t seed = 1; seed <= 8; ++seed)
        {
            const Song s = songFor (id, seed);
            const LaneOut& ch = laneOf (s, "chords");
            std::map<std::int64_t, std::pair<std::int64_t, std::int64_t>> stacks;
            for (std::size_t i = 0; i < ch.score.size(); ++i)
            {
                auto [it, fresh] = stacks.emplace (ch.score[i].tick, std::make_pair (ch.perf[i].tick, ch.perf[i].tick));
                if (!fresh)
                {
                    it->second.first = std::min (it->second.first, ch.perf[i].tick);
                    it->second.second = std::max (it->second.second, ch.perf[i].tick);
                }
            }
            CHECK (stacks.size() > 20);
            for (const auto& [tick, mm] : stacks)
            {
                INFO (id << " seed " << seed << " tick " << tick);
                CHECK (mm.second - mm.first <= 4);
            }
            if (std::string (id) != "FUT02")
                continue;
            const LaneOut& arp = laneOf (s, "arp");
            for (std::size_t i = 1; i < arp.score.size(); ++i)
                CHECK (arp.score[i].tick > arp.score[i - 1].tick); // a monophonic line
        }
}

TEST_CASE ("styles: each lane swings its odd 16ths by its own amount", "[gen][styles][feel]")
{
    for (const char* id : kits)
        for (std::uint64_t seed = 1; seed <= 16; ++seed)
        {
            INFO (id << " seed " << seed);
            const Bundle& b = bundleFor (id);
            const Song s = songFor (id, seed);
            std::set<std::int32_t> swings;
            for (const auto& lane : s.lanes)
                if (lane.id != "kick" && lane.id != "snare")
                    swings.insert (lane.swing);
            CHECK (swings.size() >= 3); // G8: 3 or more distinct swings among the non-anchor lanes

            const LaneOut& hats = laneOf (s, "hats");
            const FeelLane drop = b.feel.resolve ("hats", "drop");
            std::vector<double> odd, even;
            for (const Note& p : hats.perf)
            {
                const Section& sec = s.sections[static_cast<std::size_t> (p.section)];
                if (sec.kind != "drop" || p.tick == sec.startTick())
                    continue;
                const std::int32_t map = drop.offset ? (*drop.offset)[static_cast<std::size_t> (p.step % 32)] : 0;
                (p.step % 2 ? odd : even).push_back (static_cast<double> (p.tick - p.step * ticksPerStep - map));
            }
            REQUIRE (odd.size() > 100);
            CHECK (std::abs ((mean (odd) - mean (even)) - static_cast<double> (swingDelay (hats.swing))) < 3.0);
            CHECK (hats.swing >= b.kit.swing + drop.swing->lo);
            CHECK (hats.swing <= b.kit.swing + drop.swing->hi);
        }
}

namespace
{
    // The metrics' drum group of a GM key (tools/legacy_audit/model.js): kick, snare (snare and clap),
    // hats (hats and rides), cymbal, and perc for everything else, toms and rims included.
    std::string_view drumGroupOf (std::int32_t key)
    {
        // clang-format off
        switch (key)
        {
            case 35: case 36: return "kick";
            case 38: case 39: case 40: return "snare";
            case 42: case 44: case 46: case 51: case 53: case 54: case 59: case 69: case 70: case 82: return "hats";
            case 49: case 52: case 55: case 57: return "cymbal";
            default: return "perc";
        }
        // clang-format on
    }

    // The metrics' role of a pitched track: the first word of its name (as in test_gen_styles_content.cpp,
    // only the words these kits use).
    std::string_view pitchedRoleOf (std::string_view track)
    {
        std::string word;
        for (const char c : track)
        {
            if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')))
                break;
            word += static_cast<char> (c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
        }
        if (word == "bass" || word == "acid")
            return "bass";
        if (word == "pad")
            return "chords";
        if (word == "arp")
            return "lead";
        return "other";
    }

    // G8 as the metrics measure it (tools/legacy_audit/metrics.js, laneSwings and distinctSwings): in the
    // first drop, each lane's swing is the lower median of its odd-16th offsets (onsets at 60-179 ticks
    // into an 8th, less 120), for lanes with 16 or more of them; the distinct swings are the clusters
    // that sorting gives when each takes every value within 2 ticks of its smallest.
    std::map<std::string, std::int64_t> measuredSwings (const Song& s)
    {
        const Section* drop = nullptr;
        for (const Section& sec : s.sections)
            if (sec.kind == "drop")
            {
                drop = &sec;
                break;
            }
        REQUIRE (drop != nullptr);
        std::map<std::string, std::vector<std::int64_t>> odd;
        for (const auto& lane : s.lanes)
            for (const Note& p : lane.perf)
            {
                if (p.tick < drop->startTick() || p.tick >= drop->endTick())
                    continue;
                const std::string_view group = lane.kind == LaneKind::drums ? drumGroupOf (p.key) : pitchedRoleOf (lane.track);
                if (group == "kick" || group == "cymbal" || group == "other")
                    continue;
                const std::int64_t r = ((p.tick % 240) + 240) % 240;
                if (r >= 60 && r < 180)
                    odd[std::string (group)].push_back (r - 120);
            }
        std::map<std::string, std::int64_t> out;
        for (auto& [group, v] : odd)
            if (v.size() >= 16)
            {
                std::sort (v.begin(), v.end());
                out[group] = v[(v.size() - 1) / 2];
            }
        return out;
    }

    std::size_t swingClusters (const std::map<std::string, std::int64_t>& swings)
    {
        std::vector<std::int64_t> d;
        for (const auto& [group, delay] : swings)
            d.push_back (delay);
        std::sort (d.begin(), d.end());
        std::size_t count = 0;
        for (std::size_t i = 0; i < d.size();)
        {
            const std::int64_t start = d[i];
            ++count;
            while (i < d.size() && d[i] <= start + 2)
                ++i;
        }
        return count;
    }
}

TEST_CASE ("styles: the first drop has 3 or more swing clusters as the metrics measure them (G8)", "[gen][styles][feel]")
{
    // A proxy on FUT02 and FUT04; the gate itself (G8, all four kits) runs in test_gen_gates.cpp.
    // Every seed passes, not a share: each style keeps three lanes that every drop measures (sci-fi club:
    // the bass, the arp and the hats; techno: the acid, the hats and the perc) in swing tiers far enough
    // apart that the 1/f drift and the hats' offset map can't merge them.
    for (const char* id : kits)
    {
        std::map<std::size_t, std::size_t> histogram;
        for (std::uint64_t seed = 0; seed < 128; ++seed)
        {
            const Song s = songFor (id, seed);
            const auto swings = measuredSwings (s);
            std::string lanes;
            for (const auto& [group, delay] : swings)
                lanes += group + "=" + std::to_string (delay) + " ";
            INFO (id << " seed " << seed << ": " << lanes);
            for (const char* always : { "bass", "hats" })
                CHECK (swings.count (always) == 1);
            CHECK (swings.count (std::string (id) == "FUT02" ? "lead" : "perc") == 1);
            const std::size_t k = swingClusters (swings);
            CHECK (k >= 3);
            ++histogram[k];
        }
        std::string h;
        for (const auto& [k, n] : histogram)
            h += std::to_string (k) + ":" + std::to_string (n) + " ";
        WARN (id << " G8 clusters over 128 seeds: " << h);
    }
}

TEST_CASE ("styles: hat and perc microtiming is 1/f, correlated with a moderate spread", "[gen][styles][feel]")
{
    // A proxy per track; the gate itself (G7 on the drum groups) runs in test_gen_gates.cpp.
    for (const char* id : kits)
        for (const char* laneId : { "hats", "perc" })
            for (std::uint64_t seed = 1; seed <= 4; ++seed)
            {
                INFO (id << " " << laneId << " seed " << seed);
                const Song s = songFor (id, seed);
                const LaneOut& lane = laneOf (s, laneId);
                std::map<std::int64_t, std::vector<double>> byPos;
                std::vector<std::pair<std::int64_t, double>> raw;
                for (const Note& p : lane.perf)
                {
                    const Section& sec = s.sections[static_cast<std::size_t> (p.section)];
                    if (!(sec.kind == "drop" || sec.kind == "drop2") || p.tick == sec.startTick())
                        continue;
                    const double d = static_cast<double> (p.tick - p.step * ticksPerStep);
                    byPos[p.step % 32].push_back (d);
                    raw.emplace_back (p.step, d);
                }
                std::map<std::int64_t, double> posMean;
                for (const auto& [pos, v] : byPos)
                    posMean[pos] = mean (v);
                std::vector<double> resid;
                for (const auto& [step, d] : raw)
                    resid.push_back (d - posMean[step % 32]);
                REQUIRE (resid.size() > 100);
                const double s1 = sd (resid);
                CHECK (s1 > 1.5); // G7: residual SD 1.5-6 ticks
                CHECK (s1 < 6.0);
                CHECK (autocorr (resid, 1) > 0.2); // G7: lag-1 autocorrelation at least 0.2
            }
}
