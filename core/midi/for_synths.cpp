#include "midi/for_synths.h"

#include "midi/reader.h"
#include "midi/track_select.h"
#include "midi/writer.h"

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>

namespace wubansia::midi
{
    namespace
    {
        constexpr int kOctave = 12;

        // Whether toFile () lays the track out with everything but its notes as it is: notes, controllers, program
        // changes, text events and one track name, not empty, at tick 0. Not pitch bend, key or channel pressure, sysex,
        // or another meta event.
        bool carried (const Track& track)
        {
            int names = 0;
            for (const auto& e : track.events)
            {
                if (e.isChannel())
                {
                    if (e.kind() == status::polyPressure || e.kind() == status::channelPressure || e.kind() == status::pitchBend)
                        return false;
                    continue;
                }
                if (! e.isMeta() || ! meta::isText (e.data1))
                    return false;
                if (e.data1 == meta::trackName && (++names > 1 || e.tick != 0 || e.payload.empty()))
                    return false;
            }
            return true;
        }

        // The track's MTrk chunk made for synths, or nothing when it can't be carried as it is.
        std::optional<std::vector<std::uint8_t>> rewritten (const Track& track, std::uint16_t ppq, bool bass)
        {
            if (! carried (track))
                return std::nullopt;
            TrackSpec spec;
            spec.name = trackName (track);
            spec.notes = synthNotes (notesOf (track), ppq, track.endTick, bass);
            spec.controllers = controllersOf (track);
            spec.programs = programsOf (track);
            for (auto& text : textsOf (track))
                if (text.type != meta::trackName)
                    spec.texts.push_back (std::move (text));

            // Laid out and written by the SMF writer, as mergeTracks () writes its track: the chunk is what follows
            // encode ()'s MThd.
            SongSpec song;
            song.ppq = ppq;
            song.endTick = track.endTick;
            song.tracks.push_back (std::move (spec));
            auto laidOut = toFile (song);
            if (! laidOut.ok())
                return std::nullopt;
            File one;
            one.format = 1;
            one.ppq = ppq;
            one.tracks.push_back (std::move (laidOut.value.tracks[1]));
            auto written = encode (one);
            if (! written.ok())
                return std::nullopt;
            constexpr std::ptrdiff_t mthdSize = 14;
            return std::vector<std::uint8_t> (written.value.begin() + mthdSize, written.value.end());
        }
    }

    std::vector<Note> synthNotes (std::vector<Note> notes, std::uint16_t ppq, std::int64_t endTick, bool bass)
    {
        const std::int64_t sixteenth = std::max<std::int64_t> (1, ppq / 4);
        const std::int64_t gap = synthGap (ppq);

        // Where the track's notes start, each once: a note's next note is the first of these after it.
        std::vector<std::int64_t> onsets;
        onsets.reserve (notes.size());
        for (const auto& n : notes)
            onsets.push_back (n.tick);
        std::sort (onsets.begin(), onsets.end());
        onsets.erase (std::unique (onsets.begin(), onsets.end()), onsets.end());

        const bool up = bass && std::all_of (notes.begin(), notes.end(), [] (const Note& n) { return n.key + kOctave <= 127; });
        for (auto& n : notes)
        {
            auto end = std::max (n.tick + n.length, n.tick + sixteenth);
            if (const auto next = std::upper_bound (onsets.begin(), onsets.end(), n.tick); next != onsets.end())
                end = std::min (end, *next - gap);
            end = std::min (end, endTick);
            n.length = std::max<std::int64_t> (1, end - n.tick);
            n.velocity = std::max (n.velocity, kSynthVelocityFloor);
            if (up)
                n.key = static_cast<std::uint8_t> (n.key + kOctave);
        }
        trimOverlaps (notes); // notes on one tick, channel and key: one note
        return notes;
    }

    util::Result<std::vector<std::uint8_t>> forSynths (std::span<const std::uint8_t> smf, std::span<const SynthPart> parts)
    {
        using R = util::Result<std::vector<std::uint8_t>>;
        auto file = readSmf (smf);
        if (! file.ok())
            return R::failure (std::move (file.error));
        auto chunks = trackChunks (smf);
        if (! chunks.ok())
            return R::failure (std::move (chunks.error));
        const auto& tracks = chunks.value;
        if (parts.size() > tracks.size())
            return R::failure ("parts for " + std::to_string (parts.size()) + " tracks, but the file has " + std::to_string (tracks.size()));

        // The header chunk as it is (readSmf has checked its length), then every track in file order.
        const std::size_t headerSize = 8 + ((std::size_t { smf[4] } << 24) | (std::size_t { smf[5] } << 16) | (std::size_t { smf[6] } << 8) | std::size_t { smf[7] });
        std::vector<std::uint8_t> out (smf.begin(), smf.begin() + static_cast<std::ptrdiff_t> (headerSize));
        for (std::size_t i = 0; i < tracks.size(); ++i)
        {
            const auto part = i < parts.size() ? parts[i] : SynthPart::asIs;
            if (part != SynthPart::asIs)
                if (const auto chunk = rewritten (file.value.tracks[i], file.value.ppq, part == SynthPart::bass))
                {
                    out.insert (out.end(), chunk->begin(), chunk->end());
                    continue;
                }
            const auto begin = smf.begin() + static_cast<std::ptrdiff_t> (tracks[i].offset);
            out.insert (out.end(), begin, begin + static_cast<std::ptrdiff_t> (tracks[i].size));
        }
        return R::success (std::move (out));
    }
}
