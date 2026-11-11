// P8's mixer (ADR 0021): routing, lane levels and mutes, the `when` crossfade, the cc_filter's CC74 law, the
// macros, the meters, the mono fold, and the real-time rules (no allocation, no NaN or subnormals, the same output
// for any block size).

#include "alloc_counter.h"
#include "mix_harness.h"

#include "fx/limiter.h"
#include "fx/oversampled.h"
#include "fx/space.h"
#include "kit/manifest.h"
#include "mix/channel.h"
#include "mix/club_master.h"
#include "mix/graph.h"
#include "mix/inserts.h"
#include "mix/mixer.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <iterator>
#include <random>
#include <sstream>
#include <string>
#include <vector>

using namespace wubansia::engine;
using namespace mixtest;

namespace
{
    Controls clubOff()
    {
        Controls c;
        c.club = false;
        return c;
    }

    bool same (float a, float b) { return std::equal_to<float> {}(a, b); }
}

TEST_CASE ("mixer: without a config every bus plays at unity, as late as a mix, and mono folds to (L + R) / 2", "[mixer]")
{
    Mixer mixer;
    mixer.prepare (48000.0, 256, 2);
    mixer.setConfig (nullptr);
    const int latency = mixer.latencySamples();
    REQUIRE (latency == mix::kMixLatency + mix::ClubMaster::latencyFor (48000.0));
    CHECK (latency == 424);

    auto input = [] (int c, std::int64_t t, int side) -> float {
        if (t != 100)
            return 0.0f;
        return c == 0 ? (side == 0 ? 1.0f : -1.0f) : c == 5 ? 0.5f : 0.0f;
    };
    const auto stereo = run (mixer, 1024, 256, 2, input, 8);
    CHECK (same (stereo[2 * static_cast<std::size_t> (100 + latency)], 1.5f));
    CHECK (same (stereo[2 * static_cast<std::size_t> (100 + latency) + 1], -0.5f));
    float other = 0.0f;
    for (std::size_t i = 0; i < stereo.size(); ++i)
        if (i / 2 != static_cast<std::size_t> (100 + latency))
            other = std::max (other, std::abs (stereo[i]));
    CHECK (same (other, 0.0f));

    Mixer mono;
    mono.prepare (48000.0, 256, 1);
    mono.setConfig (nullptr);
    const auto folded = run (mono, 1024, 256, 1, input, 8);
    CHECK (same (folded[static_cast<std::size_t> (100 + latency)], 0.5f));
}

TEST_CASE ("mixer: plain channels sum at their lanes' levels; the club master off keeps the latency", "[mixer]")
{
    const auto cfg = makeMix ({ channel ("kick", Lane::kick), channel ("bass", Lane::bass, {}, Group::music) });
    Mixer mixer;
    mixer.prepare (48000.0, 128, 2);
    mixer.setConfig (&cfg);
    const int latency = mixer.latencySamples();
    auto controls = [] (std::int64_t) {
        Controls c = clubOff();
        c.laneGain[static_cast<std::size_t> (Lane::kick)] = 0.5f;
        c.laneGain[static_cast<std::size_t> (Lane::bass)] = 0.25f;
        return c;
    };
    // a fresh config starts at its targets: no ramp
    mixer.reset();
    auto input = [] (int c, std::int64_t t, int side) -> float {
        const float v = static_cast<float> ((t % 17) - 8) / 16.0f;
        return c == 0 ? v : (side == 0 ? 0.25f : -0.5f) * v;
    };
    const auto out = run (mixer, 4096, 128, 2, input, 2, {}, controls);
    // the lane levels ramp from their defaults (1) to the new targets over 10 ms; after that the sum is exact
    for (int t = 1000; t < 4096 - latency; ++t)
    {
        const float v = static_cast<float> ((t % 17) - 8) / 16.0f;
        const auto i = static_cast<std::size_t> (t + latency);
        CAPTURE (t);
        REQUIRE (std::abs (out[2 * i] - (v * 0.5f + 0.25f * v * 0.25f)) <= 1e-7f);
        REQUIRE (std::abs (out[2 * i + 1] - (v * 0.5f - 0.5f * v * 0.25f)) <= 1e-7f);
    }
}

TEST_CASE ("mixer: a lane's level and mute move over 10 ms", "[mixer]")
{
    const double rate = GENERATE (44100.0, 48000.0, 96000.0);
    CAPTURE (rate);
    const auto cfg = makeMix ({ channel ("pad", Lane::chords) });
    Mixer mixer;
    mixer.prepare (rate, 64, 2);
    mixer.setConfig (&cfg);
    const int latency = mixer.latencySamples();
    const int ramp = static_cast<int> (std::nearbyint (rate * 0.010));
    constexpr int at = 4096; // a block start (64 | 4096)
    auto controls = [] (std::int64_t start) {
        Controls c = clubOff();
        if (start >= at)
            c.laneGain[static_cast<std::size_t> (Lane::chords)] = 0.0f; // muted
        return c;
    };
    const auto out = run (mixer, at + 2 * ramp + latency + 64, 64, 2, [] (int, std::int64_t, int) { return 0.5f; }, 1, {}, controls);
    auto level = [&] (int t) { return out[2 * static_cast<std::size_t> (t + latency)]; };
    CHECK (std::abs (level (at - 1) - 0.5f) < 1e-7f);
    CHECK (std::abs (level (at + ramp / 2 - 1) - 0.25f) < 1e-3f); // halfway down the line
    CHECK (same (level (at + ramp), 0.0f));
    float step = 0.0f;
    for (int t = at - 10; t < at + ramp + 10; ++t)
        step = std::max (step, std::abs (level (t + 1) - level (t)));
    CHECK (step <= 0.5f / static_cast<float> (ramp) + 1e-6f);
}

TEST_CASE ("mixer: a when-conditioned insert crossfades over 10 ms with no step above -60 dBFS", "[mixer]")
{
    // kick B gets -12 dB and a clip (latency 27, its bypass delayed to match); switching to A crossfades both
    When onB;
    onB.ab = static_cast<std::int8_t> (AbSwitch::kick);
    onB.is = 1;
    const auto cfg = makeMix ({ channel ("kick", Lane::kick, { gain (-12.0, onB), clip (0.0, 1.5, onB) }) });
    Mixer mixer;
    mixer.prepare (48000.0, 32, 2);
    mixer.setConfig (&cfg);
    const int latency = mixer.latencySamples();
    constexpr int at = 9600;
    auto controls = [] (std::int64_t start) {
        Controls c = clubOff();
        c.ab[static_cast<std::size_t> (AbSwitch::kick)] = start >= at ? 0 : 1;
        return c;
    };
    const int total = at + 2000 + latency;
    const auto out = run (mixer, total, 32, 2, [] (int, std::int64_t, int) { return 0.5f; }, 1, {}, controls);
    auto y = [&] (int t) { return out[2 * static_cast<std::size_t> (t + latency)]; };
    const float quiet = static_cast<float> (0.5 * std::pow (10.0, -12.0 / 20.0));
    CHECK (std::abs (y (at - 100) - quiet) < 1e-6f);
    CHECK (std::abs (y (at + 480 + 27 + 10) - 0.5f) < 1e-6f); // 10 ms after, plus the clip's 27
    float step = 0.0f;
    for (int t = 1000; t < total - latency - 1; ++t)
        step = std::max (step, std::abs (y (t + 1) - y (t)));
    INFO ("largest step " << 20.0 * std::log10 (step) << " dBFS");
    CHECK (step <= 0.001f);
}

TEST_CASE ("mixer: cc_filter follows CC74 through the manifest's map, interpolated between points", "[mixer]")
{
    // FUT01's bass map: 650-2210 Hz, Q 1.6; CC74 steps to 0, then ramps to 127 over 4800 samples
    mix::CcFilterInsert f;
    CcFilterParams p;
    p.cc74LoHz = 650;
    p.cc74HiHz = 2210;
    p.cc74Default = 127;
    p.cc71LoPm = 1600;
    p.cc71HiPm = 1600;
    f.setup (p, 48000.0);
    f.setDelay (5); // the insert's input latency: controllers wait as long
    std::vector<mix::CcEvent> ring (mix::kCcEvents);
    ring[0] = { 0, 74, 0, 0 };
    ring[1] = { 1000, 74, 127, 4800 };
    mix::BlockContext ctx;
    ctx.cc = ring.data();
    ctx.ccWritten = 2;
    std::vector<double> v;
    for (std::int64_t t = 0; t < 7000; ++t)
    {
        ctx.start = t;
        float l = 0.0f, r = 0.0f;
        f.process (&l, &r, 1, ctx);
        v.push_back (f.cutoffValue());
    }
    CHECK (v[4] > 126.0); // the default until the first controller arrives, 5 samples late
    CHECK (v[5] < 1e-12);
    CHECK (v[1005] < 1e-12); // a ramp holds its value at its own sample...
    CHECK (std::abs (v[1005 + 2400] - 63.5) < 1e-9); // ...moves in a straight line...
    CHECK (std::abs (v[1005 + 4800] - 127.0) < 1e-12); // ...and lands on its target `ramp` samples on
    CHECK (std::abs (v[6999] - 127.0) < 1e-12);

    // and the map: at CC74 = 127 the cutoff is the top, 2210 Hz; at 0 the bottom, 650 Hz; geometric in between
    const auto at = [] (double value) { return 650.0 * std::pow (2210.0 / 650.0, value / 127.0); };
    CHECK (std::abs (at (127.0) - 2210.0) < 1e-9);
    CHECK (std::abs (at (63.5) - std::sqrt (650.0 * 2210.0)) < 1e-9);
}

TEST_CASE ("mixer: the cc_filter's cutoff shapes a tone the way its CC74 says", "[mixer]")
{
    const auto cfg = makeMix ({ channel ("bass", Lane::bass, { ccFilter (650, 2210, 707, 127) }) });
    Mixer mixer;
    mixer.prepare (48000.0, 256, 2);
    mixer.setConfig (&cfg);
    const int latency = mixer.latencySamples();
    auto tone = [] (int, std::int64_t t, int) { return static_cast<float> (0.25 * std::sin (2.0 * 3.141592653589793 * 3000.0 * static_cast<double> (t) / 48000.0)); };
    const std::vector<Event> events { { 24000, Event::Kind::cc, 0, 74, 0, 0 } };
    const auto out = run (mixer, 48000, 256, 2, tone, 1, events, [] (std::int64_t) { return clubOff(); });
    auto rms = [&] (int from, int to) {
        double s = 0.0;
        for (int t = from; t < to; ++t)
            s += static_cast<double> (out[2 * static_cast<std::size_t> (t + latency)]) * out[2 * static_cast<std::size_t> (t + latency)];
        return std::sqrt (s / (to - from)) / (0.25 / std::sqrt (2.0));
    };
    // the TPT low-pass's response: the analog 1 / (s^2 + s / Q + 1) at the bilinear transform's warped frequency
    const auto expected = [] (double fc) {
        const double w = std::tan (3.141592653589793 * 3000.0 / 48000.0) / std::tan (3.141592653589793 * fc / 48000.0);
        const double k = 1.0 / 0.707;
        return -10.0 * std::log10 ((1.0 - w * w) * (1.0 - w * w) + k * k * w * w);
    };
    const auto db = [] (double g) { return 20.0 * std::log10 (g); };
    CHECK (std::abs (db (rms (12000, 23000)) - expected (2210.0)) < 0.05); // CC74 127: the top of the map
    CHECK (std::abs (db (rms (36000, 47000)) - expected (650.0)) < 0.05); // CC74 0: the bottom
}

TEST_CASE ("mixer: staticGain is the product of a channel's active gain inserts", "[mixer]")
{
    When novox;
    novox.variant = 1;
    const auto ch = channel ("acc", Lane::accents, { gain (-6.0), clip (-1.0), gain (2.0, novox), parallel (0.0, { gain (12.0) }) });
    CHECK (std::abs (mix::staticGain (ch, Variant::vox, { 1, 1, 0 }) - std::pow (10.0, -6.0 / 20.0)) < 1e-6);
    CHECK (std::abs (mix::staticGain (ch, Variant::novox, { 1, 1, 0 }) - std::pow (10.0, -4.0 / 20.0)) < 1e-6);
}

namespace
{
    // Noise for the first `loud` samples on every channel, then silence; keys on every beat at 134 BPM.
    std::function<float (int, std::int64_t, int)> noiseThenSilence (std::int64_t loud)
    {
        return [loud] (int c, std::int64_t t, int side) -> float {
            if (t >= loud)
                return 0.0f;
            std::uint64_t z = static_cast<std::uint64_t> (t * 4 + c * 2 + side) * 0x9E3779B97F4A7C15ull + 0x632BE59BD9B4E019ull;
            z ^= z >> 31;
            z *= 0xBF58476D1CE4E5B9ull;
            z ^= z >> 29;
            return static_cast<float> (static_cast<double> (z >> 11) / 9007199254740992.0 - 0.5) * 0.8f;
        };
    }

    std::vector<Event> beats (std::int64_t until, double rate)
    {
        std::vector<Event> e;
        const auto beat = static_cast<std::int64_t> (rate * 60.0 / 134.0);
        for (std::int64_t t = 0; t < until; t += beat)
        {
            e.push_back ({ t, Event::Kind::key, static_cast<int> (Lane::kick), 110, 0, 0 });
            e.push_back ({ t + beat / 4, Event::Kind::cc, 2, 74, static_cast<int> ((t / beat * 37) % 128), static_cast<int> (beat / 4) });
        }
        return e;
    }

    bool clean (const std::vector<float>& x)
    {
        for (const float v : x)
            if (! std::isfinite (v) || std::fpclassify (v) == FP_SUBNORMAL)
                return false;
        return true;
    }
}

TEST_CASE ("mixer: no NaN, Inf or subnormals after 30 s of silence following noise", "[mixer][slow]")
{
    const auto cfg = busyMix();
    Mixer mixer;
    mixer.prepare (48000.0, 512, 2);
    mixer.setConfig (&cfg);
    const int total = 48000 * 32;
    const auto out = run (mixer, total, 512, 2, noiseThenSilence (2 * 48000), 5, beats (2 * 48000, 48000.0));
    CHECK (clean (out));
    float tail = 0.0f;
    for (std::size_t i = out.size() - 4800; i < out.size(); ++i)
        tail = std::max (tail, std::abs (out[i]));
    CHECK (tail < 1e-6f); // the tails have died away
    float loud = 0.0f;
    for (std::size_t i = 0; i < 2 * 48000 * 2; ++i)
        loud = std::max (loud, std::abs (out[i]));
    CHECK (loud > 0.1f);
}

TEST_CASE ("mixer: random macros every 32 samples stay finite", "[mixer]")
{
    const auto cfg = busyMix();
    Mixer mixer;
    mixer.prepare (48000.0, 32, 2);
    mixer.setConfig (&cfg);
    std::mt19937 rng (3);
    std::uniform_real_distribution<float> macro (0.0f, 2.0f);
    std::uniform_int_distribution<int> bit (0, 1);
    auto controls = [&] (std::int64_t) {
        Controls c;
        c.space = macro (rng);
        c.echo = macro (rng);
        c.pump = macro (rng);
        c.club = bit (rng) != 0;
        c.variant = bit (rng) != 0 ? Variant::novox : Variant::vox;
        c.ab[0] = static_cast<std::uint8_t> (bit (rng));
        for (auto& g : c.laneGain)
            g = macro (rng);
        return c;
    };
    const auto out = run (mixer, 48000 * 3, 32, 2, noiseThenSilence (48000 * 2), 5, beats (48000 * 3, 48000.0), controls);
    CHECK (clean (out));
}

TEST_CASE ("mixer: the output is the same for any host block size", "[mixer]")
{
    const auto cfg = busyMix();
    const int total = 48000;
    auto render = [&] (int block) {
        Mixer mixer;
        mixer.prepare (48000.0, 512, 2);
        mixer.setConfig (&cfg);
        auto controls = [] (std::int64_t start) {
            Controls c;
            c.variant = start >= 24000 ? Variant::novox : Variant::vox; // switched where every block size has a boundary
            return c;
        };
        return run (mixer, total, block, 2, noiseThenSilence (30000), 5, beats (total, 48000.0), controls);
    };
    const auto reference = render (64);
    for (const int block : { 1, 7, 32, 333, 480 })
    {
        CAPTURE (block);
        if (block == 7 || block == 333) // 24000 isn't a boundary of these: compare up to it
        {
            const auto out = render (block);
            CHECK (std::equal (out.begin(), out.begin() + 2 * 24000 - 2 * 512, reference.begin()));
        }
        else
            CHECK (render (block) == reference);
    }
}

TEST_CASE ("mixer: a mono output is (L + R) / 2 of the stereo one", "[mixer]")
{
    const auto cfg = busyMix();
    auto render = [&] (int outputs) {
        Mixer mixer;
        mixer.prepare (48000.0, 256, outputs);
        mixer.setConfig (&cfg);
        return run (mixer, 24000, 256, outputs, noiseThenSilence (12000), 5, beats (24000, 48000.0));
    };
    const auto stereo = render (2);
    const auto mono = render (1);
    REQUIRE (mono.size() * 2 == stereo.size());
    bool ok = true;
    for (std::size_t i = 0; i < mono.size(); ++i)
        ok = ok && same (mono[i], 0.5f * (stereo[2 * i] + stereo[2 * i + 1]));
    CHECK (ok);
}

TEST_CASE ("mixer: neither allocates nor frees while processing or adopting a mix", "[mixer][alloc]")
{
    if (! allocations::available())
        SKIP ("a sanitizer owns the allocation functions in this build");
    const auto busy = busyMix();
    const auto plain = makeMix ({ channel ("kick", Lane::kick, { clip (-1.0) }), channel ("pad", Lane::chords, { onsetDuck (1, 6.0, 0.0, 0.01, 0.02) }) });
    Mixer mixer;
    mixer.prepare (48000.0, 256, 2);
    mixer.setConfig (&plain);
    std::vector<float> l (256), r (256);
    float* out[2] { l.data(), r.data() };

    allocations::Scope scope;
    Controls c;
    for (int block = 0; block < 40; ++block)
    {
        if (block == 10)
            mixer.setConfig (&busy);
        if (block == 30)
            mixer.setConfig (nullptr);
        if (block == 35)
            mixer.setConfig (&busy);
        mixer.keyHit (0, 120, 17);
        mixer.cc (2, 74, block * 3 % 128, 40, 100);
        for (int ch = 0; ch < 5; ++ch)
            std::fill_n (mixer.bus().left (ch), 256, 0.1f * static_cast<float> (block % 3));
        c.variant = block % 7 == 0 ? Variant::novox : Variant::vox;
        mixer.process (out, 2, 256, c, 134.0);
        mixer.bus().clear (256);
    }
    mixer.reset();
    const auto counts = scope.counts();
    CHECK (counts.allocations == 0);
    CHECK (counts.frees == 0);
}

TEST_CASE ("mixer: the meters show each lane's post-fader peak and the club master's reduction", "[mixer]")
{
    const auto cfg = makeMix ({ channel ("kick", Lane::kick), channel ("bass", Lane::bass) });
    Mixer mixer;
    mixer.prepare (48000.0, 512, 2);
    mixer.setConfig (&cfg);
    Controls c;
    c.laneGain[static_cast<std::size_t> (Lane::bass)] = 0.5f;
    std::vector<float> l (512), r (512);
    float* out[2] { l.data(), r.data() };
    for (int block = 0; block < 40; ++block)
    {
        for (int i = 0; i < 512; ++i)
        {
            const auto v = static_cast<float> (std::sin (0.05 * (block * 512 + i)));
            mixer.bus().left (0)[i] = mixer.bus().right (0)[i] = 3.0f * v; // hot: the master works
            mixer.bus().left (1)[i] = mixer.bus().right (1)[i] = 0.8f * v;
        }
        mixer.process (out, 2, 512, c, 134.0);
    }
    const auto& m = mixer.meters();
    CHECK (std::abs (m.lanePeak[static_cast<std::size_t> (Lane::kick)] - 3.0f) < 0.01f);
    CHECK (std::abs (m.lanePeak[static_cast<std::size_t> (Lane::bass)] - 0.4f) < 0.01f);
    CHECK (m.lanePeak[static_cast<std::size_t> (Lane::vox)] < 1e-9f);
    CHECK (m.gainReduction > 2.0f);
}

// ------------------------------------------------------------------ the whole chain against engine_ref

namespace
{
    std::vector<float> readF32 (const std::string& path)
    {
        std::ifstream in (path, std::ios::binary);
        std::vector<char> bytes { std::istreambuf_iterator<char> (in), std::istreambuf_iterator<char>() };
        std::vector<float> out (bytes.size() / sizeof (float));
        std::memcpy (out.data(), bytes.data(), out.size() * sizeof (float));
        return out;
    }

    std::string slurp (const std::string& path)
    {
        std::ifstream in (path, std::ios::binary);
        return { std::istreambuf_iterator<char> (in), std::istreambuf_iterator<char>() };
    }
}

// Hidden: run with WUBANSIA_WHOLECHAIN_DIR naming a folder of engine_ref's render of one kit's loop (P3's
// tools/sound/engine_ref.render_mix, master 'lookback', from rest): meta.txt ("loop passes bpm variant channels"
// and the kit's two manifest paths), dry_<c>.f32 (each channel's dry loop, stereo), keys.txt ("lane sample
// velocity"), cc.txt ("channel controller value sample ramp"), throw_space.f32 and throw_echo.f32 (the thrown
// voices' direct sends, mono, the whole stream) and ref.f32 (the aligned output, stereo). The mixer plays the same
// stream at 48 kHz; its output, latencySamples () later, is compared with the reference's last pass.
TEST_CASE ("mixer: the whole chain against engine_ref's render of a kit's loop", "[.][wholechain]")
{
    const char* dirEnv = std::getenv ("WUBANSIA_WHOLECHAIN_DIR");
    if (dirEnv == nullptr)
        return;
    const std::string dir = std::string (dirEnv) + "/";
    std::istringstream meta (slurp (dir + "meta.txt"));
    int loop = 0, passes = 0, channels = 0;
    double bpm = 0.0;
    std::string variant, kitPath, mixPath;
    meta >> loop >> passes >> bpm >> variant >> channels >> kitPath >> mixPath;
    REQUIRE (loop > 0);
    const auto km = parseKitManifest (slurp (kitPath));
    INFO (km.error);
    REQUIRE (km.ok());
    const auto mc = parseMixManifest (slurp (mixPath), km.value);
    INFO (mc.error);
    REQUIRE (mc.ok());
    REQUIRE (static_cast<int> (mc.value.channels.size()) == channels);

    std::vector<std::vector<float>> dry;
    for (int c = 0; c < channels; ++c)
        dry.push_back (readF32 (dir + "dry_" + std::to_string (c) + ".f32"));
    const auto throwSpace = readF32 (dir + "throw_space.f32");
    const auto throwEcho = readF32 (dir + "throw_echo.f32");
    const auto ref = readF32 (dir + "ref.f32");
    std::vector<Event> events;
    {
        std::istringstream keys (slurp (dir + "keys.txt"));
        long long at;
        int lane, velocity;
        while (keys >> lane >> at >> velocity)
            events.push_back ({ at, Event::Kind::key, lane, velocity, 0, 0 });
        std::istringstream cc (slurp (dir + "cc.txt"));
        int channel, controller, value, ramp;
        while (cc >> channel >> controller >> value >> at >> ramp)
            events.push_back ({ at, Event::Kind::cc, channel, controller, value, ramp });
        std::stable_sort (events.begin(), events.end(), [] (const Event& a, const Event& b) { return a.time < b.time; });
    }

    Mixer mixer;
    mixer.prepare (48000.0, 512, 2);
    mixer.setConfig (&mc.value);
    const int latency = mixer.latencySamples();
    const int total = loop * passes + latency;
    Controls controls;
    controls.variant = variant == "novox" ? Variant::novox : Variant::vox;
    std::vector<float> out (static_cast<std::size_t> (2 * total));
    std::vector<float> l (512), r (512);
    float* outs[2] { l.data(), r.data() };
    std::size_t next = 0;
    double processNs = 0.0;
    for (int start = 0; start < total; start += 512)
    {
        const int n = std::min (512, total - start);
        mixer.bus().clear (n);
        mixer.sends().clear (n);
        for (int i = 0; i < n; ++i)
        {
            const int t = start + i;
            if (t >= loop * passes)
                continue;
            const auto k = static_cast<std::size_t> (t % loop);
            for (int c = 0; c < channels; ++c)
            {
                mixer.bus().left (c)[i] = dry[static_cast<std::size_t> (c)][2 * k];
                mixer.bus().right (c)[i] = dry[static_cast<std::size_t> (c)][2 * k + 1];
            }
            mixer.sends().send (Send::space)[i] = throwSpace[static_cast<std::size_t> (t)];
            mixer.sends().send (Send::echo)[i] = throwEcho[static_cast<std::size_t> (t)];
        }
        while (next < events.size() && events[next].time < start + n)
        {
            const auto& e = events[next++];
            const int offset = static_cast<int> (e.time - start);
            if (e.kind == Event::Kind::key)
                mixer.keyHit (static_cast<std::uint8_t> (e.a), e.b, offset);
            else
                mixer.cc (e.a, e.b, e.c, offset, e.d);
        }
        const double t0 = threadCpuNs();
        mixer.process (outs, 2, n, controls, bpm);
        processNs += threadCpuNs() - t0;
        for (int i = 0; i < n; ++i)
        {
            out[static_cast<std::size_t> (2 * (start + i))] = l[static_cast<std::size_t> (i)];
            out[static_cast<std::size_t> (2 * (start + i) + 1)] = r[static_cast<std::size_t> (i)];
        }
    }

    // the last pass: the reference at t, the mixer at t + latency
    double diff = 0.0, energy = 0.0, worst = 0.0;
    for (int t = loop * (passes - 1); t < loop * passes; ++t)
        for (int s = 0; s < 2; ++s)
        {
            const double want = ref[static_cast<std::size_t> (2 * t + s)];
            const double got = out[static_cast<std::size_t> (2 * (t + latency) + s)];
            diff += (got - want) * (got - want);
            energy += want * want;
            worst = std::max (worst, std::abs (got - want));
        }
    const double residualDb = 10.0 * std::log10 (diff / energy);
    WARN ("the mix took " << processNs / total << " ns/sample (" << 100.0 * processNs / total * 48000.0 * 1e-9 << " % of a core at 48 kHz)");
    WARN ("whole chain, last pass: residual " << residualDb << " dB under the loop, largest difference " << worst << " ("
                                              << 20.0 * std::log10 (worst) << " dBFS)");
    CHECK (residualDb <= -80.0);
}

// ------------------------------------------------------------------ CPU (P12's gate reads these)

// Hidden: ns per sample at 48 kHz (best of 5) for the costly blocks and a whole busy mix, in the thread's CPU time
// where the platform keeps it (threadCpuNs). Run it on a quiet machine all the same: a busy one slows the caches.
TEST_CASE ("mixer: CPU of the lane clip, the FDN, the limiter, the club master and a busy mix", "[.][cpu]")
{
    constexpr int n = 48000 * 2, block = 256;
    std::vector<float> l (n), r (n), ol (block), orr (block);
    for (int i = 0; i < n; ++i)
    {
        l[static_cast<std::size_t> (i)] = static_cast<float> (0.5 * std::sin (0.01 * i) + 0.2 * std::sin (0.37 * i));
        r[static_cast<std::size_t> (i)] = static_cast<float> (0.5 * std::sin (0.011 * i) + 0.2 * std::sin (0.29 * i));
    }
    auto best = [&] (const std::function<void()>& run) {
        double ns = 1e30;
        for (int rep = 0; rep < 5; ++rep)
        {
            const double t0 = threadCpuNs();
            run();
            ns = std::min (ns, (threadCpuNs() - t0) / n);
        }
        return ns;
    };
    auto report = [] (const char* what, double ns) {
        WARN (what << ": " << ns << " ns/sample, " << 100.0 * ns * 48000.0 * 1e-9 << " % of a core at 48 kHz");
    };
    {
        fx::LaneClip clip;
        clip.setCurve (-3.0, 1.5);
        auto x = l, y = r;
        report ("lane clip (stereo, half-band 4x)", best ([&] { for (int s = 0; s < n; s += block) clip.process (x.data() + s, y.data() + s, block); }));
    }
    {
        fx::Space space;
        space.prepare (48000.0);
        fx::SpaceSettings s;
        s.delaysFrames = { 520, 597, 685, 783, 899, 1033, 1167, 1329 };
        s.signs = { 1, -1, 1, -1, -1, 1, 1, -1 };
        s.t60S = 1.1;
        s.t60LowS = 0.4;
        s.shimmer = 0.03;
        space.setup (s);
        report ("Space (FDN, low band, shimmer, mono under 150)", best ([&] { for (int s0 = 0; s0 < n; s0 += block) space.process (l.data() + s0, ol.data(), orr.data(), block); }));
    }
    {
        fx::Limiter lim;
        lim.prepare (48000.0);
        lim.setup ({}, fx::Limiter::Hold::lookBack);
        auto x = l, y = r;
        report ("limiter (stereo, 4x true peak, look-back)", best ([&] { for (int s = 0; s < n; s += block) lim.process (x.data() + s, y.data() + s, block); }));
    }
    {
        mix::ClubMaster master;
        master.prepare (48000.0, block);
        MasterConfig m;
        m.glueThresholdMdb = -20000;
        m.driveMdb = 3000;
        master.setup ({ m, m }, Controls {}, 64);
        auto x = l, y = r;
        report ("club master (all of it)", best ([&] { for (int s = 0; s < n; s += block) master.process (x.data() + s, y.data() + s, block, Controls {}); }));
    }
    {
        const auto cfg = busyMix();
        Mixer mixer;
        mixer.prepare (48000.0, block, 2);
        mixer.setConfig (&cfg);
        float* outs[2] { ol.data(), orr.data() };
        report ("a busy mix (5 channels, clips, ducks, comp, cc_filter, all returns, club master)", best ([&] {
            for (int s = 0; s < n; s += block)
            {
                for (int c = 0; c < 5; ++c)
                {
                    std::copy_n (l.data() + s, block, mixer.bus().left (c));
                    std::copy_n (r.data() + s, block, mixer.bus().right (c));
                }
                mixer.process (outs, 2, block, Controls {}, 134.0);
            }
        }));
    }
}
