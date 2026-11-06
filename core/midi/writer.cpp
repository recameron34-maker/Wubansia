#include "midi/writer.h"

#include "util/utf8.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>
#include <tuple>
#include <utility>

namespace wubansia::midi
{
    namespace
    {
        using Bytes = std::vector<std::uint8_t>;

        // The classes of events on one tick, in the order they go out (writer.h).
        enum class Order : std::uint8_t {
            trackName,
            tempo,
            timeSignature,
            keySignature,
            text,
            noteOff,
            program,
            controller,
            noteOn,
        };

        // An event and its place in the order: (tick, order, channel, number, payload), where number is
        // the key, the controller number or the text type, and the payload breaks ties between texts.
        struct Placed
        {
            Event event;
            Order order;
            std::uint8_t channel;
            std::uint8_t number;
        };

        bool placedBefore (const Placed& a, const Placed& b)
        {
            return std::tie (a.event.tick, a.order, a.channel, a.number, a.event.payload)
                   < std::tie (b.event.tick, b.order, b.channel, b.number, b.event.payload);
        }

        std::string str (std::int64_t v) { return std::to_string (v); }

        std::string hex (std::uint32_t v)
        {
            constexpr char digits[] = "0123456789ABCDEF";
            std::string s = "0x";
            s += digits[(v >> 4) & 0xF];
            s += digits[v & 0xF];
            return s;
        }

        Bytes bytesOf (const std::string& s)
        {
            Bytes b;
            b.reserve (s.size());
            for (const char c : s)
                b.push_back (static_cast<std::uint8_t> (c));
            return b;
        }

        std::string trackLabel (std::size_t index, const std::string& name)
        {
            std::string s = "track " + std::to_string (index);
            if (index == 0)
                s += " (the conductor)";
            if (!name.empty() && util::isValidUtf8 (name))
                s += " \"" + name + "\"";
            return s;
        }

        std::string textKind (std::uint8_t type)
        {
            switch (type)
            {
                case meta::text:
                    return "text event";
                case meta::copyright:
                    return "copyright notice";
                case meta::trackName:
                    return "track name";
                case meta::instrumentName:
                    return "instrument name";
                case meta::lyric:
                    return "lyric";
                case meta::marker:
                    return "marker";
                case meta::cuePoint:
                    return "cue point";
                default:
                    return "text event (type " + hex (type) + ")";
            }
        }

        // Builds one track: collects its events with their places, checking each, then sorts them
        // and refuses two values for one thing on one tick. The first problem found is kept.
        class TrackBuilder
        {
        public:
            TrackBuilder (std::size_t index, const std::string& name, std::int64_t endTick)
                : label (trackLabel (index, name)), end (endTick)
            {
            }

            std::int64_t endTick() const noexcept { return end; }

            // A tick an event may sit on: 0 to the end tick. Fails otherwise.
            bool tickOk (std::int64_t tick, const std::string& what)
            {
                if (tick < 0)
                    return fail (what + " is at tick " + str (tick) + "; ticks start at 0");
                if (tick > end)
                    return fail (what + " at tick " + str (tick) + " is after the end tick " + str (end));
                return true;
            }

            bool fail (const std::string& message)
            {
                if (error.empty())
                    error = label + ": " + message;
                return false;
            }

            void add (Event e, Order order, std::uint8_t channel, std::uint8_t number)
            {
                placed.push_back ({ std::move (e), order, channel, number });
            }

            void addMeta (std::int64_t tick, std::uint8_t type, Bytes payload, Order order, std::uint8_t number = 0)
            {
                add ({ tick, status::meta, type, 0, std::move (payload) }, order, 0, number);
            }

            bool addText (std::int64_t tick, std::uint8_t type, const std::string& text)
            {
                const std::string what = textKind (type);
                if (!tickOk (tick, what))
                    return false;
                if (!util::isValidUtf8 (text))
                    return fail (what + " at tick " + str (tick) + " isn't valid UTF-8");
                if (text.size() > static_cast<std::size_t> (maxTick))
                    return fail (what + " at tick " + str (tick) + " is longer than an SMF event holds");
                addMeta (tick, type, bytesOf (text), Order::text, type);
                return true;
            }

            // Sorts, refuses two values for one thing on one tick, and hands the events over.
            bool finish (Track& out)
            {
                if (!error.empty())
                    return false;

                std::stable_sort (placed.begin(), placed.end(), placedBefore);

                for (std::size_t i = 1; i < placed.size(); ++i)
                {
                    const Placed& a = placed[i - 1];
                    const Placed& b = placed[i];
                    if (a.event.tick != b.event.tick || a.order != b.order || a.channel != b.channel || a.number != b.number)
                        continue;
                    const std::string at = " at tick " + str (a.event.tick);
                    switch (a.order)
                    {
                        case Order::tempo:
                            return fail ("two tempos" + at);
                        case Order::timeSignature:
                            return fail ("two time signatures" + at);
                        case Order::keySignature:
                            return fail ("two key signatures" + at);
                        case Order::program:
                            return fail ("two program changes on channel " + str (a.channel) + at);
                        case Order::controller:
                            return fail ("two values of controller " + str (a.number) + " on channel " + str (a.channel) + at);
                        case Order::text:
                            if (a.event.payload == b.event.payload)
                                return fail ("the same " + textKind (a.number) + " twice" + at);
                            break; // different texts on one tick: ordered by their bytes
                        case Order::trackName: // one per track
                        case Order::noteOff: // impossible without overlapping notes, which are refused first
                        case Order::noteOn:
                            return fail ("two events in one place" + at);
                    }
                }

                out.events.clear();
                out.events.reserve (placed.size());
                for (auto& p : placed)
                    out.events.push_back (std::move (p.event));
                out.endTick = end;
                return true;
            }

            std::string error;

        private:
            std::string label;
            std::int64_t end;
            std::vector<Placed> placed;
        };

        bool addConductor (const SongSpec& spec, TrackBuilder& b)
        {
            if (!spec.name.empty())
            {
                if (!util::isValidUtf8 (spec.name))
                    return b.fail ("the song name isn't valid UTF-8");
                b.addMeta (0, meta::trackName, bytesOf (spec.name), Order::trackName);
            }

            for (const Tempo& t : spec.tempos)
            {
                if (!b.tickOk (t.tick, "a tempo"))
                    return false;
                if (t.microsPerQuarter < 1 || t.microsPerQuarter > 0xFFFFFF)
                    return b.fail ("tempo " + str (t.microsPerQuarter) + " us per quarter note at tick " + str (t.tick) + " is out of range (1-16777215)");
                const std::uint32_t us = t.microsPerQuarter;
                b.addMeta (t.tick, meta::tempo, { static_cast<std::uint8_t> (us >> 16), static_cast<std::uint8_t> ((us >> 8) & 0xFF), static_cast<std::uint8_t> (us & 0xFF) }, Order::tempo);
            }

            for (const TimeSignature& t : spec.timeSignatures)
            {
                if (!b.tickOk (t.tick, "a time signature"))
                    return false;
                const std::string what = "time signature at tick " + str (t.tick);
                if (t.numerator < 1)
                    return b.fail (what + " has numerator 0");
                if (t.denominatorPow2 > 7)
                    return b.fail (what + " has denominator 2^" + str (t.denominatorPow2) + "; the most is 2^7 (128ths)");
                if (t.clocksPerClick < 1 || t.thirtySecondsPerQuarter < 1)
                    return b.fail (what + " needs at least 1 clock per click and 1 32nd per quarter note");
                b.addMeta (t.tick, meta::timeSignature, { t.numerator, t.denominatorPow2, t.clocksPerClick, t.thirtySecondsPerQuarter }, Order::timeSignature);
            }

            for (const KeySignature& k : spec.keySignatures)
            {
                if (!b.tickOk (k.tick, "a key signature"))
                    return false;
                if (k.sharps < -7 || k.sharps > 7)
                    return b.fail ("key signature with " + str (k.sharps) + " sharps at tick " + str (k.tick) + " is out of range (-7 to 7)");
                b.addMeta (k.tick, meta::keySignature, { static_cast<std::uint8_t> (k.sharps), static_cast<std::uint8_t> (k.minor ? 1 : 0) }, Order::keySignature);
            }

            for (const Marker& m : spec.markers)
                if (!b.addText (m.tick, meta::marker, m.text))
                    return false;

            return true;
        }

        std::string noteLabel (std::size_t index, const Note& n)
        {
            return "note " + std::to_string (index) + " (key " + str (n.key) + ", channel " + str (n.channel) + ", tick " + str (n.tick) + ")";
        }

        bool addLane (const TrackSpec& spec, TrackBuilder& b)
        {
            if (!util::isValidUtf8 (spec.name))
                return b.fail ("the name isn't valid UTF-8");
            if (!spec.name.empty())
                b.addMeta (0, meta::trackName, bytesOf (spec.name), Order::trackName);

            for (std::size_t i = 0; i < spec.notes.size(); ++i)
            {
                const Note& n = spec.notes[i];
                const auto what = [i, &n] { return noteLabel (i, n); }; // built only for an error message
                if (n.channel > 15)
                    return b.fail (what() + " has channel " + str (n.channel) + "; channels are 0-15");
                if (n.key > 127)
                    return b.fail (what() + " has key " + str (n.key) + "; keys are 0-127");
                if (n.velocity < 1 || n.velocity > 127)
                    return b.fail (what() + " has velocity " + str (n.velocity) + "; velocities are 1-127");
                if (n.releaseVelocity > 127)
                    return b.fail (what() + " has release velocity " + str (n.releaseVelocity) + "; release velocities are 0-127");
                if (n.length < 1)
                    return b.fail (what() + " has length " + str (n.length) + "; a note needs at least 1 tick");
                if (n.tick < 0 || n.tick > b.endTick())
                    return b.tickOk (n.tick, what());
                if (n.length > maxTick || n.tick + n.length > b.endTick()) // n.tick <= maxTick here, so no overflow
                    return b.fail (what() + " ends at tick " + str (n.tick + std::min (n.length, maxTick + 1)) + ", after the end tick " + str (b.endTick()));

                b.add ({ n.tick, static_cast<std::uint8_t> (status::noteOn | n.channel), n.key, n.velocity, {} }, Order::noteOn, n.channel, n.key);
                b.add ({ n.tick + n.length, static_cast<std::uint8_t> (status::noteOff | n.channel), n.key, n.releaseVelocity, {} }, Order::noteOff, n.channel, n.key);
            }

            // Overlaps: two notes on one channel and key where the second starts before the first ends.
            std::vector<std::size_t> byKey (spec.notes.size());
            for (std::size_t i = 0; i < byKey.size(); ++i)
                byKey[i] = i;
            std::stable_sort (byKey.begin(), byKey.end(), [&spec] (std::size_t x, std::size_t y) {
                const Note& p = spec.notes[x];
                const Note& q = spec.notes[y];
                return std::tie (p.channel, p.key, p.tick) < std::tie (q.channel, q.key, q.tick);
            });
            for (std::size_t i = 1; i < byKey.size(); ++i)
            {
                const Note& p = spec.notes[byKey[i - 1]];
                const Note& q = spec.notes[byKey[i]];
                if (p.channel == q.channel && p.key == q.key && p.tick + p.length > q.tick)
                    return b.fail ("notes on key " + str (p.key) + ", channel " + str (p.channel) + " overlap: one at tick " + str (p.tick)
                                   + " (length " + str (p.length) + ") and one at tick " + str (q.tick) + " (trimOverlaps () fixes this)");
            }

            for (const Controller& c : spec.controllers)
            {
                const auto what = [&c] { return "controller " + str (c.number) + " on channel " + str (c.channel); };
                if (c.channel > 15)
                    return b.fail (what() + ": channels are 0-15");
                if (c.number > 127)
                    return b.fail (what() + ": controller numbers are 0-127");
                if (c.value > 127)
                    return b.fail (what() + " at tick " + str (c.tick) + " has value " + str (c.value) + "; values are 0-127");
                if (c.tick < 0 || c.tick > b.endTick())
                    return b.tickOk (c.tick, what());
                b.add ({ c.tick, static_cast<std::uint8_t> (status::controlChange | c.channel), c.number, c.value, {} }, Order::controller, c.channel, c.number);
            }

            for (const ProgramChange& p : spec.programs)
            {
                const std::string what = "program change on channel " + str (p.channel);
                if (p.channel > 15)
                    return b.fail (what + ": channels are 0-15");
                if (p.program > 127)
                    return b.fail (what + " at tick " + str (p.tick) + " to program " + str (p.program) + "; programs are 0-127");
                if (!b.tickOk (p.tick, what))
                    return false;
                b.add ({ p.tick, static_cast<std::uint8_t> (status::programChange | p.channel), p.program, 0, {} }, Order::program, p.channel, 0);
            }

            for (const TextEvent& t : spec.texts)
            {
                if (!meta::isText (t.type) || t.type == meta::trackName)
                    return b.fail ("a text event at tick " + str (t.tick) + " has type " + hex (t.type) + "; texts take types 0x01-0x0F except 0x03 (the name has its own field)");
                if (!b.addText (t.tick, t.type, t.text))
                    return false;
            }

            return true;
        }

        void appendU32 (Bytes& out, std::uint32_t v)
        {
            out.push_back (static_cast<std::uint8_t> (v >> 24));
            out.push_back (static_cast<std::uint8_t> ((v >> 16) & 0xFF));
            out.push_back (static_cast<std::uint8_t> ((v >> 8) & 0xFF));
            out.push_back (static_cast<std::uint8_t> (v & 0xFF));
        }

        void appendU16 (Bytes& out, std::uint32_t v)
        {
            out.push_back (static_cast<std::uint8_t> ((v >> 8) & 0xFF));
            out.push_back (static_cast<std::uint8_t> (v & 0xFF));
        }

        // Appends one event's bytes after its delta time. Returns what is wrong with it, or "".
        std::string encodeEvent (const Event& e, Bytes& body)
        {
            if (e.isChannel())
            {
                if (!e.payload.empty())
                    return "a channel message has no payload";
                if (e.data1 > 0x7F)
                    return "data byte " + hex (e.data1) + " is above 0x7F";
                const bool oneDataByte = e.kind() == status::programChange || e.kind() == status::channelPressure;
                if (oneDataByte && e.data2 != 0)
                    return "data2 must be 0 for a program change or channel pressure, not " + hex (e.data2);
                if (!oneDataByte && e.data2 > 0x7F)
                    return "data byte " + hex (e.data2) + " is above 0x7F";
                body.push_back (e.status);
                body.push_back (e.data1);
                if (!oneDataByte)
                    body.push_back (e.data2);
                return {};
            }

            if (e.isMeta() || e.isSysex())
            {
                if (e.isMeta())
                {
                    if (e.data1 > 0x7F)
                        return "meta type " + hex (e.data1) + " is above 0x7F";
                    if (e.data1 == meta::endOfTrack)
                        return "an end-of-track event among the events (encode () writes it at endTick)";
                }
                else if (e.data1 != 0)
                    return "data1 must be 0 for a sysex event, not " + hex (e.data1);
                if (e.data2 != 0)
                    return "data2 must be 0 for a meta or sysex event, not " + hex (e.data2);
                if (e.payload.size() > static_cast<std::size_t> (maxTick))
                    return "a payload of " + std::to_string (e.payload.size()) + " bytes, more than an SMF event holds";

                body.push_back (e.status);
                if (e.isMeta())
                    body.push_back (e.data1);
                appendVarLen (body, static_cast<std::uint32_t> (e.payload.size()));
                body.insert (body.end(), e.payload.begin(), e.payload.end());
                return {};
            }

            return "status " + hex (e.status) + " isn't a MIDI file event";
        }
    }

    void appendVarLen (std::vector<std::uint8_t>& out, std::uint32_t value)
    {
        if (value > static_cast<std::uint32_t> (maxTick))
            value = static_cast<std::uint32_t> (maxTick);
        std::array<std::uint8_t, 4> groups {};
        std::size_t n = 0;
        groups[n++] = static_cast<std::uint8_t> (value & 0x7F);
        while ((value >>= 7) != 0)
            groups[n++] = static_cast<std::uint8_t> ((value & 0x7F) | 0x80);
        while (n > 0)
            out.push_back (groups[--n]);
    }

    util::Result<File> toFile (const SongSpec& spec)
    {
        using R = util::Result<File>;

        if (spec.ppq < 1 || spec.ppq > 0x7FFF)
            return R::failure ("ppq " + str (spec.ppq) + " is out of range (1-32767)");
        if (spec.endTick < 0 || spec.endTick > maxTick)
            return R::failure ("the end tick " + str (spec.endTick) + " is out of range (0-" + str (maxTick) + ")");
        if (spec.tracks.size() > 65534)
            return R::failure (std::to_string (spec.tracks.size()) + " lane tracks: an SMF holds 65535 tracks, the conductor included");

        File file;
        file.format = 1;
        file.ppq = spec.ppq;
        file.tracks.resize (spec.tracks.size() + 1);

        {
            TrackBuilder conductor (0, spec.name, spec.endTick);
            if (!addConductor (spec, conductor) || !conductor.finish (file.tracks[0]))
                return R::failure (conductor.error);
        }

        for (std::size_t i = 0; i < spec.tracks.size(); ++i)
        {
            TrackBuilder lane (i + 1, spec.tracks[i].name, spec.endTick);
            if (!addLane (spec.tracks[i], lane) || !lane.finish (file.tracks[i + 1]))
                return R::failure (lane.error);
        }

        return R::success (std::move (file));
    }

    util::Result<std::vector<std::uint8_t>> encode (const File& file)
    {
        using R = util::Result<Bytes>;

        if (file.format > 1)
            return R::failure ("format " + str (file.format) + ": the writer writes formats 0 and 1");
        if (file.format == 0 && file.tracks.size() != 1)
            return R::failure ("format 0 needs exactly one track, not " + std::to_string (file.tracks.size()));
        if (file.ppq < 1 || file.ppq > 0x7FFF)
            return R::failure ("ppq " + str (file.ppq) + " is out of range (1-32767)");
        if (file.tracks.size() > 0xFFFF)
            return R::failure (std::to_string (file.tracks.size()) + " tracks: an SMF holds at most 65535");

        Bytes out = { 'M', 'T', 'h', 'd' };
        appendU32 (out, 6);
        appendU16 (out, file.format);
        appendU16 (out, static_cast<std::uint32_t> (file.tracks.size()));
        appendU16 (out, file.ppq);

        Bytes body;
        for (std::size_t t = 0; t < file.tracks.size(); ++t)
        {
            const Track& track = file.tracks[t];
            const std::string where = "track " + std::to_string (t);
            body.clear();
            std::int64_t last = 0;
            for (std::size_t i = 0; i < track.events.size(); ++i)
            {
                const Event& e = track.events[i];
                const auto what = [&where, i, &e] { return where + ", event " + std::to_string (i) + " (tick " + str (e.tick) + "): "; };
                if (e.tick < 0)
                    return R::failure (what() + "ticks start at 0");
                if (e.tick < last)
                    return R::failure (what() + "goes backwards from tick " + str (last));
                if (e.tick - last > maxTick)
                    return R::failure (what() + "comes " + str (e.tick - last) + " ticks after the event before; a delta time holds at most " + str (maxTick));
                appendVarLen (body, static_cast<std::uint32_t> (e.tick - last));
                if (const std::string problem = encodeEvent (e, body); !problem.empty())
                    return R::failure (what() + problem);
                last = e.tick;
            }
            if (track.endTick < last)
                return R::failure (where + ": the end tick " + str (track.endTick) + " is before the last event, at tick " + str (last));
            if (track.endTick - last > maxTick)
                return R::failure (where + ": the end of track comes " + str (track.endTick - last) + " ticks after the last event; a delta time holds at most " + str (maxTick));
            appendVarLen (body, static_cast<std::uint32_t> (track.endTick - last));
            body.insert (body.end(), { status::meta, meta::endOfTrack, 0x00 });

            if (body.size() > 0xFFFFFFFFu)
                return R::failure (where + ": " + std::to_string (body.size()) + " bytes, more than an MTrk chunk holds");
            out.insert (out.end(), { 'M', 'T', 'r', 'k' });
            appendU32 (out, static_cast<std::uint32_t> (body.size()));
            out.insert (out.end(), body.begin(), body.end());
        }

        return R::success (std::move (out));
    }

    util::Result<std::vector<std::uint8_t>> writeSmf (const SongSpec& spec)
    {
        auto file = toFile (spec);
        if (!file.ok())
            return util::Result<std::vector<std::uint8_t>>::failure (std::move (file.error));
        return encode (file.value);
    }

    std::size_t trimOverlaps (std::vector<Note>& notes)
    {
        std::stable_sort (notes.begin(), notes.end(), [] (const Note& a, const Note& b) {
            return std::tie (a.channel, a.key, a.tick) < std::tie (b.channel, b.key, b.tick);
        });

        std::vector<Note> out;
        out.reserve (notes.size());
        std::size_t changed = 0;
        for (std::size_t i = 0; i < notes.size();)
        {
            Note merged = notes[i];
            std::size_t j = i + 1;
            for (; j < notes.size() && notes[j].channel == merged.channel && notes[j].key == merged.key && notes[j].tick == merged.tick; ++j)
            {
                merged.length = std::max (merged.length, notes[j].length);
                merged.velocity = std::max (merged.velocity, notes[j].velocity);
                merged.releaseVelocity = std::max (merged.releaseVelocity, notes[j].releaseVelocity);
                ++changed; // one note removed
            }

            if (!out.empty())
            {
                Note& before = out.back();
                if (before.channel == merged.channel && before.key == merged.key && before.tick + before.length > merged.tick)
                {
                    before.length = merged.tick - before.tick; // at least 1: their ticks differ
                    ++changed;
                }
            }
            out.push_back (merged);
            i = j;
        }

        std::stable_sort (out.begin(), out.end(), [] (const Note& a, const Note& b) {
            return std::tie (a.tick, a.channel, a.key) < std::tie (b.tick, b.channel, b.key);
        });
        notes = std::move (out);
        return changed;
    }
}
