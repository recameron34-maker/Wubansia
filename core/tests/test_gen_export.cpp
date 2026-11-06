// Tests for core/gen/export: the full arrangement file, the region files, regions.json, and the
// files on disk. Every file is read back with core/midi's reader.

#include "gen/content.h"
#include "gen/export.h"
#include "gen/json.h"
#include "gen/song.h"
#include "midi/reader.h"
#include "midi/writer.h"
#include "util/file_io.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

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
using Catch::Matchers::ContainsSubstring;

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

    Song songFor (const std::string& id, std::uint64_t seed, std::int32_t bars = 0)
    {
        Options o;
        o.seed = seed;
        o.bars = bars;
        auto s = generate (bundleFor (id), o);
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

    // A track's channel events at absolute ticks, moved by offset.
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
}

TEST_CASE ("the full file is SMF type 1 with a conductor and one track per lane", "[gen][export]")
{
    for (const char* id : { "FUT01", "FUT03" })
        for (const Version v : { Version::feel, Version::quantized })
        {
            INFO (id << " " << versionName (v));
            const Bundle& b = bundleFor (id);
            const Song s = songFor (id, 7);
            const auto bytes = midi::writeSmf (fullSpec (s, v));
            REQUIRE (bytes.ok());
            const midi::File f = read (bytes.value);
            CHECK (f.format == 1);
            CHECK (f.ppq == 480);
            REQUIRE (f.tracks.size() == 1 + b.style.lanes.size());
            for (const auto& t : f.tracks)
                CHECK (t.endTick == s.totalBars * 1920);
            const auto tempos = midi::tempoMap (f);
            REQUIRE (tempos.size() == 1);
            CHECK (tempos[0].tick == 0);
            CHECK (tempos[0].microsPerQuarter == midi::microsPerQuarter (b.kit.bpm));
            const auto ts = midi::timeSignaturesOf (f);
            REQUIRE (ts.size() == 1);
            CHECK (ts[0].numerator == 4);
            CHECK (ts[0].denominatorPow2 == 2);
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
            // the kind first, as the metrics read it (tools/legacy_audit/README.md, "Sections")
            CHECK (markers.front().text == "intro (01, 32 bars)");
            for (const auto& m : markers)
                CHECK (std::find (sectionKinds.begin(), sectionKinds.end(), m.text.substr (0, m.text.find (' '))) != sectionKinds.end());
            CHECK_THAT (midi::trackName (f.tracks[0]), ContainsSubstring (b.kit.id));
            // pitched tracks are named so their first word gives their role (tools/legacy_audit/README.md, "Lanes and roles")
            const auto firstWord = [] (std::string_view name) {
                std::string w;
                for (const char c : name)
                {
                    if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')))
                        break;
                    w += static_cast<char> (c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
                }
                return w;
            };
            const std::set<std::string> bassWords = { "bass", "sub", "reese", "acid", "donk" };
            const std::set<std::string> chordWords = { "chords", "stabs", "stab", "pad", "organ", "strings", "piano" };
            CHECK (bassWords.contains (firstWord (b.kit.lane ("bass")->track)));
            CHECK (chordWords.contains (firstWord (b.kit.lane ("chords")->track)));
            for (std::size_t l = 0; l < b.style.lanes.size(); ++l)
            {
                const KitLane* kl = b.kit.lane (b.style.lanes[l].id);
                CHECK (midi::trackName (f.tracks[l + 1]) == kl->track);
                for (const auto& n : midi::notesOf (f.tracks[l + 1]))
                    CHECK (n.channel == kl->channel);
            }
        }
}

TEST_CASE ("region files are named by order kind and bars", "[gen][export]")
{
    const Song s = songFor ("FUT01", 7);
    REQUIRE (s.sections.size() >= 4);
    CHECK (regionFileName (s.sections[0]) == "01_intro_32bars.mid");
    CHECK (regionFileName (s.sections[1]) == "02_groove_32bars.mid");
    CHECK (regionFileName (s.sections[2]) == "03_build_8bars.mid");
    CHECK (regionFileName (s.sections[3]) == "04_drop_32bars.mid");
    CHECK (fullFileName (s) == "FUT01_seed7_134bpm_11A_full.mid");
    CHECK (fullFileName (songFor ("FUT03", 12)) == "FUT03_seed12_128bpm_6A_full.mid");
}

TEST_CASE ("the region files tile the full file exactly", "[gen][export]")
{
    for (const char* id : { "FUT01", "FUT03" })
        for (std::uint64_t seed : { 2ull, 31ull })
            for (const Version v : { Version::feel, Version::quantized })
            {
                INFO (id << " seed " << seed << " " << versionName (v));
                const Song s = songFor (id, seed);
                const auto files = renderFiles (s, v);
                REQUIRE (files.ok());
                const midi::File full = read (fileNamed (files.value, fullFileName (s)).bytes);
                std::vector<std::vector<midi::Event>> joined (full.tracks.size());
                std::vector<midi::Marker> markers;
                for (const Section& sec : s.sections)
                {
                    const midi::File region = read (fileNamed (files.value, "regions/" + regionFileName (sec)).bytes);
                    REQUIRE (region.tracks.size() == full.tracks.size());
                    for (std::size_t t = 0; t < region.tracks.size(); ++t)
                    {
                        CHECK (region.tracks[t].endTick == sec.bars * 1920);
                        CHECK (midi::trackName (region.tracks[t]).empty() == midi::trackName (full.tracks[t]).empty());
                        if (t > 0)
                            CHECK (midi::trackName (region.tracks[t]) == midi::trackName (full.tracks[t]));
                        const auto ev = channelEvents (region.tracks[t], sec.startTick());
                        joined[t].insert (joined[t].end(), ev.begin(), ev.end());
                    }
                    CHECK (midi::tempoMap (region) == midi::tempoMap (full));
                    CHECK (midi::keySignaturesOf (region) == midi::keySignaturesOf (full));
                    for (auto m : midi::markersOf (region))
                    {
                        m.tick += sec.startTick();
                        markers.push_back (m);
                    }
                }
                for (std::size_t t = 0; t < full.tracks.size(); ++t)
                {
                    INFO ("track " << t);
                    CHECK (joined[t] == channelEvents (full.tracks[t], 0));
                }
                CHECK (markers == midi::markersOf (full));
            }
}

TEST_CASE ("regions.json lists each region's file start length kind and lanes", "[gen][export]")
{
    const Song s = songFor ("FUT01", 7);
    const auto parsed = json::parse (regionsJson (s, Version::feel));
    REQUIRE (parsed.ok());
    json::Errors errors ("regions.json");
    const json::Node root = json::root (parsed.value, errors);
    CHECK (root["format"].int32 (1, 1) == 1);
    CHECK (root["kit"].string() == "FUT01");
    CHECK (root["name"].string() == "Alloy Signal");
    CHECK (root["seed"].string() == "7"); // a string: seeds go up to 2^64 - 1
    CHECK (root["version"].string() == "feel");
    CHECK (root["bpm"].int32 (1, 999) == 134);
    CHECK (root["key"].string() == "F# minor");
    CHECK (root["camelot"].string() == "11A");
    CHECK (root["ppq"].int32 (1, 9999) == 480);
    CHECK (root["bars"].int32 (1, 9999) == s.totalBars);
    CHECK (root["full"].string() == fullFileName (s));
    const json::Node lanes = root["lanes"];
    REQUIRE (lanes.size() == s.lanes.size());
    CHECK (lanes.at (0)["id"].string() == "kick");
    CHECK (lanes.at (0)["track"].string() == "Kick");
    const json::Node regions = root["regions"];
    REQUIRE (regions.size() == s.sections.size());
    for (std::size_t i = 0; i < s.sections.size(); ++i)
    {
        const Section& sec = s.sections[i];
        const json::Node r = regions.at (i);
        CHECK (r["order"].int32 (1, 99) == static_cast<std::int32_t> (i + 1));
        CHECK (r["file"].string() == "regions/" + regionFileName (sec));
        CHECK (r["kind"].string() == sec.kind);
        CHECK (r["startBar"].int32 (0, 9999) == sec.startBar);
        CHECK (r["dawBar"].int32 (1, 9999) == sec.startBar + 1);
        CHECK (r["bars"].int32 (1, 9999) == sec.bars);
        CHECK (r["startTick"].integer (0, 1 << 30) == sec.startTick());
        const json::Node blocks = r["blocks"];
        REQUIRE (blocks.size() == sec.blocks.size());
        std::int32_t tiled = 0; // the blocks tile the region, each with its bar as the DAW counts it
        for (std::size_t k = 0; k < sec.blocks.size(); ++k)
        {
            CHECK (blocks.at (k)["startBar"].int32 (0, 9999) == sec.startBar + static_cast<std::int32_t> (k) * 8);
            CHECK (blocks.at (k)["startBar"].int32 (0, 9999) == sec.startBar + tiled);
            CHECK (blocks.at (k)["dawBar"].int32 (1, 9999) == blocks.at (k)["startBar"].int32 (0, 9999) + 1);
            CHECK (blocks.at (k)["bars"].int32 (1, 8) == 8);
            tiled += blocks.at (k)["bars"].int32 (1, 8);
            const auto active = blocks.at (k)["lanes"].strings (0, 16);
            std::vector<std::string> expected;
            for (std::size_t l = 0; l < s.lanes.size(); ++l)
                if (sec.blocks[k] & (std::uint64_t (1) << l))
                    expected.push_back (s.lanes[l].id);
            CHECK (active == expected);
        }
        CHECK (tiled == sec.bars);
        const auto anywhere = r["lanes"].strings (0, 16);
        CHECK_FALSE (anywhere.empty());
    }
    CHECK (regions.at (0)["blocks"].at (0)["lanes"].strings (0, 16) == std::vector<std::string> { "kick", "hats" });
    INFO (errors.first());
    CHECK_FALSE (errors.any());
    CHECK (regionsJson (s, Version::quantized).find ("\"version\": \"quantized\"") != std::string::npos);
}

TEST_CASE ("quantized files sit on the 16th grid and feel files don't", "[gen][export]")
{
    const Song s = songFor ("FUT01", 4);
    const midi::File q = read (midi::writeSmf (fullSpec (s, Version::quantized)).value);
    const midi::File f = read (midi::writeSmf (fullSpec (s, Version::feel)).value);
    REQUIRE (q.tracks.size() == f.tracks.size());
    for (std::size_t t = 1; t < q.tracks.size(); ++t)
    {
        const auto qn = midi::notesOf (q.tracks[t]);
        const auto fn = midi::notesOf (f.tracks[t]);
        CHECK (qn.size() == fn.size()); // the same notes, played
        for (const auto& n : qn)
            CHECK (n.tick % 120 == 0);
        std::size_t off = 0;
        for (const auto& n : fn)
            off += n.tick % 120 != 0 ? 1u : 0u;
        if (s.lanes[t - 1].id == "hats")
            CHECK (off * 2 > fn.size());
    }
}

TEST_CASE ("cutting at a section line gives exactly the first bars of the whole song", "[gen][export]")
{
    for (const char* id : { "FUT01", "FUT03" })
    {
        const Song full = songFor (id, 21);
        const std::int32_t cutBars = full.sections[0].bars + full.sections[1].bars;
        const Song cut = songFor (id, 21, cutBars);
        REQUIRE (cut.totalBars == cutBars);
        const std::int64_t cutTick = cutBars * 1920;
        for (const Version v : { Version::feel, Version::quantized })
        {
            const midi::File a = read (midi::writeSmf (fullSpec (full, v)).value);
            const midi::File b = read (midi::writeSmf (fullSpec (cut, v)).value);
            for (std::size_t t = 1; t < a.tracks.size(); ++t)
            {
                INFO (id << " track " << t << " " << versionName (v));
                // the notes that start before the cut (a note may end on it: no note crosses a section line)
                std::vector<midi::Note> head;
                for (const auto& n : midi::notesOf (a.tracks[t]))
                    if (n.tick < cutTick)
                    {
                        CHECK (n.tick + n.length <= cutTick);
                        head.push_back (n);
                    }
                CHECK (midi::notesOf (b.tracks[t]) == head);
            }
        }
        // and mid-section, everything that ends a beat before the cut is the same
        const std::int32_t midBars = full.sections[0].bars + 16;
        const Song mid = songFor (id, 21, midBars);
        for (std::size_t l = 0; l < full.lanes.size(); ++l)
        {
            std::vector<Note> head, got;
            for (const Note& n : full.lanes[l].perf)
                if (n.tick + n.length <= midBars * 1920 - 480)
                    head.push_back (n);
            for (const Note& n : mid.lanes[l].perf)
                if (n.tick + n.length <= midBars * 1920 - 480)
                    got.push_back (n);
            INFO (id << " lane " << full.lanes[l].id);
            CHECK (got == head);
        }
    }
}

TEST_CASE ("writeFiles writes both versions' trees", "[gen][export]")
{
    const std::filesystem::path out = std::filesystem::path (WUBANSIA_TEST_TMP_DIR) / "gen_write";
    std::error_code ec;
    std::filesystem::remove_all (out, ec);
    const Song s = songFor ("FUT03", 3);
    const auto written = writeFiles (s, out, true, true);
    REQUIRE (written.ok());
    for (const Version v : { Version::feel, Version::quantized })
    {
        const auto files = renderFiles (s, v);
        REQUIRE (files.ok());
        for (const auto& f : files.value)
        {
            const auto p = out / std::string (versionName (v)) / f.path;
            INFO (p.string());
            REQUIRE (std::filesystem::exists (p));
            const auto bytes = util::readBinaryFile (p);
            REQUIRE (bytes.ok());
            CHECK (bytes.value == f.bytes);
        }
        CHECK (std::filesystem::exists (out / std::string (versionName (v)) / "regions.json"));
    }
    CHECK (written.value.size() == 2 * (s.sections.size() + 2));

    const std::filesystem::path only = std::filesystem::path (WUBANSIA_TEST_TMP_DIR) / "gen_write_quantized";
    std::filesystem::remove_all (only, ec);
    REQUIRE (writeFiles (s, only, false, true).ok());
    CHECK_FALSE (std::filesystem::exists (only / "feel"));
    CHECK (std::filesystem::exists (only / "quantized" / "regions.json"));
}
