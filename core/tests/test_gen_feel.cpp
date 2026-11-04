// Tests for core/gen/feel: the swing formula, the Voss-McCartney 1/f source and the feel pass on
// whole songs. Statistics here use doubles: they only report on what the generator wrote.

#include "gen/content.h"
#include "gen/feel.h"
#include "gen/song.h"
#include "util/file_io.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace wubansia;
using namespace wubansia::gen;

namespace
{
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

    Song songFor (const std::string& id, std::uint64_t seed, std::int32_t amount = -1)
    {
        Options o;
        o.seed = seed;
        o.feelAmount = amount;
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

    // Detrended fluctuation analysis: the slope of log F(n) against log n, boxes from 8 to 256.
    double dfaAlpha (const std::vector<double>& x)
    {
        const double m = mean (x);
        std::vector<double> y (x.size());
        double acc = 0;
        for (std::size_t i = 0; i < x.size(); ++i)
            y[i] = (acc += x[i] - m);
        std::vector<double> lx, ly;
        for (std::size_t n = 8; n <= 256; n *= 2)
        {
            double sum = 0;
            std::size_t boxes = 0;
            for (std::size_t b = 0; (b + 1) * n <= y.size(); ++b)
            {
                // least-squares line through the box
                double sx = 0, sy = 0, sxx = 0, sxy = 0;
                for (std::size_t i = 0; i < n; ++i)
                {
                    const double t = static_cast<double> (i);
                    const double v = y[b * n + i];
                    sx += t;
                    sy += v;
                    sxx += t * t;
                    sxy += t * v;
                }
                const double dn = static_cast<double> (n);
                const double slope = (dn * sxy - sx * sy) / (dn * sxx - sx * sx);
                const double icpt = (sy - slope * sx) / dn;
                for (std::size_t i = 0; i < n; ++i)
                {
                    const double r = y[b * n + i] - (icpt + slope * static_cast<double> (i));
                    sum += r * r;
                }
                ++boxes;
            }
            lx.push_back (std::log (static_cast<double> (n)));
            ly.push_back (std::log (std::sqrt (sum / static_cast<double> (boxes * n))));
        }
        const double mx = mean (lx), my = mean (ly);
        double num = 0, den = 0;
        for (std::size_t i = 0; i < lx.size(); ++i)
        {
            num += (lx[i] - mx) * (ly[i] - my);
            den += (lx[i] - mx) * (lx[i] - mx);
        }
        return num / den;
    }
}

TEST_CASE ("swingDelay is the plan's formula", "[gen][feel]")
{
    CHECK (swingDelay (500) == 0);
    CHECK (swingDelay (540) == 10); // 9.3 ms at 134 BPM (plan §2.3)
    CHECK (swingDelay (630) == 31); // 28.9 ms
    CHECK (swingDelay (570) == 17); // FUT01's base swing: 0.57 of an 8th is 16.8 ticks late
    CHECK (swingDelay (580) == 19);
    CHECK (swingDelay (530) == 7);
    CHECK (swingDelay (520) == 5);
    CHECK (swingDelay (499) == 0);
    CHECK (swingDelay (480) == -5); // -4.8 rounded half up
    // ((sw - 500) * 240 + 500) / 1000 with the division floored, for every permille
    for (std::int32_t sw = 0; sw <= 1000; ++sw)
    {
        const std::int64_t x = (static_cast<std::int64_t> (sw) - 500) * 240 + 500;
        std::int64_t floored = x / 1000;
        if (x % 1000 != 0 && x < 0)
            --floored;
        CHECK (swingDelay (sw) == floored);
        // and it is (sw - 500) * 0.24 rounded half up, checked in exact integer form
        CHECK (2 * 1000 * swingDelay (sw) <= 2 * (static_cast<std::int64_t> (sw) - 500) * 240 + 1000);
        CHECK (2 * 1000 * (swingDelay (sw) + 1) > 2 * (static_cast<std::int64_t> (sw) - 500) * 240 + 1000);
    }
}

TEST_CASE ("vossSum is pink: correlated over steps and never white", "[gen][feel]")
{
    const rng::Key k = rng::seedKey (11).with (rng::tag ("flux"));
    constexpr std::int64_t n = 1 << 16;
    std::vector<double> v, white;
    v.reserve (n);
    for (std::int64_t i = 0; i < n; ++i)
    {
        v.push_back (vossSum (k, i));
        white.push_back (rng::between (k.with (rng::tag ("white")).with (static_cast<std::uint64_t> (i)), -1023, 1023));
    }
    const double s = sd (v);
    CHECK (std::abs (mean (v)) < 0.05 * s);
    CHECK (std::abs (s - 1671.4) < 0.05 * 1671.4); // sqrt (8 * 1023 * 1024 / 3)
    const double r1 = autocorr (v, 1);
    CHECK (r1 > 0.65); // theory: 0.751
    CHECK (r1 < 0.85);
    CHECK (autocorr (v, 2) > 0.5); // theory: 0.627
    CHECK (autocorr (v, 8) > 0.2);
    CHECK (std::abs (autocorr (white, 1)) < 0.03);
    const double alpha = dfaAlpha (v);
    INFO ("DFA alpha " << alpha);
    CHECK (alpha > 0.65); // G7's band for feel lanes: 0.65-1.1
    CHECK (alpha < 1.1);
    CHECK (std::abs (dfaAlpha (white) - 0.5) < 0.1);
    // a pure function of (key, n)
    CHECK (vossSum (k, 12345) == vossSum (k, 12345));
    CHECK (vossSum (k, -5) == vossSum (k, -5));
    std::int32_t diff = 0;
    for (std::int64_t i = 0; i < 64; ++i)
        diff += vossSum (k, i) != vossSum (k.with (std::uint64_t (1)), i) ? 1 : 0;
    CHECK (diff > 60);
}

TEST_CASE ("vossScaled has the requested standard deviation", "[gen][feel]")
{
    const rng::Key k = rng::seedKey (3);
    for (const std::int32_t tenths : { 10, 30, 50 })
    {
        std::vector<double> v;
        for (std::int64_t i = 0; i < (1 << 15); ++i)
            v.push_back (vossScaled (k, i, tenths));
        INFO (tenths);
        CHECK (std::abs (sd (v) - tenths / 10.0) < 0.08 * tenths / 10.0 + 0.05);
    }
    CHECK (vossScaled (k, 7, 0) == 0);
}

TEST_CASE ("feel amount 0 gives the score back exactly", "[gen][feel]")
{
    for (const char* id : { "FUT01", "FUT03" })
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

TEST_CASE ("anchors stay within 2 ticks and nothing leaves its section", "[gen][feel]")
{
    for (const char* id : { "FUT01", "FUT03" })
        for (std::uint64_t seed = 1; seed <= 8; ++seed)
        {
            INFO (id << " seed " << seed);
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
                    // anchors: the 100% cells of the lanes whose feel row makes them anchors (kick, snare),
                    // within the clamp of their place on the lane's swung grid: an odd 16th's is the 16th
                    // plus the lane's swing (plan §2.3, the kick: 500 on even 16ths, the swing on odd ones)
                    const FeelLane f = bundleFor (id).feel.resolve (lane.id, sec.kind);
                    if ((n.flags & noteFlag::anchor) && f.anchor.value_or (false))
                    {
                        ++anchors;
                        REQUIRE (f.swing->lo == f.swing->hi); // the anchor lanes' swing is the kit's, plus a fixed offset
                        const std::int64_t place = n.step * ticksPerStep + (n.step % 2 != 0 ? swingDelay (bundleFor (id).kit.swing + f.swing->lo) : 0);
                        CHECK (std::abs (n.tick - place) <= bundleFor (id).feel.anchorClamp);
                    }
                }
            }
            CHECK (anchors > 100);
        }
}

TEST_CASE ("nothing leaves its section even at the feel's extremes", "[gen][feel]")
{
    // The loader takes a kit swing up to 750, a lane swing up to +250 on top, a clamp up to 60, an
    // after rule up to 30 ticks and the amount up to 2000: together far more than the 120 ticks that
    // move a section's last 16th into the next section. The feel must keep every note inside its
    // own section regardless (ADR 0013 §3), or the regions wouldn't tile the song.
    const auto read = [] (const std::filesystem::path& p) {
        const auto r = util::readBinaryFile (p);
        REQUIRE (r.ok());
        return std::string (r.value.begin(), r.value.end());
    };
    // every `"<key>": <value>` (a number or a [lo, hi] range) set to `to`
    const auto setAll = [] (std::string text, std::string_view key, std::string_view to) {
        const std::string tag = "\"" + std::string (key) + "\": ";
        for (std::size_t at = text.find (tag); at != std::string::npos; at = text.find (tag, at + 1))
        {
            const std::size_t from = at + tag.size();
            std::size_t end = from;
            if (text[from] == '[')
                end = text.find (']', from) + 1;
            else
                while (end < text.size() && (std::isdigit (static_cast<unsigned char> (text[end])) || text[end] == '-'))
                    ++end;
            text.replace (from, end - from, to);
        }
        return text;
    };
    const auto content = std::filesystem::path (WUBANSIA_SOURCE_DIR) / "content";
    for (const auto& [kitId, style] : { std::pair { "fut01", "garage2step" }, std::pair { "fut02", "sciclub" }, std::pair { "fut03", "futurehouse" }, std::pair { "fut04", "techno" } })
    {
        INFO (kitId);
        std::string kitText = setAll (read (content / "kits" / (std::string (kitId) + ".json")), "swing", "750");
        std::string feelText = setAll (setAll (setAll (read (content / "feels" / (std::string (style) + ".json")), "swing", "[250, 250]"), "clamp", "60"), "by", "[30, 30]");
        auto kit = parseKit (kitText, "kit.json");
        auto sty = parseStyle (read (content / "styles" / (std::string (style) + ".json")), "style.json");
        auto feel = parseFeel (feelText, "feel.json");
        REQUIRE (kit.ok());
        REQUIRE (sty.ok());
        INFO (feel.error);
        REQUIRE (feel.ok());
        auto bundle = makeBundle (std::move (kit.value), std::move (sty.value), std::move (feel.value));
        INFO (bundle.error);
        REQUIRE (bundle.ok());
        std::size_t lastSixteenths = 0;
        for (std::uint64_t seed = 1; seed <= 3; ++seed)
        {
            Options o;
            o.seed = seed;
            o.feelAmount = 2000;
            const auto s = generate (bundle.value, o);
            REQUIRE (s.ok());
            for (const auto& lane : s.value.lanes)
            {
                REQUIRE (lane.perf.size() == lane.score.size());
                for (std::size_t i = 0; i < lane.perf.size(); ++i)
                {
                    const Note& p = lane.perf[i];
                    const Section& sec = s.value.sections[static_cast<std::size_t> (p.section)];
                    INFO (lane.id << " score tick " << lane.score[i].tick << " performed at " << p.tick << ", section " << sec.startTick() << "-" << sec.endTick());
                    CHECK (p.tick >= sec.startTick());
                    CHECK (p.tick < sec.endTick());
                    CHECK (p.length >= 1);
                    CHECK (p.tick + p.length <= sec.endTick());
                    lastSixteenths += lane.score[i].tick == sec.endTick() - ticksPerStep ? 1u : 0u;
                }
            }
        }
        CHECK (lastSixteenths > 0); // the case that matters: notes on a section's last 16th
    }
}

TEST_CASE ("the low-end lock puts bass onsets on kick steps exactly on the kick", "[gen][feel]")
{
    for (const char* id : { "FUT01", "FUT03" })
        for (std::uint64_t seed = 1; seed <= 16; ++seed)
        {
            INFO (id << " seed " << seed);
            const Song s = songFor (id, seed);
            std::map<std::int64_t, std::int64_t> kickAt; // step -> performed tick
            for (const Note& n : laneOf (s, "kick").perf)
                kickAt.emplace (n.step, n.tick);
            std::size_t coincident = 0;
            for (const Note& n : laneOf (s, "bass").perf)
            {
                const auto it = kickAt.find (n.step);
                if (it == kickAt.end())
                    continue;
                ++coincident;
                CHECK (n.tick == it->second);
            }
            if (std::string (id) == "FUT01")
                CHECK (coincident > 50); // the garage sub follows the kick
        }
}

TEST_CASE ("the clap plays 4-8 ticks behind the snare in 2-step", "[gen][feel]")
{
    for (std::uint64_t seed = 1; seed <= 8; ++seed)
    {
        const Song s = songFor ("FUT01", seed);
        const LaneOut& sn = laneOf (s, "snare");
        // voice 0 is the snare, voice 1 the clap (the style's order)
        std::map<std::int64_t, std::int64_t> snareAt;
        for (const Note& n : sn.perf)
            if (n.voice == 0)
                snareAt.emplace (n.step, n.tick);
        std::size_t pairs = 0;
        for (const Note& n : sn.perf)
            if (n.voice == 1)
            {
                const auto it = snareAt.find (n.step);
                if (it == snareAt.end())
                    continue;
                ++pairs;
                const std::int64_t d = n.tick - it->second;
                INFO ("seed " << seed << " step " << n.step);
                CHECK (d >= 4);
                CHECK (d <= 8);
            }
        CHECK (pairs > 20);
    }
}

TEST_CASE ("chord stacks spread over at most 4 ticks", "[gen][feel]")
{
    for (const char* id : { "FUT01", "FUT03" })
        for (std::uint64_t seed = 1; seed <= 8; ++seed)
        {
            const Song s = songFor (id, seed);
            const LaneOut& ch = laneOf (s, "chords");
            std::map<std::int64_t, std::pair<std::int64_t, std::int64_t>> stacks; // score tick -> min, max perf tick
            for (std::size_t i = 0; i < ch.score.size(); ++i)
            {
                auto [it, fresh] = stacks.emplace (ch.score[i].tick, std::make_pair (ch.perf[i].tick, ch.perf[i].tick));
                if (!fresh)
                {
                    it->second.first = std::min (it->second.first, ch.perf[i].tick);
                    it->second.second = std::max (it->second.second, ch.perf[i].tick);
                }
            }
            CHECK (stacks.size() > 50);
            for (const auto& [tick, mm] : stacks)
            {
                INFO (id << " seed " << seed << " tick " << tick);
                CHECK (mm.second - mm.first >= 0);
                CHECK (mm.second - mm.first <= 4);
            }
        }
}

TEST_CASE ("each lane swings its odd 16ths by its own amount", "[gen][feel]")
{
    for (const char* id : { "FUT01", "FUT03" })
        for (std::uint64_t seed = 1; seed <= 32; ++seed)
        {
            INFO (id << " seed " << seed);
            const Song s = songFor (id, seed);
            std::set<std::int32_t> swings;
            for (const auto& lane : s.lanes)
                if (lane.id != "kick" && lane.id != "snare")
                    swings.insert (lane.swing);
            CHECK (swings.size() >= 3); // G8: at least 3 distinct swings among the non-anchor lanes

            // the hats: the mean late-ness of odd 16ths over even ones, less the offset map, is the
            // lane's swing delay (the 1/f part averages out over both)
            const LaneOut& hats = laneOf (s, "hats");
            const FeelLane drop = bundleFor (id).feel.resolve ("hats", "drop");
            std::vector<double> odd, even;
            for (std::size_t i = 0; i < hats.perf.size(); ++i)
            {
                const Note& p = hats.perf[i];
                const Section& sec = s.sections[static_cast<std::size_t> (p.section)];
                if (sec.kind != "drop" && sec.kind != "groove")
                    continue; // rows with other swings
                if (p.tick == sec.startTick())
                    continue; // clamped to its section's start
                const std::int32_t map = drop.offset ? (*drop.offset)[static_cast<std::size_t> (p.step % 32)] : 0;
                (p.step % 2 ? odd : even).push_back (static_cast<double> (p.tick - p.step * ticksPerStep - map));
            }
            REQUIRE (odd.size() > 100);
            CHECK (std::abs ((mean (odd) - mean (even)) - static_cast<double> (swingDelay (hats.swing))) < 3.0);
            const std::int32_t base = bundleFor (id).kit.swing;
            CHECK (hats.swing >= base + drop.swing->lo);
            CHECK (hats.swing <= base + drop.swing->hi);
        }
}

TEST_CASE ("hat microtiming is 1/f: correlated residuals with a moderate spread", "[gen][feel]")
{
    // A proxy for one property, on the hats track alone; the gate itself (G7, which reads the hats
    // drum group, the shaker included) runs in test_gen_gates.cpp.
    for (const char* id : { "FUT01", "FUT03" })
        for (std::uint64_t seed = 1; seed <= 4; ++seed)
        {
            INFO (id << " seed " << seed);
            const Song s = songFor (id, seed);
            const LaneOut& hats = laneOf (s, "hats");
            // residual: performed minus score tick, less the mean of its step position (swing and offset map)
            std::map<std::int64_t, std::vector<double>> byPos;
            std::vector<std::pair<std::int64_t, double>> raw;
            for (const Note& p : hats.perf)
            {
                const Section& sec = s.sections[static_cast<std::size_t> (p.section)];
                if (sec.kind != "drop" || p.tick == sec.startTick())
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
            REQUIRE (resid.size() > 200);
            const double s1 = sd (resid);
            CHECK (s1 > 1.5); // G7: residual SD 1.5-6 ticks
            CHECK (s1 < 6.0);
            CHECK (autocorr (resid, 1) > 0.2); // G7: lag-1 autocorrelation at least 0.2
        }
}
