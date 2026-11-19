// Tests for MIDI drag-out (plugin/source/dragout/DragFiles.h) on the real content/midi files: the
// Loop and Song drags, lane drags, phrase drags, the file names and the drag cache folder.
//
// The kit files are loaded the way the plugin embeds them (EmbeddedContent, P9): every kit-prefixed
// .mid under content/midi/<kit>/, by file name. The kits' info mirrors their kit.json (P2) and
// tools/sound/channels.py's EXTRAS; their chord tracks are the catalog's, read off the embedded
// kit.json (ADR 0027, chords as chords). Whatever a drag keeps is checked against the source chunks,
// which a small walker here finds on its own.

#include "content/EmbeddedContent.h"
#include "dragout/DragFiles.h"
#include "editor/DragChip.h"
#include "editor/ExternalDrag.h"
#include "editor/PhrasesPanel.h"
#include "editor/SamplesPanel.h"
#include "helpers/fake_controller.h"
#include "services/KitCatalog.h"

#include "kit/channel_map.h"
#include "kit/manifest.h"
#include "midi/for_synths.h"
#include "midi/reader.h"
#include "midi/track_select.h"
#include "util/sha256.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <map>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#if JUCE_LINUX
    #include <cerrno>
    #include <fcntl.h>
    #include <sys/wait.h>
    #include <unistd.h>
#endif

using Catch::Matchers::ContainsSubstring;
using Catch::Matchers::EndsWith;
using Catch::Matchers::StartsWith;

namespace
{
    namespace dragout = wubansia::dragout;
    namespace editor = wubansia::editor;
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
        // The tracks whose sample plays a whole chord, as the catalog reads them off the embedded kit.json.
        const auto manifest = engine::parseKitManifest (wubansia::EmbeddedContent::kitManifest (info.id));
        INFO (info.id << ": " << manifest.error);
        REQUIRE (manifest.ok());
        info.chordTracks = wubansia::kitInfoOf (manifest.value, 0).chordTracks;
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

    dragout::DragState state (PlayMode mode, Variant variant, std::uint32_t muted = 0, bool drumsOnOneTrack = false)
    {
        dragout::DragState s;
        s.mode = mode;
        s.variant = variant;
        s.mutedLanes = muted;
        s.drumsOnOneTrack = drumsOnOneTrack;
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

    // The tracks 1 drum track puts on one track (ADR 0025): the names with the drums role in channel_map.h's kTracks.
    bool isDrumTrack (const std::string& name)
    {
        static const std::set<std::string> drums { "Kick", "Rumble", "Snare", "Clap", "Snare/Clap", "Hats", "Perc" };
        return drums.count (name) != 0;
    }

    // The kit's chord track of that name with zones (ADR 0027, chords as chords), or none.
    const engine::KitChordTrack* chordTrackOf (const KitContent& k, const std::string& name)
    {
        for (const auto& t : k.info.chordTracks)
            if (t.name == name && ! t.zones.empty())
                return &t;
        return nullptr;
    }

    midi::ChordSample sampleOf (const engine::KitChordTrack& track)
    {
        midi::ChordSample sample;
        for (const auto& z : track.zones)
            sample.push_back ({ z.lo, z.hi, z.intervals });
        return sample;
    }

    // For my synths (ADR 0027), restated: drums and one-key sounds (accents, FX, vox chops) stay as they are, and so does a
    // track no lane claims (the merged Drums); the bass lane's tracks (Bass, FUT01's Organ Bass, FUT02's Drop Glide) go up
    // an octave; a chord sample's track (the kit's chord tracks) is written as its chords; every other track (lead, chord
    // voicing, vox, the other extras) is made for synths. plain: chord tracks as pitched tracks, the two-argument rule.
    midi::SynthPart synthPartOf (const KitContent& k, const std::string& name, bool plain = false)
    {
        static const std::set<std::string> asIs { "Kick", "Rumble", "Snare", "Clap", "Snare/Clap", "Hats", "Perc", "Accents", "FX", "Vox chops" };
        const auto lane = laneOf (k, name);
        if (asIs.count (name) != 0 || lane < 0)
            return midi::SynthPart::asIs;
        if (! plain && chordTrackOf (k, name) != nullptr)
            return midi::SynthPart::chords;
        return lane == static_cast<int> (Lane::bass) ? midi::SynthPart::bass : midi::SynthPart::pitched;
    }

    // Each of a file's tracks as For my synths takes it, the conductor as it is.
    std::vector<midi::SynthPart> synthPartsOf (const KitContent& k, const Bytes& smf, bool plain = false)
    {
        const auto names = trackNames (smf);
        std::vector<midi::SynthPart> parts;
        for (std::size_t i = 0; i < names.size(); ++i)
            parts.push_back (i == 0 ? midi::SynthPart::asIs : synthPartOf (k, names[i], plain));
        return parts;
    }

    // Each of a file's tracks' chord sample: a chord track's, else none.
    std::vector<midi::ChordSample> chordSamplesOf (const KitContent& k, const Bytes& smf)
    {
        std::vector<midi::ChordSample> samples;
        for (const auto& name : trackNames (smf))
        {
            const auto* track = chordTrackOf (k, name);
            samples.push_back (track != nullptr ? sampleOf (*track) : midi::ChordSample {});
        }
        return samples;
    }

    bool anyForSynths (const std::vector<midi::SynthPart>& parts)
    {
        return std::any_of (parts.begin(), parts.end(), [] (midi::SynthPart p) { return p != midi::SynthPart::asIs; });
    }

    // s with For my synths lit.
    dragout::DragState forSynths (dragout::DragState s)
    {
        s.forSynths = true;
        return s;
    }

    // A name with "synths" first among its marks: "... - Loop.mid" is "... - Loop (synths).mid", and
    // "... - Song (1 drum track).mid" is "... - Song (synths, 1 drum track).mid". (No kit's label has a bracket.)
    std::string withSynthsMark (const std::string& name)
    {
        const auto stem = name.substr (0, name.size() - 4);
        if (! stem.empty() && stem.back() == ')')
        {
            const auto open = stem.rfind (" (");
            return stem.substr (0, open) + " (synths, " + stem.substr (open + 2) + ".mid";
        }
        return stem + " (synths).mid";
    }

    // Notes by tick, channel and key, as notesOf () gives one track's.
    std::vector<midi::Note> byTick (std::vector<midi::Note> notes)
    {
        std::stable_sort (notes.begin(), notes.end(), [] (const midi::Note& a, const midi::Note& b) {
            if (a.tick != b.tick)
                return a.tick < b.tick;
            return a.channel != b.channel ? a.channel < b.channel : a.key < b.key;
        });
        return notes;
    }

    // Whether every note-off on a tick comes before every note-on on it (docs/midi-map.md, "On a tick").
    bool offsBeforeOns (const midi::Track& track)
    {
        std::int64_t tick = -1;
        bool onSeen = false;
        for (const auto& e : track.events)
        {
            if (e.tick != tick)
            {
                tick = e.tick;
                onSeen = false;
            }
            if (e.isNoteOn())
                onSeen = true;
            else if (e.isNoteOff() && onSeen)
                return false;
        }
        return true;
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

    // ------------------------------------------------------------------ parts (ADR 0029)

    using wubansia::DragPart;

    // Every part but All, in the drag strip's order.
    constexpr std::array<DragPart, 6> partsButAll { DragPart::drums, DragPart::bass, DragPart::chords, DragPart::lead, DragPart::vox, DragPart::accents };

    std::string partWord (DragPart part) { return std::string (wubansia::kDragPartNames[static_cast<std::size_t> (part)]); }

    // A part's lanes, restated: Drums the kick, snare, hats and perc lanes; Accents the accents and FX lanes; Bass, Chords,
    // Lead and Vox their own; All every lane.
    std::uint32_t lanesOf (DragPart part)
    {
        using engine::laneBit;
        switch (part)
        {
            case DragPart::all: return everyLane;
            case DragPart::drums: return laneBit (Lane::kick) | laneBit (Lane::snare) | laneBit (Lane::hats) | laneBit (Lane::perc);
            case DragPart::bass: return laneBit (Lane::bass);
            case DragPart::chords: return laneBit (Lane::chords);
            case DragPart::lead: return laneBit (Lane::lead);
            case DragPart::vox: return laneBit (Lane::vox);
            case DragPart::accents: return laneBit (Lane::accents) | laneBit (Lane::fx);
        }
        return 0;
    }

    // s with the drag strip's Part on part.
    dragout::DragState withPart (dragout::DragState s, DragPart part)
    {
        s.part = part;
        return s;
    }

    // The source's chunks whose lane is one of lanes (a bit per Lane), in file order.
    std::vector<Bytes> partChunks (const KitContent& k, const Bytes& source, std::uint32_t lanes)
    {
        const auto chunks = chunksOf (source);
        const auto names = trackNames (source);
        std::vector<Bytes> out;
        for (std::size_t i = 1; i < chunks.tracks.size(); ++i)
            if (const auto lane = laneOf (k, names[i]); lane >= 0 && (lanes & (1u << static_cast<unsigned> (lane))) != 0)
                out.push_back (chunks.tracks[i]);
        return out;
    }

    const char* whatName (DragWhat what) { return what == DragWhat::song ? "song" : "loop"; }

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

TEST_CASE ("the Loop and Song files' drum tracks follow the conductor, on channel 10, notes only, no key shared", "[dragout]")
{
    // What 1 drum track relies on: the merged track takes the first drum track's place, right after the conductor.
    for (const auto& k : kits())
        for (const auto what : { DragWhat::loop, DragWhat::song })
            for (const auto v : { Variant::vox, Variant::novox })
            {
                INFO (k.info.id << " " << (what == DragWhat::song ? "song" : "loop") << " " << variantName (v));
                const auto file = read (k.bytesOf (sourceName (k, what, v)));
                std::vector<std::size_t> drums;
                for (std::size_t i = 1; i < file.tracks.size(); ++i)
                    if (isDrumTrack (midi::trackName (file.tracks[i])))
                        drums.push_back (i);
                REQUIRE (drums.size() >= 4);
                for (std::size_t j = 0; j < drums.size(); ++j)
                    CHECK (drums[j] == j + 1);

                std::map<int, std::string> keyTrack; // each key's drum track
                for (const auto i : drums)
                {
                    const auto name = midi::trackName (file.tracks[i]);
                    for (const auto& e : file.tracks[i].events)
                    {
                        if (! e.isChannel())
                            continue;
                        INFO (name << " event at tick " << e.tick);
                        CHECK (e.channel() == 9);
                        CHECK ((e.kind() == midi::status::noteOn || e.kind() == midi::status::noteOff)); // no CC, program change or bend
                        const auto owner = keyTrack.emplace (e.data1, name).first->second;
                        CHECK (owner == name);
                    }
                }
            }
}

TEST_CASE ("1 drum track: a Loop or Song drag's kept drum tracks become one track, Drums, the rest byte for byte", "[dragout]")
{
    // Nothing muted, each lane alone, every lane but one, and a few mixes: the muted lanes' tracks are gone first.
    std::vector<std::uint32_t> mutes { 0u, 0b0000000101u, 0b1010101010u, 0b0101010101u, 0b0110011001u };
    for (const auto lane : allLanes)
    {
        mutes.push_back (engine::laneBit (lane));
        mutes.push_back (everyLane & ~engine::laneBit (lane));
    }

    int merged = 0, plain = 0;
    for (const auto& k : kits())
        for (const auto what : { DragWhat::loop, DragWhat::song })
            for (const auto v : { Variant::vox, Variant::novox })
            {
                const auto& source = k.bytesOf (sourceName (k, what, v));
                const auto sourceChunks = chunksOf (source);
                const auto sourceNames = trackNames (source);
                const auto sourceFile = read (source);

                for (const auto muted : mutes)
                {
                    INFO (k.info.id << " " << (what == DragWhat::song ? "song" : "loop") << " " << variantName (v) << " muted " << muted);
                    DragRequest request;
                    request.what = what;
                    const auto off = dragout::makeDragFile (k.info, k.files, state (PlayMode::loop, v, muted), request);
                    const auto on = dragout::makeDragFile (k.info, k.files, state (PlayMode::loop, v, muted, true), request);

                    std::vector<std::size_t> keptDrums, keptOthers; // in file order
                    for (std::size_t i = 1; i < sourceChunks.tracks.size(); ++i)
                        if (! isMuted (muted, laneOf (k, sourceNames[i])))
                            (isDrumTrack (sourceNames[i]) ? keptDrums : keptOthers).push_back (i);

                    if (keptDrums.size() < 2)
                    {
                        // Nothing to merge: the drag without the switch, name and bytes, or its error.
                        ++plain;
                        CHECK (on.ok() == off.ok());
                        CHECK (on.error == off.error);
                        CHECK (on.value.name == off.value.name);
                        CHECK (on.value.bytes == off.value.bytes);
                        continue;
                    }
                    INFO (on.error);
                    REQUIRE (on.ok());
                    REQUIRE (off.ok());
                    ++merged;

                    // The conductor, the merged track, then every other kept track byte for byte, in file order.
                    const auto cut = chunksOf (on.value.bytes);
                    REQUIRE (cut.tracks.size() == 2 + keptOthers.size());
                    CHECK (cut.tracks[0] == sourceChunks.tracks[0]);
                    for (std::size_t j = 0; j < keptOthers.size(); ++j)
                        CHECK (cut.tracks[2 + j] == sourceChunks.tracks[keptOthers[j]]);
                    CHECK (headerTrackCount (on.value.bytes) == static_cast<int> (cut.tracks.size()));
                    auto header = cut.header; // the source's but for the track count
                    header[10] = sourceChunks.header[10];
                    header[11] = sourceChunks.header[11];
                    CHECK (header == sourceChunks.header);

                    // Drums: every kept drum track's notes as they were, on channel 10, and nothing else but its name.
                    const auto parsed = read (on.value.bytes);
                    REQUIRE (parsed.tracks.size() == cut.tracks.size());
                    const auto& drums = parsed.tracks[1];
                    CHECK (midi::trackName (drums) == "Drums");
                    std::vector<midi::Note> notes;
                    for (const auto i : keptDrums)
                    {
                        const auto n = midi::notesOf (sourceFile.tracks[i]);
                        notes.insert (notes.end(), n.begin(), n.end());
                    }
                    CHECK (midi::notesOf (drums) == byTick (notes));
                    int names = 0, others = 0;
                    for (const auto& e : drums.events)
                    {
                        if (e.isMeta (midi::meta::trackName))
                            ++names;
                        else if (e.status != 0x99 && e.status != 0x89)
                            ++others;
                    }
                    CHECK (names == 1);
                    CHECK (others == 0);
                    CHECK (drums.endTick == sourceFile.tracks[0].endTick);
                    CHECK (offsBeforeOns (drums));

                    // The name: "1 drum track" first among the marks, then the muted lanes as without the switch.
                    std::string gone; // the muted lanes' labels that lose tracks, in lane order
                    for (const auto lane : allLanes)
                        if ((muted & engine::laneBit (lane)) != 0 && ! laneChunks (k, source, lane).empty())
                            gone += (gone.empty() ? "" : ", ") + k.info.laneLabels[static_cast<std::size_t> (lane)];
                    const auto stem = k.prefix() + (what == DragWhat::song ? "Song" : "Loop") + (v == Variant::novox ? " NoVox" : "");
                    CHECK (on.value.name == stem + " (1 drum track" + (gone.empty() ? "" : ", no " + gone) + ").mid");
                    CHECK (off.value.name == stem + (gone.empty() ? "" : " (no " + gone + ")") + ".mid");
                }
            }
    CHECK (merged > 100);
    CHECK (plain > 50);
}

TEST_CASE ("1 drum track's file names, spelled out", "[dragout]")
{
    const auto name = [] (std::string_view id, DragWhat what, Variant v, std::uint32_t muted) {
        DragRequest request;
        request.what = what;
        return drag (kit (id), state (PlayMode::loop, v, muted, true), request).name;
    };
    CHECK (name ("FUT01", DragWhat::song, Variant::vox, 0) == "FUT01 Alloy Signal 134 F#m 11A - Song (1 drum track).mid");
    CHECK (name ("FUT01", DragWhat::song, Variant::novox, engine::laneBit (Lane::snare) | engine::laneBit (Lane::bass))
           == "FUT01 Alloy Signal 134 F#m 11A - Song NoVox (1 drum track, no Clap, Warp Bass).mid");
    CHECK (name ("FUT04", DragWhat::loop, Variant::vox, engine::laneBit (Lane::perc)) == "FUT04 Furnace Code 136 Em 9A - Loop (1 drum track, no Anvil).mid");
    // Only Zap is left: one drum track, so the plain file.
    CHECK (name ("FUT02", DragWhat::song, Variant::vox, engine::laneBit (Lane::kick) | engine::laneBit (Lane::snare) | engine::laneBit (Lane::hats))
           == "FUT02 Orbit Relay 130 Dm 7A - Song (no Kick, Clap, Hats).mid");
}

TEST_CASE ("1 drum track gives the vox and no-vox files the same Drums track", "[dragout]")
{
    for (const auto& k : kits())
        for (const auto what : { DragWhat::loop, DragWhat::song })
        {
            INFO (k.info.id << " " << (what == DragWhat::song ? "song" : "loop"));
            DragRequest request;
            request.what = what;
            const auto vox = drag (k, state (PlayMode::loop, Variant::vox, 0, true), request);
            const auto novox = drag (k, state (PlayMode::loop, Variant::novox, 0, true), request);
            CHECK (trackNames (vox.bytes)[1] == "Drums");
            CHECK (chunksOf (vox.bytes).tracks[1] == chunksOf (novox.bytes).tracks[1]);
            CHECK (vox.name != novox.name);
        }
}

TEST_CASE ("1 drum track leaves lane and phrase drags as they are", "[dragout]")
{
    for (const auto& k : kits())
        for (const auto mode : { PlayMode::loop, PlayMode::song, PlayMode::off })
            for (const auto v : { Variant::vox, Variant::novox })
            {
                INFO (k.info.id << " mode " << static_cast<int> (mode) << " " << variantName (v));
                std::vector<DragRequest> requests;
                for (const auto lane : allLanes)
                    requests.push_back (laneRequest (lane));
                for (const auto& phrase : dragout::kitPhraseFiles (k.info.id, k.files))
                    requests.push_back (phraseRequest (phrase.toStdString()));
                for (const auto& request : requests)
                {
                    INFO ("lane " << static_cast<int> (request.lane) << " phrase " << request.phraseFile);
                    const auto off = dragout::makeDragFile (k.info, k.files, state (mode, v), request);
                    const auto on = dragout::makeDragFile (k.info, k.files, state (mode, v, 0, true), request);
                    CHECK (on.ok() == off.ok());
                    CHECK (on.error == off.error);
                    CHECK (on.value.name == off.value.name);
                    CHECK (on.value.bytes == off.value.bytes);
                }
            }

    // FUT04's kick lane keeps its Kick and Rumble apart.
    CHECK (trackNames (drag (kit ("FUT04"), state (PlayMode::loop, Variant::vox, 0, true), laneRequest (Lane::kick)).bytes)
           == std::vector<std::string> { "FUT04 Furnace Code, 136 BPM, E minor, loop, Vox", "Kick", "Rumble" });
}

TEST_CASE ("1 drum track never merges a kit's extras, whatever their lane", "[dragout]")
{
    // The merge takes the tracks whose names have the drums role in kTracks, never a kit's extras (channels 11-16), even
    // on a drum lane: a later kit may put one there. So with every extra moved onto one drum lane, and nothing muted, a
    // Loop or Song drag is the same file as with the extras on their own lanes.
    int extrasInLoops = 0, extrasInSongs = 0;
    for (const auto& k : kits())
        for (const auto lane : { Lane::kick, Lane::snare, Lane::hats, Lane::perc })
        {
            auto moved = k.info;
            for (auto& extra : moved.extras)
                extra.lane = lane;
            for (const auto what : { DragWhat::loop, DragWhat::song })
                for (const auto v : { Variant::vox, Variant::novox })
                {
                    INFO (k.info.id << "'s extras on lane " << static_cast<int> (lane) << ", " << (what == DragWhat::song ? "song" : "loop")
                                    << " " << variantName (v));
                    const auto names = trackNames (k.bytesOf (sourceName (k, what, v)));
                    const auto extras = std::count_if (names.begin(), names.end(), [&k] (const std::string& name) {
                        return std::any_of (k.info.extras.begin(), k.info.extras.end(), [&name] (const engine::KitExtra& e) { return e.name == name; });
                    });
                    (what == DragWhat::song ? extrasInSongs : extrasInLoops) += static_cast<int> (extras);

                    DragRequest request;
                    request.what = what;
                    const auto s = state (PlayMode::loop, v, 0, true);
                    const auto made = dragout::makeDragFile (moved, k.files, s, request);
                    INFO (made.error);
                    REQUIRE (made.ok());
                    const auto expected = drag (k, s, request);
                    CHECK (made.value.name == expected.name);
                    CHECK (made.value.bytes == expected.bytes);
                }
        }
    CHECK (extrasInLoops > 0); // FUT02's Drop Glide and FUT03's Pad Warm play in the Loops
    CHECK (extrasInSongs > extrasInLoops);
}

TEST_CASE ("1 drum track: drums that can't go on one track fail the drag, saying why", "[dragout]")
{
    // FUT01's vox Loop with a pitch bend at the head of its Clap track (track 2), which a merged track can't carry.
    auto patched = kit ("FUT01");
    const auto loopName = dragout::loopFileName ("FUT01", Variant::vox);
    const auto at = std::find (patched.names.begin(), patched.names.end(), loopName);
    REQUIRE (at != patched.names.end());
    auto& bytes = patched.data[static_cast<std::size_t> (at - patched.names.begin())];
    const auto chunks = midi::trackChunks (bytes);
    REQUIRE (chunks.ok());
    REQUIRE (chunks.value.size() > 2);
    REQUIRE (chunks.value[2].name == "Clap");
    const auto chunk = chunks.value[2].offset;
    const Bytes bend { 0x00, 0xE9, 0x00, 0x40 }; // at tick 0, channel 10
    bytes.insert (bytes.begin() + static_cast<std::ptrdiff_t> (chunk + 8), bend.begin(), bend.end()); // after "MTrk" and the length
    const auto length = be32 (bytes, chunk + 4) + static_cast<std::uint32_t> (bend.size());
    for (std::size_t i = 0; i < 4; ++i)
        bytes[chunk + 4 + i] = static_cast<std::uint8_t> (length >> (24 - 8 * i));
    // The copy's file views still point at the shared kit's bytes: point them at the copy's.
    patched.files.clear();
    for (std::size_t i = 0; i < patched.names.size(); ++i)
        patched.files.push_back ({ patched.names[i], patched.data[i] });

    CHECK (dragError (patched, state (PlayMode::loop, Variant::vox, 0, true), loopRequest())
           == "FUT01_Loop_Vox_4bars.mid: can't put the drums on one track: can't merge tracks 1, 2, 3, 4: track 2 \"Clap\" has a pitch "
              "bend at tick 0; a merged track carries only notes, controllers, program changes and text events");

    // The file itself is fine: with the switch off, the drag is the patched file as it is.
    const auto off = drag (patched, state (PlayMode::loop, Variant::vox), loopRequest());
    CHECK (off.name == patched.prefix() + "Loop.mid");
    CHECK (off.bytes == bytes);
}

TEST_CASE ("For my synths: every drag is the plain drag's file with its pitched tracks made for synths, and says so", "[dragout]")
{
    // Loop and Song: nothing muted, each lane alone, every lane but one, and only the drums, accents and FX left, each
    // with 1 drum track off and on. Then every lane in both modes, and every phrase file and lead line, each in both
    // variants.
    std::vector<std::uint32_t> mutes { 0u, 0b0011110000u };
    for (const auto lane : allLanes)
    {
        mutes.push_back (engine::laneBit (lane));
        mutes.push_back (everyLane & ~engine::laneBit (lane));
    }

    int marked = 0, changed = 0, plain = 0;
    for (const auto& k : kits())
    {
        std::vector<std::pair<dragout::DragState, DragRequest>> drags;
        for (const auto v : { Variant::vox, Variant::novox })
        {
            for (const auto what : { DragWhat::loop, DragWhat::song })
                for (const auto muted : mutes)
                    for (const bool drums : { false, true })
                    {
                        DragRequest r;
                        r.what = what;
                        drags.emplace_back (state (PlayMode::loop, v, muted, drums), r);
                    }
            for (const auto mode : { PlayMode::loop, PlayMode::song })
                for (const auto lane : allLanes)
                    drags.emplace_back (state (mode, v), laneRequest (lane));
            for (const auto& phrase : dragout::kitPhraseFiles (k.info.id, k.files))
                drags.emplace_back (state (PlayMode::loop, v), phraseRequest (phrase.toStdString()));
        }

        for (const auto& [s, r] : drags)
        {
            INFO (k.info.id << " what " << static_cast<int> (r.what) << " lane " << static_cast<int> (r.lane) << " " << r.phraseFile << " mode "
                            << static_cast<int> (s.mode) << " " << variantName (s.variant) << " muted " << s.mutedLanes << " drums " << s.drumsOnOneTrack);
            const auto off = dragout::makeDragFile (k.info, k.files, s, r);
            const auto on = dragout::makeDragFile (k.info, k.files, forSynths (s), r);
            if (! off.ok())
            {
                CHECK_FALSE (on.ok());
                CHECK (on.error == off.error);
                continue;
            }
            INFO (on.error);
            REQUIRE (on.ok());

            // Nothing for a synth (drums, accents and FX alone): the plain drag, name and bytes.
            const auto parts = synthPartsOf (k, off.value.bytes);
            if (! anyForSynths (parts))
            {
                ++plain;
                CHECK (on.value.name == off.value.name);
                CHECK (on.value.bytes == off.value.bytes);
                continue;
            }
            ++marked;
            CHECK (on.value.name == withSynthsMark (off.value.name));
            const auto expected = midi::forSynths (off.value.bytes, parts, chordSamplesOf (k, off.value.bytes));
            INFO (expected.error);
            REQUIRE (expected.ok());
            CHECK (on.value.bytes == expected.value);
            if (on.value.bytes != off.value.bytes)
                ++changed; // a pitched track whose notes already play well on a synth comes out the same
        }
    }
    CHECK (marked > 500); // 584: 572 changed, 12 already played well
    CHECK (changed > 500);
    CHECK (plain > 250); // 294
}

TEST_CASE ("For my synths moves the bass lane up an octave, plays every pitched track's notes apart, and leaves the drums", "[dragout]")
{
    for (const auto& k : kits())
        for (const auto what : { DragWhat::loop, DragWhat::song })
            for (const auto v : { Variant::vox, Variant::novox })
            {
                INFO (k.info.id << " " << (what == DragWhat::song ? "song" : "loop") << " " << variantName (v));
                DragRequest request;
                request.what = what;
                const auto off = drag (k, state (PlayMode::loop, v), request);
                const auto on = drag (k, forSynths (state (PlayMode::loop, v)), request);
                const auto names = trackNames (off.bytes);
                REQUIRE (trackNames (on.bytes) == names);
                const auto offChunks = chunksOf (off.bytes);
                const auto onChunks = chunksOf (on.bytes);
                const auto offFile = read (off.bytes);
                const auto onFile = read (on.bytes);
                CHECK (onChunks.header == offChunks.header);
                CHECK (onChunks.tracks[0] == offChunks.tracks[0]); // the conductor: tempo, key, markers

                for (std::size_t i = 1; i < names.size(); ++i)
                {
                    INFO (names[i]);
                    const auto lane = laneOf (k, names[i]);
                    if (isDrumTrack (names[i]) || names[i] == "Accents" || names[i] == "FX")
                    {
                        CHECK (onChunks.tracks[i] == offChunks.tracks[i]);
                        continue;
                    }
                    if (chordTrackOf (k, names[i]) != nullptr)
                        continue; // a chord sample's track: each note its chord (the chords as chords cases)
                    const auto before = midi::notesOf (offFile.tracks[i]);
                    const auto after = midi::notesOf (onFile.tracks[i]);
                    REQUIRE (after.size() == before.size());
                    CHECK (midi::controllersOf (onFile.tracks[i]) == midi::controllersOf (offFile.tracks[i]));
                    std::set<std::int64_t> onsets;
                    for (const auto& n : after)
                        onsets.insert (n.tick);
                    for (std::size_t j = 0; j < after.size(); ++j)
                    {
                        CHECK (after[j].tick == before[j].tick);
                        CHECK (int { after[j].key } == before[j].key + (lane == static_cast<int> (Lane::bass) ? 12 : 0));
                        CHECK (after[j].velocity >= 72);
                        const auto next = onsets.upper_bound (after[j].tick);
                        if (next != onsets.end())
                            CHECK (after[j].tick + after[j].length <= *next - 10);
                    }
                    if (lane == static_cast<int> (Lane::bass))
                        for (const auto& n : after)
                            CHECK ((n.key >= 36 && n.key <= 55)); // C1 to G2 in Live's and Logic's names
                }
            }
}

TEST_CASE ("For my synths' file names, spelled out", "[dragout]")
{
    const auto name = [] (std::string_view id, const dragout::DragState& s, const DragRequest& r) { return drag (kit (id), forSynths (s), r).name; };
    CHECK (name ("FUT01", state (PlayMode::loop, Variant::vox), loopRequest()) == "FUT01 Alloy Signal 134 F#m 11A - Loop (synths).mid");
    CHECK (name ("FUT01", state (PlayMode::loop, Variant::novox, engine::laneBit (Lane::snare) | engine::laneBit (Lane::bass), true), songRequest())
           == "FUT01 Alloy Signal 134 F#m 11A - Song NoVox (synths, 1 drum track, no Clap, Warp Bass).mid");
    CHECK (name ("FUT01", state (PlayMode::song, Variant::vox), laneRequest (Lane::bass)) == "FUT01 Alloy Signal 134 F#m 11A - Warp Bass Song (synths).mid");
    CHECK (name ("FUT01", state (PlayMode::loop, Variant::vox), phraseRequest ("FUT01_Phrase_OrganRiff_4bars.mid"))
           == "FUT01 Alloy Signal 134 F#m 11A - Phrase Organ Riff 4 bars (synths).mid");

    // Drums, accents and FX alone hold nothing for a synth: the plain names.
    CHECK (name ("FUT01", state (PlayMode::loop, Variant::vox), laneRequest (Lane::kick)) == "FUT01 Alloy Signal 134 F#m 11A - Kick.mid");
    CHECK (name ("FUT04", state (PlayMode::loop, Variant::vox), laneRequest (Lane::accents)) == "FUT04 Furnace Code 136 Em 9A - Accents.mid");
    CHECK (name ("FUT02", state (PlayMode::loop, Variant::vox, 0b0011110000u, true), loopRequest())
           == "FUT02 Orbit Relay 130 Dm 7A - Loop (1 drum track, no Roll Bass, Chrome Arp, Starfield Pad, Vox Synth).mid");
    // In the Song, FUT04's accents lane also plays its Acid Stab and Rave Stab, which are pitched.
    CHECK (name ("FUT04", state (PlayMode::song, Variant::vox), laneRequest (Lane::accents)) == "FUT04 Furnace Code 136 Em 9A - Accents Song (synths).mid");
}

TEST_CASE ("For my synths leaves a pitched track it can't carry as it is, and still gives the drag", "[dragout]")
{
    // FUT01's vox Loop with a pitch bend at the head of its Bass track (track 5), which the writer can't lay out.
    auto patched = kit ("FUT01");
    const auto loopName = dragout::loopFileName ("FUT01", Variant::vox);
    const auto at = std::find (patched.names.begin(), patched.names.end(), loopName);
    REQUIRE (at != patched.names.end());
    auto& bytes = patched.data[static_cast<std::size_t> (at - patched.names.begin())];
    const auto chunks = midi::trackChunks (bytes);
    REQUIRE (chunks.ok());
    REQUIRE (chunks.value.size() > 6);
    REQUIRE (chunks.value[5].name == "Bass");
    const auto chunk = chunks.value[5].offset;
    const Bytes bend { 0x00, 0xE0, 0x00, 0x40 }; // at tick 0, channel 1
    bytes.insert (bytes.begin() + static_cast<std::ptrdiff_t> (chunk + 8), bend.begin(), bend.end());
    const auto length = be32 (bytes, chunk + 4) + static_cast<std::uint32_t> (bend.size());
    for (std::size_t i = 0; i < 4; ++i)
        bytes[chunk + 4 + i] = static_cast<std::uint8_t> (length >> (24 - 8 * i));
    patched.files.clear();
    for (std::size_t i = 0; i < patched.names.size(); ++i)
        patched.files.push_back ({ patched.names[i], patched.data[i] });

    const auto on = drag (patched, forSynths (state (PlayMode::loop, Variant::vox)), loopRequest());
    CHECK (on.name == patched.prefix() + "Loop (synths).mid");
    const auto out = chunksOf (on.bytes);
    const auto in = chunksOf (bytes);
    REQUIRE (out.tracks.size() == in.tracks.size());
    CHECK (out.tracks[5] == in.tracks[5]); // the Bass as it is, bend and all
    CHECK (out.tracks[6] != in.tracks[6]); // the Lead made for synths
    CHECK (trackNames (on.bytes)[6] == "Lead");
}

namespace
{
    // The drags chords as chords changes (ADR 0027, amended): each kit's Loop and Song, vox and no-vox, nothing muted,
    // and the five phrase files that hold a chord sample's track.
    struct ChordDrag
    {
        const KitContent* kit = nullptr;
        dragout::DragState state;
        DragRequest request;
        std::string what;
    };

    const std::map<std::string, std::vector<std::string>> chordPhrases {
        { "FUT01", { "FUT01_Phrase_EPianoStabRhythm_2bars.mid" } },
        { "FUT02", { "FUT02_Phrase_LaserStabRiff_2bars.mid" } },
        { "FUT03", { "FUT03_Phrase_DubChords_4bars.mid" } },
        { "FUT04", { "FUT04_Phrase_MetalChords_4bars.mid", "FUT04_Phrase_RaveStabRiff_2bars.mid" } },
    };

    std::vector<ChordDrag> chordDrags()
    {
        std::vector<ChordDrag> drags;
        for (const auto& k : kits())
        {
            for (const auto what : { DragWhat::loop, DragWhat::song })
                for (const auto v : { Variant::vox, Variant::novox })
                {
                    DragRequest r;
                    r.what = what;
                    drags.push_back ({ &k, state (PlayMode::loop, v), r, sourceName (k, what, v) });
                }
            for (const auto& phrase : chordPhrases.at (k.info.id))
                drags.push_back ({ &k, state (PlayMode::loop, Variant::vox), phraseRequest (phrase), phrase });
        }
        return drags;
    }

    std::vector<int> keysOf (const std::vector<std::uint8_t>& chord) { return std::vector<int> (chord.begin(), chord.end()); }

    // A drag with For my synths lit, and its plain file made by the two-argument rule (chord tracks as pitched tracks).
    struct ChordDragFiles
    {
        dragout::DragFile off, on;
        Bytes plain;
    };

    ChordDragFiles filesOf (const ChordDrag& d)
    {
        ChordDragFiles f;
        f.off = drag (*d.kit, d.state, d.request);
        f.on = drag (*d.kit, forSynths (d.state), d.request);
        auto plain = midi::forSynths (f.off.bytes, synthPartsOf (*d.kit, f.off.bytes, true));
        INFO (plain.error);
        REQUIRE (plain.ok());
        f.plain = std::move (plain.value);
        return f;
    }
}

TEST_CASE ("For my synths: every chord-sample note comes as its chord, at its tick, length and velocity", "[dragout]")
{
    std::size_t notes = 0, tones = 0;
    for (const auto& d : chordDrags())
    {
        const auto& k = *d.kit;
        INFO (k.info.id << " " << d.what);
        const auto files = filesOf (d);
        const auto source = read (files.off.bytes);
        const auto made = read (files.on.bytes);
        const auto plain = read (files.plain);
        REQUIRE (made.tracks.size() == source.tracks.size());
        std::size_t chordTracks = 0;
        for (std::size_t i = 1; i < source.tracks.size(); ++i)
        {
            const auto name = midi::trackName (source.tracks[i]);
            const auto* track = chordTrackOf (k, name);
            if (track == nullptr)
                continue;
            INFO (name);
            ++chordTracks;
            const auto sample = sampleOf (*track);
            const auto roots = midi::notesOf (source.tracks[i]);
            const auto alone = midi::notesOf (plain.tracks[i]); // each root as the pitched rule makes it
            REQUIRE (alone.size() == roots.size()); // one note per chord: no two on a tick and key
            std::map<std::int64_t, std::vector<midi::Note>> byTickOut;
            for (const auto& n : midi::notesOf (made.tracks[i]))
                byTickOut[n.tick].push_back (n);
            std::size_t expectedTones = 0;
            for (std::size_t j = 0; j < roots.size(); ++j)
            {
                const auto& root = roots[j];
                INFO ("the note at " << root.tick << ", key " << int { root.key });
                REQUIRE (alone[j].tick == root.tick);
                REQUIRE (alone[j].key == root.key);
                const auto chord = keysOf (midi::chordFor (sample, root.key));
                REQUIRE (chord.size() >= 4);
                std::vector<int> got;
                for (const auto& tone : byTickOut[root.tick])
                {
                    got.push_back (tone.key);
                    CHECK (tone.channel == root.channel);
                    CHECK (tone.length == alone[j].length);
                    CHECK (tone.velocity == alone[j].velocity);
                }
                CHECK (got == chord);
                expectedTones += chord.size();
                ++notes;
            }
            CHECK (midi::notesOf (made.tracks[i]).size() == expectedTones); // nothing else on the track
            tones += expectedTones;
        }
        CHECK (chordTracks > 0);
    }
    CHECK (notes == 3268); // the 3,301 measured, less the 4-bar Chords files, which aren't dragged
    CHECK (tones > 4 * notes); // 15,856
}

TEST_CASE ("For my synths: every chord sits above the bass", "[dragout]")
{
    std::size_t tones = 0, bass = 0;
    int lowest = 127, highest = 0;
    for (const auto& d : chordDrags())
    {
        const auto& k = *d.kit;
        INFO (k.info.id << " " << d.what);
        const auto file = read (filesOf (d).on.bytes);
        for (std::size_t i = 1; i < file.tracks.size(); ++i)
        {
            const auto name = midi::trackName (file.tracks[i]);
            INFO (name);
            const bool chords = chordTrackOf (k, name) != nullptr;
            const bool bassLane = laneOf (k, name) == static_cast<int> (Lane::bass);
            for (const auto& n : midi::notesOf (file.tracks[i]))
            {
                if (chords)
                {
                    CHECK (int { n.key } >= midi::kChordFloor);
                    lowest = std::min (lowest, int { n.key });
                    highest = std::max (highest, int { n.key });
                    ++tones;
                }
                else if (bassLane)
                {
                    CHECK (int { n.key } <= 55);
                    ++bass;
                }
            }
        }
    }
    INFO ("chord tones " << lowest << "-" << highest);
    CHECK (tones > 4 * 3268); // 15,856
    CHECK (bass > 1000); // 9,834
    CHECK (lowest == 58); // FUT02's B-flat maj9 on A#3, unlifted
    CHECK (highest == 92); // FUT01's Air Pad F#m9, an octave up
}

TEST_CASE ("For my synths: the Chord voicing track and every other track come out as before", "[dragout]")
{
    std::size_t chordTracks = 0, voicings = 0, others = 0;
    for (const auto& d : chordDrags())
    {
        const auto& k = *d.kit;
        INFO (k.info.id << " " << d.what);
        const auto files = filesOf (d);
        CHECK (files.on.name == withSynthsMark (files.off.name));
        const auto names = trackNames (files.off.bytes);
        const auto made = chunksOf (files.on.bytes);
        const auto plain = chunksOf (files.plain);
        CHECK (made.header == plain.header);
        REQUIRE (made.tracks.size() == names.size());
        REQUIRE (plain.tracks.size() == names.size());
        for (std::size_t i = 0; i < names.size(); ++i)
        {
            INFO (names[i]);
            if (i > 0 && chordTrackOf (k, names[i]) != nullptr)
            {
                CHECK (made.tracks[i] != plain.tracks[i]); // its chords
                ++chordTracks;
                continue;
            }
            CHECK (made.tracks[i] == plain.tracks[i]);
            ++others;
            if (names[i] == "Chord voicing")
                ++voicings; // at the samples' own register, made for synths as before
        }
    }
    CHECK (chordTracks == 33); // Chords in every file, and the kits' chord extras where they play
    CHECK (voicings == 16); // every Loop and Song carries it
    CHECK (others > 100);
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

TEST_CASE ("parts: every lane is in exactly one part but All, and All holds every lane", "[dragout]")
{
    CHECK (wubansia::kDragParts == 7);
    CHECK (std::vector<std::string_view> (wubansia::kDragPartNames.begin(), wubansia::kDragPartNames.end())
           == std::vector<std::string_view> { "All", "Drums", "Bass", "Chords", "Lead", "Vox", "Accents" });
    CHECK (wubansia::dragPartLanes (DragPart::all) == everyLane);

    // The six others share the lanes out: none twice, none left over.
    std::uint32_t covered = 0;
    for (const auto part : partsButAll)
    {
        INFO (partWord (part));
        const auto lanes = wubansia::dragPartLanes (part);
        CHECK (lanes != 0u);
        CHECK ((covered & lanes) == 0u);
        covered |= lanes;
    }
    CHECK (covered == everyLane);

    for (const auto part : partsButAll)
    {
        INFO (partWord (part));
        CHECK (wubansia::dragPartLanes (part) == lanesOf (part));
    }
    CHECK (wubansia::dragPartLanes (static_cast<DragPart> (wubansia::kDragParts)) == 0u); // no part
}

TEST_CASE ("a part drag is the conductor and the part's tracks from the chip's file, picked as a lane drag picks it, byte for byte", "[dragout]")
{
    int made = 0, missing = 0;
    for (const auto& k : kits())
        for (const auto what : { DragWhat::loop, DragWhat::song })
            for (const auto v : { Variant::vox, Variant::novox })
                for (const auto part : partsButAll)
                {
                    INFO (k.info.id << " " << whatName (what) << " " << variantName (v) << " " << partWord (part));
                    DragRequest request;
                    request.what = what;
                    const auto lanes = lanesOf (part);
                    const auto& voxFile = k.bytesOf (sourceName (k, what, Variant::vox));
                    const auto& novoxFile = k.bytesOf (sourceName (k, what, Variant::novox));
                    const auto inVox = partChunks (k, voxFile, lanes);
                    const auto inNovox = partChunks (k, novoxFile, lanes);
                    if (inVox.empty() && inNovox.empty())
                    {
                        ++missing;
                        for (const auto mode : { PlayMode::loop, PlayMode::song, PlayMode::off })
                            CHECK_THAT (dragError (k, withPart (state (mode, v), part), request), ContainsSubstring ("has no tracks in"));
                        continue;
                    }

                    // The vox file, unless the switch is off and the part differs in the no-vox file (then that file's), or
                    // the vox file has none of it (the vox line in the no-vox file can't be: the no-vox files hold no Vox).
                    const bool fromNovox = inVox.empty() || (v == Variant::novox && ! inNovox.empty() && inNovox != inVox);
                    const auto& source = fromNovox ? novoxFile : voxFile;
                    const auto sourceChunks = chunksOf (source);
                    const auto file = drag (k, withPart (state (PlayMode::loop, v), part), request);
                    ++made;
                    const auto cut = chunksOf (file.bytes);
                    REQUIRE_FALSE (cut.tracks.empty());
                    CHECK (cut.tracks[0] == sourceChunks.tracks[0]);
                    CHECK (std::vector<Bytes> (cut.tracks.begin() + 1, cut.tracks.end()) == (fromNovox ? inNovox : inVox));
                    CHECK (headerTrackCount (file.bytes) == static_cast<int> (cut.tracks.size()));
                    auto header = cut.header; // the source's but for the track count
                    header[10] = sourceChunks.header[10];
                    header[11] = sourceChunks.header[11];
                    CHECK (header == sourceChunks.header);
                    CHECK (file.name == k.prefix() + (what == DragWhat::song ? "Song " : "Loop ") + partWord (part) + (fromNovox ? " NoVox" : "") + ".mid");

                    // The chip's own file, whatever the play mode.
                    for (const auto mode : { PlayMode::song, PlayMode::off })
                    {
                        const auto other = drag (k, withPart (state (mode, v), part), request);
                        CHECK (other.name == file.name);
                        CHECK (other.bytes == file.bytes);
                    }
                }
    CHECK (made >= 92); // 94: 4 kits, Loop and Song, 2 variants, 6 parts, less FUT03's Loop Lead in both variants
    CHECK (missing == 2);
}

TEST_CASE ("part drags' names, spelled out", "[dragout]")
{
    const auto name = [] (std::string_view id, DragWhat what, Variant v, DragPart part, bool drums, bool synths) {
        DragRequest request;
        request.what = what;
        auto s = withPart (state (PlayMode::loop, v, 0, drums), part);
        s.forSynths = synths;
        return drag (kit (id), s, request).name;
    };
    CHECK (name ("FUT01", DragWhat::song, Variant::vox, DragPart::drums, false, false) == "FUT01 Alloy Signal 134 F#m 11A - Song Drums.mid");
    for (const auto v : { Variant::vox, Variant::novox }) // one file, whatever the vox switch; drums hold nothing for a synth
        CHECK (name ("FUT01", DragWhat::song, v, DragPart::drums, true, true) == "FUT01 Alloy Signal 134 F#m 11A - Song Drums (1 drum track).mid");
    CHECK (name ("FUT04", DragWhat::loop, Variant::vox, DragPart::drums, true, false) == "FUT04 Furnace Code 136 Em 9A - Loop Drums (1 drum track).mid");
    CHECK (name ("FUT01", DragWhat::song, Variant::novox, DragPart::chords, false, true) == "FUT01 Alloy Signal 134 F#m 11A - Song Chords NoVox (synths).mid");
    // FUT02's chords are the same in both Songs: the vox file's, with the switch off too.
    CHECK (name ("FUT02", DragWhat::song, Variant::novox, DragPart::chords, false, true) == "FUT02 Orbit Relay 130 Dm 7A - Song Chords (synths).mid");
    CHECK (name ("FUT02", DragWhat::loop, Variant::vox, DragPart::bass, false, true) == "FUT02 Orbit Relay 130 Dm 7A - Loop Bass (synths).mid");
    CHECK (name ("FUT03", DragWhat::song, Variant::novox, DragPart::vox, false, true) == "FUT03 Glass Current 128 Gm 6A - Song Vox (synths).mid");
    CHECK (name ("FUT04", DragWhat::song, Variant::vox, DragPart::accents, false, true) == "FUT04 Furnace Code 136 Em 9A - Song Accents (synths).mid");
    CHECK (name ("FUT04", DragWhat::song, Variant::novox, DragPart::accents, false, true) == "FUT04 Furnace Code 136 Em 9A - Song Accents NoVox (synths).mid");
    // One-key tracks only: nothing for a synth.
    CHECK (name ("FUT01", DragWhat::song, Variant::vox, DragPart::accents, false, true) == "FUT01 Alloy Signal 134 F#m 11A - Song Accents.mid");
    CHECK (name ("FUT02", DragWhat::loop, Variant::vox, DragPart::accents, false, true) == "FUT02 Orbit Relay 130 Dm 7A - Loop Accents.mid");

    // FUT03's loop has no lead in either file; and a part the strip doesn't have drags nothing.
    for (const auto v : { Variant::vox, Variant::novox })
        CHECK (dragError (kit ("FUT03"), withPart (state (PlayMode::loop, v), DragPart::lead), loopRequest())
               == "Lead has no tracks in FUT03_Loop_Vox_4bars.mid or FUT03_Loop_NoVox_4bars.mid");
    CHECK (dragError (kit ("FUT01"), withPart (state (PlayMode::loop, Variant::vox), static_cast<DragPart> (wubansia::kDragParts)), songRequest())
           == "an unknown part");
}

TEST_CASE ("a Song part keeps the extended mix's tempo, key, markers and length", "[dragout]")
{
    const std::map<std::string, std::size_t> markers { { "FUT01", 9 }, { "FUT02", 9 }, { "FUT03", 9 }, { "FUT04", 7 } };
    int bassParts = 0;
    for (const auto& k : kits())
        for (const auto what : { DragWhat::loop, DragWhat::song })
            for (const auto v : { Variant::vox, Variant::novox })
                for (const auto part : partsButAll)
                {
                    INFO (k.info.id << " " << whatName (what) << " " << variantName (v) << " " << partWord (part));
                    DragRequest request;
                    request.what = what;
                    const auto made = dragout::makeDragFile (k.info, k.files, withPart (state (PlayMode::loop, v), part), request);
                    if (! made.ok() && k.info.id == "FUT03" && what == DragWhat::loop && part == DragPart::lead)
                        continue; // its loop has no lead (the names case)
                    INFO (made.error);
                    REQUIRE (made.ok());
                    const auto file = read (made.value.bytes);
                    const auto fromNovox = made.value.name.find (" NoVox") != std::string::npos;
                    const auto source = read (k.bytesOf (sourceName (k, what, fromNovox ? Variant::novox : Variant::vox)));

                    // The conductor's: tempo, 4/4, key, and in a Song every section marker.
                    CHECK (midi::markersOf (file) == midi::markersOf (source));
                    CHECK (midi::tempoMap (file) == midi::tempoMap (source));
                    CHECK (midi::timeSignaturesOf (file) == midi::timeSignaturesOf (source));
                    CHECK (midi::keySignaturesOf (file) == midi::keySignaturesOf (source));
                    if (what == DragWhat::song)
                        CHECK (midi::markersOf (file).size() == markers.at (k.info.id));

                    // Every track runs the file's whole length: 224 bars, or the loop's 4.
                    for (const auto& t : file.tracks)
                        CHECK (t.endTick == (what == DragWhat::song ? 860160 : 15360));

                    // The bass comes in at bar 33 of the Song: a part dropped at bar 1 lines up with the Song.
                    if (what == DragWhat::song && part == DragPart::bass)
                    {
                        ++bassParts;
                        std::int64_t first = -1;
                        for (std::size_t i = 1; i < file.tracks.size(); ++i)
                            for (const auto& n : midi::notesOf (file.tracks[i]))
                                first = first < 0 ? n.tick : std::min (first, n.tick);
                        CHECK (first / 3840 + 1 == 33);
                    }
                }
    CHECK (bassParts == 8);
}

TEST_CASE ("mutes leave a part as it is", "[dragout]")
{
    for (const auto& k : kits())
        for (const auto what : { DragWhat::loop, DragWhat::song })
            for (const auto v : { Variant::vox, Variant::novox })
                for (const bool drums : { false, true })
                    for (const auto part : partsButAll)
                    {
                        INFO (k.info.id << " " << whatName (what) << " " << variantName (v) << " " << partWord (part) << " drums " << drums);
                        DragRequest request;
                        request.what = what;
                        const auto open = dragout::makeDragFile (k.info, k.files, withPart (state (PlayMode::loop, v, 0, drums), part), request);
                        for (const auto muted : { everyLane, lanesOf (part) })
                        {
                            const auto shut = dragout::makeDragFile (k.info, k.files, withPart (state (PlayMode::loop, v, muted, drums), part), request);
                            CHECK (shut.ok() == open.ok());
                            CHECK (shut.error == open.error);
                            CHECK (shut.value.name == open.value.name);
                            CHECK (shut.value.bytes == open.value.bytes);
                        }
                    }
}

TEST_CASE ("the Vox part comes from the vox file when the vox switch is off", "[dragout]")
{
    for (const auto& k : kits())
        for (const auto what : { DragWhat::loop, DragWhat::song })
        {
            INFO (k.info.id << " " << whatName (what));
            DragRequest request;
            request.what = what;
            const auto on = drag (k, withPart (state (PlayMode::loop, Variant::vox), DragPart::vox), request);
            const auto off = drag (k, withPart (state (PlayMode::loop, Variant::novox), DragPart::vox), request);
            CHECK (off.name == on.name);
            CHECK (off.bytes == on.bytes);
            CHECK (on.name == k.prefix() + (what == DragWhat::song ? "Song Vox.mid" : "Loop Vox.mid"));
            const auto names = trackNames (on.bytes);
            REQUIRE (names.size() == 2);
            CHECK_THAT (names[0], EndsWith (", Vox"));
            CHECK (names[1] == "Vox");
        }
}

TEST_CASE ("1 drum track: a Drums part is the conductor and Drums, every drum note as it was; other parts don't change", "[dragout]")
{
    for (const auto& k : kits())
        for (const auto what : { DragWhat::loop, DragWhat::song })
            for (const auto v : { Variant::vox, Variant::novox })
            {
                INFO (k.info.id << " " << whatName (what) << " " << variantName (v));
                DragRequest request;
                request.what = what;
                const auto file = drag (k, withPart (state (PlayMode::loop, v, 0, true), DragPart::drums), request);
                CHECK (file.name == k.prefix() + (what == DragWhat::song ? "Song" : "Loop") + " Drums (1 drum track).mid");

                // The drums are the same in both files: the vox file's conductor, then Drums with every drum note.
                const auto& sourceBytes = k.bytesOf (sourceName (k, what, Variant::vox));
                const auto source = read (sourceBytes);
                const auto sourceNames = trackNames (sourceBytes);
                const auto parsed = read (file.bytes);
                REQUIRE (parsed.tracks.size() == 2);
                CHECK (chunksOf (file.bytes).tracks[0] == chunksOf (sourceBytes).tracks[0]);
                CHECK (midi::trackName (parsed.tracks[1]) == "Drums");
                std::vector<midi::Note> notes;
                int drumTracks = 0;
                for (std::size_t i = 1; i < source.tracks.size(); ++i)
                    if (isDrumTrack (sourceNames[i]))
                    {
                        ++drumTracks;
                        const auto n = midi::notesOf (source.tracks[i]);
                        notes.insert (notes.end(), n.begin(), n.end());
                    }
                CHECK (drumTracks >= 4);
                CHECK (midi::notesOf (parsed.tracks[1]) == byTick (notes));
                CHECK (parsed.tracks[1].endTick == source.tracks[0].endTick);
                CHECK (offsBeforeOns (parsed.tracks[1]));

                // No other part holds a drum track: 1 drum track changes none of them.
                for (const auto part : { DragPart::bass, DragPart::chords, DragPart::lead, DragPart::vox, DragPart::accents })
                {
                    INFO (partWord (part));
                    const auto off = dragout::makeDragFile (k.info, k.files, withPart (state (PlayMode::loop, v), part), request);
                    const auto on = dragout::makeDragFile (k.info, k.files, withPart (state (PlayMode::loop, v, 0, true), part), request);
                    CHECK (on.ok() == off.ok());
                    CHECK (on.error == off.error);
                    CHECK (on.value.name == off.value.name);
                    CHECK (on.value.bytes == off.value.bytes);
                }
            }
}

TEST_CASE ("For my synths: a part drag is the plain part drag made for synths; Drums is the plain one", "[dragout]")
{
    int marked = 0, plain = 0, chordTracks = 0;
    for (const auto& k : kits())
        for (const auto what : { DragWhat::loop, DragWhat::song })
            for (const auto v : { Variant::vox, Variant::novox })
                for (const bool drums : { false, true })
                    for (const auto part : partsButAll)
                    {
                        INFO (k.info.id << " " << whatName (what) << " " << variantName (v) << " " << partWord (part) << " drums " << drums);
                        DragRequest request;
                        request.what = what;
                        const auto s = withPart (state (PlayMode::loop, v, 0, drums), part);
                        const auto off = dragout::makeDragFile (k.info, k.files, s, request);
                        const auto on = dragout::makeDragFile (k.info, k.files, forSynths (s), request);
                        if (! off.ok())
                        {
                            CHECK_FALSE (on.ok());
                            CHECK (on.error == off.error);
                            continue;
                        }
                        INFO (on.error);
                        REQUIRE (on.ok());

                        // Nothing for a synth: the plain drag, name and bytes. Otherwise For my synths' own pass on the plain
                        // drag, its chord tables the kit's (KitInfo::chordTracks).
                        const auto parts = synthPartsOf (k, off.value.bytes);
                        if (! anyForSynths (parts))
                        {
                            ++plain;
                            CHECK (on.value.name == off.value.name);
                            CHECK (on.value.bytes == off.value.bytes);
                        }
                        else
                        {
                            ++marked;
                            CHECK (on.value.name == withSynthsMark (off.value.name));
                            const auto expected = midi::forSynths (off.value.bytes, parts, chordSamplesOf (k, off.value.bytes));
                            INFO (expected.error);
                            REQUIRE (expected.ok());
                            CHECK (on.value.bytes == expected.value);

                            // A chord sample's track comes as whole chords: more notes than in the plain drag.
                            const auto names = trackNames (off.value.bytes);
                            const auto before = read (off.value.bytes);
                            const auto after = read (on.value.bytes);
                            REQUIRE (after.tracks.size() == before.tracks.size());
                            for (std::size_t i = 1; i < names.size(); ++i)
                                if (chordTrackOf (k, names[i]) != nullptr)
                                {
                                    INFO (names[i]);
                                    ++chordTracks;
                                    CHECK (midi::notesOf (after.tracks[i]).size() > midi::notesOf (before.tracks[i]).size());
                                }
                        }

                        // Drums is always plain; Accents is made for synths only in FUT02's and FUT04's Songs, whose accent
                        // synths are pitched (in their Loops the part is the one-key Accents and FX tracks alone).
                        const auto synthsMark = on.value.name.find ("(synths") != std::string::npos;
                        if (part == DragPart::drums)
                            CHECK_FALSE (synthsMark);
                        if (part == DragPart::accents)
                            CHECK (synthsMark == (what == DragWhat::song && (k.info.id == "FUT02" || k.info.id == "FUT04")));
                        if (part == DragPart::chords || part == DragPart::bass)
                            CHECK (synthsMark);
                    }
    CHECK (marked > 250); // 288
    CHECK (plain > 80); // 88: every Drums drag, and Accents but in FUT02's and FUT04's Songs
    CHECK (chordTracks > 100);
}

TEST_CASE ("each kit's Song parts hold these tracks", "[dragout]")
{
    // The content's mapping, spelled out: a track that moves to another part fails here.
    using Names = std::vector<std::string>;
    const std::map<std::string, std::map<DragPart, Names>> table {
        { "FUT01", { { DragPart::drums, { "Kick", "Clap", "Hats", "Perc" } }, { DragPart::bass, { "Bass", "Organ Bass" } },
                       { DragPart::chords, { "Chords", "Chord voicing", "Air Pad" } }, { DragPart::lead, { "Lead", "Organ Lead" } },
                       { DragPart::vox, { "Vox" } }, { DragPart::accents, { "Accents", "FX" } } } },
        { "FUT02", { { DragPart::drums, { "Kick", "Clap", "Hats", "Perc" } }, { DragPart::bass, { "Bass", "Drop Glide" } },
                       { DragPart::chords, { "Chords", "Chord voicing", "Laser Chord Stab" } }, { DragPart::lead, { "Lead" } },
                       { DragPart::vox, { "Vox" } }, { DragPart::accents, { "Accents", "Glass Ping", "FX" } } } },
        { "FUT03", { { DragPart::drums, { "Kick", "Clap", "Hats", "Perc" } }, { DragPart::bass, { "Bass" } },
                       { DragPart::chords, { "Chords", "Chord voicing", "Pad Warm" } }, { DragPart::lead, { "Lead" } },
                       { DragPart::vox, { "Vox" } }, { DragPart::accents, { "Accents", "FX" } } } },
        { "FUT04", { { DragPart::drums, { "Kick", "Rumble", "Snare", "Hats", "Perc" } }, { DragPart::bass, { "Bass" } },
                       { DragPart::chords, { "Chords", "Chord voicing", "Metal Chord" } }, { DragPart::lead, { "Lead" } },
                       { DragPart::vox, { "Vox" } }, { DragPart::accents, { "Accents", "Acid Stab", "Rave Stab", "FX" } } } },
    };
    for (const auto& k : kits())
        for (const auto part : partsButAll)
        {
            INFO (k.info.id << " " << partWord (part));
            const auto names = trackNames (drag (k, withPart (state (PlayMode::loop, Variant::vox), part), songRequest()).bytes);
            REQUIRE_FALSE (names.empty());
            CHECK (Names (names.begin() + 1, names.end()) == table.at (k.info.id).at (part));
        }
}

TEST_CASE ("trackLanes gives the lanes with a track in each Loop and Song file", "[dragout]")
{
    // By engine::timelineIndex: Loop vox, Loop no-vox, Song vox, Song no-vox. The no-vox files have no Vox; FUT03's loop
    // has no lead and no FX.
    const std::map<std::string, std::array<std::uint32_t, 4>> expected {
        { "FUT01", { 0x3FFu, 0x37Fu, 0x3FFu, 0x37Fu } },
        { "FUT02", { 0x3FFu, 0x37Fu, 0x3FFu, 0x37Fu } },
        { "FUT03", { 0x1DFu, 0x15Fu, 0x3FFu, 0x37Fu } },
        { "FUT04", { 0x3FFu, 0x37Fu, 0x3FFu, 0x37Fu } },
    };
    for (const auto& k : kits())
    {
        const std::array<std::string, 4> files { dragout::loopFileName (k.info.id, Variant::vox), dragout::loopFileName (k.info.id, Variant::novox),
            dragout::songFileName (k.info.id, Variant::vox), dragout::songFileName (k.info.id, Variant::novox) };
        for (std::size_t i = 0; i < files.size(); ++i)
        {
            INFO (files[i]);
            CHECK (dragout::trackLanes (k.info, read (k.bytesOf (files[i]))) == expected.at (k.info.id)[i]);
        }
    }

    // The conductor alone, or no track at all, holds no lane.
    midi::File conductorOnly;
    conductorOnly.tracks.resize (1);
    CHECK (dragout::trackLanes (kit ("FUT01").info, conductorOnly) == 0u);
    CHECK (dragout::trackLanes (kit ("FUT01").info, midi::File {}) == 0u);
}

TEST_CASE ("lane and phrase drags ignore the part", "[dragout]")
{
    for (const auto& k : kits())
        for (const auto mode : { PlayMode::loop, PlayMode::song })
            for (const auto v : { Variant::vox, Variant::novox })
            {
                std::vector<DragRequest> requests;
                for (const auto lane : allLanes)
                    requests.push_back (laneRequest (lane));
                for (const auto& phrase : dragout::kitPhraseFiles (k.info.id, k.files))
                    requests.push_back (phraseRequest (phrase.toStdString()));
                for (const auto& request : requests)
                {
                    const auto all = dragout::makeDragFile (k.info, k.files, state (mode, v), request);
                    for (const auto part : partsButAll)
                    {
                        INFO (k.info.id << " mode " << static_cast<int> (mode) << " " << variantName (v) << " lane " << static_cast<int> (request.lane) << " "
                                        << request.phraseFile << " " << partWord (part));
                        const auto withIt = dragout::makeDragFile (k.info, k.files, withPart (state (mode, v), part), request);
                        CHECK (withIt.ok() == all.ok());
                        CHECK (withIt.error == all.error);
                        CHECK (withIt.value.name == all.value.name);
                        CHECK (withIt.value.bytes == all.value.bytes);
                    }
                }
            }
}

TEST_CASE ("a phrase drag is the phrase file, byte for byte", "[dragout]")
{
    // Every file the Phrases drawer lists (kitPhraseFiles' own case checks the list): the phrases and the lead line.
    for (const auto& k : kits())
    {
        INFO (k.info.id);
        const auto phrases = dragout::kitPhraseFiles (k.info.id, k.files);
        CHECK (phrases.size() >= 5);
        for (const auto& name : phrases)
        {
            INFO (name);
            const auto file = drag (k, state (PlayMode::song, Variant::novox, everyLane), phraseRequest (name.toStdString()));
            CHECK (file.bytes == k.bytesOf (name.toStdString()));
            const auto lead = name.startsWith (juce::String (k.info.id) + "_Lead_");
            CHECK_THAT (file.name, StartsWith (k.prefix() + (lead ? "Lead " : "Phrase ")) && EndsWith (" bars.mid"));
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

TEST_CASE ("kitPhraseFiles lists the kit's phrases and its lead line", "[dragout]")
{
    // The Phrases drawer's From the kit (ADR 0031): the phrase files by name, then the lead line.
    const std::map<std::string, juce::StringArray> expected {
        { "FUT01", { "FUT01_Phrase_EPianoStabRhythm_2bars.mid", "FUT01_Phrase_OrganRiff_4bars.mid", "FUT01_Phrase_PluckRiff_4bars.mid",
                       "FUT01_Phrase_VoxSynthRiff_2bars.mid", "FUT01_Phrase_WarpRiff_2bars.mid", "FUT01_Lead_ChromePluck_2bars.mid" } },
        { "FUT02", { "FUT02_Phrase_GlassMelody_4bars.mid", "FUT02_Phrase_LaserStabRiff_2bars.mid", "FUT02_Phrase_VoxChoirChords_4bars.mid",
                       "FUT02_Phrase_VoxSynthRiff_2bars.mid", "FUT02_Lead_ChromeArp_2bars.mid" } },
        { "FUT03", { "FUT03_Phrase_DubChords_4bars.mid", "FUT03_Phrase_KeysArp_4bars.mid", "FUT03_Phrase_RubberRiff_2bars.mid",
                       "FUT03_Phrase_VoxSynthRiff_2bars.mid", "FUT03_Lead_KeysEP_2bars.mid" } },
        { "FUT04", { "FUT04_Phrase_AcidStabRiff_2bars.mid", "FUT04_Phrase_MetalChords_4bars.mid", "FUT04_Phrase_RaveStabRiff_2bars.mid",
                       "FUT04_Phrase_VoxSynthRiff_2bars.mid", "FUT04_Lead_ModBlip_2bars.mid" } },
    };

    // Every kit's files in one list, and each file in a folder: each kit still finds only its own, by file name.
    std::vector<dragout::MidiFile> all;
    for (const auto& k : kits())
        all.insert (all.end(), k.files.begin(), k.files.end());
    for (const auto& k : kits())
    {
        INFO (k.info.id);
        const auto& wanted = expected.at (k.info.id);
        CHECK (dragout::kitPhraseFiles (k.info.id, k.files) == wanted);
        CHECK (dragout::kitPhraseFiles (k.info.id, all) == wanted);
        std::vector<std::string> paths;
        for (const auto& name : k.names)
            paths.push_back ("midi/" + juce::String (k.info.id).toLowerCase().toStdString() + "/" + name);
        std::vector<dragout::MidiFile> inFolders;
        for (std::size_t i = 0; i < paths.size(); ++i)
            inFolders.push_back ({ paths[i], k.data[i] });
        CHECK (dragout::kitPhraseFiles (k.info.id, inFolders) == wanted);
    }
    CHECK (dragout::kitPhraseFiles ("FUT05", all).isEmpty());
}

TEST_CASE ("the kit's lead line drags as a phrase does", "[dragout]")
{
    // FUT01's, spelled out: the file's own bytes and the phrases' name form, and " (synths)" with For my synths lit.
    const auto& fut01 = kit ("FUT01");
    const auto request = phraseRequest ("FUT01_Lead_ChromePluck_2bars.mid");
    const auto plain = drag (fut01, state (PlayMode::loop, Variant::vox), request);
    CHECK (plain.bytes == fut01.bytesOf ("FUT01_Lead_ChromePluck_2bars.mid"));
    CHECK (plain.name == "FUT01 Alloy Signal 134 F#m 11A - Lead Chrome Pluck 2 bars.mid");
    const auto synths = drag (fut01, forSynths (state (PlayMode::loop, Variant::vox)), request);
    CHECK (synths.name == "FUT01 Alloy Signal 134 F#m 11A - Lead Chrome Pluck 2 bars (synths).mid");
    const auto made = midi::forSynths (plain.bytes, synthPartsOf (fut01, plain.bytes));
    INFO (made.error);
    REQUIRE (made.ok());
    CHECK (synths.bytes == made.value);

    // Every kit's lead line, whatever the mode, the vox switch, the mutes and 1 drum track: its own bytes, named as a
    // phrase is, and made for synths like one.
    int leads = 0;
    for (const auto& k : kits())
        for (const auto& name : dragout::kitPhraseFiles (k.info.id, k.files))
        {
            if (! name.startsWith (juce::String (k.info.id) + "_Lead_"))
                continue;
            ++leads;
            INFO (name);
            const auto file = name.toStdString();
            const auto asIs = drag (k, state (PlayMode::song, Variant::novox, everyLane, true), phraseRequest (file));
            CHECK (asIs.bytes == k.bytesOf (file));
            CHECK_THAT (asIs.name, StartsWith (k.prefix() + "Lead ") && EndsWith (" 2 bars.mid"));
            const auto lit = drag (k, forSynths (state (PlayMode::loop, Variant::vox)), phraseRequest (file));
            CHECK (lit.name == withSynthsMark (asIs.name));
            const auto expected = midi::forSynths (asIs.bytes, synthPartsOf (k, asIs.bytes));
            INFO (expected.error);
            REQUIRE (expected.ok());
            CHECK (lit.bytes == expected.value);
        }
    CHECK (leads == 4);
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
    CHECK (dragout::kitPhraseFiles (k.info.id, files) == dragout::kitPhraseFiles (k.info.id, k.files));
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
        CHECK (dragout::kitPhraseFiles (k.info.id, all) == dragout::kitPhraseFiles (k.info.id, k.files));
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

    // Every name the four kits give: Loop and Song in both variants under several mutes, with 1 drum
    // track off and on, and for every part (ADR 0029) with 1 drum track off and on; every lane in both
    // modes and variants, every phrase file; each with For my synths off and on.
    std::vector<std::pair<std::string, const KitContent*>> named;
    std::map<std::string, Bytes> byName;
    const auto add = [&] (const KitContent& k, const dragout::DragState& s, const DragRequest& r) {
        for (const auto& t : { s, forSynths (s) })
        {
            const auto made = dragout::makeDragFile (k.info, k.files, t, r);
            if (!made.ok())
                continue;
            named.emplace_back (made.value.name, &k);
            const auto it = byName.emplace (made.value.name, made.value.bytes).first;
            INFO (made.value.name);
            CHECK (it->second == made.value.bytes); // one name is one file
        }
    };
    for (const auto& k : kits())
    {
        for (const auto v : { Variant::vox, Variant::novox })
        {
            for (const auto what : { DragWhat::loop, DragWhat::song })
                for (const bool drums : { false, true })
                {
                    DragRequest r;
                    r.what = what;
                    add (k, state (PlayMode::loop, v, 0, drums), r);
                    add (k, state (PlayMode::loop, v, 0b1111100000u, drums), r);
                    for (const auto lane : allLanes)
                    {
                        add (k, state (PlayMode::loop, v, engine::laneBit (lane), drums), r);
                        add (k, state (PlayMode::loop, v, everyLane & ~engine::laneBit (lane), drums), r);
                    }
                    for (const auto part : partsButAll)
                        add (k, withPart (state (PlayMode::loop, v, 0, drums), part), r);
                }
            for (const auto mode : { PlayMode::loop, PlayMode::song })
                for (const auto lane : allLanes)
                    add (k, state (mode, v), laneRequest (lane));
        }
        for (const auto& name : dragout::kitPhraseFiles (k.info.id, k.files))
            add (k, state (PlayMode::loop, Variant::vox), phraseRequest (name.toStdString()));
    }
    REQUIRE (named.size() > 1600);
    CHECK (std::count_if (named.begin(), named.end(), [] (const auto& n) { return n.first.find ("1 drum track") != std::string::npos; }) > 300);
    CHECK (std::count_if (named.begin(), named.end(), [] (const auto& n) { return n.first.find ("(synths") != std::string::npos; }) > 500);
    CHECK (std::count_if (named.begin(), named.end(), [] (const auto& n) { return n.first.find (" - Song Drums") != std::string::npos; }) > 0);
    const auto partName = [] (const std::string& name) {
        return std::any_of (partsButAll.begin(), partsButAll.end(), [&name] (DragPart part) {
            return name.find (" - Loop " + partWord (part)) != std::string::npos || name.find (" - Song " + partWord (part)) != std::string::npos;
        });
    };
    CHECK (std::count_if (named.begin(), named.end(), [&partName] (const auto& n) { return partName (n.first); }) > 80);

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

        // With 1 drum track: the merge too.
        const auto d = state (PlayMode::song, Variant::novox, engine::laneBit (Lane::hats) | engine::laneBit (Lane::fx), true);
        const auto c = drag (k, d, songRequest());
        const auto e = drag (k, d, songRequest());
        CHECK_THAT (c.name, ContainsSubstring ("(1 drum track, no "));
        CHECK (c.name == e.name);
        CHECK (c.bytes == e.bytes);

        // With For my synths: its rewrite too.
        const auto f = drag (k, forSynths (d), songRequest());
        const auto g = drag (k, forSynths (d), songRequest());
        CHECK_THAT (f.name, ContainsSubstring ("(synths, 1 drum track, no "));
        CHECK (f.name == g.name);
        CHECK (f.bytes == g.bytes);

        // A part (ADR 0029): Drums on one track, and Accents made for synths.
        const auto p = withPart (d, DragPart::drums);
        const auto h = drag (k, p, songRequest());
        const auto i = drag (k, p, songRequest());
        CHECK_THAT (h.name, EndsWith (" - Song Drums (1 drum track).mid"));
        CHECK (h.name == i.name);
        CHECK (h.bytes == i.bytes);
        const auto q = forSynths (withPart (d, DragPart::accents));
        const auto j = drag (k, q, songRequest());
        const auto l = drag (k, q, songRequest());
        CHECK_THAT (j.name, ContainsSubstring (" - Song Accents"));
        CHECK (j.name == l.name);
        CHECK (j.bytes == l.bytes);
    }
}

TEST_CASE ("an OS drag hands over a percent-encoded file URL on Linux, and the path elsewhere", "[dragout]")
{
    // Drop targets read text/uri-list as URIs: GLib, Qt and Ardour cut a bare path at '#', REAPER at the first space.
    const auto folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile (juce::CharPointer_UTF8 ("S\xc3\xa4mples #1 (x) 100% a+b"));
    const juce::File files[] {
        folder.getChildFile ("FUT01 Alloy Signal").getChildFile ("Rave Stab v2 - FUT01 134 F#m 11A.wav"), // every FUT01 name has '#'
        folder.getChildFile ("FUT03 Glass Current").getChildFile ("Riser - FUT03 128 Gm 6A.wav"), // spaces only
        folder.getChildFile ("FUT01 Alloy Signal 134 F#m 11A - Loop.mid"), // a MIDI chip's
    };
    for (const auto& file : files)
    {
        const auto handed = dragout::externalDragPath (file);
        CAPTURE (file.getFullPathName().toStdString(), handed.toStdString());
#if JUCE_LINUX || JUCE_BSD
        CHECK (handed.startsWith ("file:///"));
        CHECK (handed.matchesWildcard ("?*://*", true)); // what JUCE's X11 drag hands over unchanged
        const auto rest = handed.fromFirstOccurrenceOf ("file://", false, false);
        CHECK_FALSE (rest.containsAnyOf (" \t#?\"<>\\`"));
        bool ascii = true, escapes = true; // UTF-8 percent-encoded; every '%' starts an escape
        const juce::String hex ("0123456789ABCDEFabcdef");
        for (int i = 0; i < rest.length(); ++i)
        {
            ascii = ascii && rest[i] < 0x80;
            if (rest[i] == '%')
                escapes = escapes && i + 2 < rest.length() && hex.containsChar (rest[i + 1]) && hex.containsChar (rest[i + 2]);
        }
        CHECK (ascii);
        CHECK (escapes);
        CHECK (juce::URL (handed).getLocalFile() == file);
#else
        CHECK (handed == file.getFullPathName());
#endif
    }
}

#if JUCE_LINUX
namespace
{
    // This process's child processes, from /proc: "<pid> <state> <name>" each, the state Z for one that has ended and
    // that nobody has waited for (a zombie, kept in the process table until its parent waits or quits).
    juce::StringArray childProcesses()
    {
        juce::StringArray children;
        const auto self = juce::String (static_cast<int> (getpid()));
        for (const auto& entry : juce::File ("/proc").findChildFiles (juce::File::findDirectories, false))
        {
            if (! entry.getFileName().containsOnly ("0123456789"))
                continue;
            const auto stat = entry.getChildFile ("stat").loadFileAsString(); // "<pid> (<name>) <state> <ppid> ..."
            const auto name = stat.fromFirstOccurrenceOf ("(", false, false).upToLastOccurrenceOf (")", false, false);
            const auto fields = juce::StringArray::fromTokens (stat.fromLastOccurrenceOf (")", false, false).trim(), " ", "");
            if (fields.size() >= 2 && fields[1] == self)
                children.add (entry.getFileName() + " " + fields[0] + " " + name);
        }
        return children;
    }

    // The PIDs of childProcesses ()'s entries: the first word of each.
    juce::StringArray pidsOf (const juce::StringArray& children)
    {
        juce::StringArray pids;
        for (const auto& child : children)
            pids.add (child.upToFirstOccurrenceOf (" ", false, false));
        return pids;
    }

    // This process's children whose PIDs aren't among before's (pidsOf ()): the ones that came since. By PID, not the
    // whole entry: a child that was running then shows as S before and as Z once it ends.
    juce::StringArray childProcessesSince (const juce::StringArray& before)
    {
        juce::StringArray since;
        for (const auto& child : childProcesses())
            if (! before.contains (child.upToFirstOccurrenceOf (" ", false, false)))
                since.add (child);
        return since;
    }

    // Waits up to 2 s for /proc to show pid as this process's zombie: it has ended and nobody has waited for it.
    bool showsAsZombie (pid_t pid)
    {
        const auto zombie = juce::String (static_cast<int> (pid)) + " Z ";
        const auto until = juce::Time::getMillisecondCounter() + 2000;
        for (;;)
        {
            for (const auto& child : childProcesses())
                if (child.startsWith (zombie))
                    return true;
            if (juce::Time::getMillisecondCounter() >= until)
                return false;
            juce::Thread::sleep (5);
        }
    }

    // A child of this process that isn't an opener, as an earlier test in the same run can leave one: JUCE's dark-mode
    // probe (gsettings, started as the Desktop is made) is never waited for once it takes over 200 ms. It runs until
    // end (), then exits, and stays a zombie until this goes, which waits for it. It runs only close (), read () and
    // _exit (), which are safe after a fork in a process with threads. The pipe closes on exec, so no opener holds it,
    // and the child also ends if this process dies first.
    struct LeftChild
    {
        int fds[2] { -1, -1 };
        pid_t pid = -1;

        LeftChild()
        {
            REQUIRE (::pipe2 (fds, O_CLOEXEC) == 0);
            pid = ::fork();
            if (pid == 0)
            {
                ::close (fds[1]);
                char byte = 0;
                while (::read (fds[0], &byte, 1) < 0 && errno == EINTR)
                {
                }
                ::_exit (0);
            }
            ::close (fds[0]);
            fds[0] = -1;
            REQUIRE (pid > 0);
        }

        ~LeftChild()
        {
            end();
            if (pid > 0)
                while (::waitpid (pid, nullptr, 0) < 0 && errno == EINTR)
                {
                }
        }

        LeftChild (const LeftChild&) = delete;
        LeftChild& operator= (const LeftChild&) = delete;

        void end()
        {
            if (fds[1] < 0)
                return;
            const char byte = 0;
            const auto written = ::write (fds[1], &byte, 1); // wakes the child's read (), whoever else holds the pipe
            juce::ignoreUnused (written);
            ::close (fds[1]);
            fds[1] = -1;
        }
    };
}
#endif

#if JUCE_LINUX || JUCE_BSD
namespace
{
    // A stub xdg-open first on PATH that logs the arguments it gets, and a copy in a samples folder whose name a shell
    // would act on: the person picks that folder with Change..., so it can hold $HOME, backticks, $(...) and quotes. The
    // working folder is a fresh one, where a command smuggled in through the name would leave a file. PATH, the working
    // folder and the temp files come back as they were however the test ends.
    struct StubOpener
    {
        struct Restore
        {
            juce::File folder = juce::File::getSpecialLocation (juce::File::tempDirectory).getNonexistentChildFile ("WubansiaRevealTest", "", false);
            juce::String path = juce::SystemStats::getEnvironmentVariable ("PATH", {});
            juce::File cwd = juce::File::getCurrentWorkingDirectory();

            ~Restore()
            {
                setenv ("PATH", path.toRawUTF8(), 1);
                cwd.setAsCurrentWorkingDirectory();
                folder.deleteRecursively();
            }
        } restore; // first, so it is undone even when a REQUIRE below throws
        juce::File log, work, kitFolder, copy;

        StubOpener()
        {
            const auto& temp = restore.folder;
            REQUIRE (temp.createDirectory().wasOk());
            const auto bin = temp.getChildFile ("bin");
            log = temp.getChildFile ("args.txt");
            REQUIRE (bin.createDirectory().wasOk());
            const auto stub = bin.getChildFile ("xdg-open");
            REQUIRE (stub.replaceWithText ("#!/bin/sh\nprintf '%s\\n' \"$#\" \"$@\" > '" + log.getFullPathName() + ".part' && mv '"
                                               + log.getFullPathName() + ".part' '" + log.getFullPathName() + "'\n",
                false, false, "\n")); // Unix line ends: "#!/bin/sh\r" names no interpreter
            REQUIRE (stub.setExecutePermission (true));
            REQUIRE (setenv ("PATH", (bin.getFullPathName() + ":" + restore.path).toRawUTF8(), 1) == 0);

            work = temp.getChildFile ("cwd");
            REQUIRE (work.createDirectory().wasOk());
            REQUIRE (work.setAsCurrentWorkingDirectory());

            const auto samples = temp.getChildFile ("Beats $HOME `touch INJECTED` \"2025\" $(touch INJECTED2)");
            kitFolder = samples.getChildFile ("FUT02 Orbit Relay");
            REQUIRE (kitFolder.createDirectory().wasOk());
            copy = kitFolder.getChildFile ("Glass Ping v2 - FUT02 130 Dm 7A.wav");
            REQUIRE (copy.replaceWithText ("RIFF"));
        }

        // reveal (copy), then what the stub got: how many arguments, then each one.
        juce::StringArray reveal (const std::function<void (const juce::File&)>& revealer) const
        {
            REQUIRE (revealer != nullptr);
            revealer (copy);
            const auto deadline = juce::Time::getMillisecondCounter() + 5000;
            while (! log.existsAsFile() && juce::Time::getMillisecondCounter() < deadline)
                juce::Thread::sleep (5);
            REQUIRE (log.existsAsFile());
            return juce::StringArray::fromLines (log.loadFileAsString().trimEnd());
        }

        bool injected() const { return work.getChildFile ("INJECTED").exists() || work.getChildFile ("INJECTED2").exists(); }
    };
}
#endif

TEST_CASE ("revealing a file on Linux opens its folder with xdg-open, the path as one argument and no shell", "[dragout]")
{
#if ! (JUCE_LINUX || JUCE_BSD)
    SKIP ("macOS and Windows reveal without a shell");
#else
    const StubOpener stub;
   #if JUCE_LINUX
    // Two children this process has that aren't the reveal's, as an earlier test can leave them, made here so that
    // every run has them: one that has ended and that nobody has waited for (a zombie), and one still running at the
    // reveal that ends during the check below.
    LeftChild zombie;
    zombie.end();
    REQUIRE (showsAsZombie (zombie.pid));
    LeftChild ending;
    const auto before = pidsOf (childProcesses()); // none of them is the opener
   #endif
    CHECK (stub.reveal (dragout::revealToUser) == juce::StringArray { "1", stub.kitFolder.getFullPathName() }); // the folder, exactly
    CHECK_FALSE (stub.injected());

   #if JUCE_LINUX
    ending.end();
    REQUIRE (showsAsZombie (ending.pid));

    // The opener has written its log and ends now. Nothing the reveal started stays this process's child: in a host, a
    // child nobody waits for stays in the process table as a zombie until the host quits (or until something reaps it).
    auto added = childProcessesSince (before);
    const auto until = juce::Time::getMillisecondCounter() + 2000;
    while (! added.isEmpty() && juce::Time::getMillisecondCounter() < until)
    {
        juce::Thread::sleep (10);
        added = childProcessesSince (before);
    }
    CAPTURE (added.joinIntoString (", ").toStdString());
    CHECK (added.isEmpty());
   #endif
#endif
}

TEST_CASE ("the drawer's and the MIDI chips' own reveal opens the folder with xdg-open, no shell, on Linux", "[dragout]")
{
#if ! (JUCE_LINUX || JUCE_BSD)
    SKIP ("macOS and Windows reveal without a shell; the hooks' own case checks they are dragout::revealToUser");
#else
    // Each component as the plugin builds it, its default hooks untouched: the call the plugin makes, not the helper alone.
    wubansia::test::FakeController fake;
    const editor::SamplesPanel drawer (fake);
    const editor::PhrasesPanel phrases (fake);
    const editor::DragChip chip (fake, DragRequest {}, "Loop");
    for (const auto* revealFile : { &drawer.revealFile, &phrases.revealFile, &chip.revealFile })
    {
        const StubOpener stub;
        CHECK (stub.reveal (*revealFile) == juce::StringArray { "1", stub.kitFolder.getFullPathName() });
        CHECK_FALSE (stub.injected());
    }
#endif
}

TEST_CASE ("the drawer's and the MIDI chips' default hooks are drag-out's own functions, on every OS", "[dragout]")
{
    // A default the OS can't be asked about here (no window can start a drag; only Linux reveals through a stub) is
    // checked by what it is: the very function, not a lambda that might call something else.
    using Drag = bool (*) (const juce::File&, juce::Component&);
    using Reveal = void (*) (const juce::File&);
    using Announce = void (*) (const juce::String&);
    wubansia::test::FakeController fake;
    editor::SamplesPanel drawer (fake);
    editor::PhrasesPanel phrases (fake);
    editor::DragChip chip (fake, DragRequest {}, "Loop");
    for (auto* startDrag : { &drawer.startDrag, &phrases.startDrag, &chip.startDrag })
    {
        const auto* drag = startDrag->target<Drag>();
        REQUIRE (drag != nullptr);
        CHECK (*drag == &editor::startExternalDrag);
    }
    for (auto* revealFile : { &drawer.revealFile, &phrases.revealFile, &chip.revealFile })
    {
        const auto* reveal = revealFile->target<Reveal>();
        REQUIRE (reveal != nullptr);
        CHECK (*reveal == &dragout::revealToUser);
    }
    for (auto* announce : { &drawer.announce, &phrases.announce })
    {
        const auto* say = announce->target<Announce>();
        REQUIRE (say != nullptr);
        CHECK (*say == &editor::announceToScreenReader);
    }
}

TEST_CASE ("an OS drag from the drawer or a MIDI chip hands over its one file as externalDragPath gives it", "[dragout]")
{
    static juce::StringArray handed;
    static bool movable = true, called = false;
    static juce::Component* from = nullptr;
    const editor::PerformExternalDrag perform = [] (const juce::StringArray& files, bool canMoveFiles, juce::Component* source, std::function<void()> callback)
    {
        handed = files;
        movable = canMoveFiles;
        from = source;
        called = callback != nullptr;
        return true;
    };
    juce::Component source;
    const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("FUT01 Alloy Signal").getChildFile ("Rave Stab v2 - FUT01 134 F#m 11A.wav");
    CHECK (editor::handOverDrag (file, source, perform));
    CHECK (handed == juce::StringArray { dragout::externalDragPath (file) }); // a file:// URL on Linux, the path elsewhere
    CHECK_FALSE (movable); // a copy: the DAW never moves it
    CHECK (from == &source);
    CHECK_FALSE (called);
    CHECK_FALSE (editor::handOverDrag (file, source, nullptr));
}
