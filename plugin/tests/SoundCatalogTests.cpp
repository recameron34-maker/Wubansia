// The Samples drawer's catalog (ADR 0024, sounds/SoundCatalog.h): every sound of a kit, built from its
// embedded kit.json alone, so none of this needs samples. The counts, merges, default files and lengths
// pinned here are the four committed manifests' (the spec's section 4.6 table).

#include "content/EmbeddedContent.h"
#include "services/KitCatalog.h"
#include "sounds/SoundCatalog.h"

#include "testing/synthetic_kit.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <set>
#include <string>
#include <vector>

using namespace wubansia;

namespace
{
    // The plugin's four kits, parsed once.
    const KitCatalog& embedded()
    {
        static const KitCatalog catalog (EmbeddedContent::sources());
        return catalog;
    }

    const KitCatalog::Entry& kitEntry (std::string_view kit)
    {
        const auto* entry = embedded().find (kit);
        REQUIRE (entry != nullptr);
        REQUIRE (entry->ok());
        return *entry;
    }

    const engine::KitManifest& manifestOf (std::string_view kit) { return kitEntry (kit).manifest; }

    // The sound with this name; a failed REQUIRE when there is none or more than one.
    const SoundInfo& named (const std::vector<SoundInfo>& sounds, const juce::String& name)
    {
        const SoundInfo* found = nullptr;
        int count = 0;
        for (const auto& s : sounds)
            if (s.name == name)
            {
                found = &s;
                ++count;
            }
        INFO ("sound " << name.toStdString());
        REQUIRE (count == 1);
        return *found;
    }

    const SoundInfo* withId (const std::vector<SoundInfo>& sounds, const juce::String& id)
    {
        for (const auto& s : sounds)
            if (s.id == id)
                return &s;
        return nullptr;
    }

    // The kit.json path of a sound's default file.
    std::string defaultPath (const engine::KitManifest& kit, const SoundInfo& sound)
    {
        REQUIRE (sound.defaultFile >= 0);
        REQUIRE ((std::size_t) sound.defaultFile < sound.files.size());
        const auto file = sound.files[(std::size_t) sound.defaultFile].file;
        REQUIRE (file >= 0);
        REQUIRE ((std::size_t) file < kit.files.size());
        return kit.files[(std::size_t) file].path;
    }

    std::array<int, kSoundCategories> byCategory (const std::vector<SoundInfo>& sounds)
    {
        std::array<int, kSoundCategories> counts {};
        for (const auto& s : sounds)
            ++counts[(std::size_t) s.category];
        return counts;
    }

    std::set<int> fileSet (const SoundInfo& sound)
    {
        std::set<int> files;
        for (const auto& f : sound.files)
            files.insert (f.file);
        return files;
    }

    // The index of the file kit.json lists at path.
    int fileIndex (const engine::KitManifest& kit, std::string_view path)
    {
        for (std::size_t i = 0; i < kit.files.size(); ++i)
            if (kit.files[i].path == path)
                return (int) i;
        FAIL ("no file " << std::string (path));
        return -1;
    }

    // A file name without its folder and ".wav".
    juce::String labelOf (const std::string& path)
    {
        const auto name = juce::String::fromUTF8 (path.data(), (int) path.size()).fromLastOccurrenceOf ("/", false, false);
        return name.endsWith (".wav") ? name.dropLastCharacters (4) : name;
    }

    engine::LayerEntry layer (std::int32_t vel, std::vector<std::int32_t> rr)
    {
        engine::LayerEntry l;
        l.vel = vel;
        l.rr = std::move (rr);
        return l;
    }

    engine::ZoneEntry zone (std::int32_t root, std::int32_t lo, std::int32_t hi, std::vector<engine::LayerEntry> layers)
    {
        engine::ZoneEntry z;
        z.root = root;
        z.lo = lo;
        z.hi = hi;
        z.layers = std::move (layers);
        return z;
    }

    engine::InstrumentEntry instrument (std::string id, std::string name, engine::Lane lane, std::vector<engine::ZoneEntry> zones)
    {
        engine::InstrumentEntry in;
        in.id = std::move (id);
        in.name = std::move (name);
        in.lane = lane;
        in.zones = std::move (zones);
        return in;
    }
}

TEST_CASE ("every kit's sounds by category: 35, 37, 36 and 32", "[sounds]")
{
    struct Row
    {
        const char* kit;
        std::size_t sounds;
        std::array<int, kSoundCategories> byCategory; // Accents, FX, Vox, Synths, Bass, Drums, Loops
        std::size_t files;
    };
    const Row rows[] {
        { "FUT01", 35, { 8, 3, 6, 5, 3, 8, 2 }, 189 },
        { "FUT02", 37, { 9, 3, 6, 3, 4, 5, 7 }, 138 },
        { "FUT03", 36, { 9, 3, 5, 3, 2, 8, 6 }, 161 },
        { "FUT04", 32, { 8, 3, 3, 2, 2, 7, 7 }, 130 },
    };
    for (const auto& row : rows)
    {
        CAPTURE (row.kit);
        const auto& kit = manifestOf (row.kit);
        const auto sounds = sounds::soundsOf (kit);
        CHECK (sounds.size() == row.sounds);
        CHECK (byCategory (sounds) == row.byCategory);
        CHECK (kit.files.size() == row.files);
    }

    CHECK (sounds::categoryName (SoundCategory::accents) == "Accents");
    CHECK (sounds::categoryName (SoundCategory::fx) == "FX");
    CHECK (sounds::categoryName (SoundCategory::vox) == "Vox");
    CHECK (sounds::categoryName (SoundCategory::synths) == "Synths");
    CHECK (sounds::categoryName (SoundCategory::bass) == "Bass");
    CHECK (sounds::categoryName (SoundCategory::drums) == "Drums");
    CHECK (sounds::categoryName (SoundCategory::loops) == "Loops");

    using engine::Lane;
    CHECK (sounds::categoryOf (Lane::accents) == SoundCategory::accents);
    CHECK (sounds::categoryOf (Lane::fx) == SoundCategory::fx);
    CHECK (sounds::categoryOf (Lane::vox) == SoundCategory::vox);
    CHECK (sounds::categoryOf (Lane::lead) == SoundCategory::synths);
    CHECK (sounds::categoryOf (Lane::chords) == SoundCategory::synths);
    CHECK (sounds::categoryOf (Lane::bass) == SoundCategory::bass);
    for (const auto drum : { Lane::kick, Lane::snare, Lane::hats, Lane::perc })
        CHECK (sounds::categoryOf (drum) == SoundCategory::drums);
}

TEST_CASE ("every listed file is in a sound, every stem a Loop: 618 files", "[sounds]")
{
    std::size_t reached = 0;
    for (const auto& entry : embedded().entries())
    {
        CAPTURE (entry.id);
        REQUIRE (entry.ok());
        const auto& kit = entry.manifest;
        const auto sounds = sounds::soundsOf (kit);

        std::set<int> files;
        for (const auto& sound : sounds)
        {
            CAPTURE (sound.id.toStdString());
            REQUIRE_FALSE (sound.files.empty());
            CHECK (fileSet (sound).size() == sound.files.size()); // each file once
            for (const auto& f : sound.files)
            {
                REQUIRE (f.file >= 0);
                REQUIRE ((std::size_t) f.file < kit.files.size());
                const auto& listed = kit.files[(std::size_t) f.file];
                CHECK (f.label == labelOf (listed.path)); // the installed name, as it is
                CHECK (juce::exactlyEqual (f.seconds, (double) listed.frames / 48000.0));
                CHECK (f.channels == listed.channels);
                files.insert (f.file);
            }
        }
        CHECK (files.size() == kit.files.size()); // nothing unreachable
        reached += files.size();

        // Each phrase stem is a Loop of its own: that one file, the default.
        for (const auto& phrase : kit.phrases)
            for (const auto stem : { phrase.stemA, phrase.stemB })
            {
                if (stem < 0)
                    continue;
                CAPTURE (kit.files[(std::size_t) stem].path);
                int loops = 0;
                for (const auto& sound : sounds)
                    if (sound.files.size() == 1 && sound.files[0].file == stem)
                    {
                        CHECK (sound.category == SoundCategory::loops);
                        CHECK (sound.defaultFile == 0);
                        CHECK (sound.loopBars == phrase.cycle16ths / 16);
                        ++loops;
                    }
                CHECK (loops == 1);
            }

        // Loops are the stems, and only they.
        for (const auto& sound : sounds)
            if (sound.category == SoundCategory::loops)
                CHECK (sound.loopBars > 0);
            else
                CHECK (sound.loopBars == 0);
    }
    CHECK (reached == 618);
}

TEST_CASE ("instruments that share their files are one sound, in the most flair category", "[sounds]")
{
    SECTION ("the four kits: no two sounds share a file set")
    {
        for (const auto& entry : embedded().entries())
        {
            CAPTURE (entry.id);
            const auto sounds = sounds::soundsOf (entry.manifest);
            std::set<std::set<int>> sets;
            for (const auto& sound : sounds)
                CHECK (sets.insert (fileSet (sound)).second);
        }
    }

    SECTION ("FUT01: Glitch Fill Lift Beat 4 is Glitch Fill Lift, in FX")
    {
        const auto sounds = sounds::soundsOf (manifestOf ("FUT01"));
        const auto& lift = named (sounds, "Glitch Fill Lift");
        CHECK (lift.id == "FUT01/glitch_fill_lift");
        CHECK (lift.category == SoundCategory::fx);
        CHECK (withId (sounds, "FUT01/glitch_fill_lift_beat_4") == nullptr);
        for (const auto& s : sounds)
            CHECK_FALSE (s.name.contains ("Beat 4"));
    }

    SECTION ("FUT02: Glass Ping and its accents-channel twin are one Accents row")
    {
        const auto sounds = sounds::soundsOf (manifestOf ("FUT02"));
        const auto& ping = named (sounds, "Glass Ping");
        CHECK (ping.id == "FUT02/glass_ping");
        CHECK (ping.category == SoundCategory::accents);
        CHECK (ping.files.size() == 4);
        CHECK (withId (sounds, "FUT02/glass_ping_accents") == nullptr);
    }

    SECTION ("FUT04: Acid Stab E4, Rave Stab Em and Metal Chord Em are one sound each, in Accents")
    {
        const auto sounds = sounds::soundsOf (manifestOf ("FUT04"));
        for (const auto* name : { "Acid Stab E4", "Rave Stab Em", "Metal Chord Em" })
        {
            CAPTURE (name);
            CHECK (named (sounds, name).category == SoundCategory::accents);
        }
        // Metal Chord's first instrument plays on the chords lane (Synths); its twin on the accents lane wins.
        const auto& kit = manifestOf ("FUT04");
        bool chordsLane = false;
        for (const auto& in : kit.instruments)
            if (in.id == "accent_metal_chord_em")
                chordsLane = in.lane == engine::Lane::chords;
        CHECK (chordsLane);
        CHECK (named (sounds, "Metal Chord Em").id == "FUT04/accent_metal_chord_em");
        for (const auto& s : sounds)
            CHECK_FALSE (s.id.endsWith ("_accents"));
    }

    SECTION ("the earlier sound keeps its id, name and place; a pitched newcomer chooses the default file")
    {
        auto kit = engine::testing::syntheticContent().kit; // G minor
        const auto before = sounds::soundsOf (kit).size();
        // A one-key drum on the stab's two files (the first zone: file 22), then a pitched accent on the same files
        // whose zone on the tonic (G4, 67) plays file 23.
        kit.instruments.push_back (instrument ("stab_hits", "Stab Hits", engine::Lane::perc,
            { zone (60, 60, 60, { layer (127, { 22 }) }), zone (61, 61, 61, { layer (127, { 23 }) }) }));
        kit.instruments.push_back (instrument ("stab_accent", "Accent Stab Hits", engine::Lane::accents,
            { zone (62, 40, 64, { layer (127, { 22 }) }), zone (67, 65, 80, { layer (127, { 23 }) }) }));
        const auto sounds = sounds::soundsOf (kit);
        // The stab (chords, files 22 and 23) came first: both newcomers are it.
        CHECK (sounds.size() == before);
        const auto& stab = named (sounds, "Stab");
        CHECK (stab.id == "SYN01/stab");
        CHECK (stab.category == SoundCategory::accents); // Accents beats Synths and Drums
        CHECK (defaultPath (kit, stab) == "Music/Stab G3.wav"); // the stab is pitched already: its own tonic zone

        // On their own, the drum comes first and the pitched accent chooses the default.
        kit.instruments.erase (kit.instruments.begin() + engine::testing::syn::stab);
        const auto merged = sounds::soundsOf (kit);
        const auto& hits = named (merged, "Stab Hits");
        CHECK (hits.id == "SYN01/stab_hits");
        CHECK (hits.category == SoundCategory::accents);
        CHECK (defaultPath (kit, hits) == "Music/Stab D4.wav"); // file 23: the accent's G4 zone, not the drum's first
        CHECK (withId (merged, "SYN01/stab_accent") == nullptr);
    }
}

TEST_CASE ("the default file: the top layer's first round robin, on the kit's tonic for pitched instruments", "[sounds]")
{
    struct Example
    {
        const char* kit;
        const char* sound;
        const char* path;
    };
    const Example examples[] {
        { "FUT01", "Chrome Pluck", "Music/Chrome Pluck F#4 v2.wav" },
        { "FUT01", "Vox Synth", "Vox/Vox Synth F#5 v2.wav" },
        { "FUT02", "Vox Synth", "Vox/Vox Synth D4 v3.wav" }, // the "ee" layer, the top of three
        { "FUT03", "Conga", "Drums/Conga G3 v3 rr1.wav" }, // one key per zone: the first zone
        { "FUT04", "Mod Blip", "Music/Mod Blip E5 v2.wav" },
        { "FUT01", "Kick Split", "Drums/Kick Split v5 rr1.wav" },
    };
    for (const auto& e : examples)
    {
        CAPTURE (e.kit, e.sound);
        const auto& kit = manifestOf (e.kit);
        CHECK (defaultPath (kit, named (sounds::soundsOf (kit), e.sound)) == e.path);
    }

    SECTION ("files: zones low to high, layers soft to hard, then round robins")
    {
        const auto& kit = manifestOf ("FUT01");
        const auto fut01 = sounds::soundsOf (kit);
        const auto& kick = named (fut01, "Kick Split");
        std::vector<juce::String> labels;
        for (const auto& f : kick.files)
            labels.push_back (f.label);
        CHECK (labels
               == std::vector<juce::String> { "Kick Split v1 rr1", "Kick Split v1 rr2", "Kick Split v2 rr1", "Kick Split v2 rr2", "Kick Split v3 rr1",
                   "Kick Split v3 rr2", "Kick Split v4 rr1", "Kick Split v4 rr2", "Kick Split v5 rr1", "Kick Split v5 rr2" });
        CHECK (kick.defaultFile == 8);

        const auto& pluck = named (fut01, "Chrome Pluck");
        REQUIRE (pluck.files.size() == 18);
        CHECK (pluck.files.front().label == "Chrome Pluck C4 v1");
        CHECK (pluck.files[1].label == "Chrome Pluck C4 v2");
        CHECK (pluck.files.back().label == "Chrome Pluck C6 v2");
    }

    SECTION ("the synthetic kit: the first zone on the tonic, else the middle zone")
    {
        auto kit = engine::testing::syntheticContent().kit; // G minor
        auto sounds = sounds::soundsOf (kit);
        CHECK (defaultPath (kit, named (sounds, "Bass A")) == "Bass/Bass A G1.wav"); // G1 and G2: the first
        CHECK (defaultPath (kit, named (sounds, "Bass B")) == "Bass/Bass B G1 v2.wav");
        CHECK (defaultPath (kit, named (sounds, "Vox")) == "Vox/Vox G4 v3.wav");
        CHECK (defaultPath (kit, named (sounds, "Lead")) == "Music/Lead C5.wav"); // one zone, off the tonic
        CHECK (defaultPath (kit, named (sounds, "Kick B")) == "Drums/Kick B v2 rr1.wav");

        kit.kit.key = "A minor"; // no zone of either bass is on A: the middle one, D2
        sounds = sounds::soundsOf (kit);
        CHECK (defaultPath (kit, named (sounds, "Bass A")) == "Bass/Bass A D2.wav");
        CHECK (defaultPath (kit, named (sounds, "Bass B")) == "Bass/Bass B D2 v2.wav");

        kit.kit.key = "not a key"; // no tonic at all: the middle zone too
        CHECK (defaultPath (kit, named (sounds::soundsOf (kit), "Bass A")) == "Bass/Bass A D2.wav");
    }

    SECTION ("the tonic is the key's first word")
    {
        CHECK (sounds::tonicOf ("C major") == 0);
        CHECK (sounds::tonicOf ("F# minor") == 6);
        CHECK (sounds::tonicOf ("D minor") == 2);
        CHECK (sounds::tonicOf ("G minor") == 7);
        CHECK (sounds::tonicOf ("E minor") == 4);
        CHECK (sounds::tonicOf ("Bb major") == 10);
        CHECK (sounds::tonicOf ("B minor") == 11);
        CHECK (sounds::tonicOf ("Cb major") == 11);
        CHECK (sounds::tonicOf ("B# minor") == 0);
        CHECK (sounds::tonicOf ("H minor") == -1);
        CHECK (sounds::tonicOf ("F#m") == -1);
        CHECK (sounds::tonicOf ("") == -1);
    }
}

TEST_CASE ("lengths: whole bars at the kit's tempo, end-aligned sounds, lead-ins and loop cycles", "[sounds]")
{
    const auto fut01 = sounds::soundsOf (manifestOf ("FUT01"));
    const auto& riser = named (fut01, "Shift Riser"); // 687 761 frames: 8 bars at 134 BPM, within a frame
    CHECK (riser.category == SoundCategory::fx);
    CHECK (riser.bars == 8);
    CHECK (riser.endAligned);
    CHECK (riser.loopBars == 0);

    const auto& reverse = named (fut01, "Reverse Stab"); // 1.30 s: no whole bars
    CHECK (reverse.bars == 0);
    CHECK (reverse.endAligned);

    const auto& rave = named (fut01, "Rave Stab");
    CHECK (rave.bars == 0);
    CHECK_FALSE (rave.endAligned);
    CHECK (rave.leadInMs == 0);
    CHECK (rave.loopBars == 0);

    CHECK (named (fut01, "Vox Synth").leadInMs == 6); // 288 frames
    const auto& phrase = named (fut01, "Chrome Pluck Phrase");
    CHECK (phrase.bars == 2);
    CHECK (phrase.loopBars == 2);
    CHECK_FALSE (phrase.endAligned);

    const auto fut02 = sounds::soundsOf (manifestOf ("FUT02"));
    CHECK (named (fut02, "Vox Chop Ah D5").leadInMs == 20); // 960 frames
    CHECK (named (fut02, "Context Vox Riff").loopBars == 4);

    const auto fut03 = sounds::soundsOf (manifestOf ("FUT03"));
    CHECK (named (fut03, "Vox Chop Ooh Ah G4").leadInMs == 25); // 1200 frames
    const auto& riff = named (fut03, "Context Vox Riff"); // a 4-bar loop, then a 2-bar echo tail
    CHECK (riff.category == SoundCategory::loops);
    CHECK (riff.bars == 6);
    CHECK (riff.loopBars == 4);

    SECTION ("within 48 frames (1 ms) of whole bars, and at least one")
    {
        auto kit = engine::testing::syntheticContent().kit;
        kit.kit.bpm = 120; // 96 000 frames a bar
        auto& riserFile = kit.files[(std::size_t) fileIndex (kit, "FX/Riser.wav")];
        const auto barsFor = [&] (std::int64_t frames) {
            riserFile.frames = frames;
            return named (sounds::soundsOf (kit), "Riser").bars;
        };
        CHECK (barsFor (192000) == 2);
        CHECK (barsFor (192048) == 2);
        CHECK (barsFor (191952) == 2);
        CHECK (barsFor (192049) == 0);
        CHECK (barsFor (191951) == 0);
        CHECK (barsFor (96000) == 1);
        CHECK (barsFor (48) == 0); // round () is 0: never "0 bars"
        CHECK (barsFor (47999) == 0);
        riserFile.prerollFrames = 72; // 1.5 ms rounds to 2
        CHECK (named (sounds::soundsOf (kit), "Riser").leadInMs == 2);
    }
}

TEST_CASE ("names drop a leading 'Accent '; ids are unique, stable and name the kit", "[sounds]")
{
    const auto fut03 = sounds::soundsOf (manifestOf ("FUT03"));
    const auto& dub = named (fut03, "Dub Chord Cm9");
    CHECK (dub.id == "FUT03/accent_dub_chord_cm9");
    CHECK (dub.category == SoundCategory::accents);
    CHECK (named (fut03, "Context Vox Riff").id == "FUT03/Stems/Context Vox Riff.wav");

    const auto fut01 = sounds::soundsOf (manifestOf ("FUT01"));
    CHECK (named (fut01, "Chrome Pluck Phrase").id == "FUT01/Loops/Chrome Pluck Phrase.wav");
    CHECK (named (fut01, "Rave Stab").id == "FUT01/rave_stab");

    std::set<juce::String> ids;
    for (const auto& entry : embedded().entries())
    {
        CAPTURE (entry.id);
        const auto prefix = juce::String (entry.id) + "/";
        for (const auto& sound : sounds::soundsOf (entry.manifest))
        {
            CAPTURE (sound.id.toStdString());
            CHECK (sound.id.startsWith (prefix));
            CHECK (ids.insert (sound.id).second); // unique, across the kits too
            CHECK (sound.name.isNotEmpty());
            CHECK_FALSE (sound.name.startsWith ("Accent "));
        }
    }
    CHECK (ids.size() == 35 + 37 + 36 + 32);
}

TEST_CASE ("the synthetic kit has 13 sounds", "[sounds]")
{
    const auto& kit = engine::testing::syntheticContent().kit;
    const auto sounds = sounds::soundsOf (kit);
    std::vector<juce::String> names;
    for (const auto& s : sounds)
        names.push_back (s.name);
    // The instruments in manifest order, then the phrase stem.
    CHECK (names
           == std::vector<juce::String> { "Kick A", "Kick B", "Snare", "Closed Hat", "Open Hat", "Bass A", "Bass B", "Lead", "Stab", "Pad", "Vox",
               "Riser", "Lead Phrase" });
    CHECK (byCategory (sounds) == std::array<int, kSoundCategories> { 0, 1, 1, 3, 2, 5, 1 });

    std::set<int> files;
    for (const auto& s : sounds)
        for (const auto& f : s.files)
            files.insert (f.file);
    CHECK (files.size() == 30);

    const auto& riser = named (sounds, "Riser");
    CHECK (riser.endAligned);
    CHECK (riser.id == "SYN01/riser");
    CHECK (named (sounds, "Vox").leadInMs == 20);
    const auto& stem = named (sounds, "Lead Phrase");
    CHECK (stem.id == "SYN01/Loops/Lead Phrase.wav");
    CHECK (stem.loopBars == 2);
    CHECK (stem.bars == 2); // 180 000 frames: 2 bars of 90 000 at 128 BPM
}

TEST_CASE ("the catalog is the same every time", "[sounds]")
{
    const KitCatalog again (EmbeddedContent::sources());
    REQUIRE (again.entries().size() == embedded().entries().size());
    for (std::size_t i = 0; i < again.entries().size(); ++i)
    {
        const auto& entry = embedded().entries()[i];
        CAPTURE (entry.id);
        const auto sounds = sounds::soundsOf (entry.manifest);
        CHECK_FALSE (sounds.empty());
        CHECK (sounds == sounds::soundsOf (entry.manifest));
        CHECK (entry.sounds == sounds); // what the catalog keeps for the Controller
        CHECK (again.entries()[i].sounds == sounds);
    }
    CHECK (sounds::soundsOf (engine::testing::syntheticContent().kit) == sounds::soundsOf (engine::testing::buildSyntheticContent().kit));
}
