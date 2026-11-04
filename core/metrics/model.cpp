#include "metrics/model.h"

#include "midi/reader.h"
#include "util/file_io.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <tuple>
#include <utility>

namespace wubansia::metrics
{
    namespace
    {
        // Generator v2's section kinds (README, "Sections"), the first word of its markers.
        constexpr std::array<std::string_view, 7> newKinds = { "intro", "groove", "build", "drop", "drop2", "break", "outro" };

        struct RoleWord
        {
            std::string_view word;
            Role role;
        };
        // README, "Lanes and roles".
        constexpr std::array<RoleWord, 31> roleWords = { {
            { "bass", Role::bass },
            { "sub", Role::bass },
            { "reese", Role::bass },
            { "acid", Role::bass },
            { "donk", Role::bass },
            { "chords", Role::chords },
            { "stabs", Role::chords },
            { "stab", Role::chords },
            { "pad", Role::chords },
            { "organ", Role::chords },
            { "strings", Role::chords },
            { "piano", Role::chords },
            { "brass", Role::chords },
            { "orchestra", Role::chords },
            { "choir", Role::chords },
            { "melody", Role::lead },
            { "lead", Role::lead },
            { "pluck", Role::lead },
            { "arp", Role::lead },
            { "counter", Role::lead },
            { "talkbox", Role::lead },
            { "hoover", Role::lead },
            { "vocal", Role::vox },
            { "vox", Role::vox },
            { "chops", Role::vox },
            { "shouts", Role::vox },
            { "riser", Role::fx },
            { "fx", Role::fx },
            { "impact", Role::fx },
            { "impacts", Role::fx },
            { "dub", Role::fx },
        } };

        // The legacy part files' slots (Drums-Main, Bass-A...) give their single track's role.
        constexpr std::array<RoleWord, 4> slotRoles = { { { "Drums", Role::drums }, { "Bass", Role::bass }, { "Chords", Role::chords }, { "Melody", Role::lead } } };

        bool isUpper (char c) noexcept { return c >= 'A' && c <= 'Z'; }
        bool isDigit (char c) noexcept { return c >= '0' && c <= '9'; }

        // What a JavaScript regex's "." doesn't match: the line terminators \n, \r, U+2028, U+2029.
        bool hasLineTerminator (std::string_view s) noexcept
        {
            return s.find ('\n') != std::string_view::npos || s.find ('\r') != std::string_view::npos
                   || s.find ("\xE2\x80\xA8") != std::string_view::npos || s.find ("\xE2\x80\xA9") != std::string_view::npos;
        }
    }

    std::string_view roleName (Role role) noexcept
    {
        switch (role)
        {
            case Role::drums:
                return "drums";
            case Role::bass:
                return "bass";
            case Role::chords:
                return "chords";
            case Role::lead:
                return "lead";
            case Role::vox:
                return "vox";
            case Role::fx:
                return "fx";
            case Role::other:
                return "other";
            case Role::ignore:
                return "ignore";
        }
        return "other";
    }

    std::string_view groupName (Group group) noexcept
    {
        switch (group)
        {
            case Group::kick:
                return "kick";
            case Group::snare:
                return "snare";
            case Group::clap:
                return "clap";
            case Group::rim:
                return "rim";
            case Group::hats:
                return "hats";
            case Group::cymbal:
                return "cymbal";
            case Group::tom:
                return "tom";
            case Group::perc:
                return "perc";
            case Group::none:
                return "none";
        }
        return "none";
    }

    Group drumGroup (int key) noexcept
    {
        switch (key)
        {
            case 35:
            case 36:
                return Group::kick;
            case 38:
            case 40:
                return Group::snare;
            case 39:
                return Group::clap;
            case 37:
                return Group::rim;
            case 42: // closed, pedal, open
            case 44:
            case 46:
            case 51: // rides
            case 53:
            case 59:
            case 54: // tambourine, cabasa, maracas, shaker
            case 69:
            case 70:
            case 82:
                return Group::hats;
            case 49:
            case 52:
            case 55:
            case 57:
                return Group::cymbal;
            case 41:
            case 43:
            case 45:
            case 47:
            case 48:
            case 50:
                return Group::tom;
            default:
                return Group::perc;
        }
    }

    std::int64_t Song::noteCount() const noexcept
    {
        std::int64_t n = 0;
        for (const auto& t : tracks)
            n += static_cast<std::int64_t> (t.notes.size());
        return n;
    }

    std::string sectionKind (std::string_view markerText)
    {
        const std::string_view first = markerText.substr (0, markerText.find (' ')); // text.split (' ')[0]
        for (const auto k : newKinds)
            if (first == k)
                return std::string (k);
        const std::string_view legacy = legacySectionKind (markerText);
        return legacy.empty() ? std::string ("unknown") : std::string (legacy);
    }

    std::string firstWord (std::string_view name)
    {
        std::string out;
        for (const char c : name)
        {
            if (c >= 'A' && c <= 'Z')
                out += static_cast<char> (c - 'A' + 'a');
            else if (c >= 'a' && c <= 'z')
                out += c;
            else
                break;
        }
        return out;
    }

    Role roleOf (std::string_view trackName, bool allOnChannel10, std::string_view slot)
    {
        const std::string word = firstWord (trackName);
        if (word == "sidechain")
            return Role::ignore;
        if (allOnChannel10)
            return Role::drums;
        if (!slot.empty() && slot != "FullArrangement" && slot != "FullSong" && slot != "ExtendedMix")
        {
            const std::string_view prefix = slot.substr (0, slot.find ('-')); // slot.split ('-')[0]
            for (const auto& s : slotRoles)
                if (prefix == s.word)
                    return s.role;
        }
        for (const auto& w : roleWords)
            if (word == w.word)
                return w.role;
        return Role::other;
    }

    bool splitLegacyPath (std::string_view path, std::string& kit, std::string& slot)
    {
        // /^([A-Z]+[0-9]+)\/\1_(.+)\.mid$/
        kit.clear();
        slot.clear();
        const std::size_t slash = path.find ('/');
        if (slash == std::string_view::npos || slash == 0)
            return false;
        const std::string_view k = path.substr (0, slash);
        std::size_t i = 0;
        while (i < k.size() && isUpper (k[i]))
            ++i;
        const std::size_t letters = i;
        while (i < k.size() && isDigit (k[i]))
            ++i;
        if (letters == 0 || i == letters || i != k.size())
            return false;
        const std::string_view rest = path.substr (slash + 1);
        constexpr std::string_view ext = ".mid";
        if (rest.size() < k.size() + 1 + 1 + ext.size() || rest.substr (0, k.size()) != k || rest[k.size()] != '_' || !rest.ends_with (ext))
            return false;
        const std::string_view s = rest.substr (k.size() + 1, rest.size() - k.size() - 1 - ext.size());
        if (s.empty() || hasLineTerminator (s))
            return false;
        kit = std::string (k);
        slot = std::string (s);
        return true;
    }

    util::Result<Song> loadSong (std::span<const std::uint8_t> bytes, std::string path)
    {
        using R = util::Result<Song>;
        auto file = midi::readSmf (bytes);
        if (!file.ok())
            return R::failure (path + ": " + file.error);
        if (file.value.ppq != ppq)
            return R::failure (path + ": " + std::to_string (file.value.ppq) + " ticks per quarter note; the metrics read 480 only");

        Song song;
        song.path = std::move (path);
        splitLegacyPath (song.path, song.kit, song.slot);
        song.format = file.value.format;
        for (const auto& t : file.value.tracks)
            song.endTick = std::max (song.endTick, t.endTick);
        song.bars = util::ceilDiv (song.endTick, barTicks);
        song.keySignatures = midi::keySignaturesOf (file.value);

        const auto markers = midi::markersOf (file.value);
        for (std::size_t i = 0; i < markers.size(); ++i)
        {
            Section s;
            s.tick = markers[i].tick;
            s.end = i + 1 < markers.size() ? markers[i + 1].tick : song.endTick;
            if (s.end <= s.tick)
                continue; // an empty section: two markers on one tick, or a marker at the end
            s.text = markers[i].text;
            s.kind = sectionKind (s.text);
            s.bar = util::floorDiv (s.tick, barTicks);
            s.bars = util::ceilDiv (s.end - s.tick, barTicks);
            song.sections.push_back (std::move (s));
        }

        for (std::size_t index = 0; index < file.value.tracks.size(); ++index)
        {
            const auto& t = file.value.tracks[index];
            const auto notes = midi::notesOf (t);
            if (notes.empty())
                continue;
            Track track;
            track.index = static_cast<std::int32_t> (index);
            track.name = midi::trackName (t);
            const bool allOn10 = std::all_of (notes.begin(), notes.end(), [] (const midi::Note& n) { return n.channel == 9; });
            track.role = roleOf (track.name, allOn10, song.slot);
            track.notes.reserve (notes.size());
            for (const auto& n : notes)
            {
                Note m;
                m.tick = n.tick;
                m.length = n.length;
                m.channel = n.channel;
                m.key = n.key;
                m.velocity = n.velocity;
                m.track = track.index;
                m.group = track.role == Role::drums ? drumGroup (n.key) : Group::none;
                track.notes.push_back (m);
            }
            song.tracks.push_back (std::move (track));
        }

        for (const auto& t : song.tracks)
            if (t.role == Role::drums)
                song.drumNotes.insert (song.drumNotes.end(), t.notes.begin(), t.notes.end());
        std::stable_sort (song.drumNotes.begin(), song.drumNotes.end(), [] (const Note& a, const Note& b) {
            return std::tie (a.tick, a.track, a.key) < std::tie (b.tick, b.track, b.key);
        });
        return R::success (std::move (song));
    }

    util::Result<Song> loadSongFile (const std::filesystem::path& file, std::string path)
    {
        const auto bytes = util::readBinaryFile (file);
        if (!bytes.ok())
            return util::Result<Song>::failure (bytes.error);
        return loadSong (bytes.value, std::move (path));
    }

    std::vector<Section> sectionsOf (const Song& song)
    {
        if (!song.sections.empty())
            return song.sections;
        Section whole;
        whole.end = song.bars * barTicks;
        whole.kind = "none";
        whole.bars = song.bars;
        return { whole };
    }

    const Section* firstSectionOf (const Song& song, std::string_view kind) noexcept
    {
        for (const auto& s : song.sections)
            if (s.kind == kind)
                return &s;
        return nullptr;
    }

    std::vector<const Track*> tracksWithRole (const Song& song, Role role)
    {
        std::vector<const Track*> out;
        for (const auto& t : song.tracks)
            if (t.role == role)
                out.push_back (&t);
        return out;
    }

    std::vector<Note> notesWithRole (const Song& song, Role role)
    {
        std::vector<Note> out;
        for (const auto& t : song.tracks)
            if (t.role == role)
                out.insert (out.end(), t.notes.begin(), t.notes.end());
        std::stable_sort (out.begin(), out.end(), [] (const Note& a, const Note& b) { return std::tie (a.tick, a.track, a.key) < std::tie (b.tick, b.track, b.key); });
        return out;
    }

    std::vector<Note> drumNotesIn (const Song& song, std::span<const Group> groups)
    {
        std::vector<Note> out;
        for (const auto& n : song.drumNotes)
            if (std::find (groups.begin(), groups.end(), n.group) != groups.end())
                out.push_back (n);
        return out;
    }

    std::vector<Note> drumNotesIn (const Song& song, Group group)
    {
        return drumNotesIn (song, std::span<const Group> (&group, 1));
    }
}
