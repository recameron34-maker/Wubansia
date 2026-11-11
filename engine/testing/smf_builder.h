#pragma once

// A Standard MIDI File builder for tests: the files the content has (SMF type 1, a conductor with
// tempo, 4/4, a key signature, FF 06 section markers and FF 07 cues, then one track per lane), or
// any odd file a test needs. It writes through core's encoder (midi/writer.h), so the bytes are
// what core's reader reads back.
//
// Within a track, events at one tick go out in midi_plan's order: the track name, tempo, time and
// key signatures, markers, cues, then note-offs, controllers, note-ons; ties keep the order they
// were added in. Channels are 1-16, as docs/midi-map.md counts them. A note-off is 0x80 with
// velocity 0, as tools/sound/midi.py writes it. Nothing is validated: a test can build overlaps or
// stray events on purpose.
//
//     SmfBuilder smf (960);
//     smf.name ("SYN01").tempo (0, 468750).timeSignature (0, 4, 2).keySignature (0, -2, true);
//     smf.marker (0, "Intro");
//     auto& kick = smf.track ("Kick");
//     kick.note (0, 120, 10, 36, 127);
//     std::vector<std::uint8_t> bytes = smf.bytes (15360);

#include "midi/smf.h"
#include "midi/writer.h"

#include <algorithm>
#include <cstdint>
#include <deque>
#include <string>
#include <utility>
#include <vector>

namespace wubansia::engine::testing
{
    class SmfBuilder
    {
    public:
        // Event classes, in their order on one tick.
        enum class Order : std::uint8_t {
            name,
            tempo,
            timeSignature,
            keySignature,
            marker,
            cue,
            text,
            noteOff,
            controller,
            noteOn
        };

        class Track
        {
        public:
            explicit Track (std::string trackName) : name (std::move (trackName)) {}

            // A note: on at tick, off at tick + length (0x80, velocity 0). channel 1-16.
            Track& note (std::int64_t tick, std::int64_t length, int channel, int key, int velocity)
            {
                add (tick, Order::noteOn, channelMessage (midi::status::noteOn, channel, key, velocity));
                add (tick + length, Order::noteOff, channelMessage (midi::status::noteOff, channel, key, 0));
                return *this;
            }

            Track& noteOn (std::int64_t tick, int channel, int key, int velocity)
            {
                add (tick, Order::noteOn, channelMessage (midi::status::noteOn, channel, key, velocity));
                return *this;
            }

            Track& noteOff (std::int64_t tick, int channel, int key)
            {
                add (tick, Order::noteOff, channelMessage (midi::status::noteOff, channel, key, 0));
                return *this;
            }

            Track& cc (std::int64_t tick, int channel, int controller, int value)
            {
                add (tick, Order::controller, channelMessage (midi::status::controlChange, channel, controller, value));
                return *this;
            }

            // A text meta event of any type 0x01-0x0F (FF 01 text, FF 06 marker, FF 07 cue...).
            Track& meta (std::int64_t tick, std::uint8_t type, const std::string& text)
            {
                midi::Event e;
                e.status = midi::status::meta;
                e.data1 = type;
                e.payload.assign (text.begin(), text.end());
                add (tick, type == midi::meta::marker ? Order::marker : type == midi::meta::cuePoint ? Order::cue : Order::text, std::move (e));
                return *this;
            }

            // A meta event with raw bytes (tempo, signatures...).
            Track& rawMeta (std::int64_t tick, Order order, std::uint8_t type, std::vector<std::uint8_t> payload)
            {
                midi::Event e;
                e.status = midi::status::meta;
                e.data1 = type;
                e.payload = std::move (payload);
                add (tick, order, std::move (e));
                return *this;
            }

            // The track as core's File holds it: the name first, then every event by (tick, order, insertion).
            midi::Track build (std::int64_t endTick) const
            {
                auto sorted = events;
                std::stable_sort (sorted.begin(), sorted.end(), [] (const Item& a, const Item& b) {
                    if (a.event.tick != b.event.tick)
                        return a.event.tick < b.event.tick;
                    return a.order < b.order;
                });

                midi::Track track;
                if (! name.empty())
                {
                    midi::Event e;
                    e.status = midi::status::meta;
                    e.data1 = midi::meta::trackName;
                    e.payload.assign (name.begin(), name.end());
                    track.events.push_back (std::move (e));
                }
                for (const auto& item : sorted)
                    track.events.push_back (item.event);
                track.endTick = std::max (endTick, track.events.empty() ? std::int64_t { 0 } : track.events.back().tick);
                return track;
            }

            const std::string name;

        private:
            struct Item
            {
                midi::Event event;
                Order order;
            };

            static midi::Event channelMessage (std::uint8_t kind, int channel, int data1, int data2)
            {
                midi::Event e;
                e.status = static_cast<std::uint8_t> (kind | ((channel - 1) & 0x0F));
                e.data1 = static_cast<std::uint8_t> (data1 & 0x7F);
                e.data2 = static_cast<std::uint8_t> (data2 & 0x7F);
                return e;
            }

            void add (std::int64_t tick, Order order, midi::Event e)
            {
                e.tick = tick;
                events.push_back ({ std::move (e), order });
            }

            std::vector<Item> events;
        };

        explicit SmfBuilder (std::uint16_t ppq = 960) : ppq_ (ppq), conductor_ ("") {}

        // The conductor: track 0.
        SmfBuilder& name (const std::string& text)
        {
            conductorName = text;
            return *this;
        }

        SmfBuilder& tempo (std::int64_t tick, std::uint32_t microsPerQuarter)
        {
            conductor_.rawMeta (tick, Order::tempo, midi::meta::tempo, { static_cast<std::uint8_t> (microsPerQuarter >> 16), static_cast<std::uint8_t> (microsPerQuarter >> 8), static_cast<std::uint8_t> (microsPerQuarter) });
            return *this;
        }

        // numerator / 2^denominatorPow2: (4, 2) is 4/4.
        SmfBuilder& timeSignature (std::int64_t tick, int numerator, int denominatorPow2)
        {
            conductor_.rawMeta (tick, Order::timeSignature, midi::meta::timeSignature, { static_cast<std::uint8_t> (numerator), static_cast<std::uint8_t> (denominatorPow2), 24, 8 });
            return *this;
        }

        // sharps -7..7 (below 0: flats).
        SmfBuilder& keySignature (std::int64_t tick, int sharps, bool minor)
        {
            conductor_.rawMeta (tick, Order::keySignature, midi::meta::keySignature, { static_cast<std::uint8_t> (static_cast<std::int8_t> (sharps)), static_cast<std::uint8_t> (minor ? 1 : 0) });
            return *this;
        }

        SmfBuilder& marker (std::int64_t tick, const std::string& text)
        {
            conductor_.meta (tick, midi::meta::marker, text);
            return *this;
        }

        SmfBuilder& cue (std::int64_t tick, const std::string& text)
        {
            conductor_.meta (tick, midi::meta::cuePoint, text);
            return *this;
        }

        // A new lane track, after the ones before it. The reference stays valid.
        Track& track (const std::string& trackName)
        {
            tracks.emplace_back (trackName);
            return tracks.back();
        }

        // Every track ends at endTick (or at its last event, if later).
        midi::File file (std::int64_t endTick) const
        {
            midi::File f;
            f.format = 1;
            f.ppq = ppq_;
            midi::Track built = conductor_.build (endTick);
            if (! conductorName.empty())
            {
                midi::Event e;
                e.status = midi::status::meta;
                e.data1 = midi::meta::trackName;
                e.payload.assign (conductorName.begin(), conductorName.end());
                built.events.insert (built.events.begin(), std::move (e));
            }
            f.tracks.push_back (std::move (built));
            for (const auto& t : tracks)
                f.tracks.push_back (t.build (endTick));
            return f;
        }

        // The file's bytes; empty if core's encoder refuses it (an event past a track's end...).
        std::vector<std::uint8_t> bytes (std::int64_t endTick) const
        {
            auto encoded = midi::encode (file (endTick));
            return encoded.ok() ? std::move (encoded.value) : std::vector<std::uint8_t> {};
        }

    private:
        std::uint16_t ppq_;
        std::string conductorName;
        Track conductor_;
        std::deque<Track> tracks; // a deque, so track () references stay valid
    };
}
