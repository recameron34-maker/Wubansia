// The processor against groovecore render (P12), hidden ([.render]): for the same kit and options, the plugin's
// output equals groovecore render's, bit for bit, at 48 kHz. Both drive the same engine (wubansia_engine) from the
// same manifests and MIDI (the plugin's embedded copies are the checkout's files) and samples; the plugin adds only
// its master_gain at unity. groovecore render runs with --keep-latency, as a host hears the plugin.
//
//     WUBANSIA_CONTENT_DIR=<content> [WUBANSIA_GROOVECORE=<groovecore>] WubansiaPluginTests "[.render]"
//
// Without content, or without a built groovecore (default: build/release/cli/groovecore in the checkout), it
// SKIPs.

#include "helpers/test_helpers.h"

#include <catch2/catch_test_macros.hpp>
#include <juce_core/juce_core.h>

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace wubansia;

namespace
{
    struct Stereo
    {
        std::vector<float> left, right;
    };

    // groovecore render's float WAV (wav_writer.h): RIFF, 'fmt ' IEEE float, 2 channels, the 'data' chunk.
    bool readFloatWav (const juce::File& file, Stereo& out, juce::String& why)
    {
        juce::MemoryBlock bytes;
        if (! file.loadFileAsData (bytes) || bytes.getSize() < 44)
        {
            why = "can't read " + file.getFullPathName();
            return false;
        }
        const auto* b = static_cast<const std::uint8_t*> (bytes.getData());
        const auto size = bytes.getSize();
        auto u32 = [&] (std::size_t at) { return (std::uint32_t) b[at] | (std::uint32_t) b[at + 1] << 8 | (std::uint32_t) b[at + 2] << 16 | (std::uint32_t) b[at + 3] << 24; };
        auto u16 = [&] (std::size_t at) { return (std::uint16_t) (b[at] | b[at + 1] << 8); };
        if (std::memcmp (b, "RIFF", 4) != 0 || std::memcmp (b + 8, "WAVE", 4) != 0)
        {
            why = "not a WAV";
            return false;
        }
        bool isFloat = false;
        for (std::size_t at = 12; at + 8 <= size;)
        {
            const auto length = (std::size_t) u32 (at + 4);
            if (std::memcmp (b + at, "fmt ", 4) == 0)
                isFloat = u16 (at + 8) == 3 && u16 (at + 10) == 2 && u16 (at + 22) == 32;
            else if (std::memcmp (b + at, "data", 4) == 0)
            {
                if (! isFloat || at + 8 + length > size)
                {
                    why = "not a 2-channel 32-bit float WAV";
                    return false;
                }
                const auto frames = length / 8;
                out.left.resize (frames);
                out.right.resize (frames);
                for (std::size_t i = 0; i < frames; ++i)
                {
                    std::uint32_t l = u32 (at + 8 + 8 * i), r = u32 (at + 12 + 8 * i);
                    std::memcpy (&out.left[i], &l, 4);
                    std::memcpy (&out.right[i], &r, 4);
                }
                return true;
            }
            at += 8 + length + (length & 1);
        }
        why = "no data chunk";
        return false;
    }

    juce::File groovecore()
    {
        if (const char* path = std::getenv ("WUBANSIA_GROOVECORE"); path != nullptr && *path != '\0')
            return juce::File (path);
        return juce::File (WUBANSIA_SOURCE_DIR).getChildFile ("build/release/cli/groovecore");
    }

    // groovecore render's Loop of a kit, one pass, with the latency kept, as a host hears the plugin.
    bool renderWithGroovecore (const juce::String& kit, int bars, const juce::File& out, juce::String& why)
    {
        const juce::File source (WUBANSIA_SOURCE_DIR);
        juce::StringArray args { groovecore().getFullPathName(), "render", "--kit", kit, "--bars", juce::String (bars), "--block", "512",
            "--keep-latency", "--manifests", source.getChildFile ("content/plugin").getFullPathName(), "--midi",
            source.getChildFile ("content/midi").getFullPathName(), "--out", out.getFullPathName() };
        juce::ChildProcess process;
        if (! process.start (args))
        {
            why = "can't start " + args[0];
            return false;
        }
        const auto text = process.readAllProcessOutput();
        process.waitForProcessToFinish (120000);
        if (process.getExitCode() != 0)
        {
            why = "groovecore render failed: " + text;
            return false;
        }
        return true;
    }

    int kitIndex (const juce::String& kit) { return kit.getTrailingIntValue() - 1; } // FUT01..FUT04: the catalog's order
}

TEST_CASE ("the processor plays a kit's Loop as groovecore render does, bit for bit", "[.render]")
{
    if (std::getenv ("WUBANSIA_CONTENT_DIR") == nullptr)
        SKIP ("set WUBANSIA_CONTENT_DIR to a content folder");
    if (! groovecore().existsAsFile())
        SKIP ("no groovecore at " << groovecore().getFullPathName() << " (build it, or set WUBANSIA_GROOVECORE)");

    constexpr double rate = 48000.0;
    constexpr int block = 512;
    constexpr int bars = 4;

    for (const juce::String kit : { "FUT01", "FUT02", "FUT03", "FUT04" })
    {
        CAPTURE (kit);
        const auto wav = juce::File::createTempFile (".wav");
        juce::String why;
        const bool rendered = renderWithGroovecore (kit, bars, wav, why);
        INFO (why);
        REQUIRE (rendered);
        Stereo expected;
        const bool read = readFloatWav (wav, expected, why);
        wav.deleteFile();
        wav.withFileExtension (".json").deleteFile();
        INFO (why);
        REQUIRE (read);

        PluginProcessor plugin;
        test::TestPlayHead head;
        plugin.setPlayHead (&head);
        plugin.setNonRealtime (true);
        test::prepare (plugin, rate, block);
        plugin.controller().selectKit (kit);
        REQUIRE (test::waitForKit (plugin, kitIndex (kit), std::chrono::seconds (60)));
        REQUIRE (plugin.getLatencySamples() > 0);
        CHECK (std::abs (test::masterGainDb (plugin)) < 1.0e-6f); // unity: the only stage after the engine

        // From rest at sample 0, playing from bar 1 at the kit's tempo, as groovecore render starts.
        plugin.reset();
        double kitBpm = 0.0;
        for (const auto& card : plugin.controller().kits())
            if (card.id == kit)
                kitBpm = card.bpm;
        REQUIRE (kitBpm > 0.0);
        head.play (kitBpm, 0.0);
        const auto frames = (int) expected.left.size();
        juce::AudioBuffer<float> out (2, frames);
        for (int start = 0; start < frames; start += block)
        {
            const int n = std::min (block, frames - start);
            juce::AudioBuffer<float> buffer (2, n);
            juce::MidiBuffer midi;
            plugin.processBlock (buffer, midi);
            for (int ch = 0; ch < 2; ++ch)
                out.copyFrom (ch, start, buffer, ch, 0, n);
            head.advance (n, rate);
        }
        plugin.setPlayHead (nullptr);

        int differing = 0, first = -1;
        for (int i = 0; i < frames; ++i)
            if (std::memcmp (out.getReadPointer (0) + i, &expected.left[(std::size_t) i], 4) != 0
                || std::memcmp (out.getReadPointer (1) + i, &expected.right[(std::size_t) i], 4) != 0)
            {
                ++differing;
                if (first < 0)
                    first = i;
            }
        INFO (kit << ": " << differing << " of " << frames << " frames differ, the first at " << first);
        CHECK (test::peakOf (out) > 0.1f);
        CHECK (differing == 0);
    }
}
