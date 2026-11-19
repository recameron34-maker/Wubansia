// Where the plugin's kits come from: the content folders in their order (ADR 0022) and what each
// problem reports, the person's settings file, the installed content end to end on the synthetic kit's
// WAVs, and the manifests and MIDI the plugin carries in its binary (EmbeddedContent).

#include "helpers/test_helpers.h"

#include "content/ContentLocator.h"
#include "content/EmbeddedContent.h"
#include "content/InstalledContent.h"
#include "content/UserSettings.h"
#include "dragout/DragFiles.h"
#include "services/KitCatalog.h"

#include "kit/kit_builder.h"
#include "midi/reader.h"
#include "testing/synthetic_kit.h"
#include "util/file_io.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <map>
#include <set>
#include <sstream>
#include <thread>
#include <tuple>
#include <vector>

#if ! JUCE_WINDOWS
    #include <fcntl.h>
    #include <sys/stat.h>
    #include <unistd.h>
#endif

using namespace wubansia;
using namespace std::chrono_literals;
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

TEST_CASE ("the drag strip's drum option is kept in the person's settings file, beside the content and samples folders", "[content][dragout][sounds]")
{
    TempFolder temp;
    const auto file = (temp / "Wubansia Audio").getChildFile ("settings.xml");
    const auto chosen = temp / "My Samples";
    const auto drags = temp / "My Drags"; // the Samples drawer's folder (ADR 0024)
    const auto attributes = [&file]
    {
        const auto xml = juce::XmlDocument::parse (file);
        REQUIRE (xml != nullptr);
        return std::tuple { xml->hasAttribute ("drag_drums_one_track") ? xml->getStringAttribute ("drag_drums_one_track") : juce::String ("(none)"),
            xml->getStringAttribute ("content_folder"), xml->getStringAttribute ("samples_folder") };
    };

    {
        UserSettings settings (file);
        CHECK_FALSE (settings.dragDrumsOnOneTrack()); // off by default, with no file at all
        REQUIRE (settings.setContentFolder (chosen));
        REQUIRE (settings.setSamplesFolder (drags));
        const auto generation = settings.generation();

        CHECK (settings.setDragDrumsOnOneTrack (true));
        CHECK (settings.dragDrumsOnOneTrack());
        CHECK (attributes() == std::tuple { juce::String ("1"), chosen.getFullPathName(), drags.getFullPathName() });
        CHECK (settings.generation() == generation); // the content folders didn't change
        CHECK (settings.fileReads() == 1);
    }
    {
        UserSettings again (file);
        CHECK (again.dragDrumsOnOneTrack());
        CHECK (again.contentFolder() == chosen);
        CHECK (again.samplesFolder() == drags);
        CHECK (again.setDragDrumsOnOneTrack (false));
        CHECK_FALSE (again.dragDrumsOnOneTrack());
    }
    CHECK (attributes() == std::tuple { juce::String ("(none)"), chosen.getFullPathName(), drags.getFullPathName() }); // gone, the folders kept
    CHECK_FALSE (UserSettings (file).dragDrumsOnOneTrack());
    CHECK (UserSettings (file).contentFolder() == chosen);
    CHECK (UserSettings (file).samplesFolder() == drags);

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

TEST_CASE ("the drag strip's For my synths is kept in the person's settings file: on unless it says drag_for_synths=\"0\"", "[content][dragout]")
{
    TempFolder temp;
    const auto file = (temp / "Wubansia Audio").getChildFile ("settings.xml");
    const auto chosen = temp / "My Samples";
    const auto attribute = [&file]
    {
        const auto xml = juce::XmlDocument::parse (file);
        REQUIRE (xml != nullptr);
        return xml->hasAttribute ("drag_for_synths") ? xml->getStringAttribute ("drag_for_synths") : juce::String ("(none)");
    };

    {
        UserSettings settings (file);
        CHECK (settings.dragForSynths()); // on by default, with no file at all
        REQUIRE (settings.setContentFolder (chosen));
        REQUIRE (settings.setDragDrumsOnOneTrack (true));
        const auto generation = settings.generation();

        CHECK (settings.setDragForSynths (false));
        CHECK_FALSE (settings.dragForSynths());
        CHECK (attribute() == "0");
        CHECK (settings.generation() == generation); // the content folders didn't change
        CHECK (settings.fileReads() == 1);
    }
    {
        UserSettings again (file);
        CHECK_FALSE (again.dragForSynths());
        CHECK (again.contentFolder() == chosen);
        CHECK (again.dragDrumsOnOneTrack());
        CHECK (again.setDragForSynths (true));
        CHECK (again.dragForSynths());
    }
    CHECK (attribute() == "(none)"); // on again: gone, the rest kept
    CHECK (UserSettings (file).dragForSynths());
    CHECK (UserSettings (file).contentFolder() == chosen);
    CHECK (UserSettings (file).dragDrumsOnOneTrack());

    // Only "0" turns it off; any other value, or another root tag, reads as on.
    for (const auto* value : { "1", "false", "no", "" })
    {
        INFO ("drag_for_synths=\"" << value << "\"");
        REQUIRE (file.replaceWithText ("<WubansiaSettings drag_for_synths=\"" + juce::String (value) + "\"/>"));
        CHECK (UserSettings (file).dragForSynths());
    }
    REQUIRE (file.replaceWithText ("<OtherSettings drag_for_synths=\"0\"/>"));
    CHECK (UserSettings (file).dragForSynths());
    REQUIRE (file.replaceWithText ("<WubansiaSettings drag_for_synths=\"0\"/>"));
    CHECK_FALSE (UserSettings (file).dragForSynths());
}

TEST_CASE ("each setter of the settings file keeps whatever else the file holds", "[content][dragout][sounds]")
{
    TempFolder temp;
    const auto file = (temp / "Wubansia Audio").getChildFile ("settings.xml");
    const auto chosen = temp / "My Samples";
    const auto drags = temp / "My Drags";

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
        { "samples_folder", drags.getFullPathName(), [&drags] (UserSettings& s, bool on) { return s.setSamplesFolder (on ? drags : juce::File()); },
            [&drags] (const UserSettings& s) { return s.samplesFolder() == drags; } },
        { "drag_drums_one_track", "1", [] (UserSettings& s, bool on) { return s.setDragDrumsOnOneTrack (on); },
            [] (const UserSettings& s) { return s.dragDrumsOnOneTrack(); } },
        // Set is For my synths turned off: on is its default, the attribute absent.
        { "drag_for_synths", "0", [] (UserSettings& s, bool on) { return s.setDragForSynths (! on); },
            [] (const UserSettings& s) { return ! s.dragForSynths(); } },
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

TEST_CASE ("a settings file that can't be written: the setters say so, and the choices hold until the process ends", "[content][dragout][sounds]")
{
    TempFolder temp;
    const auto blocker = temp / "Wubansia Audio"; // a file where the settings file's folder should be
    REQUIRE (blocker.replaceWithText ("a file, not a folder"));
    const auto file = blocker.getChildFile ("settings.xml");
    const auto chosen = temp / "My Samples";
    const auto drags = temp / "My Drags";

    UserSettings settings (file);
    CHECK_FALSE (settings.setDragDrumsOnOneTrack (true));
    CHECK (settings.dragDrumsOnOneTrack());
    CHECK_FALSE (settings.setContentFolder (chosen));
    CHECK (settings.contentFolder() == chosen);
    CHECK (settings.dragDrumsOnOneTrack());
    CHECK_FALSE (settings.setSamplesFolder (drags));
    CHECK (settings.samplesFolder() == drags);
    CHECK (settings.contentFolder() == chosen);
    CHECK (settings.dragDrumsOnOneTrack());

    CHECK_FALSE (settings.setDragDrumsOnOneTrack (false));
    CHECK_FALSE (settings.dragDrumsOnOneTrack());
    CHECK (settings.samplesFolder() == drags);
    CHECK_FALSE (settings.setDragForSynths (false));
    CHECK_FALSE (settings.dragForSynths());
    CHECK (settings.samplesFolder() == drags);
    CHECK_FALSE (settings.setDragForSynths (true));
    CHECK (settings.dragForSynths());
    CHECK_FALSE (settings.setContentFolder ({}));
    CHECK (settings.contentFolder() == juce::File());
    CHECK_FALSE (settings.setSamplesFolder ({}));
    CHECK (settings.samplesFolder() == juce::File());

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

TEST_CASE ("readFile gives a listed file's bytes from the kit's folder and refuses paths that leave it", "[content][sounds]")
{
    TempFolder temp;
    const auto root = temp / "root";
    const auto kitFolder = root.getChildFile ("syn01").getChildFile ("v1");
    writeKit (kitFolder);
    UserSettings settings (temp / "settings.xml");
    ContentLocator::Places places;
    places.platformDefault = root;
    InstalledContent installed (places, settings);
    const auto found = installed.locate (synManifest());
    REQUIRE (found.state == ContentState::ready);
    REQUIRE (found.folder == kitFolder);

    // Every listed file, byte for byte as installed.
    for (std::size_t i = 0; i < synManifest().files.size(); ++i)
    {
        const auto& path = synManifest().files[i].path;
        CAPTURE (path);
        const auto bytes = installed.readFile (found, path);
        INFO (bytes.error);
        REQUIRE (bytes.ok());
        CHECK (bytes.value == syn::syntheticContent().wavs[i]);
    }

    // Paths that would leave the kit's folder, or aren't kit paths, are refused before the disk is touched, even
    // where a file waits at the end of them.
    REQUIRE (root.getChildFile ("syn01").getChildFile ("x.wav").replaceWithText ("outside the kit's folder"));
    REQUIRE (root.getChildFile ("x.wav").replaceWithText ("outside the kit's folder"));
    for (const auto* path : { "../x.wav", "../../x.wav", "Drums/../../x.wav", "./Drums/Kick A.wav", "/x.wav", "Drums\\Kick A.wav",
             "C:/x.wav", "Drums//Kick A.wav", "Drums/Kick A.aif", "" })
    {
        CAPTURE (path);
        const auto refused = installed.readFile (found, path);
        CHECK_FALSE (refused.ok());
        CHECK (refused.error.rfind ("missing: ", 0) == 0);
        CHECK (refused.value.empty());
    }

    // A listed file that is gone, a folder in its place, and a kit that wasn't found.
    const auto& gone = synManifest().files[3].path;
    REQUIRE (fileIn (kitFolder, 3).deleteFile());
    auto missing = installed.readFile (found, gone);
    CHECK_FALSE (missing.ok());
    CHECK (missing.error.rfind ("missing: " + gone + ": ", 0) == 0);
    REQUIRE (fileIn (kitFolder, 3).createDirectory().wasOk());
    const auto folder = installed.readFile (found, gone);
    CHECK_FALSE (folder.ok());
    CHECK (folder.error.rfind ("unreadable: " + gone + ": ", 0) == 0);
    ContentStatus nowhere;
    nowhere.state = ContentState::missing;
    const auto noFolder = installed.readFile (nowhere, synManifest().files[0].path);
    CHECK_FALSE (noFolder.ok());
    CHECK (noFolder.error.rfind ("missing: ", 0) == 0);
}

TEST_CASE ("readFile reads regular files only: a FIFO or a device at a listed path is refused at once", "[content][sounds]")
{
#if JUCE_WINDOWS
    SKIP ("POSIX FIFOs and devices only");
#else
    // After the kit was found ready, something not a file takes a listed path. A drag reads on the message thread and
    // an audition on the sound thread: a read that waits for a FIFO's writer, or never ends on a device, hangs them.
    TempFolder temp;
    const auto root = temp / "root";
    const auto kitFolder = root.getChildFile ("syn01").getChildFile ("v1");
    writeKit (kitFolder);
    UserSettings settings (temp / "settings.xml");
    ContentLocator::Places places;
    places.platformDefault = root;
    InstalledContent installed (places, settings);
    const auto found = installed.locate (synManifest());
    REQUIRE (found.state == ContentState::ready);
    const auto& path = synManifest().files[2].path;
    const auto file = fileIn (kitFolder, 2);
    REQUIRE (file.deleteFile());

    SECTION ("a FIFO: refused before it is opened, so nothing waits for a writer")
    {
        REQUIRE (mkfifo (file.getFullPathName().toRawUTF8(), 0600) == 0);
        auto reading = std::async (std::launch::async, [&] { return installed.readFile (found, path); });
        const bool returned = reading.wait_for (2s) == std::future_status::ready;
        // A read that waits for a writer gets one, so this fails instead of hanging.
        for (int i = 0; ! returned && i < 50 && reading.wait_for (0s) != std::future_status::ready; ++i)
        {
            if (const int writer = open (file.getFullPathName().toRawUTF8(), O_WRONLY | O_NONBLOCK); writer >= 0)
                close (writer);
            std::this_thread::sleep_for (100ms);
        }
        REQUIRE (returned);
        const auto read = reading.get();
        CHECK_FALSE (read.ok());
        CHECK (read.error == "unreadable: " + path + ": not a regular file");
    }

    SECTION ("a link to a device: refused, never read")
    {
        REQUIRE (symlink ("/dev/null", file.getFullPathName().toRawUTF8()) == 0);
        const auto read = installed.readFile (found, path);
        CHECK_FALSE (read.ok());
        CHECK (read.error == "unreadable: " + path + ": not a regular file");
    }
#endif
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

TEST_CASE ("each kit's phrase spec and library are embedded and parse, and the spec's swing is the manifest's", "[content][embedded][phrases]")
{
    // ADR 0031: content/phrases/<kit>/<KIT>_phrases.json and <KIT>_phrase_library.json, as the checkout has them.
    const juce::File root (WUBANSIA_SOURCE_DIR);
    const KitCatalog catalog (EmbeddedContent::sources());
    REQUIRE (catalog.entries().size() == 4);
    for (const auto& source : EmbeddedContent::sources())
    {
        const std::string id (source.id);
        CAPTURE (id);
        const auto folder = root.getChildFile ("content/phrases").getChildFile (juce::String (id).toLowerCase());
        CHECK (folder.getChildFile (id + "_phrases.json").loadFileAsString().toStdString() == std::string (source.phraseSpec));
        CHECK (folder.getChildFile (id + "_phrase_library.json").loadFileAsString().toStdString() == std::string (source.phraseLibrary));

        const auto* entry = catalog.find (id);
        REQUIRE (entry != nullptr);
        CHECK (entry->phraseError.empty());
        REQUIRE (entry->phraseSpec.has_value());
        CHECK (entry->phraseSpec->kitId == id);
        CHECK (entry->phraseSpec->bpm == entry->info.bpm);
        CHECK (entry->phraseRows.size() == 64);
        // The odd 16ths are late by the kit's swing: (swing_pm - 500) thousandths of an 8th, 480 ticks at 960 PPQ.
        CHECK (entry->phraseSpec->swing == (int) std::floor ((entry->info.swingPm - 500) * 480 / 1000.0 + 0.5));

        // The kit line's chords.
        juce::StringArray chords;
        for (const auto& chord : entry->phraseSpec->chords)
            chords.add (juce::String (chord.name));
        CHECK (entry->card.phraseChords == chords);
    }
    CHECK (catalog.find ("FUT01")->card.phraseChords == juce::StringArray { "F#m9", "Bm9" });
    CHECK (catalog.find ("FUT02")->card.phraseChords == juce::StringArray { "Dm9" });

    // A spec that doesn't parse leaves the kit without phrases, and says why; the kit itself still loads.
    std::vector<KitSource> broken (EmbeddedContent::sources().begin(), EmbeddedContent::sources().end());
    broken.front().phraseSpec = "{ \"format\": 1 }";
    const KitCatalog brokenCatalog (broken);
    const auto* fut01 = brokenCatalog.find ("FUT01");
    REQUIRE (fut01 != nullptr);
    CHECK (fut01->ok());
    CHECK_FALSE (fut01->phraseSpec.has_value());
    CHECK (fut01->phraseRows.empty());
    CHECK_FALSE (fut01->phraseError.empty());
}

TEST_CASE ("the embedded files are the checkout's, byte for byte: manifests and every kit-prefixed MIDI file, no region files, and the phrase files", "[content][embedded]")
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
    for (const auto& source : EmbeddedContent::sources()) // and the phrase spec and library (ADR 0031), checked by their own case
        total += source.phraseSpec.size() + source.phraseLibrary.size();
    CHECK (total == EmbeddedContent::totalBytes());
    CHECK (total < 2'000'000); // about 1.5 MB
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

        // The lanes each Loop and Song file holds (ADR 0029): trackLanes of the file, by timelineIndex.
        REQUIRE (entry.source != nullptr);
        const auto& source = *entry.source;
        for (std::size_t i = 0; i < 4; ++i)
        {
            CAPTURE (i);
            const auto parsed = midi::readSmf (source.smfs[i]);
            REQUIRE (parsed.ok());
            CHECK (entry.card.fileLanes[i] == dragout::trackLanes (entry.info, parsed.value));
            CHECK (entry.card.fileLanes[i] != 0u);
            const bool novox = i == static_cast<std::size_t> (engine::timelineIndex (engine::PlayMode::loop, engine::Variant::novox))
                || i == static_cast<std::size_t> (engine::timelineIndex (engine::PlayMode::song, engine::Variant::novox));
            CHECK ((entry.card.fileLanes[i] & engine::laneBit (engine::Lane::vox)) == (novox ? 0u : engine::laneBit (engine::Lane::vox)));
        }
        const auto loopLanes = [&entry] (engine::Variant v) {
            return entry.card.fileLanes[static_cast<std::size_t> (engine::timelineIndex (engine::PlayMode::loop, v))];
        };
        const auto noLeadOrFx = engine::laneBit (engine::Lane::lead) | engine::laneBit (engine::Lane::fx);
        for (const auto v : { engine::Variant::vox, engine::Variant::novox })
            CHECK ((loopLanes (v) & noLeadOrFx) == (entry.id == "FUT03" ? 0u : noLeadOrFx));

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

TEST_CASE ("kit.json's chord voicings are the chords the Chord voicing track plays", "[content][embedded]")
{
    // Chords as chords (ADR 0027, amended): the recipes wrote the Chord voicing track (channel 3) beside each Chords note
    // as its sample's chord, so at every Chords note of every Loop and Song, kit.json's voicing for channel 4's instrument
    // (the A side of its switch) is the voicing track's notes on that tick, with the same length and velocity.
    const std::map<std::string, std::size_t> expected { { "FUT01", 979 }, { "FUT02", 22 }, { "FUT03", 1120 }, { "FUT04", 50 } };
    std::size_t total = 0;
    for (const auto id : EmbeddedContent::kits())
    {
        CAPTURE (std::string (id));
        const auto kit = engine::parseKitManifest (EmbeddedContent::kitManifest (id));
        REQUIRE (kit.ok());
        const auto& keymap = kit.value.keymap;
        const auto ch4 = std::find_if (keymap.begin(), keymap.end(), [] (const engine::KeymapEntry& k) { return k.ch == 4; });
        REQUIRE (ch4 != keymap.end());
        const auto& zones = kit.value.instruments[static_cast<std::size_t> (ch4->a)].zones;

        std::size_t notes = 0;
        for (const auto mode : { engine::PlayMode::loop, engine::PlayMode::song })
            for (const auto variant : { engine::Variant::vox, engine::Variant::novox })
            {
                const auto file = midi::readSmf (EmbeddedContent::timeline (id, mode, variant));
                REQUIRE (file.ok());
                std::vector<midi::Note> chords;
                std::map<std::int64_t, std::vector<midi::Note>> voicing; // by tick
                for (const auto& track : file.value.tracks)
                {
                    if (midi::trackName (track) == "Chords")
                        chords = midi::notesOf (track);
                    if (midi::trackName (track) == "Chord voicing")
                        for (const auto& n : midi::notesOf (track))
                            voicing[n.tick].push_back (n);
                }
                REQUIRE_FALSE (chords.empty());
                for (const auto& n : chords)
                {
                    INFO ("the Chords note at " << n.tick << ", key " << int { n.key });
                    const auto zone = std::find_if (zones.begin(), zones.end(), [&] (const engine::ZoneEntry& z) { return z.lo <= n.key && n.key <= z.hi; });
                    REQUIRE (zone != zones.end());
                    REQUIRE_FALSE (zone->voicing.empty());
                    std::vector<int> want;
                    for (const auto interval : zone->voicing)
                        want.push_back (n.key + interval);
                    std::vector<int> got;
                    for (const auto& v : voicing[n.tick])
                    {
                        got.push_back (v.key);
                        CHECK (v.length == n.length);
                        CHECK (v.velocity == n.velocity);
                    }
                    std::sort (got.begin(), got.end());
                    CHECK (got == want);
                    ++notes;
                }
            }
        CHECK (notes == expected.at (std::string (id)));
        total += notes;
    }
    CHECK (total == 2171);
}

TEST_CASE ("the catalog gives each kit its chord tracks", "[content][embedded]")
{
    // Chords as chords (ADR 0027, amended): Chords (channel 4) and each chord extra, in channel order, with each zone's
    // keys and its voicing (semitones over the zone's root), as the recipes voice them.
    using Zone = engine::KitChordZone;
    using Track = engine::KitChordTrack;
    const std::vector<std::int8_t> m9 { 0, 7, 10, 14, 15 }; // root, 5, m7, 9, m3: FUT01's stabs, FUT02's Dm9 stab, FUT03's Gm9
    const std::vector<std::int8_t> em { 0, 7, 12, 15 }; // FUT04's Em, open
    const std::map<std::string, std::vector<Track>> expected {
        { "FUT01", { Track { "Chords", { Zone { 0, 58, m9 }, Zone { 59, 127, m9 } } }, Track { "Air Pad", { Zone { 0, 127, { 0, 10, 15, 19, 26 } } } } } },
        { "FUT02", { Track { "Chords", { Zone { 0, 127, { 0, 7, 10, 15, 19, 26 } } } },
                       Track { "Laser Chord Stab", { Zone { 0, 61, { 0, 7, 11, 14, 16 } }, Zone { 62, 127, m9 } } } } },
        { "FUT03", { Track { "Chords", { Zone { 0, 127, m9 } } }, Track { "Pad Warm", { Zone { 0, 127, m9 } } } } },
        { "FUT04", { Track { "Chords", { Zone { 0, 127, { 0, 7, 12, 15, 26 } } } }, Track { "Metal Chord", { Zone { 0, 127, em } } },
                       Track { "Rave Stab", { Zone { 0, 127, em } } } } },
    };
    const KitCatalog catalog (EmbeddedContent::sources());
    REQUIRE (catalog.entries().size() == 4);
    for (const auto& entry : catalog.entries())
    {
        CAPTURE (entry.id);
        const auto& tracks = entry.info.chordTracks;
        const auto& want = expected.at (entry.id);
        REQUIRE (tracks.size() == want.size());
        for (std::size_t i = 0; i < tracks.size(); ++i)
        {
            CAPTURE (i);
            CHECK (tracks[i].name == want[i].name);
            CHECK (tracks[i] == want[i]);
        }
    }

    // FUT01's stab switch: the organ (A) and the e-piano (B) voice alike, so Chords plays one chord on either side.
    const auto fut01 = engine::parseKitManifest (EmbeddedContent::kitManifest ("FUT01"));
    REQUIRE (fut01.ok());
    const auto instrument = [&] (std::string_view iid) -> const engine::InstrumentEntry& {
        const auto& all = fut01.value.instruments;
        const auto found = std::find_if (all.begin(), all.end(), [&] (const engine::InstrumentEntry& in) { return in.id == iid; });
        REQUIRE (found != all.end());
        return *found;
    };
    const auto& organ = instrument ("organ_stab");
    const auto& piano = instrument ("e_piano_stab");
    REQUIRE (organ.zones.size() == 2);
    REQUIRE (piano.zones.size() == 2);
    for (std::size_t i = 0; i < 2; ++i)
    {
        CHECK (organ.zones[i].voicing == std::vector<std::int32_t> { 0, 7, 10, 14, 15 });
        CHECK (piano.zones[i].voicing == organ.zones[i].voicing);
        CHECK (piano.zones[i].lo == organ.zones[i].lo);
        CHECK (piano.zones[i].hi == organ.zones[i].hi);
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
