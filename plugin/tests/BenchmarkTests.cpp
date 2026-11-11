// The CPU budget (P12), hidden ([!benchmark]): the whole plugin at 48 kHz in 128-sample blocks, its kit's Loop
// playing, the voices kept full through MIDI in and every effect on (the macros at 100 %, the club master on), takes
// at most 15 % of one core. The gate is 2x that (30 %) for a CI runner's noise; the figure is reported either way.
// It measures the audio thread's own CPU time, so other processes on the machine don't count against it.
//
//     WubansiaPluginTests "[!benchmark]"                                (the synthetic kit: runs anywhere, CI's macos-15)
//     WUBANSIA_CONTENT_DIR=<content> WubansiaPluginTests "[!benchmark]"  (and FUT01 and FUT04, the heaviest kits)

#include "helpers/test_helpers.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cstdlib>
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
    // sound (every channel and key in turn: the keymap plays what it maps), after one second to warm up.
    Load measure (PluginProcessor& plugin, double bpm, double seconds)
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
        }
    }
}
