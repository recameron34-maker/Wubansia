// One voice and the rules that start it: FUT01's glides against context4.bass_voice, the modes and
// envelopes, end alignment, chokes, FUT02's and FUT04's glides, restrike-continue, layering and throws.

#include "sampler_fixtures.h"

#include "sampler/selection.h"
#include "sampler/voice.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <cmath>
#include <vector>

using namespace sampler_test;

namespace
{
    // FUT01's warp bass as kit.json describes it, on the Python vectors' sources: zones F#1, A1, C#2
    // (basslines.ZONES), layers round (0.6 x 127) and 127, a sustain loop, Hermite, a 1 ms attack and 30 ms
    // release, mono with a 70 ms raised-cosine slide and a 30 ms dip, pow15.
    struct WarpKit
    {
        TestKit t;
        int warp = -1;

        WarpKit()
        {
            const int a = t.addFile (readVector ("source_a.f32"), {}, 1024, 3584);
            const int b = t.addFile (readVector ("source_b.f32"), {}, 512, 3000);
            auto inst = instrument (SampleMode::sustainLoop, 0, Lane::bass);
            inst.interp = Interp::hermite;
            inst.velocityLaw = VelocityLaw::pow15;
            inst.attackUs = 1000;
            inst.releaseUs = 30000;
            inst.mono = true;
            inst.glide = Glide { GlideCurve::raisedCosine, 70000, 30000 };
            warp = t.addInstrument (inst, {
                ZoneSpec { 30, 0, 32, { LayerSpec { 76, { a } }, LayerSpec { 127, { a } } } },
                ZoneSpec { 33, 33, 36, { LayerSpec { 76, { b } }, LayerSpec { 127, { a } } } },
                ZoneSpec { 37, 37, 127, { LayerSpec { 76, { a } }, LayerSpec { 127, { a } } } },
            });
        }
    };

    struct Step
    {
        std::int64_t at;
        int key, velocity;
        bool dip;
    };

    // Plays a legato run the way the MIDI writes it: each note-on, the note before held 12 ticks
    // (about 270 samples) into it, then the last note's note-off at nOn.
    std::vector<float> playRun (const WarpKit& w, const std::vector<Step>& steps, std::int64_t nOn, int piece)
    {
        Rig rig (48000.0, piece);
        for (std::size_t k = 0; k < steps.size(); ++k)
        {
            const auto& s = steps[k];
            rig.at (s.at, [&w, s] (Sampler& sampler, int offset) {
                auto n = note (w.warp, s.key, s.velocity, offset);
                n.dip = s.dip;
                sampler.noteOn (w.t.kit, n);
            });
            if (k > 0)
            {
                const int before = steps[k - 1].key;
                rig.at (s.at + 270, [&w, before] (Sampler& sampler, int offset) { sampler.noteOff (w.warp, before, offset, Source::sequencer); });
            }
        }
        const int last = steps.back().key;
        rig.at (nOn, [&w, last] (Sampler& sampler, int offset) { sampler.noteOff (w.warp, last, offset, Source::sequencer); });
        rig.runUntil (nOn + 2000);
        CHECK (rig.sampler.activeVoices() == 0);
        CHECK (rig.peakVoices == 1); // legato: one voice throughout
        return rig.left[0];
    }
}

TEST_CASE ("FUT01's dip and slide match context4.bass_voice within 1e-5", "[sampler][voice][vectors]")
{
    const WarpKit w;
    const int piece = GENERATE (32, 1, 7, 16);
    CAPTURE (piece);

    SECTION ("a dip: F#2 down to C#2 and back, 30 ms each way, at one velocity")
    {
        const auto expected = readVector ("glide_dip.f32");
        REQUIRE (expected.size() == 8440);
        const auto got = playRun (w, { { 0, 42, 89, false }, { 3600, 37, 89, true }, { 5200, 42, 89, true } }, 7000, piece);
        REQUIRE (got.size() > expected.size());
        CHECK (maxAbs (got) > 0.5);
        const auto [worst, where] = maxDiff (got, expected, 0, expected.size());
        CAPTURE (worst, where);
        CHECK (worst <= 1.0e-5);
        CHECK (silent (got, expected.size())); // released over exactly 30 ms
    }

    SECTION ("a slide: A1 up to C#2 over 70 ms, its gain gliding from a plain note's to an accent's")
    {
        const auto expected = readVector ("glide_slide.f32");
        REQUIRE (expected.size() == 7440);
        const auto got = playRun (w, { { 0, 33, 89, false }, { 2000, 37, 127, false } }, 6000, piece);
        REQUIRE (got.size() > expected.size());
        CHECK (maxAbs (got) > 0.5);
        const auto [worst, where] = maxDiff (got, expected, 0, expected.size());
        CAPTURE (worst, where);
        CHECK (worst <= 1.0e-5);
        CHECK (silent (got, expected.size()));
    }

    SECTION ("a dip flag on a slide-only instrument's note still glides over us")
    {
        // A dip needs dip_us: an instrument without one slides.
        WarpKit plain;
        plain.t.kit.instruments[static_cast<std::size_t> (plain.warp)].glide.dipUs = 0;
        const auto dip = playRun (plain, { { 0, 42, 89, false }, { 3600, 37, 89, true } }, 7000, piece);
        const auto slide = playRun (plain, { { 0, 42, 89, false }, { 3600, 37, 89, false } }, 7000, piece);
        CHECK (sameBits (dip, slide));
    }
}

TEST_CASE ("the attack and release follow parts.raised and dsp48.taper; one-shots ignore note-off", "[sampler][voice][vectors]")
{
    const auto env = readVector ("envelopes.f32");
    REQUIRE (env.size() == 1900);

    TestKit t;
    const int dc = t.addFile (constant (20000, 0.5f));
    auto gated = instrument (SampleMode::gated);
    gated.attackUs = 1000;
    gated.releaseUs = 30000;
    const int g = t.addSimple (gated, dc);
    auto shot = instrument (SampleMode::oneShot, 1);
    shot.attackUs = 1000;
    shot.releaseUs = 30000;
    const int o = t.addSimple (shot, dc);

    Rig rig;
    rig.at (0, [&] (Sampler& s, int offset) {
        s.noteOn (t.kit, note (g, 60, 127, offset));
        s.noteOn (t.kit, note (o, 60, 127, offset));
    });
    rig.at (3000, [&] (Sampler& s, int offset) {
        s.noteOff (g, 60, offset, Source::sequencer);
        s.noteOff (o, 60, offset, Source::sequencer);
    });
    rig.runUntil (21000);

    double attackWorst = 0.0, releaseWorst = 0.0;
    for (std::size_t j = 0; j < 100; ++j)
        attackWorst = std::max (attackWorst, std::fabs (rig.left[0][j] / 0.5 - env[j]));
    for (std::size_t j = 0; j < 1500; ++j)
        releaseWorst = std::max (releaseWorst, std::fabs (rig.left[0][3000 + j] / 0.5 - env[100 + j]));
    CAPTURE (attackWorst, releaseWorst);
    CHECK (attackWorst <= 1.0e-6);
    CHECK (releaseWorst <= 1.0e-6);
    CHECK (silent (rig.left[0], 3000 + 1440));

    // The one-shot plays its whole file, note-off or not, and ends with it.
    CHECK (std::fabs (rig.left[1][19999] - 0.5) < 1.0e-7);
    CHECK (silent (rig.left[1], 20000));
    for (std::size_t j = 100; j < 20000; j += 997)
        CHECK (std::fabs (rig.left[1][j] - 0.5) < 1.0e-7);
}

TEST_CASE ("a sustain loop wraps seamlessly: no step above 1e-4 against the same tone unlooped", "[sampler][voice]")
{
    const auto interp = GENERATE (Interp::sinc, Interp::hermite);
    const double host = GENERATE (44100.0, 48000.0, 96000.0);
    CAPTURE (interp, host);

    // 1 kHz at 48 kHz is 48 frames a cycle: a loop of 50 cycles joins with no seam, so the looped file and a
    // long file of the same tone are the same signal.
    TestKit t;
    const int looped = t.addFile (sine (4800, 1000.0, 0.7), {}, 2400, 4800);
    const int straight = t.addFile (sine (200000, 1000.0, 0.7));
    auto a = instrument (SampleMode::sustainLoop, 0);
    a.interp = interp;
    auto b = instrument (SampleMode::gated, 1);
    b.interp = interp;
    const int ia = t.addSimple (a, looped);
    const int ib = t.addSimple (b, straight);

    Rig rig (host);
    rig.at (0, [&] (Sampler& s, int offset) {
        s.noteOn (t.kit, note (ia, 63, 127, offset));
        s.noteOn (t.kit, note (ib, 63, 127, offset));
    });
    rig.runUntil (static_cast<std::int64_t> (host * 1.5));
    const auto [worst, where] = maxDiff (rig.left[0], rig.left[1]);
    CAPTURE (worst, where);
    CHECK (worst <= 1.0e-4);
    CHECK (maxAbs (rig.left[0], rig.left[0].size() - 100) > 0.5); // still playing: it loops
}

TEST_CASE ("end alignment puts the file's end on the note's end within one sample", "[sampler][voice]")
{
    const double bpm = GENERATE (100.0, 134.0, 170.0);
    const double host = GENERATE (48000.0, 44100.0);
    const double beats = GENERATE (0.25, 1.0); // shorter and longer than the 200 ms file
    CAPTURE (bpm, host, beats);

    TestKit t;
    const int file = t.addFile (constant (9600, 0.5f));
    auto inst = instrument (SampleMode::oneShot);
    inst.align = Align::end;
    const int i = t.addSimple (inst, file, 60);

    const auto length = static_cast<std::int64_t> (std::llround (beats * 60.0 / bpm * host));
    const std::int64_t on = 1000;
    Rig rig (host);
    rig.at (on, [&] (Sampler& s, int offset) {
        auto n = note (i, 60, 127, offset);
        n.lengthSamples = length;
        s.noteOn (t.kit, n);
    });
    rig.runUntil (on + length + 4000);

    const auto& y = rig.left[0];
    std::int64_t first = -1, last = -1;
    for (std::size_t n = 0; n < y.size(); ++n)
        if (! same (y[n], 0.0f))
        {
            if (first < 0)
                first = static_cast<std::int64_t> (n);
            last = static_cast<std::int64_t> (n);
        }
    const double fileSamples = 9600.0 * host / 48000.0;
    CAPTURE (length, first, last, fileSamples);
    CHECK (std::llabs (last - (on + length - 1)) <= 1);
    if (static_cast<double> (length) > fileSamples)
        CHECK (std::fabs (static_cast<double> (first) - (static_cast<double> (on + length) - fileSamples)) <= 1.0);
    else
        CHECK (first == on);
}

TEST_CASE ("chokes: the open hat fades within fade_us of the closed hat; the ride isn't choked", "[sampler][voice][vectors]")
{
    const bool cos2 = GENERATE (true, false);
    CAPTURE (cos2);
    const auto env = readVector ("envelopes.f32"); // [1600, 1900): FUT02-04's choke gain from the closed hat on

    TestKit t;
    const int open = t.addFile (constant (30000, 0.5f));
    const int closed = t.addFile (constant (500, 0.25f));
    const int ride = t.addFile (constant (30000, 0.3f));
    auto openHat = instrument (SampleMode::oneShot, 0, Lane::hats);
    openHat.choke = Choke { 1, ChokeRole::choked, cos2 ? 4000 : 5000, cos2 ? ChokeCurve::cos2 : ChokeCurve::linear };
    auto closedHat = instrument (SampleMode::oneShot, 1, Lane::hats);
    closedHat.choke = Choke { 1, ChokeRole::chokes, cos2 ? 4000 : 5000, cos2 ? ChokeCurve::cos2 : ChokeCurve::linear };
    auto rideCymbal = instrument (SampleMode::oneShot, 2, Lane::hats); // FUT04's ride: in no group
    const int io = t.addSimple (openHat, open, 46);
    const int ic = t.addSimple (closedHat, closed, 42);
    const int ir = t.addSimple (rideCymbal, ride, 51);

    Rig rig;
    rig.at (0, [&] (Sampler& s, int offset) {
        s.noteOn (t.kit, note (io, 46, 127, offset));
        s.noteOn (t.kit, note (ir, 51, 127, offset));
    });
    rig.at (1000, [&] (Sampler& s, int offset) { s.noteOn (t.kit, note (ic, 42, 127, offset)); });
    rig.runUntil (3000);

    const std::size_t n = cos2 ? 192 : 240;
    double worst = 0.0;
    for (std::size_t j = 0; j < 300; ++j)
    {
        const double want = cos2 ? env[1600 + j] : (j < n ? 1.0 - static_cast<double> (j) / static_cast<double> (n) : 0.0);
        worst = std::max (worst, std::fabs (rig.left[0][1000 + j] / 0.5 - want));
    }
    CAPTURE (worst);
    CHECK (worst <= 1.0e-6);
    CHECK (std::fabs (rig.left[0][999] - 0.5) < 1.0e-7); // untouched before the closed hat
    CHECK (silent (rig.left[0], 1000 + n));
    CHECK (std::fabs (rig.left[1][1100] - 0.25) < 1.0e-7);
    for (std::size_t j = 0; j < 3000; j += 37)
        CHECK (std::fabs (rig.left[2][j] - 0.3) < 1.0e-7);
}

TEST_CASE ("FUT02's exponential glide (tau 18 ms) and FUT04's 50 ms raised cosine are continuous and land on the note", "[sampler][voice]")
{
    SampleView view;
    const auto data = sine (48000, 440.0, 0.5);
    view.left = data.data();
    view.right = view.left;
    view.frames = 48000;
    view.channels = 1;

    struct Case
    {
        GlideCurve curve;
        double samples;
    };
    const auto c = GENERATE (Case { GlideCurve::exponential, 0.018 * 48000.0 }, Case { GlideCurve::raisedCosine, 0.050 * 48000.0 },
        Case { GlideCurve::linear, 0.050 * 48000.0 });
    CAPTURE (c.curve);

    VoiceStart s;
    s.sample = view;
    s.interp = Interp::sinc;
    s.mode = SampleMode::gated;
    s.semitones = 0;
    Voice v;
    v.start (s);
    std::vector<float> l (1), r (1), sp (1), ec (1), lane (1, 1.0f);
    VoiceTarget target { l.data(), r.data(), sp.data(), ec.data(), lane.data() };
    v.render (target, 0, 1);
    v.glide (7, 1.0, c.curve, c.samples);

    std::vector<double> trace;
    for (int k = 0; k < 24000; ++k)
    {
        v.render (target, 0, 1);
        trace.push_back (v.currentSemitones());
    }
    CHECK (same (trace.front(), 0.0)); // the legato sample itself still sounds the old pitch
    double steepest = 0.0;
    for (std::size_t k = 1; k < trace.size(); ++k)
    {
        CHECK (trace[k] >= trace[k - 1]); // monotone
        steepest = std::max (steepest, trace[k] - trace[k - 1]);
    }
    CAPTURE (steepest);
    CHECK (steepest <= 7.0 * kPi / 2.0 / c.samples + 1.0e-9); // no jump: at most the curve's own steepest slope
    for (std::size_t k = 0; k < trace.size(); k += 101)
    {
        const double u = static_cast<double> (k) / c.samples;
        double want = 7.0;
        if (c.curve == GlideCurve::exponential)
            want = std::exp (-u) < 1.0e-9 ? 7.0 : 7.0 * (1.0 - std::exp (-u));
        else if (u < 1.0)
            want = c.curve == GlideCurve::linear ? 7.0 * u : 7.0 * (0.5 - 0.5 * std::cos (kPi * u));
        CAPTURE (k);
        CHECK (std::fabs (trace[k] - want) < 1.0e-9);
    }
    CHECK (same (trace.back(), 7.0)); // it lands on the note
}

TEST_CASE ("a mono legato note through the sampler reaches its pitch and its waveform stays continuous", "[sampler][voice]")
{
    const auto curve = GENERATE (GlideCurve::exponential, GlideCurve::raisedCosine, GlideCurve::none);
    CAPTURE (curve);
    TestKit t;
    const int file = t.addFile (sine (48000, 220.0, 0.5), {}, 24000, 48000);
    auto inst = instrument (SampleMode::sustainLoop, 0, Lane::bass);
    inst.mono = true;
    inst.glide = Glide { curve, curve == GlideCurve::exponential ? 18000 : (curve == GlideCurve::none ? 0 : 50000), 0 };
    inst.releaseUs = 20000;
    const int i = t.addSimple (inst, file, 48);

    Rig rig (44100.0);
    rig.at (0, [&] (Sampler& s, int offset) { s.noteOn (t.kit, note (i, 48, 100, offset)); });
    rig.at (4410, [&] (Sampler& s, int offset) { s.noteOn (t.kit, note (i, 55, 100, offset)); });
    rig.at (4700, [&] (Sampler& s, int offset) { s.noteOff (i, 48, offset, Source::sequencer); }); // ignored: the voice is on 55
    rig.runUntil (44100);
    CHECK (rig.peakVoices == 1);
    CHECK (rig.sampler.activeVoices() == 1);

    const double target = 220.0 * std::exp2 (7.0 / 12.0);
    // The waveform's slope never exceeds the higher note's: no step.
    CHECK (maxStep (rig.left[0], 100) <= 0.5 * 2.0 * kPi * target / 44100.0 * 1.02);
    std::vector<float> tail (rig.left[0].begin() + 30000, rig.left[0].end());
    double first = -1.0, last = -1.0;
    int crossings = 0;
    for (std::size_t n = 1; n < tail.size(); ++n)
        if (tail[n - 1] < 0.0f && tail[n] >= 0.0f)
        {
            const double t0 = static_cast<double> (n - 1) + tail[n - 1] / (tail[n - 1] - tail[n]);
            first = first < 0.0 ? t0 : first;
            last = t0;
            ++crossings;
        }
    const double measured = (crossings - 1) / (last - first) * 44100.0;
    CAPTURE (measured, target);
    CHECK (std::fabs (1200.0 * std::log2 (measured / target)) < 1.0);
}

TEST_CASE ("restrike-continue: a pad struck again on its own end tick continues with no phase jump", "[sampler][voice]")
{
    TestKit t;
    const int file = t.addFile (sine (20000, 220.0, 0.6), sine (20000, 330.0, 0.4), 10000, 20000);
    auto pad = instrument (SampleMode::sustainLoop, 0, Lane::chords);
    pad.attackUs = 5000;
    pad.releaseUs = 50000;
    pad.restrikeContinues = true;
    const int ip = t.addSimple (pad, file, 60);
    auto plain = pad;
    plain.restrikeContinues = false;
    const int iq = t.addSimple (plain, file, 60);

    auto play = [&] (int inst, bool restrike, std::int64_t chase) {
        Rig rig (44100.0);
        rig.at (0, [&] (Sampler& s, int offset) { s.noteOn (t.kit, note (inst, 60, 100, offset)); });
        if (restrike)
        {
            rig.at (4800, [&] (Sampler& s, int offset) {
                s.noteOff (inst, 60, offset, Source::sequencer); // note-offs first, then note-ons
                auto n = note (inst, 60, 100, offset);
                n.startFrame = chase;
                s.noteOn (t.kit, n);
            });
        }
        rig.at (9600, [&] (Sampler& s, int offset) { s.noteOff (inst, 60, offset, Source::sequencer); });
        rig.runUntil (14000);
        return std::make_pair (rig.left[0], rig.peakVoices);
    };

    const auto [held, heldVoices] = play (ip, false, 0);
    const auto [restruck, restruckVoices] = play (ip, true, 0);
    CHECK (sameBits (held, restruck));
    CHECK (restruckVoices == 1);

    // A chased restrike restarts the voice where its note is now; a pad without the flag re-attacks.
    const auto [chased, chasedVoices] = play (ip, true, 4800);
    CHECK (chasedVoices == 2);
    CHECK (! sameBits (held, chased));
    const auto [again, againVoices] = play (iq, true, 0);
    CHECK (againVoices == 2);
    CHECK (! sameBits (held, again));
}

TEST_CASE ("layered_with plays both instruments, and the first's note-off releases both", "[sampler][voice]")
{
    TestKit t;
    const int rubber = t.addFile (constant (40000, 0.25f));
    const int sub = t.addFile (constant (40000, 0.125f));
    auto top = instrument (SampleMode::gated, 0, Lane::bass);
    top.releaseUs = 1000;
    auto under = instrument (SampleMode::gated, 1, Lane::bass);
    under.releaseUs = 1000;
    const int iu = t.addSimple (under, sub, 31);
    top.layeredWith = static_cast<std::int16_t> (iu);
    const int it = t.addSimple (top, rubber, 31);

    Rig rig;
    rig.at (0, [&] (Sampler& s, int offset) { s.noteOn (t.kit, note (it, 31, 100, offset)); });
    rig.at (2000, [&] (Sampler& s, int offset) { s.noteOff (it, 31, offset, Source::sequencer); });
    rig.runUntil (4000);
    CHECK (rig.peakVoices == 2);
    CHECK (rig.sampler.activeVoices() == 0);
    CHECK (std::fabs (rig.left[0][1000] - 0.25) < 1.0e-7);
    CHECK (std::fabs (rig.left[1][1000] - 0.125) < 1.0e-7);
    CHECK (silent (rig.left[0], 2048));
    CHECK (silent (rig.left[1], 2048));
}

TEST_CASE ("throws: a voice started with throwSpace = 1 writes into the Space send at unity", "[sampler][voice]")
{
    TestKit t;
    const int file = t.addFile (constant (40000, 0.5f));
    const int vox = t.addSimple (instrument (SampleMode::oneShot, 3, Lane::vox), file, 60);
    const int dry = t.addSimple (instrument (SampleMode::oneShot, 4, Lane::lead), file, 60);

    Rig rig;
    rig.at (0, [&] (Sampler& s, int offset) {
        auto thrown = note (vox, 60, 127, offset);
        thrown.throwSpace = 1.0f;
        thrown.throwEcho = 0.25f;
        s.noteOn (t.kit, thrown);
        s.noteOn (t.kit, note (dry, 60, 127, offset)); // reaches the sends only through its channel's sends
    });
    rig.runUntil (2000);
    CHECK (std::fabs (rig.space[1000] - 0.5) < 1.0e-7);
    CHECK (std::fabs (rig.echo[1000] - 0.125) < 1.0e-7);
    CHECK (std::fabs (rig.left[3][1000] - 0.5) < 1.0e-7);
    CHECK (std::fabs (rig.left[4][1000] - 0.5) < 1.0e-7);

    // A muted lane stops throwing within 10 ms; its channel keeps playing (the mixer mutes it).
    std::array<float, kLanes> gains {};
    gains.fill (1.0f);
    gains[static_cast<std::size_t> (Lane::vox)] = 0.0f;
    rig.sampler.setLaneGains (gains);
    rig.runUntil (2000 + 480 + 64);
    CHECK (rig.space[2000] < 0.5f);
    CHECK (maxStep (rig.space, 1990, 2000 + 480) <= 0.5 / 480.0 + 1.0e-6); // a ramp, not a step
    CHECK (silent (rig.space, 2000 + 480));
    CHECK (std::fabs (rig.left[3].back() - 0.5) < 1.0e-7);
}
