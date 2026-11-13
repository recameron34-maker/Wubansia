// The Samples drawer's auditions through the real processor (ADR 0024), on the synthetic kit with the host stopped and
// the player in Off mode, so the engine plays nothing and what comes out is the audition alone: the file dry at -6 dB,
// sample for sample, then master_gain; at 44.1 kHz its resampled length; a mono output's fold; Stop's 5 ms fade, the
// host's reset and bypass; offline renders that never print one; a host that runs no blocks, before or after a block
// took the clip (the 300 ms hint, and nothing stale when blocks come back); the groove left as it was; latest wins;
// selectKit stopping it; bytes that aren't kit.json's refused; the sound thread started by the first audition only;
// and the message, sound and audio threads at once (run it under TSan too: WubansiaPluginTests "[audition]" in a
// -fsanitize=thread build).

#include "helpers/test_helpers.h"

#include "audition/audition_clip.h"
#include "testing/synthetic_kit.h"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <cstring>
#include <memory>
#include <string_view>
#include <thread>

using namespace wubansia;
using test::SyntheticContent;
using test::SyntheticPlugin;
using namespace std::chrono_literals;

namespace
{
    constexpr int kBlock = 512;
    constexpr int kFade = 240; // 5 ms at 48 kHz

    // The synthetic kit's index of the file at path.
    int synFile (std::string_view path)
    {
        const auto& files = engine::testing::syntheticContent().kit.files;
        for (std::size_t i = 0; i < files.size(); ++i)
            if (files[i].path == path)
                return (int) i;
        return -1;
    }

    const int riser = synFile ("FX/Riser.wav"); // 2 channels, 90000 frames
    const int phrase = synFile ("Loops/Lead Phrase.wav"); // 2 channels, 180000 frames

    // What the sound thread makes of a synthetic file for rate: the clip the processor must play.
    std::unique_ptr<engine::AuditionClip> clipFor (int file, double rate)
    {
        return engine::makeAuditionClip (engine::testing::syntheticContent().audio[(std::size_t) file], rate, 1);
    }

    // A clip's channel ch.
    const float* channel (const engine::AuditionClip& clip, int ch) { return clip.audio.data() + (std::size_t) ch * (std::size_t) clip.frames; }

    SyntheticContent::Options withKit (const char* kit, SyntheticContent::Options options = {})
    {
        options.ids = { kit };
        return options;
    }

    // A processor on the synthetic kit, its kit loaded, in Off mode with the host stopped: the engine plays nothing,
    // so what comes out is the audition alone.
    struct Quiet
    {
        explicit Quiet (SyntheticContent::Options options, double rate = 48000.0)
            : kit (options.ids.front()), plugin (std::move (options))
        {
            plugin->setPlayHead (&head);
            test::setParameter (*plugin, ParameterIds::playMode, 0.0f);
            test::prepareWithKit (*plugin, rate, kBlock);
        }

        ~Quiet() { plugin->setPlayHead (nullptr); }

        PluginProcessor& operator*() { return *plugin; }
        Controller& controller() { return plugin->controller(); }
        SoundRef ref (int file) const { return { kit, file }; }

        juce::String kit;
        SyntheticPlugin plugin;
        test::TestPlayHead head;
    };

    // Waits until the sound thread has offered its clip to the player.
    bool waitForOffer (PluginProcessor& p) { return test::waitFor ([&] { return p.auditionPlayer().offered(); }); }

    // How many samples of channel ch in [start, start + count) aren't exactly expected (i), i counted from start.
    template <class F>
    int mismatches (const juce::AudioBuffer<float>& out, int ch, int start, int count, F&& expected)
    {
        int wrong = 0;
        for (int i = 0; i < count; ++i)
            if (! juce::exactlyEqual (out.getSample (ch, start + i), expected (i)))
                ++wrong;
        return wrong;
    }

    // The fade-out's gain on its i-th sample: (240 - i) / 240.
    float fadeGain (int i) { return (float) (kFade - i) / (float) kFade; }

    // numSamples on a play head that moves on block by block, as a host does.
    juce::AudioBuffer<float> playOn (PluginProcessor& p, test::TestPlayHead& head, int numSamples)
    {
        juce::AudioBuffer<float> out (p.getTotalNumOutputChannels(), numSamples);
        for (int start = 0; start < numSamples; start += kBlock)
        {
            const auto n = std::min (kBlock, numSamples - start);
            const auto block = test::process (p, n);
            for (int ch = 0; ch < out.getNumChannels(); ++ch)
                out.copyFrom (ch, start, block, ch, 0, n);
            head.advance (n, p.getSampleRate());
        }
        return out;
    }
}

TEST_CASE ("an audition plays the file dry at -6 dB, sample for sample, at 48 kHz", "[audio][audition]")
{
    Quiet q (withKit ("AUD01"));
    auto& controller = q.controller();

    // First: with the host stopped, Off mode's output is exactly zero, so the audition can be compared exactly.
    REQUIRE (test::silent (test::render (*q, 8 * kBlock, kBlock)));

    const auto clip = clipFor (riser, 48000.0);
    REQUIRE (clip != nullptr);
    REQUIRE (clip->frames == 90000);
    const auto ticket = controller.audition (q.ref (riser));
    REQUIRE (ticket > 0u);
    CHECK (controller.auditionState().latest == ticket);
    REQUIRE (waitForOffer (*q));
    CHECK (controller.auditionState().waiting); // offered: it waits for the host's next block
    CHECK (controller.auditionState().sounding == 0u);
    CHECK (controller.auditionState().problem.isEmpty());

    const int total = 90000 + 4 * kBlock;
    juce::AudioBuffer<float> out (2, total);
    for (int start = 0; start < total; start += kBlock)
    {
        const int n = std::min (kBlock, total - start);
        const auto block = test::process (*q, n);
        for (int ch = 0; ch < 2; ++ch)
            out.copyFrom (ch, start, block, ch, 0, n);
        if (start == 0)
        {
            CHECK (controller.auditionState().sounding == ticket);
            CHECK_FALSE (controller.auditionState().waiting);
        }
    }
    for (int ch = 0; ch < 2; ++ch)
    {
        CAPTURE (ch);
        const float* x = channel (*clip, ch);
        CHECK (mismatches (out, ch, 0, 90000, [x] (int i) { return 0.5f * x[i]; }) == 0);
    }
    CHECK (test::peakOf (out, 0, 90000) > 0.05f);
    CHECK (test::silent (out, 90000));
    CHECK (controller.auditionState().sounding == 0u);
}

TEST_CASE ("at 44.1 kHz it lasts the file's length in output samples, rounded up", "[audio][audition]")
{
    Quiet q (withKit ("AUD02"), 44100.0);
    const auto clip = clipFor (riser, 44100.0);
    REQUIRE (clip != nullptr);
    CHECK (clip->frames == 82688); // ceil (90000 x 44100 / 48000) = ceil (82687.5)

    REQUIRE (q.controller().audition (q.ref (riser)) > 0u);
    REQUIRE (waitForOffer (*q));
    const auto out = test::render (*q, 82688 + 2 * kBlock, kBlock);
    for (int ch = 0; ch < 2; ++ch)
    {
        CAPTURE (ch);
        const float* x = channel (*clip, ch);
        CHECK (mismatches (out, ch, 0, 82688, [x] (int i) { return 0.5f * x[i]; }) == 0);
    }
    CHECK (test::silent (out, 82688));
}

TEST_CASE ("master_gain applies to auditions; mono output folds them", "[audio][audition]")
{
    const auto clip = clipFor (riser, 48000.0);
    REQUIRE (clip != nullptr);
    test::TestPlayHead head;

    SECTION ("master_gain at -6 dB scales the audition too")
    {
        SyntheticPlugin plugin (withKit ("AUD03"));
        plugin->setPlayHead (&head);
        test::setParameter (*plugin, ParameterIds::playMode, 0.0f);
        test::setMasterGainDb (*plugin, -6.0f); // before prepare: no ramp
        test::prepareWithKit (*plugin);
        const auto gain = juce::Decibels::decibelsToGain (test::masterGainDb (*plugin), Parameters::minDb);
        REQUIRE (gain < 0.6f);

        REQUIRE (plugin->controller().audition ({ "AUD03", riser }) > 0u);
        REQUIRE (waitForOffer (*plugin));
        const auto out = test::render (*plugin, 90000 + 2 * kBlock, kBlock); // the whole riser: it starts quietly
        for (int ch = 0; ch < 2; ++ch)
        {
            const float* x = channel (*clip, ch);
            CHECK (mismatches (out, ch, 0, 90000, [x, gain] (int i) { return (0.5f * x[i]) * gain; }) == 0);
        }
        CHECK (test::peakOf (out) > 0.01f);
        CHECK (test::silent (out, 90000));
        plugin->setPlayHead (nullptr);
    }

    SECTION ("a mono output gets (L + R) / 2")
    {
        SyntheticPlugin plugin (withKit ("AUD04"));
        juce::AudioProcessor::BusesLayout mono;
        mono.outputBuses.add (juce::AudioChannelSet::mono());
        REQUIRE (plugin->setBusesLayout (mono));
        plugin->setPlayHead (&head);
        test::setParameter (*plugin, ParameterIds::playMode, 0.0f);
        test::prepareWithKit (*plugin);

        REQUIRE (plugin->controller().audition ({ "AUD04", riser }) > 0u);
        REQUIRE (waitForOffer (*plugin));
        const auto out = test::render (*plugin, 90000 + 2 * kBlock, kBlock); // the whole riser: it starts quietly
        REQUIRE (out.getNumChannels() == 1);
        const float* l = channel (*clip, 0);
        const float* r = channel (*clip, 1);
        CHECK (mismatches (out, 0, 0, 90000, [l, r] (int i) { return 0.5f * (0.5f * (l[i] + r[i])); }) == 0);
        CHECK (test::peakOf (out) > 0.01f);
        CHECK (test::silent (out, 90000));
        plugin->setPlayHead (nullptr);
    }
}

TEST_CASE ("Stop fades out within 5 ms; a host reset and bypass silence it at once", "[audio][audition]")
{
    Quiet q (withKit ("AUD05"));
    auto& controller = q.controller();
    const auto clip = clipFor (riser, 48000.0);
    REQUIRE (clip != nullptr);
    REQUIRE (controller.audition (q.ref (riser)) > 0u);
    REQUIRE (waitForOffer (*q));
    test::process (*q, kBlock); // frames 0-511
    REQUIRE (controller.auditionState().sounding > 0u);
    juce::AudioBuffer<float> buffer (2, kBlock);
    juce::MidiBuffer midi;

    SECTION ("Stop: a 5 ms fade from where it was, then silence")
    {
        controller.stopAudition();
        CHECK (controller.auditionState().sounding == 0u); // stopped at once, though it fades out in the next block
        const auto out = test::process (*q, kBlock);
        for (int ch = 0; ch < 2; ++ch)
        {
            const float* x = channel (*clip, ch);
            CHECK (mismatches (out, ch, 0, kFade, [x] (int i) { return 0.5f * (x[512 + i] * fadeGain (i)); }) == 0);
        }
        CHECK_FALSE (test::silent (out, 0, kFade));
        CHECK (test::silent (out, kFade));
    }

    SECTION ("the host's reset: silent from the next block's first sample")
    {
        (*q).reset();
        CHECK (test::silent (test::process (*q, kBlock)));
    }

    SECTION ("bypass: silent at once")
    {
        (*q).processBlockBypassed (buffer, midi);
        CHECK (test::silent (buffer));
    }

    SECTION ("bypass while a new audition waits for its block: it never plays")
    {
        REQUIRE (controller.audition (q.ref (phrase)) > 0u);
        REQUIRE (waitForOffer (*q));
        (*q).processBlockBypassed (buffer, midi);
        CHECK (test::silent (buffer));
        CHECK_FALSE ((*q).auditionPlayer().offered());
    }

    CHECK (controller.auditionState().sounding == 0u);
    CHECK (test::silent (test::render (*q, 8 * kBlock, kBlock))); // and nothing comes back
    CHECK ((*q).auditionPlayer().sounding() == 0u);
}

TEST_CASE ("a host's reset or release before a prepare silences auditions at the first block; a prepare alone doesn't", "[audio][audition]")
{
    // A deactivate and reactivate (VST3 and CLAP) or a device restart (the Standalone app): reset (), releaseResources ()
    // and prepareToPlay at the same rate, then blocks. The groove starts from rest, and so must the audition.
    Quiet q (withKit ("AUD09"));
    auto& controller = q.controller();
    const auto clip = clipFor (riser, 48000.0);
    REQUIRE (clip != nullptr);
    REQUIRE (controller.audition (q.ref (riser)) > 0u);
    REQUIRE (waitForOffer (*q));
    test::process (*q, kBlock); // frames 0-511
    REQUIRE (controller.auditionState().sounding > 0u);

    SECTION ("reset, release, then a prepare at the same rate: silent from the first block")
    {
        (*q).reset();
        (*q).releaseResources();
        test::prepare (*q, 48000.0, kBlock);
        CHECK (test::silent (test::process (*q, kBlock)));
        CHECK ((*q).auditionPlayer().sounding() == 0u);
        CHECK (controller.auditionState().sounding == 0u);
        CHECK (test::silent (test::render (*q, 8 * kBlock, kBlock)));
    }

    SECTION ("a release alone, then a prepare: silent")
    {
        (*q).releaseResources();
        test::prepare (*q, 48000.0, kBlock);
        CHECK (test::silent (test::process (*q, kBlock)));
        CHECK ((*q).auditionPlayer().sounding() == 0u);
    }

    SECTION ("a reset alone, then a prepare: silent")
    {
        (*q).reset();
        test::prepare (*q, 48000.0, kBlock);
        CHECK (test::silent (test::process (*q, kBlock)));
    }

    SECTION ("a prepare at the same rate alone: it plays on where it was")
    {
        test::prepare (*q, 48000.0, kBlock);
        const auto out = test::process (*q, kBlock);
        for (int ch = 0; ch < 2; ++ch)
        {
            const float* x = channel (*clip, ch);
            CHECK (mismatches (out, ch, 0, kBlock, [x] (int i) { return 0.5f * x[512 + i]; }) == 0);
        }
        CHECK ((*q).auditionPlayer().sounding() > 0u);
    }
}

TEST_CASE ("an offline render never prints an audition", "[audio][audition]")
{
    // The same kit rendered offline from bar 1 twice: once with an audition offered just before, once without.
    const auto renderOffline = [] (bool withAudition) {
        SyntheticPlugin plugin (withKit ("OFF01"));
        test::TestPlayHead head;
        plugin->setPlayHead (&head);
        plugin->setNonRealtime (true);
        test::setParameter (*plugin, ParameterIds::playMode, 1.0f); // Loop
        test::prepareWithKit (*plugin);
        plugin->reset(); // from rest at sample 0, as a bounce starts
        test::process (*plugin, kBlock);
        if (withAudition)
        {
            REQUIRE (plugin->controller().audition ({ "OFF01", riser }) > 0u);
            REQUIRE (waitForOffer (*plugin));
        }
        head.play (128.0, 0.0);
        auto out = playOn (*plugin, head, 32 * kBlock);
        CHECK_FALSE (plugin->auditionPlayer().offered()); // taken by the first block and let go, unplayed
        CHECK (plugin->auditionPlayer().sounding() == 0u);
        plugin->setPlayHead (nullptr);
        return out;
    };
    const auto without = renderOffline (false);
    const auto with = renderOffline (true);
    REQUIRE (test::peakOf (without) > 0.05f);
    for (int ch = 0; ch < 2; ++ch)
    {
        CAPTURE (ch);
        CHECK (std::memcmp (with.getReadPointer (ch), without.getReadPointer (ch), sizeof (float) * (std::size_t) without.getNumSamples()) == 0);
    }
}

TEST_CASE ("a host that runs no blocks: the audition is dropped after 300 ms and the hint is raised", "[audio][audition]")
{
    Quiet q (withKit ("AUD06"));
    auto& controller = q.controller();
    const auto asked = std::chrono::steady_clock::now();
    const auto ticket = controller.audition (q.ref (riser));
    REQUIRE (ticket > 0u);
    REQUIRE (waitForOffer (*q));
    CHECK (controller.auditionState().waiting);
    CHECK_FALSE (controller.auditionState().hostSilent);

    // No block runs (Logic, a stopped track that isn't selected): after 300 ms the offer goes, and the hint says why.
    REQUIRE (test::waitFor ([&] { return controller.auditionState().hostSilent; }, 3000ms));
    CHECK (std::chrono::steady_clock::now() - asked >= SoundService::kUntakenLimit);
    const auto state = controller.auditionState();
    CHECK (state.latest == ticket);
    CHECK_FALSE (state.waiting);
    CHECK (state.sounding == 0u);
    CHECK_FALSE ((*q).auditionPlayer().offered());

    // When the host runs blocks again, nothing stale plays; the hint stays until the next audition.
    CHECK (test::silent (test::render (*q, 8 * kBlock, kBlock)));
    CHECK (controller.auditionState().hostSilent);
    REQUIRE (controller.audition (q.ref (riser)) > ticket);
    CHECK_FALSE (controller.auditionState().hostSilent);
}

TEST_CASE ("a host that stops running blocks mid-clip: after 300 ms the clip is given up, and never heard again", "[audio][audition]")
{
    Quiet q (withKit ("AUD08"));
    auto& controller = q.controller();
    const auto& sounds = (*q).soundService();
    const auto ticket = controller.audition (q.ref (phrase));
    REQUIRE (ticket > 0u);
    REQUIRE (waitForOffer (*q));
    test::process (*q, kBlock);
    test::process (*q, kBlock); // the phrase's frames 0-1023; then no block (Logic, once the track isn't selected)
    REQUIRE (controller.auditionState().sounding == ticket);

    SECTION ("left alone: the hint after 300 ms, and silence when the host runs blocks again")
    {
        const auto stopped = std::chrono::steady_clock::now();
        REQUIRE (test::waitFor ([&] { return controller.auditionState().hostSilent; }, 3000ms));
        CHECK (std::chrono::steady_clock::now() - stopped >= SoundService::kUntakenLimit);
        const auto state = controller.auditionState();
        CHECK (state.latest == ticket);
        CHECK (state.sounding == 0u); // the row shows play again, and Esc goes back to the host
        CHECK_FALSE (state.waiting);
        CHECK (state.problem.isEmpty());
        CHECK (sounds.clipsGivenUp() == 1);

        // Blocks come back: not the phrase's frames 1024 on, nor a fade of them.
        CHECK (test::silent (test::process (*q, kBlock)));
        CHECK (test::silent (test::render (*q, 8 * kBlock, kBlock)));
        CHECK ((*q).auditionPlayer().sounding() == 0u);
        CHECK (controller.auditionState().hostSilent); // until the next audition
        CHECK (test::waitFor ([&] { return sounds.clipsMade() == sounds.clipsDeleted(); }));

        // The next click plays as ever.
        const auto riserClip = clipFor (riser, 48000.0);
        REQUIRE (riserClip != nullptr);
        REQUIRE (controller.audition (q.ref (riser)) > ticket);
        CHECK_FALSE (controller.auditionState().hostSilent);
        REQUIRE (waitForOffer (*q));
        const auto out = test::process (*q, kBlock);
        for (int ch = 0; ch < 2; ++ch)
        {
            const float* x = channel (*riserClip, ch);
            CHECK (mismatches (out, ch, 0, kBlock, [x] (int i) { return 0.5f * x[i]; }) == 0);
        }
    }

    SECTION ("stopped during the gap: no stale fade when the host comes back")
    {
        controller.stopAudition();
        CHECK (controller.auditionState().sounding == 0u);
        REQUIRE (test::waitFor ([&] { return sounds.clipsGivenUp() > 0; }, 3000ms));
        CHECK_FALSE (controller.auditionState().hostSilent); // stopped by hand: nothing to explain
        CHECK (test::silent (test::process (*q, kBlock)));
        CHECK (test::silent (test::render (*q, 8 * kBlock, kBlock)));
    }
}

TEST_CASE ("an audition plays over the groove without changing it", "[audio][audition]")
{
    // The kit's Loop from bar 1, twice: once alone, once with an audition. The difference is exactly the clip x 0.5.
    const auto play = [] (bool withAudition) {
        SyntheticPlugin plugin (withKit ("GRV01"));
        test::TestPlayHead head;
        plugin->setPlayHead (&head);
        test::setParameter (*plugin, ParameterIds::playMode, 1.0f); // Loop
        test::prepareWithKit (*plugin);
        plugin->reset(); // from rest at sample 0
        test::process (*plugin, kBlock);
        if (withAudition)
        {
            REQUIRE (plugin->controller().audition ({ "GRV01", riser }) > 0u);
            REQUIRE (waitForOffer (*plugin));
        }
        head.play (128.0, 0.0);
        auto out = playOn (*plugin, head, 48 * kBlock);
        plugin->setPlayHead (nullptr);
        return out;
    };
    const auto groove = play (false);
    const auto both = play (true);
    REQUIRE (test::peakOf (groove) > 0.05f);
    const auto clip = clipFor (riser, 48000.0);
    REQUIRE (clip != nullptr);
    for (int ch = 0; ch < 2; ++ch)
    {
        CAPTURE (ch);
        const float* x = channel (*clip, ch);
        CHECK (mismatches (both, ch, 0, 48 * kBlock, [&groove, x, ch] (int i) { return groove.getSample (ch, i) + 0.5f * x[i]; }) == 0);
    }
}

TEST_CASE ("a new audition replaces the last: the old fades out in 5 ms, the new starts at once", "[audio][audition]")
{
    Quiet q (withKit ("AUD07"));
    auto& controller = q.controller();
    const auto riserClip = clipFor (riser, 48000.0);
    const auto phraseClip = clipFor (phrase, 48000.0);
    REQUIRE (riserClip != nullptr);
    REQUIRE (phraseClip != nullptr);

    const auto first = controller.audition (q.ref (riser));
    REQUIRE (first > 0u);
    REQUIRE (waitForOffer (*q));
    test::process (*q, kBlock);
    test::process (*q, kBlock); // the riser's frames 0-1023

    const auto second = controller.audition (q.ref (phrase));
    REQUIRE (second > first);
    REQUIRE (waitForOffer (*q));
    CHECK (controller.auditionState().sounding == first); // the riser plays on until the phrase's block
    CHECK (controller.auditionState().waiting);

    const auto out = test::process (*q, kBlock);
    CHECK (controller.auditionState().sounding == second);
    for (int ch = 0; ch < 2; ++ch)
    {
        CAPTURE (ch);
        const float* r = channel (*riserClip, ch);
        const float* p = channel (*phraseClip, ch);
        CHECK (mismatches (out, ch, 0, kFade, [r, p] (int i) {
            float e = 0.0f;
            e += 0.5f * (r[1024 + i] * fadeGain (i));
            e += 0.5f * p[i];
            return e;
        }) == 0);
        CHECK (mismatches (out, ch, kFade, kBlock - kFade, [p] (int i) { return 0.5f * p[kFade + i]; }) == 0);
    }
}

TEST_CASE ("selectKit stops the audition", "[audio][audition]")
{
    SyntheticContent::Options options;
    options.ids = { "SEL01", "SEL02" };
    Quiet q (options);
    auto& controller = q.controller();
    REQUIRE (controller.audition (q.ref (riser)) > 0u);
    REQUIRE (waitForOffer (*q));
    test::process (*q, kBlock);
    REQUIRE (controller.auditionState().sounding > 0u);

    controller.selectKit ("SEL02");
    CHECK (controller.auditionState().sounding == 0u);
    const auto out = test::process (*q, kBlock);
    CHECK_FALSE (test::silent (out, 0, kFade)); // it fades out...
    CHECK (test::silent (out, kFade)); // ...within 5 ms
    CHECK ((*q).auditionPlayer().sounding() == 0u);
    CHECK (test::silent (test::render (*q, 8 * kBlock, kBlock)));
}

TEST_CASE ("bytes that aren't kit.json's are never played, and say so", "[audio][audition]")
{
    SECTION ("a file with a byte changed: refused, and the kit's other files still play")
    {
        SyntheticContent::Options altered;
        altered.alteredPath = "FX/Riser.wav";
        Quiet q (withKit ("BAD01", altered));
        auto& controller = q.controller();
        const auto ticket = controller.audition (q.ref (riser));
        REQUIRE (ticket > 0u); // the kit is ready: the sound thread finds out
        REQUIRE (test::waitFor ([&] { return ! controller.auditionState().waiting; }));
        const auto state = controller.auditionState();
        CHECK (state.problem.contains ("Riser.wav isn't the file BAD01's content v1 lists"));
        CHECK (state.latest == ticket);
        CHECK (state.sounding == 0u);
        CHECK_FALSE ((*q).auditionPlayer().offered());
        CHECK (test::silent (test::render (*q, 8 * kBlock, kBlock)));

        REQUIRE (controller.audition (q.ref (phrase)) > ticket);
        CHECK (controller.auditionState().problem.isEmpty());
        REQUIRE (waitForOffer (*q));
        CHECK_FALSE (test::silent (test::process (*q, kBlock)));
    }

    SECTION ("a file that can't be read")
    {
        Quiet q (withKit ("BAD02"));
        auto& controller = q.controller();
        q.plugin.content->setSamples (SyntheticContent::Samples::failed); // the kit stays loaded; its files can't be read now
        REQUIRE (controller.audition (q.ref (riser)) > 0u);
        REQUIRE (test::waitFor ([&] { return ! controller.auditionState().waiting; }));
        const auto problem = controller.auditionState().problem;
        CHECK (problem.contains ("Riser.wav can't be read"));
        CHECK (problem.contains ("unreadable"));
        CHECK_FALSE ((*q).auditionPlayer().offered());
        CHECK (test::silent (test::render (*q, 8 * kBlock, kBlock)));
    }

    SECTION ("a request that fails stops the one before, as a new sound would have replaced it")
    {
        SyntheticContent::Options altered;
        altered.alteredPath = "FX/Riser.wav";
        Quiet q (withKit ("BAD03", altered));
        auto& controller = q.controller();
        REQUIRE (controller.audition (q.ref (phrase)) > 0u);
        REQUIRE (waitForOffer (*q));
        test::process (*q, kBlock);
        REQUIRE (controller.auditionState().sounding > 0u);

        REQUIRE (controller.audition (q.ref (riser)) > 0u);
        REQUIRE (test::waitFor ([&] { return ! controller.auditionState().waiting; }));
        CHECK (controller.auditionState().problem.isNotEmpty());
        CHECK (controller.auditionState().sounding == 0u);
        const auto out = test::process (*q, kBlock);
        CHECK (test::silent (out, kFade)); // the phrase faded out
        CHECK (test::silent (test::render (*q, 8 * kBlock, kBlock)));
    }
}

TEST_CASE ("the sound thread starts with the first audition, never before", "[audio][audition]")
{
    // Hosts and validators make, prepare and run instances by the dozen: none of that starts it.
    PluginProcessor plain;
    CHECK_FALSE (plain.soundService().started());

    SyntheticPlugin plugin (withKit ("THR00"));
    CHECK_FALSE (plugin->soundService().started());
    test::prepareWithKit (*plugin);
    test::render (*plugin, 8 * kBlock, kBlock);
    plugin->controller().stopAudition();
    CHECK_FALSE (plugin->soundService().started());
    CHECK (plugin->controller().audition ({ "THR00", riser }) > 0u);
    CHECK (plugin->soundService().started());
}

TEST_CASE ("auditions and stops on the message thread while the audio thread runs: every clip is made and deleted once", "[audio][audition]")
{
    SyntheticPlugin plugin (withKit ("THR01"));
    test::prepareWithKit (*plugin, 48000.0, 64);
    auto& controller = plugin->controller();
    std::atomic<bool> running { true };
    std::atomic<int> blocks { 0 };
    std::thread audio ([&] {
        juce::AudioBuffer<float> buffer (2, 64);
        juce::MidiBuffer midi;
        while (running.load())
        {
            if (blocks.fetch_add (1) % 97 == 96)
                plugin->processBlockBypassed (buffer, midi);
            else
                plugin->processBlock (buffer, midi);
            std::this_thread::sleep_for (300us);
        }
    });

    const int files[] { riser, phrase, synFile ("Drums/Kick A.wav"), synFile ("Drums/Snare v1 rr1.wav"), synFile ("Music/Lead C5.wav"),
        synFile ("Vox/Vox G4 v2.wav") };
    juce::Random random (11);
    for (int i = 0; i < 300; ++i)
    {
        if (random.nextInt (5) == 0)
            controller.stopAudition();
        else
            controller.audition ({ "THR01", files[random.nextInt (6)] });
        (void) controller.auditionState();
        std::this_thread::sleep_for (std::chrono::milliseconds (random.nextInt (4)));
    }
    running = false;
    audio.join();
    CHECK (blocks.load() > 100);

    // Then everything goes: a stop, bypassed blocks that let every clip go, and the sound thread deleting them.
    controller.stopAudition();
    CHECK (test::waitFor ([&] {
        juce::AudioBuffer<float> buffer (2, 64);
        juce::MidiBuffer midi;
        plugin->processBlockBypassed (buffer, midi);
        const auto& sounds = plugin->soundService();
        return ! controller.auditionState().waiting && sounds.clipsMade() == sounds.clipsDeleted();
    }));
    CHECK (plugin->soundService().clipsMade() > 10);
    CHECK_FALSE (plugin->auditionPlayer().offered());
}
