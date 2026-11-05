// The sampler under load and on the audio thread's terms: 64 voices and click-free stealing, mono files on
// both sides at unity, no allocation or free while it plays, nothing but clean numbers in long tails, the
// same output for any piece size, the fades a transport and a kit change ask for, and the CPU a voice costs.

#include "sampler_fixtures.h"

#include "alloc_counter.h"

#include "sampler/voice.h"
#include "sampler/voice_pool.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <ctime>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <vector>

using namespace sampler_test;

namespace
{
    // A kit of 65 one-shot instruments on one mix channel, each a constant (1 + i / 1000) / 128 that lasts
    // `frames`, with a 2 ms attack: 64 of them sum to about half scale, and the level of the output says which
    // voices are playing.
    struct ManyKit
    {
        TestKit t;
        std::vector<int> inst;

        explicit ManyKit (std::size_t frames = 48000)
        {
            for (int i = 0; i < 65; ++i)
            {
                const int f = t.addFile (constant (frames, static_cast<float> ((1.0 + i / 1000.0) / 128.0)));
                auto in = instrument (SampleMode::oneShot);
                in.attackUs = 2000;
                inst.push_back (t.addSimple (in, f, 60));
            }
        }

        static double level (int i) { return (1.0 + i / 1000.0) / 128.0; }
    };
}

TEST_CASE ("64 voices sound at once; the 65th steals the oldest released voice with a fade, no step above -40 dBFS", "[sampler][rt]")
{
    const ManyKit k;
    Rig rig;
    // 64 notes over the first 64 pieces of 32 samples; notes 10 and 20 get their note-off (one-shots ring on),
    // then the 65th note.
    for (int i = 0; i < 64; ++i)
        rig.at (i * 32, [&k, i] (Sampler& s, int offset) { s.noteOn (k.t.kit, note (k.inst[static_cast<std::size_t> (i)], 60, 127, offset)); });
    rig.at (3000, [&k] (Sampler& s, int offset) {
        s.noteOff (k.inst[20], 60, offset, Source::sequencer);
        s.noteOff (k.inst[10], 60, offset, Source::sequencer);
    });
    rig.runUntil (4000);
    CHECK (rig.sampler.activeVoices() == 64);
    const double before = rig.left[0][3999];
    double sum = 0.0;
    for (int i = 0; i < 64; ++i)
        sum += ManyKit::level (i);
    CHECK (std::fabs (before - sum) < 1.0e-5);

    rig.at (4000, [&k] (Sampler& s, int offset) { s.noteOn (k.t.kit, note (k.inst[64], 60, 127, offset)); });
    rig.runUntil (6000);
    CHECK (rig.peakVoices == 65); // the stolen voice fading in its own slot beside the 64
    CHECK (rig.sampler.activeVoices() == 64);
    // Voice 10 (released, and older than 20) went; voice 64 came.
    const double after = rig.left[0][5999];
    CHECK (std::fabs (after - (sum - ManyKit::level (10) + ManyKit::level (64))) < 1.0e-5);
    // At about half scale with 64 voices summed, a cut voice would step by 1/128 (-42 dBFS) in a sample and a
    // full-scale mix by twice that: the 2 ms fades keep every step far under -40 dBFS.
    const double step = maxStep (rig.left[0], 1);
    CAPTURE (step);
    CHECK (step < 0.01);
    CHECK (step < 1.0e-3);
}

TEST_CASE ("with no voice released, the quietest is stolen", "[sampler][rt]")
{
    // 65 constants of 1 / 128 played pow15: voice 37's softer note makes it the quietest by gain.
    TestKit t;
    std::vector<int> inst;
    for (int i = 0; i < 65; ++i)
    {
        const int f = t.addFile (constant (48000, 1.0f / 128.0f));
        auto in = instrument (SampleMode::oneShot);
        in.attackUs = 2000;
        in.velocityLaw = VelocityLaw::pow15;
        inst.push_back (t.addSimple (in, f, 60));
    }
    Rig rig;
    for (int i = 0; i < 64; ++i)
        rig.at (i, [&t, &inst, i] (Sampler& s, int offset) {
            s.noteOn (t.kit, note (inst[static_cast<std::size_t> (i)], 60, i == 37 ? 64 : 127, offset));
        });
    rig.at (3000, [&t, &inst] (Sampler& s, int offset) { s.noteOn (t.kit, note (inst[64], 60, 127, offset)); });
    rig.runUntil (4000);
    const double soft = std::pow (64.0 / 127.0, 1.5) / 128.0;
    const double before = 63.0 / 128.0 + soft;
    CHECK (std::fabs (rig.left[0][2999] - before) < 1.0e-5);
    CHECK (std::fabs (rig.left[0][3999] - (before - soft + 1.0 / 128.0)) < 1.0e-5);
    CHECK (maxStep (rig.left[0], 2000) < 1.0e-3);
}

TEST_CASE ("a chord's notes steal old voices, never each other", "[sampler][rt]")
{
    // The same for any piece size: which voice is stolen depends on the voices' state, not on the pieces.
    const int piece = GENERATE (32, 8, 7, 1);
    CAPTURE (piece);
    const ManyKit k;
    Rig rig (48000.0, piece);
    for (int i = 0; i < 64; ++i)
        rig.at (i, [&k, i] (Sampler& s, int offset) { s.noteOn (k.t.kit, note (k.inst[static_cast<std::size_t> (i)], 60, 127, offset)); });
    rig.at (3000, [&k] (Sampler& s, int offset) {
        for (int key = 60; key < 64; ++key) // four notes on one sample, all voices held
            s.noteOn (k.t.kit, note (k.inst[64], key, 127, offset));
    });
    rig.runUntil (4000);
    double sum = 0.0;
    for (int i = 4; i < 64; ++i)
        sum += ManyKit::level (i);
    // The four quietest old voices (0-3) went; the four new notes all sound.
    CHECK (std::fabs (rig.left[0][3999] - (sum + 4.0 * ManyKit::level (64))) < 1.0e-5);
    CHECK (rig.sampler.activeVoices() == 64);
    CHECK (maxStep (rig.left[0], 2000) < 1.0e-3);
}

TEST_CASE ("stealing gives the same output for any piece size", "[sampler][rt]")
{
    const ManyKit k (6000);
    auto play = [&k] (int piece) {
        Rig rig (44100.0, piece);
        for (int n = 0; n < 300; ++n)
        {
            const std::int64_t at = (n * 7919) % 20000; // scattered, many on one sample
            const int i = (n * 13) % 65;
            const int key = 55 + n % 9;
            rig.at (at, [&k, i, key, n] (Sampler& s, int offset) { s.noteOn (k.t.kit, note (k.inst[static_cast<std::size_t> (i)], key, 40 + n % 88, offset)); });
            if (n % 3 == 0)
                rig.at (at + 50 + n, [&k, i, key] (Sampler& s, int offset) { s.noteOff (k.inst[static_cast<std::size_t> (i)], key, offset, Source::sequencer); });
        }
        rig.runUntil (26000);
        return rig.left[0];
    };
    const auto reference = play (32);
    CHECK (maxAbs (reference) > 0.3);
    for (const int piece : { 1, 5, 16 })
    {
        CAPTURE (piece);
        CHECK (sameBits (reference, play (piece)));
    }
}

TEST_CASE ("mono files land on both sides at unity; stereo files keep their sides", "[sampler][rt]")
{
    const double host = GENERATE (48000.0, 44100.0, 96000.0);
    CAPTURE (host);
    TestKit t;
    const int mono = t.addFile (constant (8000, 0.5f));
    const int stereo = t.addFile (constant (8000, 0.25f), constant (8000, -0.125f));
    const int im = t.addSimple (instrument (SampleMode::oneShot, 0), mono);
    const int is = t.addSimple (instrument (SampleMode::oneShot, 1), stereo);
    Rig rig (host);
    rig.at (0, [&] (Sampler& s, int offset) {
        s.noteOn (t.kit, note (im, 60, 127, offset));
        s.noteOn (t.kit, note (is, 60, 127, offset));
    });
    rig.runUntil (2000);
    for (std::size_t n = 100; n < 2000; n += 13)
    {
        CHECK (std::fabs (rig.left[0][n] - 0.5) < 1.0e-6);
        CHECK (same (rig.left[0][n], rig.right[0][n]));
        CHECK (std::fabs (rig.left[1][n] - 0.25) < 1.0e-6);
        CHECK (std::fabs (rig.right[1][n] + 0.125) < 1.0e-6);
    }
}

TEST_CASE ("the same notes give the same output for any piece size", "[sampler][rt]")
{
    TestKit t;
    const int pad = t.addFile (sine (30000, 220.0, 0.3), sine (30000, 223.0, 0.3), 15000, 30000);
    const int hit = t.addFile (sine (3000, 3000.0, 0.4));
    auto p = instrument (SampleMode::sustainLoop, 0, Lane::chords);
    p.attackUs = 3000;
    p.releaseUs = 40000;
    auto h = instrument (SampleMode::oneShot, 1, Lane::perc);
    h.choke = Choke { 1, ChokeRole::both, 4000, ChokeCurve::cos2 };
    auto b = instrument (SampleMode::sustainLoop, 2, Lane::bass);
    b.mono = true;
    b.glide = Glide { GlideCurve::exponential, 18000, 0 };
    b.releaseUs = 20000;
    const int ip = t.addSimple (p, pad, 60);
    const int ih = t.addSimple (h, hit, 60);
    const int ib = t.addSimple (b, pad, 48);

    struct Outputs
    {
        std::map<int, std::vector<float>> left, right;
        std::vector<float> echo;
    };
    auto play = [&] (int piece) {
        Rig rig (44100.0, piece);
        for (int k = 0; k < 20; ++k)
            rig.at (k * 997 + 13, [&, k] (Sampler& s, int offset) {
                auto n = note (ih, 58 + k % 5, 60 + k * 3, offset);
                n.throwEcho = 0.5f;
                s.noteOn (t.kit, n);
            });
        rig.at (5, [&] (Sampler& s, int offset) { s.noteOn (t.kit, note (ip, 64, 100, offset)); });
        rig.at (9000, [&] (Sampler& s, int offset) { s.noteOff (ip, 64, offset, Source::sequencer); });
        rig.at (300, [&] (Sampler& s, int offset) { s.noteOn (t.kit, note (ib, 43, 100, offset)); });
        rig.at (4400, [&] (Sampler& s, int offset) { s.noteOn (t.kit, note (ib, 50, 120, offset)); });
        rig.at (4600, [&] (Sampler& s, int offset) { s.noteOff (ib, 43, offset, Source::sequencer); });
        rig.at (12000, [&] (Sampler& s, int offset) { s.noteOff (ib, 50, offset, Source::sequencer); });
        rig.at (15000, [&] (Sampler& s, int offset) { s.fadeOut (Source::sequencer, offset, 441); });
        rig.runUntil (22050);
        return Outputs { rig.left, rig.right, rig.echo };
    };
    const auto reference = play (32);
    for (const int piece : { 1, 3, 16, 32 })
    {
        CAPTURE (piece);
        const auto other = play (piece);
        for (int c = 0; c < 3; ++c)
        {
            CHECK (sameBits (reference.left.at (c), other.left.at (c)));
            CHECK (sameBits (reference.right.at (c), other.right.at (c)));
        }
        CHECK (sameBits (reference.echo, other.echo));
    }
    CHECK (maxAbs (reference.left.at (0)) > 0.1);
    CHECK (maxAbs (reference.echo) > 0.01);
}

TEST_CASE ("fadeOut fades one source's voices; releaseAll releases its held notes; fadeKit and holdsKit hand a kit back", "[sampler][rt]")
{
    TestKit t;
    const int file = t.addFile (constant (96000, 0.5f), {}, 1000, 96000);
    auto held = instrument (SampleMode::sustainLoop, 0);
    held.releaseUs = 10000;
    const int ih = t.addSimple (held, file);
    auto other = instrument (SampleMode::sustainLoop, 1);
    other.releaseUs = 10000;
    const int io = t.addSimple (other, file);

    Rig rig;
    rig.at (0, [&] (Sampler& s, int offset) {
        s.noteOn (t.kit, note (ih, 60, 127, offset, Source::sequencer));
        s.noteOn (t.kit, note (io, 60, 127, offset, Source::live));
    });
    rig.at (1000, [&] (Sampler& s, int offset) { s.fadeOut (Source::sequencer, offset, 480); });
    rig.runUntil (2000);
    CHECK (silent (rig.left[0], 1480));
    CHECK (std::fabs (rig.left[0][1240] - 0.25) < 1.0e-3); // halfway down its cos^2
    CHECK (std::fabs (rig.left[1][1999] - 0.5) < 1.0e-7); // live notes play on
    CHECK (rig.sampler.activeVoices() == 1);

    rig.at (2000, [&] (Sampler& s, int offset) { s.releaseAll (Source::live, offset); });
    rig.runUntil (3000);
    CHECK (silent (rig.left[1], 2480));
    CHECK (rig.sampler.activeVoices() == 0);

    // A kit change: the old kit's voices fade over 5 ms, then nothing holds it.
    TestKit second;
    const int f2 = second.addFile (constant (96000, 0.25f));
    const int i2 = second.addSimple (instrument (SampleMode::oneShot, 2), f2);
    rig.at (3000, [&] (Sampler& s, int offset) {
        s.noteOn (t.kit, note (ih, 60, 127, offset));
        s.noteOn (t.kit, note (io, 60, 127, offset, Source::live));
    });
    rig.runUntil (3200);
    CHECK (rig.sampler.holdsKit (&t.kit));
    CHECK (! rig.sampler.holdsKit (&second.kit));
    rig.at (3200, [&] (Sampler& s, int offset) {
        s.fadeKit (&t.kit, offset);
        s.noteOn (second.kit, note (i2, 60, 127, offset));
    });
    rig.sampler.noteOn (second.kit, note (i2, 60, 127, 0)); // queued, not yet played: it holds the kit too
    CHECK (rig.sampler.holdsKit (&second.kit));
    rig.runUntil (3200 + 239);
    CHECK (rig.sampler.holdsKit (&t.kit)); // still fading
    rig.runUntil (3200 + 240); // 5 ms
    CHECK (! rig.sampler.holdsKit (&t.kit));
    CHECK (rig.sampler.holdsKit (&second.kit));
    CHECK (silent (rig.left[0], 3200 + 240));
    CHECK (silent (rig.left[1], 3200 + 240));
    CHECK (std::fabs (rig.left[2].back() - 0.5) < 1.0e-7); // two notes of the new kit

    rig.sampler.reset();
    CHECK (rig.sampler.activeVoices() == 0);
    CHECK (! rig.sampler.holdsKit (&second.kit));
}

TEST_CASE ("a full queue drops events and counts them; reset empties it", "[sampler][rt]")
{
    TestKit t;
    const int file = t.addFile (constant (480, 0.25f));
    const int i = t.addSimple (instrument (SampleMode::oneShot), file);
    Rig rig;
    for (int n = 0; n < Sampler::kMaxEvents + 10; ++n)
        rig.sampler.noteOff (i, 60, 0, Source::sequencer);
    CHECK (rig.sampler.droppedEvents() == 10);
    rig.sampler.noteOn (t.kit, note (i, 60, 127, 0)); // dropped too: the queue is full
    CHECK (rig.sampler.droppedEvents() == 11);
    CHECK (rig.sampler.holdsKit (&t.kit) == false);
    rig.runUntil (32);
    CHECK (silent (rig.left[0]));

    rig.sampler.noteOn (t.kit, note (i, 60, 127, 0));
    CHECK (rig.sampler.holdsKit (&t.kit));
    rig.sampler.reset();
    CHECK (! rig.sampler.holdsKit (&t.kit));
    rig.runUntil (64);
    CHECK (silent (rig.left[0]));

    Sampler unprepared; // nothing to queue into before prepare (): dropped, and nothing allocates
    unprepared.noteOn (t.kit, note (i, 60, 127, 0));
    CHECK (unprepared.droppedEvents() == 1);
}

TEST_CASE ("phrase notes go to the phrase voices, never to a multisample voice", "[sampler][rt]")
{
    TestKit t;
    const int file = t.addFile (constant (4800, 0.5f));
    const int i = t.addSimple (instrument (SampleMode::oneShot), file);
    Phrase ph;
    ph.stem = { static_cast<std::uint16_t> (file), static_cast<std::uint16_t> (file) };
    t.kit.phrases.push_back (ph);
    t.kit.slices.push_back (PhraseSlice {});

    Rig rig;
    rig.at (0, [&] (Sampler& s, int offset) {
        auto n = note (i, 60, 127, offset);
        n.phrase = 0;
        n.slice = 0;
        s.noteOn (t.kit, n);
        n.phrase = 3; // no such phrase: nothing plays
        s.noteOn (t.kit, n);
    });
    rig.runUntil (256);
    CHECK (silent (rig.left[0])); // P0's PhraseVoice stub is silent; the multisample didn't play the note
}

namespace
{
    // A kit of phrases for the phrase slots' tests: one stem, each phrase two long slices of it (valid for P6's
    // voice too, so these tests hold with it dropped in), on the lanes given.
    struct PhraseKit
    {
        TestKit t;
        int stem = -1;

        PhraseKit() { stem = t.addFile (sine (96000, 220.0, 0.3)); }

        int add (Lane lane)
        {
            Phrase ph;
            ph.lane = lane;
            ph.mixChannel = 5;
            ph.stem = { static_cast<std::uint16_t> (stem), static_cast<std::uint16_t> (stem) };
            ph.firstSlice = static_cast<std::uint16_t> (t.kit.slices.size());
            ph.sliceCount = 2;
            t.kit.slices.push_back (PhraseSlice { 0, 60, 0, 24000, 22000, 0, 1 });
            t.kit.slices.push_back (PhraseSlice { 8, 62, 24000, 48000, 46000, 0, 1 });
            t.kit.phrases.push_back (ph);
            return static_cast<int> (t.kit.phrases.size()) - 1;
        }

        NoteOn note (int phrase, int k, Source source = Source::sequencer, std::int64_t startFrame = 0) const
        {
            NoteOn n;
            n.phrase = static_cast<std::int16_t> (phrase);
            n.slice = static_cast<std::int16_t> (t.kit.phrases[static_cast<std::size_t> (phrase)].firstSlice + k);
            n.key = static_cast<std::uint8_t> (60 + 2 * k);
            n.velocity = 100;
            n.source = source;
            n.startFrame = startFrame;
            n.continues = k == 0;
            return n;
        }
    };

    // A VoicePool's phrase voices rendered on their own, in pieces of 32, at 48 kHz.
    struct PhrasePool
    {
        static constexpr int kSteal = 96; // 2 ms

        PhrasePool()
        {
            pool.prepare (48000.0);
            bus.prepare (kMaxMixChannels, 32);
            sends.prepare (32);
            laneSends.assign (static_cast<std::size_t> (kLanes) * 32, 1.0f);
        }

        void render (int samples)
        {
            for (int done = 0; done < samples; done += 32)
            {
                bus.clear (32);
                sends.clear (32);
                pool.renderPhrases (bus, sends, 32, laneSends.data(), 32);
            }
        }

        const PhraseSlot* slotOf (const Kit& kit, int phrase) const
        {
            for (const auto& p : pool.phrases)
                if (p.busy && p.kit == &kit && p.phrase == phrase)
                    return &p;
            return nullptr;
        }

        int busy() const
        {
            int n = 0;
            for (const auto& p : pool.phrases)
                n += p.busy ? 1 : 0;
            return n;
        }

        VoicePool pool;
        MixBus bus;
        SendBus sends;
        std::vector<float> laneSends;
    };
}

TEST_CASE ("phraseLanes: a bit per lane whose phrase voice plays and isn't stopping", "[sampler][rt][phrase]")
{
    PhraseKit k;
    const int lead = k.add (Lane::lead);
    const int vox = k.add (Lane::vox);
    PhrasePool p;
    CHECK (p.pool.phraseLanes() == 0u);
    p.pool.phraseNoteOn (k.t.kit, k.note (lead, 0, Source::sequencer), 0, PhrasePool::kSteal);
    p.pool.phraseNoteOn (k.t.kit, k.note (vox, 0, Source::live), 0, PhrasePool::kSteal);
    p.render (32);
    CHECK (p.pool.phraseLanes() == (laneBit (Lane::lead) | laneBit (Lane::vox)));
    p.pool.stopPhrases (Source::live, 10, 480); // the vox voice fades: no longer its lane's recording
    CHECK (p.pool.phraseLanes() == laneBit (Lane::lead));
    p.render (32);
    CHECK (p.pool.phraseLanes() == laneBit (Lane::lead));
    p.pool.stopPhrasesOf (&k.t.kit, 10, 480);
    CHECK (p.pool.phraseLanes() == 0u);
    p.pool.reset();
    CHECK (p.pool.phraseLanes() == 0u);
    CHECK (p.busy() == 0);

    Sampler sampler;
    sampler.prepare (48000.0, 64);
    CHECK (sampler.phraseLanes() == 0u);
}

TEST_CASE ("a jump mid-phrase: the chased note starts in a fresh voice while the old one fades", "[sampler][rt][phrase]")
{
    PhraseKit k;
    const int lead = k.add (Lane::lead);
    PhrasePool p;
    p.pool.phraseNoteOn (k.t.kit, k.note (lead, 0), 0, PhrasePool::kSteal);
    p.render (32);
    const PhraseSlot* first = p.slotOf (k.t.kit, lead);
    REQUIRE (first != nullptr);
    CHECK (! first->stopping);
    CHECK (first->voice.kit() == &k.t.kit);
    CHECK (first->voice.phrase() == lead);

    p.pool.phraseNoteOn (k.t.kit, k.note (lead, 1), 0, PhrasePool::kSteal); // the next note reads on in the same voice
    p.render (32);
    CHECK (p.busy() == 1);

    // The jump: the transport stops the sequenced voices over 10 ms, then the note sounding at the new
    // position comes chased.
    p.pool.stopPhrases (Source::sequencer, 5, 480);
    p.pool.phraseNoteOn (k.t.kit, k.note (lead, 1, Source::sequencer, 3000), 5, PhrasePool::kSteal);
    CHECK (p.busy() == 2);
    CHECK (first->stopping);
    const PhraseSlot* chased = nullptr;
    for (const auto& s : p.pool.phrases)
        if (s.busy && ! s.stopping)
            chased = &s;
    REQUIRE (chased != nullptr);
    CHECK (chased != first);
    CHECK (chased->phrase == lead);
    CHECK (p.pool.phraseLanes() == laneBit (Lane::lead));

    p.render (480);
    CHECK (first->busy); // still fading
    p.render (64);
    CHECK (! first->busy); // its 10 ms are over
    CHECK (! first->stopping);
    CHECK (chased->busy);
    CHECK (p.pool.phraseSteals == 0);
}

TEST_CASE ("a stop then a start within the fade never routes into the fading voice", "[sampler][rt][phrase]")
{
    PhraseKit k;
    const int lead = k.add (Lane::lead);
    PhrasePool p;
    p.pool.phraseNoteOn (k.t.kit, k.note (lead, 0), 0, PhrasePool::kSteal);
    p.render (64);
    const PhraseSlot* old = p.slotOf (k.t.kit, lead);
    REQUIRE (old != nullptr);
    p.pool.stopPhrases (Source::sequencer, 0, 480);
    p.render (96); // 2 ms into the 10 ms fade
    CHECK (old->busy);
    CHECK (old->stopping);
    CHECK (p.pool.phraseLanes() == 0u);

    p.pool.phraseNoteOn (k.t.kit, k.note (lead, 0), 0, PhrasePool::kSteal); // play again: a fresh voice
    CHECK (p.busy() == 2);
    CHECK (old->stopping);
    CHECK (p.pool.phraseLanes() == laneBit (Lane::lead));
    p.render (480);
    CHECK (! old->busy);
    CHECK (p.busy() == 1);
    CHECK (p.slotOf (k.t.kit, lead) != old);
}

TEST_CASE ("noteOn, noteOff, the fades and render neither allocate nor free", "[sampler][rt][alloc]")
{
    if (! allocations::available())
        SKIP ("a sanitizer owns the allocation functions in this build");

    const ManyKit k (4000);
    TestKit t;
    const int pad = t.addFile (sine (20000, 220.0, 0.3), sine (20000, 221.0, 0.3), 10000, 20000);
    auto p = instrument (SampleMode::sustainLoop, 1, Lane::bass);
    p.mono = true;
    p.glide = Glide { GlideCurve::raisedCosine, 50000, 20000 };
    p.releaseUs = 30000;
    p.interp = Interp::hermite;
    p.restrikeContinues = true;
    const int ip = t.addSimple (p, pad, 48);
    Phrase ph;
    t.kit.phrases.push_back (ph);

    Sampler sampler;
    sampler.prepare (44100.0, 256);
    MixBus bus;
    bus.prepare (kMaxMixChannels, 256);
    SendBus sends;
    sends.prepare (256);
    std::array<float, kLanes> lanes {};
    lanes.fill (1.0f);

    allocations::Scope scope;
    for (int block = 0; block < 400; ++block)
    {
        bus.clear (256);
        sends.clear (256);
        for (int j = 0; j < 8; ++j) // 3200 notes: stealing all the way
        {
            auto n = note (k.inst[static_cast<std::size_t> ((block * 8 + j) % 65)], 60, 100, j * 30);
            n.throwSpace = 0.3f;
            sampler.noteOn (k.t.kit, n);
            sampler.noteOff (n.instrument, 60, j * 30 + 5, Source::sequencer);
        }
        if (block % 10 == 0)
            sampler.noteOn (t.kit, note (ip, 40 + block % 12, 90, 7));
        if (block % 10 == 5)
            sampler.noteOff (ip, 40 + (block - 5) % 12, 100, Source::sequencer);
        if (block % 50 == 7)
            sampler.fadeOut (Source::sequencer, 30, 441);
        if (block % 70 == 9)
            sampler.fadeKit (&k.t.kit, 0);
        if (block % 30 == 2)
            sampler.releaseAll (Source::live, 3);
        auto phraseNote = note (ip, 50, 100, 11);
        phraseNote.phrase = 0;
        phraseNote.slice = 0;
        sampler.noteOn (t.kit, phraseNote);
        lanes[0] = block % 2 ? 1.0f : 0.0f;
        sampler.setLaneGains (lanes);
        sampler.render (bus, sends, 256);
        (void) sampler.holdsKit (&t.kit);
        (void) sampler.activeVoices();
    }
    const auto counts = scope.counts();
    CHECK (counts.allocations == 0);
    CHECK (counts.frees == 0);
    CHECK (sampler.droppedEvents() == 0);
}

TEST_CASE ("no NaN, infinity or subnormal after 10 s of tails", "[sampler][rt]")
{
    TestKit t;
    const int loud = t.addFile (sine (48000, 97.0, 0.9), sine (48000, 131.0, 0.9), 24000, 48000);
    const int quiet = t.addFile (sine (48000, 4000.0, 1.0e-30f)); // tiny but normal floats
    auto p = instrument (SampleMode::sustainLoop, 0);
    p.releaseUs = 4000000; // 4 s releases
    p.attackUs = 10000;
    auto q = instrument (SampleMode::oneShot, 1);
    q.gainMdb = -60000;
    const int ip = t.addSimple (p, loud, 60);
    const int iq = t.addSimple (q, quiet, 60);

    Rig rig (44100.0, 32, 512);
    for (int k = 0; k < 40; ++k)
    {
        rig.at (k * 101, [&, k] (Sampler& s, int offset) {
            s.noteOn (t.kit, note (ip, 40 + k, 30 + 2 * k, offset));
            s.noteOn (t.kit, note (iq, 60 + k % 12, 100, offset));
        });
        rig.at (k * 101 + 50, [&, k] (Sampler& s, int offset) { s.noteOff (ip, 40 + k, offset, Source::sequencer); });
    }
    rig.runUntil (static_cast<std::int64_t> (44100.0 * 10.5));
    bool clean = true;
    std::size_t bad = 0;
    for (const auto* v : { &rig.left[0], &rig.right[0], &rig.left[1], &rig.right[1], &rig.space, &rig.echo })
        for (std::size_t n = 0; n < v->size(); ++n)
        {
            const float x = (*v)[n];
            if (! std::isfinite (x) || std::fpclassify (x) == FP_SUBNORMAL)
            {
                clean = false;
                bad = n;
            }
        }
    CAPTURE (bad);
    CHECK (clean);
    CHECK (rig.sampler.activeVoices() == 0); // every tail ended
}

TEST_CASE ("the CPU a voice costs", "[.cpu]")
{
    // Hidden ([.cpu]): run it by hand, in a release build. 64 voices held on one 2-second stereo file at a
    // 44.1 kHz host, through the sinc or Hermite: on 64 keys (64 ratios and banks, reads spread over the
    // file: the caches' worst case) or 2.
    const auto interp = GENERATE (Interp::sinc, Interp::hermite);
    const int keys = GENERATE (64, 2);
    TestKit t;
    const int file = t.addFile (sine (96000, 110.0, 0.2), sine (96000, 111.0, 0.2), 48000, 96000);
    auto in = instrument (SampleMode::sustainLoop);
    in.interp = interp;
    in.releaseUs = 20000;
    const int i = t.addSimple (in, file, 60);

    Sampler sampler;
    sampler.prepare (44100.0, 512);
    MixBus bus;
    bus.prepare (kMaxMixChannels, 512);
    SendBus sends;
    sends.prepare (512);
    for (int v = 0; v < 64; ++v)
        sampler.noteOn (t.kit, note (i, 36 + v % keys, 100, v % 32));
    const int blocks = 44100 * 10 / 512;
    const std::clock_t start = std::clock(); // CPU time, so a busy machine slows it less than wall time
    for (int b = 0; b < blocks; ++b)
    {
        bus.clear (512);
        sampler.render (bus, sends, 512);
    }
    const double seconds = static_cast<double> (std::clock() - start) / CLOCKS_PER_SEC;
    const double nsPerVoiceSample = seconds * 1.0e9 / (64.0 * blocks * 512.0);
    const double percentPerVoice = 100.0 * seconds / (blocks * 512.0 / 44100.0) / 64.0;
    WARN ((interp == Interp::sinc ? "sinc" : "hermite") << ", " << keys << " keys: " << nsPerVoiceSample
                                                         << " ns per voice-sample, " << percentPerVoice
                                                         << " % of one core per voice at 44.1 kHz");
    CHECK (sampler.activeVoices() == 64);
}
