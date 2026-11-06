#pragma once

// From SMF bytes to what the metrics measure: sections, lanes with roles, and drum notes with
// groups. tools/legacy_audit/README.md ("Reading the files", "The model") defines all of it; this is
// the C++ port of tools/legacy_audit/model.js, rule for rule, over core/midi's reader. The metrics
// read only MIDI bytes, so the same code scores the legacy baseline and generator v2.
//
// Integer arithmetic only.

#include "midi/smf.h"
#include "util/ints.h"
#include "util/result.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace wubansia::metrics
{
    // 480 ticks per quarter note and 4/4 throughout: the audit reads nothing else.
    inline constexpr std::int64_t ppq = 480;
    inline constexpr std::int64_t stepTicks = 120; // a 16th
    inline constexpr std::int64_t beatTicks = 480;
    inline constexpr std::int64_t barTicks = 1920;

    // Step attribution: the nearest 16th, a tie (60 ticks) going to the later step. A note pulled up
    // to 59 ticks early by a feel pass keeps its own step, and bar floorDiv (step, 16).
    constexpr std::int64_t step16 (std::int64_t tick) noexcept { return util::floorDiv (tick + 60, stepTicks); }

    // A track's role: drums by channel, the others by the first word of the track name.
    enum class Role : std::uint8_t {
        drums,
        bass,
        chords,
        lead,
        vox,
        fx,
        other,
        ignore // the legacy ghost-kick "Sidechain" track
    };
    std::string_view roleName (Role role) noexcept; // "drums", "bass"...

    // A drum note's group, from its GM key alone (README, "Drum groups").
    enum class Group : std::uint8_t {
        kick,
        snare,
        clap,
        rim,
        hats, // closed, pedal and open hats, rides, tambourine, cabasa, maracas, shaker
        cymbal,
        tom,
        perc, // every other key
        none // not a drum note
    };
    std::string_view groupName (Group group) noexcept; // "kick", "snare"...
    Group drumGroup (int key) noexcept;

    struct Note
    {
        std::int64_t tick = 0;
        std::int64_t length = 0;
        std::uint8_t channel = 0;
        std::uint8_t key = 0;
        std::uint8_t velocity = 0;
        std::int32_t track = 0; // the track's index in the file, conductor included
        Group group = Group::none; // drum notes only
    };

    struct Section
    {
        std::int64_t tick = 0; // its marker's tick
        std::int64_t end = 0; // the next marker's tick, or the file's end
        std::string text; // the marker text
        std::string kind; // sectionKind (text); "none" for the one section of a file without markers
        std::int64_t bar = 0; // floorDiv (tick, 1920)
        std::int64_t bars = 0; // ceilDiv (end - tick, 1920)
    };

    struct Track
    {
        std::int32_t index = 0; // in the file, conductor included
        std::string name; // its first FF 03 event, or ""
        Role role = Role::other;
        std::vector<Note> notes; // core/midi's notesOf (): by (tick, channel, key), stable
    };

    struct Song
    {
        std::string path; // "<KIT>/<KIT>_<slot>.mid" for the legacy baseline; any name otherwise
        std::string kit; // from a legacy path, else ""
        std::string slot; // "Bass-A", "FullSong"... from a legacy path, else ""
        std::uint16_t format = 1;
        std::int64_t endTick = 0; // the largest end-of-track tick of its tracks
        std::int64_t bars = 0; // ceilDiv (endTick, 1920)
        std::vector<midi::KeySignature> keySignatures; // by tick
        std::vector<Section> sections; // one per marker, empty ones dropped; empty without markers
        std::vector<Track> tracks; // the tracks that have notes, in file order
        std::vector<Note> drumNotes; // every note of every drum track, by (tick, track, key), stable

        std::int64_t noteCount() const noexcept;
    };

    // A marker's section kind (README, "Sections"): generator v2 markers start with a lower-case
    // kind word ("drop2 four to the floor"); a legacy marker is looked up whole in the legacy table;
    // anything else is "unknown".
    std::string sectionKind (std::string_view markerText);

    // The kind core.js gives a legacy marker name ("Breakdown (organ)": "break"), or "" if the name
    // isn't one of the 105 the legacy full songs and extended mixes use.
    std::string_view legacySectionKind (std::string_view markerText) noexcept;

    struct LegacySection
    {
        std::string_view text;
        std::string_view kind;
    };
    // The whole legacy table (tools/legacy_audit/legacy_sections.json), sorted by text in byte order.
    std::span<const LegacySection> legacySections() noexcept;

    // A track's role: "ignore" if the name's first word is "sidechain" (any case), drums if all its
    // notes are on channel 10, the part file's slot for a legacy part file (Drums-*, Bass-*,
    // Chords-*, Melody-*), and otherwise the first word of the name (README, "Lanes and roles").
    Role roleOf (std::string_view trackName, bool allOnChannel10, std::string_view slot);

    // The first word of a track name, lower-cased: its leading ASCII letters ("Riser (builds)": "riser").
    std::string firstWord (std::string_view name);

    // The kit and slot of a legacy baseline path, "<KIT>/<KIT>_<slot>.mid" (KIT is capital letters
    // then digits, the same twice). False, and both empty, for any other path.
    bool splitLegacyPath (std::string_view path, std::string& kit, std::string& slot);

    // Reads a file into the model. Refuses what core/midi's reader refuses, and anything but 480
    // ticks per quarter note, with a message that starts with the path.
    [[nodiscard]] util::Result<Song> loadSong (std::span<const std::uint8_t> bytes, std::string path);

    // loadSong () on a file's bytes; path is the name the song is reported under.
    [[nodiscard]] util::Result<Song> loadSongFile (const std::filesystem::path& file, std::string path);

    // The sections the metrics walk: the file's own, or for a file without markers one section of
    // kind "none" over all its bars.
    std::vector<Section> sectionsOf (const Song& song);

    // The first section of a kind, or nullptr (the file's own sections only).
    const Section* firstSectionOf (const Song& song, std::string_view kind) noexcept;

    std::vector<const Track*> tracksWithRole (const Song& song, Role role);

    // The notes of every track of a role, merged by (tick, track, key), stable.
    std::vector<Note> notesWithRole (const Song& song, Role role);

    // The metric groups (README, "Metric groups").
    inline constexpr std::array<Group, 1> hatsGroups = { Group::hats };
    inline constexpr std::array<Group, 3> percGroups = { Group::perc, Group::tom, Group::rim };
    inline constexpr std::array<Group, 2> snareFamily = { Group::snare, Group::clap };

    // The song's drum notes in these groups, in drumNotes order.
    std::vector<Note> drumNotesIn (const Song& song, std::span<const Group> groups);
    std::vector<Note> drumNotesIn (const Song& song, Group group);
}
