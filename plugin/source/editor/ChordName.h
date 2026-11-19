#pragma once

// Chord and note names for the looks' displays (ADR 0032): the chord a kit's voicing plays, named from its lowest
// key, and notes with C4 = 60 in the key's spelling (ADR 0024). Never uppercased by a look: "m" against "M" is
// minor against major.

#include <juce_core/juce_core.h>

#include <cstdint>
#include <span>

namespace wubansia::editor
{
    // m9, maj9, 9, m(add9), add9, m7, maj7, 7, m, major, sus2, sus4, m11, 5 on the lowest key; else the first other
    // key's pitch that names one, as a slash chord ("C/E"); else the lowest note's name alone. "" for no keys.
    juce::String chordName (std::span<const std::uint8_t> keys, bool flats);

    juce::String noteName (int key, bool flats); // C4 = 60: "F#4", "Bb3"
    juce::String pitchName (int key, bool flats); // without the octave: "F#", "Bb"
    bool keyUsesFlats (const juce::String& key); // "D minor": true; "F# minor": false
    juce::String shortTempo (double bpm); // "134", "133.5"
}
