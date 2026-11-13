// Where the plugin's kits come from: the content folders in their order (ADR 0022) and what each
// problem reports, the person's settings file, the installed content end to end on the synthetic kit's
// WAVs, and the manifests and MIDI the plugin carries in its binary (EmbeddedContent).

#include "helpers/test_helpers.h"

#include "content/ContentLocator.h"
#include "content/EmbeddedContent.h"
#include "content/InstalledContent.h"
#include "content/UserSettings.h"
#include "services/KitCatalog.h"

#include "kit/kit_builder.h"
#include "midi/reader.h"
#include "testing/synthetic_kit.h"
#include "util/file_io.h"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <functional>
#include <set>
#include <sstream>
#include <vector>

using namespace wubansia;
namespace syn = engine::testing;

namespace
{
    // A fresh folder under the temp folder, deleted with this.
    struct TempFolder
    {
        TempFolder() : folder (juce::File::getSpecialLocation (juce::File::tempDirectory).getNonexistentChildFile ("WubansiaContentTest", "", false))
        {
            folder.createDirectory();
        }
        ~TempFolder() { folder.deleteRecursively(); }

        juce::File operator/ (const char* child) const { return folder.getChildFile (child); }

        juce::File folder;
    };

    const engine::KitManifest& synManifest() { return syn::syntheticContent().kit; }

    // The synthetic kit's WAVs in kitFolder (made if needed).
    void writeKit (const juce::File& kitFolder)
    {
        REQUIRE (kitFolder.createDirectory().wasOk());
        const auto written = syn::writeSyntheticKitFolder (toPath (kitFolder));
        REQUIRE (written.ok());
        REQUIRE (written.value == syn::syn::fileCount);
    }

    juce::File fileIn (const juce::File& kitFolder, int index)
    {
        return kitFolder.getChildFile (juce::String (synManifest().files[(std::size_t) index].path));
    }

    ContentLocator locatorFor (const juce::File& environment, const juce::File& user, const juce::File& platform, const juce::File& debug)
    {
        ContentLocator::Places places;
        places.environment = environment;
        places.user = user;
        places.platformDefault = platform;
        places.debug = debug;
        return ContentLocator (places);
    }
}

TEST_CASE ("the content folders, in order: $WUBANSIA_CONTENT_DIR, the chosen folder, the platform's, then Debug's checkout", "[content]")
{
    TempFolder temp;
    const auto env = temp / "env", user = temp / "user", platform = temp / "platform", debug = temp / "debug";

    auto locator = locatorFor (env, user, platform, debug);
    CHECK (locator.roots() == std::vector<juce::File> { env, user, platform, debug });

    // Folders not set are left out, and one folder is searched once.
    CHECK (locatorFor ({}, {}, platform, {}).roots() == std::vector<juce::File> { platform });
    CHECK (locatorFor (platform, {}, platform, {}).roots() == std::vector<juce::File> { platform });

    // The first folder that holds the kit ready wins.
    writeKit (platform.getChildFile ("syn01"));
    writeKit (debug.getChildFile ("syn01"));
    auto status = locator.locate (synManifest());
    CHECK (status.state == ContentState::ready);
    CHECK (status.folder == platform.getChildFile ("syn01"));

    writeKit (user.getChildFile ("syn01").getChildFile ("v1"));
    status = locator.locate (synManifest());
    CHECK (status.folder == user.getChildFile ("syn01").getChildFile ("v1"));

    writeKit (env.getChildFile ("syn01"));
    status = locator.locate (synManifest());
    CHECK (status.folder == env.getChildFile ("syn01"));
    CHECK (status.problem.isEmpty());

    // A user folder set later goes in second place.
    auto later = locatorFor ({}, {}, platform, {});
    later.setUserFolder (user);
    CHECK (later.roots() == std::vector<juce::File> { user, platform });
    CHECK (later.locate (synManifest()).folder == user.getChildFile ("syn01").getChildFile ("v1"));
}

TEST_CASE ("in each folder, <kit>/v<N>/ comes before <kit>/", "[content]")
{
    TempFolder temp;
    const auto root = temp / "root";
    const auto plain = root.getChildFile ("syn01");
    const auto versioned = plain.getChildFile ("v1");
    writeKit (plain);
    writeKit (versioned);
    const auto locator = locatorFor ({}, {}, root, {});

    auto status = locator.locate (synManifest());
    CHECK (status.state == ContentState::ready);
    CHECK (status.folder == versioned);
    CHECK (status.searched == juce::StringArray { versioned.getFullPathName() });

    // A broken v1 gives way to a ready plain folder.
    REQUIRE (fileIn (versioned, 3).deleteFile());
    status = locator.locate (synManifest());
    CHECK (status.state == ContentState::ready);
    CHECK (status.folder == plain);
    CHECK (status.searched == juce::StringArray { versioned.getFullPathName(), plain.getFullPathName() });
}

TEST_CASE ("what a folder that isn't ready reports: the first problem, named", "[content]")
{
    TempFolder temp;
    const auto root = temp / "root";
    const auto kit = root.getChildFile ("syn01").getChildFile ("v1");
    const auto locator = locatorFor ({}, {}, root, {});

    SECTION ("no folder holds the kit: missing")
    {
        const auto status = locator.locate (synManifest());
        CHECK (status.state == ContentState::missing);
        CHECK (status.folder == juce::File());
        CHECK (status.problem.contains ("SYN01"));
        CHECK (status.searched.size() == 2);
    }

    SECTION ("an empty kit folder: missing, naming the first file")
    {
        REQUIRE (kit.createDirectory().wasOk());
        const auto status = locator.locate (synManifest());
        CHECK (status.state == ContentState::missing);
        CHECK (status.folder == kit);
        CHECK (status.problem.contains (juce::String (synManifest().files[0].path)));
    }

    SECTION ("a Git LFS pointer: lfs_pointers")
    {
        writeKit (kit);
        REQUIRE (fileIn (kit, 5).replaceWithText ("version https://git-lfs.github.com/spec/v1\noid sha256:"
                                                   + juce::String::repeatedString ("0", 64) + "\nsize 12345\n"));
        const auto status = locator.locate (synManifest());
        CHECK (status.state == ContentState::lfsPointers);
        CHECK (status.problem.contains (juce::String (synManifest().files[5].path)));
        CHECK (status.problem.contains ("git lfs pull"));
    }

    SECTION ("a 44.1 kHz file: mismatch")
    {
        writeKit (kit);
        auto audio = syn::syntheticContent().audio[7];
        audio.sampleRate = 44100;
        const auto bytes = syn::wavBytes (audio);
        REQUIRE (util::writeBinaryFile (toPath (fileIn (kit, 7)), bytes).ok());
        const auto status = locator.locate (synManifest());
        CHECK (status.state == ContentState::mismatch);
        CHECK (status.problem.contains (juce::String (synManifest().files[7].path)));
    }

    SECTION ("another version's file (other frames): mismatch")
    {
        writeKit (kit);
        auto audio = syn::syntheticContent().audio[2];
        audio.frames -= 10;
        audio.audio.resize ((std::size_t) (audio.frames * audio.channels));
        REQUIRE (util::writeBinaryFile (toPath (fileIn (kit, 2)), syn::wavBytes (audio)).ok());
        CHECK (locator.locate (synManifest()).state == ContentState::mismatch);
    }

    SECTION ("a folder where a file should be: failed (unreadable)")
    {
        writeKit (kit);
        REQUIRE (fileIn (kit, 4).deleteFile());
        REQUIRE (fileIn (kit, 4).createDirectory().wasOk());
        const auto status = locator.locate (synManifest());
        CHECK (status.state == ContentState::failed);
        CHECK (status.problem.isNotEmpty());
    }

    SECTION ("the installers' SHA256SUMS and other files are ignored")
    {
        writeKit (kit);
        REQUIRE (kit.getChildFile ("SHA256SUMS").replaceWithText ("0000  Drums/Kick A.wav\n"));
        REQUIRE (kit.getChildFile ("README.txt").replaceWithText ("hello"));
        REQUIRE (kit.getChildFile ("Drums").getChildFile ("Extra.wav").replaceWithText ("not audio"));
        CHECK (locator.locate (synManifest()).state == ContentState::ready);
    }
}

TEST_CASE ("the platform's content folder is the installers' (tools/content/kitfiles.default_content_dir)", "[content]")
{
    const auto home = juce::File::getSpecialLocation (juce::File::userHomeDirectory);
#if JUCE_MAC
    const auto expected = home.getChildFile ("Library/Application Support/Wubansia Audio/Wubansia/Content");
#elif JUCE_WINDOWS
    const auto expected = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Wubansia Audio/Wubansia/Content");
#else
    const auto expected = home.getChildFile (".local/share/Wubansia Audio/Wubansia/Content");
#endif
    CHECK (ContentLocator::platformDefaultFolder() == expected);
    CHECK (UserSettings::defaultFile() == expected.getParentDirectory().getChildFile ("settings.xml"));
    CHECK (ContentLocator::systemPlaces().platformDefault == expected);
#if ! defined(WUBANSIA_DEV_CONTENT_DIR)
    CHECK (ContentLocator::systemPlaces().debug == juce::File()); // Release builds never look in the checkout
#endif
}

TEST_CASE ("the chosen content folder is kept in the person's settings file", "[content]")
{
    TempFolder temp;
    const auto file = (temp / "Wubansia Audio").getChildFile ("settings.xml"); // its folder doesn't exist yet
    const auto chosen = temp / "My Samples";

    {
        UserSettings settings (file);
        CHECK (settings.contentFolder() == juce::File());
        CHECK (settings.fileReads() == 1);
        const auto before = settings.generation();
        CHECK (settings.setContentFolder (chosen));
        CHECK (settings.generation() == before + 1);
        CHECK (settings.contentFolder() == chosen);
        CHECK (settings.fileReads() == 1);
    }
    REQUIRE (file.existsAsFile());
    {
        UserSettings again (file);
        CHECK (again.contentFolder() == chosen);
        CHECK (again.setContentFolder ({}));
    }
    CHECK (UserSettings (file).contentFolder() == juce::File());
}

TEST_CASE ("the drag strip's drum option is kept in the person's settings file, beside the content folder", "[content][dragout]")
{
    TempFolder temp;
    const auto file = (temp / "Wubansia Audio").getChildFile ("settings.xml");
    const auto chosen = temp / "My Samples";
    const auto attributes = [&file]
    {
        const auto xml = juce::XmlDocument::parse (file);
        REQUIRE (xml != nullptr);
        return std::pair { xml->hasAttribute ("drag_drums_one_track") ? xml->getStringAttribute ("drag_drums_one_track") : juce::String ("(none)"),
            xml->getStringAttribute ("content_folder") };
    };

    {
        UserSettings settings (file);
        CHECK_FALSE (settings.dragDrumsOnOneTrack()); // off by default, with no file at all
        REQUIRE (settings.setContentFolder (chosen));
        const auto generation = settings.generation();

        CHECK (settings.setDragDrumsOnOneTrack (true));
        CHECK (settings.dragDrumsOnOneTrack());
        CHECK (attributes() == std::pair { juce::String ("1"), chosen.getFullPathName() });
        CHECK (settings.generation() == generation); // the content folders didn't change
        CHECK (settings.fileReads() == 1);
    }
    {
        UserSettings again (file);
        CHECK (again.dragDrumsOnOneTrack());
        CHECK (again.contentFolder() == chosen);
        CHECK (again.setDragDrumsOnOneTrack (false));
        CHECK_FALSE (again.dragDrumsOnOneTrack());
    }
    CHECK (attributes() == std::pair { juce::String ("(none)"), chosen.getFullPathName() }); // gone, the folder kept
    CHECK_FALSE (UserSettings (file).dragDrumsOnOneTrack());
    CHECK (UserSettings (file).contentFolder() == chosen);

    // Only "1" lights it; any other value, or another root tag, reads as off.
    for (const auto* value : { "true", "0", "" })
    {
        INFO ("drag_drums_one_track=\"" << value << "\"");
        REQUIRE (file.replaceWithText ("<WubansiaSettings drag_drums_one_track=\"" + juce::String (value) + "\"/>"));
        CHECK_FALSE (UserSettings (file).dragDrumsOnOneTrack());
    }
    REQUIRE (file.replaceWithText ("<OtherSettings drag_drums_one_track=\"1\"/>"));
    CHECK_FALSE (UserSettings (file).dragDrumsOnOneTrack());
    REQUIRE (file.replaceWithText ("<WubansiaSettings drag_drums_one_track=\"1\"/>"));
    CHECK (UserSettings (file).dragDrumsOnOneTrack());
}

TEST_CASE ("each setter of the settings file keeps whatever else the file holds", "[content][dragout]")
{
    TempFolder temp;
    const auto file = (temp / "Wubansia Audio").getChildFile ("settings.xml");
    const auto chosen = temp / "My Samples";

    // Every setting this build writes: its attribute, its value when set, its setter, and what a new reader sees.
    struct Setting
    {
        const char* attribute;
        juce::String value;
        std::function<bool (UserSettings&, bool)> set; // set it (true) or clear it (false)
        std::function<bool (const UserSettings&)> isSet;
    };
    const std::vector<Setting> settings {
        { "content_folder", chosen.getFullPathName(), [&chosen] (UserSettings& s, bool on) { return s.setContentFolder (on ? chosen : juce::File()); },
            [&chosen] (const UserSettings& s) { return s.contentFolder() == chosen; } },
        { "drag_drums_one_track", "1", [] (UserSettings& s, bool on) { return s.setDragDrumsOnOneTrack (on); },
            [] (const UserSettings& s) { return s.dragDrumsOnOneTrack(); } },
    };

    // A file every setting is set in, with an attribute and a child element this build doesn't know (a later build's).
    const auto seed = [&]
    {
        juce::XmlElement root ("WubansiaSettings");
        root.setAttribute ("future_option", "kept");
        for (const auto& s : settings)
            root.setAttribute (s.attribute, s.value);
        root.createNewChildElement ("Future")->setAttribute ("note", "kept too");
        REQUIRE (file.getParentDirectory().createDirectory().wasOk());
        REQUIRE (root.writeTo (file));
    };
    // The file after a write: the one setting as written, every other one and the unknown parts as they were.
    const auto checkFile = [&] (const Setting& written, bool on)
    {
        const auto xml = juce::XmlDocument::parse (file);
        REQUIRE (xml != nullptr);
        CHECK (xml->hasTagName ("WubansiaSettings"));
        CHECK (xml->getStringAttribute ("future_option") == "kept");
        const auto* child = xml->getChildByName ("Future");
        CHECK (child != nullptr);
        CHECK ((child != nullptr && child->getStringAttribute ("note") == "kept too"));
        for (const auto& s : settings)
        {
            INFO (s.attribute);
            if (&s == &written && ! on)
                CHECK_FALSE (xml->hasAttribute (s.attribute));
            else
                CHECK (xml->getStringAttribute (s.attribute) == s.value);
            CHECK (s.isSet (UserSettings (file)) == (&s != &written || on));
        }
    };

    for (const auto& written : settings)
    {
        INFO ("writing " << written.attribute);
        seed();
        {
            UserSettings reader (file);
            CHECK (written.set (reader, false));
        }
        checkFile (written, false);
        {
            UserSettings reader (file);
            CHECK (written.set (reader, true));
        }
        checkFile (written, true);
    }
}

TEST_CASE ("a settings file that can't be written: the setters say so, and the choices hold until the process ends", "[content][dragout]")
{
    TempFolder temp;
    const auto blocker = temp / "Wubansia Audio"; // a file where the settings file's folder should be
    REQUIRE (blocker.replaceWithText ("a file, not a folder"));
    const auto file = blocker.getChildFile ("settings.xml");
    const auto chosen = temp / "My Samples";

    UserSettings settings (file);
    CHECK_FALSE (settings.setDragDrumsOnOneTrack (true));
    CHECK (settings.dragDrumsOnOneTrack());
    CHECK_FALSE (settings.setContentFolder (chosen));
    CHECK (settings.contentFolder() == chosen);
    CHECK (settings.dragDrumsOnOneTrack());

    CHECK_FALSE (settings.setDragDrumsOnOneTrack (false));
    CHECK_FALSE (settings.dragDrumsOnOneTrack());
    CHECK_FALSE (settings.setContentFolder ({}));
    CHECK (settings.contentFolder() == juce::File());

    CHECK (blocker.loadFileAsString() == "a file, not a folder");
    CHECK_FALSE (file.exists());
}

TEST_CASE ("the installed content: a chosen folder's WAVs located, then read into a Kit", "[content]")
{
    TempFolder temp;
    const auto chosen = temp / "chosen";
    writeKit (chosen.getChildFile ("syn01").getChildFile ("v1"));
    UserSettings settings (temp / "settings.xml");
    REQUIRE (settings.setContentFolder (chosen));

    ContentLocator::Places places;
    places.platformDefault = temp / "nothing here";
    InstalledContent installed (places, settings);
    CHECK (installed.userFolder() == chosen);
    CHECK (installed.kits().size() == EmbeddedContent::kits().size());

    const auto found = installed.locate (synManifest());
    REQUIRE (found.state == ContentState::ready);
    CHECK (found.folder == chosen.getChildFile ("syn01").getChildFile ("v1"));
    CHECK (found.searched[0] == chosen.getChildFile ("syn01").getChildFile ("v1").getFullPathName());

    auto source = installed.open (synManifest(), found);
    REQUIRE (source != nullptr);
    const std::atomic<bool> cancel { false };
    const auto kit = engine::buildKit (synManifest(), syn::syntheticContent().mix, *source, std::array<engine::Timeline, 4> {}, cancel, [] (int) {});
    REQUIRE (kit.ok());
    CHECK (kit.value->audio.size() == synManifest().files.size());
    CHECK (kit.value->audio[0] == syn::syntheticContent().audio[0]);

    // Another folder chosen: the folders' generation moves.
    const auto generation = installed.foldersGeneration();
    installed.setUserFolder (temp / "elsewhere");
    CHECK (installed.foldersGeneration() != generation);
    CHECK (installed.locate (synManifest()).state == ContentState::missing);
}

TEST_CASE ("a raised cancel flag stops the content check and the reads within one file", "[content]")
{
    TempFolder temp;
    const auto root = temp / "root";
    const auto kitFolder = root.getChildFile ("syn01").getChildFile ("v1");
    writeKit (kitFolder);
    const auto locator = locatorFor ({}, {}, root, {});

    std::atomic<bool> cancel { false };
    const auto ready = locator.locate (synManifest(), &cancel);
    REQUIRE (ready.state == ContentState::ready);

    // Raised: the check stops before its first file, the folder isn't ready, and no other folder is tried.
    cancel.store (true);
    const auto stopped = locator.locate (synManifest(), &cancel);
    CHECK (stopped.state != ContentState::ready);
    CHECK (stopped.problem.containsIgnoreCase ("cancelled"));
    CHECK (stopped.searched == juce::StringArray { kitFolder.getFullPathName() });

    // InstalledContent hands the flag to the check and to the FileSampleSource that reads the samples.
    UserSettings settings (temp / "settings.xml");
    REQUIRE (settings.setContentFolder (root));
    InstalledContent installed (ContentLocator::Places {}, settings);
    CHECK (installed.locate (synManifest(), &cancel).state != ContentState::ready);
    auto source = installed.open (synManifest(), ready, &cancel);
    REQUIRE (source != nullptr);
    const auto read = source->read (synManifest().files[0].path);
    REQUIRE (! read.ok());
    CHECK (engine::loadErrorOf (read.error) == engine::LoadError::cancelled);
}

TEST_CASE ("the plugin carries four kits, each with two manifests and four SMFs that read", "[content][embedded]")
{
    const auto kits = EmbeddedContent::kits();
    REQUIRE (kits.size() == 4);
    CHECK (std::vector<std::string_view> (kits.begin(), kits.end()) == std::vector<std::string_view> { "FUT01", "FUT02", "FUT03", "FUT04" });

    for (const auto id : kits)
    {
        CAPTURE (std::string (id));
        const auto kitJson = EmbeddedContent::kitManifest (id);
        const auto mixJson = EmbeddedContent::mixManifest (id);
        REQUIRE_FALSE (kitJson.empty());
        REQUIRE_FALSE (mixJson.empty());
        const auto kit = engine::parseKitManifest (kitJson);
        REQUIRE (kit.error.empty());
        CHECK (kit.value.kit.id == id);
        CHECK (engine::parseMixManifest (mixJson, kit.value).error.empty());

        for (const auto mode : { engine::PlayMode::loop, engine::PlayMode::song })
            for (const auto variant : { engine::Variant::vox, engine::Variant::novox })
            {
                const auto smf = EmbeddedContent::timeline (id, mode, variant);
                REQUIRE_FALSE (smf.empty());
                const auto file = midi::readSmf (smf);
                CHECK (file.error.empty());
                CHECK (file.value.ppq == 960);
            }
        CHECK (EmbeddedContent::timeline (id, engine::PlayMode::off, engine::Variant::vox).empty());
    }
    CHECK (EmbeddedContent::kitManifest ("FUT09").empty());
    CHECK (EmbeddedContent::midiFiles ("FUT09").empty());
}

TEST_CASE ("the embedded files are the checkout's, byte for byte: manifests and every kit-prefixed MIDI file, no region files", "[content][embedded]")
{
    const juce::File root (WUBANSIA_SOURCE_DIR);
    REQUIRE (root.getChildFile ("content/plugin").isDirectory());
    std::size_t total = 0;

    for (const auto id : EmbeddedContent::kits())
    {
        const auto folder = ContentLocator::kitFolderName (id);
        const juce::String kit (std::string (id).c_str());
        CAPTURE (kit);

        const auto same = [] (const juce::File& file, std::span<const std::uint8_t> bytes) {
            juce::MemoryBlock block;
            return file.loadFileAsData (block) && block.getSize() == bytes.size() && std::equal (bytes.begin(), bytes.end(), static_cast<const std::uint8_t*> (block.getData()));
        };
        const auto asBytes = [] (std::string_view text) { return std::span<const std::uint8_t> (reinterpret_cast<const std::uint8_t*> (text.data()), text.size()); };
        CHECK (same (root.getChildFile ("content/plugin/" + folder + "/" + kit + "_kit.json"), asBytes (EmbeddedContent::kitManifest (id))));
        CHECK (same (root.getChildFile ("content/plugin/" + folder + "/" + kit + "_mix.json"), asBytes (EmbeddedContent::mixManifest (id))));
        total += EmbeddedContent::kitManifest (id).size() + EmbeddedContent::mixManifest (id).size();

        // Every .mid under content/midi/<kit>/ whose name starts with the kit's id, and nothing else.
        std::set<std::string> onDisk;
        for (const auto& entry : juce::RangedDirectoryIterator (root.getChildFile ("content/midi/" + folder), true, "*.mid"))
            if (entry.getFile().getFileName().startsWith (kit + "_"))
                onDisk.insert (entry.getFile().getFileName().toStdString());
        std::set<std::string> embedded;
        for (const auto& file : EmbeddedContent::midiFiles (id))
        {
            embedded.insert (std::string (file.name));
            total += file.bytes.size();
            auto matches = juce::RangedDirectoryIterator (root.getChildFile ("content/midi/" + folder), true, juce::String (std::string (file.name).c_str()));
            REQUIRE (matches != juce::RangedDirectoryIterator());
            CHECK (same (matches->getFile(), file.bytes));
        }
        CHECK (embedded == onDisk);
        CHECK (embedded.count (std::string (id) + "_Loop_Vox_4bars.mid") == 1);
        CHECK (embedded.count (std::string (id) + "_ExtendedMix_NoVox_224bars.mid") == 1);
        for (const auto& name : embedded)
            CHECK (name.rfind (std::string (id) + "_", 0) == 0); // never a region file ("01_Intro_32bars.mid")
    }
    CHECK (total == EmbeddedContent::totalBytes());
    CHECK (total < 2'000'000); // about 1.4 MB
}

TEST_CASE ("the catalog of the embedded kits: cards, sections and drag-out files", "[content][embedded]")
{
    const KitCatalog catalog (EmbeddedContent::sources());
    REQUIRE (catalog.entries().size() == 4);
    for (const auto& entry : catalog.entries())
    {
        CAPTURE (entry.id);
        REQUIRE (entry.error.empty());
        CHECK (entry.info.id == entry.id);
        CHECK (entry.info.index == entry.index);
        CHECK (entry.card.id == juce::String (entry.id));
        CHECK (entry.card.title.isNotEmpty());
        CHECK (entry.card.bpm >= 128);
        CHECK (entry.card.bpm <= 140);
        CHECK (entry.card.camelot.isNotEmpty());
        CHECK (entry.card.laneLabels[0].isNotEmpty()); // every kit has a kick
        CHECK (entry.card.phraseLanes != 0u);

        // The Song's sections run from bar 1 to bar 224 without a gap.
        for (const auto& sections : entry.sections)
        {
            REQUIRE_FALSE (sections.empty());
            CHECK (sections.front().startBar == 1);
            int bar = 1;
            for (const auto& s : sections)
            {
                CHECK (s.startBar == bar);
                CHECK (s.bars > 0);
                CHECK (s.name.isNotEmpty());
                bar += s.bars;
            }
            CHECK (bar == 225);
        }
        CHECK (entry.midiFiles.size() == EmbeddedContent::midiFiles (entry.id).size());

        // The four timelines build.
        const auto timelines = buildTimelines (entry);
        REQUIRE (timelines.error.empty());
        CHECK (timelines.value[0].loops);
        CHECK (timelines.value[0].length == engine::kLoopLength);
        CHECK_FALSE (timelines.value[2].loops);
        CHECK (timelines.value[2].length == engine::kSongLength);
        CHECK_FALSE (timelines.value[2].sections.empty());
    }
}

TEST_CASE ("the four kits load from a real content folder and play their loops and songs", "[.real]")
{
    // Hidden: it reads 160-200 MB of WAV. Run it on a content folder (<kit>/v<N>/ or the repo's <kit>/):
    //     WUBANSIA_CONTENT_DIR=/path/to/content WubansiaPluginTests "[.real]"
    if (juce::SystemStats::getEnvironmentVariable ("WUBANSIA_CONTENT_DIR", {}).isEmpty())
        SKIP ("set WUBANSIA_CONTENT_DIR to a content folder");

    for (const auto id : EmbeddedContent::kits())
    {
        const juce::String kit (std::string (id).c_str());
        CAPTURE (kit);
        PluginProcessor plugin; // the plugin's own content: $WUBANSIA_CONTENT_DIR comes first
        test::TestPlayHead head;
        plugin.setPlayHead (&head);
        plugin.controller().selectKit (kit);

        const auto start = std::chrono::steady_clock::now();
        test::prepare (plugin, 48000.0, 512);
        REQUIRE (test::waitFor ([&] { return ! plugin.kitService().loadInFlight(); }, std::chrono::milliseconds (30000)));
        const auto loaded = std::chrono::steady_clock::now() - start;
        const auto status = plugin.controller().status();
        INFO (status.problem);
        REQUIRE (status.state == ContentState::ready);

        const auto* entry = plugin.kitService().catalog().find (std::string (id));
        REQUIRE (entry != nullptr);
        const auto bpm = (double) entry->info.bpm;
        const auto play = [&] (float mode, double fromPpq, int bars) {
            test::setParameter (plugin, ParameterIds::playMode, mode);
            head.play (bpm, fromPpq);
            const auto samples = (int) std::lround (bars * 4 * 60.0 / bpm * 48000.0);
            juce::AudioBuffer<float> out (2, samples);
            for (int done = 0; done < samples; done += 512)
            {
                const auto n = std::min (512, samples - done);
                juce::AudioBuffer<float> block (out.getArrayOfWritePointers(), 2, done, n);
                juce::MidiBuffer midi;
                plugin.processBlock (block, midi);
                head.advance (n, 48000.0);
            }
            return out;
        };

        const auto loop = play (1.0f, 0.0, 4);
        const auto hits = plugin.controller().snapshot();
        const auto song = play (2.0f, 4.0 * 4 * 48, 8); // from bar 49, inside the first groove
        const auto loopPeak = test::peakOf (loop, 48000), songPeak = test::peakOf (song, 48000);
        std::ostringstream line;
        line << std::string (id) << ": loaded in " << std::chrono::duration_cast<std::chrono::milliseconds> (loaded).count() << " ms from "
             << status.folder.getFullPathName() << "; loop peak " << loopPeak << ", song peak " << songPeak << ", lanes hit 0x" << std::hex
             << hits.laneHits << ", recorded phrases 0x" << hits.phraseRecorded << std::dec << ", " << hits.voices << " voices, GR "
             << hits.gainReductionDb << " dB";
        WARN (line.str());
        CHECK (test::countBad (loop) == 0);
        CHECK (test::countBad (song) == 0);
        CHECK (loopPeak > 0.1f);
        CHECK (loopPeak < 0.9f); // the club master's ceiling, -1.02 dBTP
        CHECK (songPeak > 0.05f);
        CHECK (hits.laneHits != 0u);
        plugin.setPlayHead (nullptr);
    }
}
