// The CPU budget (P12), hidden ([!benchmark]): the whole plugin at 48 kHz in 128-sample blocks, its kit's Loop
// playing, the voices kept full through MIDI in and every effect on (the macros at 100 %, the club master on), takes
// at most 15 % of one core, with a Samples drawer audition playing on top too (ADR 0024). The gate is 2x that (30 %)
// for a CI runner's noise; the figure is reported either way. It measures the audio thread's own CPU time, so other
// processes on the machine (and the sound thread making the clips) don't count against it.
//
// And the Samples drawer's synchronous work, with real content: exporting an 8-bar riser (the drag copies it inside the
// mouse event) under 100 ms, and its 44.1 kHz audition clip ready under 200 ms.
//
//     WubansiaPluginTests "[!benchmark]"                                (the synthetic kit: runs anywhere, CI's macos-15)
//     WUBANSIA_CONTENT_DIR=<content> WubansiaPluginTests "[!benchmark]"  (and FUT01 and FUT04, the heaviest kits)

#include "helpers/test_helpers.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <functional>
#include <vector>

#if ! defined(_WIN32)
    #include <time.h>
#endif

using namespace wubansia;

namespace
{
    // This thread's CPU time, seconds (wall time where there is no such clock).
    double threadSeconds()
    {
#if defined(_WIN32)
        return std::chrono::duration<double> (std::chrono::steady_clock::now().time_since_epoch()).count();
#else
        timespec ts {};
        clock_gettime (CLOCK_THREAD_CPUTIME_ID, &ts);
        return (double) ts.tv_sec + 1e-9 * (double) ts.tv_nsec;
#endif
    }

    struct Load
    {
        double mean = 0.0; // CPU time over audio time
        double p99 = 0.0; // the 99th percentile block
        double voices = 0.0; // sounding voices, on average
    };

    // seconds of the kit's Loop at its tempo in blocks of `block`, a note-on every block while fewer than 64 voices
    // sound (every channel and key in turn: the keymap plays what it maps), after one second to warm up. Before each
    // block, off the clock, beforeBlock () does what the message thread would.
    Load measure (PluginProcessor& plugin, double bpm, double seconds, const std::function<void()>& beforeBlock = {})
    {
        constexpr double rate = 48000.0;
        constexpr int block = 128;
        test::TestPlayHead head;
        plugin.setPlayHead (&head);
        head.play (bpm, 0.0);
        int channel = 1, key = 24;
        std::vector<double> loads;
        double voiceSum = 0.0;
        const int warm = (int) (rate / block);
        const int blocks = warm + (int) (seconds * rate / block);
        juce::AudioBuffer<float> buffer (2, block);
        for (int b = 0; b < blocks; ++b)
        {
            juce::MidiBuffer midi;
            for (int i = 0; i < 4 && plugin.telemetry().voices.load() + i < 64; ++i)
            {
                midi.addEvent (juce::MidiMessage::noteOn (channel, key, (juce::uint8) 100), i);
                if (++key > 96)
                {
                    key = 24;
                    channel = channel % 16 + 1;
                }
            }
            if (beforeBlock)
                beforeBlock();
            const double t0 = threadSeconds();
            plugin.processBlock (buffer, midi);
            const double spent = threadSeconds() - t0;
            head.advance (block, rate);
            if (b >= warm)
            {
                loads.push_back (spent / (block / rate));
                voiceSum += plugin.telemetry().voices.load();
            }
        }
        plugin.setPlayHead (nullptr);
        Load load;
        for (const double l : loads)
            load.mean += l;
        load.mean /= (double) loads.size();
        std::sort (loads.begin(), loads.end());
        load.p99 = loads[(std::size_t) (0.99 * (double) (loads.size() - 1))];
        load.voices = voiceSum / (double) loads.size();
        return load;
    }

    void report (const char* what, const Load& load)
    {
        WARN (what << ": " << 100.0 * load.mean << " % of one core at 48 kHz / 128 samples (99 % of blocks under " << 100.0 * load.p99
                   << " %), " << load.voices << " voices on average");
    }

    // An audition of the kit's file kept playing, as someone clicking it again whenever it ends would: before each block,
    // it asks again once nothing sounds or waits. Counts the blocks it sounded before.
    struct Auditioning
    {
        Auditioning (PluginProcessor& p, const juce::String& kit, int file) : plugin (p), ref { kit, file } {}

        void operator()()
        {
            auto& controller = plugin.controller();
            const auto state = controller.auditionState();
            ++blocks;
            if (state.sounding != 0)
                ++sounding;
            else if (! state.waiting)
                controller.audition (ref);
        }

        double share() const { return blocks > 0 ? (double) sounding / (double) blocks : 0.0; }

        PluginProcessor& plugin;
        SoundRef ref;
        int blocks = 0;
        int sounding = 0;
    };

    // The kit's longest listed file (its 8-bar riser, in the four kits).
    int longestFile (const PluginProcessor& plugin, const juce::String& kit)
    {
        const auto* entry = plugin.kitService().catalog().find (kit.toStdString());
        if (entry == nullptr || entry->manifest.files.empty())
            return -1;
        const auto& files = entry->manifest.files;
        const auto it = std::max_element (files.begin(), files.end(), [] (const auto& a, const auto& b) { return a.frames < b.frames; });
        return (int) (it - files.begin());
    }
}

TEST_CASE ("the plugin at 48 kHz / 128 samples, 64 voices and every effect, takes at most 15 % of one core", "[!benchmark]")
{
    constexpr double gate = 0.30; // 15 % x 2 for a runner's noise

    SECTION ("the synthetic kit")
    {
        test::SyntheticPlugin plugin;
        plugin->setNonRealtime (false);
        test::prepare (*plugin, 48000.0, 128);
        REQUIRE (test::waitForKit (*plugin));
        const auto load = measure (*plugin, 128.0, 20.0);
        report ("SYN01", load);
        CHECK (load.voices > 32.0);
        CHECK (load.mean <= gate);

        // From rest again (prepare resets the voices; the kit stays), as the first run started: a second run straight on
        // starts with every voice busy and costs more whatever plays on top.
        test::prepare (*plugin, 48000.0, 128);
        Auditioning audition (*plugin, "SYN01", longestFile (*plugin, "SYN01"));
        const auto withAudition = measure (*plugin, 128.0, 20.0, std::ref (audition));
        report ("SYN01 with an audition playing", withAudition);
        WARN ("the audition sounded in " << 100.0 * audition.share() << " % of the blocks");
        CHECK (audition.share() > 0.9);
        CHECK (withAudition.voices > 32.0);
        CHECK (withAudition.mean <= gate);
    }

    SECTION ("the real kits, with content")
    {
        if (std::getenv ("WUBANSIA_CONTENT_DIR") == nullptr)
            SKIP ("set WUBANSIA_CONTENT_DIR to a content folder for FUT01 and FUT04");
        for (const juce::String kit : { "FUT01", "FUT04" })
        {
            CAPTURE (kit);
            PluginProcessor plugin;
            test::prepare (plugin, 48000.0, 128);
            plugin.controller().selectKit (kit);
            plugin.controller().setScopeOn (true); // machine room's output scope (ADR 0032), nobody reading: the ring fills
            REQUIRE (test::waitForKit (plugin, kit.getTrailingIntValue() - 1, std::chrono::seconds (60)));
            double bpm = 0.0;
            for (const auto& card : plugin.controller().kits())
                if (card.id == kit)
                    bpm = card.bpm;
            REQUIRE (bpm > 0.0);
            const auto load = measure (plugin, bpm, 20.0);
            report (kit.toRawUTF8(), load);
            CHECK (load.voices > 32.0);
            CHECK (load.mean <= gate);

            test::prepare (plugin, 48000.0, 128); // from rest again, as the first run started
            Auditioning audition (plugin, kit, longestFile (plugin, kit));
            const auto withAudition = measure (plugin, bpm, 20.0, std::ref (audition));
            report ((kit + " with an audition playing").toRawUTF8(), withAudition);
            WARN ("the audition sounded in " << 100.0 * audition.share() << " % of the blocks");
            CHECK (audition.share() > 0.9);
            CHECK (withAudition.voices > 32.0);
            CHECK (withAudition.mean <= gate);
        }
    }
}

TEST_CASE ("exporting an 8-bar riser takes under 100 ms; its 44.1 kHz clip under 200 ms", "[!benchmark]")
{
    // The drag copies the file inside the mouse event (ADR 0024), so the copy is the synchronous budget; the clip is how
    // long a click waits for sound. FUT03's FX/Riser.wav: the largest file of the four kits (4.3 MB, 8 bars at 128 BPM).
    if (std::getenv ("WUBANSIA_CONTENT_DIR") == nullptr)
        SKIP ("set WUBANSIA_CONTENT_DIR to a content folder");
    using Clock = std::chrono::steady_clock;
    const auto ms = [] (Clock::duration d) { return std::chrono::duration<double, std::milli> (d).count(); };

    PluginProcessor plugin;
    test::prepare (plugin, 44100.0, 512);
    auto& controller = static_cast<ControllerImpl&> (plugin.controller());
    controller.selectKit ("FUT03");
    REQUIRE (test::waitForStatus (plugin, ContentState::ready, std::chrono::seconds (60)));
    const auto riser = longestFile (plugin, "FUT03");
    REQUIRE (riser >= 0);
    const auto samples = juce::File::getSpecialLocation (juce::File::tempDirectory).getNonexistentChildFile ("WubansiaBenchmarkSamples", "", false);
    controller.setSamplesFolderForTesting (samples);

    const auto exportStart = Clock::now();
    const auto exported = controller.exportSound ({ "FUT03", riser });
    const auto exportMs = ms (Clock::now() - exportStart);
    INFO (exported.problem);
    REQUIRE (exported.file.existsAsFile());
    WARN ("exporting " << exported.file.getFileName() << " (" << exported.file.getSize() << " bytes) took " << exportMs << " ms");
    CHECK (exportMs < 100.0);

    const auto clipStart = Clock::now();
    REQUIRE (controller.audition ({ "FUT03", riser }) > 0u);
    REQUIRE (test::waitFor ([&] { return plugin.auditionPlayer().offered() || ! controller.auditionState().waiting; }));
    const auto clipMs = ms (Clock::now() - clipStart);
    INFO (controller.auditionState().problem);
    REQUIRE (plugin.auditionPlayer().offered());
    WARN ("its 44.1 kHz audition clip was ready in " << clipMs << " ms");
    CHECK (clipMs < 200.0);
    controller.stopAudition();
    samples.deleteRecursively();
}
