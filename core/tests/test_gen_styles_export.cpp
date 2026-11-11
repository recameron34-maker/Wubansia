// Export tests for the sci-fi club (FUT02) and techno (FUT04) songs: the full file and the region
// files, now with the lanes' CC74 automation, which both versions carry and the regions tile.

#include "gen/content.h"
#include "gen/export.h"
#include "gen/json.h"
#include "gen/song.h"
#include "midi/reader.h"
#include "midi/writer.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
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

    Song songFor (std::string_view id, std::uint64_t seed, std::int32_t bars = 0)
    {
        Options o;
        o.seed = seed;
        o.bars = bars;
        auto s = generate (bundleFor (id), o);
        INFO (s.error);
        REQUIRE (s.ok());
        return std::move (s.value);
    }

    midi::File read (const std::vector<std::uint8_t>& bytes)
    {
        const auto f = midi::readSmf (bytes);
        REQUIRE (f.ok());
        return f.value;
    }

    const OutputFile& fileNamed (const std::vector<OutputFile>& files, std::string_view path)
    {
        for (const auto& f : files)
            if (f.path == path)
                return f;
        FAIL ("no file " << path);
        return files.front();
    }

    std::vector<midi::Event> channelEvents (const midi::Track& t, std::int64_t offset)
    {
        std::vector<midi::Event> out;
        for (const auto& e : t.events)
            if (e.isChannel())
            {
                midi::Event m = e;
                m.tick += offset;
                out.push_back (m);
            }
        return out;
    }

    std::vector<midi::Event> ccEventsOf (const midi::Track& t)
    {
        std::vector<midi::Event> out;
        for (const auto& e : t.events)
            if (e.isController())
                out.push_back (e);
        return out;
    }

    // The notes a version's file holds of one lane from `from` to `to` in the whole song's head: the
    // score's onsets in [from, to), with their performed notes in the feel version, ended by `to` (a
    // performed onset at or after it plays on its last tick), at ticks from `from`, in the reader's order.
    std::vector<midi::Note> headNotes (const LaneOut& lane, Version v, std::int64_t from, std::int64_t to)
    {
        std::vector<midi::Note> out;
        for (std::size_t i = 0; i < lane.score.size(); ++i)
        {
            if (lane.score[i].tick < from || lane.score[i].tick >= to)
                continue;
            const Note& n = v == Version::feel ? lane.perf[i] : lane.score[i];
            const std::int64_t tick = std::min (n.tick, to - 1);
            midi::Note m;
            m.tick = tick - from;
            m.length = std::max<std::int64_t> (1, std::min (n.tick + n.length, to) - tick);
            m.channel = lane.channel;
            m.key = n.key;
            m.velocity = n.velocity;
            out.push_back (m);
        }
        std::stable_sort (out.begin(), out.end(), [] (const midi::Note& a, const midi::Note& b) { return std::tie (a.tick, a.channel, a.key) < std::tie (b.tick, b.channel, b.key); });
        return out;
    }

    // The first difference between two files' note lists, or "" where they are the same.
    std::string noteDifference (const std::vector<midi::Note>& got, const std::vector<midi::Note>& want)
    {
        const auto shown = [] (const midi::Note& n) {
            return "tick " + std::to_string (n.tick) + " length " + std::to_string (n.length) + " key " + std::to_string (n.key) + " velocity " + std::to_string (n.velocity);
        };
        for (std::size_t i = 0; i < std::min (got.size(), want.size()); ++i)
            if (!(got[i] == want[i]))
                return "note " + std::to_string (i) + " is " + shown (got[i]) + ", the head's " + shown (want[i]);
        if (got.size() != want.size())
            return std::to_string (got.size()) + " notes, the head " + std::to_string (want.size());
        return {};
    }

    // The path of the first difference between two JSON values ("" if none), line and column aside.
    std::string jsonDifference (const json::Value& a, const json::Value& b, const std::string& path)
    {
        if (a.type != b.type || a.boolean != b.boolean || a.integer != b.integer || a.string != b.string || a.items.size() != b.items.size()
            || a.members.size() != b.members.size())
            return path.empty() ? "the document" : path;
        for (std::size_t i = 0; i < a.items.size(); ++i)
            if (auto d = jsonDifference (a.items[i], b.items[i], path + "[" + std::to_string (i) + "]"); !d.empty())
                return d;
        for (std::size_t i = 0; i < a.members.size(); ++i)
        {
            if (a.members[i].key != b.members[i].key)
                return path + "." + a.members[i].key;
            if (auto d = jsonDifference (a.members[i].value, b.members[i].value, path + "." + a.members[i].key); !d.empty())
                return d;
        }
        return {};
    }

    json::Value& memberOf (json::Value& v, std::string_view key)
    {
        for (auto& m : v.members)
            if (m.key == key)
                return m.value;
        FAIL ("no member " << key);
        return v;
    }

    json::Value parsedJson (const std::vector<std::uint8_t>& bytes)
    {
        auto parsed = json::parse (std::string_view (reinterpret_cast<const char*> (bytes.data()), bytes.size()));
        INFO (parsed.error);
        REQUIRE (parsed.ok());
        return std::move (parsed.value);
    }

    std::string twoDigits (std::size_t n) { return (n < 10 ? "0" : "") + std::to_string (n); }
}

TEST_CASE ("styles: the full file is SMF type 1 with a conductor and one named track per lane", "[gen][styles][export]")
{
    for (const char* id : kits)
        for (const Version v : { Version::feel, Version::quantized })
        {
            INFO (id << " " << versionName (v));
            const Bundle& b = bundleFor (id);
            const Song s = songFor (id, 7);
            const midi::File f = read (midi::writeSmf (fullSpec (s, v)).value);
            CHECK (f.format == 1);
            CHECK (f.ppq == 480);
            REQUIRE (f.tracks.size() == 1 + b.style.lanes.size());
            for (const auto& t : f.tracks)
                CHECK (t.endTick == s.totalBars * 1920);
            const auto tempos = midi::tempoMap (f);
            REQUIRE (tempos.size() == 1);
            CHECK (tempos[0].microsPerQuarter == midi::microsPerQuarter (b.kit.bpm));
            const auto ks = midi::keySignaturesOf (f);
            REQUIRE (ks.size() == 1);
            CHECK (ks[0].sharps == midi::sharpsFor (b.kit.tonic, true));
            CHECK (ks[0].minor);
            const auto markers = midi::markersOf (f);
            REQUIRE (markers.size() == s.sections.size());
            for (std::size_t i = 0; i < markers.size(); ++i)
            {
                CHECK (markers[i].tick == s.sections[i].startTick());
                CHECK (markers[i].text == sectionMarker (s.sections[i]));
            }
            for (std::size_t t = 0; t < s.lanes.size(); ++t)
            {
                CHECK (midi::trackName (f.tracks[t + 1]) == s.lanes[t].track);
                for (const auto& e : f.tracks[t + 1].events)
                    if (e.isChannel())
                        CHECK (e.channel() == s.lanes[t].channel);
            }
            CHECK (fullFileName (s) == std::string (id) + "_seed7_" + std::to_string (b.kit.bpm) + "bpm_" + b.kit.camelot + "_full.mid");
        }
}

TEST_CASE ("styles: the region files tile the full file exactly, controllers included", "[gen][styles][export]")
{
    for (const char* id : kits)
        for (std::uint64_t seed : { 2ull, 31ull })
            for (const Version v : { Version::feel, Version::quantized })
            {
                INFO (id << " seed " << seed << " " << versionName (v));
                const Song s = songFor (id, seed);
                const auto files = renderFiles (s, v);
                REQUIRE (files.ok());
                CHECK (files.value.size() == s.sections.size() + 2);
                const midi::File full = read (fileNamed (files.value, fullFileName (s)).bytes);
                std::vector<std::vector<midi::Event>> joined (full.tracks.size());
                for (const Section& sec : s.sections)
                {
                    const midi::File region = read (fileNamed (files.value, "regions/" + regionFileName (sec)).bytes);
                    REQUIRE (region.tracks.size() == full.tracks.size());
                    for (std::size_t t = 0; t < region.tracks.size(); ++t)
                    {
                        CHECK (region.tracks[t].endTick == sec.bars * 1920);
                        const auto ev = channelEvents (region.tracks[t], sec.startTick());
                        joined[t].insert (joined[t].end(), ev.begin(), ev.end());
                    }
                }
                std::size_t controllers = 0;
                for (std::size_t t = 0; t < full.tracks.size(); ++t)
                {
                    INFO ("track " << t);
                    CHECK (joined[t] == channelEvents (full.tracks[t], 0));
                    controllers += ccEventsOf (full.tracks[t]).size();
                }
                CHECK (controllers > 100); // the filter arcs are in the files
            }
}

TEST_CASE ("styles: both versions carry the same controllers, from the lanes' CC lanes", "[gen][styles][export]")
{
    for (const char* id : kits)
    {
        const Song s = songFor (id, 11);
        const midi::File q = read (midi::writeSmf (fullSpec (s, Version::quantized)).value);
        const midi::File f = read (midi::writeSmf (fullSpec (s, Version::feel)).value);
        for (std::size_t t = 0; t < s.lanes.size(); ++t)
        {
            INFO (id << " " << s.lanes[t].id);
            const auto qc = ccEventsOf (q.tracks[t + 1]);
            CHECK (qc == ccEventsOf (f.tracks[t + 1]));
            REQUIRE (qc.size() == s.lanes[t].cc.size());
            for (std::size_t i = 0; i < qc.size(); ++i)
            {
                CHECK (qc[i].tick == s.lanes[t].cc[i].tick);
                CHECK (qc[i].data1 == s.lanes[t].cc[i].number);
                CHECK (qc[i].data2 == s.lanes[t].cc[i].value);
                CHECK (qc[i].channel() == s.lanes[t].channel);
            }
        }
        // the acid (FUT04) and the starfield pad (FUT02) carry CC74
        const std::string lane = std::string (id) == "FUT04" ? "bass" : "chords";
        for (std::size_t t = 0; t < s.lanes.size(); ++t)
            if (s.lanes[t].id == lane)
            {
                INFO (id << " " << lane);
                CHECK_FALSE (s.lanes[t].cc.empty());
                for (const auto& c : s.lanes[t].cc)
                    CHECK (c.number == 74);
            }
    }
}

TEST_CASE ("styles: regions.json lists the new lanes and every region", "[gen][styles][export]")
{
    for (const char* id : kits)
    {
        INFO (id);
        const Song s = songFor (id, 7);
        const auto parsed = json::parse (regionsJson (s, Version::quantized));
        REQUIRE (parsed.ok());
        json::Errors errors ("regions.json");
        const json::Node root = json::root (parsed.value, errors);
        CHECK (root["kit"].string() == id);
        CHECK (root["bars"].int32 (1, 9999) == s.totalBars);
        const json::Node lanes = root["lanes"];
        REQUIRE (lanes.size() == s.lanes.size());
        for (std::size_t l = 0; l < s.lanes.size(); ++l)
        {
            CHECK (lanes.at (l)["id"].string() == s.lanes[l].id);
            CHECK (lanes.at (l)["track"].string() == s.lanes[l].track);
            CHECK (lanes.at (l)["channel"].int32 (1, 16) == s.lanes[l].channel + 1);
        }
        const json::Node regions = root["regions"];
        REQUIRE (regions.size() == s.sections.size());
        for (std::size_t i = 0; i < s.sections.size(); ++i)
        {
            CHECK (regions.at (i)["file"].string() == "regions/" + regionFileName (s.sections[i]));
            CHECK (regions.at (i)["bars"].int32 (1, 9999) == s.sections[i].bars);
        }
        INFO (errors.first());
        CHECK_FALSE (errors.any());
    }
}

TEST_CASE ("styles: regions.json gives the song's length and each region's ticks and lanes, in all four kits", "[gen][styles][export]")
{
    // The fields a producer reads off regions.json, pinned by more than the snapshot hashes: the
    // song's length (m:ss of its bars at the kit's tempo: each seed draws its form, 200 to 240 bars,
    // 5:52 to 7:30 at the kits' tempos), each region's length in ticks, and its lanes, the union of
    // its 8-bar blocks' lanes in track order.
    for (const char* id : { "FUT01", "FUT02", "FUT03", "FUT04" })
    {
        INFO (id);
        const Song s = songFor (id, 3);
        CHECK (s.totalBars >= 200);
        CHECK (s.totalBars <= 240);
        const std::int32_t seconds = s.totalBars * 240 / s.bpm; // 4 beats a bar, rounded down
        const std::string length = std::to_string (seconds / 60) + ":" + (seconds % 60 < 10 ? "0" : "") + std::to_string (seconds % 60);
        CHECK (seconds >= 5 * 60 + 52);
        CHECK (seconds <= 7 * 60 + 30);
        for (const Version v : { Version::feel, Version::quantized })
        {
            const auto parsed = json::parse (regionsJson (s, v));
            REQUIRE (parsed.ok());
            json::Errors errors ("regions.json");
            const json::Node root = json::root (parsed.value, errors);
            CHECK (root["length"].string() == length);
            const json::Node regions = root["regions"];
            REQUIRE (regions.size() == s.sections.size());
            for (std::size_t i = 0; i < s.sections.size(); ++i)
            {
                const Section& sec = s.sections[i];
                const json::Node r = regions.at (i);
                INFO ("region " << i + 1 << ", " << sec.kind);
                CHECK (r["lengthTicks"].integer (0, 1 << 30) == static_cast<std::int64_t> (sec.bars) * 1920);
                std::vector<std::string> lanes;
                for (std::size_t l = 0; l < s.lanes.size(); ++l)
                    for (const auto block : sec.blocks)
                        if (block & (std::uint64_t (1) << l))
                        {
                            lanes.push_back (s.lanes[l].id);
                            break;
                        }
                CHECK (r["lanes"].strings (0, 32) == lanes);
                const json::Node blocks = r["blocks"];
                REQUIRE (blocks.size() == sec.blocks.size());
                std::vector<std::string> fromBlocks;
                for (std::size_t k = 0; k < sec.blocks.size(); ++k)
                    for (const auto& lane : blocks.at (k)["lanes"].strings (0, 32))
                        if (std::find (fromBlocks.begin(), fromBlocks.end(), lane) == fromBlocks.end())
                            fromBlocks.push_back (lane);
                std::vector<std::string> ordered;
                for (const auto& l : s.lanes)
                    if (std::find (fromBlocks.begin(), fromBlocks.end(), l.id) != fromBlocks.end())
                        ordered.push_back (l.id);
                CHECK (r["lanes"].strings (0, 32) == ordered);
            }
            INFO (errors.first());
            CHECK_FALSE (errors.any());
        }
    }
}

TEST_CASE ("styles: a cut's files are the whole song's regions, the last one shortened", "[gen][styles][export]")
{
    // What groovecore gen --bars writes for a cut 8 bars into a section (cli/tests/cut.cmake checks the
    // same through groovecore): the whole song's regions.json and region files, but for the region
    // the cut ends, shortened to it, and a full file and last region that hold the whole song's head.
    std::size_t cuts = 0;
    for (const char* id : { "FUT01", "FUT02", "FUT03", "FUT04" })
        for (const std::uint64_t seed : { 5ull, 31ull })
        {
            const Song whole = songFor (id, seed);
            for (const Section& sec : whole.sections)
            {
                if (sec.bars <= 8)
                    continue;
                const std::int32_t bars = sec.startBar + 8;
                const std::int64_t end = static_cast<std::int64_t> (bars) * 1920;
                const Song cut = songFor (id, seed, bars);
                const std::size_t kept = sec.index + 1; // the regions the cut keeps
                const std::string oldMarker = sectionMarker (sec);
                const std::string newMarker = sec.kind + " (" + twoDigits (kept) + ", 8 bars)";
                const std::string lastRegion = "regions/" + twoDigits (kept) + "_" + sec.kind + "_8bars.mid";
                ++cuts;
                for (const Version v : { Version::feel, Version::quantized })
                {
                    INFO (id << " seed " << seed << " cut at bar " << bars << ", 8 bars into the " << sec.kind << ", " << versionName (v));
                    const auto all = renderFiles (whole, v);
                    const auto files = renderFiles (cut, v);
                    REQUIRE (all.ok());
                    REQUIRE (files.ok());
                    REQUIRE (files.value.size() == kept + 2); // the full file, the regions, regions.json
                    // the regions before the last: the whole song's, byte for byte
                    for (std::size_t i = 0; i + 1 < kept; ++i)
                    {
                        const std::string path = "regions/" + regionFileName (whole.sections[i]);
                        CHECK (fileNamed (files.value, path).bytes == fileNamed (all.value, path).bytes);
                    }
                    CHECK (files.value[kept].path == lastRegion);
                    // regions.json: the whole song's, with the cut's length and its last region shortened
                    json::Value want = parsedJson (fileNamed (all.value, "regions.json").bytes);
                    const std::int32_t seconds = bars * 240 / whole.bpm;
                    memberOf (want, "bars").integer = bars;
                    memberOf (want, "length").string = std::to_string (seconds / 60) + ":" + twoDigits (static_cast<std::size_t> (seconds % 60));
                    auto& regions = memberOf (want, "regions").items;
                    REQUIRE (regions.size() >= kept);
                    regions.resize (kept);
                    memberOf (regions.back(), "file").string = lastRegion;
                    memberOf (regions.back(), "bars").integer = 8;
                    memberOf (regions.back(), "lengthTicks").integer = 8 * 1920;
                    memberOf (regions.back(), "blocks").items.resize (1);
                    // and none of its one-bar dropouts, which come before a 16-bar line of the section, after its first 8 bars
                    for (auto& m : regions.back().members)
                        if (m.key == "dropouts")
                            m.value.items.clear();
                    auto& lanes = memberOf (regions.back(), "lanes").items; // the lanes of the block it keeps
                    lanes.clear();
                    for (std::size_t l = 0; l < whole.lanes.size(); ++l)
                        if (sec.blocks[0] & (std::uint64_t (1) << l))
                        {
                            json::Value laneId;
                            laneId.type = json::Value::Type::string;
                            laneId.string = whole.lanes[l].id;
                            lanes.push_back (laneId);
                        }
                    CHECK (jsonDifference (parsedJson (fileNamed (files.value, "regions.json").bytes), want, "") == "");
                    // the full file and the last region: the whole song's head, the last marker naming
                    // the shortened region
                    struct Pair
                    {
                        std::string got, whole;
                        std::int64_t from;
                    };
                    for (const Pair& p : { Pair { fullFileName (cut), fullFileName (whole), 0 }, Pair { lastRegion, "regions/" + regionFileName (sec), sec.startTick() } })
                    {
                        INFO (p.got);
                        const midi::File got = read (fileNamed (files.value, p.got).bytes);
                        const midi::File was = read (fileNamed (all.value, p.whole).bytes);
                        REQUIRE (got.tracks.size() == was.tracks.size());
                        for (std::size_t t = 0; t < got.tracks.size(); ++t)
                        {
                            INFO ("track " << t);
                            CHECK (got.tracks[t].endTick == end - p.from);
                            std::string name = midi::trackName (was.tracks[t]);
                            if (const auto at = name.find (oldMarker); t == 0 && at != std::string::npos)
                                name.replace (at, oldMarker.size(), newMarker);
                            CHECK (midi::trackName (got.tracks[t]) == name);
                            std::vector<midi::Controller> cc;
                            for (const auto& c : midi::controllersOf (was.tracks[t]))
                                if (c.tick < end - p.from)
                                    cc.push_back (c);
                            CHECK (midi::controllersOf (got.tracks[t]) == cc);
                            if (t > 0)
                                CHECK (noteDifference (midi::notesOf (got.tracks[t]), headNotes (whole.lanes[t - 1], v, p.from, end)) == "");
                        }
                        std::vector<midi::Marker> markers;
                        for (const auto& m : midi::markersOf (was))
                            if (m.tick < end - p.from)
                                markers.push_back (m);
                        REQUIRE_FALSE (markers.empty());
                        CHECK (markers.back().text == oldMarker);
                        markers.back().text = newMarker;
                        CHECK (midi::markersOf (got) == markers);
                        CHECK (midi::tempoMap (got) == midi::tempoMap (was));
                        CHECK (midi::keySignaturesOf (got) == midi::keySignaturesOf (was));
                    }
                }
            }
        }
    CHECK (cuts > 40);
}

TEST_CASE ("styles: quantized files sit on the 16th grid and feel files don't", "[gen][styles][export]")
{
    for (const char* id : kits)
    {
        const Song s = songFor (id, 4);
        const midi::File q = read (midi::writeSmf (fullSpec (s, Version::quantized)).value);
        const midi::File f = read (midi::writeSmf (fullSpec (s, Version::feel)).value);
        for (std::size_t t = 1; t < q.tracks.size(); ++t)
        {
            INFO (id << " " << s.lanes[t - 1].id);
            const auto qn = midi::notesOf (q.tracks[t]);
            const auto fn = midi::notesOf (f.tracks[t]);
            CHECK (qn.size() == fn.size());
            for (const auto& n : qn)
                CHECK (n.tick % 120 == 0);
            std::size_t off = 0;
            for (const auto& n : fn)
                off += n.tick % 120 != 0 ? 1u : 0u;
            if (s.lanes[t - 1].id == "hats")
                CHECK (off * 2 > fn.size());
            if (s.lanes[t - 1].id == "rumble")
                CHECK (off * 10 <= fn.size() * 9); // locked to a kick that sits within 2 ticks of the grid
        }
    }
}

TEST_CASE ("styles: writeFiles writes both versions' trees for the new kits", "[gen][styles][export]")
{
    for (const char* id : kits)
    {
        const std::filesystem::path out = std::filesystem::path (WUBANSIA_TEST_TMP_DIR) / ("styles_write_" + std::string (id));
        std::error_code ec;
        std::filesystem::remove_all (out, ec);
        const Song s = songFor (id, 3);
        const auto written = writeFiles (s, out, true, true);
        REQUIRE (written.ok());
        CHECK (written.value.size() == 2 * (s.sections.size() + 2));
        for (const auto& p : written.value)
            CHECK (std::filesystem::exists (p));
    }
}
