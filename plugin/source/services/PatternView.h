#pragma once

// The play mode's notes for the looks' displays and step grids (ADR 0032): the kit's own Loop or Song file, read from
// the embedded MIDI on the message thread (no file I/O), by lane in 16ths. The Chord voicing track (channel 3, which
// Wubansia doesn't play) is kept apart and named, for the chord at the playhead.

#include "Controller.h"
#include "services/KitCatalog.h"

#include <juce_core/juce_core.h>

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace wubansia
{
    // One note of a kit's Loop or Song file in 16ths.
    struct PatternNote
    {
        std::int32_t step = 0; // its note-on: (tick * 4 + ppq / 2) / ppq from the file's start
        std::int16_t length = 1; // in 16ths, rounded the same way, at least 1
        std::uint8_t key = 0;
        std::uint8_t velocity = 0; // 1-127

        std::int32_t end() const noexcept { return step + length; }
    };
    static_assert (sizeof (PatternNote) == 8);

    struct ChordChange
    {
        std::int32_t step = 0; // the onset 16th
        std::int32_t end = 0; // the 16th after the onset's last note ends
        juce::String name;
    };

    struct PatternView
    {
        juce::String kit;
        engine::PlayMode mode = engine::PlayMode::loop; // loop or song; Off asks for the loop
        engine::Variant variant = engine::Variant::vox;
        std::int32_t steps = 0; // 64 or 3584; 0: no file
        std::array<std::vector<PatternNote>, engine::kLanes> lanes; // by step then key; the voicing left out
        std::vector<PatternNote> voicing; // the Chord voicing track
        std::vector<ChordChange> chords; // the voicing's onsets, named, by step
        bool flats = false; // the kit's key signature has flats

        // A lane's notes sounding in [from, to): onsets in it and notes held into it.
        std::vector<PatternNote> notesIn (engine::Lane, std::int32_t from, std::int32_t to) const;
        // The latest chord change at or before step, while step < max (its end, its step + 33); "" when none.
        juce::String chordAt (std::int32_t step) const;
    };

    // The view of the kit's file for mode (Off: the Loop) and variant, from its embedded MIDI. Message thread; no file
    // I/O. An empty view (steps 0) for a kit without the file or a file that doesn't read.
    std::shared_ptr<const PatternView> patternOf (const KitCatalog::Entry&, engine::PlayMode, engine::Variant);
}
