// The Phrases drawer's audition (ADR 0031): BlockLoop's phrase overlay on recording voices, and the Engine's
// hand-off. Tests 1-10 of the phrases plan and the brief's 11-15. A clip's notes play on the synthetic kit's vox
// (channel 5), the scheduler fixtures' lead-like lane.

#include "alloc_counter.h"
#include "audition/phrase_clip.h"
#include "engine.h"
#include "kit/manifest.h"
#include "mix/mix_config.h"
#include "sched_fixtures.h"
#include "testing/fake_host.h"
#include "testing/synthetic_kit.h"
#include "util/file_io.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

using namespace p7;
using wubansia::engine::testing::FakeHost;
using wubansia::engine::testing::makeSyntheticKit;
using wubansia::engine::testing::syntheticKitManifest;
using wubansia::engine::testing::syntheticMixConfig;
using Item = RecordingVoices::Item;

namespace
{
    constexpr std::int64_t kBarTicks = 3840;
    constexpr double kRate = 48000.0;
    constexpr double kQuarter = 24000.0; // samples at 120 BPM and 48 kHz

    // 8 bars on the vox: a note on every bar line (60 + bar) and one on beat 3 (72), as a phrase file writes them.
    std::vector<std::uint8_t> phraseFile (int channel = 5)
    {
        std::vector<midi::Note> notes;
        for (int bar = 0; bar < 8; ++bar)
        {
            notes.push_back (note (bar * kBarTicks, 480, channel, 60 + bar, 100));
            notes.push_back (note (bar * kBarTicks + 1920, 240, channel, 72, 90));
        }
        return smf ({ track ("Lead", notes) }, 8 * kBarTicks);
    }

    std::unique_ptr<PhraseClip> clipOf (std::uint32_t ticket, std::int64_t alignTicks = kBarTicks, int kitIndex = 0)
    {
        auto clip = makePhraseClip (phraseFile(), synManifest(), cleanMix(), kitIndex, alignTicks, ticket);
        REQUIRE (clip != nullptr);
        return clip;
    }

    struct Harness
    {
        BlockLoop<RecordingVoices> loop;
        PendingSlot<Kit> pending;
        RetireFifo<Kit, 16> retired;
        Telemetry telemetry;
        PhraseHandoff phrases;

        Harness() { loop.setPhrases (&phrases); }
        void prepare (double rate, int block, int outputs) { loop.prepare (rate, block, outputs); }
        void process (float* const* out, int numOutputs, int n, const TransportInfo& transport, const Controls& controls,
            const MidiEvent* midi, int numMidi) noexcept
        {
            loop.process (out, numOutputs, n, transport, controls, midi, numMidi, pending, retired, telemetry);
        }
    };

    Controls quiet()
    {
        Controls c;
        c.club = false;
        return c;
    }

    // What came back through the hand-off, by clip.
    struct Returns
    {
        std::map<const PhraseClip*, int> times;
        void add (const PhraseClip* c)
        {
            if (c != nullptr)
                ++times[c];
        }
        void collect (PhraseHandoff& h)
        {
            h.collectRetired ([this] (const PhraseClip* c) { add (c); });
        }
    };

    std::vector<Item> auditionOns (const std::vector<Item>& log)
    {
        std::vector<Item> out;
        for (const auto& i : log)
            if (i.kind == Item::Kind::on && i.note.source == Source::audition)
                out.push_back (i);
        return out;
    }

    std::int64_t sampleOfQuarter (double q) { return static_cast<std::int64_t> (q * kQuarter); }

    // Renders until the host reaches quarter q (to the block that crosses it).
    void renderTo (Harness& h, FakeHost& host, double q, FakeHost::Output& out, const Controls& c)
    {
        const std::int64_t target = sampleOfQuarter (q);
        if (host.samplePosition() < target)
            host.render (h, c, target - host.samplePosition(), out);
    }
}

TEST_CASE ("phrase overlay 1: bar 1 lands on the next chord line", "[phrase-overlay]")
{
    const Kit kit = synKit (0, true);
    const Controls c = quiet();
    FakeHost::Output out;

    SECTION ("playing: offered at 5.3 quarters with a chord line of 8, its first note sounds on the sample of 8.0")
    {
        Harness h;
        FakeHost host (kRate, 512);
        host.prepare (h);
        h.pending.publish (&kit);
        host.setTempo (120.0);
        host.play (0.0);
        renderTo (h, host, 5.3, out, c);
        auto clip = clipOf (1, 2 * kBarTicks);
        REQUIRE (h.phrases.offer (clip.get()) == nullptr);
        host.render (h, c, 512, out); // taken: it waits for its line, at 8.0
        CHECK (h.telemetry.phraseTicks.load() < 0);
        CHECK (h.telemetry.phraseTicks.load() > -7680 + 5 * 960);
        renderTo (h, host, 20.0, out, c);
        CHECK (h.telemetry.phraseTicks.load() >= 12 * 960); // 12 quarters past its bar 1, give or take the last block
        CHECK (h.telemetry.phraseTicks.load() < 12 * 960 + 512);
        const auto ons = auditionOns (h.loop.voices().log);
        REQUIRE (!ons.empty());
        CHECK (ons.front().sample == sampleOfQuarter (8.0));
        CHECK (ons.front().note.key == 60);
        CHECK (h.telemetry.phraseSounding.load() == 1u);
        CHECK (h.telemetry.phraseStartTicks.load() == 7680);
    }

    SECTION ("stopped: at ppq 0 when play starts")
    {
        Harness h;
        FakeHost host (kRate, 512);
        host.prepare (h);
        h.pending.publish (&kit);
        host.setTempo (120.0);
        host.render (h, c, 512, out);
        auto clip = clipOf (1, 2 * kBarTicks);
        h.phrases.offer (clip.get());
        host.render (h, c, 1024, out); // taken while stopped: nothing sounds
        CHECK (auditionOns (h.loop.voices().log).empty());
        CHECK (h.telemetry.phraseSounding.load() == 1u);
        const auto start = host.samplePosition();
        host.play (0.0);
        host.render (h, c, 48000, out);
        const auto ons = auditionOns (h.loop.voices().log);
        REQUIRE (!ons.empty());
        CHECK (ons.front().sample == start);
        CHECK (ons.front().note.key == 60);
    }
}

TEST_CASE ("phrase overlay 2: it loops its bars with the host, through tempo changes", "[phrase-overlay]")
{
    const Kit kit = synKit (0, true);
    const Controls c = quiet();
    auto clip = clipOf (1);

    // The overlay: offered while stopped at 0, then played from 0 for 3 passes of its 8 bars.
    Harness h;
    FakeHost host (kRate, 512);
    host.prepare (h);
    h.pending.publish (&kit);
    host.setTempo (120.0);
    FakeHost::Output out;
    host.render (h, c, 512, out);
    h.phrases.offer (clip.get());
    host.render (h, c, 512, out);
    // The same notes in a timeline of their own, under the same transport.
    SchedulerHarness s;
    s.timeline = &clip->timeline;
    FakeHost ref (kRate, 512);
    ref.prepare (s);
    ref.setTempo (120.0);
    FakeHost::Output refOut;
    ref.render (s, c, 1024, refOut);

    for (FakeHost* x : { &host, &ref })
        x->play (0.0);
    host.render (h, c, 40 * 24000, out);
    ref.render (s, c, 40 * 24000, refOut);
    for (FakeHost* x : { &host, &ref })
        x->setTempo (131.5);
    host.render (h, c, 1500000, out);
    ref.render (s, c, 1500000, refOut);
    for (FakeHost* x : { &host, &ref })
        x->setTempo (97.25);
    host.render (h, c, 1000000, out);
    ref.render (s, c, 1000000, refOut);
    REQUIRE (host.ppq() > 96.0);

    std::vector<std::pair<std::int64_t, int>> got, expected;
    for (const auto& i : auditionOns (h.loop.voices().log))
        got.emplace_back (i.sample, i.note.key);
    for (const auto& e : s.sink.only (Event::Kind::on))
        expected.emplace_back (e.sample, e.key);
    REQUIRE (expected.size() >= 48);
    CHECK (got == expected);
}

TEST_CASE ("phrase overlay 3: its lanes sit out of the timeline while it sounds", "[phrase-overlay]")
{
    // The loop's vox note starts at quarter 8 of every 16.
    const Kit kit = synKit (0, true);
    const Controls c = quiet();
    Harness h;
    FakeHost host (kRate, 512);
    host.prepare (h);
    h.pending.publish (&kit);
    host.setTempo (120.0);
    host.play (0.0);
    FakeHost::Output out;
    auto clip = clipOf (1);
    CHECK (clip->lanes == laneBit (Lane::vox));
    renderTo (h, host, 2.0, out, c);
    h.phrases.offer (clip.get());
    const auto adopted = host.samplePosition();
    renderTo (h, host, 36.0, out, c); // the vox's onsets at 8 and 24 sit out (each pre-rolled 0.1 quarter early)
    h.phrases.stopUpTo (1);
    const auto stopped = host.samplePosition();
    renderTo (h, host, 60.0, out, c); // and at 40 and 56 play again
    std::vector<std::int64_t> vox;
    for (const auto& i : h.loop.voices().log)
        if (i.kind == Item::Kind::on && i.note.source == Source::sequencer && i.note.instrument == inst::vox)
            vox.push_back (i.sample);
    for (const auto at : vox)
        CHECK ((at < adopted || at >= stopped));
    CHECK (std::count_if (vox.begin(), vox.end(), [&] (std::int64_t at) { return at >= stopped; }) == 2);
    CHECK (!auditionOns (h.loop.voices().log).empty());
}

TEST_CASE ("phrase overlay 4: Stop fades only its voices, over 10 ms", "[phrase-overlay]")
{
    const Kit kit = synKit (0, true);
    const Controls c = quiet();
    Harness h;
    FakeHost host (kRate, 512);
    host.prepare (h);
    h.pending.publish (&kit);
    host.setTempo (120.0);
    host.play (0.0);
    FakeHost::Output out;
    auto clip = clipOf (3);
    h.phrases.offer (clip.get());
    renderTo (h, host, 6.0, out, c);
    const auto before = h.loop.voices().log.size();
    h.phrases.stopUpTo (3);
    host.render (h, c, 512, out);
    std::vector<Item> fades;
    for (std::size_t i = before; i < h.loop.voices().log.size(); ++i)
        if (h.loop.voices().log[i].kind == Item::Kind::fadeOut || h.loop.voices().log[i].kind == Item::Kind::releaseAll)
            fades.push_back (h.loop.voices().log[i]);
    REQUIRE (fades.size() == 1);
    CHECK (fades[0].kind == Item::Kind::fadeOut);
    CHECK (fades[0].source == Source::audition);
    CHECK (fades[0].fade == 480);
    CHECK (h.telemetry.phraseSounding.load() == 0u);
    CHECK (h.telemetry.phraseStartTicks.load() == -1);
    const auto ons = auditionOns (h.loop.voices().log);
    renderTo (h, host, 20.0, out, c);
    CHECK (auditionOns (h.loop.voices().log).size() == ons.size()); // nothing more of it
    Returns back;
    back.collect (h.phrases);
    CHECK (back.times[clip.get()] == 1);
}

TEST_CASE ("phrase overlay 5: a jump or a host loop chases it; a loop back before its line re-anchors it", "[phrase-overlay]")
{
    const Kit kit = synKit (0, true);
    const Controls c = quiet();
    FakeHost::Output out;

    SECTION ("a jump: the next note is the one the phrase plays there")
    {
        Harness h;
        FakeHost host (kRate, 512);
        host.prepare (h);
        h.pending.publish (&kit);
        host.setTempo (120.0);
        host.render (h, c, 512, out);
        auto clip = clipOf (1);
        h.phrases.offer (clip.get());
        host.render (h, c, 512, out); // anchored at 0
        host.play (0.0);
        renderTo (h, host, 3.0, out, c);
        host.seek (10.25);
        const auto at = host.samplePosition();
        const auto count = auditionOns (h.loop.voices().log).size();
        host.render (h, c, 2 * 24000, out);
        const auto ons = auditionOns (h.loop.voices().log);
        REQUIRE (ons.size() > count);
        CHECK (ons[count].note.key == 63); // bar 4 of the phrase: quarter 12, pre-rolled as its timeline places it
        const auto it = std::find_if (clip->timeline.notes.begin(), clip->timeline.notes.end(), [] (const Note& n) { return n.key == 63; });
        REQUIRE (it != clip->timeline.notes.end());
        const double q = static_cast<double> (it->pos) / static_cast<double> (kMilliticksPerQuarter);
        CHECK (ons[count].sample == at + static_cast<std::int64_t> (std::floor ((q - 10.25) * kQuarter + 0.5)));
    }

    SECTION ("a loop back before its chord line re-anchors it at the line at or before the loop's start")
    {
        Harness h;
        FakeHost host (kRate, 512);
        host.prepare (h);
        h.pending.publish (&kit);
        host.setTempo (120.0);
        host.setLoop (12.0, 24.0);
        host.play (0.0);
        renderTo (h, host, 17.5, out, c);
        auto clip = clipOf (1);
        h.phrases.offer (clip.get()); // anchored at 20
        renderTo (h, host, 23.0, out, c);
        CHECK (h.telemetry.phraseStartTicks.load() == 20 * 960);
        const auto wrap = sampleOfQuarter (24.0);
        while (host.samplePosition() < wrap + 24000)
            host.render (h, c, 512, out);
        CHECK (h.telemetry.phraseStartTicks.load() == 12 * 960);
        const auto ons = auditionOns (h.loop.voices().log);
        const auto it = std::find_if (ons.begin(), ons.end(), [&] (const Item& i) { return i.sample >= wrap; });
        REQUIRE (it != ons.end());
        CHECK (it->note.key == 60);
        CHECK (it->sample < wrap + 512); // re-anchored at the next block's start, and chased there
    }
}

TEST_CASE ("phrase overlay 6: the same samples at every block size", "[phrase-overlay]")
{
    const Kit kit = synKit (0, true);
    const Controls c = quiet();
    std::vector<std::pair<std::int64_t, int>> first;
    for (const int block : { 1, 32, 64, 441, 512, 1024 })
    {
        CAPTURE (block);
        Harness h;
        FakeHost host (kRate, block);
        host.prepare (h);
        h.pending.publish (&kit);
        host.setTempo (120.0);
        FakeHost::Output out;
        host.render (h, c, 2048, out);
        auto clip = clipOf (1);
        h.phrases.offer (clip.get());
        host.render (h, c, 2048, out);
        host.play (0.0);
        host.render (h, c, 6 * 24000, out);
        host.setTempo (133.0);
        host.render (h, c, 6 * 24000, out);
        std::vector<std::pair<std::int64_t, int>> got;
        for (const auto& i : auditionOns (h.loop.voices().log))
            got.emplace_back (i.sample, i.note.key);
        REQUIRE (got.size() >= 6);
        if (first.empty())
            first = got;
        CHECK (got == first);
    }
}

TEST_CASE ("phrase overlay 7: no allocation, lock or free in process", "[phrase-overlay][alloc]")
{
    if (! allocations::available())
        SKIP ("a sanitizer owns the allocation functions in this build");
    const Kit kit = synKit (0, true);
    const Controls c = quiet();
    Harness h;
    h.loop.voices().record = false;
    FakeHost host (kRate, 512);
    host.prepare (h);
    h.pending.publish (&kit);
    host.setTempo (120.0);
    FakeHost::Output out;
    out.left.reserve (1 << 21);
    out.right.reserve (1 << 21);
    auto a = clipOf (1), b = clipOf (2), d = clipOf (3);
    const auto quietly = [] (auto&& step) {
        allocations::Scope scope;
        step();
        return scope.counts().none();
    };
    host.render (h, c, 512, out);
    h.phrases.offer (a.get());
    CHECK (quietly ([&] { host.render (h, c, 512, out); })); // taken while stopped
    host.play (0.0);
    CHECK (quietly ([&] { host.render (h, c, 96000, out); })); // plays
    h.phrases.offer (b.get());
    CHECK (quietly ([&] { host.render (h, c, 48000, out); })); // replaces it
    h.phrases.stopUpTo (2);
    CHECK (quietly ([&] { host.render (h, c, 4800, out); })); // stops
    h.phrases.offer (d.get());
    CHECK (quietly ([&] { host.render (h, c, 48000, out); }));
    CHECK (quietly ([&] { h.loop.dropPhraseOffer(); }));
    CHECK (quietly ([&] { h.loop.reset(); }));
    CHECK (quietly ([&] { host.render (h, c, 4800, out); }));
}

TEST_CASE ("phrase overlay 8: every clip comes back once", "[phrase-overlay]")
{
    const Kit kit0 = synKit (0, true), kit1 = synKit (1, true);
    const Controls c = quiet();
    Returns back;
    std::vector<std::unique_ptr<PhraseClip>> made;
    const auto make = [&] (std::uint32_t ticket) {
        made.push_back (clipOf (ticket));
        return made.back().get();
    };

    {
        Harness h;
        FakeHost host (kRate, 512);
        host.prepare (h);
        h.pending.publish (&kit0);
        host.setTempo (120.0);
        host.play (0.0);
        FakeHost::Output out;
        host.render (h, c, 512, out);
        back.add (h.phrases.offer (make (1)));
        back.add (h.phrases.offer (make (2))); // the first never taken: it comes back from offer
        host.render (h, c, 24000, out);
        back.add (h.phrases.offer (make (3))); // replaces 2
        host.render (h, c, 24000, out);
        h.phrases.stopUpTo (3); // stops 3
        host.render (h, c, 24000, out);
        back.add (h.phrases.offer (make (4)));
        host.render (h, c, 24000, out);
        h.pending.publish (&kit1); // a kit switch lets 4 go
        host.render (h, c, 4 * 24000, out);
        CHECK (h.telemetry.kitIndex.load() == 1);
        CHECK (h.telemetry.phraseSounding.load() == 0u);
        back.add (h.phrases.offer (make (5))); // made for kit 0: never plays
        host.render (h, c, 24000, out);
        back.collect (h.phrases);
    }
    for (std::size_t i = 0; i < made.size(); ++i)
    {
        CAPTURE (i);
        CHECK (back.times[made[i].get()] == 1);
    }

    SECTION ("the engine's destruction: the clips it still holds are the phrase thread's to delete")
    {
        auto kit = makeSyntheticKit();
        PhraseClip* playing = nullptr;
        PhraseClip* offered = nullptr;
        {
            Engine engine;
            engine.prepare (kRate, 512, 2);
            engine.publish (kit.get());
            const auto m = syntheticKitManifest();
            std::vector<midi::Note> notes;
            for (int bar = 0; bar < 8; ++bar)
                notes.push_back (note (bar * kBarTicks, 480, 2, 72, 100));
            const auto bytes = smf ({ track ("Lead", notes) }, 8 * kBarTicks);
            playing = makePhraseClip (bytes, m, syntheticMixConfig(), kit->info.index, kBarTicks, 1).release();
            offered = makePhraseClip (bytes, m, syntheticMixConfig(), kit->info.index, kBarTicks, 2).release();
            REQUIRE ((playing != nullptr && offered != nullptr));
            CHECK (playing->lanes == laneBit (Lane::lead));
            std::vector<float> l (512), r (512);
            float* outs[2] { l.data(), r.data() };
            TransportInfo t;
            t.playing = true;
            engine.process (outs, 2, 512, t, Controls {}, nullptr, 0);
            CHECK (engine.offerPhrase (playing) == nullptr);
            engine.process (outs, 2, 512, t, Controls {}, nullptr, 0);
            CHECK (engine.telemetry().phraseSounding.load() == 1u);
            CHECK (engine.offerPhrase (offered) == nullptr);
            int retired = 0;
            engine.collectRetiredPhrases ([&] (const PhraseClip*) { ++retired; });
            CHECK (retired == 0);
        }
        delete playing; // the Engine let neither go: no double free here
        delete offered;
    }
}

TEST_CASE ("phrase overlay 9: offline renders and another kit's clips play nothing", "[phrase-overlay]")
{
    const Kit kit = synKit (0, true);
    const Controls c = quiet();
    const auto render = [&] (bool offer, bool offline, int clipKit) {
        Harness h;
        FakeHost host (kRate, 512);
        host.prepare (h);
        host.setNonRealtime (offline);
        h.pending.publish (&kit);
        host.setTempo (120.0);
        host.play (0.0);
        FakeHost::Output out;
        host.render (h, c, 512, out);
        auto clip = clipOf (1, kBarTicks, clipKit);
        if (offer)
            h.phrases.offer (clip.get());
        host.render (h, c, 10 * 24000, out);
        Returns back;
        back.collect (h.phrases);
        CHECK (back.times[clip.get()] == (offer ? 1 : 0));
        CHECK (auditionOns (h.loop.voices().log).empty());
        return out;
    };
    SECTION ("offline: bit for bit the render without a clip")
    {
        const auto with = render (true, true, 0);
        const auto without = render (false, true, 0);
        CHECK (with.left == without.left);
        CHECK (with.right == without.right);
    }
    SECTION ("another kit's clip")
    {
        const auto with = render (true, false, 7);
        const auto without = render (false, false, 7);
        CHECK (with.left == without.left);
    }
}

TEST_CASE ("phrase overlay 10: makePhraseClip has no phrase slices", "[phrase-overlay]")
{
    const std::string dir = std::string (WUBANSIA_SOURCE_DIR) + "/content/plugin/fut01/";
    const auto kitJson = wubansia::util::readBinaryFile (dir + "FUT01_kit.json");
    const auto mixJson = wubansia::util::readBinaryFile (dir + "FUT01_mix.json");
    REQUIRE ((kitJson.ok() && mixJson.ok()));
    const auto m = parseKitManifest (std::string_view (reinterpret_cast<const char*> (kitJson.value.data()), kitJson.value.size()));
    REQUIRE (m.ok());
    const auto mix = parseMixManifest (std::string_view (reinterpret_cast<const char*> (mixJson.value.data()), mixJson.value.size()), m.value);
    REQUIRE ((m.ok() && mix.ok()));
    const auto smf = wubansia::util::readBinaryFile (std::string (WUBANSIA_SOURCE_DIR) + "/content/midi/fut01/FUT01_Lead_ChromePluck_2bars.mid");
    REQUIRE (smf.ok());

    TimelineOptions with;
    with.loopTicks = 2 * kBarTicks;
    const auto phrased = buildTimeline (smf.value, m.value, mix.value, with);
    REQUIRE (phrased.ok());
    const bool anyPhrase = std::any_of (phrased.value.notes.begin(), phrased.value.notes.end(), [] (const Note& n) { return n.phrase >= 0; });
    CHECK (anyPhrase); // with phrases on, the pluck's cycles match its stem

    const auto clip = makePhraseClip (smf.value, m.value, mix.value, 3, 2 * kBarTicks, 9);
    REQUIRE (clip != nullptr);
    CHECK (clip->timeline.length == 2 * kBarTicks * kMilliticksPerTick);
    CHECK (clip->alignH == 2 * kBarTicks * kMilliticksPerTick);
    CHECK (clip->kitIndex == 3);
    CHECK (clip->ticket == 9u);
    CHECK (!clip->timeline.notes.empty());
    for (const auto& n : clip->timeline.notes)
        CHECK ((n.phrase == -1 && n.slice < 0));
    CHECK (clip->timeline.sliceThrough.empty());
    CHECK (clip->timeline.notes.size() == phrased.value.notes.size() - static_cast<std::size_t> (std::count_if (
                                              phrased.value.notes.begin(), phrased.value.notes.end(), [] (const Note& n) { return n.slice >= 0; })));
    CHECK (makePhraseClip (smf.value, m.value, mix.value, 3, 0, 9) == nullptr);
}

TEST_CASE ("phrase overlay 11: under the internal clock, a clip offered at 5.3 quarters sounds on the sample of 8.0",
    "[phrase-overlay]")
{
    const Kit kit = synKit (0, true);
    const Controls c = quiet();
    Harness h;
    FakeHost host (kRate, 512);
    host.setHasPosition (false); // the Standalone and Preview: the scheduler runs its own clock
    host.prepare (h);
    h.pending.publish (&kit);
    host.setTempo (120.0);
    FakeHost::Output out;
    host.render (h, c, 512, out);
    host.play (0.0);
    const auto start = host.samplePosition();
    while (host.samplePosition() - start < sampleOfQuarter (5.3))
        host.render (h, c, 512, out);
    auto clip = clipOf (1, 2 * kBarTicks);
    h.phrases.offer (clip.get());
    host.render (h, c, 10 * 24000, out);
    const auto ons = auditionOns (h.loop.voices().log);
    REQUIRE (!ons.empty());
    CHECK (ons.front().sample == start + sampleOfQuarter (8.0));
}

TEST_CASE ("phrase overlay 12: Preview starting in the block a clip is adopted starts both at 0", "[phrase-overlay]")
{
    const Kit kit = synKit (0, true);
    const Controls c = quiet();
    Harness h;
    FakeHost host (kRate, 512);
    host.setHasPosition (false);
    host.prepare (h);
    h.pending.publish (&kit);
    host.setTempo (120.0);
    FakeHost::Output out;
    host.render (h, c, 512, out);
    auto clip = clipOf (1, 2 * kBarTicks);
    h.phrases.offer (clip.get());
    host.play (0.0); // the same block
    const auto start = host.samplePosition();
    host.render (h, c, 24000, out);
    const auto ons = auditionOns (h.loop.voices().log);
    REQUIRE (!ons.empty());
    CHECK (ons.front().sample == start);
    CHECK (h.telemetry.phraseStartTicks.load() == 0);
    bool kick = false;
    for (const auto& i : h.loop.voices().log)
        kick = kick || (i.kind == Item::Kind::on && i.note.source == Source::sequencer && i.sample == start);
    CHECK (kick);
}

TEST_CASE ("phrase overlay 13: a bypassed block drops the offer", "[phrase-overlay]")
{
    const Kit kit = synKit (0, true);
    const Controls c = quiet();
    Harness h;
    FakeHost host (kRate, 512);
    host.prepare (h);
    h.pending.publish (&kit);
    host.setTempo (120.0);
    host.play (0.0);
    FakeHost::Output out;
    host.render (h, c, 512, out);
    auto clip = clipOf (1);
    h.phrases.offer (clip.get());
    h.loop.dropPhraseOffer(); // processBlockBypassed
    CHECK (!h.phrases.pending.pending());
    Returns back;
    back.collect (h.phrases);
    CHECK (back.times[clip.get()] == 1);
    host.render (h, c, 48000, out);
    CHECK (auditionOns (h.loop.voices().log).empty());

    SECTION ("the Engine's, too")
    {
        Engine engine;
        engine.prepare (kRate, 512, 2);
        auto other = clipOf (2);
        engine.offerPhrase (other.get());
        engine.dropPhraseOffer();
        int retired = 0;
        engine.collectRetiredPhrases ([&] (const PhraseClip* p) { retired += p == other.get() ? 1 : 0; });
        CHECK (retired == 1);
    }
}

TEST_CASE ("phrase overlay 14: reset lets the clip go and resets the overlay", "[phrase-overlay]")
{
    const Kit kit = synKit (0, true);
    const Controls c = quiet();
    Harness h;
    FakeHost host (kRate, 512);
    host.prepare (h);
    h.pending.publish (&kit);
    host.setTempo (120.0);
    host.play (0.0);
    FakeHost::Output out;
    auto clip = clipOf (1);
    h.phrases.offer (clip.get());
    host.render (h, c, 120000, out); // its line is at quarter 4
    REQUIRE (!auditionOns (h.loop.voices().log).empty());
    REQUIRE (h.loop.overlay().playing());
    h.loop.reset();
    CHECK (!h.loop.overlay().playing());
    CHECK (h.loop.overlay().sounding() == 0);
    const auto count = auditionOns (h.loop.voices().log).size();
    host.render (h, c, 96000, out);
    CHECK (auditionOns (h.loop.voices().log).size() == count);
    CHECK (h.telemetry.phraseSounding.load() == 0u);
    Returns back;
    back.collect (h.phrases);
    CHECK (back.times[clip.get()] == 1);
}

TEST_CASE ("phrase overlay 15: a held clip is given up once no block has run since the service's mark", "[phrase-overlay]")
{
    const Kit kit = synKit (0, true);
    const Controls c = quiet();
    Harness h;
    FakeHost host (kRate, 512);
    host.prepare (h);
    h.pending.publish (&kit);
    host.setTempo (120.0);
    host.play (0.0);
    FakeHost::Output out;
    auto clip = clipOf (1);
    h.phrases.offer (clip.get());
    host.render (h, c, 24000, out);
    REQUIRE (h.telemetry.phraseSounding.load() == 1u);

    SECTION ("a block ran since the mark: it plays on")
    {
        const auto seen = h.phrases.blocksRun();
        host.render (h, c, 512, out);
        h.phrases.dropIfIdleSince (seen);
        host.render (h, c, 512, out);
        CHECK (h.telemetry.phraseSounding.load() == 1u);
    }
    SECTION ("no block since the mark: the next block lets it go, unplayed")
    {
        const auto seen = h.phrases.blocksRun();
        h.phrases.dropIfIdleSince (seen);
        const auto count = auditionOns (h.loop.voices().log).size();
        host.render (h, c, 48000, out);
        CHECK (h.telemetry.phraseSounding.load() == 0u);
        CHECK (auditionOns (h.loop.voices().log).size() == count);
        Returns back;
        back.collect (h.phrases);
        CHECK (back.times[clip.get()] == 1);
    }
}
