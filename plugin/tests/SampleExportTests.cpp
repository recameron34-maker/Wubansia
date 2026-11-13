// The Samples drawer's copies (ADR 0024, sounds/SampleExport.h): each sound's file under a name that says
// what it is, in <samples folder>/<KIT> <Title>/, the installed bytes exactly, never overwriting or
// deleting a file. The names are the four committed manifests'; the copies come from the synthetic kit's
// WAVs (SyntheticContent::readFile), and, in the hidden [.real] case, from installed content.

#include "content/ContentLocator.h"
#include "content/EmbeddedContent.h"
#include "content/InstalledContent.h"
#include "content/UserSettings.h"
#include "dragout/DragFiles.h"
#include "helpers/synthetic_content.h"
#include "services/KitCatalog.h"
#include "sounds/SampleExport.h"
#include "sounds/SoundCatalog.h"

#include "testing/synthetic_kit.h"
#include "util/sha256.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <algorithm>
#include <chrono>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#if ! JUCE_WINDOWS
    #include <csignal>
    #include <sys/resource.h>
    #include <sys/stat.h>
    #include <unistd.h>
#endif

using namespace wubansia;

namespace
{
    // A fresh folder under the temp folder, deleted with this.
    struct TempFolder
    {
        TempFolder() : folder (juce::File::getSpecialLocation (juce::File::tempDirectory).getNonexistentChildFile ("WubansiaExportTest", "", false))
        {
            folder.createDirectory();
        }
        ~TempFolder() { folder.deleteRecursively(); }

        juce::File operator/ (const char* child) const { return folder.getChildFile (child); }

        juce::File folder;
    };

    // A time well before any test runs: a file that keeps it was never rewritten.
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
    const std::vector<std::uint8_t>& synWav (std::size_t file) { return engine::testing::syntheticContent().wavs[file]; }
    engine::KitInfo synInfo() { return kitInfoOf (syn(), 0); }

    std::size_t fileIndex (const engine::KitManifest& kit, std::string_view path)
    {
        for (std::size_t i = 0; i < kit.files.size(); ++i)
            if (kit.files[i].path == path)
                return i;
        FAIL ("no file " << std::string (path));
        return 0;
    }

    const engine::FileEntry& fileAt (const engine::KitManifest& kit, std::string_view path) { return kit.files[fileIndex (kit, path)]; }

    const SoundInfo& named (const std::vector<SoundInfo>& sounds, const juce::String& name)
    {
        const auto it = std::find_if (sounds.begin(), sounds.end(), [&] (const SoundInfo& s) { return s.name == name; });
        INFO ("sound " << name.toStdString());
        REQUIRE (it != sounds.end());
        return *it;
    }

    std::vector<std::uint8_t> bytesOf (const juce::File& file)
    {
        juce::MemoryBlock block;
        if (! file.loadFileAsData (block))
            return {};
        const auto* data = static_cast<const std::uint8_t*> (block.getData());
        return std::vector<std::uint8_t> (data, data + block.getSize());
    }

    juce::String text (const std::string& s) { return juce::String::fromUTF8 (s.data(), (int) s.size()); }

    // Everything in folder, hidden files and folders included.
    int entriesIn (const juce::File& folder) { return folder.getNumberOfChildFiles (juce::File::findFilesAndDirectories); }

    // The synthetic kit's file at path, exported from content into samples.
    sounds::ExportResult exportSyn (test::SyntheticContent& content, std::string_view path, const juce::File& samples)
    {
        const auto found = content.locate (syn());
        REQUIRE (found.state == ContentState::ready);
        return sounds::exportCopy (content, found, synInfo(), fileAt (syn(), path), samples);
    }
}

TEST_CASE ("export names: the installed name first, less a leading 'Accent ', then the kit's id, tempo, key and Camelot", "[sounds][export]")
{
    struct Example
    {
        const char* kit;
        const char* sound; // whose default file is path: what the drawer's tile names
        const char* path;
        const char* folder;
        const char* name;
    };
    const Example examples[] {
        { "FUT01", "Rave Stab", "Music/Rave Stab v2.wav", "FUT01 Alloy Signal", "Rave Stab v2 - FUT01 134 F#m 11A.wav" },
        { "FUT01", "Shift Riser", "FX/Shift Riser.wav", "FUT01 Alloy Signal", "Shift Riser - FUT01 134 F#m 11A.wav" },
        { "FUT02", "Laser Chord Stab Dm9", "Music/Laser Chord Stab D4 v2.wav", "FUT02 Orbit Relay", "Laser Chord Stab D4 v2 - FUT02 130 Dm 7A.wav" },
        // Synths' Laser Chord Stab defaults to the same file: one file, one path.
        { "FUT02", "Laser Chord Stab", "Music/Laser Chord Stab D4 v2.wav", "FUT02 Orbit Relay", "Laser Chord Stab D4 v2 - FUT02 130 Dm 7A.wav" },
        // "Accent " goes, as it goes from the drawer's names: a pad or Simpler shows the sound, not "Accent Du...".
        { "FUT03", "Dub Chord Cm9", "Music/Accent Dub Chord Cm9.wav", "FUT03 Glass Current", "Dub Chord Cm9 - FUT03 128 Gm 6A.wav" },
        { "FUT03", "Context Vox Riff", "Stems/Context Vox Riff.wav", "FUT03 Glass Current", "Context Vox Riff - FUT03 128 Gm 6A.wav" },
        { "FUT04", "Mod Blip", "Music/Mod Blip E5 v2.wav", "FUT04 Furnace Code", "Mod Blip E5 v2 - FUT04 136 Em 9A.wav" },
        { "FUT04", "Sub Drop E2", "Bass/Accent Sub Drop E2.wav", "FUT04 Furnace Code", "Sub Drop E2 - FUT04 136 Em 9A.wav" }, // in FX
    };
    for (const auto& e : examples)
    {
        CAPTURE (e.kit, e.sound);
        const auto& entry = kitEntry (e.kit);
        const auto kitSounds = sounds::soundsOf (entry.manifest);
        const auto& sound = named (kitSounds, e.sound);
        REQUIRE (sound.defaultFile < (int) sound.files.size());
        const auto& file = entry.manifest.files[(std::size_t) sound.files[(std::size_t) sound.defaultFile].file];
        CHECK (file.path == e.path);
        CHECK (sounds::exportName (entry.info, file) == e.name);
        CHECK (sounds::kitFolderName (entry.info) == e.folder);
    }

    CHECK (sounds::exportName (synInfo(), fileAt (syn(), "FX/Riser.wav")) == "Riser - SYN01 128 Gm 6A.wav");
    CHECK (sounds::kitFolderName (synInfo()) == "SYN01 Synthetic Test Kit");
}

TEST_CASE ("every export name is unique in its kit ignoring case, in safe characters, within 196 characters", "[sounds][export]")
{
    std::size_t count = 0, accents = 0;
    std::string longest;
    for (const auto& entry : embedded().entries())
    {
        CAPTURE (entry.id);
        REQUIRE (entry.ok());
        std::set<std::string> lowered;
        for (const auto& file : entry.manifest.files)
        {
            const auto name = sounds::exportName (entry.info, file);
            CAPTURE (file.path, name);
            REQUIRE (name.size() > 4);
            CHECK (name.ends_with (".wav"));
            CHECK (name.size() - 4 <= sounds::kMaxNameStem);
            CHECK (dragout::safeName (name) == name); // only safe characters, single spaces, none at the ends
            CHECK (name.front() != '.');

            // The installed name comes first, as it is but for a leading "Accent " (18 of FUT03's and FUT04's).
            auto stem = file.path.substr (file.path.rfind ('/') + 1);
            stem.resize (stem.size() - 4);
            if (stem.starts_with ("Accent ") && stem.size() > 7)
            {
                stem = stem.substr (7);
                ++accents;
            }
            CHECK (name.rfind (stem + " - " + entry.id + " ", 0) == 0);
            CHECK_FALSE (name.starts_with ("Accent "));

            CHECK (lowered.insert (text (name).toLowerCase().toStdString()).second);
            if (name.size() > longest.size())
                longest = name;
            ++count;
        }
    }
    CHECK (count == 618);
    CHECK (accents == 18);
    CHECK (longest == "Acid Line 1 Clean 4 Bars - FUT04 136 Em 9A.wav"); // 46 characters
}

TEST_CASE ("content v2 and later add c<N>; v1 adds nothing", "[sounds][export]")
{
    auto info = kitEntry ("FUT01").info;
    const auto& file = fileAt (kitEntry ("FUT01").manifest, "Music/Rave Stab v2.wav");
    REQUIRE (info.contentVersion == 1);
    CHECK (sounds::exportName (info, file) == "Rave Stab v2 - FUT01 134 F#m 11A.wav");
    info.contentVersion = 2;
    CHECK (sounds::exportName (info, file) == "Rave Stab v2 - FUT01 134 F#m 11A c2.wav");
    info.contentVersion = 12;
    CHECK (sounds::exportName (info, file) == "Rave Stab v2 - FUT01 134 F#m 11A c12.wav");
    info.contentVersion = 0;
    CHECK (sounds::exportName (info, file) == "Rave Stab v2 - FUT01 134 F#m 11A.wav");
    CHECK (sounds::kitFolderName (info) == "FUT01 Alloy Signal"); // one folder for every version

    SECTION ("names stay safe whatever the kit's strings")
    {
        engine::KitInfo odd;
        odd.id = "ODD01";
        odd.title = "Odd: \"Kit\" / Test";
        odd.bpm = 130;
        odd.key = "Bb major";
        odd.camelot = "6B";
        odd.contentVersion = 1;
        engine::FileEntry f;
        f.path = "Music/Pad*Wide? <Dm> | v2.wav";
        CHECK (sounds::exportName (odd, f) == "Pad Wide Dm v2 - ODD01 130 Bbmaj 6B.wav");
        CHECK (sounds::kitFolderName (odd) == "ODD01 Odd Kit Test");

        f.path = "Music/.hidden take.wav"; // never a hidden file
        CHECK (sounds::exportName (odd, f) == "hidden take - ODD01 130 Bbmaj 6B.wav");

        f.path = "Music/Accent Pad.wav"; // a leading "Accent " goes, as in the drawer...
        CHECK (sounds::exportName (odd, f) == "Pad - ODD01 130 Bbmaj 6B.wav");
        f.path = "Music/Accent.wav"; // ...but never the whole name
        CHECK (sounds::exportName (odd, f) == "Accent - ODD01 130 Bbmaj 6B.wav");
        f.path = "Music/Accented Pad.wav";
        CHECK (sounds::exportName (odd, f) == "Accented Pad - ODD01 130 Bbmaj 6B.wav");

        f.path = "Music/" + std::string (300, 'x') + ".wav"; // cut to fit, the kit's part kept
        const auto cut = sounds::exportName (odd, f);
        CHECK (cut.size() == sounds::kMaxNameStem + 4);
        CHECK (cut == std::string (175, 'x') + " - ODD01 130 Bbmaj 6B.wav");
    }
}

TEST_CASE ("a copy is the installed file byte for byte, in <samples>/<KIT> <Title>/", "[sounds][export]")
{
    TempFolder temp;
    test::SyntheticContent content;
    const auto found = content.locate (syn());
    REQUIRE (found.state == ContentState::ready);
    const auto samples = temp / "Samples"; // not there yet: made
    const auto kitFolder = samples.getChildFile ("SYN01 Synthetic Test Kit");

    for (std::size_t i = 0; i < syn().files.size(); ++i)
    {
        const auto& file = syn().files[i];
        CAPTURE (file.path);
        const auto copy = sounds::exportCopy (content, found, synInfo(), file, samples);
        INFO (copy.problem);
        REQUIRE (copy.file.existsAsFile());
        CHECK (copy.problem.empty());
        CHECK (copy.file.getParentDirectory() == kitFolder);
        CHECK (copy.file.getFileName() == text (sounds::exportName (synInfo(), file)));
        CHECK (bytesOf (copy.file) == synWav (i));
    }
    CHECK (entriesIn (kitFolder) == 30);
    CHECK (entriesIn (samples) == 1);
    CHECK (content.fileReads.load() == 30);
}

TEST_CASE ("a second export writes nothing and gives the same file", "[sounds][export]")
{
    TempFolder temp;
    test::SyntheticContent content;
    const auto samples = temp / "Samples";
    const auto first = exportSyn (content, "FX/Riser.wav", samples);
    REQUIRE (first.file.existsAsFile());
    REQUIRE (first.file.setLastModificationTime (past));
    const auto size = first.file.getSize();

    const auto second = exportSyn (content, "FX/Riser.wav", samples);
    CHECK (second.problem.empty());
    CHECK (second.file == first.file);
    CHECK (second.file.getLastModificationTime() == past); // not rewritten
    CHECK (second.file.getSize() == size);
    CHECK (entriesIn (first.file.getParentDirectory()) == 1);
}

TEST_CASE ("an edited copy is never overwritten: the next export writes (2), and the one after reuses it", "[sounds][export]")
{
    TempFolder temp;
    test::SyntheticContent content;
    const auto samples = temp / "Samples";
    const auto kitFolder = samples.getChildFile ("SYN01 Synthetic Test Kit");
    const auto riser = synWav (fileIndex (syn(), "FX/Riser.wav"));

    const auto first = exportSyn (content, "FX/Riser.wav", samples);
    REQUIRE (first.file == kitFolder.getChildFile ("Riser - SYN01 128 Gm 6A.wav"));

    // Edited in a DAW: other bytes under the export's name.
    auto edited = riser;
    edited[1000] ^= 0xFF;
    edited.push_back (0);
    REQUIRE (first.file.replaceWithData (edited.data(), edited.size()));
    REQUIRE (first.file.setLastModificationTime (past));

    const auto second = exportSyn (content, "FX/Riser.wav", samples);
    CHECK (second.problem.empty());
    CHECK (second.file == kitFolder.getChildFile ("Riser - SYN01 128 Gm 6A (2).wav"));
    CHECK (bytesOf (second.file) == riser);
    CHECK (bytesOf (first.file) == edited); // untouched
    CHECK (first.file.getLastModificationTime() == past);

    REQUIRE (second.file.setLastModificationTime (past));
    const auto third = exportSyn (content, "FX/Riser.wav", samples);
    CHECK (third.file == second.file); // reused, not rewritten
    CHECK (second.file.getLastModificationTime() == past);

    // (2) edited too, and a folder holding (3): the copy goes to (4), and nothing else moves.
    REQUIRE (second.file.replaceWithData (edited.data(), edited.size() - 1));
    REQUIRE (kitFolder.getChildFile ("Riser - SYN01 128 Gm 6A (3).wav").createDirectory().wasOk());
    const auto fourth = exportSyn (content, "FX/Riser.wav", samples);
    CHECK (fourth.file == kitFolder.getChildFile ("Riser - SYN01 128 Gm 6A (4).wav"));
    CHECK (bytesOf (fourth.file) == riser);
    CHECK (bytesOf (first.file) == edited);
    CHECK (kitFolder.getChildFile ("Riser - SYN01 128 Gm 6A (3).wav").isDirectory());
    CHECK (entriesIn (kitFolder) == 4);
}

TEST_CASE ("bytes that aren't the ones kit.json lists are refused, and nothing is written", "[sounds][export]")
{
    TempFolder temp;
    const auto samples = temp / "Samples";
    const auto& riser = fileAt (syn(), "FX/Riser.wav");

    SECTION ("a byte changed: another SHA-256")
    {
        test::SyntheticOptions options;
        options.alteredPath = "FX/Riser.wav";
        test::SyntheticContent content (options);
        const auto refused = exportSyn (content, "FX/Riser.wav", samples);
        CHECK (refused.file == juce::File());
        CHECK (refused.problem == "Riser.wav isn't the file SYN01's content v1 lists: reinstall SYN01");
        CHECK (refused.problem == sounds::listingProblem (synInfo(), riser));
        CHECK (refused.sourceProblem); // the kit's file, not the samples folder
        CHECK_FALSE (samples.exists()); // not even the folder

        // The kit's other files are still copied.
        CHECK (exportSyn (content, "Vox/Vox G4 v3.wav", samples).file.existsAsFile());
    }

    SECTION ("a Git LFS pointer where the sample should be")
    {
        test::SyntheticContent content;
        const auto found = content.locate (syn());
        content.setSamples (test::SyntheticSamples::lfsPointers);
        const auto refused = sounds::exportCopy (content, found, synInfo(), riser, samples);
        CHECK (refused.file == juce::File());
        CHECK (refused.problem == sounds::listingProblem (synInfo(), riser));
        CHECK (refused.sourceProblem);
        CHECK_FALSE (samples.exists());
    }

    SECTION ("a file that can't be read: named, and nothing written")
    {
        test::SyntheticContent content;
        const auto found = content.locate (syn());
        content.setSamples (test::SyntheticSamples::missing); // gone since the kit was found
        const auto refused = sounds::exportCopy (content, found, synInfo(), fileAt (syn(), "Drums/Snare v1 rr2.wav"), samples);
        CHECK (refused.file == juce::File());
        CHECK (text (refused.problem).contains ("Drums/Snare v1 rr2.wav"));
        CHECK (text (refused.problem).contains ("missing"));
        CHECK (refused.sourceProblem);
        CHECK_FALSE (samples.exists());
    }

    SECTION ("the listed size: 44 bytes of header, the audio, and a pad byte when it is odd")
    {
        for (std::size_t i = 0; i < syn().files.size(); ++i)
        {
            CAPTURE (syn().files[i].path);
            CHECK (sounds::listedBytes (syn().files[i]) == synWav (i).size());
            CHECK (sounds::matchesListing (synWav (i), syn().files[i]));
        }
        CHECK (sounds::listedBytes (riser) == 44u + 90000u * 2u * 3u);
        engine::FileEntry odd;
        odd.frames = 3;
        odd.channels = 1;
        CHECK (sounds::listedBytes (odd) == 44u + 9u + 1u);

        auto bytes = synWav (fileIndex (syn(), "FX/Riser.wav"));
        CHECK (sounds::matchesListing (bytes, riser));
        bytes.push_back (0); // one byte more: the size alone refuses it
        CHECK_FALSE (sounds::matchesListing (bytes, riser));
        bytes.pop_back();
        bytes[44] ^= 0x01; // the same size, another SHA-256
        CHECK_FALSE (sounds::matchesListing (bytes, riser));
        CHECK_FALSE (sounds::matchesListing ({}, riser));
    }
}

TEST_CASE ("a folder that can't be written gives a problem naming it", "[sounds][export]")
{
    TempFolder temp;
    test::SyntheticContent content;
    const auto blocker = temp / "Samples"; // a file where the samples folder should be
    REQUIRE (blocker.replaceWithText ("a file, not a folder"));

    const auto refused = exportSyn (content, "FX/Riser.wav", blocker);
    CHECK (refused.file == juce::File());
    CHECK_FALSE (refused.sourceProblem); // the folder's: another one helps
    INFO (refused.problem);
    CHECK (text (refused.problem).contains (blocker.getChildFile ("SYN01 Synthetic Test Kit").getFullPathName()));
    CHECK (blocker.loadFileAsString() == "a file, not a folder");
    CHECK (entriesIn (temp.folder) == 1);
}

TEST_CASE ("a read-only kit folder gives a problem naming it and leaves nothing in it", "[sounds][export]")
{
#if JUCE_WINDOWS
    SKIP ("POSIX permissions only");
#else
    if (geteuid() == 0)
        SKIP ("root writes into read-only folders");

    TempFolder temp;
    test::SyntheticContent content;
    const auto samples = temp / "Samples";
    const auto kitFolder = samples.getChildFile ("SYN01 Synthetic Test Kit");
    REQUIRE (kitFolder.createDirectory().wasOk());
    REQUIRE (chmod (kitFolder.getFullPathName().toRawUTF8(), 0555) == 0);
    const auto refused = exportSyn (content, "FX/Riser.wav", samples);
    chmod (kitFolder.getFullPathName().toRawUTF8(), 0755);

    CHECK (refused.file == juce::File());
    INFO (refused.problem);
    CHECK (text (refused.problem).contains (kitFolder.getFullPathName()));
    CHECK (entriesIn (kitFolder) == 0); // no temporary file left behind
#endif
}

#if ! JUCE_WINDOWS
namespace
{
    // Caps the size of any file this process writes (RLIMIT_FSIZE, the soft limit only, so it can be raised back),
    // with SIGXFSZ ignored so a write past it fails (EFBIG) instead of killing the process: a disk that fills up
    // part-way through a copy. Both come back when this goes, even if a REQUIRE throws meanwhile.
    struct FileSizeCap
    {
        explicit FileSizeCap (rlim_t bytes)
        {
            if (getrlimit (RLIMIT_FSIZE, &saved) != 0)
                return;
            previous = std::signal (SIGXFSZ, SIG_IGN);
            auto capped = saved;
            capped.rlim_cur = std::min (bytes, saved.rlim_max);
            capping = setrlimit (RLIMIT_FSIZE, &capped) == 0;
        }

        ~FileSizeCap()
        {
            if (capping)
                setrlimit (RLIMIT_FSIZE, &saved);
            if (previous != SIG_ERR)
                std::signal (SIGXFSZ, previous);
        }

        rlimit saved {};
        void (*previous) (int) = SIG_ERR;
        bool capping = false;
    };
}
#endif

TEST_CASE ("a write that fails part-way leaves nothing under the export name; the next export writes it whole", "[sounds][export]")
{
#if JUCE_WINDOWS
    SKIP ("POSIX file size limits only");
#else
    // writeNew checks the write and the size before the rename: File::replaceWithData () would rename a short file
    // into place, under the frozen name projects refer to, and every later export would go to " (2)".
    TempFolder temp;
    test::SyntheticContent content;
    const auto samples = temp / "Samples";
    const auto kitFolder = samples.getChildFile ("SYN01 Synthetic Test Kit");
    const auto target = kitFolder.getChildFile ("Riser - SYN01 128 Gm 6A.wav");
    const auto& riser = synWav (fileIndex (syn(), "FX/Riser.wav"));
    REQUIRE (riser.size() == 540044u);
    const auto found = content.locate (syn());
    REQUIRE (found.state == ContentState::ready);

    const rlim_t limit = GENERATE (rlim_t { 100000 }, rlim_t { 0 }); // part of it written, then none of it
    CAPTURE (limit);
    sounds::ExportResult refused;
    bool capped = false;
    {
        FileSizeCap cap (limit);
        capped = cap.capping;
        refused = sounds::exportCopy (content, found, synInfo(), fileAt (syn(), "FX/Riser.wav"), samples);
    }
    // Every check once the cap is gone: under it, even this test's own output could be cut short.
    REQUIRE (capped);
    CHECK (refused.file == juce::File());
    CHECK_FALSE (refused.sourceProblem);
    CHECK (text (refused.problem).contains (target.getFullPathName()));
    CHECK_FALSE (target.exists());
    CHECK (entriesIn (kitFolder) == 0); // no hidden temporary file left behind

    const auto whole = sounds::exportCopy (content, found, synInfo(), fileAt (syn(), "FX/Riser.wav"), samples);
    INFO (whole.problem);
    CHECK (whole.file == target); // the plain name: nothing short took it
    CHECK (bytesOf (whole.file) == riser);
#endif
}

TEST_CASE ("a dangling link at the export name is left as it is: the copy takes the next name", "[sounds][export]")
{
#if JUCE_WINDOWS
    SKIP ("POSIX symbolic links only");
#else
    // A link reads as missing when its target has gone, and JUCE's rename deletes what is at the name first: without
    // exportCopy's own check, the person's link would go.
    TempFolder temp;
    test::SyntheticContent content;
    const auto samples = temp / "Samples";
    const auto kitFolder = samples.getChildFile ("SYN01 Synthetic Test Kit");
    REQUIRE (kitFolder.createDirectory().wasOk());
    const auto link = kitFolder.getChildFile ("Riser - SYN01 128 Gm 6A.wav");
    const auto movedAway = (temp / "Moved Away").getChildFile ("Riser.wav"); // not there
    REQUIRE (symlink (movedAway.getFullPathName().toRawUTF8(), link.getFullPathName().toRawUTF8()) == 0);
    REQUIRE (link.isSymbolicLink());
    REQUIRE_FALSE (link.exists());

    const auto copy = exportSyn (content, "FX/Riser.wav", samples);
    INFO (copy.problem);
    CHECK (copy.problem.empty());
    CHECK (copy.file == kitFolder.getChildFile ("Riser - SYN01 128 Gm 6A (2).wav"));
    CHECK (bytesOf (copy.file) == synWav (fileIndex (syn(), "FX/Riser.wav")));
    CHECK (link.isSymbolicLink()); // the link, as it was
    CHECK (link.getLinkedTarget() == movedAway);
    CHECK_FALSE (movedAway.exists()); // nothing written through it
    CHECK (entriesIn (kitFolder) == 2);
#endif
}

TEST_CASE ("exports leave no temporary file and never touch other files", "[sounds][export]")
{
    TempFolder temp;
    const auto samples = temp / "Samples";
    const auto kitFolder = samples.getChildFile ("SYN01 Synthetic Test Kit");
    REQUIRE (kitFolder.createDirectory().wasOk());

    // 100 files a person or a DAW keeps beside the copies: samples, Live's .asd analysis files, MIDI. One
    // holds an export's own name with other bytes.
    struct Seeded
    {
        juce::File file;
        std::vector<std::uint8_t> bytes;
    };
    std::vector<Seeded> seeded;
    juce::Random random (24);
    const char* kinds[] { ".wav", ".asd", ".mid" };
    for (int i = 0; i < 100; ++i)
    {
        const auto name = i == 0   ? juce::String ("Riser - SYN01 128 Gm 6A.wav")
                          : i == 1 ? juce::String ("Kick A - SYN01 128 Gm 6A.wav.asd")
                                   : "Seed " + juce::String (i).paddedLeft ('0', 3) + kinds[i % 3];
        std::vector<std::uint8_t> bytes ((std::size_t) (100 + random.nextInt (4000)));
        for (auto& b : bytes)
            b = (std::uint8_t) random.nextInt (256);
        const auto file = kitFolder.getChildFile (name);
        REQUIRE (file.replaceWithData (bytes.data(), bytes.size()));
        REQUIRE (file.setLastModificationTime (past));
        seeded.push_back ({ file, std::move (bytes) });
    }

    test::SyntheticContent content;
    for (int pass = 0; pass < 2; ++pass)
        for (const auto& file : syn().files)
        {
            CAPTURE (pass, file.path);
            const auto copy = exportSyn (content, file.path, samples);
            INFO (copy.problem);
            REQUIRE (copy.file.existsAsFile());
        }

    for (const auto& s : seeded)
    {
        CAPTURE (s.file.getFileName().toStdString());
        CHECK (bytesOf (s.file) == s.bytes);
        CHECK (s.file.getLastModificationTime() == past);
    }
    CHECK (kitFolder.getChildFile ("Riser - SYN01 128 Gm 6A (2).wav").existsAsFile());
    CHECK (entriesIn (kitFolder) == 130); // the 100, and the 30 copies once each
    for (const auto& f : temp.folder.findChildFiles (juce::File::findFilesAndDirectories, true))
        CHECK_FALSE (f.getFileName().startsWithChar ('.')); // no hidden temporary file anywhere
}

TEST_CASE ("the samples folder: the default, a chosen one kept in settings.xml beside the content folder, without a content recheck", "[sounds][export][content]")
{
    CHECK (sounds::defaultSamplesFolder() == juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Wubansia").getChildFile ("Samples"));

    TempFolder temp;
    const auto file = (temp / "Wubansia Audio").getChildFile ("settings.xml"); // its folder doesn't exist yet
    const auto content = temp / "My Content";
    const auto mine = temp / "My Samples";
    {
        UserSettings settings (file);
        CHECK (settings.samplesFolder() == juce::File()); // none chosen: the Controller uses the default
        REQUIRE (settings.setContentFolder (content));
        const auto generation = settings.generation();
        CHECK (settings.setSamplesFolder (mine));
        CHECK (settings.generation() == generation); // no content recheck
        CHECK (settings.samplesFolder() == mine);
        CHECK (settings.contentFolder() == content);
        CHECK (settings.fileReads() == 1);
    }
    REQUIRE (file.existsAsFile());
    const auto xml = juce::XmlDocument::parse (file);
    REQUIRE (xml != nullptr);
    CHECK (xml->getStringAttribute ("samples_folder") == mine.getFullPathName());
    CHECK (xml->getStringAttribute ("content_folder") == content.getFullPathName());

    {
        UserSettings again (file);
        CHECK (again.samplesFolder() == mine);
        CHECK (again.contentFolder() == content);
        CHECK (again.setContentFolder ({})); // the content folder's setter keeps the samples folder
    }
    CHECK (UserSettings (file).samplesFolder() == mine);
    {
        UserSettings again (file);
        CHECK (again.setSamplesFolder ({})); // forgotten: back to the default
        CHECK (again.samplesFolder() == juce::File());
    }
    CHECK (UserSettings (file).samplesFolder() == juce::File());

    // A relative or empty path in the file is ignored.
    REQUIRE (file.replaceWithText ("<?xml version=\"1.0\"?>\n<WubansiaSettings samples_folder=\"relative/folder\"/>\n"));
    CHECK (UserSettings (file).samplesFolder() == juce::File());
}

TEST_CASE ("every sound's default file of the four kits exports byte for byte", "[sounds][export][.real]")
{
    // Hidden: it reads and copies about 60 MB of WAV. Run it on a content folder:
    //     WUBANSIA_CONTENT_DIR=/path/to/content WubansiaPluginTests "[.real]"
    const auto dir = juce::SystemStats::getEnvironmentVariable ("WUBANSIA_CONTENT_DIR", {});
    if (dir.isEmpty())
        SKIP ("set WUBANSIA_CONTENT_DIR to a content folder");

    TempFolder temp;
    UserSettings settings (temp / "settings.xml");
    ContentLocator::Places places;
    places.environment = juce::File::getCurrentWorkingDirectory().getChildFile (dir);
    InstalledContent installed (places, settings);
    const auto samples = temp / "Samples";

    std::size_t exported = 0;
    std::set<juce::String> files;
    const auto start = std::chrono::steady_clock::now();
    for (const auto& entry : embedded().entries())
    {
        CAPTURE (entry.id);
        const auto found = installed.locate (entry.manifest);
        INFO (found.problem);
        REQUIRE (found.state == ContentState::ready);
        for (const auto& sound : sounds::soundsOf (entry.manifest))
        {
            CAPTURE (sound.id.toStdString());
            const auto& file = entry.manifest.files[(std::size_t) sound.files[(std::size_t) sound.defaultFile].file];
            const auto copy = sounds::exportCopy (installed, found, entry.info, file, samples);
            INFO (copy.problem);
            REQUIRE (copy.file.existsAsFile());
            CHECK (copy.file.getFileName() == text (sounds::exportName (entry.info, file)));
            CHECK (copy.file.getParentDirectory() == samples.getChildFile (text (sounds::kitFolderName (entry.info))));
            const auto bytes = bytesOf (copy.file);
            CHECK (bytes == bytesOf (found.folder.getChildFile (text (file.path))));
            CHECK (util::sha256Hex (bytes) == file.sha256);
            files.insert (copy.file.getFullPathName());
            ++exported;
        }
    }
    const auto took = std::chrono::steady_clock::now() - start;
    CHECK (exported == 140);
    CHECK (files.size() == 139); // FUT02's Laser Chord Stab D4 v2 is two sounds' default: one file
    std::ostringstream line;
    line << exported << " default files exported byte for byte from " << places.environment.getFullPathName() << " in "
         << std::chrono::duration_cast<std::chrono::milliseconds> (took).count() << " ms";
    WARN (line.str());
}
