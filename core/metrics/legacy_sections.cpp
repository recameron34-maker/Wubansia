// The section kind of every marker name the legacy full songs and extended mixes use, from frozen
// legacy/web-pack/core.js: a copy of tools/legacy_audit/legacy_sections.json (made by its
// section_table.js), sorted by name in byte order. core.js keeps the kind out of its files, so the
// metrics need this table to read legacy markers. test_metrics_model.cpp checks it against the JSON
// file and against legacy_expected.json's section_kinds, entry by entry.

#include "metrics/model.h"

#include <algorithm>
#include <array>

namespace wubansia::metrics
{
    namespace
    {
        // clang-format off
        constexpr std::array<LegacySection, 105> table = { {
            { "2-step switch", "switch" },
            { "4x4 switch", "switch" },
            { "Acid", "switch" },
            { "Acid build", "build" },
            { "Bass + drums", "switch" },
            { "Bass first", "groove" },
            { "Bass groove", "groove" },
            { "Bass in", "groove" },
            { "Bass solo", "build" },
            { "Bass solo (B line)", "switch" },
            { "Bassline", "groove" },
            { "Beads", "groove" },
            { "Break", "break" },
            { "Breakdown", "break" },
            { "Breakdown (choir)", "break" },
            { "Breakdown (chords)", "break" },
            { "Breakdown (organ)", "break" },
            { "Breakdown (strings)", "break" },
            { "Bridge", "break" },
            { "Build", "build" },
            { "Build 2", "build" },
            { "Build 2 (jungle)", "build" },
            { "Chords", "groove" },
            { "Chords in", "groove" },
            { "Chorus", "drop" },
            { "Chorus (vocal)", "drop" },
            { "Chorus 2", "drop" },
            { "Chorus 2 (vocal)", "drop" },
            { "Dip (no kick)", "break" },
            { "Disco switch", "switch" },
            { "Drop", "drop" },
            { "Drop (diamonds)", "drop" },
            { "Drop (reharmonised)", "drop" },
            { "Drop 1", "drop" },
            { "Drop 1 (come again)", "drop" },
            { "Drop 1 (vocal)", "drop" },
            { "Drop 2", "drop" },
            { "Drop 2 (big bass)", "drop" },
            { "Drop 2 (both hooks)", "drop" },
            { "Drop 2 (breaks)", "drop" },
            { "Drop 2 (donk)", "drop" },
            { "Drop 2 (reharmonised)", "drop" },
            { "Drop 2 (rewind)", "drop" },
            { "Drop 2 (vocal)", "drop" },
            { "Dub break", "switch" },
            { "Fifth bass", "groove" },
            { "Fifths lead", "groove" },
            { "Filter switch", "switch" },
            { "Final chorus (both hooks)", "drop" },
            { "Final chorus (key lift +2)", "drop" },
            { "Final drop", "drop" },
            { "Final switch", "switch" },
            { "Ghetto switch", "switch" },
            { "Gospel breakdown", "break" },
            { "Groove", "groove" },
            { "Groove (2-step)", "groove" },
            { "Groove (organ)", "groove" },
            { "Half-time switch", "switch" },
            { "Hook (brat)", "groove" },
            { "Intro", "intro" },
            { "Intro (DJ)", "intro" },
            { "Intro (bongos)", "intro" },
            { "Intro (breaks)", "intro" },
            { "Intro (filtered kick)", "intro" },
            { "Intro (filtered)", "intro" },
            { "Intro (hats first)", "intro" },
            { "Intro (strings)", "intro" },
            { "Intro 2", "groove" },
            { "Key change chorus (+1)", "drop" },
            { "Kick + bass", "switch" },
            { "Kick + bass switch", "switch" },
            { "Lead", "drop" },
            { "Main", "groove" },
            { "Micro-edits", "switch" },
            { "Octave bass", "groove" },
            { "One-note lead", "groove" },
            { "Outro", "outro" },
            { "Outro (DJ)", "outro" },
            { "Outro (filtered)", "outro" },
            { "Peak", "drop" },
            { "Peak (hoover)", "drop" },
            { "Piano", "groove" },
            { "Piano breakdown", "break" },
            { "Piano build", "build" },
            { "Piano intro", "groove" },
            { "Plucks", "groove" },
            { "Post-hook (brat)", "switch" },
            { "Pre-chorus", "build" },
            { "Rave stabs", "groove" },
            { "Reese switch", "switch" },
            { "Roller", "drop" },
            { "Siren switch", "switch" },
            { "String break", "break" },
            { "String hook", "drop" },
            { "Strings enter", "groove" },
            { "Stutter chords", "groove" },
            { "Switch", "switch" },
            { "Switch (bass lick)", "switch" },
            { "Talking bass", "groove" },
            { "Tom switch", "switch" },
            { "Toms", "switch" },
            { "Verse", "groove" },
            { "Verse (vocal)", "groove" },
            { "Whisper", "groove" },
            { "Wub switch", "switch" },
        } };
        // clang-format on
    }

    std::span<const LegacySection> legacySections() noexcept { return table; }

    std::string_view legacySectionKind (std::string_view markerText) noexcept
    {
        const auto it = std::lower_bound (table.begin(), table.end(), markerText, [] (const LegacySection& s, std::string_view t) { return s.text < t; });
        return it != table.end() && it->text == markerText ? it->kind : std::string_view {};
    }
}
