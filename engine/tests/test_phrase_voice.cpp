// PhraseVoice (P6, engine/sampler/phrase_voice.h): a phrase lane's stem read slice by slice, against
// tools/sound/phrase_tables.py's play () on the four kits' Loop files (vectors from
// tools/sound/export_phrase_vectors.py), the synthetic kit's lead phrase, and phrases built here on smooth stems.

#include "phrase_fixtures.h"

#include "alloc_counter.h"
#include "sampler/envelope.h" // kPi, samplesOf
#include "sampler/sinc_bank.h"
#include "sched/phrase_match.h"
#include "testing/synthetic_kit.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <random>
#include <vector>

using namespace phrase_test;
namespace testing = wubansia::engine::testing;

namespace
{
    constexpr double kRate = 48000.0;
    constexpr std::int64_t k16th = 5625; // frames of a 16th at 128 BPM and 48 kHz
    constexpr std::int64_t kCycle = 32 * k16th; // 2 bars

    double raisedIn (double n, double len) { return 0.5 - 0.5 * std::cos (kPi * n / len); }
    double raisedOut (double n, double len) { return 0.5 + 0.5 * std::cos (kPi * n / len); }

    // A smooth test stem: a 55 Hz sine at 0.25 (its own steps stay under -55 dBFS), the right side at 0.9.
    float sine (std::int64_t n, int c)
    {
        return static_cast<float> ((c == 0 ? 0.25 : 0.225) * std::sin (2.0 * kPi * 55.0 * static_cast<double> (n) / kRate));
    }

    // A 2-bar phrase at 128 BPM, no swing: 8 slices of four 16ths, lead 0, each fading over the last 5 ms of its end;
    // on a looped stem (the first slice starts on frame 0, the last ends on the stem's end).
    struct SineKit
    {
        PhraseKit k;
        int phrase = 0;

        explicit SineKit (int mixChannel = 0, std::int32_t gainMdb = 0, std::int64_t stemFrames = kCycle,
            const std::function<float (std::int64_t, int)>& b = {})
        {
            const int a = k.addStem (stemOf (stemFrames, 2, sine));
            const int bFile = b ? k.addStem (stemOf (stemFrames, 2, b)) : a;
            Phrase p;
            p.lane = Lane::lead;
            p.channel = 1;
            p.mixChannel = static_cast<std::uint8_t> (mixChannel);
            p.gainMdb = gainMdb;
            p.stem = { static_cast<std::uint16_t> (a), static_cast<std::uint16_t> (bFile) };
            p.ab = b ? static_cast<std::int8_t> (AbSwitch::stab) : std::int8_t { -1 };
            p.cycle16ths = 32;
            p.tolerancePm = 20;
            p.fadeUs = 5000;
            std::vector<SliceSpec> slices;
            for (int j = 0; j < 8; ++j)
            {
                SliceSpec s;
                s.step = 4 * j;
                s.key = 60 + j;
                s.start = 4 * j * k16th;
                s.end = j == 7 ? stemFrames : 4 * (j + 1) * k16th;
                s.fade = s.end - 240;
                slices.push_back (s);
            }
            phrase = k.addPhrase (p, slices);
        }

        const Kit& kit() const { return k.kit; }
        const PhraseSlice& slice (int j) const { return k.kit.slices[static_cast<std::size_t> (j)]; }
    };

    // Slices first..last of every cycle in [0, cycles), each at its first frame at a tempo `scale` times 128 BPM
    // (start sample = the slice's frame / scale, rounded), all reading on.
    std::vector<Hit> sineHits (const SineKit& s, int cycles, double scale = 1.0, int first = 0, int last = 7)
    {
        std::vector<Hit> out;
        for (int c = 0; c < cycles; ++c)
            for (int j = first; j <= last; ++j)
            {
                Hit h;
                h.at = std::llround (static_cast<double> (c * kCycle + s.slice (j).start) / scale);
                h.note = phraseNote (s.phrase, j, true);
                out.push_back (h);
            }
        return out;
    }

    // The synthetic kit's Loop, annotated.
    Timeline syntheticLoop()
    {
        const auto m = testing::syntheticKitManifest();
        auto t = timelineOf (testing::syntheticSmf (PlayMode::loop, Variant::vox), m.kit.swingPm, 15360);
        annotatePhrases (t, m);
        return t;
    }
}

TEST_CASE ("phrase voice: slices play within the phrase's tolerance of the kit's tempo, never with PhraseMode::off", "[phrase][voice]")
{
    Phrase p;
    p.tolerancePm = 20;
    CHECK (playsSlices (p, 128, 128.0, PhraseMode::automatic));
    CHECK (playsSlices (p, 128, 128.0 * 1.019, PhraseMode::automatic));
    CHECK (playsSlices (p, 128, 128.0 * 0.981, PhraseMode::automatic));
    CHECK (playsSlices (p, 128, 130.56, PhraseMode::automatic)); // 2 % exactly
    CHECK (playsSlices (p, 128, 125.44, PhraseMode::automatic));
    CHECK_FALSE (playsSlices (p, 128, 128.0 * 1.021, PhraseMode::automatic));
    CHECK_FALSE (playsSlices (p, 128, 128.0 * 0.979, PhraseMode::automatic));
    CHECK_FALSE (playsSlices (p, 128, 128.0, PhraseMode::off));
    CHECK_FALSE (playsSlices (p, 128, 0.0, PhraseMode::automatic)); // no tempo known
    CHECK_FALSE (playsSlices (p, 0, 128.0, PhraseMode::automatic));
    p.tolerancePm = 0;
    CHECK (playsSlices (p, 134, 134.0, PhraseMode::automatic));
    CHECK_FALSE (playsSlices (p, 134, 134.01, PhraseMode::automatic));
}

TEST_CASE ("phrase voice: SYN01's lead lane is its stem tiled, bit for bit, for 200 cycles from bar 1", "[phrase][voice]")
{
    const auto kit = testing::makeSyntheticKit();
    const auto& phrase = kit->phrases.at (0);
    const auto stem = kit->view (phrase.stem[0]);
    REQUIRE (stem.frames == kCycle);
    const auto t = syntheticLoop();
    constexpr int loops = 100;
    const auto hits = hitsOf (t, 128.0, kRate, loops);
    REQUIRE (hits.size() == 16u * loops);

    Rig rig (kRate, static_cast<int> (kit->mix.channels.size()), 2);
    rig.collect = false;
    const int ch = phrase.mixChannel;
    std::int64_t wrong = 0, firstWrong = -1, sounding = 0;
    rig.listen = [&] (std::int64_t start, int n, const MixBus& bus, const SendBus&) {
        for (int i = 0; i < n; ++i)
        {
            const auto f = start + i;
            const auto k = f % stem.frames;
            const double g = f < 144 ? raisedIn (static_cast<double> (f), 144.0) : 1.0; // the run's 3 ms, once
            const auto l = static_cast<float> (static_cast<double> (stem.left[k]) * g);
            const auto r = static_cast<float> (static_cast<double> (stem.right[k]) * g);
            bool ok = same (bus.left (ch)[i], l) && same (bus.right (ch)[i], r);
            for (int c = 0; c < bus.numChannels(); ++c)
                ok = ok && (c == ch || (same (bus.left (c)[i], 0.0f) && same (bus.right (c)[i], 0.0f)));
            if (! ok && firstWrong < 0)
                firstWrong = f;
            wrong += ok ? 0 : 1;
            sounding += same (l, 0.0f) ? 0 : 1;
        }
    };
    rig.run (*kit, hits, loops * 2 * kCycle);
    INFO ("first wrong sample " << firstWrong);
    CHECK (wrong == 0);
    CHECK (sounding > loops * kCycle);
    const auto& stats = rig.voice (0).stats();
    CHECK (stats.runs == 1); // no restart
    CHECK (stats.readOns == 16 * loops - 1);
    CHECK (stats.jumps == 0);
    CHECK (stats.resyncs == 0);
    CHECK (stats.missed == 0);
    CHECK (stats.dropped == 0);
}

TEST_CASE ("phrase voice: the four kits' Loop lanes are the Python model's, bit for bit", "[phrase][voice][vectors]")
{
    const auto v = readJson ("voice.json");
    const auto block = integer (v, "block");
    for (const auto& c : at (v, "cases").items)
    {
        const auto& p = at (c, "phrase");
        CAPTURE (at (c, "kit").string, at (c, "variant").string, at (c, "line").string);
        PhraseKit k;
        const auto stemFrames = integer (p, "stem_frames");
        const int file = k.addStem (noiseStem (static_cast<std::uint32_t> (integer (c, "seed")), stemFrames,
            static_cast<int> (integer (c, "channels"))));
        Phrase phrase;
        phrase.lane = static_cast<Lane> (laneIndex (at (p, "lane").string));
        phrase.channel = static_cast<std::uint8_t> (integer (p, "ch") - 1);
        phrase.gainMdb = static_cast<std::int32_t> (integer (p, "gain_mdb"));
        phrase.stem = { static_cast<std::uint16_t> (file), static_cast<std::uint16_t> (file) };
        phrase.cycle16ths = static_cast<std::int32_t> (integer (p, "cycle_16ths"));
        phrase.tolerancePm = static_cast<std::int32_t> (integer (p, "tolerance_pm"));
        phrase.fadeUs = static_cast<std::int32_t> (integer (p, "fade_us"));
        std::vector<SliceSpec> slices;
        for (const auto& s : at (p, "slices").items)
        {
            SliceSpec x;
            x.step = static_cast<int> (s.items.at (0).integer);
            x.key = static_cast<int> (s.items.at (1).integer);
            x.start = s.items.at (2).integer;
            x.end = s.items.at (3).integer;
            x.fade = s.items.at (4).integer;
            x.lead = static_cast<int> (s.items.at (5).integer);
            slices.push_back (x);
        }
        const int index = k.addPhrase (phrase, slices);

        const auto length = integer (c, "loop_frames");
        std::vector<Hit> hits;
        for (int pass = 0; pass < 3; ++pass)
            for (const auto& e : at (c, "events").items)
            {
                Hit h;
                h.at = pass * length + e.items.at (0).integer;
                h.note = phraseNote (index, static_cast<int> (e.items.at (1).integer), e.items.at (2).integer != 0);
                hits.push_back (h);
            }
        std::stable_sort (hits.begin(), hits.end(), [] (const Hit& a, const Hit& b) { return a.at < b.at; });

        Rig rig (kRate, 1, 4);
        rig.run (k.kit, hits, 3 * length);
        const auto& l = rig.left[0];
        const auto& r = rig.right[0];
        const auto from = static_cast<std::size_t> (2 * length); // the third pass: tails wrapped round, as the model's
        const auto& blocks = at (c, "blocks").items;
        std::size_t firstBad = blocks.size();
        for (std::size_t b = 0; b < blocks.size() && firstBad == blocks.size(); ++b)
        {
            const auto lo = from + b * static_cast<std::size_t> (block);
            const auto hi = std::min (lo + static_cast<std::size_t> (block), from + static_cast<std::size_t> (length));
            if (sha256Planar (l, r, lo, hi).substr (0, 16) != blocks[b].string)
                firstBad = b;
        }
        INFO ("first block that differs: " << firstBad << " of " << blocks.size());
        CHECK (firstBad == blocks.size());
        CHECK (sha256Planar (l, r, from, from + static_cast<std::size_t> (length)) == at (c, "sha256").string);
    }
}

TEST_CASE ("phrase voice: its fades are the recipe's masks: FUT02's 5 ms mute edge and the arp's rest, within 1e-6", "[phrase][voice][vectors]")
{
    const auto v = readJson ("voice.json");
    const auto values = readFloats ("masks.f32");
    for (const auto& c : at (v, "masks").items)
    {
        CAPTURE (at (c, "line").string);
        const auto& p = at (c, "phrase");
        PhraseKit k;
        const int file = k.addStem (stemOf (integer (p, "stem_frames"), 2, [] (std::int64_t, int) { return 1.0f; }));
        Phrase phrase;
        phrase.channel = static_cast<std::uint8_t> (integer (p, "ch") - 1);
        phrase.stem = { static_cast<std::uint16_t> (file), static_cast<std::uint16_t> (file) };
        phrase.cycle16ths = static_cast<std::int32_t> (integer (p, "cycle_16ths"));
        phrase.fadeUs = static_cast<std::int32_t> (integer (p, "fade_us"));
        std::vector<SliceSpec> slices;
        for (const auto& s : at (p, "slices").items)
            slices.push_back ({ static_cast<int> (s.items.at (0).integer), static_cast<int> (s.items.at (1).integer),
                s.items.at (2).integer, s.items.at (3).integer, s.items.at (4).integer, static_cast<int> (s.items.at (5).integer), {} });
        const int index = k.addPhrase (phrase, slices);
        const auto length = integer (c, "loop_frames");
        std::vector<Hit> hits;
        for (int pass = 0; pass < 3; ++pass)
            for (const auto& e : at (c, "events").items)
                hits.push_back ({ pass * length + e.items.at (0).integer,
                    phraseNote (index, static_cast<int> (e.items.at (1).integer), e.items.at (2).integer != 0) });
        std::stable_sort (hits.begin(), hits.end(), [] (const Hit& a, const Hit& b) { return a.at < b.at; });
        Rig rig (kRate, 1, 4);
        rig.run (k.kit, hits, 3 * length);
        for (const auto& span : at (c, "spans").items)
        {
            const auto lo = integer (span, "from");
            const auto hi = integer (span, "to");
            const auto offset = integer (span, "offset");
            double worst = 0.0;
            for (auto f = lo; f < hi; ++f)
            {
                const auto got = static_cast<double> (rig.left[0][static_cast<std::size_t> (2 * length + f)]);
                worst = std::max (worst, std::fabs (got - static_cast<double> (values[static_cast<std::size_t> (offset + f - lo)])));
            }
            CAPTURE (lo, hi);
            CHECK (worst <= 1e-6);
        }
    }
}

TEST_CASE ("phrase voice: a seek starts mid-slice at the right frame, and is bit-exact from there on", "[phrase][voice]")
{
    const auto kit = testing::makeSyntheticKit();
    const auto& phrase = kit->phrases.at (0);
    const auto stem = kit->view (phrase.stem[0]);
    const auto t = syntheticLoop();
    const auto all = hitsOf (t, 128.0, kRate, 2);
    const int ch = phrase.mixChannel;

    // Seek 1000 frames into slice 2 (step 8): its note comes chased; the rest come on time.
    const auto& s2 = kit->slices.at (2);
    const auto seekAt = s2.start + 1000; // the loop's frame where play starts (frame = sample at 48 kHz, 128 BPM)
    std::vector<Hit> hits;
    hits.push_back ({ 0, phraseNote (0, 2, true, 1000) });
    for (const auto& h : all)
        if (h.at > seekAt)
            hits.push_back ({ h.at - seekAt, h.note });
    Rig rig (kRate, static_cast<int> (kit->mix.channels.size()), 2);
    rig.run (*kit, hits, 3 * kCycle);

    const double chase = samplesOf (Sampler::kChaseFadeUs, kRate);
    std::int64_t wrong = 0, firstWrong = -1;
    for (std::int64_t n = 0; n < 3 * kCycle; ++n)
    {
        const auto k = (seekAt + n) % stem.frames;
        const double g = static_cast<double> (n) < chase ? raisedIn (static_cast<double> (n), chase) : 1.0;
        const auto want = static_cast<float> (static_cast<double> (stem.left[k]) * g);
        if (! same (rig.left[static_cast<std::size_t> (ch)][static_cast<std::size_t> (n)], want))
        {
            ++wrong;
            if (firstWrong < 0)
                firstWrong = n;
        }
    }
    INFO ("first wrong sample " << firstWrong);
    CHECK (wrong == 0);
    CHECK (rig.voice (0).stats().runs == 1);
    CHECK (rig.voice (0).stats().jumps == 0);
}

TEST_CASE ("phrase voice: at 44.1 kHz the lane is a resample of the tiled stem, within -80 dB", "[phrase][voice]")
{
    constexpr double rate = 44100.0;
    const auto kit = testing::makeSyntheticKit();
    const auto& phrase = kit->phrases.at (0);
    const auto stem = kit->view (phrase.stem[0]);
    const auto t = syntheticLoop();
    const auto hits = hitsOf (t, 128.0, rate, 2);
    Rig rig (rate, static_cast<int> (kit->mix.channels.size()), 2);
    const auto total = samplesAt (2 * kLoopLength, 128.0, rate);
    rig.run (*kit, hits, total);

    // The reference: the stem tiled, read through the same sinc bank at n x 48000 / rate.
    std::vector<float> tiled (static_cast<std::size_t> (4 * stem.frames));
    for (std::size_t i = 0; i < tiled.size(); ++i)
        tiled[i] = stem.left[static_cast<std::int64_t> (i) % stem.frames];
    const auto& bank = SincBank::instance();
    const double ratio = 48000.0 / rate;
    const float* table = bank.table (bank.bankFor (ratio));
    std::vector<double> ref (static_cast<std::size_t> (total));
    for (std::int64_t n = 0; n < total; ++n)
        ref[static_cast<std::size_t> (n)] = sincRead (table, tiled.data(), static_cast<std::int64_t> (tiled.size()), static_cast<double> (n) * ratio);
    const auto db = nullDb (rig.left[static_cast<std::size_t> (phrase.mixChannel)], ref, 400, static_cast<std::size_t> (total));
    CAPTURE (db);
    CHECK (db <= -80.0);
    CHECK (rig.voice (0).stats().jumps == 0);
    CHECK (rig.voice (0).stats().resyncs == 0);
}

TEST_CASE ("phrase voice: within the tolerance off the kit's tempo the slices play on, re-synced without a click", "[phrase][voice]")
{
    for (const double scale : { 1.019, 0.981 })
    {
        CAPTURE (scale);
        const SineKit s;
        const auto hits = sineHits (s, 4, scale);
        Rig rig (kRate, 1, 2);
        const auto total = std::llround (4.0 * static_cast<double> (kCycle) / scale);
        rig.run (s.kit(), hits, total);
        const auto& out = rig.left[0];
        CHECK (maxStep (out, 1, out.size()) <= 0.01); // -40 dBFS
        double quietest = 1.0;
        for (std::size_t w = 0; w + 1024 <= out.size(); w += 1024)
        {
            double e = 0.0;
            for (std::size_t i = w; i < w + 1024; ++i)
                e += static_cast<double> (out[i]) * out[i];
            quietest = std::min (quietest, std::sqrt (e / 1024.0));
        }
        CHECK (quietest > 0.1); // the sine's RMS is 0.177: no gap anywhere
        CHECK (rig.voice (0).stats().jumps > 0);
        CHECK (rig.voice (0).stats().runs == 1);
    }
}

TEST_CASE ("phrase voice: a jump crossfades over 3 ms; a slice without a follower fades from its fade frame", "[phrase][voice]")
{
    const SineKit s;
    const auto& stemView = s.kit().view (0);

    SECTION ("a jump: slice 5 where slice 3 was expected")
    {
        std::vector<Hit> hits { { 0, phraseNote (s.phrase, 2, true) } };
        hits.push_back ({ s.slice (3).start - s.slice (2).start, phraseNote (s.phrase, 5, true) });
        Rig rig (kRate, 1, 2);
        rig.run (s.kit(), hits, 3 * 4 * k16th);
        const auto& out = rig.left[0];
        const auto at = static_cast<std::size_t> (s.slice (3).start - s.slice (2).start);
        CHECK (maxStep (out, 1, out.size()) <= 0.01);
        CHECK (rig.voice (0).stats().jumps == 1);
        // after 144 samples only the new slice sounds, exactly
        for (std::size_t i = at + 144; i < at + 4 * k16th; ++i)
            REQUIRE (same (out[i], stemView.left[s.slice (5).start + static_cast<std::int64_t> (i - at)]));
        // the crossfade itself: the old position fading out and the new one fading in, sample for sample
        for (std::size_t i = at; i < at + 144; ++i)
        {
            const double n = static_cast<double> (i - at);
            const double old = stemView.left[s.slice (3).start + static_cast<std::int64_t> (i - at)];
            const double next = stemView.left[s.slice (5).start + static_cast<std::int64_t> (i - at)];
            REQUIRE (std::fabs (out[i] - (old * raisedOut (n, 144.0) + next * raisedIn (n, 144.0))) <= 1e-6);
        }
    }

    SECTION ("no follower: it fades over fade_us from its fade frame and is silent from there")
    {
        Rig rig (kRate, 1, 2);
        rig.run (s.kit(), { { 0, phraseNote (s.phrase, 2, false) } }, 6 * k16th);
        const auto& out = rig.left[0];
        const auto& sl = s.slice (2);
        for (std::int64_t f = 0; f < 6 * k16th; ++f)
        {
            const auto frame = sl.start + f;
            double g = 1.0;
            if (frame >= sl.fade)
                g = frame - sl.fade < 240 ? raisedOut (static_cast<double> (frame - sl.fade), 240.0) : 0.0;
            if (frame >= sl.end)
                g = 0.0;
            if (f < 144)
                g *= raisedIn (static_cast<double> (f), 144.0); // the run's fade-in
            REQUIRE (same (out[static_cast<std::size_t> (f)], static_cast<float> (static_cast<double> (stemView.left[frame]) * g)));
        }
        CHECK_FALSE (rig.voice (0).active());
    }
}

TEST_CASE ("phrase voice: cycle wraps re-sync over 1 ms; no drift after 1,000 cycles", "[phrase][voice]")
{
    const SineKit s;
    constexpr int cycles = 1000;
    constexpr double scale = 1.0 - 2.0e-6; // each cycle 0.36 samples longer than the stem's
    const auto hits = sineHits (s, cycles, scale);
    Rig rig (kRate, 1, 2);
    rig.collect = false;
    double worstStep = 0.0;
    float last = 0.0f;
    rig.listen = [&] (std::int64_t start, int n, const MixBus& bus, const SendBus&) {
        for (int i = 0; i < n; ++i)
        {
            if (start + i > 0)
                worstStep = std::max (worstStep, std::fabs (static_cast<double> (bus.left (0)[i]) - last));
            last = bus.left (0)[i];
        }
    };
    const auto lastCycle = std::llround (static_cast<double> ((cycles - 1) * kCycle) / scale);
    rig.run (s.kit(), hits, lastCycle + 100);
    const auto& stats = rig.voice (0).stats();
    CAPTURE (stats.resyncs, stats.jumps, stats.readOns);
    CHECK (stats.jumps == 0);
    CHECK (stats.resyncs >= 250);
    CHECK (stats.resyncs <= 400);
    CHECK (std::fabs (rig.voice (0).readPosition() - 100.0) <= 1.0); // where the last cycle's note says
    CHECK (worstStep <= 0.01);
}

TEST_CASE ("phrase voice: notes a frame or two off the read position read on", "[phrase][voice]")
{
    const SineKit s;
    auto hits = sineHits (s, 3);
    for (std::size_t i = 0; i < hits.size(); ++i)
        if (hits[i].note.slice != 0) // mid-cycle, jittered by up to 2 samples either way
            hits[i].at += static_cast<std::int64_t> (i % 5) - 2;
    Rig rig (kRate, 1, 2);
    rig.run (s.kit(), hits, 3 * kCycle);
    const auto& stemView = s.kit().view (0);
    for (std::int64_t n = 144; n < 3 * kCycle; ++n)
        REQUIRE (same (rig.left[0][static_cast<std::size_t> (n)], stemView.left[n % kCycle]));
    CHECK (rig.voice (0).stats().jumps == 0);
    CHECK (rig.voice (0).stats().resyncs == 0);
}

TEST_CASE ("phrase voice: a slice that plays across the cycle's wrap plays out under the next cycle's run", "[phrase][voice]")
{
    // FUT03's vox, small: a 3-bar stem for a 2-bar cycle; slice 0 starts on step 4 (not frame 0: the stem doesn't
    // loop), slice 1 (step 28) runs to the stem's end, a bar past the cycle, fading over its last 5 ms.
    PhraseKit k;
    const auto frames = kCycle + 16 * k16th;
    const int file = k.addStem (noiseStem (77, frames, 2));
    Phrase p;
    p.stem = { static_cast<std::uint16_t> (file), static_cast<std::uint16_t> (file) };
    p.cycle16ths = 32;
    p.fadeUs = 5000;
    const SliceSpec a { 4, 60, 4 * k16th - 480, 28 * k16th - 480, 28 * k16th - 720, 480, {} };
    const SliceSpec b { 28, 62, 28 * k16th - 480, frames, frames - 240, 480, {} };
    const int index = k.addPhrase (p, { a, b });
    std::vector<Hit> hits;
    for (int c = 0; c < 3; ++c)
    {
        hits.push_back ({ c * kCycle + a.start, phraseNote (index, 0, true) });
        hits.push_back ({ c * kCycle + b.start, phraseNote (index, 1, false) });
    }
    Rig rig (kRate, 1, 2);
    rig.run (k.kit, hits, 4 * kCycle);

    // The model: each note's slice placed at its start, the run's first slice fading in over 3 ms, slice 1 (no
    // follower) fading from its fade frame; tails summed.
    std::vector<double> want (static_cast<std::size_t> (4 * kCycle), 0.0);
    const auto& stem = k.kit.view (0);
    for (int c = 0; c < 3; ++c)
    {
        for (auto f = a.start; f < a.end; ++f)
        {
            const double g = f - a.start < 144 ? raisedIn (static_cast<double> (f - a.start), 144.0) : 1.0;
            want[static_cast<std::size_t> (c * kCycle + f)] += static_cast<double> (stem.left[f]) * g;
        }
        for (auto f = b.start; f < b.end; ++f)
        {
            const double g = f >= b.fade ? raisedOut (static_cast<double> (f - b.fade), 240.0) : 1.0;
            want[static_cast<std::size_t> (c * kCycle + f)] += static_cast<double> (stem.left[f]) * g;
        }
    }
    std::int64_t wrong = 0, firstWrong = -1;
    for (std::size_t n = 0; n < want.size(); ++n)
        if (! same (rig.left[0][n], static_cast<float> (want[n])))
        {
            ++wrong;
            if (firstWrong < 0)
                firstWrong = static_cast<std::int64_t> (n);
        }
    INFO ("first wrong sample " << firstWrong);
    CHECK (wrong == 0);
    CHECK (rig.voice (0).stats().runs == 3);
    CHECK (rig.voice (0).stats().jumps == 0);
}

TEST_CASE ("phrase voice: the other A/B stem mid-run crossfades to it at the same frame", "[phrase][voice]")
{
    const SineKit s (0, 0, kCycle, [] (std::int64_t n, int c) { return 0.5f * sine (n, c); });
    auto hits = sineHits (s, 1);
    for (auto& h : hits)
        if (h.note.slice >= 4)
            h.note.character = 1;
    Rig rig (kRate, 1, 2);
    rig.run (s.kit(), hits, kCycle);
    const auto& stemB = s.kit().view (1);
    const auto at = static_cast<std::size_t> (s.slice (4).start);
    for (std::size_t i = at + 144; i < static_cast<std::size_t> (kCycle); ++i)
        REQUIRE (same (rig.left[0][i], stemB.left[static_cast<std::int64_t> (i)]));
    CHECK (maxStep (rig.left[0], 1, static_cast<std::size_t> (kCycle)) <= 0.01);
    CHECK (rig.voice (0).stats().jumps == 1);
}

TEST_CASE ("phrase voice: a follower that never comes fades over fade_us once it reads 2 frames into it", "[phrase][voice]")
{
    const SineKit s;
    Rig rig (kRate, 1, 2);
    rig.run (s.kit(), { { 0, phraseNote (s.phrase, 2, true) } }, 8 * k16th);
    const auto& stemView = s.kit().view (0);
    const auto& out = rig.left[0];
    const auto end = static_cast<std::size_t> (s.slice (3).start - s.slice (2).start);
    for (std::size_t i = 144; i <= end + 2; ++i)
        REQUIRE (same (out[i], stemView.left[s.slice (2).start + static_cast<std::int64_t> (i)]));
    for (std::size_t i = end + 3; i < end + 3 + 240; ++i)
        REQUIRE (std::fabs (out[i] - stemView.left[s.slice (2).start + static_cast<std::int64_t> (i)] * raisedOut (static_cast<double> (i - end - 3), 240.0)) <= 1e-7);
    CHECK (silent (out.data() + end + 3 + 240, out.size() - (end + 3 + 240)));
    CHECK (rig.voice (0).stats().missed == 1);
    CHECK_FALSE (rig.voice (0).active());
}

TEST_CASE ("phrase voice: the stem's gain, the phrase's mix channel, and the throws into the sends", "[phrase][voice]")
{
    const SineKit s (2, -6000);
    auto hits = sineHits (s, 1, 1.0, 0, 1);
    hits[0].note.throwSpace = 0.5f;
    hits[0].note.throwEcho = 0.25f;
    hits[1].note.continues = false;
    hits[1].note.throwSpace = 0.5f;
    hits[1].note.throwEcho = 0.25f;
    Rig rig (kRate, 4, 2);
    rig.run (s.kit(), hits, kCycle / 2, {}, 0.8f);
    const auto& stemView = s.kit().view (0);
    const double gain = std::pow (10.0, (-6000 / 1000.0) / 20.0);
    for (int c : { 0, 1, 3 })
        CHECK (silent (rig.left[static_cast<std::size_t> (c)].data(), rig.left[static_cast<std::size_t> (c)].size()));
    for (std::size_t i = 144; i < static_cast<std::size_t> (s.slice (1).fade); ++i)
    {
        const double l = static_cast<double> (stemView.left[static_cast<std::int64_t> (i)]) * gain;
        const double r = static_cast<double> (stemView.right[static_cast<std::int64_t> (i)]) * gain;
        REQUIRE (same (rig.left[2][i], static_cast<float> (l)));
        REQUIRE (same (rig.right[2][i], static_cast<float> (r)));
        const double mono = 0.5 * (l + r) * 0.8;
        REQUIRE (std::fabs (rig.space[i] - mono * 0.5) <= 1e-7);
        REQUIRE (std::fabs (rig.echo[i] - mono * 0.25) <= 1e-7);
    }
}

TEST_CASE ("phrase voice: stop fades every head and leaves it idle; reset silences at once", "[phrase][voice]")
{
    const SineKit s;
    SECTION ("stop over 480 samples")
    {
        Rig rig (kRate, 1, 2);
        rig.run (s.kit(), sineHits (s, 1), 2 * k16th);
        rig.voice (0).stop (5, 480);
        rig.run (s.kit(), {}, 1024);
        const auto& out = rig.left[0];
        const auto& stemView = s.kit().view (0);
        const auto from = static_cast<std::size_t> (2 * k16th + 5);
        for (std::size_t i = from; i < from + 480; ++i)
            REQUIRE (std::fabs (out[i] - stemView.left[static_cast<std::int64_t> (i)] * raisedOut (static_cast<double> (i - from), 480.0)) <= 1e-7);
        CHECK (silent (out.data() + from + 480, out.size() - (from + 480)));
        CHECK_FALSE (rig.anyActive());
    }
    SECTION ("stop at once, and reset")
    {
        Rig rig (kRate, 1, 2);
        rig.run (s.kit(), sineHits (s, 1), 2 * k16th);
        rig.voice (0).stop (0, 0);
        rig.run (s.kit(), {}, 64);
        CHECK (silent (rig.left[0].data() + rig.left[0].size() - 64, 64));
        CHECK_FALSE (rig.anyActive());

        PhraseVoice v;
        v.prepare (kRate);
        v.start (s.kit(), phraseNote (s.phrase, 0, true), 0);
        CHECK (v.active());
        v.reset();
        CHECK_FALSE (v.active());
    }
}

TEST_CASE ("phrase voice: nothing allocates or frees on the audio path", "[phrase][voice][rt]")
{
    if (! allocations::available())
        SKIP ("no allocation counter in this build");
    const SineKit s;
    MixBus bus;
    SendBus sends;
    bus.prepare (1, 32);
    sends.prepare (32);
    PhraseVoice v;
    v.prepare (44100.0); // the sinc path too
    std::array<float, 32> lane {};
    lane.fill (1.0f);
    allocations::Scope scope;
    v.start (s.kit(), phraseNote (s.phrase, 0, true), 3);
    for (int piece = 0; piece < 2000; ++piece)
    {
        bus.clear (32);
        sends.clear (32);
        if (piece % 150 == 7)
            v.continueWith (phraseNote (s.phrase, (piece / 150) % 8, piece % 300 != 7), 11);
        if (piece == 1900)
            v.stop (0, 64);
        v.render (bus, sends, 32, lane.data());
    }
    const auto counts = scope.counts();
    CHECK (counts.allocations == 0);
    CHECK (counts.frees == 0);
    CHECK_FALSE (v.active());
}

TEST_CASE ("phrase voice: any piece sizes give the same bytes", "[phrase][voice]")
{
    const auto kit = testing::makeSyntheticKit();
    const auto& phrase = kit->phrases.at (0);
    const auto t = syntheticLoop();
    auto hits = hitsOf (t, 128.0, kRate, 2);
    for (std::size_t i = 0; i < hits.size(); i += 5) // some jumps and runs too
        hits[i].at += 7;
    auto render = [&] (const std::function<int (std::int64_t)>& piece) {
        Rig rig (kRate, static_cast<int> (kit->mix.channels.size()), 2);
        rig.run (*kit, hits, 4 * kCycle, piece);
        return rig.left[static_cast<std::size_t> (phrase.mixChannel)];
    };
    const auto whole = render ({});
    std::mt19937 rng (5);
    CHECK (sameBits (render ([] (std::int64_t) { return 1; }), whole));
    CHECK (sameBits (render ([] (std::int64_t) { return 7; }), whole));
    CHECK (sameBits (render ([] (std::int64_t) { return 13; }), whole));
    CHECK (sameBits (render ([&rng] (std::int64_t) { return static_cast<int> (rng() % 32) + 1; }), whole));
}
