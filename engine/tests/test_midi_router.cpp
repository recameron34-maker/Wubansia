// The MIDI router (sched/midi_router.h): live MIDI resolved through the channel map, the keymap and the
// A/B switches, in Multi and Lane mode, with throws, controllers, all-notes-off and all-sound-off.

#include "sched_fixtures.h"

#include "alloc_counter.h"
#include "sched/midi_router.h"

#include <catch2/catch_test_macros.hpp>

#include <vector>

using namespace p7;

namespace
{
    struct Call
    {
        enum class Kind : std::uint8_t {
            on,
            off,
            cc,
            key,
            releaseAll,
            soundOff
        };

        Kind kind = Kind::on;
        int instrument = -1;
        int key = 0; // a cc's controller, a duck key's lane
        int value = 0; // a note-on's velocity, a cc's value, a duck key's velocity
        int channel = -1; // a cc's
        int offset = 0;
        LiveNote note;
    };

    struct RouterSink
    {
        std::vector<Call> calls;

        void liveNoteOn (const LiveNote& n) noexcept
        {
            Call c;
            c.kind = Call::Kind::on;
            c.instrument = n.instrument;
            c.key = n.key;
            c.value = n.velocity;
            c.offset = n.offset;
            c.note = n;
            calls.push_back (c);
        }

        void liveNoteOff (int instrument, std::uint8_t key, int offset) noexcept
        {
            Call c;
            c.kind = Call::Kind::off;
            c.instrument = instrument;
            c.key = key;
            c.offset = offset;
            calls.push_back (c);
        }

        void cc (std::uint8_t channel, std::uint8_t controller, std::uint8_t value, int offset, int) noexcept
        {
            Call c;
            c.kind = Call::Kind::cc;
            c.channel = channel;
            c.key = controller;
            c.value = value;
            c.offset = offset;
            calls.push_back (c);
        }

        void keyHit (std::uint8_t lane, std::uint8_t velocity, int offset) noexcept
        {
            Call c;
            c.kind = Call::Kind::key;
            c.key = lane;
            c.value = velocity;
            c.offset = offset;
            calls.push_back (c);
        }

        void liveReleaseAll (int offset) noexcept
        {
            Call c;
            c.kind = Call::Kind::releaseAll;
            c.offset = offset;
            calls.push_back (c);
        }

        void liveSoundOff (int offset) noexcept
        {
            Call c;
            c.kind = Call::Kind::soundOff;
            c.offset = offset;
            calls.push_back (c);
        }

        std::vector<Call> only (Call::Kind kind) const
        {
            std::vector<Call> out;
            for (const auto& c : calls)
                if (c.kind == kind)
                    out.push_back (c);
            return out;
        }
    };

    // A channel message; channel 1-16.
    MidiEvent msg (int offset, int kind, int channel, int d1, int d2)
    {
        return { offset, static_cast<std::uint8_t> (kind | ((channel - 1) & 15)), static_cast<std::uint8_t> (d1), static_cast<std::uint8_t> (d2) };
    }

    const Kit& kit()
    {
        static const Kit k = synKit();
        return k;
    }

    std::vector<Call> route (MidiRouter& router, std::vector<MidiEvent> events, const Controls& controls = {})
    {
        RouterSink sink;
        router.route (kit(), events.data(), static_cast<int> (events.size()), controls, sink);
        return sink.calls;
    }
}

TEST_CASE ("the router resolves notes through the channel map and the A/B switches", "[router]")
{
    MidiRouter router;
    Controls a;
    a.ab = { 0, 0, 0 };
    Controls b;
    b.ab = { 1, 1, 1 };

    auto first = [&] (const std::vector<Call>& calls) {
        for (const auto& c : calls)
            if (c.kind == Call::Kind::on)
                return c;
        return Call {};
    };
    CHECK (first (route (router, { msg (0, 0x90, 10, 36, 100) }, a)).instrument == inst::kickA);
    CHECK (first (route (router, { msg (0, 0x90, 10, 36, 100) }, b)).instrument == inst::kickB);
    CHECK (first (route (router, { msg (0, 0x90, 8, 48, 100) }, a)).instrument == inst::chopA);
    CHECK (first (route (router, { msg (0, 0x90, 8, 48, 100) }, b)).instrument == inst::chopB);
    CHECK (first (route (router, { msg (0, 0x90, 1, 40, 100) }, b)).instrument == inst::bass); // no alternative: A either way
    CHECK (first (route (router, { msg (0, 0x90, 4, 60, 100) }, a)).instrument == inst::pad);
    CHECK (first (route (router, { msg (0, 0x90, 10, 42, 100) }, a)).instrument == inst::hatClosed);
    CHECK (first (route (router, { msg (0, 0x90, 7, 48, 100) }, a)).instrument == inst::riser);
}

TEST_CASE ("channel 3, unmapped keys and other messages are ignored", "[router]")
{
    MidiRouter router;
    CHECK (route (router, { msg (0, 0x90, 3, 60, 100), msg (1, 0x80, 3, 60, 0) }).empty()); // the voicing
    CHECK (route (router, { msg (0, 0x90, 10, 37, 100) }).empty()); // no rim in this kit
    CHECK (route (router, { msg (0, 0x90, 9, 60, 100) }).empty()); // reserved
    CHECK (route (router, { msg (0, 0x90, 16, 60, 100) }).empty()); // no extra on 16
    CHECK (route (router, { msg (0, 0x90, 1, 100, 100) }).empty()); // above the bass's keys
    CHECK (route (router, { msg (0, 0xE0, 1, 0, 64), msg (0, 0xC0, 1, 5, 0), msg (0, 0xD0, 1, 90, 0), msg (0, 0xA0, 1, 40, 90) }).empty());
    CHECK (route (router, { { 0, 0xF8, 0, 0 }, { 0, 0x40, 0, 0 } }).empty()); // not channel messages
    CHECK (router.held() == 0);
}

TEST_CASE ("each live note keeps its own note-off, through A/B switches, and a struck held key ends first", "[router]")
{
    MidiRouter router;
    Controls a;
    a.ab = { 0, 0, 0 };
    Controls b;
    b.ab = { 1, 1, 1 };

    auto on = route (router, { msg (5, 0x90, 10, 36, 100) }, a);
    REQUIRE (on.size() == 2u); // the note and its duck key
    CHECK (router.held() == 1);
    auto off = route (router, { msg (7, 0x80, 10, 36, 64) }, b);
    REQUIRE (off.size() == 1u);
    CHECK (off[0].kind == Call::Kind::off);
    CHECK (off[0].instrument == inst::kickA); // what it started with, not what B says now
    CHECK (off[0].offset == 7);
    CHECK (router.held() == 0);

    // A note-on with velocity 0 is a note-off.
    route (router, { msg (0, 0x90, 1, 40, 90) });
    off = route (router, { msg (3, 0x90, 1, 40, 0) });
    REQUIRE (off.size() == 1u);
    CHECK (off[0].kind == Call::Kind::off);
    CHECK (off[0].instrument == inst::bass);

    // Struck again while held: the first note ends, then the new one starts, on the same offset.
    auto twice = route (router, { msg (0, 0x90, 5, 60, 90), msg (10, 0x90, 5, 60, 100), msg (20, 0x80, 5, 60, 0) });
    REQUIRE (twice.size() == 4u);
    CHECK (twice[0].kind == Call::Kind::on);
    CHECK (twice[1].kind == Call::Kind::off);
    CHECK (twice[1].offset == 10);
    CHECK (twice[2].kind == Call::Kind::on);
    CHECK (twice[2].offset == 10);
    CHECK (twice[3].kind == Call::Kind::off);
    CHECK (twice[3].offset == 20);
    CHECK (router.held() == 0);

    // A note-off with nothing held does nothing.
    CHECK (route (router, { msg (0, 0x80, 1, 41, 0) }).empty());
}

TEST_CASE ("Lane mode plays every channel on one lane, through that lane's keymap", "[router]")
{
    MidiRouter router;
    Controls lane;
    lane.midiIn = MidiInMode::lane;
    lane.ab = { 0, 0, 0 };

    lane.midiInLane = static_cast<std::uint8_t> (Lane::bass);
    for (const int ch : { 1, 2, 5, 10, 16 })
    {
        CAPTURE (ch);
        const auto calls = route (router, { msg (0, 0x90, ch, 40, 100), msg (1, 0x80, ch, 40, 0) }, lane);
        REQUIRE (calls.size() == 2u);
        CHECK (calls[0].instrument == inst::bass);
        CHECK (calls[1].instrument == inst::bass);
    }
    CHECK (route (router, { msg (0, 0x90, 3, 40, 100) }, lane).empty()); // the voicing stays ignored

    // A drum lane plays the drum map: every key its own drum.
    lane.midiInLane = static_cast<std::uint8_t> (Lane::hats);
    auto drums = route (router, { msg (0, 0x90, 1, 42, 100), msg (0, 0x90, 1, 36, 100), msg (0, 0x90, 1, 60, 100) }, lane);
    std::vector<int> played;
    for (const auto& c : drums)
        if (c.kind == Call::Kind::on)
            played.push_back (c.instrument);
    CHECK (played == std::vector<int> { inst::hatClosed, inst::kickA });

    lane.midiInLane = static_cast<std::uint8_t> (Lane::vox);
    const auto vox = route (router, { msg (0, 0x90, 7, 60, 100) }, lane);
    REQUIRE (vox.size() == 1u);
    CHECK (vox[0].instrument == inst::vox);

    // Leaving Lane mode while a note is held still ends that note.
    lane.midiInLane = static_cast<std::uint8_t> (Lane::bass);
    route (router, { msg (0, 0x90, 2, 45, 100) }, lane);
    const auto offMulti = route (router, { msg (0, 0x80, 2, 45, 0) }, Controls {});
    REQUIRE (offMulti.size() == 1u);
    CHECK (offMulti[0].instrument == inst::bass);
}

TEST_CASE ("CC74 and CC71 go to the filters of the note's channel; CC91 and CC94 latch per note", "[router]")
{
    MidiRouter router;
    auto calls = route (router, { msg (0, 0xB0, 1, 74, 99), msg (1, 0xB0, 1, 71, 12), msg (2, 0xB0, 1, 7, 100), msg (3, 0xB0, 1, 1, 64) });
    REQUIRE (calls.size() == 2u);
    CHECK (calls[0].kind == Call::Kind::cc);
    CHECK (calls[0].channel == 0);
    CHECK (calls[0].key == 74);
    CHECK (calls[0].value == 99);
    CHECK (calls[0].offset == 0);
    CHECK (calls[1].key == 71);
    CHECK (calls[1].value == 12);

    Controls lane;
    lane.midiIn = MidiInMode::lane;
    lane.midiInLane = static_cast<std::uint8_t> (Lane::chords);
    calls = route (router, { msg (4, 0xB0, 1, 74, 30) }, lane);
    REQUIRE (calls.size() == 1u);
    CHECK (calls[0].channel == 3); // the chords' channel 4

    // Throws latch at the note-on: the note keeps them; the next note gets the new values.
    calls = route (router, { msg (0, 0xB0, 5, 91, 127), msg (0, 0xB0, 5, 94, 64), msg (0, 0x90, 5, 60, 100), msg (5, 0xB0, 5, 91, 0),
                               msg (6, 0x90, 5, 62, 100), msg (7, 0x90, 1, 40, 100) });
    const auto ons = [&] {
        std::vector<Call> out;
        for (const auto& c : calls)
            if (c.kind == Call::Kind::on)
                out.push_back (c);
        return out;
    }();
    REQUIRE (ons.size() == 3u);
    CHECK (ons[0].note.throwSpace == 127);
    CHECK (ons[0].note.throwEcho == 64);
    CHECK (ons[1].note.throwSpace == 0);
    CHECK (ons[1].note.throwEcho == 64);
    CHECK (ons[2].note.throwSpace == 0); // another channel's
    CHECK (ons[2].note.throwEcho == 0);

    // CC121 resets the channel's throws.
    calls = route (router, { msg (0, 0xB0, 5, 121, 0), msg (1, 0x90, 5, 64, 100) });
    REQUIRE (calls.size() == 1u);
    CHECK (calls[0].note.throwEcho == 0);
}

TEST_CASE ("CC123 releases every live note and CC120 fades every live voice", "[router]")
{
    MidiRouter router;
    route (router, { msg (0, 0x90, 1, 40, 100), msg (0, 0x90, 4, 60, 100), msg (0, 0x90, 5, 64, 100) });
    CHECK (router.held() == 3);
    auto calls = route (router, { msg (9, 0xB0, 4, 123, 0) });
    REQUIRE (calls.size() == 1u);
    CHECK (calls[0].kind == Call::Kind::releaseAll);
    CHECK (calls[0].offset == 9);
    CHECK (router.held() == 0);
    // Their note-offs, arriving after, find nothing to end.
    CHECK (route (router, { msg (0, 0x80, 1, 40, 0), msg (0, 0x80, 4, 60, 0) }).empty());

    route (router, { msg (0, 0x90, 10, 46, 100), msg (0, 0x90, 1, 41, 100) });
    calls = route (router, { msg (3, 0xB0, 10, 120, 0) });
    REQUIRE (calls.size() == 1u);
    CHECK (calls[0].kind == Call::Kind::soundOff);
    CHECK (router.held() == 0);
}

TEST_CASE ("live round robins count per instrument and restart at a transport start", "[router]")
{
    MidiRouter router;
    std::vector<int> rr;
    for (int i = 0; i < 3; ++i)
        for (const auto& c : route (router, { msg (0, 0x90, 10, 36, 100), msg (1, 0x80, 10, 36, 0), msg (2, 0x90, 10, 38, 100) }))
            if (c.kind == Call::Kind::on)
                rr.push_back (c.note.rr);
    CHECK (rr == std::vector<int> { 0, 0, 1, 1, 2, 2 }); // kick and clap count apart
    router.resetRoundRobins();
    const auto again = route (router, { msg (0, 0x90, 10, 36, 100) });
    REQUIRE_FALSE (again.empty());
    CHECK (again[0].note.rr == 0);
}

TEST_CASE ("live notes on duck-key instruments key the ducks at their onsets", "[router]")
{
    MidiRouter router;
    const auto calls = route (router, { msg (12, 0x90, 10, 36, 90), msg (20, 0x90, 10, 42, 90), msg (30, 0x90, 10, 38, 70) });
    const auto keys = [&] {
        std::vector<Call> out;
        for (const auto& c : calls)
            if (c.kind == Call::Kind::key)
                out.push_back (c);
        return out;
    }();
    REQUIRE (keys.size() == 2u); // the kick and the clap; the hat keys nothing
    CHECK (keys[0].key == static_cast<int> (Lane::kick));
    CHECK (keys[0].value == 90);
    CHECK (keys[0].offset == 12);
    CHECK (keys[1].key == static_cast<int> (Lane::snare));
    CHECK (keys[1].offset == 30);
}

TEST_CASE ("all 128 notes on all 16 channels, in both modes, are safe and end", "[router]")
{
    MidiRouter router;
    for (const bool lane : { false, true })
        for (int ab = 0; ab < 2; ++ab)
        {
            Controls controls;
            controls.ab = { static_cast<std::uint8_t> (ab), static_cast<std::uint8_t> (ab), static_cast<std::uint8_t> (ab) };
            controls.midiIn = lane ? MidiInMode::lane : MidiInMode::multi;
            controls.midiInLane = static_cast<std::uint8_t> (Lane::accents); // a lane this kit has nothing on
            std::vector<MidiEvent> ons, offs;
            for (int ch = 1; ch <= 16; ++ch)
                for (int key = 0; key < 128; ++key)
                {
                    ons.push_back (msg (0, 0x90, ch, key, 1 + key % 127));
                    offs.push_back (msg (1, 0x80, ch, key, 0));
                }
            RouterSink sink;
            router.route (kit(), ons.data(), static_cast<int> (ons.size()), controls, sink);
            const auto played = sink.only (Call::Kind::on).size();
            if (lane)
                CHECK (played == 0u);
            else
                CHECK (played == 48u + 25u + 37u + 1u + 1u + 4u); // bass 24-71, pad 48-72, vox 48-84, riser, chop, the 4 drums
            CHECK (router.held() == static_cast<int> (played));
            router.route (kit(), offs.data(), static_cast<int> (offs.size()), controls, sink);
            CHECK (sink.only (Call::Kind::off).size() == played);
            CHECK (router.held() == 0);
        }
}

TEST_CASE ("routing allocates nothing", "[router][alloc]")
{
    if (! allocations::available())
        SKIP ("a sanitizer owns the allocation functions in this build");
    struct Quiet
    {
        int calls = 0;
        void liveNoteOn (const LiveNote&) noexcept { ++calls; }
        void liveNoteOff (int, std::uint8_t, int) noexcept { ++calls; }
        void cc (std::uint8_t, std::uint8_t, std::uint8_t, int, int) noexcept { ++calls; }
        void keyHit (std::uint8_t, std::uint8_t, int) noexcept { ++calls; }
        void liveReleaseAll (int) noexcept { ++calls; }
        void liveSoundOff (int) noexcept { ++calls; }
    };
    const std::array<MidiEvent, 8> events { msg (0, 0x90, 10, 36, 100), msg (1, 0xB0, 1, 74, 3), msg (2, 0xB0, 5, 91, 127), msg (3, 0x90, 5, 60, 100),
        msg (4, 0x80, 10, 36, 0), msg (5, 0xB0, 1, 123, 0), msg (6, 0xB0, 1, 120, 0), msg (7, 0x90, 1, 40, 1) };
    MidiRouter router;
    Quiet sink;
    const Kit& k = kit();
    const Controls controls;
    allocations::Scope scope;
    for (int i = 0; i < 100; ++i)
        router.route (k, events.data(), static_cast<int> (events.size()), controls, sink);
    const auto counts = scope.counts();
    CHECK (counts.none());
    CHECK (sink.calls > 0);
}
