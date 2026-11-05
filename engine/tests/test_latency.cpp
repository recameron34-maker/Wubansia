// The mix graph's latency (ADR 0021): constant per rate whatever the kit, equal to the measured impulse delay with
// the club master on and off, and every path aligned inside it: parallel paths, keys, level-duck keys and returns.

#include "mix_harness.h"

#include "kit/manifest.h"
#include "mix/club_master.h"
#include "mix/graph.h"
#include "mix/inserts.h"
#include "mix/mixer.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <functional>
#include <iterator>
#include <string>
#include <vector>

using namespace wubansia::engine;
using namespace mixtest;

namespace
{
    Controls club (bool on)
    {
        Controls c;
        c.club = on;
        return c;
    }

    // The index of the largest |left| sample.
    std::size_t peakAt (const std::vector<float>& stereo)
    {
        std::size_t best = 0;
        for (std::size_t i = 0; i < stereo.size() / 2; ++i)
            if (std::abs (stereo[2 * i]) > std::abs (stereo[2 * best]))
                best = i;
        return best;
    }

    std::function<float (int, std::int64_t, int)> impulse (std::int64_t at, float height, int onlyChannel = -1)
    {
        return [=] (int c, std::int64_t t, int) { return t == at && (onlyChannel < 0 || c == onlyChannel) ? height : 0.0f; };
    }
}

TEST_CASE ("latency: fixed per rate, whatever the kit: 64 + the club master's", "[latency]")
{
    struct Row
    {
        double rate;
        int total;
    };
    for (const Row row : { Row { 44100.0, 404 }, Row { 48000.0, 424 }, Row { 96000.0, 664 } })
    {
        CAPTURE (row.rate);
        Mixer mixer;
        mixer.prepare (row.rate, 256, 2);
        CHECK (mixer.latencySamples() == row.total);
        const auto busy = busyMix();
        mixer.setConfig (&busy);
        CHECK (mixer.latencySamples() == row.total);
        mixer.setConfig (nullptr);
        CHECK (mixer.latencySamples() == row.total);
        CHECK (mixer.tailSeconds() >= 6.0);
    }
    CHECK (mix::ClubMaster::latencyFor (48000.0) == 23 + 48 + 289);
}

TEST_CASE ("latency: the reported latency is the measured impulse delay, the club master on and off", "[latency]")
{
    const double rate = GENERATE (44100.0, 48000.0, 96000.0);
    const bool on = GENERATE (true, false);
    CAPTURE (rate, on);
    const auto cfg = makeMix ({ channel ("kick", Lane::kick, { clip (0.0) }), channel ("pad", Lane::chords) });
    Mixer mixer;
    mixer.prepare (rate, 512, 2);
    mixer.setConfig (&cfg);
    mixer.reset();
    const int latency = mixer.latencySamples();
    const auto out = run (mixer, 4096, 512, 2, impulse (1000, 0.25f, 1), 2, {}, [on] (std::int64_t) { return club (on); });
    CHECK (static_cast<int> (peakAt (out)) == 1000 + latency);
}

TEST_CASE ("latency: an impulse through parallel paths of unequal latency comes out as one impulse", "[latency]")
{
    // a comp under its threshold is a pure 23-sample delay; the plain channel, and the parallel insert's dry path,
    // wait for it
    const auto cfg = makeMix ({ channel ("vox", Lane::vox, { comp (0.0) }), channel ("pad", Lane::chords),
        channel ("acc", Lane::accents, { parallel (0.0, { comp (0.0) }) }) });
    Mixer mixer;
    mixer.prepare (48000.0, 256, 2);
    mixer.setConfig (&cfg);
    const int latency = mixer.latencySamples();
    const auto out = run (mixer, 2048, 256, 2, impulse (500, 0.25f), 3, {}, [] (std::int64_t) { return club (false); });
    // vox 0.25 + pad 0.25 + acc (dry 0.25 + wet 0.25), all at one sample
    for (std::size_t i = 0; i < out.size() / 2; ++i)
    {
        CAPTURE (i);
        const float want = i == static_cast<std::size_t> (500 + latency) ? 1.0f : 0.0f;
        REQUIRE (std::abs (out[2 * i] - want) < 1e-7f);
    }
}

TEST_CASE ("latency: an onset duck's dip lands on the delayed kick within a sample", "[latency]")
{
    // the kick has no insert; the pad clips (27 samples) before its duck, so the key waits 27 at the duck and the
    // kick waits 27 to meet the pad
    const auto cfg = makeMix ({ channel ("kick", Lane::kick),
        channel ("pad", Lane::chords, { clip (0.0), onsetDuck (1u << static_cast<unsigned> (Lane::kick), 20.0, 0.0, 0.002, 0.010) }) });
    auto render = [&] (bool kickAudio, bool padAudio) {
        Mixer mixer;
        mixer.prepare (48000.0, 256, 2);
        mixer.setConfig (&cfg);
        const std::vector<Event> keys { { 3000, Event::Kind::key, static_cast<int> (Lane::kick), 127, 0, 0 } };
        return run (mixer, 6000, 256, 2, [=] (int c, std::int64_t t, int) {
            if (c == 0)
                return kickAudio && t == 3000 ? 0.5f : 0.0f;
            return padAudio ? 0.25f : 0.0f;
        }, 2, keys, [] (std::int64_t) { return club (false); });
    };
    const auto kick = render (true, false);
    const auto pad = render (false, true);
    const auto kickAt = peakAt (kick);
    std::size_t dipAt = 0;
    for (std::size_t i = 2000; i < pad.size() / 2; ++i)
        if (pad[2 * i] < 0.2f)
        {
            dipAt = i;
            break;
        }
    CHECK (kickAt == 3000 + static_cast<std::size_t> (424));
    CHECK (dipAt >= kickAt);
    CHECK (dipAt - kickAt <= 1);
}

TEST_CASE ("latency: a level duck waits for a key channel that is later than it", "[latency]")
{
    // the key (vox) clips: 27 samples; the pad's level duck delays the pad 27 before ducking, so its key and its
    // signal stay together and the pad still meets the vox
    const auto cfg = makeMix ({ channel ("vox", Lane::vox, { clip (0.0) }), channel ("pad", Lane::chords, { levelDuck (0, 6.0, 18.0, 0.0, 0.01, 0.3) }) });
    CHECK (mix::MixGraph::requiredLatency (cfg) == 27);
    Mixer mixer;
    mixer.prepare (48000.0, 256, 2);
    mixer.setConfig (&cfg);
    const auto vox = run (mixer, 4096, 256, 2, impulse (1000, 0.25f, 0), 2, {}, [] (std::int64_t) { return club (false); });
    Mixer other;
    other.prepare (48000.0, 256, 2);
    other.setConfig (&cfg);
    const auto pad = run (other, 4096, 256, 2, impulse (1000, 0.25f, 1), 2, {}, [] (std::int64_t) { return club (false); });
    CHECK (peakAt (vox) == peakAt (pad));
    CHECK (peakAt (pad) == static_cast<std::size_t> (1000 + other.latencySamples()));
}

TEST_CASE ("latency: a return meets the dry paths: the echo's first repeat lands one delay after the dry hit", "[latency]")
{
    // the pad sends to the echo; the vox's comp makes the channels 23 late, so the sends are too
    auto cfg = makeMix ({ channel ("pad", Lane::chords, {}, Group::none, { 0, 1000, 0 }), channel ("vox", Lane::vox, { comp (0.0) }) });
    auto& e = cfg.echo;
    e.steps = 3;
    e.feedbackPm = 0;
    e.lpHz = 20000;
    e.hpHz = 10;
    e.drivePm = 100;
    e.returnMdb = 0;
    Mixer mixer;
    mixer.prepare (48000.0, 256, 2);
    mixer.setConfig (&cfg);
    const int latency = mixer.latencySamples();
    const double bpm = 120.0; // 3 16ths: 0.375 s, 18000 samples
    const auto out = run (mixer, 24000, 256, 2, impulse (1000, 0.5f, 0), 2, {}, [] (std::int64_t) { return club (false); }, bpm);
    CHECK (std::abs (out[2 * static_cast<std::size_t> (1000 + latency)] - 0.5f) < 1e-6f); // the dry hit
    std::size_t repeat = 0;
    for (std::size_t i = static_cast<std::size_t> (1000 + latency + 100); i < out.size() / 2; ++i)
        if (std::abs (out[2 * i]) > std::abs (out[2 * repeat]))
            repeat = i;
    // the wow and flutter move the repeat by under 0.06 % of the delay: within 11 samples of 18000
    CHECK (std::abs (static_cast<double> (repeat) - (1000.0 + latency + 18000.0)) <= 11.0);
}

namespace
{
    std::string readText (const std::string& path)
    {
        std::ifstream in (path, std::ios::binary);
        if (! in)
            return {};
        return { std::istreambuf_iterator<char> (in), std::istreambuf_iterator<char>() };
    }
}

TEST_CASE ("latency: every embedded mix builds in the mixer and fits the 64-sample budget", "[latency][content]")
{
    // P3's content/plugin/<kit>/<KIT>_mix.json against P2's _kit.json, through P4's parser: where the branch has
    // them all (the integration), each must parse, plan within the budget and the slot pool, and play
    int checked = 0;
    for (const std::string kit : { "FUT01", "FUT02", "FUT03", "FUT04" })
    {
        CAPTURE (kit);
        std::string folder = kit;
        for (auto& ch : folder)
            ch = static_cast<char> (std::tolower (static_cast<unsigned char> (ch)));
        const std::string dir = std::string (WUBANSIA_SOURCE_DIR) + "/content/plugin/" + folder + "/";
        const auto kitJson = readText (dir + kit + "_kit.json");
        const auto mixJson = readText (dir + kit + "_mix.json");
        if (kitJson.empty() || mixJson.empty())
            continue;
        const auto km = parseKitManifest (kitJson);
        if (! km.ok() && km.error.find ("isn't built yet") != std::string::npos)
            continue; // P0's parser stub: the integration runs this
        REQUIRE (km.ok());
        const auto mc = parseMixManifest (mixJson, km.value);
        INFO (mc.error);
        REQUIRE (mc.ok());
        const auto& cfg = mc.value;

        const int need = mix::MixGraph::requiredLatency (cfg);
        INFO ("the longest path " << need << " samples");
        CHECK (need <= mix::kMixLatency);
        for (const double rate : { 44100.0, 48000.0, 96000.0 })
        {
            CAPTURE (rate);
            mix::MixGraph graph;
            graph.prepare (rate, 256);
            Controls controls;
            graph.setConfig (&cfg, controls);
            CHECK (! graph.overBudget());
            CHECK (! graph.overCapacity());
            CHECK (graph.pathLatency() == need);
            MixBus bus;
            SendBus sends;
            bus.prepare (kMaxMixChannels, 256);
            sends.prepare (256);
            std::vector<float> l (256), r (256);
            bool finite = true;
            float peak = 0.0f;
            for (int block = 0; block < 200; ++block)
            {
                bus.clear (256);
                sends.clear (256);
                for (int c = 0; c < static_cast<int> (cfg.channels.size()); ++c)
                    for (int i = 0; i < 256; ++i)
                        bus.left (c)[i] = bus.right (c)[i] = 0.2f * static_cast<float> (std::sin (0.01 * (block * 256 + i) * (c + 1)));
                if (block % 20 == 0)
                    for (std::uint8_t lane = 0; lane < kLanes; ++lane)
                        graph.keyHit (lane, 120, 0);
                controls.variant = block >= 100 ? Variant::novox : Variant::vox;
                graph.process (bus, sends, l.data(), r.data(), 256, controls, 134.0);
                for (int i = 0; i < 256; ++i)
                {
                    finite = finite && std::isfinite (l[static_cast<std::size_t> (i)]) && std::isfinite (r[static_cast<std::size_t> (i)]);
                    peak = std::max (peak, std::abs (l[static_cast<std::size_t> (i)]));
                }
            }
            CHECK (finite);
            CHECK (peak > 0.01f);
            CHECK (peak < 1.0f); // the club master's ceiling
        }
        ++checked;
    }
    if (checked == 0)
        WARN ("no kit's manifests and parser on this branch: checked in the integration");
}
