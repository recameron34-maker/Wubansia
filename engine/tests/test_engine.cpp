// The engine's block loop (engine_loop.h) on recording voices, and the Engine itself (P5's Sampler, P0's
// pass-through mixer): what reaches the voices and when, kit switches, Off, telemetry, and real-time
// safety.

#include "sched_fixtures.h"

#include "alloc_counter.h"
#include "engine.h"
#include "engine_loop.h"
#include "testing/fake_host.h"
#include "testing/synthetic_kit.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <map>
#include <memory>
#include <thread>
#include <tuple>
#include <vector>

using namespace p7;
using wubansia::engine::testing::FakeHost;
using Item = RecordingVoices::Item;

namespace
{
    using Harness = LoopHarness<RecordingVoices>;

    Timeline loopTimeline (std::vector<midi::TrackSpec> tracks, const KitManifest& m = synManifest(), const MixConfig& mix = synMix())
    {
        return build (smf (std::move (tracks), 15360), m, mix, 15360);
    }

    // The synthetic loop, but with one timeline per mode and variant: Loop no-vox drops the vox; the songs
    // are 8 bars of kicks with a pad and a riser, sections Intro and Drop.
    Kit switchingKit (int index = 0, bool clean = false)
    {
        const auto m = synManifest();
        const auto mix = clean ? cleanMix() : synMix();
        const auto loopVox = build (synLoopSmf(), m, mix, 15360);
        auto loopNoVox = loopVox;
        std::erase_if (loopNoVox.notes, [] (const Note& n) { return n.channel == 4; });
        loopNoVox.chase.clear();
        for (std::size_t i = 0; i < loopNoVox.notes.size(); ++i)
            if ((loopNoVox.notes[i].flags & NoteFlag::chase) != 0)
                loopNoVox.chase.push_back (static_cast<std::uint32_t> (i));
        std::vector<midi::Note> kick;
        for (int beat = 0; beat < 32; ++beat)
            kick.push_back (note (beat * 960, 120, 10, 36, 127));
        const auto song = build (smf ({ track ("Kick", kick), track ("Chords", { note (0, 15360, 4, 57) }), track ("FX", { note (11520, 3840, 7, 48) }) },
                                     30720, kSynBpm, { { 0, "Intro" }, { 15360, "Drop" } }),
            m, mix, 0);
        return assemble (m, mix, { loopVox, loopNoVox, song, song }, index);
    }

    struct Played
    {
        std::vector<Item> log;
        FakeHost::Output out;
    };

    Played playKit (const Kit& kit, const Controls& controls, double bpm, double fromPpq, std::int64_t samples, double rate = 48000.0,
        int block = 512)
    {
        Harness h;
        FakeHost host (rate, block);
        host.prepare (h);
        h.pending.publish (&kit);
        host.setTempo (bpm);
        host.play (fromPpq);
        Played p;
        host.render (h, controls, samples, p.out);
        p.log = h.loop.voices().log;
        return p;
    }

    std::vector<Item> ons (const std::vector<Item>& log)
    {
        std::vector<Item> out;
        for (const auto& i : log)
            if (i.kind == Item::Kind::on)
                out.push_back (i);
        return out;
    }

    const Item* onOf (const std::vector<Item>& log, int instrument, std::int64_t sample = -1)
    {
        for (const auto& i : log)
            if (i.kind == Item::Kind::on && i.note.instrument == instrument && (sample < 0 || i.sample == sample))
                return &i;
        return nullptr;
    }
}

TEST_CASE ("the block loop plays the timeline on the voices at its samples, into their mix channels", "[engine]")
{
    // A mix with no inserts and the club master off: P8's mixer is then a plain delay of the channel buses'
    // sum, latencySamples () long. One kit-less block with the club off first, since the mixer takes the
    // club master's state at a kit's adoption from its last block's controls.
    const Kit kit = synKit (0, true);
    Controls controls; // Loop, vox, kick B, bass B, stab A
    controls.club = false;
    Harness h;
    FakeHost host (48000.0, 512);
    host.prepare (h);
    FakeHost::Output out;
    host.render (h, controls, 64, out);
    h.pending.publish (&kit);
    host.setTempo (120.0);
    host.play (0.0);
    host.render (h, controls, 96000, out);

    const auto& log = h.loop.voices().log;
    std::vector<std::int64_t> kicks;
    for (const auto& i : ons (log))
        if (i.note.instrument == inst::kickB)
            kicks.push_back (i.sample - 64);
    CHECK (kicks == std::vector<std::int64_t> { 0, 24000, 48000, 72000 });
    for (const auto& i : ons (log))
    {
        CHECK (i.note.source == Source::sequencer);
        CHECK (i.note.instrument != inst::kickA); // kick B is on
    }

    // The engine reports its mixer's latency, and every note-on's impulse comes out that late, mixed at unity,
    // with nothing else.
    const int latency = h.loop.latencySamples();
    Mixer mixer;
    mixer.prepare (48000.0, 32, 2);
    CHECK (latency == mixer.latencySamples());
    CHECK (latency > 0);
    std::map<std::int64_t, double> expected;
    for (const auto& i : ons (log))
        expected[i.sample + latency] += RecordingVoices::impulse (i.note);
    REQUIRE (expected.begin()->first == 64 + latency);
    for (std::size_t s = 0; s < out.left.size(); ++s)
    {
        const auto it = expected.find (static_cast<std::int64_t> (s));
        const double want = it == expected.end() ? 0.0 : it->second;
        if (std::abs (out.left[s] - want) > 1.0e-6 || std::abs (out.right[s] - 0.5 * want) > 1.0e-6)
        {
            CAPTURE (s, want, out.left[s], out.right[s]);
            FAIL ("the output isn't the impulses, latencySamples () late");
        }
    }
}

TEST_CASE ("note-ons carry the chase, the end alignment, the A/B pre-roll, throws and dips", "[engine]")
{
    SECTION ("a start at 13 quarters chases the pad and the riser")
    {
        for (const double rate : { 48000.0, 44100.0 })
        {
            CAPTURE (rate);
            const Kit kit = synKit();
            const auto played = playKit (kit, Controls {}, 120.0, 13.0, 4800, rate);
            const Item* pad = onOf (played.log, inst::pad, 0);
            const Item* riser = onOf (played.log, inst::riser, 0);
            REQUIRE (pad != nullptr);
            REQUIRE (riser != nullptr);
            CHECK (pad->note.startFrame == 13 * 24000); // 48 kHz frames of time, whatever the rate
            CHECK (pad->note.lengthSamples == 0); // the pad isn't end-aligned
            CHECK (riser->note.startFrame == 24000);
            CHECK (riser->note.lengthSamples == static_cast<std::int64_t> (std::floor (3.0 * 60.0 / 120.0 * rate + 0.5)));
        }
    }

    SECTION ("an end-aligned note's length follows the tempo")
    {
        const Kit kit = synKit();
        const auto played = playKit (kit, Controls {}, 150.0, 0.0, 16 * 19200 + 100);
        const Item* riser = onOf (played.log, inst::riser);
        REQUIRE (riser != nullptr);
        CHECK (riser->sample == 12 * 19200);
        CHECK (riser->note.lengthSamples == 4 * 19200);
        CHECK (riser->note.startFrame == 0);
    }

    SECTION ("an A/B pair moved by the smaller pre-roll: the larger side skips the rest; MIDI in skips all of it")
    {
        const auto m = synManifest();
        const auto mix = synMix();
        const auto t = loopTimeline ({ track ("Vox chops", { note (3840, 240, 8, 48) }) });
        const Kit kit = assemble (m, mix, { t, t, t, t });
        Controls a;
        a.ab = { 1, 1, 0 };
        Controls b;
        b.ab = { 1, 1, 1 };
        const auto withA = playKit (kit, a, 120.0, 0.0, 100000);
        const auto withB = playKit (kit, b, 120.0, 0.0, 100000);
        const Item* chopA = onOf (withA.log, inst::chopA);
        const Item* chopB = onOf (withB.log, inst::chopB);
        REQUIRE (chopA != nullptr);
        REQUIRE (chopB != nullptr);
        CHECK (chopA->sample == 96000 - 480); // 480 frames (the smaller) early
        CHECK (chopB->sample == 96000 - 480);
        CHECK (chopA->note.startFrame == 960 - 480);
        CHECK (chopB->note.startFrame == 0);

        // MIDI in: the vox at its root skips its 2400 frames; an octave up, 1200.
        Harness h;
        FakeHost host (48000.0, 256);
        host.prepare (h);
        h.pending.publish (&kit);
        host.addMidi (100, 0x94, 60, 100);
        host.addMidi (200, 0x94, 72, 100);
        host.addMidi (300, 0x97, 48, 100);
        FakeHost::Output out;
        host.render (h, a, 1024, out);
        const auto live = ons (h.loop.voices().log);
        REQUIRE (live.size() == 3u);
        CHECK (live[0].sample == 100);
        CHECK (live[0].note.source == Source::live);
        CHECK (live[0].note.startFrame == 2400);
        CHECK (live[1].note.startFrame == 1200);
        CHECK (live[2].note.instrument == inst::chopA);
        CHECK (live[2].note.startFrame == 960);
    }

    SECTION ("throws: (1 - the channel's send) x cc / 127 x its static gain")
    {
        auto m = synManifest();
        auto mix = synMix();
        const int vox = mix.channelIndex ("vox");
        mix.channels[static_cast<std::size_t> (vox)].sendsPm = { 250, 100, 0 };
        Insert gain;
        gain.type = InsertType::gain;
        gain.params = GainParams { -6000 };
        mix.channels[static_cast<std::size_t> (vox)].inserts.push_back (gain);
        Insert novoxOnly = gain;
        novoxOnly.params = GainParams { -12000 };
        novoxOnly.when.variant = 1;
        mix.channels[static_cast<std::size_t> (vox)].inserts.push_back (novoxOnly);
        const auto t = loopTimeline ({ track ("Vox", { note (960, 480, 5, 60), note (3840, 480, 5, 62) }, { cc (960, 5, 91, 127), cc (960, 5, 94, 64), cc (1440, 5, 91, 0), cc (1440, 5, 94, 0) }) }, m, mix);
        const Kit kit = assemble (m, mix, { t, t, t, t });
        const auto played = playKit (kit, Controls {}, 120.0, 0.0, 120000);
        const auto all = ons (played.log);
        REQUIRE (all.size() == 2u);
        const double g = std::pow (10.0, -6000 / 20000.0);
        CHECK (all[0].note.throwSpace == Catch::Approx (0.75 * g).epsilon (1e-6));
        CHECK (all[0].note.throwEcho == Catch::Approx (0.9 * 64.0 / 127.0 * g).epsilon (1e-6));
        CHECK_FALSE (all[1].note.throwSpace > 0.0f);
        CHECK_FALSE (all[1].note.throwEcho > 0.0f);

        Controls novox;
        novox.variant = Variant::novox;
        const auto quieter = ons (playKit (kit, novox, 120.0, 0.0, 30000).log);
        REQUIRE_FALSE (quieter.empty());
        CHECK (quieter[0].note.throwSpace == Catch::Approx (0.75 * g * std::pow (10.0, -12000 / 20000.0)).epsilon (1e-6));
    }

    SECTION ("dips reach the voices on both notes of the pair; MIDI in never sets them")
    {
        const auto t = loopTimeline ({ track ("Bass", { note (0, 250, 1, 36), note (240, 250, 1, 38), note (480, 200, 1, 36), note (960, 200, 1, 40) }) });
        const Kit kit = assemble (synManifest(), synMix(), { t, t, t, t });
        const auto all = ons (playKit (kit, Controls {}, 120.0, 0.0, 30000).log);
        REQUIRE (all.size() == 4u);
        CHECK_FALSE (all[0].note.dip);
        CHECK (all[1].note.dip);
        CHECK (all[2].note.dip);
        CHECK_FALSE (all[3].note.dip);
    }
}

namespace
{
    // The loop's bass line as P6's annotatePhrases encodes a matched line: each note stays, as its
    // multisample twin (phrase 0, slice -1), and a slice note sits beside it (phrase 0, slice k, on the
    // note's onset, continuing into the next). The phrase plays on the vox channel, so its voices are told
    // apart; its cycle is 2 bars; a thrown first note.
    Kit phraseKit (PhraseThrow throwMode = PhraseThrow::live)
    {
        auto m = synManifest();
        const auto mix = synMix();
        auto smfBytes = synLoopSmf();
        Timeline t = build (smfBytes, m, mix, 15360);
        std::vector<Note> slices;
        std::int16_t k = 0;
        for (auto& n : t.notes)
            if (n.channel == 0)
            {
                n.phrase = 0;
                Note s = n;
                s.slice = k++;
                s.flags = static_cast<std::uint8_t> (NoteFlag::chase | NoteFlag::continues);
                if (k == 1)
                    s.throwSpace = n.throwSpace = 127;
                slices.push_back (s);
            }
        t.notes.insert (t.notes.end(), slices.begin(), slices.end());
        std::stable_sort (t.notes.begin(), t.notes.end(), [] (const Note& a, const Note& b) {
            return std::tie (a.pos, a.channel, a.key) < std::tie (b.pos, b.channel, b.key);
        });
        t.chase.clear();
        for (std::size_t i = 0; i < t.notes.size(); ++i)
            if ((t.notes[i].flags & NoteFlag::chase) != 0)
                t.chase.push_back (static_cast<std::uint32_t> (i));
        buildSliceThrough (t);
        Kit kit = assemble (m, mix, { t, t, t, t });
        Phrase phrase;
        phrase.lane = Lane::bass;
        phrase.channel = 0;
        phrase.ab = static_cast<std::int8_t> (AbSwitch::bass);
        phrase.mixChannel = static_cast<std::uint8_t> (mix.channelIndex ("vox"));
        phrase.tolerancePm = 20;
        phrase.cycle16ths = 32;
        phrase.throwMode = throwMode;
        kit.phrases.push_back (phrase);
        kit.slices.resize (static_cast<std::size_t> (k));
        return kit;
    }

    // The bass's note-ons: slice notes (phrase 0, slice >= 0) or multisample ones.
    std::vector<Item> bassOns (const std::vector<Item>& log)
    {
        std::vector<Item> out;
        for (const auto& i : ons (log))
            if (i.note.instrument == inst::bass)
                out.push_back (i);
        return out;
    }
}

TEST_CASE ("a phrase plays its slice notes near the kit's tempo and its twins elsewhere, decided once per cycle", "[engine][phrase]")
{
    const Kit kit = phraseKit();

    SECTION ("within the tolerance: slices, with the stem their A/B picks, no note-offs, the twins silent")
    {
        for (const double factor : { 1.0, 1.019, 0.981 })
        {
            CAPTURE (factor);
            const auto played = playKit (kit, Controls {}, 120.0 * factor, 0.0, 200000);
            const auto bass = bassOns (played.log);
            REQUIRE (bass.size() >= 8u);
            for (const auto& i : bass)
            {
                CHECK (i.note.phrase == 0);
                CHECK (i.note.slice >= 0);
                CHECK (i.note.character == 1); // bass B
                // To its twin's end: 200 ticks at the kit's tempo (5005 where the swung start moved 0.2 earlier).
                if (factor > 0.99999 && factor < 1.00001)
                    CHECK ((i.note.lengthSamples == 5000 || i.note.lengthSamples == 5005));
                else
                    CHECK (i.note.lengthSamples > 0);
            }
            for (const auto& i : played.log)
                CHECK_FALSE ((i.kind == Item::Kind::off && i.instrument == inst::bass));
        }
        Controls bassA;
        bassA.ab = { 1, 0, 0 };
        CHECK (bassOns (playKit (kit, bassA, 120.0, 0.0, 30000).log).front().note.character == 0);
    }

    SECTION ("past the tolerance, or with phrase lanes off: the twins, with their note-offs")
    {
        Controls off;
        off.phrases = PhraseMode::off;
        for (const auto& [factor, controls] : { std::pair { 1.021, Controls {} }, std::pair { 0.979, Controls {} }, std::pair { 1.5, Controls {} },
                 std::pair { 1.0, off } })
        {
            CAPTURE (factor);
            const auto played = playKit (kit, controls, 120.0 * factor, 0.0, 200000);
            const auto bass = bassOns (played.log);
            REQUIRE (bass.size() >= 8u);
            std::size_t offs = 0;
            for (const auto& i : played.log)
                offs += (i.kind == Item::Kind::off && i.instrument == inst::bass) ? 1 : 0;
            for (const auto& i : bass)
            {
                CHECK (i.note.phrase == -1);
                CHECK (i.note.slice == -1);
            }
            CHECK (offs + 1 >= bass.size()); // the last may still sound
        }
    }

    SECTION ("continues only on the kit's exact tempo")
    {
        const auto exact = bassOns (playKit (kit, Controls {}, 120.0, 0.0, 100000).log);
        const auto near = bassOns (playKit (kit, Controls {}, 121.2, 0.0, 100000).log); // +1 %: slices, each its own head
        REQUIRE_FALSE (exact.empty());
        REQUIRE_FALSE (near.empty());
        for (const auto& i : exact)
            CHECK (i.note.continues);
        for (const auto& i : near)
        {
            CHECK (i.note.slice >= 0);
            CHECK_FALSE (i.note.continues);
        }
    }

    SECTION ("a tempo change mid-cycle keeps the cycle's decision; the next cycle decides again")
    {
        Harness h;
        FakeHost host (48000.0, 512);
        host.prepare (h);
        h.pending.publish (&kit);
        host.setTempo (120.0);
        host.play (0.0);
        FakeHost::Output out;
        host.render (h, Controls {}, 96256, out); // a bar and a block, at the kit's tempo
        host.setTempo (130.0); // past the tolerance, a bar into the 2-bar cycle
        host.render (h, Controls {}, 200000, out);
        // The cycle ends on bar 3: 96256 samples at 120, then (8 quarters - that) at 130.
        const double rest = (8.0 - 96256.0 / 24000.0) * 60.0 / 130.0 * 48000.0;
        const auto boundary = static_cast<std::int64_t> (96256.0 + rest);
        int before = 0, after = 0;
        for (const auto& i : bassOns (h.loop.voices().log))
        {
            if (i.sample < boundary - 1)
            {
                ++before;
                CHECK (i.note.slice >= 0);
            }
            else if (i.sample > boundary + 1)
            {
                ++after;
                CHECK (i.note.slice < 0);
            }
        }
        CHECK (before >= 12);
        CHECK (after >= 4);
    }

    SECTION ("throws: a slice note's from the phrase's channel, held to its twin's end; none for a baked phrase; twins keep theirs")
    {
        const auto live = bassOns (playKit (kit, Controls {}, 120.0, 0.0, 30000).log);
        REQUIRE_FALSE (live.empty());
        CHECK (live.front().note.throwSpace > 0.0f);
        CHECK (live.front().note.slice >= 0);
        CHECK (live.front().note.lengthSamples == 200 * 25); // its twin, 200 ticks long at 25 samples a tick
        // Chased 0.05 quarters (1200 samples) into it: what is left of its twin.
        const auto chased = bassOns (playKit (kit, Controls {}, 120.0, 0.05, 30000).log);
        REQUIRE_FALSE (chased.empty());
        CHECK (chased.front().note.startFrame == 1200);
        CHECK (chased.front().note.lengthSamples == 200 * 25 - 1200);
        const Kit baked = phraseKit (PhraseThrow::baked);
        const auto none = bassOns (playKit (baked, Controls {}, 120.0, 0.0, 30000).log);
        REQUIRE_FALSE (none.empty());
        CHECK_FALSE (none.front().note.throwSpace > 0.0f);
        CHECK (none.front().note.lengthSamples == 0);
        const auto twins = bassOns (playKit (baked, Controls {}, 130.0, 0.0, 30000).log);
        REQUIRE_FALSE (twins.empty());
        CHECK (twins.front().note.slice < 0);
        CHECK (twins.front().note.throwSpace > 0.0f);
    }

    SECTION ("the telemetry: twins light the lane either way; the voices say which lanes play recordings")
    {
        Harness h;
        FakeHost host (48000.0, 512);
        host.prepare (h);
        h.pending.publish (&kit);
        host.setTempo (120.0);
        host.play (0.0);
        FakeHost::Output out;
        host.render (h, Controls {}, 24064, out);
        CHECK ((h.telemetry.laneHits.exchange (0) & laneBit (Lane::bass)) != 0u);
        CHECK (h.telemetry.phraseRecorded.load() == laneBit (Lane::bass));
        host.setTempo (130.0);
        host.render (h, Controls {}, 200000, out);
        CHECK ((h.telemetry.laneHits.exchange (0) & laneBit (Lane::bass)) != 0u);
        CHECK (h.telemetry.phraseRecorded.load() == 0u);

        // MIDI in on the phrase's channel plays the multisample.
        host.stop();
        host.addMidi (host.samplePosition() + 10, 0x90, 40, 100);
        host.render (h, Controls {}, 512, out);
        const auto live = ons (h.loop.voices().log);
        REQUIRE_FALSE (live.empty());
        CHECK (live.back().note.source == Source::live);
        CHECK (live.back().note.phrase == -1);
        CHECK (live.back().note.slice == -1);
    }
}

TEST_CASE ("Off plays only MIDI in", "[engine]")
{
    const Kit kit = synKit();
    Harness h;
    FakeHost host (48000.0, 512);
    host.prepare (h);
    h.pending.publish (&kit);
    host.setTempo (120.0);
    host.play (0.0);
    Controls off;
    off.mode = PlayMode::off;
    host.addMidi (1000, 0x99, 36, 100);
    host.addMidi (2000, 0x89, 36, 0);
    FakeHost::Output out;
    host.render (h, off, 48000, out);
    const auto all = ons (h.loop.voices().log);
    REQUIRE (all.size() == 1u);
    CHECK (all[0].note.source == Source::live);
    CHECK (all[0].sample == 1000);
    CHECK (all[0].note.instrument == inst::kickB);
}

TEST_CASE ("a kit is adopted at once when there is none, after a 20 ms fade while stopped, and on the next bar line while playing", "[engine][kit]")
{
    // The drone (a constant into the kick's mix channel) shows the output's gain. The mix has no inserts and
    // the club master is off, so the mixer is a plain delay (see the first test); a kit's adoption resets its
    // tails, so the drone is back latencySamples () after each switch.
    const Kit a = switchingKit (0, true), b = switchingKit (1, true), c = switchingKit (2, true);
    Harness h;
    FakeHost host (48000.0, 64);
    host.prepare (h);
    h.loop.voices().drone = 0.5f;
    host.setTempo (120.0);
    Controls controls;
    controls.club = false;
    FakeHost::Output out;
    const auto at = [&] (std::int64_t s) { return static_cast<double> (out.left[static_cast<std::size_t> (s)]); };
    host.render (h, controls, 1024, out); // no kit yet: the buses at unity, as late as a mix
    const int latency = h.loop.latencySamples();
    REQUIRE (latency > 0);
    REQUIRE (latency < 480); // the checks below sit 480 samples after a switch: the drone is back by then
    CHECK (at (latency + 10) == Catch::Approx (0.5));

    // The first kit: at once, with no fade.
    h.pending.publish (&a);
    host.render (h, controls, 64, out);
    CHECK (h.telemetry.kitIndex.load() == 0);
    host.render (h, controls, 1024, out);
    CHECK (at (1024 + latency + 10) == Catch::Approx (0.5));

    // Stopped: the output fades out over 960 samples (20 ms), then the switch, then back in over 960.
    const auto t0 = host.samplePosition();
    h.pending.publish (&b);
    host.render (h, controls, 64 * 15, out); // 960 samples
    CHECK (h.telemetry.kitIndex.load() == 0);
    host.render (h, controls, 64, out);
    CHECK (h.telemetry.kitIndex.load() == 1);
    CHECK (at (t0) == Catch::Approx (0.5));
    CHECK (at (t0 + 480) == Catch::Approx (0.25).margin (1e-6));
    CHECK (at (t0 + 960) == Catch::Approx (0.0).margin (1e-9)); // the switch's sample
    host.render (h, controls, 64 * 15, out);
    CHECK (at (t0 + 960 + 480) == Catch::Approx (0.25).margin (1e-6));
    CHECK (at (t0 + 960 + 960) == Catch::Approx (0.5));
    CHECK (h.retired.pop() == &a); // nothing held it
    CHECK (h.retired.pop() == nullptr);

    // Playing: c, published 2.5 quarters in, is adopted on the bar line at 4 quarters.
    const auto start = host.samplePosition();
    host.play (0.0);
    host.render (h, controls, 60000, out);
    h.pending.publish (&c);
    host.render (h, controls, 40000, out);
    CHECK (h.telemetry.kitIndex.load() == 2);
    const auto bar = start + 96000;
    const auto& log = h.loop.voices().log;
    bool faded = false;
    for (const auto& i : log)
    {
        if (i.kind == Item::Kind::fadeKit && i.sample >= start)
        {
            faded = true;
            CHECK (i.kit == &b);
            CHECK (i.sample == bar);
        }
        if (i.kind == Item::Kind::on && i.note.source == Source::sequencer && i.sample >= start)
            CHECK (i.kit == (i.sample < bar ? &b : &c));
    }
    CHECK (faded);
    const Item* kick = nullptr;
    for (const auto& i : log)
        if (i.kind == Item::Kind::on && i.sample == bar && i.note.instrument == inst::kickB)
            kick = &i;
    REQUIRE (kick != nullptr); // beat 4's kick, on the new kit
    CHECK (kick->kit == &c);
    // The output fades out over the 960 samples before the bar line and back in over the 960 after (no note
    // comes out on the samples checked: the drone alone shows the gain). On the switch's sample it is 0.
    CHECK (at (bar - 961) == Catch::Approx (0.5).margin (1e-6));
    CHECK (at (bar - 480) == Catch::Approx (0.25).margin (1e-6));
    CHECK (at (bar) == Catch::Approx (0.0).margin (1e-9));
    CHECK (at (bar + 480) == Catch::Approx (0.25).margin (1e-6));
    CHECK (at (bar + 1000) == Catch::Approx (0.5).margin (1e-6));
    // b's voices fade within 240 samples, then b retires.
    CHECK (h.retired.pop() == &b);
}

TEST_CASE ("a kit waits for room in the retire FIFO", "[engine][kit]")
{
    const Kit a = switchingKit (0), b = switchingKit (1);
    std::vector<Kit> dummies (16);
    Harness h;
    FakeHost host (48000.0, 512);
    host.prepare (h);
    h.pending.publish (&a);
    FakeHost::Output out;
    host.render (h, Controls {}, 512, out);
    for (auto& d : dummies)
        REQUIRE (h.retired.push (&d)); // the service hasn't collected anything: the FIFO is full
    h.pending.publish (&b);
    host.render (h, Controls {}, 48000, out);
    CHECK (h.telemetry.kitIndex.load() == 0);
    CHECK (h.retired.pop() == &dummies[0]);
    host.render (h, Controls {}, 4096, out);
    CHECK (h.telemetry.kitIndex.load() == 1);
}

TEST_CASE ("mode, variant, A/B and kit switches mid-note end the old notes at their own note-offs or fade them", "[engine][switch]")
{
    const Kit a = switchingKit (0), b = switchingKit (1);
    Harness h;
    FakeHost host (48000.0, 512);
    host.prepare (h);
    h.pending.publish (&a);
    host.setTempo (120.0);
    host.play (0.0);
    FakeHost::Output out;
    Controls controls;
    host.render (h, controls, 30000, out);
    controls.variant = Variant::novox;
    host.render (h, controls, 30000, out);
    controls.ab = { 0, 0, 0 };
    host.render (h, controls, 30000, out);
    controls.mode = PlayMode::song;
    host.render (h, controls, 300000, out); // to 390000: the loop's pad ends on 384000 under the song
    h.pending.publish (&b);
    host.render (h, controls, 200000, out);
    controls.mode = PlayMode::loop;
    host.render (h, controls, 100000, out);
    host.stop();
    host.render (h, controls, 512, out);

    // Every sequenced note-on ends: its own note-off (the instrument it started), or a fade of its kit
    // or of every sequenced voice, after it.
    const auto& log = h.loop.voices().log;
    std::map<std::pair<int, int>, int> open; // (instrument, key) of kit a, then b
    int fades = 0;
    for (const auto& i : log)
    {
        if (i.kind == Item::Kind::on && i.note.source == Source::sequencer)
            ++open[{ i.note.instrument, i.note.key }];
        else if (i.kind == Item::Kind::off && i.source == Source::sequencer)
        {
            auto it = open.find ({ i.instrument, i.key });
            if (it != open.end() && it->second > 0)
                --it->second;
        }
        else if ((i.kind == Item::Kind::fadeOut && i.source == Source::sequencer) || i.kind == Item::Kind::fadeKit)
        {
            ++fades;
            open.clear();
        }
    }
    CHECK (fades >= 2); // the kit switch and the stop
    for (const auto& [note, count] : open)
        CHECK (count == 0);

    // The loop's pad, sounding when the mode went to Song, still ended on its own 4 bars.
    const Item* pad = onOf (log, inst::pad, 0);
    REQUIRE (pad != nullptr);
    bool padEnded = false;
    for (const auto& i : log)
        if (i.kind == Item::Kind::off && i.instrument == inst::pad && i.sample == 384000)
            padEnded = true;
    CHECK (padEnded);
}

TEST_CASE ("the telemetry follows the lanes, the position, the section, the voices and the kit", "[engine][telemetry]")
{
    const Kit kit = switchingKit (3);
    Harness h;
    FakeHost host (48000.0, 512);
    host.prepare (h);
    CHECK (h.telemetry.kitIndex.load() == -1);
    h.pending.publish (&kit);
    host.setTempo (120.0);
    host.play (0.0);
    FakeHost::Output out;
    host.render (h, Controls {}, 384000, out);
    const auto hits = h.telemetry.laneHits.exchange (0);
    CHECK (hits == (laneBit (Lane::kick) | laneBit (Lane::snare) | laneBit (Lane::hats) | laneBit (Lane::bass) | laneBit (Lane::chords)
                       | laneBit (Lane::vox) | laneBit (Lane::fx)));
    CHECK (h.telemetry.kitIndex.load() == 3);
    CHECK (h.telemetry.songTicks.load() == Catch::Approx (0.0).margin (0.5)); // a whole loop: back at its start
    CHECK (h.telemetry.voices.load() == h.loop.voices().activeVoices());
    host.render (h, Controls {}, 512, out);
    CHECK (h.telemetry.voices.load() > 0); // the downbeat's notes

    Controls song;
    song.mode = PlayMode::song;
    host.seek (17.0); // tick 16320: in the Drop
    host.render (h, song, 512, out);
    CHECK (h.telemetry.section.load() == 1);
    CHECK (h.telemetry.songTicks.load() == Catch::Approx (16320.0 + 512.0 / 25.0).margin (0.01));
    host.stop();
    host.render (h, song, 512, out);
    CHECK (h.telemetry.songTicks.load() < 0.0);
    CHECK (h.telemetry.section.load() == -1);
}

TEST_CASE ("the voices and the mixer get pieces that never cross a multiple of 32 samples", "[engine][block]")
{
    const Kit kit = synKit();
    for (const int block : { 1, 17, 33, 100, 512, 4096 })
    {
        CAPTURE (block);
        Harness h;
        FakeHost host (44100.0, block);
        host.prepare (h);
        h.pending.publish (&kit);
        host.setTempo (134.0);
        host.play (0.0);
        FakeHost::Output out;
        host.render (h, Controls {}, 44100, out);
        CHECK (h.loop.voices().longestRender <= 32);
        CHECK (h.loop.voices().now == 44100);
        CHECK (h.loop.voices().renders >= 44100 / 32);
    }
}

TEST_CASE ("the engine plays the synthetic kit without allocating or freeing, through a kit adoption, MIDI in and every switch", "[engine][alloc]")
{
    if (! allocations::available())
        SKIP ("a sanitizer owns the allocation functions in this build");

    // P0's SYN01 with P7's timelines, and a second copy to switch to.
    auto layOut = [] (int index) {
        wubansia::engine::testing::SyntheticSampleSource source;
        const auto& content = wubansia::engine::testing::syntheticContent();
        auto laid = wubansia::engine::testing::layOutKit (content.kit, content.mix, source);
        REQUIRE (laid.ok());
        auto kit = laid.value;
        for (std::size_t i = 0; i < 4; ++i)
        {
            TimelineOptions options;
            options.loopTicks = i < 2 ? 15360 : 0;
            auto t = buildTimeline (content.smfs[i], content.kit, content.mix, options);
            REQUIRE (t.ok());
            kit->timelines[i] = std::move (t.value);
        }
        kit->info.index = index;
        return kit;
    };
    const auto first = layOut (0);
    const auto second = layOut (1);

    for (const int outputs : { 2, 1 })
    {
        Engine engine;
        engine.prepare (44100.0, 512, outputs);
        std::vector<float> left (4096), right (4096);
        float* out[2] { left.data(), right.data() };
        engine.publish (first.get());
        TransportInfo t;
        t.hasPosition = true;
        t.playing = true;
        t.bpm = 128.0;
        Controls controls;
        const MidiEvent midi[] { { 0, 0x99, 36, 100 }, { 3, 0xB0, 74, 30 }, { 7, 0xB4, 91, 127 }, { 9, 0x94, 64, 100 }, { 100, 0x84, 64, 0 },
            { 200, 0x89, 36, 0 }, { 300, 0xB0, 123, 0 }, { 301, 0xB0, 120, 0 } };

        allocations::Scope scope;
        double ppq = 0.0;
        for (int block = 0; block < 400; ++block)
        {
            const int n = block % 7 == 0 ? 4096 : 512; // a block larger than prepared, too
            if (block == 100)
                engine.publish (second.get());
            if (block == 150)
                controls.mode = PlayMode::song;
            if (block == 200)
                controls.variant = Variant::novox;
            if (block == 250)
                controls.ab = { 0, 0, 1 };
            if (block == 300)
                t.playing = false;
            if (block == 320)
            {
                t.playing = true;
                ppq = 33.0; // a jump
            }
            if (block == 350)
                controls.mode = PlayMode::off;
            t.ppq = ppq;
            engine.process (out, outputs, n, t, controls, midi, block % 3 == 0 ? 8 : 0);
            if (t.playing)
                ppq += n * 128.0 / (60.0 * 44100.0);
        }
        const auto counts = scope.counts();
        CHECK (counts.allocations == 0);
        CHECK (counts.frees == 0);
        CHECK (engine.telemetry().kitIndex.load() == 1);
        for (int i = 0; i < 512; ++i)
        {
            CHECK (std::isfinite (left[static_cast<std::size_t> (i)]));
            if (outputs == 2)
                CHECK (std::isfinite (right[static_cast<std::size_t> (i)]));
        }
        std::vector<const Kit*> retired;
        engine.collectRetired ([&retired] (const Kit* k) { retired.push_back (k); });
        CHECK (retired == std::vector<const Kit*> { first.get() });
    }
}

TEST_CASE ("the engine prepares again at another rate, resets, and renders silence with no kit", "[engine]")
{
    Engine engine;
    std::vector<float> left (1024, 1.0f), right (1024, 1.0f);
    float* out[2] { left.data(), right.data() };
    TransportInfo t;
    t.hasPosition = true;
    t.playing = true;
    t.bpm = 120.0;
    for (const double rate : { 11025.0, 192000.0, 48000.0 })
    {
        engine.prepare (rate, 256, 2);
        engine.process (out, 2, 1024, t, Controls {}, nullptr, 0);
        CHECK (std::all_of (left.begin(), left.end(), [] (float x) { return ! (x < 0.0f) && ! (x > 0.0f); }));
        engine.reset();
    }
    CHECK (engine.telemetry().kitIndex.load() == -1);
}

TEST_CASE ("kits published from a service thread are adopted, faded and handed back, none lost or handed back twice", "[engine][kit][threads]")
{
    // 40 distinct kits, published one after another from a "service" thread that also collects what the
    // engine retires; the audio thread renders meanwhile, stopped (each switch after its 20 ms fade) and
    // then playing (switches on bar lines), and on for a bar after the last publish, so a switch the
    // engine has planned (its kit taken from the slot) lands. Then every kit is accounted for exactly
    // once: handed back unadopted, retired, still pending, or the one playing.
    constexpr int count = 40;
    std::vector<std::unique_ptr<Kit>> kits;
    for (int i = 0; i < count; ++i)
        kits.push_back (std::make_unique<Kit> (synKit (i)));

    for (const bool playing : { false, true })
    {
        CAPTURE (playing);
        Engine engine;
        engine.prepare (48000.0, 256, 2);
        std::atomic<bool> done { false };
        std::vector<const Kit*> unadopted, retired;
        std::thread service ([&] {
            for (int i = 0; i < count; ++i)
            {
                if (const Kit* back = engine.publish (kits[static_cast<std::size_t> (i)].get()))
                    unadopted.push_back (back);
                // Until the engine adopts it, or 100 ms: at any speed (a sanitizer's too) kits are adopted in
                // turn, and a slow audio thread still sees later publishes replace earlier ones.
                const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds (100);
                while (engine.telemetry().kitIndex.load() != i && std::chrono::steady_clock::now() < deadline)
                {
                    engine.collectRetired ([&retired] (const Kit* k) { retired.push_back (k); });
                    std::this_thread::sleep_for (std::chrono::microseconds (100));
                }
                engine.collectRetired ([&retired] (const Kit* k) { retired.push_back (k); });
            }
            done.store (true);
        });

        std::vector<float> left (256), right (256);
        float* out[2] { left.data(), right.data() };
        TransportInfo t;
        t.hasPosition = true;
        t.playing = playing;
        t.bpm = 480.0; // a bar every half second
        double ppq = 0.0;
        int blocks = 0;
        int after = 0; // blocks after the service's last publish: enough for a planned switch (a bar) to land
        while (blocks < 2000 || after < 200)
        {
            if (done.load())
                ++after;
            t.ppq = ppq;
            engine.process (out, 2, 256, t, Controls {}, nullptr, 0);
            if (playing)
                ppq += 256.0 * 480.0 / (60.0 * 48000.0);
            ++blocks;
            if (blocks > 2000000)
                break;
        }
        service.join();
        engine.collectRetired ([&retired] (const Kit* k) { retired.push_back (k); });
        const Kit* pending = engine.publish (nullptr);

        std::vector<int> seen (count, 0);
        auto mark = [&] (const Kit* k) {
            for (int i = 0; i < count; ++i)
                if (kits[static_cast<std::size_t> (i)].get() == k)
                    ++seen[static_cast<std::size_t> (i)];
        };
        for (const Kit* k : unadopted)
            mark (k);
        for (const Kit* k : retired)
            mark (k);
        if (pending != nullptr)
            mark (pending);
        const int playingIndex = engine.telemetry().kitIndex.load();
        REQUIRE (playingIndex >= 0);
        ++seen[static_cast<std::size_t> (playingIndex)];
        for (int i = 0; i < count; ++i)
        {
            CAPTURE (i);
            CHECK (seen[static_cast<std::size_t> (i)] == 1);
        }
        CHECK (retired.size() >= 2u); // some were adopted in turn
    }
}

TEST_CASE ("a kit published while it is the adopted one goes straight back: no fade, no restart, no fading slot", "[engine][kit]")
{
    // The drone shows the output's gain, through a mix with no inserts and the club master off (a plain delay;
    // see the first test). After the first adoption's reset it is back latencySamples () later.
    const Kit a = switchingKit (0, true), b = switchingKit (1, true);
    Controls controls;
    controls.club = false;

    SECTION ("stopped")
    {
        Harness h;
        FakeHost host (48000.0, 64);
        host.prepare (h);
        h.loop.voices().drone = 0.5f;
        FakeHost::Output out;
        host.render (h, controls, 64, out);
        h.pending.publish (&a);
        host.render (h, controls, 64, out);
        REQUIRE (h.telemetry.kitIndex.load() == 0);
        h.pending.publish (&a); // the user picks the current kit again
        host.render (h, controls, 2048, out);
        CHECK (h.telemetry.kitIndex.load() == 0);
        CHECK (h.loop.kit() == &a);
        CHECK (h.loop.fadingKits() == 0);
        CHECK (h.retired.pop() == &a); // the duplicate publish, for the service to drop
        CHECK (h.retired.pop() == nullptr);
        for (const auto& i : h.loop.voices().log)
            CHECK (i.kind != Item::Kind::fadeKit);
        for (std::size_t s = 64 + static_cast<std::size_t> (h.loop.latencySamples()); s < out.left.size(); ++s)
            if (! (out.left[s] == Catch::Approx (0.5)))
            {
                CAPTURE (s, out.left[s]);
                FAIL ("the output faded");
            }
    }

    SECTION ("playing, and A, B, A within one bar")
    {
        for (const bool viaB : { false, true })
        {
            CAPTURE (viaB);
            Harness h;
            FakeHost host (48000.0, 512);
            host.prepare (h);
            h.loop.voices().drone = 0.5f;
            FakeHost::Output out;
            host.render (h, controls, 64, out);
            h.pending.publish (&a);
            host.setTempo (120.0);
            host.play (0.0);
            host.render (h, controls, 60000, out); // 2.5 quarters in
            if (viaB)
                CHECK (h.pending.publish (&b) == nullptr);
            const Kit* back = h.pending.publish (&a);
            CHECK (back == (viaB ? &b : nullptr)); // B, never adopted, comes back to the service at once
            host.render (h, controls, 100000, out); // past the bar line at 64 + 96000
            CHECK (h.telemetry.kitIndex.load() == 0);
            CHECK (h.loop.kit() == &a);
            CHECK (h.loop.fadingKits() == 0);
            CHECK (h.retired.pop() == &a);
            CHECK (h.retired.pop() == nullptr);
            for (const auto& i : h.loop.voices().log)
            {
                CHECK (i.kind != Item::Kind::fadeKit);
                if (i.kind == Item::Kind::on && i.sample > 64)
                    CHECK (i.note.startFrame == 0); // no restart: nothing chased
            }
            // The drone never dips: no fade around the bar line.
            for (std::size_t s = 64 + 90000; s < 64 + 100000; ++s)
                CHECK (out.left[s] >= 0.5f - 1.0e-6f);
        }
    }
}

TEST_CASE ("an offline render adopts a pending kit on the block's first sample, with no bar line to wait for and no fades", "[engine][kit][offline]")
{
    const Kit a = switchingKit (0, true), b = switchingKit (1, true);
    Controls controls;
    controls.club = false; // the mixer a plain delay (see the first test)
    for (const bool playing : { true, false })
    {
        CAPTURE (playing);
        Harness h;
        FakeHost host (48000.0, 512);
        host.prepare (h);
        h.loop.voices().drone = 0.5f;
        FakeHost::Output out;
        host.render (h, controls, 64, out);
        h.pending.publish (&a);
        host.setTempo (120.0);
        if (playing)
            host.play (0.0);
        host.render (h, controls, 60416, out); // 118 blocks: mid-bar
        REQUIRE (h.telemetry.kitIndex.load() == 0);

        h.pending.publish (&b);
        host.setNonRealtime (true);
        const auto first = host.samplePosition();
        host.render (h, controls, 512, out);
        CHECK (h.telemetry.kitIndex.load() == 1);
        bool faded = false;
        for (const auto& i : h.loop.voices().log)
        {
            if (i.kind == Item::Kind::fadeKit)
            {
                faded = true;
                CHECK (i.kit == &a);
                CHECK (i.sample == first);
            }
            if (i.kind == Item::Kind::on && i.note.source == Source::sequencer)
                CHECK (i.kit == (i.sample < first ? &a : &b));
        }
        CHECK (faded);
        if (playing)
        {
            // The new kit's timeline is chased at once: its pad, 2.5 quarters in.
            const Item* pad = onOf (h.loop.voices().log, inst::pad, first);
            REQUIRE (pad != nullptr);
            CHECK (pad->kit == &b);
            CHECK (pad->note.startFrame == 60416);
        }
        // No fade out before it; and none in after it: the drone is back whole once through the mixer, whose
        // tails the switch reset.
        host.render (h, controls, 2048, out);
        const auto latency = static_cast<std::size_t> (h.loop.latencySamples());
        for (std::size_t s = static_cast<std::size_t> (first) - 2000; s < static_cast<std::size_t> (first); ++s)
            CHECK (out.left[s] >= 0.5f - 1.0e-6f);
        for (std::size_t s = static_cast<std::size_t> (first) + latency; s < out.left.size(); ++s)
            CHECK (out.left[s] >= 0.5f - 1.0e-6f);
        CHECK (h.retired.pop() == &a);
    }

    // With no kit at all, offline adopts on the first sample too.
    Harness h;
    FakeHost host (48000.0, 256);
    host.prepare (h);
    host.setNonRealtime (true);
    host.setTempo (120.0);
    host.play (0.0);
    h.pending.publish (&a);
    FakeHost::Output out;
    host.render (h, Controls {}, 256, out);
    CHECK (h.telemetry.kitIndex.load() == 0);
    CHECK (onOf (h.loop.voices().log, inst::kickB, 0) != nullptr);
}

TEST_CASE ("the voices get the scheduler's tempo once per block: the host's, or the kit's without one", "[engine][phrase]")
{
    const Kit kit = synKit();
    for (const double hostBpm : { 125.0, 0.0 })
    {
        CAPTURE (hostBpm);
        Harness h;
        FakeHost host (48000.0, 256);
        host.prepare (h);
        h.pending.publish (&kit);
        host.setTempo (hostBpm);
        host.play (0.0);
        FakeHost::Output out;
        host.render (h, Controls {}, 256 * 10, out);
        CHECK (h.loop.voices().tempoCalls == 10);
        CHECK (h.loop.voices().tempo == Catch::Approx (hostBpm > 0.0 ? hostBpm : 120.0));
    }
}
