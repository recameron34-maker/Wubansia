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
