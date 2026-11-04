#include "midi/reader.h"

#include <algorithm>
#include <cstddef>
#include <tuple>
#include <utility>

namespace wubansia::midi
{
    namespace
    {
        using Bytes = std::span<const std::uint8_t>;

        std::string str (std::uint64_t v) { return std::to_string (v); }

        std::string hex (std::uint8_t v)
        {
            constexpr char digits[] = "0123456789ABCDEF";
            std::string s = "0x";
            s += digits[(v >> 4) & 0xF];
            s += digits[v & 0xF];
            return s;
        }

        std::uint32_t be32 (Bytes b, std::size_t at)
        {
            return (std::uint32_t { b[at] } << 24) | (std::uint32_t { b[at + 1] } << 16) | (std::uint32_t { b[at + 2] } << 8) | std::uint32_t { b[at + 3] };
        }

        std::uint16_t be16 (Bytes b, std::size_t at)
        {
            return static_cast<std::uint16_t> ((b[at] << 8) | b[at + 1]);
        }

        bool isId (Bytes b, std::size_t at, const char (&id)[5])
        {
            for (std::size_t i = 0; i < 4; ++i)
                if (b[at + i] != static_cast<std::uint8_t> (id[i]))
                    return false;
            return true;
        }

        enum class VarLen {
            ok,
            truncated,
            tooLong,
        };

        // A variable-length quantity at p, at most 4 bytes; p moves past it.
        VarLen readVarLen (Bytes d, std::size_t& p, std::uint32_t& value)
        {
            value = 0;
            for (int i = 0; i < 4; ++i)
            {
                if (p >= d.size())
                    return VarLen::truncated;
                const std::uint8_t b = d[p++];
                value = (value << 7) | (b & 0x7Fu);
                if ((b & 0x80u) == 0)
                    return VarLen::ok;
            }
            return VarLen::tooLong;
        }

        // One MTrk chunk's data. index is the track's number in the file and base the offset of
        // the data in the file, for the error messages.
        util::Result<Track> readTrack (Bytes d, std::size_t index, std::size_t base)
        {
            using R = util::Result<Track>;
            const auto fail = [index, base] (std::size_t at, const std::string& message) {
                return R::failure ("track " + str (index) + ", byte " + str (base + at) + ": " + message);
            };

            Track track;
            std::size_t p = 0;
            std::int64_t tick = 0; // at most about 2^58: each delta of up to 2^28 takes 2 bytes or more of a chunk below 2^32
            std::uint8_t running = 0;

            // Every pass reads at least one byte, so the loop ends within d.size () passes.
            while (p < d.size())
            {
                const std::size_t start = p;
                std::uint32_t delta = 0;
                switch (readVarLen (d, p, delta))
                {
                    case VarLen::ok:
                        break;
                    case VarLen::truncated:
                        return fail (start, "truncated: the track ends inside a delta time");
                    case VarLen::tooLong:
                        return fail (start, "a variable-length number longer than 4 bytes");
                }
                tick += delta;

                if (p >= d.size())
                    return fail (p, "truncated: the track ends after a delta time");
                std::uint8_t st = d[p];
                if (st < 0x80)
                {
                    if (running == 0)
                        return fail (p, "data byte " + hex (st) + " with no running status");
                    st = running; // running status: the byte is the first data byte
                }
                else
                    ++p;

                if (st < 0xF0)
                {
                    const bool oneDataByte = (st & 0xF0) == status::programChange || (st & 0xF0) == status::channelPressure;
                    const std::size_t need = oneDataByte ? 1 : 2;
                    if (d.size() - p < need)
                        return fail (p, "truncated: the track ends inside a channel message");
                    for (std::size_t k = 0; k < need; ++k)
                        if (d[p + k] > 0x7F)
                            return fail (p + k, "expected a data byte, found " + hex (d[p + k]));
                    const std::uint8_t d1 = d[p];
                    const std::uint8_t d2 = oneDataByte ? std::uint8_t { 0 } : d[p + 1];
                    p += need;
                    running = st;
                    track.events.push_back ({ tick, st, d1, d2, {} });
                    continue;
                }

                if (st == status::meta || st == status::sysex || st == status::sysexEscape)
                {
                    std::uint8_t type = 0;
                    if (st == status::meta)
                    {
                        if (p >= d.size())
                            return fail (p, "truncated: the track ends inside a meta event");
                        type = d[p];
                        if (type > 0x7F)
                            return fail (p, "meta type " + hex (type) + " is above 0x7F");
                        ++p;
                    }
                    const std::size_t lengthAt = p;
                    std::uint32_t length = 0;
                    switch (readVarLen (d, p, length))
                    {
                        case VarLen::ok:
                            break;
                        case VarLen::truncated:
                            return fail (lengthAt, "truncated: the track ends inside an event's length");
                        case VarLen::tooLong:
                            return fail (lengthAt, "a variable-length number longer than 4 bytes");
                    }
                    if (d.size() - p < length)
                        return fail (p, "truncated: an event of " + str (length) + " bytes with " + str (d.size() - p) + " left in the track");

                    if (st == status::meta && type == meta::endOfTrack)
                    {
                        track.endTick = tick; // anything after it in the chunk is ignored
                        return R::success (std::move (track));
                    }
                    track.events.push_back ({ tick, st, type, 0, std::vector<std::uint8_t> (d.begin() + static_cast<std::ptrdiff_t> (p), d.begin() + static_cast<std::ptrdiff_t> (p + length)) });
                    p += length;
                    continue; // running status carries on: see reader.h
                }

                return fail (p - 1, "status " + hex (st) + " isn't allowed in a MIDI file");
            }

            return R::failure ("track " + str (index) + " has no end-of-track event");
        }

        std::string textOf (const Event& e)
        {
            return std::string (e.payload.begin(), e.payload.end());
        }

        // Every meta event of one type across a file's tracks, by tick; on one tick, track order and
        // then file order.
        std::vector<const Event*> metasOf (const File& file, std::uint8_t type)
        {
            std::vector<const Event*> found;
            for (const auto& t : file.tracks)
                for (const auto& e : t.events)
                    if (e.isMeta (type))
                        found.push_back (&e);
            std::stable_sort (found.begin(), found.end(), [] (const Event* a, const Event* b) { return a->tick < b->tick; });
            return found;
        }
    }

    util::Result<File> readSmf (std::span<const std::uint8_t> bytes)
    {
        using R = util::Result<File>;

        if (bytes.size() < 8 || !isId (bytes, 0, "MThd"))
            return R::failure ("not a MIDI file: it doesn't start with an MThd chunk");
        const std::uint32_t headerLength = be32 (bytes, 4);
        if (headerLength < 6)
            return R::failure ("the MThd header is " + str (headerLength) + " bytes; it needs 6");
        if (bytes.size() - 8 < headerLength)
            return R::failure ("truncated: the MThd header needs " + str (headerLength) + " bytes, " + str (bytes.size() - 8) + " follow");

        const std::uint16_t format = be16 (bytes, 8);
        const std::uint16_t trackCount = be16 (bytes, 10);
        const std::uint16_t division = be16 (bytes, 12);
        if (format == 2)
            return R::failure ("format 2 (independent sequences) isn't supported");
        if (format > 2)
            return R::failure ("format " + str (format) + " isn't a MIDI file format");
        if ((division & 0x8000) != 0)
            return R::failure ("SMPTE time division isn't supported, only ticks per quarter note");
        if (division == 0)
            return R::failure ("0 ticks per quarter note");
        if (format == 0 && trackCount != 1)
            return R::failure ("format 0 has one track; this file says " + str (trackCount));

        File file;
        file.format = format;
        file.ppq = division;
        file.tracks.reserve (std::min<std::size_t> (trackCount, bytes.size() / 12)); // a track takes 12 bytes or more

        std::size_t pos = 8 + std::size_t { headerLength };
        while (file.tracks.size() < trackCount)
        {
            if (bytes.size() - pos < 8)
                return R::failure ("truncated: the file ends after " + str (file.tracks.size()) + " of " + str (trackCount) + " tracks");
            const std::uint32_t length = be32 (bytes, pos + 4);
            if (bytes.size() - pos - 8 < length)
                return R::failure ("truncated: the chunk at byte " + str (pos) + " needs " + str (length) + " bytes, " + str (bytes.size() - pos - 8) + " follow");
            if (isId (bytes, pos, "MTrk"))
            {
                auto track = readTrack (bytes.subspan (pos + 8, length), file.tracks.size(), pos + 8);
                if (!track.ok())
                    return R::failure (std::move (track.error));
                file.tracks.push_back (std::move (track.value));
            }
            // Any other chunk is skipped, as the SMF spec asks.
            pos += 8 + std::size_t { length };
        }
        return R::success (std::move (file));
    }

    std::string trackName (const Track& track)
    {
        for (const auto& e : track.events)
            if (e.isMeta (meta::trackName))
                return textOf (e);
        return {};
    }

    std::vector<Note> notesOf (const Track& track)
    {
        std::vector<Note> notes;
        std::vector<std::vector<std::size_t>> sounding (16 * 128); // per channel and key: indices into notes, oldest first
        for (const auto& e : track.events)
        {
            if (e.data1 > 127) // not from readSmf, which never gives one: skip it rather than index past the table
                continue;
            if (e.isNoteOn())
            {
                sounding[std::size_t { e.channel() } * 128 + e.data1].push_back (notes.size());
                notes.push_back ({ e.tick, 0, e.channel(), e.data1, e.data2, 0 });
            }
            else if (e.isNoteOff())
            {
                auto& open = sounding[std::size_t { e.channel() } * 128 + e.data1];
                if (open.empty())
                    continue;
                Note& n = notes[open.front()];
                n.length = e.tick - n.tick;
                n.releaseVelocity = e.kind() == status::noteOff ? e.data2 : std::uint8_t { 0 };
                open.erase (open.begin());
            }
        }
        for (const auto& open : sounding)
            for (const std::size_t i : open)
                notes[i].length = std::max<std::int64_t> (0, track.endTick - notes[i].tick);

        std::stable_sort (notes.begin(), notes.end(), [] (const Note& a, const Note& b) {
            return std::tie (a.tick, a.channel, a.key) < std::tie (b.tick, b.channel, b.key);
        });
        return notes;
    }

    std::vector<Controller> controllersOf (const Track& track)
    {
        std::vector<Controller> out;
        for (const auto& e : track.events)
            if (e.isController())
                out.push_back ({ e.tick, e.channel(), e.data1, e.data2 });
        return out;
    }

    std::vector<ProgramChange> programsOf (const Track& track)
    {
        std::vector<ProgramChange> out;
        for (const auto& e : track.events)
            if (e.isChannel() && e.kind() == status::programChange)
                out.push_back ({ e.tick, e.channel(), e.data1 });
        return out;
    }

    std::vector<TextEvent> textsOf (const Track& track)
    {
        std::vector<TextEvent> out;
        for (const auto& e : track.events)
            if (e.isMeta() && meta::isText (e.data1))
                out.push_back ({ e.tick, e.data1, textOf (e) });
        return out;
    }

    std::vector<Marker> markersOf (const File& file)
    {
        std::vector<Marker> out;
        for (const Event* e : metasOf (file, meta::marker))
            out.push_back ({ e->tick, textOf (*e) });
        return out;
    }

    std::vector<Tempo> tempoMap (const File& file)
    {
        std::vector<Tempo> out;
        for (const Event* e : metasOf (file, meta::tempo))
        {
            if (e->payload.size() != 3)
                continue;
            const std::uint32_t us = (std::uint32_t { e->payload[0] } << 16) | (std::uint32_t { e->payload[1] } << 8) | e->payload[2];
            if (us != 0)
                out.push_back ({ e->tick, us });
        }
        return out;
    }

    std::vector<TimeSignature> timeSignaturesOf (const File& file)
    {
        std::vector<TimeSignature> out;
        for (const Event* e : metasOf (file, meta::timeSignature))
            if (e->payload.size() == 4)
                out.push_back ({ e->tick, e->payload[0], e->payload[1], e->payload[2], e->payload[3] });
        return out;
    }

    std::vector<KeySignature> keySignaturesOf (const File& file)
    {
        std::vector<KeySignature> out;
        for (const Event* e : metasOf (file, meta::keySignature))
            if (e->payload.size() == 2)
                out.push_back ({ e->tick, static_cast<std::int8_t> (e->payload[0]), e->payload[1] != 0 });
        return out;
    }

    std::int64_t ticksToMicros (std::span<const Tempo> tempos, std::uint16_t ppq, std::int64_t tick)
    {
        if (ppq < 1 || ppq > 0x7FFF || tick < 0 || tick > maxTick)
            return 0;

        // Sum ticks times microseconds per quarter, then divide once: exact. Every term is below
        // 2^28 * 2^32 and the ticks add up to at most 2^28, so the sum stays below 2^60.
        std::int64_t total = 0;
        std::int64_t at = 0;
        std::int64_t us = 500000;
        for (const Tempo& t : tempos)
        {
            if (t.tick >= tick)
                break;
            if (t.tick > at)
            {
                total += (t.tick - at) * us;
                at = t.tick;
            }
            if (t.microsPerQuarter != 0)
                us = t.microsPerQuarter;
        }
        total += (tick - at) * us;
        return total / ppq;
    }
}
