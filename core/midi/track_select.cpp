#include "midi/track_select.h"

#include "midi/reader.h"
#include "midi/writer.h"
#include "util/utf8.h"

#include <algorithm>
#include <utility>

namespace wubansia::midi
{
    namespace
    {
        using Bytes = std::span<const std::uint8_t>;

        std::uint32_t be32 (Bytes b, std::size_t at)
        {
            return (std::uint32_t { b[at] } << 24) | (std::uint32_t { b[at + 1] } << 16) | (std::uint32_t { b[at + 2] } << 8) | std::uint32_t { b[at + 3] };
        }

        bool isMTrk (Bytes b, std::size_t at)
        {
            return b[at] == 'M' && b[at + 1] == 'T' && b[at + 2] == 'r' && b[at + 3] == 'k';
        }

        // The file as readSmf reads it: its parsed tracks, the size of its header chunk, and where each
        // track's chunk lies, found the way readSmf walks the chunks (the first trackCount MTrk chunks
        // after the header, other chunks skipped).
        struct Layout
        {
            File file;
            std::size_t headerSize = 0;
            std::vector<std::pair<std::size_t, std::size_t>> chunks; // (offset, size) per track
        };

        util::Result<Layout> layoutOf (Bytes smf)
        {
            using R = util::Result<Layout>;
            auto read = readSmf (smf);
            if (!read.ok())
                return R::failure (std::move (read.error));

            Layout layout;
            layout.file = std::move (read.value);
            // readSmf has checked every length below against the file's size; the checks here only
            // keep this walk safe on its own.
            layout.headerSize = 8 + std::size_t { be32 (smf, 4) };
            std::size_t pos = layout.headerSize;
            while (layout.chunks.size() < layout.file.tracks.size())
            {
                if (pos > smf.size() || smf.size() - pos < 8)
                    return R::failure ("a track chunk runs past the end of the file");
                const std::size_t size = 8 + std::size_t { be32 (smf, pos + 4) };
                if (smf.size() - pos < size)
                    return R::failure ("a track chunk runs past the end of the file");
                if (isMTrk (smf, pos))
                    layout.chunks.emplace_back (pos, size);
                pos += size;
            }
            return R::success (std::move (layout));
        }

        std::string hex (std::uint8_t v)
        {
            constexpr char digits[] = "0123456789ABCDEF";
            std::string s = "0x";
            s += digits[(v >> 4) & 0xF];
            s += digits[v & 0xF];
            return s;
        }

        // "track 2 \"Clap\"", as the writer names tracks; a track with no name, or one that isn't UTF-8, is "track 2".
        std::string trackLabel (std::size_t index, const Track& track)
        {
            auto label = "track " + std::to_string (index);
            if (const auto name = trackName (track); !name.empty() && util::isValidUtf8 (name))
                label += " \"" + name + "\"";
            return label;
        }

        // What a merged track can't carry as it is, or "".
        std::string uncarried (const Event& e)
        {
            if (e.isChannel())
                switch (e.kind())
                {
                    case status::polyPressure: return "a key pressure";
                    case status::channelPressure: return "a channel pressure";
                    case status::pitchBend: return "a pitch bend";
                    default: return {}; // notes, controllers and program changes
                }
            if (e.isSysex())
                return "a sysex event";
            if (e.isMeta() && !meta::isText (e.data1))
                return "a meta event of type " + hex (e.data1);
            return {};
        }

        // Checks that every note-on of a track is ended by a note-off before the track ends, and that every note-off
        // ends a note, pairing them as notesOf () does (the oldest sounding note on the channel and key ends first).
        // Returns what is wrong, or "".
        std::string unpairedNote (const Track& track)
        {
            constexpr std::size_t keys = 128;
            std::vector<std::vector<std::int64_t>> sounding (16 * keys); // per channel and key: note-on ticks, oldest first
            const auto where = [] (std::int64_t tick, std::size_t slot) {
                return " at tick " + std::to_string (tick) + " (key " + std::to_string (slot % keys) + ", channel " + std::to_string (slot / keys) + ")";
            };
            for (const auto& e : track.events)
            {
                if (!e.isNoteOn() && !e.isNoteOff())
                    continue;
                const auto slot = std::size_t { e.channel() } * keys + e.data1;
                auto& open = sounding[slot];
                if (e.isNoteOn())
                    open.push_back (e.tick);
                else if (open.empty())
                    return "the note-off" + where (e.tick, slot) + " has no note to end";
                else
                    open.erase (open.begin());
            }

            // The earliest note left sounding (on a tie, the lowest channel, then key), if any.
            std::size_t unended = sounding.size();
            for (std::size_t slot = 0; slot < sounding.size(); ++slot)
                if (!sounding[slot].empty() && (unended == sounding.size() || sounding[slot].front() < sounding[unended].front()))
                    unended = slot;
            if (unended < sounding.size())
                return "the note-on" + where (sounding[unended].front(), unended) + " has no note-off before the track ends";
            return {};
        }
    }

    util::Result<std::vector<TrackChunk>> trackChunks (std::span<const std::uint8_t> smf)
    {
        using R = util::Result<std::vector<TrackChunk>>;
        auto layout = layoutOf (smf);
        if (!layout.ok())
            return R::failure (std::move (layout.error));

        std::vector<TrackChunk> out;
        out.reserve (layout.value.chunks.size());
        for (std::size_t i = 0; i < layout.value.chunks.size(); ++i)
            out.push_back ({ layout.value.chunks[i].first, layout.value.chunks[i].second, trackName (layout.value.file.tracks[i]) });
        return R::success (std::move (out));
    }

    util::Result<std::vector<std::uint8_t>> keepTracks (std::span<const std::uint8_t> smf, std::span<const int> trackIndices)
    {
        using R = util::Result<std::vector<std::uint8_t>>;
        auto layout = layoutOf (smf);
        if (!layout.ok())
            return R::failure (std::move (layout.error));
        const auto& chunks = layout.value.chunks;
        if (chunks.empty())
            return R::failure ("the file has no tracks, so no conductor to keep");

        std::vector<bool> kept (chunks.size(), false);
        kept[0] = true;
        for (const int index : trackIndices)
        {
            if (index < 0 || static_cast<std::size_t> (index) >= chunks.size())
                return R::failure ("track " + std::to_string (index) + " isn't in the file (" + std::to_string (chunks.size()) + " tracks)");
            kept[static_cast<std::size_t> (index)] = true;
        }

        std::size_t count = 0;
        std::size_t size = layout.value.headerSize;
        for (std::size_t i = 0; i < chunks.size(); ++i)
            if (kept[i])
            {
                ++count;
                size += chunks[i].second;
            }

        std::vector<std::uint8_t> out;
        out.reserve (size);
        out.insert (out.end(), smf.begin(), smf.begin() + static_cast<std::ptrdiff_t> (layout.value.headerSize));
        out[10] = static_cast<std::uint8_t> ((count >> 8) & 0xFF); // count <= the source's, which fits 16 bits
        out[11] = static_cast<std::uint8_t> (count & 0xFF);
        for (std::size_t i = 0; i < chunks.size(); ++i)
            if (kept[i])
            {
                const auto begin = smf.begin() + static_cast<std::ptrdiff_t> (chunks[i].first);
                out.insert (out.end(), begin, begin + static_cast<std::ptrdiff_t> (chunks[i].second));
            }
        return R::success (std::move (out));
    }

    util::Result<std::vector<std::uint8_t>> mergeTracks (std::span<const std::uint8_t> smf, std::span<const int> trackIndices,
        std::span<const int> mergeIndices, std::string_view name)
    {
        using R = util::Result<std::vector<std::uint8_t>>;

        // The tracks to merge, distinct and ascending; every message names them.
        std::vector<int> merging (mergeIndices.begin(), mergeIndices.end());
        std::sort (merging.begin(), merging.end());
        merging.erase (std::unique (merging.begin(), merging.end()), merging.end());
        std::string prefix = "can't merge tracks";
        for (std::size_t i = 0; i < merging.size(); ++i)
            prefix += (i == 0 ? " " : ", ") + std::to_string (merging[i]);
        prefix += ": ";
        const auto fail = [&prefix] (const std::string& why) { return R::failure (prefix + why); };

        auto layout = layoutOf (smf);
        if (!layout.ok())
            return fail (layout.error);
        const auto& file = layout.value.file;
        const auto& chunks = layout.value.chunks;
        if (chunks.empty())
            return fail ("the file has no tracks, so no conductor to keep");

        // What's kept: the conductor, trackIndices and the tracks to merge.
        std::vector<bool> kept (chunks.size(), false);
        kept[0] = true;
        const auto keep = [&] (int index) {
            if (index < 0 || static_cast<std::size_t> (index) >= chunks.size())
                return false;
            kept[static_cast<std::size_t> (index)] = true;
            return true;
        };
        const auto notInFile = [&] (int index) {
            return fail ("track " + std::to_string (index) + " isn't in the file (" + std::to_string (chunks.size()) + " tracks)");
        };
        for (const int index : trackIndices)
            if (!keep (index))
                return notInFile (index);
        for (const int index : merging)
            if (!keep (index))
                return notInFile (index);
        if (!merging.empty() && merging.front() == 0)
            return fail ("track 0, the conductor, is never merged");
        if (merging.size() < 2)
            return fail ("it takes two tracks or more");

        // The merged track: every merged track's notes, controllers, program changes and text events, each track's
        // own name left out; it ends on the latest end among them.
        TrackSpec merged;
        merged.name = std::string (name);
        std::int64_t endTick = 0;
        for (const int index : merging)
        {
            const auto& track = file.tracks[static_cast<std::size_t> (index)];
            const auto label = trackLabel (static_cast<std::size_t> (index), track);
            for (const auto& e : track.events)
                if (const auto what = uncarried (e); ! what.empty())
                    return fail (label + " has " + what + " at tick " + std::to_string (e.tick)
                                 + "; a merged track carries only notes, controllers, program changes and text events");
            if (const auto problem = unpairedNote (track); ! problem.empty())
                return fail (label + ": " + problem);

            const auto notes = notesOf (track);
            const auto controllers = controllersOf (track);
            const auto programs = programsOf (track);
            merged.notes.insert (merged.notes.end(), notes.begin(), notes.end());
            merged.controllers.insert (merged.controllers.end(), controllers.begin(), controllers.end());
            merged.programs.insert (merged.programs.end(), programs.begin(), programs.end());
            for (auto& text : textsOf (track))
                if (text.type != meta::trackName)
                    merged.texts.push_back (std::move (text));
            endTick = std::max (endTick, track.endTick);
        }

        // Laid out and written by the SMF writer, in its fixed order: the chunk is what follows encode ()'s MThd.
        SongSpec spec;
        spec.ppq = file.ppq;
        spec.endTick = endTick;
        spec.tracks.push_back (std::move (merged));
        auto laidOut = toFile (spec);
        if (!laidOut.ok())
            return fail (laidOut.error);
        File one;
        one.format = 1;
        one.ppq = file.ppq;
        one.tracks.push_back (std::move (laidOut.value.tracks[1]));
        auto written = encode (one);
        if (!written.ok())
            return fail (written.error);
        constexpr std::size_t mthdSize = 14;
        const auto mergedChunk = std::span<const std::uint8_t> (written.value).subspan (mthdSize);

        // The header with its count, then the chunks in file order: the merged one where the first merged track was.
        std::vector<bool> isMerged (chunks.size(), false);
        for (const int index : merging)
            isMerged[static_cast<std::size_t> (index)] = true;
        std::size_t count = 0;
        std::size_t size = layout.value.headerSize + mergedChunk.size();
        for (std::size_t i = 0; i < chunks.size(); ++i)
            if (kept[i] && !isMerged[i])
            {
                ++count;
                size += chunks[i].second;
            }
        ++count; // the merged track; count <= the source's, which fits 16 bits

        std::vector<std::uint8_t> out;
        out.reserve (size);
        out.insert (out.end(), smf.begin(), smf.begin() + static_cast<std::ptrdiff_t> (layout.value.headerSize));
        out[10] = static_cast<std::uint8_t> ((count >> 8) & 0xFF);
        out[11] = static_cast<std::uint8_t> (count & 0xFF);
        const auto first = static_cast<std::size_t> (merging.front());
        for (std::size_t i = 0; i < chunks.size(); ++i)
        {
            if (i == first)
                out.insert (out.end(), mergedChunk.begin(), mergedChunk.end());
            else if (kept[i] && !isMerged[i])
            {
                const auto begin = smf.begin() + static_cast<std::ptrdiff_t> (chunks[i].first);
                out.insert (out.end(), begin, begin + static_cast<std::ptrdiff_t> (chunks[i].second));
            }
        }
        return R::success (std::move (out));
    }
}
