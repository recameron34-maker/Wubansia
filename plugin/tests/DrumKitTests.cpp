// Drum kits (ADR 0030, sounds/DrumKit.h and DrumKitWriter.h): the plan from the manifests alone, its SFZ read back
// against the engine's own selection code, the committed texts byte for byte, and the folder written once, never over
// anything. The writer's copies come from the synthetic kit's WAVs; the hidden [.real] case uses installed content, and
// the hidden [.golden] case writes the four kits' texts:
//     WUBANSIA_DRUMKIT_GOLDEN=plugin/tests/data/drumkit WubansiaPluginTests "[drumkit][.golden]"

#include "content/ContentLocator.h"
#include "content/EmbeddedContent.h"
#include "content/InstalledContent.h"
#include "content/UserSettings.h"
#include "dragout/DragFiles.h"
#include "helpers/sfz_reader.h"
#include "helpers/synthetic_content.h"
#include "services/KitCatalog.h"
#include "sounds/DrumKit.h"
#include "sounds/DrumKitWriter.h"
#include "sounds/SampleExport.h"

#include "sampler/selection.h"
#include "testing/synthetic_kit.h"
#include "util/sha256.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <map>
#include <numeric>
#include <set>
#include <string>
#include <vector>

#if ! JUCE_WINDOWS
    #include <sys/stat.h>
    #include <unistd.h>
#endif

using namespace wubansia;

namespace
{
    struct TempFolder
    {
        TempFolder() : folder (juce::File::getSpecialLocation (juce::File::tempDirectory).getNonexistentChildFile ("WubansiaDrumKitTest", "", false))
        {
            folder.createDirectory();
        }
        ~TempFolder() { folder.deleteRecursively(); }

        juce::File operator/ (const char* child) const { return folder.getChildFile (child); }

        juce::File folder;
    };

    const juce::Time past (2001, 0, 1, 12, 0, 0, 0, false);

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

    const engine::KitManifest& syn() { return engine::testing::syntheticContent().kit; }
    const engine::MixConfig& synMix() { return engine::testing::syntheticContent().mix; }

    sounds::DrumKitPlan planOf (const engine::KitManifest& kit, const engine::MixConfig& mix)
    {
        auto plan = sounds::planDrumKit (kit, mix);
        INFO (plan.error);
        REQUIRE (plan.ok());
        return plan.value;
    }

    sounds::DrumKitPlan planOf (std::string_view kit)
    {
        const auto& entry = kitEntry (kit);
        return planOf (entry.manifest, entry.mix);
    }

    sounds::DrumKitPlan synPlan() { return planOf (syn(), synMix()); }

    juce::String text (const std::string& s) { return juce::String::fromUTF8 (s.data(), (int) s.size()); }

    std::vector<std::uint8_t> bytesOf (const juce::File& file)
    {
        juce::MemoryBlock block;
        if (! file.loadFileAsData (block))
            return {};
        const auto* data = static_cast<const std::uint8_t*> (block.getData());
        return std::vector<std::uint8_t> (data, data + block.getSize());
    }

    std::string stringOf (const juce::File& file)
    {
        const auto bytes = bytesOf (file);
        return std::string (bytes.begin(), bytes.end());
    }

    const sounds::DrumKitFile* fileNamed (const sounds::DrumKitPlan& plan, const std::string& path)
    {
        for (const auto& f : plan.files)
            if (f.path == path)
                return &f;
        return nullptr;
    }

    // A file's name in kit.json, less folder and ".wav".
    std::string stemOf (const std::string& path)
    {
        auto name = path.substr (path.find_last_of ('/') + 1);
        return name.substr (0, name.size() - 4);
    }

    // The instrument the kit's drum keymap plays for key on side (-1: no switch).
    int instrumentFor (const engine::KitManifest& kit, int key, int side)
    {
        for (const auto& e : kit.keymap)
            if (e.ch == 10 && e.lo <= key && key <= e.hi)
                return side == 0 || e.b < 0 ? e.a : e.b;
        return -1;
    }

    // Every folder entry below folder, '/'-separated relative paths, hidden ones included.
    std::set<std::string> treeOf (const juce::File& folder)
    {
        std::set<std::string> paths;
        for (const auto& entry : juce::RangedDirectoryIterator (folder, true, "*", juce::File::findFilesAndDirectories))
            paths.insert (entry.getFile().getRelativePathFrom (folder).replaceCharacter ('\\', '/').toStdString());
        return paths;
    }

    int entriesIn (const juce::File& folder) { return folder.getNumberOfChildFiles (juce::File::findFilesAndDirectories); }

    // The synthetic kit's drum kit written from SyntheticContent into samples.
    sounds::DrumKitWrite writeSyn (test::SyntheticContent& content, const juce::File& samples, const engine::KitManifest& kit = syn(),
        const std::atomic<bool>* cancelFlag = nullptr, const std::function<void (int)>& progress = {})
    {
        const auto found = content.locate (syn());
        REQUIRE (found.state == ContentState::ready);
        static const std::atomic<bool> never { false };
        return sounds::writeDrumKit (content, found, kit, planOf (kit, synMix()), samples, cancelFlag != nullptr ? *cancelFlag : never, progress);
    }

    const char* const kits[] { "FUT01", "FUT02", "FUT03", "FUT04" };
}

TEST_CASE ("drum note names follow the drum maps", "[drumkit]")
{
    // As tools/content/tests/test_install_drums.py's note_name: middle C (60) is C3, so 36 is C1.
    const char* const names[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    for (int key = 35; key <= 59; ++key)
        CHECK (sounds::drumNoteName (key) == std::string (names[key % 12]) + std::to_string (key / 12 - 2));
    CHECK (sounds::drumNoteName (35) == "B0");
    CHECK (sounds::drumNoteName (36) == "C1");
    CHECK (sounds::drumNoteName (37) == "C#1");
    CHECK (sounds::drumNoteName (54) == "F#2");
    CHECK (sounds::drumNoteName (59) == "B2");
}

TEST_CASE ("folder names: <KIT> <Title> Drum Kit, c<N> from content v2", "[drumkit]")
{
    CHECK (sounds::drumKitFolderName (kitEntry ("FUT01").manifest.kit) == "FUT01 Alloy Signal Drum Kit");
    CHECK (sounds::drumKitFolderName (kitEntry ("FUT02").manifest.kit) == "FUT02 Orbit Relay Drum Kit");
    CHECK (sounds::drumKitFolderName (kitEntry ("FUT03").manifest.kit) == "FUT03 Glass Current Drum Kit");
    CHECK (sounds::drumKitFolderName (kitEntry ("FUT04").manifest.kit) == "FUT04 Furnace Code Drum Kit");
    auto meta = syn().kit;
    CHECK (sounds::drumKitFolderName (meta) == "SYN01 Synthetic Test Kit Drum Kit");
    meta.contentVersion = 2;
    CHECK (sounds::drumKitFolderName (meta) == "SYN01 Synthetic Test Kit Drum Kit c2");
    meta.title = "Odd/Name: \"x\"";
    CHECK (sounds::drumKitFolderName (meta) == "SYN01 Odd Name x Drum Kit c2"); // safe characters only
}

TEST_CASE ("the pads are every channel-10 key, both kick sides, with sound, level and pan", "[drumkit]")
{
    struct Pad
    {
        int key, side;
        const char* sound;
        int level, pan;
    };
    const std::map<std::string, std::vector<Pad>> expected {
        { "FUT01", { { 36, 0, "Kick Split", 0, 0 }, { 36, 1, "Kick Garage", 0, 0 }, { 37, -1, "Rim Shot", -11000, 300 }, { 38, -1, "Clap Garage", -2000, 0 },
                       { 42, -1, "Hat Closed", -9500, 220 }, { 46, -1, "Hat Open", -12500, -150 }, { 47, -1, "Alloy Hit", -8000, -250 },
                       { 54, -1, "Tambourine", -12500, -300 } } },
        { "FUT02", { { 36, -1, "Kick Split", -2000, 0 }, { 38, -1, "Electro Clap", -2500, 0 }, { 42, -1, "Closed Hat", -3440, 150 }, { 46, -1, "Open Hat", -3440, 150 },
                       { 47, -1, "Zap", -12300, 0 } } },
        { "FUT03", { { 36, -1, "Kick Deep", 0, 0 }, { 37, -1, "Rimshot", -5000, 0 }, { 38, -1, "Clap Snare", -1000, 0 }, { 41, -1, "Conga G3", -5000, 0 },
                       { 42, -1, "Closed Hat Deep", -5000, 150 }, { 43, -1, "Conga C4", -5000, 0 }, { 45, -1, "Conga D4", -5000, 0 },
                       { 46, -1, "Open Hat Deep", -5000, 150 }, { 47, -1, "Bongo G4", -5000, 0 }, { 48, -1, "Bongo D5", -5000, 0 },
                       { 54, -1, "Shaker Deep", -9000, -200 } } },
        { "FUT04", { { 35, -1, "Rumble", -14420, 0 }, { 36, -1, "Kick Split", -2658, 0 }, { 38, -1, "Snare Gated", -850, 0 }, { 42, -1, "Closed Hat", -3717, 150 },
                       { 46, -1, "Open Hat", -3717, 150 }, { 47, -1, "Anvil", -6328, -250 }, { 51, -1, "Ride", -14829, 0 } } },
        { "SYN01", { { 36, 0, "Kick A", -1000, 0 }, { 36, 1, "Kick B", -1000, 0 }, { 38, -1, "Snare", 0, 100 }, { 42, -1, "Closed Hat", 0, 0 }, { 46, -1, "Open Hat", 0, 0 } } },
    };
    for (const auto& [kit, pads] : expected)
    {
        CAPTURE (kit);
        const auto& manifest = kit == "SYN01" ? syn() : kitEntry (kit).manifest;
        const auto plan = kit == "SYN01" ? synPlan() : planOf (kit);
        REQUIRE (plan.pads.size() == pads.size());
        for (std::size_t i = 0; i < pads.size(); ++i)
        {
            CAPTURE (i);
            const auto& got = plan.pads[i];
            CHECK (got.key == pads[i].key);
            CHECK (got.side == pads[i].side);
            CHECK (got.sound == pads[i].sound);
            CHECK (got.levelMdb == pads[i].level);
            CHECK (got.panPm == pads[i].pan);
            // The instrument and zone are the keymap's and the zone holding the key.
            REQUIRE (got.instrument == instrumentFor (manifest, got.key, got.side));
            const auto& zone = manifest.instruments[(std::size_t) got.instrument].zones[(std::size_t) got.zone];
            CHECK ((zone.lo <= got.key && got.key <= zone.hi));
        }
    }
}

TEST_CASE ("the folder lists each file once", "[drumkit]")
{
    const std::map<std::string, std::array<int, 3>> counts { { "FUT01", { 100, 8, 89 } }, { "FUT02", { 67, 5, 60 } }, { "FUT03", { 115, 11, 102 } },
        { "FUT04", { 80, 7, 71 } }, { "SYN01", { 20, 5, 12 } } };
    for (const auto& [kit, count] : counts)
    {
        CAPTURE (kit);
        const auto& manifest = kit == "SYN01" ? syn() : kitEntry (kit).manifest;
        const auto plan = kit == "SYN01" ? synPlan() : planOf (kit);
        CHECK ((int) plan.files.size() == count[0]);
        CHECK (plan.wavs + plan.texts == (int) plan.files.size());

        std::set<std::string> lower;
        std::vector<std::string> pads, samples;
        for (const auto& f : plan.files)
        {
            CAPTURE (f.path);
            CHECK (lower.insert (juce::String (f.path).toLowerCase().toStdString()).second); // unique ignoring case
            const auto name = f.path.substr (f.path.find_last_of ('/') + 1);
            CHECK (name.size() <= sounds::kMaxNameStem);
            CHECK (dragout::safeName (name) == name);
            if (f.path.rfind ("Pads/", 0) == 0)
                pads.push_back (f.path);
            else if (f.path.rfind ("Samples/", 0) == 0)
                samples.push_back (f.path);
            CHECK ((f.source >= 0) == f.path.ends_with (".wav"));
            CHECK (f.text.empty() == (f.source >= 0));
        }
        CHECK ((int) pads.size() == count[1]);
        CHECK ((int) samples.size() == count[2]);
        CHECK (plan.wavs == count[1] + count[2]);

        // Pads/: one per pad, its zone's top layer's first round robin. Samples/: every round robin of every layer.
        std::set<std::string> wantPads, wantSamples;
        for (const auto& pad : plan.pads)
        {
            const auto& zone = manifest.instruments[(std::size_t) pad.instrument].zones[(std::size_t) pad.zone];
            const auto prefix = std::to_string (pad.key) + " " + sounds::drumNoteName (pad.key) + " ";
            const auto top = zone.layers.back().rr.front();
            wantPads.insert ("Pads/" + prefix + pad.sound + " - " + manifest.kit.id + ".wav");
            const auto* padFile = fileNamed (plan, "Pads/" + prefix + pad.sound + " - " + manifest.kit.id + ".wav");
            REQUIRE (padFile != nullptr);
            CHECK (padFile->source == top);
            for (const auto& layer : zone.layers)
                for (const auto r : layer.rr)
                {
                    const auto path = "Samples/" + prefix + stemOf (manifest.files[(std::size_t) r].path) + " - " + manifest.kit.id + ".wav";
                    wantSamples.insert (path);
                    const auto* sample = fileNamed (plan, path);
                    REQUIRE (sample != nullptr);
                    CHECK (sample->source == r);
                }
        }
        CHECK (std::set<std::string> (pads.begin(), pads.end()) == wantPads);
        CHECK (std::set<std::string> (samples.begin(), samples.end()) == wantSamples);
        CHECK (plan.files.back().path == "README.txt");
    }
    CHECK (fileNamed (planOf ("FUT01"), "Pads/36 C1 Kick Garage - FUT01.wav") != nullptr);
    CHECK (fileNamed (planOf ("FUT01"), "Samples/36 C1 Kick Garage v1 rr1 - FUT01.wav") != nullptr);
    CHECK (fileNamed (planOf ("FUT03"), "Pads/41 F1 Conga G3 - FUT03.wav") != nullptr);
}

TEST_CASE ("one SFZ per kick side, else one", "[drumkit]")
{
    const auto fut01 = planOf ("FUT01");
    CHECK (fut01.sfz[0] == "FUT01 Alloy Signal Drum Kit - Kick Split.sfz");
    CHECK (fut01.sfz[1] == "FUT01 Alloy Signal Drum Kit - Kick Garage.sfz");
    CHECK (fileNamed (fut01, fut01.sfz[0]) != nullptr);
    CHECK (fileNamed (fut01, fut01.sfz[1]) != nullptr);
    CHECK (fut01.texts == 3);
    for (const auto* kit : { "FUT02", "FUT03", "FUT04" })
    {
        CAPTURE (kit);
        const auto plan = planOf (kit);
        CHECK (plan.sfz[0] == plan.folder + ".sfz");
        CHECK (plan.sfz[1] == plan.sfz[0]);
        CHECK (fileNamed (plan, plan.sfz[0]) != nullptr);
        CHECK (plan.texts == 2);
    }
    const auto synthetic = synPlan();
    CHECK (synthetic.sfz[0] == "SYN01 Synthetic Test Kit Drum Kit - Kick A.sfz");
    CHECK (synthetic.sfz[1] == "SYN01 Synthetic Test Kit Drum Kit - Kick B.sfz");
}

namespace
{
    // Every (key, velocity, sequence position) of every pad on side, read back from the SFZ: exactly one region plays,
    // and its sample is the file engine::pickZone, engine::pickLayer and rr % count give, panned by the cycle's entry.
    void checkPlays (const engine::KitManifest& kit, const sounds::DrumKitPlan& plan, int side)
    {
        const auto* file = fileNamed (plan, plan.sfz[(std::size_t) side]);
        REQUIRE (file != nullptr);
        const auto sfz = test::readSfz (file->text);
        INFO (plan.sfz[(std::size_t) side] << ": " << sfz.error);
        REQUIRE (sfz.error.empty());
        CHECK (sfz.control.at ("default_path") == "Samples/");
        std::set<int> keys;
        for (const auto& pad : plan.pads)
        {
            if (pad.side >= 0 && pad.side != side)
                continue;
            keys.insert (pad.key);
            CAPTURE (pad.key);
            const auto& instrument = kit.instruments[(std::size_t) pad.instrument];
            std::vector<engine::Zone> zones;
            for (const auto& z : instrument.zones)
                zones.push_back ({ (std::uint8_t) z.root, (std::uint8_t) z.lo, (std::uint8_t) z.hi, 0, 0 });
            const auto zoneIndex = engine::pickZone (zones, pad.key);
            REQUIRE (zoneIndex >= 0);
            const auto& zone = instrument.zones[(std::size_t) zoneIndex];
            std::vector<engine::Layer> layers;
            for (const auto& l : zone.layers)
                layers.push_back ({ (std::uint8_t) l.vel, 0, (std::uint8_t) l.rr.size() });

            int length = 1;
            for (const auto& l : zone.layers)
                length = std::lcm (length, (int) l.rr.size());
            if (! instrument.panCyclePm.empty())
                length = std::lcm (length, (int) instrument.panCyclePm.size());

            for (int velocity = 1; velocity <= 127; ++velocity)
                for (int position = 1; position <= length; ++position)
                {
                    std::vector<const test::SfzRegion*> playing;
                    for (const auto& r : sfz.regions)
                        if (test::SfzFile::playsKey (r, pad.key) && r.number ("lovel", 1) <= velocity && velocity <= r.number ("hivel", 127)
                            && (! r.has ("seq_position") || r.number ("seq_position", 1) == position))
                            playing.push_back (&r);
                    CAPTURE (velocity, position);
                    REQUIRE (playing.size() == 1);
                    const auto& region = *playing.front();
                    CHECK (region.number ("seq_length", 1) == length);
                    const auto& layer = zone.layers[(std::size_t) engine::pickLayer (layers, velocity)];
                    const auto rr = layer.rr[(std::size_t) (position - 1) % layer.rr.size()];
                    CHECK (region.at ("sample") == std::to_string (pad.key) + " " + sounds::drumNoteName (pad.key) + " " + stemOf (kit.files[(std::size_t) rr].path)
                                                       + " - " + kit.kit.id + ".wav");
                    const auto pan = instrument.panCyclePm.empty() ? pad.panPm
                                                                   : instrument.panCyclePm[(std::size_t) (position - 1) % instrument.panCyclePm.size()];
                    CHECK (std::lround (std::stod (region.has ("pan") ? region.at ("pan") : "0") * 10.0) == pan);
                    CHECK (region.has ("key") == (zone.root == pad.key));
                }
        }
        // And nothing else: every region plays one of the side's pads.
        for (const auto& r : sfz.regions)
            CHECK (keys.count (r.number ("key", r.number ("lokey", -1))) == 1);
    }
}

TEST_CASE ("each SFZ plays the file Wubansia plays for every pad, velocity and round robin", "[drumkit]")
{
    for (const auto* kit : kits)
    {
        CAPTURE (kit);
        const auto plan = planOf (kit);
        checkPlays (kitEntry (kit).manifest, plan, 0);
        checkPlays (kitEntry (kit).manifest, plan, 1);
    }
    checkPlays (syn(), synPlan(), 0);
    checkPlays (syn(), synPlan(), 1);
}

TEST_CASE ("levels, pans, chokes and loops are the kit's", "[drumkit]")
{
    for (const auto* kit : kits)
    {
        CAPTURE (kit);
        const auto& manifest = kitEntry (kit).manifest;
        const auto plan = planOf (kit);
        for (int side = 0; side < 2; ++side)
        {
            const auto sfz = test::readSfz (fileNamed (plan, plan.sfz[(std::size_t) side])->text);
            REQUIRE (sfz.error.empty());
            CHECK (sfz.global.at ("amp_veltrack") == "0");
            for (const auto& r : sfz.regions)
            {
                const auto key = r.number ("key", r.number ("lokey", -1));
                const auto pad = std::find_if (plan.pads.begin(), plan.pads.end(), [&] (const auto& p) { return p.key == key && (p.side < 0 || p.side == side); });
                REQUIRE (pad != plan.pads.end());
                const auto& instrument = manifest.instruments[(std::size_t) pad->instrument];
                CAPTURE (key, instrument.id);
                CHECK (std::lround (std::stod (r.has ("volume") ? r.at ("volume") : "0") * 1000.0) == pad->levelMdb);
                if (instrument.choke.role == engine::ChokeRole::chokes)
                {
                    CHECK (r.at ("group") == "1");
                    CHECK_FALSE (r.has ("off_by"));
                }
                else if (instrument.choke.role == engine::ChokeRole::choked)
                {
                    CHECK (r.at ("group") == "2");
                    CHECK (r.at ("off_by") == "1");
                    CHECK (r.at ("off_mode") == "normal");
                    CHECK (r.at ("ampeg_release") == (std::string (kit) == "FUT01" ? "0.005" : "0.004"));
                }
                else
                    CHECK_FALSE (r.has ("group"));
                if (instrument.mode == engine::SampleMode::sustainLoop)
                {
                    CHECK (std::string (kit) == "FUT04");
                    CHECK (key == 35);
                    CHECK (r.at ("loop_mode") == "loop_sustain");
                    CHECK (r.at ("loop_start") == "0");
                    CHECK (r.at ("loop_end") == "169411");
                    CHECK (r.at ("ampeg_release") == "0.005");
                }
                else
                    CHECK (r.at ("loop_mode") == "one_shot");
            }
        }
    }
}

TEST_CASE ("the texts are the committed ones, byte for byte", "[drumkit]")
{
    const auto data = juce::File (WUBANSIA_SOURCE_DIR).getChildFile ("plugin/tests/data/drumkit");
    for (const auto* kit : kits)
    {
        CAPTURE (kit);
        const auto plan = planOf (kit);
        CHECK (planOf (kit) == plan); // two plans are equal
        int texts = 0;
        for (const auto& f : plan.files)
        {
            if (f.source >= 0)
                continue;
            ++texts;
            CAPTURE (f.path);
            const auto golden = data.getChildFile (text (plan.folder)).getChildFile (text (f.path));
            REQUIRE (golden.existsAsFile());
            CHECK (f.text == stringOf (golden));
            CHECK (f.text.ends_with ("\n"));
            CHECK (f.text.find ('\r') == std::string::npos);
            CHECK (f.text.find ('\t') == std::string::npos);
            CHECK (std::all_of (f.text.begin(), f.text.end(), [] (char c) { return (unsigned char) c < 0x80; }));
        }
        CHECK (texts == plan.texts);
        CHECK (data.getChildFile (text (plan.folder)).getNumberOfChildFiles (juce::File::findFiles) == plan.texts);
    }
}

TEST_CASE ("a pad the SFZ can't carry is refused by name", "[drumkit]")
{
    using engine::testing::syn::Instrument;
    struct Case
    {
        const char* what;
        std::function<void (engine::KitManifest&, engine::MixConfig&)> change;
        const char* pad; // "38 D1 (Snare)"
        const char* why;
    };
    const Case cases[] {
        { "end-aligned", [] (auto& k, auto&) { k.instruments[engine::testing::syn::kickA].align = engine::Align::end; }, "36 C1 (Kick A)", "end-aligned" },
        { "mono", [] (auto& k, auto&) { k.instruments[engine::testing::syn::hatClosed].mono = true; }, "42 F#1 (Closed Hat)", "one note at a time" },
        { "a glide", [] (auto& k, auto&) { k.instruments[engine::testing::syn::snare].glide = { engine::GlideCurve::linear, 20000, 0 }; }, "38 D1 (Snare)", "glides" },
        { "layered", [] (auto& k, auto&) { k.instruments[engine::testing::syn::snare].layeredWith = engine::testing::syn::hatOpen; }, "38 D1 (Snare)", "layered" },
        { "a pre-roll", [] (auto& k, auto&) { k.files[5].prerollFrames = 480; }, "38 D1 (Snare)", "pre-roll" },
        { "pow15", [] (auto& k, auto&) { k.instruments[engine::testing::syn::kickB].velocityLaw = engine::VelocityLaw::pow15; }, "36 C1 (Kick B)", "velocity" },
        { "seq_length 112",
            [] (auto& k, auto&) {
                auto& hat = k.instruments[engine::testing::syn::hatClosed];
                hat.zones[0].layers[0].rr = { 0, 1, 2, 3, 4, 5, 6 };
                hat.panCyclePm.assign (16, 100);
            },
            "42 F#1 (", "112" },
        { "restrike on a one-shot", [] (auto& k, auto&) { k.instruments[engine::testing::syn::snare].restrikeContinues = true; }, "38 D1 (Snare)", "struck again" },
        { "a level on the vox switch",
            [] (auto&, auto& m) {
                engine::Insert gain;
                gain.type = engine::InsertType::gain;
                gain.when.variant = 1;
                gain.params = engine::GainParams { -3000 };
                m.channels[(std::size_t) m.channelIndex ("snare")].inserts.push_back (gain);
            },
            "38 D1 (Snare)", "vox switch" },
    };
    for (const auto& c : cases)
    {
        CAPTURE (c.what);
        auto kit = syn();
        auto mix = synMix();
        c.change (kit, mix);
        const auto refused = sounds::planDrumKit (kit, mix);
        REQUIRE_FALSE (refused.ok());
        CHECK (refused.error.rfind (std::string ("SYN01's pad ") + c.pad, 0) == 0);
        CHECK (refused.error.find (c.why) != std::string::npos);
        CHECK (refused.error.ends_with ("which a drum kit can't carry"));
    }

    // restrike_continues is a looped pad's (FUT04's rumble): accepted there. SYN01's own restrike pad is on channel 11.
    CHECK (syn().instruments[engine::testing::syn::pad].restrikeContinues);
    CHECK (synPlan().pads.size() == 5);
    auto looped = syn();
    auto& snare = looped.instruments[engine::testing::syn::snare];
    snare.mode = engine::SampleMode::sustainLoop;
    snare.restrikeContinues = true;
    for (const auto& layer : snare.zones[0].layers)
        for (const auto r : layer.rr)
        {
            looped.files[(std::size_t) r].loopStart = 0;
            looped.files[(std::size_t) r].loopEnd = 4800;
        }
    CHECK (sounds::planDrumKit (looped, synMix()).ok());
    CHECK (kitEntry ("FUT04").manifest.instruments[(std::size_t) planOf ("FUT04").pads.front().instrument].restrikeContinues);
}

TEST_CASE ("[.golden] writes the four kits' drum kit texts", "[drumkit][.golden]")
{
    const auto dir = juce::SystemStats::getEnvironmentVariable ("WUBANSIA_DRUMKIT_GOLDEN", {});
    if (dir.isEmpty())
        SKIP ("set WUBANSIA_DRUMKIT_GOLDEN to the folder to write into");
    const auto root = juce::File::getCurrentWorkingDirectory().getChildFile (dir);
    for (const auto* kit : kits)
    {
        const auto plan = planOf (kit);
        const auto folder = root.getChildFile (text (plan.folder));
        folder.deleteRecursively();
        REQUIRE (folder.createDirectory().wasOk());
        for (const auto& f : plan.files)
            if (f.source < 0)
                REQUIRE (folder.getChildFile (text (f.path)).replaceWithData (f.text.data(), f.text.size()));
    }
}

TEST_CASE ("a drum kit is written whole", "[drumkit][export]")
{
    TempFolder temp;
    test::SyntheticContent content;
    const auto samples = temp / "Samples";
    const auto plan = synPlan();
    const auto written = writeSyn (content, samples);
    INFO (written.failure.problem << " / " << written.failure.detail);
    REQUIRE (written.folder.isDirectory());
    CHECK (written.wrote);
    CHECK_FALSE (written.cancelled);
    CHECK (written.folder == samples.getChildFile ("SYN01 Synthetic Test Kit Drum Kit"));

    std::set<std::string> want { "Pads", "Samples" };
    for (const auto& f : plan.files)
    {
        CAPTURE (f.path);
        want.insert (f.path);
        const auto file = written.folder.getChildFile (text (f.path));
        REQUIRE (file.existsAsFile());
        if (f.source >= 0)
            CHECK (bytesOf (file) == engine::testing::syntheticContent().wavs[(std::size_t) f.source]);
        else
            CHECK (stringOf (file) == f.text);
    }
    CHECK (treeOf (written.folder) == want); // nothing else
    CHECK (entriesIn (samples) == 1); // no staging folder left
    CHECK (sounds::holdsDrumKit (written.folder, syn(), plan));
}

TEST_CASE ("saved again, nothing is written", "[drumkit][export]")
{
    TempFolder temp;
    test::SyntheticContent content;
    const auto samples = temp / "Samples";
    const auto first = writeSyn (content, samples);
    REQUIRE (first.wrote);
    for (const auto& entry : juce::RangedDirectoryIterator (first.folder, true, "*", juce::File::findFiles))
        entry.getFile().setLastModificationTime (past);

    const auto reads = content.fileReads.load();
    const auto second = writeSyn (content, samples);
    CHECK (second.folder == first.folder);
    CHECK_FALSE (second.wrote);
    CHECK (second.failure.problem.empty());
    CHECK (content.fileReads.load() == reads); // the installed files aren't read: the folder holds kit.json's bytes
    for (const auto& entry : juce::RangedDirectoryIterator (first.folder, true, "*", juce::File::findFiles))
        CHECK (entry.getFile().getLastModificationTime() == past);
    CHECK (entriesIn (samples) == 1);
}

TEST_CASE ("a changed folder is never touched", "[drumkit][export]")
{
    const auto plan = synPlan();
    const auto wav = juce::String ("Samples/38 D1 Snare v1 rr1 - SYN01.wav");
    const auto sfz = text (plan.sfz[1]);
    struct Change
    {
        const char* what;
        std::function<void (const juce::File&)> apply;
    };
    const Change changes[] {
        { "an edited WAV", [&] (const juce::File& f) { f.getChildFile (wav).appendData ("x", 1); } },
        { "a deleted WAV", [&] (const juce::File& f) { f.getChildFile (wav).deleteFile(); } },
        { "an edited SFZ", [&] (const juce::File& f) { f.getChildFile (sfz).appendText ("// mine\n"); } },
    };
    for (const auto& change : changes)
    {
        CAPTURE (change.what);
        TempFolder temp;
        test::SyntheticContent content;
        const auto samples = temp / "Samples";
        const auto first = writeSyn (content, samples);
        REQUIRE (first.wrote);
        change.apply (first.folder);
        const auto before = treeOf (first.folder);
        const auto edited = bytesOf (first.folder.getChildFile (wav));

        const auto second = writeSyn (content, samples);
        CHECK (second.wrote);
        CHECK (second.folder == samples.getChildFile ("SYN01 Synthetic Test Kit Drum Kit (2)"));
        CHECK (treeOf (first.folder) == before); // never touched
        CHECK (bytesOf (first.folder.getChildFile (wav)) == edited);

        const auto third = writeSyn (content, samples);
        CHECK_FALSE (third.wrote);
        CHECK (third.folder == second.folder);
        CHECK (entriesIn (samples) == 2);
    }

    // Live's analysis files beside the WAVs don't count as a change.
    TempFolder temp;
    test::SyntheticContent content;
    const auto samples = temp / "Samples";
    const auto first = writeSyn (content, samples);
    REQUIRE (first.folder.getChildFile ("Pads/38 D1 Snare - SYN01.wav.asd").replaceWithText ("analysis"));
    const auto again = writeSyn (content, samples);
    CHECK_FALSE (again.wrote);
    CHECK (again.folder == first.folder);
}

TEST_CASE ("the kit's own file missing, unreadable or not kit.json's", "[drumkit][export]")
{
    const auto& kit = syn();
    {
        // Not kit.json's: the drawer's words, which say to reinstall.
        TempFolder temp;
        test::SyntheticContent::Options options;
        options.alteredPath = "Drums/Snare v2 rr1.wav";
        test::SyntheticContent content (options);
        const auto samples = temp / "Samples";
        const auto refused = writeSyn (content, samples);
        CHECK (refused.folder == juce::File());
        CHECK_FALSE (refused.wrote);
        CHECK (refused.failure.problem == "Snare v2 rr1.wav isn't the file SYN01's content v1 lists: reinstall SYN01");
        CHECK (refused.failure.sourceProblem);
        CHECK (refused.failure.reinstall);
        CHECK (entriesIn (samples) == 0); // nothing written, no staging
    }
    {
        // Missing: a manifest naming a file the content doesn't hold.
        TempFolder temp;
        test::SyntheticContent content;
        auto changed = kit;
        changed.files[5].path = "Drums/Gone.wav";
        const auto samples = temp / "Samples";
        const auto refused = writeSyn (content, samples, changed);
        CHECK (refused.folder == juce::File());
        CHECK (refused.failure.problem == "SYN01's Drums/Gone.wav is missing");
        CHECK (refused.failure.sourceProblem);
        CHECK_FALSE (refused.failure.reinstall);
        CHECK_FALSE (refused.failure.detail.empty());
        CHECK (entriesIn (samples) == 0);
    }
}

TEST_CASE ("a samples folder that can't be written", "[drumkit][export]")
{
#if JUCE_WINDOWS
    SKIP ("POSIX permissions only");
#else
    if (geteuid() == 0)
        SKIP ("root writes into read-only folders");
    TempFolder temp;
    test::SyntheticContent content;
    const auto samples = temp / "Samples";
    REQUIRE (samples.createDirectory().wasOk());
    REQUIRE (chmod (samples.getFullPathName().toRawUTF8(), 0555) == 0);
    const auto refused = writeSyn (content, samples);
    chmod (samples.getFullPathName().toRawUTF8(), 0755);
    CHECK (refused.folder == juce::File());
    CHECK (refused.failure.problem == "the samples folder can't be written to");
    CHECK (refused.failure.detail.find (samples.getFullPathName().toStdString()) != std::string::npos);
    CHECK_FALSE (refused.failure.sourceProblem);
    CHECK (entriesIn (samples) == 0);
#endif
}

TEST_CASE ("a write that fails part-way, or a cancel, leaves no kit folder and no staging folder", "[drumkit][export]")
{
    {
        // Cancelled after the third file.
        TempFolder temp;
        test::SyntheticContent content;
        const auto samples = temp / "Samples";
        std::atomic<bool> cancel { false };
        int seen = 0;
        const auto stopped = writeSyn (content, samples, syn(), &cancel, [&] (int done) {
            seen = done;
            if (done == 3)
                cancel = true;
        });
        CHECK (stopped.cancelled);
        CHECK_FALSE (stopped.wrote);
        CHECK (stopped.folder == juce::File());
        CHECK (seen == 3);
        CHECK (entriesIn (samples) == 0);
    }
    {
        // A file late in the plan isn't kit.json's: everything written before it goes.
        TempFolder temp;
        test::SyntheticContent::Options options;
        options.alteredPath = "Drums/Open Hat.wav";
        test::SyntheticContent content (options);
        const auto samples = temp / "Samples";
        int written = 0;
        const auto refused = writeSyn (content, samples, syn(), nullptr, [&] (int done) { written = done; });
        CHECK (written > 0);
        CHECK (refused.folder == juce::File());
        CHECK (refused.failure.reinstall);
        CHECK (entriesIn (samples) == 0);
    }
}

TEST_CASE ("a file or a link at the name takes the next one; every name to (999) taken is refused", "[drumkit][export]")
{
    TempFolder temp;
    test::SyntheticContent content;
    const auto samples = temp / "Samples";
    REQUIRE (samples.createDirectory().wasOk());
    const auto plain = samples.getChildFile ("SYN01 Synthetic Test Kit Drum Kit");
    REQUIRE (plain.replaceWithText ("a file"));
#if ! JUCE_WINDOWS
    const auto target = temp / "elsewhere";
    REQUIRE (target.createDirectory().wasOk());
    REQUIRE (target.createSymbolicLink (samples.getChildFile ("SYN01 Synthetic Test Kit Drum Kit (2)"), false));
#endif
    const auto written = writeSyn (content, samples);
    CHECK (written.wrote);
#if JUCE_WINDOWS
    CHECK (written.folder == samples.getChildFile ("SYN01 Synthetic Test Kit Drum Kit (2)"));
#else
    CHECK (written.folder == samples.getChildFile ("SYN01 Synthetic Test Kit Drum Kit (3)"));
    CHECK (entriesIn (target) == 0); // nothing through the link
#endif
    CHECK (plain.loadFileAsString() == "a file");

    TempFolder full;
    const auto taken = full / "Samples";
    REQUIRE (taken.createDirectory().wasOk());
    REQUIRE (taken.getChildFile ("SYN01 Synthetic Test Kit Drum Kit").replaceWithText ("x"));
    for (int k = 2; k <= 999; ++k)
        REQUIRE (taken.getChildFile ("SYN01 Synthetic Test Kit Drum Kit (" + juce::String (k) + ")").replaceWithText ("x"));
    const auto refused = writeSyn (content, taken);
    CHECK (refused.folder == juce::File());
    CHECK (refused.failure.problem == "every name for it is taken in the samples folder");
    CHECK_FALSE (refused.failure.sourceProblem);
    CHECK (entriesIn (taken) == 999); // no staging left
}

TEST_CASE ("renameNoReplace never replaces a file, a link or an empty folder", "[drumkit][export]")
{
    TempFolder temp;
    const auto from = temp / "from";
    REQUIRE (from.createDirectory().wasOk());
    REQUIRE (from.getChildFile ("a.txt").replaceWithText ("mine"));

    const auto file = temp / "file";
    REQUIRE (file.replaceWithText ("theirs"));
    CHECK_FALSE (sounds::renameNoReplace (from, file));
    CHECK (file.loadFileAsString() == "theirs");

    const auto empty = temp / "empty";
    REQUIRE (empty.createDirectory().wasOk());
    CHECK_FALSE (sounds::renameNoReplace (from, empty));
    CHECK (empty.isDirectory());
    CHECK (entriesIn (empty) == 0);

#if ! JUCE_WINDOWS
    const auto link = temp / "link";
    REQUIRE ((temp / "nowhere").createSymbolicLink (link, false)); // a dangling link
    CHECK_FALSE (sounds::renameNoReplace (from, link));
    CHECK (link.isSymbolicLink());
#endif
    CHECK (from.getChildFile ("a.txt").loadFileAsString() == "mine");

    const auto to = temp / "to";
    CHECK (sounds::renameNoReplace (from, to));
    CHECK_FALSE (from.exists());
    CHECK (to.getChildFile ("a.txt").loadFileAsString() == "mine");
}

TEST_CASE ("[.real] the four kits' drum kits from real content", "[drumkit][export][.real]")
{
    //     WUBANSIA_CONTENT_DIR=/path/to/content WubansiaPluginTests "[drumkit][.real]"
    const auto dir = juce::SystemStats::getEnvironmentVariable ("WUBANSIA_CONTENT_DIR", {});
    if (dir.isEmpty())
        SKIP ("set WUBANSIA_CONTENT_DIR to a content folder");
    TempFolder temp;
    UserSettings settings (temp / "settings.xml");
    ContentLocator::Places places;
    places.environment = juce::File::getCurrentWorkingDirectory().getChildFile (dir);
    InstalledContent installed (places, settings);
    const auto samples = temp / "Samples";
    const std::atomic<bool> never { false };
    for (const auto* kit : kits)
    {
        CAPTURE (kit);
        const auto& entry = kitEntry (kit);
        const auto found = installed.locate (entry.manifest);
        INFO (found.problem);
        REQUIRE (found.state == ContentState::ready);
        const auto plan = planOf (kit);
        const auto start = std::chrono::steady_clock::now();
        const auto written = sounds::writeDrumKit (installed, found, entry.manifest, plan, samples, never);
        const auto took = std::chrono::steady_clock::now() - start;
        INFO (written.failure.problem << " / " << written.failure.detail);
        REQUIRE (written.wrote);
        for (const auto& f : plan.files)
            if (f.source >= 0)
            {
                CAPTURE (f.path);
                const auto& listed = entry.manifest.files[(std::size_t) f.source];
                CHECK (bytesOf (written.folder.getChildFile (text (f.path))) == bytesOf (found.folder.getChildFile (text (listed.path))));
            }
        const auto again = std::chrono::steady_clock::now();
        CHECK_FALSE (sounds::writeDrumKit (installed, found, entry.manifest, plan, samples, never).wrote);
        WARN (kit << ": " << plan.files.size() << " files written in " << std::chrono::duration_cast<std::chrono::milliseconds> (took).count()
                  << " ms, found again in " << std::chrono::duration_cast<std::chrono::milliseconds> (std::chrono::steady_clock::now() - again).count() << " ms");
    }
}
