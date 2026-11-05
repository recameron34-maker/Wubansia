// Tests for MIDI drag-out (plugin/source/dragout/DragFiles.h) on the real content/midi files: the
// Loop and Song drags, lane drags, phrase drags, the file names and the drag cache folder.
//
// The kit files are loaded the way the plugin embeds them (EmbeddedContent, P9): every kit-prefixed
// .mid under content/midi/<kit>/, by file name. The kits' info mirrors their kit.json (P2) and
// tools/sound/channels.py's EXTRAS. Whatever a drag keeps is checked against the source chunks,
// which a small walker here finds on its own.

#include "dragout/DragFiles.h"

#include "kit/channel_map.h"
#include "midi/reader.h"
#include "util/sha256.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::EndsWith;
using Catch::Matchers::StartsWith;

namespace
{
    namespace dragout = wubansia::dragout;
    namespace engine = wubansia::engine;
    namespace midi = wubansia::midi;
    using wubansia::DragRequest;
    using wubansia::DragWhat;
    using engine::Lane;
    using engine::PlayMode;
    using engine::Variant;
    using Bytes = std::vector<std::uint8_t>;

    // ------------------------------------------------------------------ the checkout's files

    bool isCheckout (const juce::File& dir)
    {
        return dir.getChildFile ("content/midi/fut01").isDirectory() && dir.getChildFile ("tools/sound/denylists").isDirectory();
    }

    // The repository root: WUBANSIA_SOURCE_DIR from the environment, else up from this file's own
    // path (CMake compiles it by its absolute path), else up from the test executable (the presets
    // build inside the checkout).
    juce::File findCheckout()
    {
        const auto env = juce::SystemStats::getEnvironmentVariable ("WUBANSIA_SOURCE_DIR", {});
        if (env.isNotEmpty() && juce::File::isAbsolutePath (env) && isCheckout (juce::File (env)))
            return juce::File (env);
        juce::Array<juce::File> starts;
        if (juce::File::isAbsolutePath (__FILE__))
            starts.add (juce::File (__FILE__));
        starts.add (juce::File::getSpecialLocation (juce::File::currentExecutableFile));
        for (const auto& start : starts)
            for (auto dir = start.getParentDirectory(); dir != dir.getParentDirectory(); dir = dir.getParentDirectory())
                if (isCheckout (dir))
                    return dir;
        return {};
    }

    const juce::File& checkout()
    {
        static const juce::File root = findCheckout();
        return root;
    }

    Bytes loadBytes (const juce::File& file)
    {
        juce::MemoryBlock block;
        REQUIRE (file.loadFileAsData (block));
        const auto* p = static_cast<const std::uint8_t*> (block.getData());
        return Bytes (p, p + block.getSize());
    }

    // ------------------------------------------------------------------ the four kits

    engine::KitInfo makeInfo (std::string id, std::string title, std::int32_t bpm, std::string key, std::string camelot,
        std::array<std::string, engine::kLanes> labels, std::vector<engine::KitExtra> extras)
    {
        engine::KitInfo info;
        info.id = std::move (id);
        info.title = std::move (title);
        info.bpm = bpm;
        info.key = std::move (key);
        info.camelot = std::move (camelot);
        info.laneLabels = std::move (labels);
        info.extras = std::move (extras);
        info.contentVersion = 1;
        return info;
    }

    // KitExtra::channel counts from 0: MIDI channel 11 is 10.
    std::vector<engine::KitInfo> kitInfos()
    {
        return {
            makeInfo ("FUT01", "Alloy Signal", 134, "F# minor", "11A",
                { "Kick", "Clap", "Hats", "Perc", "Warp Bass", "Chrome Pluck", "Organ Stab", "Vox Synth", "Accents", "FX" },
                { { 10, "Organ Bass", Lane::bass }, { 11, "Organ Lead", Lane::lead }, { 12, "Air Pad", Lane::chords } }),
            makeInfo ("FUT02", "Orbit Relay", 130, "D minor", "7A",
                { "Kick", "Clap", "Hats", "Zap", "Roll Bass", "Chrome Arp", "Starfield Pad", "Vox Synth", "Accents", "FX" },
                { { 10, "Drop Glide", Lane::bass }, { 11, "Laser Chord Stab", Lane::chords }, { 12, "Glass Ping", Lane::accents } }),
            makeInfo ("FUT03", "Glass Current", 128, "G minor", "6A",
                { "Kick", "Clap", "Hats", "Perc", "Rubber Bass", "Keys EP", "Dub Stab", "Vox Synth", "Accents", "FX" },
                { { 10, "Pad Warm", Lane::chords } }),
            makeInfo ("FUT04", "Furnace Code", 136, "E minor", "9A",
                { "Kick", "Snare", "Hats", "Anvil", "Acid", "Mod Blip", "Drone", "Vox Synth", "Accents", "FX" },
                { { 10, "Acid Stab", Lane::accents }, { 11, "Metal Chord", Lane::chords }, { 12, "Rave Stab", Lane::accents } }),
        };
    }

    // One kit's embedded MIDI files.
    struct KitContent
    {
        engine::KitInfo info;
        std::vector<std::string> names;
        std::vector<Bytes> data;
        std::vector<dragout::MidiFile> files; // views of names and data

        const Bytes& bytesOf (const std::string& name) const
        {
            const auto it = std::find (names.begin(), names.end(), name);
            INFO ("no file " << name);
            REQUIRE (it != names.end());
            return data[static_cast<std::size_t> (it - names.begin())];
        }

        std::string prefix() const;
    };

    KitContent loadKit (engine::KitInfo info)
    {
        KitContent kit;
        kit.info = std::move (info);
        const auto folder = checkout().getChildFile ("content/midi").getChildFile (juce::String (kit.info.id).toLowerCase());
        auto found = folder.findChildFiles (juce::File::findFiles, true, juce::String (kit.info.id) + "_*.mid");
        std::sort (found.begin(), found.end(), [] (const juce::File& a, const juce::File& b) { return a.getFileName() < b.getFileName(); });
        for (const auto& f : found)
        {
            REQUIRE (std::find (kit.names.begin(), kit.names.end(), f.getFileName().toStdString()) == kit.names.end());
            kit.names.push_back (f.getFileName().toStdString());
            kit.data.push_back (loadBytes (f));
        }
        for (std::size_t i = 0; i < kit.names.size(); ++i)
            kit.files.push_back ({ kit.names[i], kit.data[i] });
        return kit;
    }

    const std::vector<KitContent>& kits()
    {
        static const std::vector<KitContent> all = [] {
            REQUIRE (checkout() != juce::File());
            std::vector<KitContent> out;
            for (auto& info : kitInfos())
                out.push_back (loadKit (std::move (info)));
            return out;
        }();
        return all;
    }

    const KitContent& kit (std::string_view id)
    {
        const auto& all = kits();
        const auto it = std::find_if (all.begin(), all.end(), [id] (const KitContent& k) { return k.info.id == id; });
        REQUIRE (it != all.end());
        return *it;
    }

    std::string KitContent::prefix() const
    {
        static const std::map<std::string, std::string> prefixes {
            { "FUT01", "FUT01 Alloy Signal 134 F#m 11A - " },
            { "FUT02", "FUT02 Orbit Relay 130 Dm 7A - " },
            { "FUT03", "FUT03 Glass Current 128 Gm 6A - " },
            { "FUT04", "FUT04 Furnace Code 136 Em 9A - " },
        };
        return prefixes.at (info.id);
    }

    // ------------------------------------------------------------------ SMF chunks, found independently

    struct Chunks
    {
        Bytes header; // the MThd chunk as it is
        std::vector<Bytes> tracks; // each MTrk chunk, whole
    };

    std::uint32_t be32 (const Bytes& b, std::size_t at)
    {
        return (std::uint32_t { b[at] } << 24) | (std::uint32_t { b[at + 1] } << 16) | (std::uint32_t { b[at + 2] } << 8) | std::uint32_t { b[at + 3] };
    }

    Chunks chunksOf (const Bytes& smf)
    {
        Chunks c;
        REQUIRE (smf.size() >= 14);
        const std::size_t headerSize = 8 + std::size_t { be32 (smf, 4) };
        c.header.assign (smf.begin(), smf.begin() + static_cast<std::ptrdiff_t> (headerSize));
        for (std::size_t pos = headerSize; pos + 8 <= smf.size();)
        {
            const std::size_t size = 8 + std::size_t { be32 (smf, pos + 4) };
            REQUIRE (pos + size <= smf.size());
            const auto begin = smf.begin() + static_cast<std::ptrdiff_t> (pos);
            if (std::equal (begin, begin + 4, "MTrk"))
                c.tracks.emplace_back (begin, begin + static_cast<std::ptrdiff_t> (size));
            pos += size;
        }
        return c;
    }

    int headerTrackCount (const Bytes& smf) { return (smf[10] << 8) | smf[11]; }

    midi::File read (const Bytes& smf)
    {
        auto r = midi::readSmf (smf);
        INFO (r.error);
        REQUIRE (r.ok());
        return r.value;
    }

    std::vector<std::string> trackNames (const Bytes& smf)
    {
        std::vector<std::string> names;
        for (const auto& t : read (smf).tracks)
            names.push_back (midi::trackName (t));
        return names;
    }

    // ------------------------------------------------------------------ drags

    DragRequest loopRequest() { return {}; }

    DragRequest songRequest()
    {
        DragRequest r;
        r.what = DragWhat::song;
        return r;
    }

    DragRequest laneRequest (Lane lane)
    {
        DragRequest r;
        r.what = DragWhat::lane;
        r.lane = lane;
        return r;
    }

    DragRequest phraseRequest (const std::string& file)
    {
        DragRequest r;
        r.what = DragWhat::phrase;
        r.phraseFile = juce::String (file);
        return r;
    }

    dragout::DragState state (PlayMode mode, Variant variant, std::uint32_t muted = 0)
    {
        dragout::DragState s;
        s.mode = mode;
        s.variant = variant;
        s.mutedLanes = muted;
        return s;
    }

    dragout::DragFile drag (const KitContent& k, const dragout::DragState& s, const DragRequest& r)
    {
        auto made = dragout::makeDragFile (k.info, k.files, s, r);
        INFO (made.error);
        REQUIRE (made.ok());
        return made.value;
    }

    std::string dragError (const KitContent& k, const dragout::DragState& s, const DragRequest& r)
    {
        const auto made = dragout::makeDragFile (k.info, k.files, s, r);
        CHECK_FALSE (made.ok());
        return made.error;
    }

    int laneOf (const KitContent& k, const std::string& trackName) { return engine::trackLane (k.info, trackName); }

    // Whether muted (a bit per lane) mutes a lane from laneOf (); -1, no lane, never is.
    bool isMuted (std::uint32_t muted, int lane) { return lane >= 0 && (muted & (1u << static_cast<unsigned> (lane))) != 0; }

    const char* variantName (Variant v) { return v == Variant::vox ? "vox" : "novox"; }

    constexpr std::array<Lane, engine::kLanes> allLanes { Lane::kick, Lane::snare, Lane::hats, Lane::perc, Lane::bass,
        Lane::lead, Lane::chords, Lane::vox, Lane::accents, Lane::fx };

    constexpr std::uint32_t everyLane = (1u << engine::kLanes) - 1;

    // The file a drag of `what` reads for a variant.
    std::string sourceName (const KitContent& k, DragWhat what, Variant v)
    {
        return what == DragWhat::song ? dragout::songFileName (k.info.id, v) : dragout::loopFileName (k.info.id, v);
    }

    // The source's chunks whose lane is `lane`.
    std::vector<Bytes> laneChunks (const KitContent& k, const Bytes& source, Lane lane)
    {
        const auto chunks = chunksOf (source);
        const auto names = trackNames (source);
        std::vector<Bytes> out;
        for (std::size_t i = 1; i < chunks.tracks.size(); ++i)
            if (laneOf (k, names[i]) == static_cast<int> (lane))
                out.push_back (chunks.tracks[i]);
        return out;
    }

    // ------------------------------------------------------------------ tools/sound/denylist.py, for ASCII text

    bool isLower (char c) { return c >= 'a' && c <= 'z'; }
    bool isUpper (char c) { return c >= 'A' && c <= 'Z'; }
    bool isDigit (char c) { return c >= '0' && c <= '9'; }
    bool isAlpha (char c) { return isLower (c) || isUpper (c); }

    // split_words: camelCase and ABCWord apart, then letters and digits apart, lowercase, apostrophes
    // dropped, anything but a-z and 0-9 between words.
    std::vector<std::string> splitWords (const std::string& text)
    {
        std::string camel;
        for (std::size_t i = 0; i < text.size(); ++i)
        {
            if (i > 0)
            {
                const char p = text[i - 1], c = text[i];
                if ((isLower (p) && isUpper (c)) || (isUpper (p) && isUpper (c) && i + 1 < text.size() && isLower (text[i + 1])))
                    camel += ' ';
            }
            camel += text[i];
        }
        std::string spaced;
        for (std::size_t i = 0; i < camel.size(); ++i)
        {
            if (i > 0 && ((isAlpha (camel[i - 1]) && isDigit (camel[i])) || (isDigit (camel[i - 1]) && isAlpha (camel[i]))))
                spaced += ' ';
            spaced += camel[i];
        }
        std::vector<std::string> words;
        std::string word;
        for (char c : spaced)
        {
            if (isUpper (c))
                c = static_cast<char> (c - 'A' + 'a');
            if (c == '\'')
                continue;
            if (isLower (c) || isDigit (c))
                word += c;
            else if (!word.empty())
            {
                words.push_back (word);
                word.clear();
            }
        }
        if (!word.empty())
            words.push_back (word);
        return words;
    }

    // grams: every run of 1-4 words, spaced and run together.
    std::set<std::string> grams (const std::string& text)
    {
        const auto w = splitWords (text);
        std::set<std::string> out;
        for (std::size_t i = 0; i < w.size(); ++i)
        {
            std::string spaced, joined;
            for (std::size_t j = i; j < std::min (w.size(), i + 4); ++j)
            {
                spaced += (j > i ? " " : "") + w[j];
                joined += w[j];
                out.insert (spaced);
                out.insert (joined);
            }
        }
        return out;
    }

    std::string digest (const std::string& gram)
    {
        const std::string salted = "wubansia-denylist-v1:" + gram;
        return wubansia::util::sha256Hex (std::span (reinterpret_cast<const std::uint8_t*> (salted.data()), salted.size()));
    }

    std::set<std::string> loadDenylist (const char* name)
    {
        const auto file = checkout().getChildFile ("tools/sound/denylists").getChildFile (juce::String (name) + ".sha256");
        REQUIRE (file.existsAsFile());
        juce::StringArray lines;
        file.readLines (lines);
        std::set<std::string> digests;
        for (const auto& line : lines)
        {
            const auto entry = line.upToFirstOccurrenceOf ("#", false, false).trim();
            if (entry.isNotEmpty())
                digests.insert (entry.toStdString());
        }
        REQUIRE (digests.size() > 10);
        return digests;
    }

    std::vector<std::string> denylistHits (const std::string& text, const std::set<std::string>& digests)
    {
        std::vector<std::string> hits;
        for (const auto& g : grams (text))
            if (digests.count (digest (g)) != 0)
                hits.push_back (g);
        return hits;
    }

    // ------------------------------------------------------------------ the cache folder

    // A new, empty folder under the temp folder, deleted when the test ends.
    struct TempFolder
    {
        juce::File folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getNonexistentChildFile ("WubansiaDragTest", "");

        TempFolder() { REQUIRE (folder.createDirectory()); }
        ~TempFolder() { folder.deleteRecursively(); }
        TempFolder (const TempFolder&) = delete;
        TempFolder& operator= (const TempFolder&) = delete;
    };

    juce::StringArray midNames (const juce::File& folder)
    {
        juce::StringArray names;
        for (const auto& f : folder.findChildFiles (juce::File::findFiles, false, "*.mid"))
            names.add (f.getFileName());
        names.sort (false);
        return names;
    }
}

TEST_CASE ("the drag tests find every kit's files in the checkout", "[dragout]")
{
    REQUIRE (checkout() != juce::File());
    REQUIRE (kits().size() == 4);
    for (const auto& k : kits())
    {
        INFO (k.info.id);
        CHECK (k.names.size() >= 15);
        for (const auto v : { Variant::vox, Variant::novox })
        {
            CHECK (read (k.bytesOf (dragout::loopFileName (k.info.id, v))).tracks.size() > 5);
            CHECK (read (k.bytesOf (dragout::songFileName (k.info.id, v))).tracks.size() > 5);
        }

        // Every track of the Loop and Song files plays on a lane: none is left out of every drag.
        for (const auto what : { DragWhat::loop, DragWhat::song })
            for (const auto v : { Variant::vox, Variant::novox })
            {
                const auto names = trackNames (k.bytesOf (sourceName (k, what, v)));
                for (std::size_t i = 1; i < names.size(); ++i)
                {
                    INFO (names[i]);
                    CHECK (laneOf (k, names[i]) >= 0);
                }
            }
    }
    CHECK (dragout::loopFileName ("FUT01", Variant::vox) == "FUT01_Loop_Vox_4bars.mid");
    CHECK (dragout::loopFileName ("FUT01", Variant::novox) == "FUT01_Loop_NoVox_4bars.mid");
    CHECK (dragout::songFileName ("FUT03", Variant::vox) == "FUT03_ExtendedMix_Vox_224bars.mid");
    CHECK (dragout::songFileName ("FUT03", Variant::novox) == "FUT03_ExtendedMix_NoVox_224bars.mid");
}

TEST_CASE ("a Loop or Song drag with nothing muted is the vox switch's file as it is", "[dragout]")
{
    for (const auto& k : kits())
        for (const auto what : { DragWhat::loop, DragWhat::song })
            for (const auto v : { Variant::vox, Variant::novox })
                for (const auto mode : { PlayMode::loop, PlayMode::song, PlayMode::off })
                {
                    INFO (k.info.id << " " << (what == DragWhat::song ? "song" : "loop") << " " << variantName (v));
                    DragRequest request;
                    request.what = what;
                    const auto file = drag (k, state (mode, v), request);
                    CHECK (file.bytes == k.bytesOf (sourceName (k, what, v)));
                    CHECK (file.name == k.prefix() + (what == DragWhat::song ? "Song" : "Loop") + (v == Variant::novox ? " NoVox" : "") + ".mid");
                }
}

TEST_CASE ("muted lanes are left out of Loop and Song drags, the rest byte for byte", "[dragout]")
{
    // Each lane alone, every lane but one, and a few mixes.
    std::vector<std::uint32_t> mutes { 0b0000000101u, 0b1010101010u, 0b0101010101u, 0b0110011001u };
    for (const auto lane : allLanes)
    {
        mutes.push_back (engine::laneBit (lane));
        mutes.push_back (everyLane & ~engine::laneBit (lane));
    }

    for (const auto& k : kits())
        for (const auto what : { DragWhat::loop, DragWhat::song })
            for (const auto v : { Variant::vox, Variant::novox })
            {
                const auto& source = k.bytesOf (sourceName (k, what, v));
                const auto sourceChunks = chunksOf (source);
                const auto sourceNames = trackNames (source);

                for (const auto muted : mutes)
                {
                    INFO (k.info.id << " " << (what == DragWhat::song ? "song" : "loop") << " " << variantName (v) << " muted " << muted);
                    std::vector<Bytes> expected { sourceChunks.tracks[0] };
                    std::vector<std::string> gone; // the muted lanes' labels that lose tracks, in lane order
                    for (std::size_t i = 1; i < sourceChunks.tracks.size(); ++i)
                    {
                        if (!isMuted (muted, laneOf (k, sourceNames[i])))
                            expected.push_back (sourceChunks.tracks[i]);
                    }
                    for (const auto lane : allLanes)
                        if ((muted & engine::laneBit (lane)) != 0 && !laneChunks (k, source, lane).empty())
                            gone.push_back (k.info.laneLabels[static_cast<std::size_t> (lane)]);

                    DragRequest request;
                    request.what = what;
                    if (expected.size() == 1)
                    {
                        // Every lane with tracks here is muted: there is nothing to drag.
                        CHECK_THAT (dragError (k, state (PlayMode::loop, v, muted), request), ContainsSubstring ("muted"));
                        continue;
                    }

                    const auto file = drag (k, state (PlayMode::loop, v, muted), request);
                    const auto cut = chunksOf (file.bytes);
                    CHECK (cut.tracks == expected);
                    CHECK (headerTrackCount (file.bytes) == static_cast<int> (expected.size()));
                    // The header is the source's but for the track count.
                    auto header = cut.header;
                    header[10] = sourceChunks.header[10];
                    header[11] = sourceChunks.header[11];
                    CHECK (header == sourceChunks.header);

                    const auto parsed = read (file.bytes);
                    REQUIRE (parsed.tracks.size() == expected.size());
                    for (std::size_t i = 1; i < parsed.tracks.size(); ++i)
                        CHECK_FALSE (isMuted (muted, laneOf (k, midi::trackName (parsed.tracks[i]))));

                    std::string name = k.prefix() + (what == DragWhat::song ? "Song" : "Loop") + (v == Variant::novox ? " NoVox" : "");
                    if (!gone.empty())
                    {
                        name += " (no ";
                        for (std::size_t i = 0; i < gone.size(); ++i)
                            name += (i > 0 ? ", " : "") + gone[i];
                        name += ")";
                    }
                    CHECK (file.name == name + ".mid");
                }
            }
}

TEST_CASE ("the vox switch picks the Loop and Song file", "[dragout]")
{
    const auto& k = kit ("FUT02");
    for (const auto what : { DragWhat::loop, DragWhat::song })
    {
        DragRequest request;
        request.what = what;
        const auto vox = drag (k, state (PlayMode::loop, Variant::vox), request);
        const auto novox = drag (k, state (PlayMode::loop, Variant::novox), request);
        const auto voxNames = trackNames (vox.bytes);
        const auto novoxNames = trackNames (novox.bytes);
        CHECK_THAT (voxNames[0], EndsWith (", Vox"));
        CHECK_THAT (novoxNames[0], EndsWith (", NoVox"));
        CHECK (std::count (voxNames.begin(), voxNames.end(), "Vox") == 1);
        CHECK (std::count (novoxNames.begin(), novoxNames.end(), "Vox") == 0);
        CHECK (vox.name != novox.name);
    }
}

TEST_CASE ("a lane drag holds the conductor and that lane's tracks from the current mode's file", "[dragout]")
{
    SECTION ("FUT01's bass in Song mode is the extended mix's Bass and Organ Bass (a kit extra)")
    {
        const auto& k = kit ("FUT01");
        const auto& song = k.bytesOf ("FUT01_ExtendedMix_Vox_224bars.mid");
        const auto file = drag (k, state (PlayMode::song, Variant::vox), laneRequest (Lane::bass));
        CHECK (trackNames (file.bytes) == std::vector<std::string> { "FUT01 Alloy Signal, 134 BPM, F# minor, extended mix, Vox", "Bass", "Organ Bass" });
        const auto cut = chunksOf (file.bytes);
        REQUIRE (cut.tracks.size() == 3);
        CHECK (cut.tracks[0] == chunksOf (song).tracks[0]);
        CHECK (std::vector<Bytes> (cut.tracks.begin() + 1, cut.tracks.end()) == laneChunks (k, song, Lane::bass));
        CHECK (headerTrackCount (file.bytes) == 3);
        CHECK (midi::markersOf (read (file.bytes)).size() == 9); // the extended mix's sections, from its conductor
        CHECK (file.name == "FUT01 Alloy Signal 134 F#m 11A - Warp Bass Song.mid");
    }

    SECTION ("FUT01's bass in Loop mode is the loop's Bass")
    {
        const auto& k = kit ("FUT01");
        const auto file = drag (k, state (PlayMode::loop, Variant::vox), laneRequest (Lane::bass));
        CHECK (trackNames (file.bytes) == std::vector<std::string> { "FUT01 Alloy Signal, 134 BPM, F# minor, loop, Vox", "Bass" });
        CHECK (file.name == "FUT01 Alloy Signal 134 F#m 11A - Warp Bass.mid");
    }

    SECTION ("FUT04's kick lane carries the Rumble; the chords lane, the chord voicing")
    {
        const auto& k = kit ("FUT04");
        CHECK (trackNames (drag (k, state (PlayMode::loop, Variant::vox), laneRequest (Lane::kick)).bytes)
               == std::vector<std::string> { "FUT04 Furnace Code, 136 BPM, E minor, loop, Vox", "Kick", "Rumble" });
        CHECK (trackNames (drag (k, state (PlayMode::loop, Variant::vox), laneRequest (Lane::chords)).bytes)
               == std::vector<std::string> { "FUT04 Furnace Code, 136 BPM, E minor, loop, Vox", "Chords", "Chord voicing" });
    }

    SECTION ("Off mode drags lanes from the Loop file, and mutes don't touch a lane drag")
    {
        const auto& k = kit ("FUT03");
        const auto loop = drag (k, state (PlayMode::loop, Variant::vox), laneRequest (Lane::chords));
        CHECK (drag (k, state (PlayMode::off, Variant::vox), laneRequest (Lane::chords)).bytes == loop.bytes);
        CHECK (drag (k, state (PlayMode::loop, Variant::vox, everyLane), laneRequest (Lane::chords)).bytes == loop.bytes);
        CHECK (trackNames (loop.bytes) == std::vector<std::string> { "FUT03 Glass Current, 128 BPM, G minor, loop, Vox", "Chords", "Chord voicing", "Pad Warm" });
    }

    SECTION ("every kit, mode, variant and lane")
    {
        for (const auto& k : kits())
            for (const auto mode : { PlayMode::loop, PlayMode::song })
                for (const auto v : { Variant::vox, Variant::novox })
                    for (const auto lane : allLanes)
                    {
                        INFO (k.info.id << " " << (mode == PlayMode::song ? "song" : "loop") << " " << variantName (v) << " lane " << static_cast<int> (lane));
                        const auto what = mode == PlayMode::song ? DragWhat::song : DragWhat::loop;
                        const auto& voxFile = k.bytesOf (sourceName (k, what, Variant::vox));
                        const auto& novoxFile = k.bytesOf (sourceName (k, what, Variant::novox));
                        const auto inVox = laneChunks (k, voxFile, lane);
                        const auto inNovox = laneChunks (k, novoxFile, lane);
                        if (inVox.empty() && inNovox.empty())
                        {
                            CHECK_THAT (dragError (k, state (mode, v), laneRequest (lane)), ContainsSubstring ("no tracks"));
                            continue;
                        }

                        // The vox switch's file, unless the lane is the same in both (then the vox file's,
                        // so one name is one file) or has no tracks there (the vox synth with the switch off).
                        const bool fromNovox = inVox.empty() || (v == Variant::novox && !inNovox.empty() && inNovox != inVox);
                        const auto& source = fromNovox ? novoxFile : voxFile;
                        const auto file = drag (k, state (mode, v), laneRequest (lane));
                        const auto cut = chunksOf (file.bytes);
                        REQUIRE (!cut.tracks.empty());
                        CHECK (cut.tracks[0] == chunksOf (source).tracks[0]);
                        CHECK (std::vector<Bytes> (cut.tracks.begin() + 1, cut.tracks.end()) == (fromNovox ? inNovox : inVox));
                        CHECK (headerTrackCount (file.bytes) == static_cast<int> (cut.tracks.size()));
                        CHECK (read (file.bytes).tracks.size() == cut.tracks.size());

                        const auto& label = k.info.laneLabels[static_cast<std::size_t> (lane)];
                        CHECK (file.name == k.prefix() + label + (mode == PlayMode::song ? " Song" : "") + (fromNovox ? " NoVox" : "") + ".mid");
                    }
    }
}

TEST_CASE ("the vox switch picks a lane's file only where the variants differ", "[dragout]")
{
    const auto& k = kit ("FUT02");
    const auto& voxLoop = k.bytesOf ("FUT02_Loop_Vox_4bars.mid");
    const auto& novoxLoop = k.bytesOf ("FUT02_Loop_NoVox_4bars.mid");

    // The accents differ: no vox gives the no-vox accents.
    const auto accents = drag (k, state (PlayMode::loop, Variant::novox), laneRequest (Lane::accents));
    CHECK (chunksOf (accents.bytes).tracks[1] == laneChunks (k, novoxLoop, Lane::accents).at (0));
    CHECK (chunksOf (accents.bytes).tracks[1] != laneChunks (k, voxLoop, Lane::accents).at (0));
    CHECK (accents.name == "FUT02 Orbit Relay 130 Dm 7A - Accents NoVox.mid");
    CHECK (drag (k, state (PlayMode::loop, Variant::vox), laneRequest (Lane::accents)).name == "FUT02 Orbit Relay 130 Dm 7A - Accents.mid");

    // The bass is the same in both: one file, whatever the switch.
    const auto bassNovox = drag (k, state (PlayMode::loop, Variant::novox), laneRequest (Lane::bass));
    const auto bassVox = drag (k, state (PlayMode::loop, Variant::vox), laneRequest (Lane::bass));
    CHECK (bassNovox.bytes == bassVox.bytes);
    CHECK (bassNovox.name == "FUT02 Orbit Relay 130 Dm 7A - Roll Bass.mid");

    // The vox synth's lane with the switch off still drags the vox line.
    const auto vox = drag (k, state (PlayMode::song, Variant::novox), laneRequest (Lane::vox));
    CHECK (trackNames (vox.bytes) == std::vector<std::string> { "FUT02 Orbit Relay, 130 BPM, D minor, extended mix, Vox", "Vox" });
    CHECK (vox.name == "FUT02 Orbit Relay 130 Dm 7A - Vox Synth Song.mid");
}

TEST_CASE ("a lane with no tracks in the mode's files can't be dragged", "[dragout]")
{
    // FUT03's loop has no lead and no FX; its extended mix has both.
    const auto& k = kit ("FUT03");
    CHECK_THAT (dragError (k, state (PlayMode::loop, Variant::vox), laneRequest (Lane::lead)), ContainsSubstring ("Keys EP") && ContainsSubstring ("no tracks"));
    CHECK_THAT (dragError (k, state (PlayMode::loop, Variant::novox), laneRequest (Lane::fx)), ContainsSubstring ("no tracks"));
    CHECK (trackNames (drag (k, state (PlayMode::song, Variant::vox), laneRequest (Lane::lead)).bytes).size() == 2);

    DragRequest bad = laneRequest (static_cast<Lane> (engine::kLanes));
    CHECK_THAT (dragError (k, state (PlayMode::loop, Variant::vox), bad), ContainsSubstring ("lane"));
}

TEST_CASE ("a phrase drag is the phrase file, byte for byte", "[dragout]")
{
    for (const auto& k : kits())
    {
        INFO (k.info.id);
        const auto phrases = dragout::phraseFiles (k.info.id, k.files);
        juce::StringArray expected;
        for (const auto& name : k.names)
            if (name.rfind (k.info.id + "_Phrase_", 0) == 0)
                expected.add (juce::String (name));
        CHECK (phrases == expected);
        CHECK (phrases.size() >= 4);

        for (const auto& name : phrases)
        {
            const auto file = drag (k, state (PlayMode::song, Variant::novox, everyLane), phraseRequest (name.toStdString()));
            CHECK (file.bytes == k.bytesOf (name.toStdString()));
            CHECK_THAT (file.name, StartsWith (k.prefix() + "Phrase ") && EndsWith (" bars.mid"));
        }
    }

    const auto& k = kit ("FUT01");
    CHECK (drag (k, state (PlayMode::loop, Variant::vox), phraseRequest ("FUT01_Phrase_EPianoStabRhythm_2bars.mid")).name
           == "FUT01 Alloy Signal 134 F#m 11A - Phrase E Piano Stab Rhythm 2 bars.mid");
    CHECK (drag (k, state (PlayMode::loop, Variant::vox), phraseRequest ("FUT01_Phrase_VoxSynthRiff_2bars.mid")).name
           == "FUT01 Alloy Signal 134 F#m 11A - Phrase Vox Synth Riff 2 bars.mid");
    CHECK_THAT (dragError (k, state (PlayMode::loop, Variant::vox), phraseRequest ("FUT01_Phrase_Nothing_2bars.mid")), ContainsSubstring ("FUT01_Phrase_Nothing_2bars.mid"));
    CHECK_THAT (dragError (k, state (PlayMode::loop, Variant::vox), phraseRequest ("FUT02_Phrase_GlassMelody_4bars.mid")), ContainsSubstring ("FUT02_Phrase_GlassMelody_4bars.mid"));
}

TEST_CASE ("embedded file names may carry folders", "[dragout]")
{
    const auto& k = kit ("FUT04");
    std::vector<std::string> paths;
    for (const auto& name : k.names)
        paths.push_back ("midi/fut04/" + name);
    std::vector<dragout::MidiFile> files;
    for (std::size_t i = 0; i < paths.size(); ++i)
        files.push_back ({ paths[i], k.data[i] });

    const auto loop = dragout::makeDragFile (k.info, files, state (PlayMode::loop, Variant::vox), loopRequest());
    REQUIRE (loop.ok());
    CHECK (loop.value.bytes == k.bytesOf ("FUT04_Loop_Vox_4bars.mid"));
    CHECK (dragout::phraseFiles (k.info.id, files) == dragout::phraseFiles (k.info.id, k.files));
    const auto phrase = dragout::makeDragFile (k.info, files, state (PlayMode::loop, Variant::vox), phraseRequest ("FUT04_Phrase_RaveStabRiff_2bars.mid"));
    REQUIRE (phrase.ok());
    CHECK (phrase.value.bytes == k.bytesOf ("FUT04_Phrase_RaveStabRiff_2bars.mid"));

    // Without the mode's file there is nothing to cut.
    const std::vector<dragout::MidiFile> none;
    CHECK_THAT (dragout::makeDragFile (k.info, none, state (PlayMode::loop, Variant::vox), loopRequest()).error, ContainsSubstring ("FUT04_Loop_Vox_4bars.mid"));
}

TEST_CASE ("a drag only ever takes the kit's own files", "[dragout]")
{
    // Every kit's files in one list: each kit still finds only its own.
    std::vector<dragout::MidiFile> all;
    for (const auto& k : kits())
        all.insert (all.end(), k.files.begin(), k.files.end());

    for (const auto& k : kits())
    {
        INFO (k.info.id);
        CHECK (dragout::phraseFiles (k.info.id, all) == dragout::phraseFiles (k.info.id, k.files));
        CHECK (drag ({ k.info, {}, {}, all }, state (PlayMode::song, Variant::novox), songRequest()).bytes == drag (k, state (PlayMode::song, Variant::novox), songRequest()).bytes);
        CHECK (drag ({ k.info, {}, {}, all }, state (PlayMode::loop, Variant::vox), laneRequest (Lane::bass)).bytes == drag (k, state (PlayMode::loop, Variant::vox), laneRequest (Lane::bass)).bytes);
    }

    const auto& fut01 = kit ("FUT01");
    CHECK_THAT (dragError ({ fut01.info, {}, {}, all }, state (PlayMode::loop, Variant::vox), phraseRequest ("FUT02_Phrase_GlassMelody_4bars.mid")),
        ContainsSubstring ("FUT02_Phrase_GlassMelody_4bars.mid"));
    CHECK (drag ({ fut01.info, {}, {}, all }, state (PlayMode::loop, Variant::vox), phraseRequest ("FUT01_Phrase_OrganRiff_4bars.mid")).bytes
           == fut01.bytesOf ("FUT01_Phrase_OrganRiff_4bars.mid"));
}

TEST_CASE ("drag file names hold kit, title, BPM, key and Camelot, in safe characters, and pass the denylists", "[dragout]")
{
    SECTION ("the denylist port agrees with tools/sound/denylist.py")
    {
        // Values printed by denylist.split_words, grams and digest.
        CHECK (splitWords ("FUT01 Alloy Signal 134 F#m 11A - Phrase E Piano Stab Rhythm 2 bars.mid")
               == std::vector<std::string> { "fut", "01", "alloy", "signal", "134", "f", "m", "11", "a", "phrase", "e", "piano", "stab", "rhythm", "2", "bars", "mid" });
        const auto g = grams ("EPianoStab 2bars");
        CHECK (std::vector<std::string> (g.begin(), std::next (g.begin(), 12))
               == std::vector<std::string> { "2", "2 bars", "2bars", "bars", "e", "e piano", "e piano stab", "e piano stab 2", "epiano", "epianostab", "epianostab2", "piano" });
        CHECK (digest ("alloy signal") == "0fa04877a993f87984b52c4609d6717f4a93605714ca11de66482168c4bbf286");
        CHECK (digest ("alloysignal") == "1c8d86fa48852838b8af53248ff56d611722460a3dae32f2936d1320b7f5c776");
        const std::set<std::string> list { digest ("warp bass song") };
        CHECK (denylistHits ("FUT01 Alloy Signal 134 F#m 11A - Warp Bass Song.mid", list) == std::vector<std::string> { "warp bass song" });
        CHECK (denylistHits ("FUT01 Alloy Signal 134 F#m 11A - Warp Bass.mid", list).empty());
    }

    // Every name the four kits give: Loop and Song in both variants under several mutes, every
    // lane in both modes and variants, every phrase file.
    std::vector<std::pair<std::string, const KitContent*>> named;
    std::map<std::string, Bytes> byName;
    const auto add = [&] (const KitContent& k, const dragout::DragState& s, const DragRequest& r) {
        const auto made = dragout::makeDragFile (k.info, k.files, s, r);
        if (!made.ok())
            return;
        named.emplace_back (made.value.name, &k);
        const auto it = byName.emplace (made.value.name, made.value.bytes).first;
        INFO (made.value.name);
        CHECK (it->second == made.value.bytes); // one name is one file
    };
    for (const auto& k : kits())
    {
        for (const auto v : { Variant::vox, Variant::novox })
        {
            for (const auto what : { DragWhat::loop, DragWhat::song })
            {
                DragRequest r;
                r.what = what;
                add (k, state (PlayMode::loop, v), r);
                add (k, state (PlayMode::loop, v, 0b1111100000u), r);
                for (const auto lane : allLanes)
                {
                    add (k, state (PlayMode::loop, v, engine::laneBit (lane)), r);
                    add (k, state (PlayMode::loop, v, everyLane & ~engine::laneBit (lane)), r);
                }
            }
            for (const auto mode : { PlayMode::loop, PlayMode::song })
                for (const auto lane : allLanes)
                    add (k, state (mode, v), laneRequest (lane));
        }
        for (const auto& name : dragout::phraseFiles (k.info.id, k.files))
            add (k, state (PlayMode::loop, Variant::vox), phraseRequest (name.toStdString()));
    }
    REQUIRE (named.size() > 400);

    const auto artists = loadDenylist ("artists");
    const auto slurs = loadDenylist ("slurs");
    const auto profanity = loadDenylist ("profanity");
    for (const auto& [name, k] : named)
    {
        INFO (name);
        CHECK_THAT (name, StartsWith (k->prefix()) && EndsWith (".mid"));
        CHECK (name.size() <= 200);
        CHECK (name.find ("  ") == std::string::npos);
        CHECK (name.back() != ' ');
        for (const char c : name)
            CHECK ((isAlpha (c) || isDigit (c) || std::string_view (" #(),-._").find (c) != std::string_view::npos));
        CHECK (denylistHits (name, artists).empty());
        CHECK (denylistHits (name, slurs).empty());
        CHECK (denylistHits (name, profanity).empty());
    }
}

TEST_CASE ("names spell the key short and keep only safe characters from the kit's strings", "[dragout]")
{
    const auto& source = kit ("FUT01");
    auto info = source.info;

    info.key = "Bb major";
    info.camelot = "6B";
    CHECK (drag ({ info, {}, {}, source.files }, state (PlayMode::loop, Variant::vox), loopRequest()).name == "FUT01 Alloy Signal 134 Bbmaj 6B - Loop.mid");
    info.key = "C# minor";
    info.camelot = "12A";
    CHECK (drag ({ info, {}, {}, source.files }, state (PlayMode::loop, Variant::vox), loopRequest()).name == "FUT01 Alloy Signal 134 C#m 12A - Loop.mid");
    info.key = "F# dorian";
    CHECK (drag ({ info, {}, {}, source.files }, state (PlayMode::loop, Variant::vox), loopRequest()).name == "FUT01 Alloy Signal 134 F# dorian 12A - Loop.mid");

    info.key = "F# minor";
    info.camelot = "11A";
    info.title = "../Night: \"Run\" *?<>|\\ Caf\xc3\xa9/x";
    info.laneLabels[4] = "Bass\tLine/2";
    const auto bass = drag ({ info, {}, {}, source.files }, state (PlayMode::loop, Variant::vox), laneRequest (Lane::bass));
    CHECK (bass.name == "FUT01 .. Night Run Caf x 134 F#m 11A - Bass Line 2.mid");

    // A lane without a label in the kit takes the lane's own name.
    info.laneLabels[4] = "";
    CHECK (drag ({ info, {}, {}, source.files }, state (PlayMode::loop, Variant::vox), laneRequest (Lane::bass)).name
           == "FUT01 .. Night Run Caf x 134 F#m 11A - Bass.mid");
}

TEST_CASE ("drag files are written to the cache folder, which keeps the newest 32", "[dragout]")
{
    const auto& k = kit ("FUT01");
    TempFolder temp;
    const auto folder = temp.folder.getChildFile ("Caches/drag"); // writeDragFile makes the folders

    SECTION ("a drag writes its file")
    {
        const auto made = drag (k, state (PlayMode::song, Variant::vox), laneRequest (Lane::bass));
        const auto written = dragout::writeDragFile (k.info, k.files, state (PlayMode::song, Variant::vox), laneRequest (Lane::bass), folder);
        INFO (written.error);
        REQUIRE (written.ok());
        CHECK (written.value == folder.getChildFile (juce::String (made.name)));
        CHECK (loadBytes (written.value) == made.bytes);

        // The same drag again gives the same file.
        const auto again = dragout::writeDragFile (k.info, k.files, state (PlayMode::song, Variant::vox), laneRequest (Lane::bass), folder);
        REQUIRE (again.ok());
        CHECK (again.value == written.value);
        CHECK (loadBytes (again.value) == made.bytes);
        CHECK (midNames (folder) == juce::StringArray { juce::String (made.name) });
    }

    SECTION ("the folder is trimmed to the newest 32 .mid files, the new one always among them")
    {
        REQUIRE (folder.createDirectory());
        const auto now = juce::Time::getCurrentTime();
        for (int i = 0; i < 40; ++i)
        {
            const auto old = folder.getChildFile ("old " + juce::String (i).paddedLeft ('0', 2) + ".mid");
            REQUIRE (old.replaceWithText ("x"));
            REQUIRE (old.setLastModificationTime (now - juce::RelativeTime::hours (100 - i)));
        }
        const auto other = folder.getChildFile ("notes.txt");
        REQUIRE (other.replaceWithText ("not ours"));
        REQUIRE (other.setLastModificationTime (now - juce::RelativeTime::hours (1000)));

        const auto written = dragout::writeDragFile (k.info, k.files, state (PlayMode::loop, Variant::vox), loopRequest(), folder);
        REQUIRE (written.ok());
        const auto names = midNames (folder);
        CHECK (names.size() == dragout::kKeepFiles);
        CHECK (names.contains (written.value.getFileName()));
        for (int i = 0; i < 40; ++i)
            CHECK (names.contains ("old " + juce::String (i).paddedLeft ('0', 2) + ".mid") == (i >= 40 - (dragout::kKeepFiles - 1)));
        CHECK (other.existsAsFile());

        // trimCache keeps the file it is told to keep, even when it is the oldest.
        const auto oldest = folder.getChildFile ("old 09.mid");
        REQUIRE (oldest.existsAsFile());
        dragout::trimCache (folder, 3, oldest);
        juce::StringArray left { "old 09.mid", "old 39.mid", written.value.getFileName() };
        left.sort (false);
        CHECK (midNames (folder) == left);
    }

    SECTION ("a drag that can't be made or written writes nothing and says why")
    {
        const auto failed = dragout::writeDragFile (k.info, k.files, state (PlayMode::loop, Variant::vox, everyLane), loopRequest(), folder);
        CHECK_FALSE (failed.ok());
        CHECK (failed.value == juce::File());
        CHECK_FALSE (folder.exists());

        const auto blocker = temp.folder.getChildFile ("file");
        REQUIRE (blocker.replaceWithText ("in the way"));
        const auto blocked = dragout::writeDragFile (k.info, k.files, state (PlayMode::loop, Variant::vox), loopRequest(), blocker.getChildFile ("drag"));
        CHECK_FALSE (blocked.ok());
        CHECK_THAT (blocked.error, ContainsSubstring ("drag"));
    }
}

TEST_CASE ("the drag cache folder is the user's caches folder, then Wubansia Audio/Wubansia/drag", "[dragout]")
{
    const auto folder = dragout::cacheFolder();
    CHECK (folder.getFileName() == "drag");
    CHECK (folder.getParentDirectory().getFileName() == "Wubansia");
    CHECK (folder.getParentDirectory().getParentDirectory().getFileName() == "Wubansia Audio");
    const auto caches = folder.getParentDirectory().getParentDirectory().getParentDirectory();
#if JUCE_MAC
    CHECK (caches == juce::File ("~/Library/Caches"));
#elif JUCE_LINUX || JUCE_BSD
    const auto xdg = juce::SystemStats::getEnvironmentVariable ("XDG_CACHE_HOME", {});
    CHECK (caches == (juce::File::isAbsolutePath (xdg) ? juce::File (xdg) : juce::File ("~/.cache")));
#elif JUCE_WINDOWS
    CHECK (caches == juce::File::getSpecialLocation (juce::File::windowsLocalAppData));
#endif
}

TEST_CASE ("a drag gives the same name and bytes every time", "[dragout]")
{
    for (const auto& k : kits())
    {
        const auto s = state (PlayMode::song, Variant::novox, engine::laneBit (Lane::hats) | engine::laneBit (Lane::fx));
        const auto a = drag (k, s, songRequest());
        const auto b = drag (k, s, songRequest());
        CHECK (a.name == b.name);
        CHECK (a.bytes == b.bytes);
    }
}
