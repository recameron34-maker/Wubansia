#pragma once

// What the sampler's tests share: small kits built in code (no content needed), a rig that renders the
// sampler into a mix bus in pieces and keeps what it wrote, and the Python vectors
// (tools/sound/export_sampler_vectors.py) checked against vectors.sha256 before use.

#include "kit/kit.h"
#include "mix/mixer.h"
#include "sampler/envelope.h" // kPi
#include "sampler/sampler.h"

#include "util/file_io.h"
#include "util/sha256.h"

#include <catch2/catch_test_macros.hpp>

#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace sampler_test
{
    using namespace wubansia::engine;

    // ------------------------------------------------------------------ the Python vectors

    inline std::string vectorDir()
    {
        return std::string (WUBANSIA_SOURCE_DIR) + "/engine/tests/vectors/sampler/";
    }

    // name -> SHA-256 from vectors.sha256.
    inline std::map<std::string, std::string> vectorSums()
    {
        std::map<std::string, std::string> sums;
        auto bytes = wubansia::util::readBinaryFile (vectorDir() + "vectors.sha256");
        REQUIRE (bytes.ok());
        std::istringstream lines (std::string (bytes.value.begin(), bytes.value.end()));
        std::string hex, name;
        while (lines >> hex >> name)
            sums[name] = hex;
        return sums;
    }

    // A vector file's float32 values, after checking its SHA-256 against vectors.sha256.
    inline std::vector<float> readVector (const std::string& name)
    {
        auto bytes = wubansia::util::readBinaryFile (vectorDir() + name);
        INFO (name);
        REQUIRE (bytes.ok());
        const auto sums = vectorSums();
        REQUIRE (sums.count (name) == 1);
        REQUIRE (wubansia::util::sha256Hex (bytes.value) == sums.at (name));
        REQUIRE (bytes.value.size() % 4 == 0);
        std::vector<float> out (bytes.value.size() / 4);
        std::memcpy (out.data(), bytes.value.data(), bytes.value.size()); // little-endian, as every test host
        return out;
    }

    // ------------------------------------------------------------------ kits built in code

    struct LayerSpec
    {
        int velocity = 127;
        std::vector<int> rr; // file indices
    };

    struct ZoneSpec
    {
        int root = 60;
        int lo = 0;
        int hi = 127;
        std::vector<LayerSpec> layers;
    };

    // A Kit laid out as kit.h says (instruments, then their zones, layers and round robins appended in
    // order), with one mix channel per instrument unless told otherwise.
    struct TestKit
    {
        Kit kit;

        // A file from its samples: mono when right is empty.
        int addFile (std::vector<float> left, std::vector<float> right = {}, std::int64_t loopStart = 0,
            std::int64_t loopEnd = 0)
        {
            SampleData d;
            d.frames = static_cast<std::int64_t> (left.size());
            d.channels = right.empty() ? 1 : 2;
            d.audio = std::move (left);
            if (! right.empty())
            {
                REQUIRE (right.size() == d.audio.size());
                d.audio.insert (d.audio.end(), right.begin(), right.end());
            }
            KitFile f;
            f.path = "test/" + std::to_string (kit.files.size()) + ".wav";
            f.frames = d.frames;
            f.channels = d.channels;
            f.loopStart = loopStart;
            f.loopEnd = loopEnd;
            kit.files.push_back (f);
            kit.audio.push_back (std::move (d));
            return static_cast<int> (kit.files.size()) - 1;
        }

        int addInstrument (Instrument inst, const std::vector<ZoneSpec>& zones, const std::vector<int>& pans = {})
        {
            inst.firstZone = static_cast<std::uint16_t> (kit.zones.size());
            inst.zoneCount = static_cast<std::uint8_t> (zones.size());
            for (const auto& z : zones)
            {
                Zone zone;
                zone.root = static_cast<std::uint8_t> (z.root);
                zone.lo = static_cast<std::uint8_t> (z.lo);
                zone.hi = static_cast<std::uint8_t> (z.hi);
                zone.firstLayer = static_cast<std::uint16_t> (kit.layers.size());
                zone.layerCount = static_cast<std::uint8_t> (z.layers.size());
                for (const auto& l : z.layers)
                {
                    Layer layer;
                    layer.velocity = static_cast<std::uint8_t> (l.velocity);
                    layer.firstRr = static_cast<std::uint16_t> (kit.rr.size());
                    layer.rrCount = static_cast<std::uint8_t> (l.rr.size());
                    for (const int f : l.rr)
                        kit.rr.push_back (static_cast<std::uint16_t> (f));
                    kit.layers.push_back (layer);
                }
                kit.zones.push_back (zone);
            }
            inst.firstPan = static_cast<std::uint16_t> (kit.pans.size());
            inst.panCount = static_cast<std::uint8_t> (pans.size());
            for (const int p : pans)
                kit.pans.push_back (static_cast<std::int16_t> (p));
            if (inst.id.empty())
                inst.id = "inst" + std::to_string (kit.instruments.size());
            kit.instruments.push_back (inst);
            return static_cast<int> (kit.instruments.size()) - 1;
        }

        // One file on one key range: the simplest instrument.
        int addSimple (Instrument inst, int file, int root = 60, int lo = 0, int hi = 127)
        {
            return addInstrument (std::move (inst), { ZoneSpec { root, lo, hi, { LayerSpec { 127, { file } } } } });
        }
    };

    inline Instrument instrument (SampleMode mode, std::uint8_t mixChannel = 0, Lane lane = Lane::perc)
    {
        Instrument i;
        i.mode = mode;
        i.mixChannel = mixChannel;
        i.lane = lane;
        return i;
    }

    inline std::vector<float> constant (std::size_t frames, float value)
    {
        return std::vector<float> (frames, value);
    }

    inline std::vector<float> sine (std::size_t frames, double hz, double amplitude, double rate = 48000.0, double phase = 0.0)
    {
        std::vector<float> x (frames);
        for (std::size_t k = 0; k < frames; ++k)
            x[k] = static_cast<float> (amplitude * std::sin (2.0 * kPi * hz * static_cast<double> (k) / rate + phase));
        return x;
    }

    inline NoteOn note (int instrumentIndex, int key, int velocity = 127, int offset = 0, Source source = Source::sequencer)
    {
        NoteOn n;
        n.instrument = static_cast<std::int16_t> (instrumentIndex);
        n.key = static_cast<std::uint8_t> (key);
        n.velocity = static_cast<std::uint8_t> (velocity);
        n.offset = offset;
        n.source = source;
        return n;
    }

    // ------------------------------------------------------------------ the rig

    // Renders a Sampler in pieces of `piece` samples (each within one 32-sample frame when piece divides
    // 32, as the engine's are) and keeps every mix channel's and send's output. Actions run at absolute
    // sample times: each is queued in the piece that holds its time, at the matching offset.
    struct Rig
    {
        explicit Rig (double sampleRate = 48000.0, int pieceSize = 32, int maxBlock = 512) : rate (sampleRate), piece (pieceSize)
        {
            sampler.prepare (rate, maxBlock);
            bus.prepare (kMaxMixChannels, maxBlock);
            sends.prepare (maxBlock);
        }

        using Action = std::function<void (Sampler&, int offset)>;

        void at (std::int64_t time, Action action) { actions.emplace (time, std::move (action)); }

        // Renders up to absolute time `until`, appending to the outputs.
        void runUntil (std::int64_t until)
        {
            while (now < until)
            {
                const int n = static_cast<int> (std::min<std::int64_t> (piece - now % piece, until - now));
                for (auto it = actions.lower_bound (now); it != actions.end() && it->first < now + n;)
                {
                    it->second (sampler, static_cast<int> (it->first - now));
                    it = actions.erase (it);
                }
                bus.clear (n);
                sends.clear (n);
                sampler.render (bus, sends, n);
                for (int c = 0; c < kMaxMixChannels; ++c)
                {
                    auto& l = left[c];
                    auto& r = right[c];
                    l.insert (l.end(), bus.left (c), bus.left (c) + n);
                    r.insert (r.end(), bus.right (c), bus.right (c) + n);
                }
                space.insert (space.end(), sends.send (Send::space), sends.send (Send::space) + n);
                echo.insert (echo.end(), sends.send (Send::echo), sends.send (Send::echo) + n);
                peakVoices = std::max (peakVoices, sampler.activeVoices());
                now += n;
            }
        }

        double rate;
        int piece;
        Sampler sampler;
        MixBus bus;
        SendBus sends;
        std::multimap<std::int64_t, Action> actions;
        std::int64_t now = 0;
        std::map<int, std::vector<float>> left, right;
        std::vector<float> space, echo;
        int peakVoices = 0;
    };

    // The largest |x[i] - x[i - 1]|.
    inline double maxStep (const std::vector<float>& x, std::size_t from = 1, std::size_t to = SIZE_MAX)
    {
        double m = 0.0;
        for (std::size_t i = std::max<std::size_t> (from, 1); i < std::min (to, x.size()); ++i)
            m = std::max (m, std::fabs (static_cast<double> (x[i]) - static_cast<double> (x[i - 1])));
        return m;
    }

    inline double maxAbs (const std::vector<float>& x, std::size_t from = 0, std::size_t to = SIZE_MAX)
    {
        double m = 0.0;
        for (std::size_t i = from; i < std::min (to, x.size()); ++i)
            m = std::max (m, std::fabs (static_cast<double> (x[i])));
        return m;
    }

    // The largest |a[i] - b[i]| over [from, to), and where.
    inline std::pair<double, std::size_t> maxDiff (const std::vector<float>& a, const std::vector<float>& b,
        std::size_t from = 0, std::size_t to = SIZE_MAX)
    {
        double m = 0.0;
        std::size_t where = 0;
        for (std::size_t i = from; i < std::min ({ to, a.size(), b.size() }); ++i)
        {
            const double d = std::fabs (static_cast<double> (a[i]) - static_cast<double> (b[i]));
            if (d > m)
            {
                m = d;
                where = i;
            }
        }
        return { m, where };
    }

    // Exact equality, bit for bit (so -0 and 0 differ, and NaN never equals anything but its own bits).
    inline bool same (float a, float b) noexcept { return std::bit_cast<std::uint32_t> (a) == std::bit_cast<std::uint32_t> (b); }
    inline bool same (double a, double b) noexcept { return std::bit_cast<std::uint64_t> (a) == std::bit_cast<std::uint64_t> (b); }

    // Nothing but silence: every |x| is 0.
    inline bool silent (const std::vector<float>& x, std::size_t from = 0, std::size_t to = SIZE_MAX)
    {
        return ! (maxAbs (x, from, to) > 0.0);
    }

    // Exact equality of two float vectors, element by element (the bits, so -0 and NaN count as different).
    inline bool sameBits (const std::vector<float>& a, const std::vector<float>& b)
    {
        return a.size() == b.size() && (a.empty() || std::memcmp (a.data(), b.data(), a.size() * sizeof (float)) == 0);
    }
}
