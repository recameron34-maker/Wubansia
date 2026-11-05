#pragma once

// What the phrase lane tests share (P6): the Python vectors (tools/sound/export_phrase_vectors.py) checked against
// vectors.sha256, the noise stems they play (made here the same way), a timeline read straight from an SMF (the
// builder's swing snap and loop wrap, nothing else: P7's buildTimeline does the rest), kits with one phrase built in
// code, and a rig that drives phrase voices as the sampler routes them: a note of a phrase a busy voice plays goes to
// continueWith (), any other to start () on a free voice.

#include "engine.h"
#include "kit/kit.h"
#include "kit/manifest.h"
#include "mix/mixer.h"
#include "sampler/phrase_voice.h"
#include "sampler/sampler.h"
#include "sched/timeline.h"

#include "gen/json.h"
#include "midi/reader.h"
#include "util/file_io.h"
#include "util/sha256.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <span>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace phrase_test
{
    using namespace wubansia::engine;
    namespace json = wubansia::gen::json;

    // ------------------------------------------------------------------ the Python vectors

    inline std::string vectorDir()
    {
        return std::string (WUBANSIA_SOURCE_DIR) + "/engine/tests/vectors/phrase/";
    }

    // A vector file's bytes, after checking its SHA-256 against vectors.sha256.
    inline std::vector<std::uint8_t> readVector (const std::string& name)
    {
        auto sums = wubansia::util::readBinaryFile (vectorDir() + "vectors.sha256");
        REQUIRE (sums.ok());
        std::istringstream lines (std::string (sums.value.begin(), sums.value.end()));
        std::map<std::string, std::string> byName;
        std::string hex, file;
        while (lines >> hex >> file)
            byName[file] = hex;
        auto bytes = wubansia::util::readBinaryFile (vectorDir() + name);
        INFO (name);
        REQUIRE (bytes.ok());
        REQUIRE (byName.count (name) == 1);
        REQUIRE (wubansia::util::sha256Hex (bytes.value) == byName.at (name));
        return std::move (bytes.value);
    }

    inline json::Value readJson (const std::string& name)
    {
        const auto bytes = readVector (name);
        auto parsed = json::parse (std::string_view (reinterpret_cast<const char*> (bytes.data()), bytes.size()));
        INFO (parsed.error);
        REQUIRE (parsed.ok());
        return std::move (parsed.value);
    }

    inline std::vector<float> readFloats (const std::string& name)
    {
        const auto bytes = readVector (name);
        REQUIRE (bytes.size() % 4 == 0);
        std::vector<float> out (bytes.size() / 4);
        std::memcpy (out.data(), bytes.data(), bytes.size()); // little-endian, as every test host
        return out;
    }

    inline const json::Value& at (const json::Value& v, std::string_view key)
    {
        const auto* found = v.find (key);
        INFO (key);
        REQUIRE (found != nullptr);
        return *found;
    }

    inline std::int64_t integer (const json::Value& v, std::string_view key) { return at (v, key).integer; }

    // ------------------------------------------------------------------ noise stems

    // export_phrase_vectors.noise (): a 24-bit step of a 32-bit hash (lowbias32) of (seed, frame, channel).
    inline float noise (std::uint32_t seed, std::int64_t frame, int channel)
    {
        std::uint32_t x = static_cast<std::uint32_t> (frame) * 0x9E3779B1u
                          + (seed * 0x85EBCA77u + static_cast<std::uint32_t> (channel) * 0xC2B2AE3Du);
        x ^= x >> 16;
        x *= 0x7FEB352Du;
        x ^= x >> 15;
        x *= 0x846CA68Bu;
        x ^= x >> 16;
        return static_cast<float> (static_cast<std::int32_t> (x >> 8) - (1 << 23)) / static_cast<float> (1 << 23);
    }

    inline SampleData stemOf (std::int64_t frames, int channels, const std::function<float (std::int64_t, int)>& value)
    {
        SampleData d;
        d.frames = frames;
        d.channels = channels;
        d.audio.resize (static_cast<std::size_t> (frames * channels));
        for (int c = 0; c < channels; ++c)
            for (std::int64_t n = 0; n < frames; ++n)
                d.audio[static_cast<std::size_t> (c * frames + n)] = value (n, c);
        return d;
    }

    inline SampleData noiseStem (std::uint32_t seed, std::int64_t frames, int channels)
    {
        return stemOf (frames, channels, [seed] (std::int64_t n, int c) { return noise (seed, n, c); });
    }

    // ------------------------------------------------------------------ positions

    // The timeline's swing snap (timeline.h, buildTimeline's step 1): a tick on an odd 16th at the written offset,
    // round ((swing - 0.5) x 480) ticks, moves to the exact (swingPm - 500) x 480 milliticks.
    inline std::int64_t snap (std::int64_t tick, std::int32_t swingPm)
    {
        const std::int64_t exact = std::int64_t { swingPm - 500 } * 480;
        const std::int64_t written = (exact >= 0 ? exact + 500 : exact - 500) / 1000;
        const std::int64_t inEighth = ((tick % 480) + 480) % 480;
        if (inEighth == 240 + written)
            return (tick - written) * kMilliticksPerTick + exact;
        return tick * kMilliticksPerTick;
    }

    // 48 kHz frames as milliticks at bpm, rounded half up.
    constexpr std::int64_t framesToMt (std::int64_t frames, std::int64_t bpm) { return (2 * frames * bpm + 3) / 6; }

    // Where a 16th starts on the swung grid, in milliticks.
    constexpr std::int64_t gridMt (std::int64_t sixteenth, std::int32_t swingPm)
    {
        return sixteenth * kMilliticksPerSixteenth + (sixteenth % 2 != 0 ? std::int64_t { swingPm - 500 } * 480 : 0);
    }

    // A position in milliticks as host samples from 0, at bpm and rate, rounded to the nearest.
    inline std::int64_t samplesAt (std::int64_t pos, double bpm, double rate)
    {
        return std::llround (static_cast<double> (pos) * rate * 60.0 / (bpm * static_cast<double> (kMilliticksPerQuarter)));
    }

    // ------------------------------------------------------------------ timelines

    // An SMF's notes and controllers as a Timeline: positions swing-snapped, a loop's events at its end moved to 0,
    // notes by (pos, channel, key), controllers by position. No pre-roll, round robins, ramps or keys.
    inline Timeline timelineOf (std::span<const std::uint8_t> smf, std::int32_t swingPm, std::int64_t loopTicks)
    {
        const auto file = wubansia::midi::readSmf (smf);
        INFO (file.error);
        REQUIRE (file.ok());
        Timeline t;
        t.loops = loopTicks > 0;
        std::int64_t end = 0;
        for (const auto& track : file.value.tracks)
        {
            for (const auto& n : wubansia::midi::notesOf (track))
            {
                Note note;
                const auto on = snap (n.tick, swingPm);
                const auto off = snap (n.tick + std::max<std::int64_t> (1, n.length), swingPm);
                note.pos = loopTicks > 0 && n.tick >= loopTicks ? on - loopTicks * kMilliticksPerTick : on;
                note.len = off - on;
                note.channel = n.channel;
                note.key = n.key;
                note.velocity = n.velocity;
                t.notes.push_back (note);
                end = std::max (end, n.tick + n.length);
            }
            for (const auto& c : wubansia::midi::controllersOf (track))
            {
                Cc cc;
                cc.pos = (loopTicks > 0 && c.tick >= loopTicks ? c.tick - loopTicks : c.tick) * kMilliticksPerTick;
                cc.rampTo = cc.pos;
                cc.channel = c.channel;
                cc.controller = c.number;
                cc.value = c.value;
                cc.nextValue = c.value;
                t.ccs.push_back (cc);
            }
        }
        t.length = loopTicks > 0 ? loopTicks * kMilliticksPerTick : (end + 3839) / 3840 * 3840 * kMilliticksPerTick;
        std::stable_sort (t.notes.begin(), t.notes.end(), [] (const Note& a, const Note& b) {
            return std::tie (a.pos, a.channel, a.key) < std::tie (b.pos, b.channel, b.key);
        });
        std::stable_sort (t.ccs.begin(), t.ccs.end(), [] (const Cc& a, const Cc& b) { return a.pos < b.pos; });
        return t;
    }

    inline std::vector<const Note*> notesOn (const Timeline& t, int channel)
    {
        std::vector<const Note*> out;
        for (const auto& n : t.notes)
            if (n.channel == channel)
                out.push_back (&n);
        return out;
    }

    inline bool sorted (const Timeline& t)
    {
        return std::is_sorted (t.notes.begin(), t.notes.end(), [] (const Note& a, const Note& b) {
            return std::tie (a.pos, a.channel, a.key) < std::tie (b.pos, b.channel, b.key);
        });
    }

    // ------------------------------------------------------------------ kits with one phrase

    struct SliceSpec
    {
        int step = 0;
        int key = 60;
        std::int64_t start = 0;
        std::int64_t end = 0;
        std::int64_t fade = 0;
        int lead = 0;
        std::vector<int> holds;
    };

    // A Kit holding stems and phrases, laid out as kit.h says; the voice needs nothing else (but the kit's BPM).
    struct PhraseKit
    {
        Kit kit;

        explicit PhraseKit (std::int32_t bpm = 128) { kit.info.bpm = bpm; }

        int addStem (SampleData audio)
        {
            KitFile f;
            f.path = "Stems/" + std::to_string (kit.files.size()) + ".wav";
            f.frames = audio.frames;
            f.channels = audio.channels;
            kit.files.push_back (f);
            kit.audio.push_back (std::move (audio));
            return static_cast<int> (kit.files.size()) - 1;
        }

        int addPhrase (Phrase p, const std::vector<SliceSpec>& slices)
        {
            p.firstSlice = static_cast<std::uint16_t> (kit.slices.size());
            p.sliceCount = static_cast<std::uint16_t> (slices.size());
            for (const auto& s : slices)
            {
                PhraseSlice x;
                x.step = s.step;
                x.key = static_cast<std::uint8_t> (s.key);
                x.start = s.start;
                x.end = s.end;
                x.fade = s.fade;
                x.lead = s.lead;
                x.holdCount = static_cast<std::uint8_t> (s.holds.size());
                for (std::size_t h = 0; h < s.holds.size(); ++h)
                    x.holds[h] = static_cast<std::uint8_t> (s.holds[h]);
                kit.slices.push_back (x);
            }
            kit.phrases.push_back (p);
            return static_cast<int> (kit.phrases.size()) - 1;
        }
    };

    // ------------------------------------------------------------------ driving voices

    // A phrase note at an absolute host sample.
    struct Hit
    {
        std::int64_t at = 0;
        NoteOn note;
    };

    inline NoteOn phraseNote (int phrase, int slice, bool continues, std::int64_t startFrame = 0, int character = 0)
    {
        NoteOn n;
        n.phrase = static_cast<std::int16_t> (phrase);
        n.slice = static_cast<std::int16_t> (slice);
        n.continues = continues;
        n.startFrame = startFrame;
        n.character = static_cast<std::uint8_t> (character);
        n.source = Source::sequencer;
        return n;
    }

    // Phrase voices on a mix bus, played the sampler's way. Pieces never cross a multiple of 32 samples from 0.
    class Rig
    {
    public:
        Rig (double rate, int mixChannels, int voices = 4)
            : sampleRate (rate), slots (static_cast<std::size_t> (voices))
        {
            bus.prepare (mixChannels, 32);
            sends.prepare (32);
            for (auto& s : slots)
            {
                s.voice = std::make_unique<PhraseVoice>();
                s.voice->prepare (rate);
            }
            left.resize (static_cast<std::size_t> (mixChannels));
            right.resize (static_cast<std::size_t> (mixChannels));
        }

        // Renders [now, now + count) with hits (sorted by at) and pieces of piece (start) samples (cut at the next
        // multiple of 32), keeping every channel's output and the sends' (unless collect is off) and showing each
        // piece to listen. laneSend: the lane's send gain.
        void run (const Kit& kit, const std::vector<Hit>& hits, std::int64_t count,
            const std::function<int (std::int64_t)>& piece = {}, float laneSend = 1.0f)
        {
            std::size_t next = 0;
            while (next < hits.size() && hits[next].at < now)
                ++next;
            const std::int64_t end = now + count;
            std::array<float, 32> lane {};
            lane.fill (laneSend);
            while (now < end)
            {
                int n = piece ? std::max (1, piece (now)) : 32;
                n = static_cast<int> (std::min<std::int64_t> ({ n, 32 - now % 32, end - now }));
                bus.clear (n);
                sends.clear (n);
                for (auto& s : slots)
                    s.voice->setTempo (tempo);
                for (; next < hits.size() && hits[next].at < now + n; ++next)
                    route (kit, hits[next].note, static_cast<int> (hits[next].at - now));
                for (auto& s : slots)
                    if (s.busy)
                    {
                        s.voice->render (bus, sends, n, lane.data());
                        s.busy = s.voice->active();
                    }
                if (listen)
                    listen (now, n, bus, sends);
                if (collect)
                {
                    for (int c = 0; c < bus.numChannels(); ++c)
                    {
                        left[static_cast<std::size_t> (c)].insert (left[static_cast<std::size_t> (c)].end(), bus.left (c), bus.left (c) + n);
                        right[static_cast<std::size_t> (c)].insert (right[static_cast<std::size_t> (c)].end(), bus.right (c), bus.right (c) + n);
                    }
                    space.insert (space.end(), sends.send (Send::space), sends.send (Send::space) + n);
                    echo.insert (echo.end(), sends.send (Send::echo), sends.send (Send::echo) + n);
                }
                now += n;
            }
        }

        // The sampler's routing: a busy voice playing the note's phrase continues; else a free voice starts.
        void route (const Kit& kit, const NoteOn& note, int offset)
        {
            for (auto& s : slots)
                if (s.busy && s.voice->kit() == &kit && s.voice->phrase() == note.phrase)
                {
                    s.voice->continueWith (note, offset);
                    return;
                }
            for (auto& s : slots)
                if (! s.busy)
                {
                    s.busy = true;
                    s.voice->start (kit, note, offset);
                    return;
                }
            FAIL ("no free phrase voice");
        }

        void stopAll (int offset, int fadeSamples)
        {
            for (auto& s : slots)
                if (s.busy)
                    s.voice->stop (offset, fadeSamples);
        }

        bool anyActive() const
        {
            return std::any_of (slots.begin(), slots.end(), [] (const Slot& s) { return s.voice->active(); });
        }

        PhraseVoice& voice (int i) { return *slots[static_cast<std::size_t> (i)].voice; }

        double sampleRate;
        std::int64_t now = 0;
        double tempo = 0.0; // the host's BPM the voices get (0: the kit's)
        bool collect = true;
        std::function<void (std::int64_t start, int n, const MixBus&, const SendBus&)> listen;
        MixBus bus;
        SendBus sends;
        std::vector<std::vector<float>> left;
        std::vector<std::vector<float>> right;
        std::vector<float> space;
        std::vector<float> echo;

    private:
        struct Slot
        {
            std::unique_ptr<PhraseVoice> voice;
            bool busy = false;
        };
        std::vector<Slot> slots;
    };

    // The slice notes of an annotated timeline as hits over `passes` passes of a loop at bpm and rate (a song:
    // passes 1), each with its NoteFlag::continues.
    inline std::vector<Hit> hitsOf (const Timeline& t, double bpm, double rate, int passes = 1)
    {
        std::vector<Hit> out;
        const auto loop = t.loops ? samplesAt (t.length, bpm, rate) : 0;
        for (int p = 0; p < passes; ++p)
            for (const auto& n : t.notes)
                if (n.slice >= 0)
                {
                    Hit h;
                    h.at = p * loop + samplesAt (n.pos, bpm, rate);
                    h.note = phraseNote (n.phrase, n.slice, (n.flags & NoteFlag::continues) != 0);
                    h.note.throwSpace = 0.0f;
                    out.push_back (h);
                }
        std::stable_sort (out.begin(), out.end(), [] (const Hit& a, const Hit& b) { return a.at < b.at; });
        return out;
    }

    // ------------------------------------------------------------------ measures

    // Bit for bit (the project warns on float ==), but for the sign of a zero: 0.0 + (-0.0) is 0.0 in a sum.
    inline bool same (float a, float b) noexcept
    {
        return std::bit_cast<std::uint32_t> (a) == std::bit_cast<std::uint32_t> (b)
               || (std::fpclassify (a) == FP_ZERO && std::fpclassify (b) == FP_ZERO);
    }

    inline bool sameBits (const std::vector<float>& a, const std::vector<float>& b)
    {
        return a.size() == b.size() && (a.empty() || std::memcmp (a.data(), b.data(), a.size() * sizeof (float)) == 0);
    }

    inline bool silent (const float* x, std::size_t n)
    {
        return std::all_of (x, x + n, [] (float v) { return same (v, 0.0f); });
    }

    // The largest |x[n] - x[n - 1]| over [from, to).
    inline double maxStep (const std::vector<float>& x, std::size_t from, std::size_t to)
    {
        double m = 0.0;
        for (std::size_t n = std::max<std::size_t> (from, 1); n < std::min (to, x.size()); ++n)
            m = std::max (m, std::fabs (static_cast<double> (x[n]) - static_cast<double> (x[n - 1])));
        return m;
    }

    // 10 log10 (sum (a - b)^2 / sum b^2) over [from, to).
    inline double nullDb (const std::vector<float>& a, const std::vector<double>& b, std::size_t from, std::size_t to)
    {
        double e = 0.0, s = 0.0;
        for (std::size_t n = from; n < to; ++n)
        {
            const double d = static_cast<double> (a[n]) - b[n];
            e += d * d;
            s += b[n] * b[n];
        }
        return 10.0 * std::log10 (e / s + 1e-300);
    }

    inline std::string sha256Planar (const std::vector<float>& l, const std::vector<float>& r, std::size_t from, std::size_t to)
    {
        std::vector<std::uint8_t> bytes ((to - from) * 8);
        std::memcpy (bytes.data(), l.data() + from, (to - from) * 4);
        std::memcpy (bytes.data() + (to - from) * 4, r.data() + from, (to - from) * 4);
        return wubansia::util::sha256Hex (bytes);
    }
}
