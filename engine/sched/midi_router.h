#pragma once

// The MIDI router: live MIDI in (the block's 3-byte messages) resolved against the adopted Kit and the
// controls, for the engine to play beside the timeline. Audio thread; nothing here allocates, locks or
// throws.
//
// - Multi mode routes each message by its channel (docs/midi-map.md); Lane mode plays every channel on
//   one lane: its channel in the map (the drums' 10 for kick, snare, hats and perc; 1 bass, 2 lead,
//   4 chords, 5 vox, 6 accents, 7 fx), whose keymap then resolves the key. A kit's extras on 11-16 are
//   reached in Multi mode only.
// - Notes resolve through the Kit's KeyTarget table and the current A/B switches. Channel 3 (the chord
//   voicing, for your own synth) is ignored in both modes, and so is a key no keymap entry covers.
// - Each note keeps its own note-off: the router remembers the instrument every held (input channel, key)
//   started, so an A/B or lane switch while it is held still ends it. A note-on for a key that is still
//   held ends the earlier note first.
// - Live notes count their own round robins per instrument, from 0 at each transport start, and latch
//   the throws (CC91 Space, CC94 Echo) of their input channel at the note-on. They never play phrase
//   slices: a LiveNote has no phrase.
// - A live note-on of a duck-key instrument is a duck key at its onset (it can't be anticipated).
// - Controllers: CC74 and CC71 go to the cc_filters of the channel (the lane's in Lane mode); CC91 and
//   CC94 latch throws; CC121 resets the channel's throws; CC123 (all notes off) releases every live note;
//   CC120 (all sound off) fades every live voice. Everything else is ignored.
//
// The Sink (any type with these members, all noexcept):
//   void liveNoteOn (const LiveNote& note);
//   void liveNoteOff (int instrument, std::uint8_t key, int offset);
//   void cc (std::uint8_t channel, std::uint8_t controller, std::uint8_t value, int offset, int rampSamples);
//   void keyHit (std::uint8_t lane, std::uint8_t velocity, int offset);
//   void liveReleaseAll (int offset);  // CC123
//   void liveSoundOff (int offset);    // CC120

#include "engine.h"
#include "kit/channel_map.h"
#include "kit/kit.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace wubansia::engine
{
    // A live note-on, resolved to an instrument.
    struct LiveNote
    {
        std::int16_t instrument = -1; // Kit::instruments index
        std::uint8_t key = 0;
        std::uint8_t velocity = 0; // 1-127
        std::uint8_t rr = 0; // the instrument's live notes since the last transport start, wrapped at 256
        std::uint8_t throwSpace = 0; // CC91 on its input channel at its note-on, 0-127
        std::uint8_t throwEcho = 0; // CC94, the same
        std::int32_t offset = 0;
    };

    // The channel (0-15) Lane mode plays a lane on: its role's channel in docs/midi-map.md.
    constexpr int laneChannel (Lane lane) noexcept
    {
        switch (lane)
        {
            case Lane::kick:
            case Lane::snare:
            case Lane::hats:
            case Lane::perc: return channelOf (ChannelRole::drums) - 1;
            case Lane::bass: return channelOf (ChannelRole::bass) - 1;
            case Lane::lead: return channelOf (ChannelRole::lead) - 1;
            case Lane::chords: return channelOf (ChannelRole::chords) - 1;
            case Lane::vox: return channelOf (ChannelRole::vox) - 1;
            case Lane::accents: return channelOf (ChannelRole::accents) - 1;
            case Lane::fx: return channelOf (ChannelRole::fx) - 1;
        }
        return channelOf (ChannelRole::drums) - 1;
    }

    class MidiRouter
    {
    public:
        static constexpr int kMaxInstruments = 256; // instruments with live round-robin counters
        static constexpr int kVoicingChannel = 2; // channel 3, ignored

        // Forgets every held note, throw and round-robin count.
        void reset() noexcept;

        // At a transport start: live round robins count from 0 again.
        void resetRoundRobins() noexcept { counts_.fill (0); }

        // At a kit change: the held notes' voices fade with their Kit, so their note-offs are forgotten.
        void forgetNotes() noexcept;

        // Routes count events (sorted by offset) to the sink, against kit and controls.
        template <class Sink>
        void route (const Kit& kit, const MidiEvent* events, int count, const Controls& controls, Sink& sink) noexcept;

        // Live notes held now (a note-on not yet ended).
        int held() const noexcept;

    private:
        template <class Sink>
        void noteOff (int inChannel, std::uint8_t key, int offset, Sink& sink) noexcept
        {
            auto& slot = held_[static_cast<std::size_t> (inChannel)][key];
            if (slot >= 0)
            {
                sink.liveNoteOff (slot, key, offset);
                slot = -1;
            }
        }

        std::array<std::array<std::int16_t, 128>, 16> held_ = emptyHeld(); // instrument per (input channel, key); -1: none
        std::array<std::array<std::uint8_t, 2>, 16> throws_ {}; // CC91, CC94 per input channel
        std::array<std::uint32_t, kMaxInstruments> counts_ {};

        static std::array<std::array<std::int16_t, 128>, 16> emptyHeld() noexcept
        {
            std::array<std::array<std::int16_t, 128>, 16> h {};
            for (auto& channel : h)
                channel.fill (-1);
            return h;
        }
    };

    template <class Sink>
    void MidiRouter::route (const Kit& kit, const MidiEvent* events, int count, const Controls& controls, Sink& sink) noexcept
    {
        if (events == nullptr)
            return;
        const int laneMode = controls.midiIn == MidiInMode::lane && controls.midiInLane < kLanes
                                 ? laneChannel (static_cast<Lane> (controls.midiInLane))
                                 : -1;
        const auto instruments = static_cast<int> (kit.instruments.size());

        for (int i = 0; i < count; ++i)
        {
            const MidiEvent& e = events[i];
            if (e.status < 0x80 || e.status >= 0xF0)
                continue;
            const int kind = e.status & 0xF0;
            const int inChannel = e.status & 0x0F;
            const int channel = laneMode >= 0 ? laneMode : inChannel;
            const auto data1 = static_cast<std::uint8_t> (e.data1 & 0x7F);
            const auto data2 = static_cast<std::uint8_t> (e.data2 & 0x7F);
            const int offset = e.offset;

            if (kind == 0x90 && data2 > 0)
            {
                if (inChannel == kVoicingChannel)
                    continue;
                const KeyTarget& target = kit.keys[static_cast<std::size_t> (channel)][data1];
                int instrument = target.a;
                if (target.b >= 0 && target.ab >= 0 && target.ab < kAbSwitches && controls.ab[static_cast<std::size_t> (target.ab)] != 0)
                    instrument = target.b;
                if (instrument < 0 || instrument >= instruments)
                    continue;

                noteOff (inChannel, data1, offset, sink); // a key struck again while held ends first

                LiveNote note;
                note.instrument = static_cast<std::int16_t> (instrument);
                note.key = data1;
                note.velocity = data2;
                if (instrument < kMaxInstruments)
                    note.rr = static_cast<std::uint8_t> (counts_[static_cast<std::size_t> (instrument)]++ & 0xFFu);
                note.throwSpace = throws_[static_cast<std::size_t> (inChannel)][0];
                note.throwEcho = throws_[static_cast<std::size_t> (inChannel)][1];
                note.offset = offset;
                sink.liveNoteOn (note);
                held_[static_cast<std::size_t> (inChannel)][data1] = static_cast<std::int16_t> (instrument);

                const Instrument& in = kit.instruments[static_cast<std::size_t> (instrument)];
                if (in.duckKey)
                    sink.keyHit (static_cast<std::uint8_t> (in.lane), data2, offset);
            }
            else if (kind == 0x80 || kind == 0x90)
                noteOff (inChannel, data1, offset, sink);
            else if (kind == 0xB0)
            {
                switch (data1)
                {
                    case 74:
                    case 71:
                        sink.cc (static_cast<std::uint8_t> (channel), data1, data2, offset, 0);
                        break;
                    case 91:
                        throws_[static_cast<std::size_t> (inChannel)][0] = data2;
                        break;
                    case 94:
                        throws_[static_cast<std::size_t> (inChannel)][1] = data2;
                        break;
                    case 121:
                        throws_[static_cast<std::size_t> (inChannel)] = { 0, 0 };
                        break;
                    case 120:
                        sink.liveSoundOff (offset);
                        held_ = emptyHeld();
                        break;
                    case 123:
                        sink.liveReleaseAll (offset);
                        held_ = emptyHeld();
                        break;
                    default:
                        break;
                }
            }
        }
    }
}
