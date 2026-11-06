#pragma once

// Pitch, key and mode arithmetic for the generator. Integers only; pitch classes count from C = 0.
//
// Every kit is in a minor key, written with its natural-minor (Aeolian) key signature. A song picks
// a mode on the kit's tonic (Aeolian or Dorian; a break may take Phrygian colour), and chords are
// built from the mode's scale by degree, so a note is "in key" when its pitch class is in the kit's
// natural minor scale. Dorian's raised 6th and Phrygian's flat 2nd are the only notes the modes add,
// and the generator tags them borrowed (plan §4.1: "every note is in key unless tagged borrowed or
// planed").

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace wubansia::gen
{
    enum class Mode : std::uint8_t {
        aeolian, // natural minor: 1 2 b3 4 5 b6 b7
        dorian, // 1 2 b3 4 5 6 b7
        phrygian // 1 b2 b3 4 5 b6 b7
    };

    inline constexpr std::size_t modeCount = 3;

    // "aeolian", "dorian", "phrygian".
    std::string_view modeName (Mode mode) noexcept;
    std::optional<Mode> modeFromName (std::string_view name) noexcept;

    // The mode's scale as semitones above its tonic, degree 0-6.
    std::array<int, 7> modeSteps (Mode mode) noexcept;

    // The pitch class (0-11) of scale degree `degree` (any integer: 7 is the octave of 0) of the mode
    // on tonic pitch class tonicPc.
    int degreePc (int tonicPc, Mode mode, int degree) noexcept;

    // Semitones from the degree's root up to a chord tone `steps` scale steps above it, within the
    // mode: chordTone (Aeolian, 0, 2) == 3 (the minor third of i), chordTone (Aeolian, 5, 2) == 4.
    int chordInterval (Mode mode, int degree, int steps) noexcept;

    // True if pitch class pc is in the natural minor scale on tonicPc (the kit's key signature).
    bool inMinorKey (int tonicPc, int pc) noexcept;

    // The key's name as the kits write it: "F# minor", "Bb minor", "D major". Spelled as the
    // legacy pack spells keys (legacy/web-pack/core.js:18-29, the same table midi::sharpsFor uses).
    std::string keyName (int tonicPc, bool minor);

    // The Camelot code: F# minor "11A", D minor "7A", G minor "6A", E minor "9A", C major "8B".
    std::string camelot (int tonicPc, bool minor);

    // A MIDI note's pitch class.
    constexpr int pitchClass (int note) noexcept { return ((note % 12) + 12) % 12; }
}
