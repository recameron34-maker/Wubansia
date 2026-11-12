// The Samples drawer's audition player and clips (engine/audition, ADR 0024).
// - A clip plays from its first frame at -6 dB, added to what the engine left; a mono clip feeds both outputs and one
//   output gets (L + R) / 2, the engine's fold.
// - A new offer replaces the playing clip at once while the old one fades out over 5 ms (240 samples at 48 kHz);
//   stopUpTo fades a playing clip out and lets an offered one go unplayed; a newer ticket still plays.
// - Silent blocks (offline renders, bypass) and the host's reset write nothing and let every clip go; a clip made for
//   another rate never plays; an idle player leaves the output bit for bit as it was, -0.0 included.
// - Every clip comes back exactly once (offer, collectRetired, the destructor); a full retire FIFO holds clips
//   silently and takes nothing new until there's room; process allocates and frees nothing.
// - makeAuditionClip: the file itself at 48 kHz; elsewhere ceil (frames x rate / 48000) frames read through the
//   voices' sinc bank, in tune, the same every time.
// - A worker, a stopper and the audio thread at once account for every clip: run it under TSan too (engine_tests
//   "[audition]" in a -fsanitize=thread build).

#include "alloc_counter.h"

#include "audition/audition_clip.h"
#include "audition/audition_player.h"
#include "sampler/sinc_bank.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <atomic>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <random>
#include <thread>
#include <vector>

using namespace wubansia::engine;

namespace
{
    constexpr double kRate = 48000.0;
    constexpr std::size_t kFade = 240; // 5 ms at 48 kHz
    constexpr double kPi = 3.14159265358979323846;

    bool same (float a, float b) noexcept { return std::bit_cast<std::uint32_t> (a) == std::bit_cast<std::uint32_t> (b); }
    bool same (double a, double b) noexcept { return std::bit_cast<std::uint64_t> (a) == std::bit_cast<std::uint64_t> (b); }

    // Deterministic values in [-1, 1): a signal no clip shape or gain can fake.
    std::vector<float> noise (std::size_t frames, std::uint32_t seed)
    {
        std::vector<float> x (frames);
        std::uint32_t s = seed * 2654435761u + 12345u;
        for (auto& v : x)
        {
            s = s * 1664525u + 1013904223u;
            v = static_cast<float> (static_cast<double> (s >> 8) / 8388608.0 - 1.0);
        }
        return x;
    }

    std::vector<float> sine (std::size_t frames, double hz, double amplitude, double phase = 0.0)
    {
        std::vector<float> x (frames);
        for (std::size_t n = 0; n < frames; ++n)
            x[n] = static_cast<float> (amplitude * std::sin (2.0 * kPi * hz * static_cast<double> (n) / kRate + phase));
        return x;
    }

    // A clip as the sound thread offers it (planar; right empty for a mono clip), owned by whoever it's handed to.
    const AuditionClip* clipOf (const std::vector<float>& left, const std::vector<float>& right, std::uint32_t ticket, double rate = kRate)
    {
        auto* clip = new AuditionClip();
        clip->frames = static_cast<std::int64_t> (left.size());
        clip->channels = right.empty() ? 1 : 2;
        clip->audio = left;
        clip->audio.insert (clip->audio.end(), right.begin(), right.end());
        clip->rate = rate;
        clip->ticket = ticket;
        return clip;
    }

    // The fade-out's gain on its i-th sample: (L - i) / L, from 1 down to 1 / L, then nothing.
    float fadeGain (std::size_t i) { return static_cast<float> (kFade - i) / static_cast<float> (kFade); }

    // An output of some channels, holding what the engine might have left there (-0.0 first), and a copy of it.
    struct Out
    {
        explicit Out (int frames, int channels = 2, std::uint32_t seed = 99)
        {
            for (int c = 0; c < channels; ++c)
            {
                data.push_back (noise (static_cast<std::size_t> (frames), seed + static_cast<std::uint32_t> (c)));
                data.back()[0] = -0.0f;
            }
            for (auto& d : data)
                pointers.push_back (d.data());
            before = data;
        }

        float* const* ptrs() noexcept { return pointers.data(); }

        std::vector<std::vector<float>> data, before;
        std::vector<float*> pointers;
    };

    // Whether every channel of out from `from` on is still, bit for bit, what was there.
    bool untouched (const Out& out, std::size_t from = 0)
    {
        for (std::size_t c = 0; c < out.data.size(); ++c)
        {
            const auto& now = out.data[c];
            const auto& then = out.before[c];
            if (from < now.size() && std::memcmp (now.data() + from, then.data() + from, (now.size() - from) * sizeof (float)) != 0)
                return false;
        }
        return true;
    }

    // How many of channel c's samples [from, from + count) aren't, bit for bit, expected (i), i counted from `from`.
    template <class F>
    int mismatches (const Out& out, std::size_t c, std::size_t from, std::size_t count, F&& expected)
    {
        int wrong = 0;
        for (std::size_t i = 0; i < count; ++i)
            if (! same (out.data[c][from + i], expected (i)))
                ++wrong;
        return wrong;
    }

    // The clips the player hands back, in order. It deletes each one once at the end; a clip the player still
    // holds is the player's to delete.
    struct Returns
    {
        Returns() { clips.reserve (64); }

        ~Returns()
        {
            auto unique = clips;
            std::sort (unique.begin(), unique.end());
            unique.erase (std::unique (unique.begin(), unique.end()), unique.end());
            for (const auto* clip : unique)
                delete clip;
        }

        void add (const AuditionClip* clip)
        {
            if (clip != nullptr)
                clips.push_back (clip);
        }

        void collect (AuditionPlayer& player)
        {
            player.collectRetired ([this] (const AuditionClip* clip) { clips.push_back (clip); });
        }

        int count (const AuditionClip* clip) const { return static_cast<int> (std::count (clips.begin(), clips.end(), clip)); }

        std::vector<const AuditionClip*> clips;
    };

    // The frequency of a steady tone, from its upward zero crossings (interpolated), in Hz at rate.
    double frequencyOf (const std::vector<float>& y, std::size_t from, double rate)
    {
        double first = -1.0, last = -1.0;
        int crossings = 0;
        for (std::size_t n = std::max<std::size_t> (from, 1); n < y.size(); ++n)
        {
            const double a = y[n - 1], b = y[n];
            if (a < 0.0 && b >= 0.0)
            {
                const double t = static_cast<double> (n - 1) + a / (a - b);
                if (first < 0.0)
                    first = t;
                last = t;
                ++crossings;
            }
        }
        REQUIRE (crossings > 10);
        return (crossings - 1) / (last - first) * rate;
    }
}

TEST_CASE ("a clip plays from its first frame at -6 dB, added to the output", "[audition]")
{
    AuditionPlayer player;
    CHECK (same (player.rate(), 0.0));
    player.prepare (kRate);
    CHECK (same (player.rate(), kRate));
    CHECK (same (AuditionPlayer::kGain, 0.5f));

    Returns back;
    const auto left = noise (1000, 1), right = noise (1000, 2);
    const auto* clip = clipOf (left, right, 1);
    back.add (player.offer (clip));
    CHECK (back.clips.empty());
    CHECK (player.offered());
    CHECK (player.sounding() == 0u);

    Out first (512);
    player.process (first.ptrs(), 2, 512, false);
    CHECK_FALSE (player.offered());
    CHECK (player.sounding() == 1u);
    CHECK (mismatches (first, 0, 0, 512, [&] (std::size_t i) { return first.before[0][i] + 0.5f * left[i]; }) == 0);
    CHECK (mismatches (first, 1, 0, 512, [&] (std::size_t i) { return first.before[1][i] + 0.5f * right[i]; }) == 0);

    // The rest of it, in a block longer than what's left: it ends on its last frame, and after it the output is as it was.
    Out second (600, 2, 7);
    player.process (second.ptrs(), 2, 600, false);
    CHECK (mismatches (second, 0, 0, 488, [&] (std::size_t i) { return second.before[0][i] + 0.5f * left[512 + i]; }) == 0);
    CHECK (mismatches (second, 1, 0, 488, [&] (std::size_t i) { return second.before[1][i] + 0.5f * right[512 + i]; }) == 0);
    CHECK (untouched (second, 488));
    CHECK (player.sounding() == 0u);

    back.collect (player);
    REQUIRE (back.clips.size() == 1);
    CHECK (back.clips[0] == clip);
}

TEST_CASE ("a mono clip feeds both outputs; one output gets (L + R) / 2", "[audition]")
{
    AuditionPlayer player;
    player.prepare (kRate);
    Returns back;
    const auto l = noise (300, 3), r = noise (300, 4);

    SECTION ("a mono clip, two outputs: the same on both")
    {
        back.add (player.offer (clipOf (l, {}, 1)));
        Out out (300);
        player.process (out.ptrs(), 2, 300, false);
        for (std::size_t c = 0; c < 2; ++c)
            CHECK (mismatches (out, c, 0, 300, [&] (std::size_t i) { return out.before[c][i] + 0.5f * l[i]; }) == 0);
    }

    SECTION ("a stereo clip, one output: (L + R) / 2")
    {
        back.add (player.offer (clipOf (l, r, 1)));
        Out out (300, 1);
        player.process (out.ptrs(), 1, 300, false);
        CHECK (mismatches (out, 0, 0, 300, [&] (std::size_t i) { return out.before[0][i] + 0.5f * (0.5f * (l[i] + r[i])); }) == 0);
    }

    SECTION ("a mono clip, one output")
    {
        back.add (player.offer (clipOf (l, {}, 1)));
        Out out (300, 1);
        player.process (out.ptrs(), 1, 300, false);
        CHECK (mismatches (out, 0, 0, 300, [&] (std::size_t i) { return out.before[0][i] + 0.5f * l[i]; }) == 0);
    }

    SECTION ("more than two outputs: the first two, as the engine")
    {
        back.add (player.offer (clipOf (l, r, 1)));
        Out out (300, 3);
        player.process (out.ptrs(), 3, 300, false);
        CHECK (mismatches (out, 0, 0, 300, [&] (std::size_t i) { return out.before[0][i] + 0.5f * l[i]; }) == 0);
        CHECK (mismatches (out, 1, 0, 300, [&] (std::size_t i) { return out.before[1][i] + 0.5f * r[i]; }) == 0);
        CHECK (std::memcmp (out.data[2].data(), out.before[2].data(), 300 * sizeof (float)) == 0);
    }

    back.collect (player); // it ended with the block
    CHECK (back.clips.size() == 1);
    CHECK (player.sounding() == 0u);
}

TEST_CASE ("a new offer replaces the playing clip: 5 ms fade out, the new one at once", "[audition]")
{
    AuditionPlayer player;
    player.prepare (kRate);
    Returns back;
    const std::vector<float> a[2] { noise (4000, 5), noise (4000, 6) };
    const std::vector<float> b[2] { noise (4000, 7), noise (4000, 8) };
    const auto* first = clipOf (a[0], a[1], 1);
    back.add (player.offer (first));
    Out one (512);
    player.process (one.ptrs(), 2, 512, false);
    REQUIRE (player.sounding() == 1u);

    const auto* second = clipOf (b[0], b[1], 2);
    back.add (player.offer (second));
    Out two (512);
    player.process (two.ptrs(), 2, 512, false);
    CHECK (player.sounding() == 2u);
    // The first fades out over 240 samples from where it was (its frame 512); the second starts on its frame 0 at once,
    // at full level: no gap, and no overlap past the fade.
    for (std::size_t c = 0; c < 2; ++c)
    {
        CAPTURE (c);
        CHECK (mismatches (two, c, 0, kFade, [&] (std::size_t i) {
            float e = two.before[c][i];
            e += 0.5f * (a[c][512 + i] * fadeGain (i));
            e += 0.5f * b[c][i];
            return e;
        }) == 0);
        CHECK (mismatches (two, c, kFade, 512 - kFade, [&] (std::size_t i) { return two.before[c][kFade + i] + 0.5f * b[c][kFade + i]; }) == 0);
    }
    back.collect (player);
    CHECK (back.count (first) == 1); // let go once faded out
    CHECK (back.count (second) == 0);

    // The next block: the second alone, on from its frame 512.
    Out three (512);
    player.process (three.ptrs(), 2, 512, false);
    for (std::size_t c = 0; c < 2; ++c)
        CHECK (mismatches (three, c, 0, 512, [&] (std::size_t i) { return three.before[c][i] + 0.5f * b[c][512 + i]; }) == 0);
    CHECK (player.sounding() == 2u);
}

TEST_CASE ("stopUpTo fades a playing clip and lets an offered one go unplayed; a newer ticket still plays", "[audition]")
{
    AuditionPlayer player;
    player.prepare (kRate);
    Returns back;
    const auto a = noise (4000, 9);
    const auto* first = clipOf (a, {}, 1);
    back.add (player.offer (first));
    Out one (512, 1);
    player.process (one.ptrs(), 1, 512, false);
    REQUIRE (player.sounding() == 1u);

    // Stopped: it fades out from the next block's first sample, and nothing counts as sounding.
    player.stopUpTo (1);
    Out two (512, 1);
    player.process (two.ptrs(), 1, 512, false);
    CHECK (player.sounding() == 0u);
    CHECK (mismatches (two, 0, 0, kFade, [&] (std::size_t i) { return two.before[0][i] + 0.5f * (a[512 + i] * fadeGain (i)); }) == 0);
    CHECK (untouched (two, kFade));
    back.collect (player);
    CHECK (back.count (first) == 1);

    // An offer whose ticket is stopped before a block takes it never plays.
    const auto* second = clipOf (noise (4000, 10), {}, 2);
    back.add (player.offer (second));
    player.stopUpTo (2);
    Out three (512, 1);
    player.process (three.ptrs(), 1, 512, false);
    CHECK (untouched (three));
    CHECK_FALSE (player.offered());
    CHECK (player.sounding() == 0u);
    back.collect (player);
    CHECK (back.count (second) == 1);

    // A newer ticket plays, and a stop of older tickets changes nothing.
    const auto c3 = noise (4000, 11);
    const auto* third = clipOf (c3, {}, 3);
    back.add (player.offer (third));
    player.stopUpTo (1);
    Out four (512, 1);
    player.process (four.ptrs(), 1, 512, false);
    CHECK (player.sounding() == 3u);
    CHECK (mismatches (four, 0, 0, 512, [&] (std::size_t i) { return four.before[0][i] + 0.5f * c3[i]; }) == 0);

    // A stop while the clip it replaced still fades out: the playing one fades out too, from that block's first sample.
    const auto c4 = noise (4000, 12);
    const auto* fourth = clipOf (c4, {}, 4);
    back.add (player.offer (fourth));
    Out five (100, 1);
    player.process (five.ptrs(), 1, 100, false); // the third fades (100 of 240 samples), the fourth plays
    REQUIRE (player.sounding() == 4u);
    player.stopUpTo (4);
    Out six (512, 1);
    player.process (six.ptrs(), 1, 512, false);
    CHECK (player.sounding() == 0u);
    CHECK (mismatches (six, 0, 0, kFade, [&] (std::size_t i) {
        float e = six.before[0][i];
        if (i < kFade - 100)
            e += 0.5f * (c3[512 + 100 + i] * fadeGain (100 + i));
        e += 0.5f * (c4[100 + i] * fadeGain (i));
        return e;
    }) == 0);
    CHECK (untouched (six, kFade));
    back.collect (player);
    CHECK (back.count (third) == 1);
    CHECK (back.count (fourth) == 1);
}

TEST_CASE ("silent blocks write nothing and let every clip go", "[audition]")
{
    AuditionPlayer player;
    player.prepare (kRate);
    Returns back;
    const auto* playing = clipOf (noise (9000, 13), noise (9000, 14), 1);
    back.add (player.offer (playing));
    Out one (512);
    player.process (one.ptrs(), 2, 512, false);
    REQUIRE (player.sounding() == 1u);
    const auto* offered = clipOf (noise (9000, 15), {}, 2);
    back.add (player.offer (offered));

    SECTION ("an offline render's block, or a bypassed one")
    {
        Out two (512);
        player.process (two.ptrs(), 2, 512, true);
        CHECK (untouched (two));
    }

    SECTION ("a block with no outputs")
    {
        player.process (nullptr, 0, 512, true);
    }

    SECTION ("the host's reset")
    {
        player.reset();
    }

    CHECK (player.sounding() == 0u);
    CHECK_FALSE (player.offered());
    back.collect (player);
    CHECK (back.count (playing) == 1);
    CHECK (back.count (offered) == 1);

    // Nothing plays on afterwards.
    Out after (512);
    player.process (after.ptrs(), 2, 512, false);
    CHECK (untouched (after));
}

TEST_CASE ("a clip made for another rate never plays", "[audition]")
{
    AuditionPlayer player;
    Returns back;

    SECTION ("an offer before the first prepare, or one made for another rate")
    {
        back.add (player.offer (clipOf (noise (500, 16), {}, 1))); // made for 48 kHz while the player has no rate
        Out zero (512, 1);
        player.process (zero.ptrs(), 1, 512, false);
        CHECK (untouched (zero));

        player.prepare (kRate);
        back.add (player.offer (clipOf (noise (500, 17), {}, 2, 44100.0)));
        Out one (512, 1);
        player.process (one.ptrs(), 1, 512, false);
        CHECK (untouched (one));
        CHECK (player.sounding() == 0u);
        back.collect (player);
        CHECK (back.clips.size() == 2);
    }

    SECTION ("a playing clip, once the player is prepared at another rate")
    {
        player.prepare (kRate);
        const auto* clip = clipOf (noise (9000, 18), {}, 1);
        back.add (player.offer (clip));
        Out one (512, 1);
        player.process (one.ptrs(), 1, 512, false);
        REQUIRE (player.sounding() == 1u);

        player.prepare (44100.0);
        Out two (512, 1);
        player.process (two.ptrs(), 1, 512, false);
        CHECK (untouched (two));
        CHECK (player.sounding() == 0u);
        back.collect (player);
        CHECK (back.count (clip) == 1);
    }
}

TEST_CASE ("every clip comes back exactly once", "[audition]")
{
    SECTION ("from offer, collectRetired and silent blocks, over 3,000 random steps")
    {
        AuditionPlayer player;
        player.prepare (kRate);
        std::vector<const AuditionClip*> made;
        std::map<const AuditionClip*, int> returned;
        const auto back = [&returned] (const AuditionClip* clip) {
            if (clip != nullptr)
                ++returned[clip];
        };
        std::minstd_rand random (17);
        std::vector<float> l (128), r (128);
        float* out[2] { l.data(), r.data() };
        std::uint32_t ticket = 0;
        for (int step = 0; step < 3000; ++step)
        {
            switch (random() % 8)
            {
                case 0:
                case 1:
                {
                    ++ticket;
                    const auto frames = static_cast<std::size_t> (1 + random() % 400);
                    const auto right = random() % 2 == 0 ? std::vector<float> {} : noise (frames, ticket + 5000);
                    made.push_back (clipOf (noise (frames, ticket), right, ticket, random() % 10 == 0 ? 44100.0 : kRate));
                    back (player.offer (made.back()));
                    break;
                }
                case 2: back (player.offer (nullptr)); break; // withdrawn
                case 3: player.stopUpTo (ticket - (ticket > 0 ? static_cast<std::uint32_t> (random() % 2) : 0u)); break;
                case 4: player.collectRetired (back); break;
                case 5: player.reset(); break;
                default: player.process (out, 1 + static_cast<int> (random() % 2), 1 + static_cast<int> (random() % 128), random() % 6 == 0); break;
            }
        }
        // Then everything goes, as at the end of a session: silent blocks, collected as the sound thread would.
        for (int i = 0; i < 4; ++i)
        {
            player.process (nullptr, 0, 64, true);
            player.collectRetired (back);
        }
        back (player.offer (nullptr));

        REQUIRE (made.size() > 100);
        int missing = 0, twice = 0;
        for (const auto* clip : made)
        {
            const auto it = returned.find (clip);
            missing += it == returned.end() ? 1 : 0;
            twice += it != returned.end() && it->second > 1 ? 1 : 0;
        }
        CHECK (missing == 0);
        CHECK (twice == 0);
        CHECK (returned.size() == made.size());
        for (const auto* clip : made) // what came back, once; anything still held is the player's
            if (const auto it = returned.find (clip); it != returned.end() && it->second == 1)
                delete clip;
    }

    SECTION ("the destructor deletes what it still holds: offered, playing, fading out and retired")
    {
        if (! allocations::available())
            SKIP ("a sanitizer owns the allocation functions in this build");
        std::optional<AuditionPlayer> player; // in place, so destroying it frees nothing but its clips
        player.emplace();
        player->prepare (kRate);
        std::vector<float> l (64), r (64);
        float* out[2] { l.data(), r.data() };
        player->offer (clipOf (noise (10, 1), {}, 1)); // ends in its first block: retired
        player->process (out, 2, 64, false);
        player->offer (clipOf (noise (9000, 2), {}, 2)); // plays...
        player->process (out, 2, 64, false);
        player->offer (clipOf (noise (9000, 3), {}, 3)); // ...and is replaced: fading out
        player->process (out, 2, 64, false);
        player->offer (clipOf (noise (9000, 4), {}, 4)); // waits, offered
        REQUIRE (player->offered());
        REQUIRE (player->sounding() == 3u);

        allocations::Scope scope;
        player.reset();
        const auto counts = scope.counts();
        CHECK (counts.frees == 8); // four clips: each the object and its audio
        CHECK (counts.allocations == 0);
    }
}

TEST_CASE ("a full retire FIFO holds clips silently and takes no new one until there's room", "[audition]")
{
    AuditionPlayer player;
    player.prepare (kRate);
    Returns back;
    STATIC_CHECK (AuditionPlayer::kRetireSlots == 8);

    // Seven short clips, each played to its end and let go: the FIFO holds 7 of its 8.
    std::vector<const AuditionClip*> shorts;
    for (std::uint32_t t = 1; t <= 7; ++t)
    {
        shorts.push_back (clipOf (noise (10, t), {}, t));
        back.add (player.offer (shorts.back()));
        Out out (64, 1);
        player.process (out.ptrs(), 1, 64, false);
    }
    // The 8th is replaced by the 9th: the 8th fades out into the last place, then the 9th ends with nowhere to go.
    const auto eighth = noise (2000, 8), ninth = noise (600, 9);
    const auto* c8 = clipOf (eighth, {}, 8);
    back.add (player.offer (c8));
    Out a (64, 1);
    player.process (a.ptrs(), 1, 64, false);
    const auto* c9 = clipOf (ninth, {}, 9);
    back.add (player.offer (c9));
    Out b (512, 1);
    player.process (b.ptrs(), 1, 512, false);
    CHECK (player.sounding() == 9u);
    Out c (512, 1);
    player.process (c.ptrs(), 1, 512, false); // the 9th ends 88 samples in, and is held, silent
    CHECK (mismatches (c, 0, 0, 88, [&] (std::size_t i) { return c.before[0][i] + 0.5f * ninth[512 + i]; }) == 0);
    CHECK (untouched (c, 88));
    CHECK (player.sounding() == 0u);

    // A new offer waits: nothing is taken while the FIFO is full, and the held clip stays silent.
    const auto tenth = noise (3000, 10);
    const auto* c10 = clipOf (tenth, {}, 10);
    back.add (player.offer (c10));
    Out d (512, 1);
    player.process (d.ptrs(), 1, 512, false);
    CHECK (untouched (d));
    CHECK (player.offered());
    CHECK (player.sounding() == 0u);

    // The sound thread empties the FIFO (oldest first); the next block lets the 9th go and plays the 10th at once.
    back.collect (player);
    REQUIRE (back.clips.size() == 8);
    for (std::size_t i = 0; i < 7; ++i)
        CHECK (back.clips[i] == shorts[i]);
    CHECK (back.clips[7] == c8);
    Out e (512, 1);
    player.process (e.ptrs(), 1, 512, false);
    CHECK (player.sounding() == 10u);
    CHECK (mismatches (e, 0, 0, 512, [&] (std::size_t i) { return e.before[0][i] + 0.5f * tenth[i]; }) == 0);
    back.collect (player);
    CHECK (back.count (c9) == 1);
}

TEST_CASE ("an idle player leaves the output untouched, -0.0 included", "[audition]")
{
    AuditionPlayer player;
    Returns back;
    Out out (512);
    for (auto* d : { &out.data[0], &out.data[1] })
    {
        (*d)[1] = std::numeric_limits<float>::quiet_NaN();
        (*d)[2] = 1.0e-40f; // subnormal
        (*d)[3] = -0.0f;
    }
    out.before = out.data;

    SECTION ("never prepared")
    {
        player.process (out.ptrs(), 2, 512, false);
    }

    SECTION ("prepared, nothing offered")
    {
        player.prepare (kRate);
        player.process (out.ptrs(), 2, 512, false);
        player.process (out.ptrs(), 1, 512, false);
    }

    SECTION ("once a clip has ended and gone")
    {
        player.prepare (kRate);
        back.add (player.offer (clipOf (noise (100, 1), {}, 1)));
        Out scratch (512);
        player.process (scratch.ptrs(), 2, 512, false);
        back.collect (player);
        REQUIRE (back.clips.size() == 1);
        player.process (out.ptrs(), 2, 512, false);
    }

    CHECK (untouched (out));
    CHECK (player.sounding() == 0u);
}

TEST_CASE ("process allocates and frees nothing", "[audition][alloc]")
{
    if (! allocations::available())
        SKIP ("a sanitizer owns the allocation functions in this build");

    AuditionPlayer player;
    player.prepare (kRate);
    Returns back;
    std::vector<float> l (512), r (512);
    float* out[2] { l.data(), r.data() };
    const auto quiet = [] (auto&& step) {
        allocations::Scope scope;
        step();
        return scope.counts().none();
    };

    back.add (player.offer (clipOf (noise (2000, 1), noise (2000, 2), 1)));
    CHECK (quiet ([&] { player.process (out, 2, 512, false); })); // taken, plays
    back.add (player.offer (clipOf (noise (300, 3), {}, 2)));
    CHECK (quiet ([&] { player.process (out, 2, 512, false); })); // replaces it: one fades, the other plays and ends
    CHECK (quiet ([&] { player.process (out, 1, 512, false); }));
    back.add (player.offer (clipOf (noise (2000, 4), {}, 3)));
    CHECK (quiet ([&] { player.process (out, 2, 64, false); }));
    CHECK (quiet ([&] {
        player.stopUpTo (3);
        player.process (out, 2, 512, false); // fades out
    }));
    back.add (player.offer (clipOf (noise (2000, 5), {}, 4)));
    CHECK (quiet ([&] { player.process (out, 2, 512, true); })); // silent: let go unplayed
    back.add (player.offer (clipOf (noise (2000, 6), {}, 5)));
    CHECK (quiet ([&] { player.process (out, 2, 64, false); }));
    CHECK (quiet ([&] { player.reset(); }));
    CHECK (quiet ([&] { player.process (out, 2, 512, false); })); // idle
    back.collect (player);
    CHECK (back.clips.size() == 5);
}

TEST_CASE ("makeAuditionClip: at 48 kHz the file itself; at 44.1 and 96 kHz ceil (frames x rate / 48000) frames, a 1 kHz tone stays 1 kHz; deterministic",
    "[audition]")
{
    constexpr std::int64_t frames = 48001; // odd, so the resampled lengths round up
    SampleData file;
    file.frames = frames;
    file.channels = 2;
    file.audio = sine (static_cast<std::size_t> (frames), 1000.0, 0.5);
    const auto right = sine (static_cast<std::size_t> (frames), 1000.0, 0.25, 1.0);
    file.audio.insert (file.audio.end(), right.begin(), right.end());

    SECTION ("48 kHz: the file itself")
    {
        const auto clip = makeAuditionClip (file, kRate, 7);
        REQUIRE (clip != nullptr);
        CHECK (clip->frames == frames);
        CHECK (clip->channels == 2);
        CHECK (same (clip->rate, kRate));
        CHECK (clip->ticket == 7u);
        REQUIRE (clip->audio.size() == file.audio.size());
        CHECK (std::memcmp (clip->audio.data(), file.audio.data(), file.audio.size() * sizeof (float)) == 0);
    }

    SECTION ("44.1 and 96 kHz: through the voices' sinc bank, in tune")
    {
        const double rate = GENERATE (44100.0, 96000.0);
        CAPTURE (rate);
        const auto clip = makeAuditionClip (file, rate, 3);
        REQUIRE (clip != nullptr);
        CHECK (clip->frames == (rate < kRate ? 44101 : 96002)); // ceil (48001 x rate / 48000)
        CHECK (clip->frames == static_cast<std::int64_t> (std::ceil (static_cast<double> (frames) * rate / kRate)));
        CHECK (clip->channels == 2);
        CHECK (same (clip->rate, rate));
        CHECK (clip->ticket == 3u);
        REQUIRE (clip->audio.size() == static_cast<std::size_t> (clip->frames) * 2);

        // Frame n of each channel is sincRead at n x 48000 / rate through the bank for that ratio.
        const double ratio = kRate / rate;
        const auto& bank = SincBank::instance();
        const float* table = bank.table (bank.bankFor (ratio));
        int differ = 0;
        for (std::size_t c = 0; c < 2; ++c)
            for (std::int64_t n = 0; n < clip->frames; ++n)
            {
                const float expected = sincRead (table, file.audio.data() + c * static_cast<std::size_t> (frames), frames, static_cast<double> (n) * ratio);
                if (! same (clip->audio[c * static_cast<std::size_t> (clip->frames) + static_cast<std::size_t> (n)], expected))
                    ++differ;
            }
        CHECK (differ == 0);

        // A 1 kHz tone stays 1 kHz (within a cent) at the output's rate.
        const std::vector<float> left (clip->audio.begin(), clip->audio.begin() + clip->frames);
        const double hz = frequencyOf (left, 200, rate);
        CAPTURE (hz);
        CHECK (std::fabs (1200.0 * std::log2 (hz / 1000.0)) <= 1.0);

        // Deterministic: the same bits every time.
        const auto again = makeAuditionClip (file, rate, 3);
        REQUIRE (again != nullptr);
        CHECK (std::memcmp (again->audio.data(), clip->audio.data(), clip->audio.size() * sizeof (float)) == 0);
    }

    SECTION ("what it refuses: a rate that can't be, a file that isn't 48 kHz mono or stereo")
    {
        CHECK (makeAuditionClip (file, 0.0, 1) == nullptr);
        CHECK (makeAuditionClip (file, -48000.0, 1) == nullptr);
        CHECK (makeAuditionClip (file, std::numeric_limits<double>::infinity(), 1) == nullptr);
        CHECK (makeAuditionClip (file, std::numeric_limits<double>::quiet_NaN(), 1) == nullptr);
        CHECK (makeAuditionClip (file, 2.0 * kMaxAuditionRate, 1) == nullptr);
        CHECK (makeAuditionClip (SampleData {}, kRate, 1) == nullptr);
        auto three = file;
        three.channels = 3;
        three.audio.resize (static_cast<std::size_t> (frames) * 3);
        CHECK (makeAuditionClip (three, kRate, 1) == nullptr);
        auto shortAudio = file;
        shortAudio.audio.pop_back();
        CHECK (makeAuditionClip (shortAudio, kRate, 1) == nullptr);
        auto otherRate = file;
        otherRate.sampleRate = 44100;
        CHECK (makeAuditionClip (otherRate, kRate, 1) == nullptr);
    }
}

TEST_CASE ("a worker, a stopper and the audio thread, 10,000 rounds: every clip accounted for", "[audition][threads]")
{
    // The sound thread's part (offers, withdrawals, collecting), the message thread's (stops) and the audio thread's
    // (blocks of every size, silent ones, resets), all at once. Every clip must come back exactly once.
    constexpr std::uint32_t rounds = 10000;
    AuditionPlayer player;
    player.prepare (kRate);
    std::vector<const AuditionClip*> made;
    made.reserve (rounds);
    std::vector<int> returned (rounds + 1, 0); // by ticket
    std::atomic<std::uint32_t> latest { 0 };
    std::atomic<bool> working { true };
    std::atomic<bool> playing { true };
    std::atomic<int> blocks { 0 };

    std::thread audio ([&] {
        std::vector<float> l (256), r (256);
        float* out[2] { l.data(), r.data() };
        std::minstd_rand random (3);
        while (playing.load (std::memory_order_acquire))
        {
            const auto pick = random() % 32;
            if (pick == 0)
                player.reset();
            else
                player.process (out, 1 + static_cast<int> (pick % 2), 1 + static_cast<int> (random() % 256), pick == 1);
            blocks.fetch_add (1, std::memory_order_relaxed);
        }
    });

    std::thread stopper ([&] {
        std::minstd_rand random (5);
        while (working.load (std::memory_order_acquire))
        {
            const auto t = latest.load (std::memory_order_acquire);
            if (t > 2)
                player.stopUpTo (t - 1 - static_cast<std::uint32_t> (random() % 2)); // older tickets: the newest mostly plays
            std::this_thread::yield();
        }
    });

    const auto back = [&returned] (const AuditionClip* clip) {
        if (clip != nullptr && clip->ticket >= 1 && clip->ticket <= rounds)
            ++returned[clip->ticket];
    };
    std::minstd_rand random (7);
    for (std::uint32_t t = 1; t <= rounds; ++t)
    {
        const auto frames = static_cast<std::size_t> (1 + random() % 300);
        const auto right = random() % 2 == 0 ? std::vector<float> {} : noise (frames, t + rounds);
        made.push_back (clipOf (noise (frames, t), right, t, random() % 16 == 0 ? 44100.0 : kRate));
        latest.store (t, std::memory_order_release);
        back (player.offer (made.back()));
        if (random() % 16 == 0)
            back (player.offer (nullptr)); // withdrawn, as an offer no block took in 300 ms is
        player.collectRetired (back);
        if (random() % 4 == 0)
            std::this_thread::yield();
    }
    working.store (false, std::memory_order_release);
    stopper.join();
    playing.store (false, std::memory_order_release);
    audio.join();

    // Then this thread plays both parts: silent blocks let everything go, and it collects what they let go.
    for (int i = 0; i < 4; ++i)
    {
        player.process (nullptr, 0, 64, true);
        player.collectRetired (back);
    }
    back (player.offer (nullptr));

    CHECK (blocks.load() > 0);
    int missing = 0, twice = 0;
    for (std::uint32_t t = 1; t <= rounds; ++t)
    {
        missing += returned[t] == 0 ? 1 : 0;
        twice += returned[t] > 1 ? 1 : 0;
    }
    CHECK (missing == 0);
    CHECK (twice == 0);
    for (const auto* clip : made) // what came back, once; anything still held is the player's
        if (returned[clip->ticket] == 1)
            delete clip;
}
