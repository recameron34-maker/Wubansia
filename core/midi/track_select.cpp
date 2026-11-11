#include "midi/track_select.h"

#include "midi/reader.h"

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
}
