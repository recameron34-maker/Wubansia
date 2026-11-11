#include "gen/export.h"

#include "gen/json.h"
#include "midi/smf.h"
#include "util/file_io.h"

#include <algorithm>
#include <cstdint>
#include <set>
#include <string>
#include <system_error>

namespace wubansia::gen
{
    namespace
    {
        std::string twoDigits (std::size_t n) { return (n < 10 ? "0" : "") + std::to_string (n); }

        std::string jsonString (std::string_view s)
        {
            static constexpr char hex[] = "0123456789abcdef";
            std::string out = "\"";
            for (const char c : s)
            {
                const auto u = static_cast<unsigned char> (c);
                if (c == '"' || c == '\\')
                {
                    out += '\\';
                    out += c;
                }
                else if (u < 0x20)
                {
                    out += "\\u00";
                    out += hex[u >> 4];
                    out += hex[u & 15];
                }
                else
                    out += c;
            }
            return out + "\"";
        }

        std::string songName (const Song& song, Version version)
        {
            return song.kitId + " " + song.kitName + ", seed " + std::to_string (song.seed) + ", extended mix (" + std::string (versionName (version)) + ")";
        }

        void addConductor (midi::SongSpec& spec, const Song& song)
        {
            spec.ppq = midi::defaultPpq;
            spec.tempos = { { 0, midi::microsPerQuarter (song.bpm) } };
            spec.timeSignatures = { { 0, 4, 2, 24, 8 } };
            spec.keySignatures = { { 0, midi::sharpsFor (song.tonic, song.minor), song.minor } };
        }

        midi::TrackSpec trackOf (const LaneOut& lane, Version version, std::int64_t from, std::int64_t to)
        {
            midi::TrackSpec t;
            t.name = lane.track;
            const auto& notes = version == Version::feel ? lane.perf : lane.score;
            for (const Note& n : notes)
            {
                if (n.tick < from || n.tick >= to)
                    continue;
                midi::Note m;
                m.tick = n.tick - from;
                m.length = n.length;
                m.channel = lane.channel;
                m.key = n.key;
                m.velocity = n.velocity;
                t.notes.push_back (m);
            }
            midi::trimOverlaps (t.notes); // belt and braces: the generator already keeps keys apart
            // the lane's controllers (cc.h): automation, the same in both versions
            for (const ControlPoint& c : lane.cc)
                if (c.tick >= from && c.tick < to)
                    t.controllers.push_back ({ c.tick - from, lane.channel, c.number, c.value });
            return t;
        }

        std::string minutes (std::int32_t seconds) { return std::to_string (seconds / 60) + ":" + twoDigits (static_cast<std::size_t> (seconds % 60)); }
    }

    std::string_view versionName (Version version) noexcept { return version == Version::feel ? "feel" : "quantized"; }

    std::string regionFileName (const Section& section)
    {
        return twoDigits (section.index + 1) + "_" + section.kind + "_" + std::to_string (section.bars) + "bars.mid";
    }

    std::string fullFileName (const Song& song)
    {
        return song.kitId + "_seed" + std::to_string (song.seed) + "_" + std::to_string (song.bpm) + "bpm_" + song.camelot + "_full.mid";
    }

    std::string sectionMarker (const Section& section)
    {
        return section.kind + " (" + twoDigits (section.index + 1) + ", " + std::to_string (section.bars) + " bars)";
    }

    midi::SongSpec fullSpec (const Song& song, Version version)
    {
        midi::SongSpec spec;
        addConductor (spec, song);
        spec.endTick = song.endTick();
        spec.name = songName (song, version);
        for (const Section& s : song.sections)
            spec.markers.push_back ({ s.startTick(), sectionMarker (s) });
        for (const LaneOut& lane : song.lanes)
            spec.tracks.push_back (trackOf (lane, version, 0, song.endTick()));
        return spec;
    }

    midi::SongSpec regionSpec (const Song& song, Version version, std::size_t section)
    {
        const Section& s = song.sections[section];
        midi::SongSpec spec;
        addConductor (spec, song);
        spec.endTick = s.endTick() - s.startTick();
        spec.name = songName (song, version) + ", " + sectionMarker (s);
        spec.markers = { { 0, sectionMarker (s) } };
        for (const LaneOut& lane : song.lanes)
            spec.tracks.push_back (trackOf (lane, version, s.startTick(), s.endTick()));
        return spec;
    }

    std::string regionsJson (const Song& song, Version version)
    {
        const auto laneList = [&song] (std::uint64_t mask) {
            std::string out = "[";
            bool first = true;
            for (std::size_t l = 0; l < song.lanes.size(); ++l)
                if (mask & (std::uint64_t (1) << l))
                {
                    out += (first ? "" : ", ") + jsonString (song.lanes[l].id);
                    first = false;
                }
            return out + "]";
        };
        const std::int32_t seconds = song.totalBars * 240 / song.bpm;
        std::string j = "{\n";
        j += "  \"format\": 1,\n";
        j += "  \"kit\": " + jsonString (song.kitId) + ",\n";
        j += "  \"name\": " + jsonString (song.kitName) + ",\n";
        j += "  \"seed\": " + jsonString (std::to_string (song.seed)) + ",\n";
        j += "  \"version\": " + jsonString (versionName (version)) + ",\n";
        j += "  \"bpm\": " + std::to_string (song.bpm) + ",\n";
        j += "  \"key\": " + jsonString (song.keyName) + ",\n";
        j += "  \"camelot\": " + jsonString (song.camelot) + ",\n";
        j += "  \"ppq\": " + std::to_string (midi::defaultPpq) + ",\n";
        j += "  \"bars\": " + std::to_string (song.totalBars) + ",\n";
        j += "  \"length\": " + jsonString (minutes (seconds)) + ",\n";
        j += "  \"full\": " + jsonString (fullFileName (song)) + ",\n";
        j += "  \"lanes\": [\n";
        for (std::size_t l = 0; l < song.lanes.size(); ++l)
            j += "    { \"id\": " + jsonString (song.lanes[l].id) + ", \"track\": " + jsonString (song.lanes[l].track)
                 + ", \"channel\": " + std::to_string (song.lanes[l].channel + 1) + " }" + (l + 1 < song.lanes.size() ? "," : "") + "\n";
        j += "  ],\n";
        j += "  \"regions\": [\n";
        for (std::size_t i = 0; i < song.sections.size(); ++i)
        {
            const Section& s = song.sections[i];
            j += "    {\n";
            j += "      \"order\": " + std::to_string (i + 1) + ",\n";
            j += "      \"file\": " + jsonString ("regions/" + regionFileName (s)) + ",\n";
            j += "      \"kind\": " + jsonString (s.kind) + ",\n";
            j += "      \"startBar\": " + std::to_string (s.startBar) + ",\n";
            j += "      \"dawBar\": " + std::to_string (s.startBar + 1) + ",\n";
            j += "      \"bars\": " + std::to_string (s.bars) + ",\n";
            j += "      \"startTick\": " + std::to_string (s.startTick()) + ",\n";
            j += "      \"lengthTicks\": " + std::to_string (s.endTick() - s.startTick()) + ",\n";
            j += "      \"lanes\": " + laneList (s.lanesAnywhere()) + ",\n";
            j += "      \"blocks\": [\n";
            for (std::size_t k = 0; k < s.blocks.size(); ++k)
            {
                const std::int32_t blockBar = s.startBar + static_cast<std::int32_t> (k) * barsPerBlock;
                j += "        { \"startBar\": " + std::to_string (blockBar) + ", \"dawBar\": " + std::to_string (blockBar + 1) + ", \"bars\": "
                     + std::to_string (barsPerBlock) + ", \"lanes\": " + laneList (s.blocks[k]) + " }" + (k + 1 < s.blocks.size() ? "," : "") + "\n";
            }
            j += "      ],\n";
            // the one-bar dropouts inside it (arrange.h): lanes of a block that sit one bar out
            j += std::string ("      \"dropouts\": [") + (s.dropouts.empty() ? "" : "\n");
            for (std::size_t k = 0; k < s.dropouts.size(); ++k)
            {
                const std::int32_t bar = s.startBar + s.dropouts[k].bar;
                j += "        { \"startBar\": " + std::to_string (bar) + ", \"dawBar\": " + std::to_string (bar + 1) + ", \"bars\": 1, \"lanes\": "
                     + laneList (s.dropouts[k].lanes) + " }" + (k + 1 < s.dropouts.size() ? "," : "") + "\n";
            }
            j += std::string (s.dropouts.empty() ? "" : "      ") + "]\n";
            j += std::string ("    }") + (i + 1 < song.sections.size() ? "," : "") + "\n";
        }
        j += "  ]\n";
        j += "}\n";
        return j;
    }

    util::Result<std::vector<OutputFile>> renderFiles (const Song& song, Version version)
    {
        using R = util::Result<std::vector<OutputFile>>;
        std::vector<OutputFile> files;
        auto full = midi::writeSmf (fullSpec (song, version));
        if (!full.ok())
            return R::failure ("the full file: " + full.error);
        files.push_back ({ fullFileName (song), std::move (full.value) });
        for (std::size_t i = 0; i < song.sections.size(); ++i)
        {
            auto region = midi::writeSmf (regionSpec (song, version, i));
            if (!region.ok())
                return R::failure (regionFileName (song.sections[i]) + ": " + region.error);
            files.push_back ({ "regions/" + regionFileName (song.sections[i]), std::move (region.value) });
        }
        const std::string j = regionsJson (song, version);
        files.push_back ({ "regions.json", std::vector<std::uint8_t> (j.begin(), j.end()) });
        return R::success (std::move (files));
    }

    util::Result<std::vector<std::filesystem::path>> writeFiles (const Song& song, const std::filesystem::path& outDir, bool feel, bool quantized)
    {
        using R = util::Result<std::vector<std::filesystem::path>>;
        const auto shownPath = [] (const std::filesystem::path& p) {
            const auto s = p.generic_u8string();
            return std::string (s.begin(), s.end());
        };

        const auto lowered = [] (std::string s) {
            for (char& c : s)
                c = (c >= 'A' && c <= 'Z') ? static_cast<char> (c - 'A' + 'a') : c;
            return s;
        };
        const std::string advice = ". Write each song to its own --out folder, or empty that folder first.";

        // First, before anything is written: the folder must hold no other song. Region names carry
        // no seed (03_drop_32bars.mid) and every seed of a kit has the same ones, so another song's
        // files left there would look like part of this one. In both version folders, written or
        // not (one --out folder is one song): no full file but this song's, and a regions.json only
        // of this song (its kit and seed). Nothing is ever deleted.
        const std::string full = fullFileName (song);
        for (const Version v : { Version::feel, Version::quantized })
        {
            const auto dir = outDir / std::string (versionName (v));
            std::error_code ec;
            if (!std::filesystem::is_directory (dir, ec))
                continue;
            std::vector<std::string> others;
            for (std::filesystem::directory_iterator it (dir, ec), end; !ec && it != end; it.increment (ec))
            {
                const auto name = shownPath (it->path().filename());
                const std::string low = lowered (name);
                const bool fullFile = low.ends_with ("_full.mid") || low.ends_with ("_full.midi");
                if (fullFile && name != full && !it->is_directory (ec))
                    others.push_back (name);
            }
            if (ec)
                return R::failure ("cannot read " + shownPath (dir) + ": " + ec.message());
            if (!others.empty())
            {
                std::stable_sort (others.begin(), others.end()); // the folder's order is the file system's
                return R::failure (shownPath (dir) + " already holds " + others.front() + ", another song's full arrangement"
                                   + (others.size() > 1 ? " (and " + std::to_string (others.size() - 1) + " more)" : std::string()) + advice);
            }
        }
        for (const Version v : { Version::feel, Version::quantized })
        {
            const auto listing = outDir / std::string (versionName (v)) / "regions.json";
            std::error_code ec;
            if (!std::filesystem::exists (listing, ec))
                continue;
            const auto bytes = util::readBinaryFile (listing);
            std::string why;
            std::string kit, seed;
            if (!bytes.ok())
                why = bytes.error;
            else
            {
                const auto parsed = json::parse (std::string_view (reinterpret_cast<const char*> (bytes.value.data()), bytes.value.size()));
                const json::Value* k = parsed.ok() ? parsed.value.find ("kit") : nullptr;
                const json::Value* n = parsed.ok() ? parsed.value.find ("seed") : nullptr;
                if (!parsed.ok())
                    why = parsed.error;
                else if (k == nullptr || n == nullptr || k->type != json::Value::Type::string || n->type != json::Value::Type::string)
                    why = "it names no kit and seed";
                else
                {
                    kit = k->string;
                    seed = n->string;
                }
            }
            if (!why.empty())
                return R::failure (shownPath (listing) + " can't be read (" + why + "), so the folder may hold another song" + advice);
            if (kit != song.kitId || seed != std::to_string (song.seed))
                return R::failure (shownPath (listing) + " belongs to another song: " + kit + " seed " + seed + advice);
        }

        // And each regions/ folder being written must hold no MIDI file this song doesn't write: the
        // same seed cut with --bars, or a file someone put there.
        std::set<std::string> ours;
        for (const Section& s : song.sections)
            ours.insert (regionFileName (s));
        for (const Version v : { Version::feel, Version::quantized })
        {
            if ((v == Version::feel && !feel) || (v == Version::quantized && !quantized))
                continue;
            const auto regions = outDir / std::string (versionName (v)) / "regions";
            std::error_code ec;
            if (!std::filesystem::is_directory (regions, ec))
                continue;
            std::vector<std::string> stale;
            for (std::filesystem::directory_iterator it (regions, ec), end; !ec && it != end; it.increment (ec))
            {
                const auto name = shownPath (it->path().filename());
                std::string ext = shownPath (it->path().extension());
                for (char& c : ext)
                    c = (c >= 'A' && c <= 'Z') ? static_cast<char> (c - 'A' + 'a') : c;
                if ((ext == ".mid" || ext == ".midi") && !ours.contains (name))
                    stale.push_back (name);
            }
            if (ec)
                return R::failure ("cannot read " + shownPath (regions) + ": " + ec.message());
            if (!stale.empty())
            {
                std::stable_sort (stale.begin(), stale.end()); // the folder's order is the file system's
                return R::failure (shownPath (regions) + " already holds " + std::to_string (stale.size())
                                   + " region file(s) this song doesn't write, from another song: " + stale.front()
                                   + (stale.size() > 1 ? " and " + std::to_string (stale.size() - 1) + " more" : std::string()) + advice);
            }
        }

        std::vector<std::filesystem::path> written;
        for (const Version v : { Version::feel, Version::quantized })
        {
            if ((v == Version::feel && !feel) || (v == Version::quantized && !quantized))
                continue;
            auto files = renderFiles (song, v);
            if (!files.ok())
                return R::failure (files.error);
            const auto dir = outDir / std::string (versionName (v));
            std::error_code ec;
            std::filesystem::create_directories (dir / "regions", ec);
            if (ec)
            {
                const auto shown = (dir / "regions").generic_u8string();
                return R::failure ("cannot create " + std::string (shown.begin(), shown.end()) + ": " + ec.message());
            }
            for (const auto& f : files.value)
            {
                const auto path = dir / std::filesystem::path (f.path);
                const auto w = util::writeBinaryFile (path, f.bytes);
                if (!w.ok())
                    return R::failure (w.error);
                written.push_back (path);
            }
        }
        return R::success (std::move (written));
    }
}
