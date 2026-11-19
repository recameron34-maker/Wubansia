#include "dragout/DragFiles.h"

#include "kit/channel_map.h"
#include "midi/for_synths.h"
#include "midi/reader.h"
#include "midi/track_select.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <utility>

#if JUCE_LINUX || JUCE_BSD
    #include <cerrno>
    #include <fcntl.h>
    #include <sys/wait.h>
    #include <unistd.h>
#endif

namespace wubansia::dragout
{
    namespace
    {
        using DragResult = util::Result<DragFile>;

        // A lane's name where the kit gives it no label.
        constexpr std::array<std::string_view, engine::kLanes> kLaneNames {
            "Kick", "Snare", "Hats", "Perc", "Bass", "Lead", "Chords", "Vox", "Accents", "FX"
        };

        // Longest name before ".mid": far below every file system's 255 bytes, with room for the folder.
        constexpr std::size_t kMaxStem = 196;

        bool isLower (char c) { return c >= 'a' && c <= 'z'; }
        bool isUpper (char c) { return c >= 'A' && c <= 'Z'; }
        bool isDigit (char c) { return c >= '0' && c <= '9'; }

        // Letters, digits, spaces and # ( ) , - . _: nothing a file system or a shell path reads.
        bool isSafe (char c)
        {
            return isLower (c) || isUpper (c) || isDigit (c) || c == ' ' || c == '#' || c == '(' || c == ')' || c == ','
                   || c == '-' || c == '.' || c == '_';
        }

        std::string_view baseName (std::string_view path)
        {
            const auto slash = path.find_last_of ("/\\");
            return slash == std::string_view::npos ? path : path.substr (slash + 1);
        }

        bool startsWith (std::string_view s, std::string_view prefix) { return s.substr (0, prefix.size()) == prefix; }

        bool endsWith (std::string_view s, std::string_view suffix)
        {
            return s.size() >= suffix.size() && s.substr (s.size() - suffix.size()) == suffix;
        }

        const MidiFile* findFile (std::span<const MidiFile> files, std::string_view name)
        {
            for (const auto& f : files)
                if (baseName (f.name) == name)
                    return &f;
            return nullptr;
        }

        std::string laneLabel (const engine::KitInfo& kit, std::size_t lane)
        {
            auto label = safeName (kit.laneLabels[lane]);
            return label.empty() ? std::string (kLaneNames[lane]) : label;
        }

        // "<KIT> <Title> <BPM> <Key> <Camelot> - <what>.mid", in safe characters.
        std::string fileName (const engine::KitInfo& kit, const std::string& what)
        {
            auto stem = safeName (kit.id + " " + kit.title + " " + std::to_string (kit.bpm) + " " + shortKey (kit.key) + " " + kit.camelot + " - " + what);
            if (stem.size() > kMaxStem)
                stem = safeName (std::string_view (stem).substr (0, kMaxStem));
            return stem + ".mid";
        }

        // A file name's words: "Phrase_EPianoStabRhythm_2bars" is "Phrase E Piano Stab Rhythm 2 bars".
        // Underscores, camelCase (and the end of a capitals run: EPiano), and a number before a word part.
        std::string words (std::string_view stem)
        {
            std::string out;
            for (std::size_t i = 0; i < stem.size(); ++i)
            {
                const char c = stem[i];
                if (c == '_')
                {
                    out += ' ';
                    continue;
                }
                if (i > 0)
                {
                    const char p = stem[i - 1];
                    if ((isLower (p) && isUpper (c)) || (isUpper (p) && isUpper (c) && i + 1 < stem.size() && isLower (stem[i + 1]))
                        || (isDigit (p) && (isLower (c) || isUpper (c))))
                        out += ' ';
                }
                out += c;
            }
            return out;
        }

        // One of the kit's files with its tracks.
        struct Source
        {
            const MidiFile* file = nullptr;
            std::string name;
            std::vector<midi::TrackChunk> tracks;
        };

        util::Result<Source> sourceOf (std::span<const MidiFile> files, std::string name)
        {
            using R = util::Result<Source>;
            const auto* file = findFile (files, name);
            if (file == nullptr)
                return R::failure ("the kit has no " + name);
            auto tracks = midi::trackChunks (file->bytes);
            if (!tracks.ok())
                return R::failure (name + ": " + tracks.error);
            return R::success ({ file, std::move (name), std::move (tracks.value) });
        }

        // The source's tracks after the conductor that play on lane.
        std::vector<int> laneTracks (const engine::KitInfo& kit, const Source& source, int lane)
        {
            std::vector<int> out;
            for (std::size_t i = 1; i < source.tracks.size(); ++i)
                if (engine::trackLane (kit, source.tracks[i].name) == lane)
                    out.push_back (static_cast<int> (i));
            return out;
        }

        // Those tracks' chunks, back to back.
        std::vector<std::uint8_t> chunkBytes (const Source& source, const std::vector<int>& tracks)
        {
            std::vector<std::uint8_t> out;
            for (const int t : tracks)
            {
                const auto& chunk = source.tracks[static_cast<std::size_t> (t)];
                const auto begin = source.file->bytes.begin() + static_cast<std::ptrdiff_t> (chunk.offset);
                out.insert (out.end(), begin, begin + static_cast<std::ptrdiff_t> (chunk.size));
            }
            return out;
        }

        DragResult cut (const Source& source, const std::vector<int>& tracks, std::string name)
        {
            auto bytes = midi::keepTracks (source.file->bytes, tracks);
            if (!bytes.ok())
                return DragResult::failure (source.name + ": " + bytes.error);
            return DragResult::success ({ std::move (name), std::move (bytes.value) });
        }

        // Whether 1 drum track puts a track on the merged track (ADR 0025): its name has the drums role in kTracks.
        bool isDrumTrack (std::string_view name)
        {
            for (const auto& t : engine::kTracks)
                if (t.name == name)
                    return t.role == engine::ChannelRole::drums;
            return false;
        }

        constexpr std::string_view kDrumTrackName = "Drums";

        // The kit's track of that name whose sample plays a whole chord (KitInfo::chordTracks), or none.
        const engine::KitChordTrack* chordTrack (const engine::KitInfo& kit, std::string_view name)
        {
            for (const auto& t : kit.chordTracks)
                if (t.name == name && ! t.zones.empty())
                    return &t;
            return nullptr;
        }

        midi::ChordSample chordSampleOf (const engine::KitChordTrack& track)
        {
            midi::ChordSample sample;
            for (const auto& z : track.zones)
                sample.push_back ({ z.lo, z.hi, z.intervals });
            return sample;
        }

        // What For my synths (ADR 0027) does with a kit's track, by its name: a chord sample's track (Chords, and the
        // kit's chord extras: KitInfo::chordTracks) is written as its chords; the bass lane's pitched tracks (Bass, and a
        // kit's extras on the bass lane) go up an octave; the other pitched tracks (lead, chord voicing, vox, and the
        // kit's other extras, multisampled instruments on channels 11-16) are made for synths; drums, one-key sounds
        // (accents, FX, vox chops), the merged Drums and a track no lane claims stay as they are.
        midi::SynthPart synthPart (const engine::KitInfo& kit, std::string_view name)
        {
            if (chordTrack (kit, name) != nullptr)
                return midi::SynthPart::chords;
            for (const auto& t : engine::kTracks)
                if (t.name == name)
                {
                    if (t.role == engine::ChannelRole::bass)
                        return midi::SynthPart::bass;
                    return engine::isOneKeyRole (t.role) || t.role == engine::ChannelRole::reserved ? midi::SynthPart::asIs : midi::SynthPart::pitched;
                }
            for (const auto& e : kit.extras)
                if (e.name == name)
                    return e.lane == engine::Lane::bass ? midi::SynthPart::bass : midi::SynthPart::pitched;
            return midi::SynthPart::asIs;
        }

        // Whether For my synths changes any of these tracks of the source.
        bool changesForSynths (const engine::KitInfo& kit, const Source& source, const std::vector<int>& tracks)
        {
            return std::any_of (tracks.begin(), tracks.end(), [&] (int i) {
                return synthPart (kit, source.tracks[static_cast<std::size_t> (i)].name) != midi::SynthPart::asIs;
            });
        }

        // A drag's file with its tracks made for synths (midi::forSynths), each by synthPart (), a chord sample's track
        // with its chords, the conductor as it is.
        DragResult madeForSynths (const engine::KitInfo& kit, DragResult made)
        {
            if (!made.ok())
                return made;
            auto& file = made.value;
            const auto tracks = midi::trackChunks (file.bytes);
            if (!tracks.ok())
                return DragResult::failure (file.name + ": " + tracks.error);
            std::vector<midi::SynthPart> parts;
            std::vector<midi::ChordSample> chords;
            for (std::size_t i = 0; i < tracks.value.size(); ++i)
            {
                parts.push_back (i == 0 ? midi::SynthPart::asIs : synthPart (kit, tracks.value[i].name));
                const auto* chord = parts.back() == midi::SynthPart::chords ? chordTrack (kit, tracks.value[i].name) : nullptr;
                chords.push_back (chord != nullptr ? chordSampleOf (*chord) : midi::ChordSample {});
            }
            auto bytes = midi::forSynths (file.bytes, parts, chords);
            if (!bytes.ok())
                return DragResult::failure (file.name + ": can't make it for synths: " + bytes.error);
            file.bytes = std::move (bytes.value);
            return made;
        }

        // Loop or Song: the vox switch's file less the muted lanes' tracks.
        DragResult wholeDrag (const engine::KitInfo& kit, std::span<const MidiFile> files, const DragState& state, bool song)
        {
            auto source = sourceOf (files, song ? songFileName (kit.id, state.variant) : loopFileName (kit.id, state.variant));
            if (!source.ok())
                return DragResult::failure (std::move (source.error));

            std::vector<int> kept;
            std::uint32_t left = 0; // the muted lanes whose tracks are left out
            for (std::size_t i = 1; i < source.value.tracks.size(); ++i)
            {
                const int lane = engine::trackLane (kit, source.value.tracks[i].name);
                const auto bit = lane >= 0 ? engine::laneBit (static_cast<engine::Lane> (lane)) : 0u;
                if ((state.mutedLanes & bit) != 0)
                    left |= bit;
                else
                    kept.push_back (static_cast<int> (i));
            }
            if (kept.empty())
                return DragResult::failure ("every lane with tracks in " + source.value.name + " is muted");

            // 1 drum track: the kept drum tracks, in file order, become one track where the first was.
            std::vector<int> drums;
            if (state.drumsOnOneTrack)
                for (const int i : kept)
                    if (isDrumTrack (source.value.tracks[static_cast<std::size_t> (i)].name))
                        drums.push_back (i);
            const bool merge = drums.size() >= 2;

            // For my synths: when a kept track is one it changes.
            const bool synths = state.forSynths && changesForSynths (kit, source.value, kept);

            std::string what = song ? "Song" : "Loop";
            if (state.variant == engine::Variant::novox)
                what += " NoVox";
            std::string marks = synths ? "synths" : "";
            if (merge)
                marks += marks.empty() ? "1 drum track" : ", 1 drum track";
            if (left != 0)
            {
                std::string labels;
                for (std::size_t lane = 0; lane < static_cast<std::size_t> (engine::kLanes); ++lane)
                    if ((left & (1u << lane)) != 0)
                        labels += (labels.empty() ? "" : ", ") + laneLabel (kit, lane);
                marks += (marks.empty() ? "no " : ", no ") + labels;
            }
            if (!marks.empty())
                what += " (" + marks + ")";
            auto made = [&] {
                if (!merge)
                    return cut (source.value, kept, fileName (kit, what));
                auto bytes = midi::mergeTracks (source.value.file->bytes, kept, drums, kDrumTrackName);
                if (!bytes.ok())
                    return DragResult::failure (source.value.name + ": can't put the drums on one track: " + bytes.error);
                return DragResult::success ({ fileName (kit, what), std::move (bytes.value) });
            }();
            if (synths)
                return madeForSynths (kit, std::move (made));
            return made;
        }

        // One lane's tracks from the mode's files: see the header.
        DragResult laneDrag (const engine::KitInfo& kit, std::span<const MidiFile> files, const DragState& state, engine::Lane lane)
        {
            const int index = static_cast<int> (lane);
            if (index < 0 || index >= engine::kLanes)
                return DragResult::failure ("lane " + std::to_string (index) + " isn't one of the " + std::to_string (engine::kLanes) + " lanes");

            const bool song = state.mode == engine::PlayMode::song;
            const auto fileOf = [&] (engine::Variant v) { return song ? songFileName (kit.id, v) : loopFileName (kit.id, v); };
            auto vox = sourceOf (files, fileOf (engine::Variant::vox));
            if (!vox.ok())
                return DragResult::failure (std::move (vox.error));
            auto novox = sourceOf (files, fileOf (engine::Variant::novox));
            if (!novox.ok())
                return DragResult::failure (std::move (novox.error));

            const auto label = laneLabel (kit, static_cast<std::size_t> (index));
            const auto inVox = laneTracks (kit, vox.value, index);
            const auto inNovox = laneTracks (kit, novox.value, index);
            if (inVox.empty() && inNovox.empty())
                return DragResult::failure (label + " has no tracks in " + vox.value.name + " or " + novox.value.name);

            const bool fromNovox = inVox.empty()
                                   || (state.variant == engine::Variant::novox && !inNovox.empty()
                                       && chunkBytes (novox.value, inNovox) != chunkBytes (vox.value, inVox));
            const auto& source = fromNovox ? novox.value : vox.value;
            const auto& tracks = fromNovox ? inNovox : inVox;
            const bool synths = state.forSynths && changesForSynths (kit, source, tracks);
            auto made = cut (source, tracks, fileName (kit, label + (song ? " Song" : "") + (fromNovox ? " NoVox" : "") + (synths ? " (synths)" : "")));
            if (synths)
                return madeForSynths (kit, std::move (made));
            return made;
        }

        // One of the kit's own files (named <KIT>_...) as it is, or made for synths.
        DragResult phraseDrag (const engine::KitInfo& kit, std::span<const MidiFile> files, const DragState& state, const juce::String& phraseFile)
        {
            const auto wanted = phraseFile.toStdString();
            const auto base = baseName (wanted);
            const auto* file = startsWith (base, kit.id + "_") ? findFile (files, base) : nullptr;
            if (file == nullptr)
                return DragResult::failure ("the kit has no phrase file '" + wanted + "'");
            const auto parsed = midi::readSmf (file->bytes);
            if (!parsed.ok())
                return DragResult::failure (std::string (base) + ": " + parsed.error);

            auto stem = base;
            if (endsWith (stem, ".mid"))
                stem.remove_suffix (4);
            if (startsWith (stem, kit.id + "_"))
                stem.remove_prefix (kit.id.size() + 1);
            bool synths = false; // For my synths: when a track after the conductor is one it changes
            for (std::size_t i = 1; state.forSynths && i < parsed.value.tracks.size(); ++i)
                synths = synths || synthPart (kit, midi::trackName (parsed.value.tracks[i])) != midi::SynthPart::asIs;
            auto made = DragResult::success ({ fileName (kit, words (stem) + (synths ? " (synths)" : "")), std::vector<std::uint8_t> (file->bytes.begin(), file->bytes.end()) });
            if (synths)
                return madeForSynths (kit, std::move (made));
            return made;
        }
    }

    std::string safeName (std::string_view text)
    {
        std::string out;
        for (const char c : text)
        {
            const char d = isSafe (c) ? c : ' ';
            if (d == ' ' && (out.empty() || out.back() == ' '))
                continue;
            out += d;
        }
        while (!out.empty() && out.back() == ' ')
            out.pop_back();
        return out;
    }

    std::string shortKey (std::string_view key)
    {
        const auto isNote = [] (std::string_view n) {
            return (n.size() == 1 || (n.size() == 2 && (n[1] == '#' || n[1] == 'b'))) && n[0] >= 'A' && n[0] <= 'G';
        };
        constexpr std::array<std::pair<std::string_view, std::string_view>, 2> modes { { { " minor", "m" }, { " major", "maj" } } };
        for (const auto& [mode, mark] : modes)
            if (endsWith (key, mode) && isNote (key.substr (0, key.size() - mode.size())))
                return std::string (key.substr (0, key.size() - mode.size())) + std::string (mark);
        return std::string (key);
    }

    std::string loopFileName (std::string_view kitId, engine::Variant variant)
    {
        return std::string (kitId) + (variant == engine::Variant::novox ? "_Loop_NoVox_4bars.mid" : "_Loop_Vox_4bars.mid");
    }

    std::string songFileName (std::string_view kitId, engine::Variant variant)
    {
        return std::string (kitId) + (variant == engine::Variant::novox ? "_ExtendedMix_NoVox_224bars.mid" : "_ExtendedMix_Vox_224bars.mid");
    }

    juce::StringArray phraseFiles (std::string_view kitId, std::span<const MidiFile> files)
    {
        const auto prefix = std::string (kitId) + "_Phrase_";
        std::vector<std::string> names;
        for (const auto& f : files)
        {
            const auto base = baseName (f.name);
            if (base.size() > prefix.size() && startsWith (base, prefix) && endsWith (base, ".mid"))
                names.emplace_back (base);
        }
        std::sort (names.begin(), names.end());
        names.erase (std::unique (names.begin(), names.end()), names.end());

        juce::StringArray out;
        for (const auto& name : names)
            out.add (juce::String::fromUTF8 (name.data(), static_cast<int> (name.size())));
        return out;
    }

    util::Result<DragFile> makeDragFile (const engine::KitInfo& kit, std::span<const MidiFile> files, const DragState& state,
        const DragRequest& request)
    {
        switch (request.what)
        {
            case DragWhat::loop: return wholeDrag (kit, files, state, false);
            case DragWhat::song: return wholeDrag (kit, files, state, true);
            case DragWhat::lane: return laneDrag (kit, files, state, request.lane);
            case DragWhat::phrase: return phraseDrag (kit, files, state, request.phraseFile);
        }
        return DragResult::failure ("an unknown kind of drag");
    }

    juce::File cacheFolder()
    {
#if JUCE_MAC || JUCE_IOS
        const auto caches = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Caches"); // ~/Library/Caches
#elif JUCE_WINDOWS
        const auto caches = juce::File::getSpecialLocation (juce::File::windowsLocalAppData);
#else
        const auto xdg = juce::SystemStats::getEnvironmentVariable ("XDG_CACHE_HOME", {});
        const auto caches = juce::File::isAbsolutePath (xdg) ? juce::File (xdg)
                                                             : juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile (".cache");
#endif
        return caches.getChildFile (JucePlugin_Manufacturer).getChildFile (JucePlugin_Name).getChildFile ("drag");
    }

    util::Result<juce::File> writeDragFile (const engine::KitInfo& kit, std::span<const MidiFile> files, const DragState& state,
        const DragRequest& request, const juce::File& folder)
    {
        using R = util::Result<juce::File>;
        auto made = makeDragFile (kit, files, state, request);
        if (!made.ok())
            return R::failure (std::move (made.error));

        if (const auto created = folder.createDirectory(); created.failed())
            return R::failure ("can't make the drag folder " + folder.getFullPathName().toStdString() + ": " + created.getErrorMessage().toStdString());
        const auto file = folder.getChildFile (juce::String::fromUTF8 (made.value.name.data(), static_cast<int> (made.value.name.size())));
        if (!file.replaceWithData (made.value.bytes.data(), made.value.bytes.size()))
            return R::failure ("can't write " + file.getFullPathName().toStdString());

        trimCache (folder, kKeepFiles, file);
        return R::success (file);
    }

    void trimCache (const juce::File& folder, int keep, const juce::File& keepFile)
    {
        struct Entry
        {
            juce::File file;
            juce::Time modified;
            juce::String name;
        };
        std::vector<Entry> entries;
        bool keepFileThere = false;
        for (const auto& f : folder.findChildFiles (juce::File::findFiles, false, "*.mid"))
        {
            if (f == keepFile)
                keepFileThere = true;
            else
                entries.push_back ({ f, f.getLastModificationTime(), f.getFileName() });
        }

        // Newest first; on a tie, by name, so a trim is the same every time.
        std::sort (entries.begin(), entries.end(), [] (const Entry& a, const Entry& b) {
            if (a.modified != b.modified)
                return a.modified > b.modified;
            return a.name < b.name;
        });
        const auto room = static_cast<std::size_t> (std::max (0, keep - (keepFileThere ? 1 : 0)));
        for (std::size_t i = room; i < entries.size(); ++i)
            entries[i].file.deleteFile();
    }

    juce::String externalDragPath (const juce::File& file)
    {
#if JUCE_LINUX || JUCE_BSD
        return juce::URL (file).toString (false); // "file:///.../F%23m%2011A.wav": JUCE passes a URL on as it is
#else
        return file.getFullPathName();
#endif
    }

    void revealToUser (const juce::File& file)
    {
#if JUCE_LINUX || JUCE_BSD
        // xdg-open with the folder as its one argument: no shell, so nothing in the name acts. It is started by a
        // middle child that only starts it, in a session of its own (as JUCE's Process::openDocument does), and exits:
        // this waits for that child alone, a moment, and init reaps xdg-open when it ends. So no opener stays the
        // host's child (a zombie until the host quits) and no process object outlives the call (a static one would
        // set off JUCE's leak detector at a Debug build's exit). The arguments are made before the fork; the children
        // call only what is safe after a fork in a process with threads, and never exit (), which would run the
        // host's atexit handlers and static destructors in them. xdg-open's input and output are /dev/null.
        const auto folder = file.isDirectory() ? file : file.getParentDirectory();
        const auto path = folder.getFullPathName().toStdString();
        char program[] = "xdg-open";
        char* const argv[] { program, const_cast<char*> (path.c_str()), nullptr };

        const pid_t middle = fork();
        if (middle == 0)
        {
            setsid();
            if (fork() == 0)
            {
                if (const int devNull = open ("/dev/null", O_RDWR); devNull >= 0)
                {
                    dup2 (devNull, STDIN_FILENO);
                    dup2 (devNull, STDOUT_FILENO);
                    dup2 (devNull, STDERR_FILENO);
                    if (devNull > STDERR_FILENO)
                        close (devNull);
                }
                execvp (argv[0], argv);
                _exit (127);
            }
            _exit (0);
        }
        if (middle > 0)
            while (waitpid (middle, nullptr, 0) < 0 && errno == EINTR)
            {
            }
#else
        file.revealToUser();
#endif
    }
}
