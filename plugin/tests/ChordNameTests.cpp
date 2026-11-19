// Chord and note names for the looks' displays (ADR 0032).

#include "editor/ChordName.h"

#include <catch2/catch_test_macros.hpp>

#include <vector>

using namespace wubansia::editor;

namespace
{
    juce::String name (std::vector<std::uint8_t> keys, bool flats = false) { return chordName (keys, flats); }
}

TEST_CASE ("chords are named from their lowest key", "[looks]")
{
    CHECK (name ({ 54, 57, 61, 64, 68 }) == "F#m9"); // FUT01's
    CHECK (name ({ 47, 50, 54, 57, 61 }) == "Bm9");
    CHECK (name ({ 50, 53, 57, 60, 64 }, true) == "Dm9"); // FUT02's
    CHECK (name ({ 55, 58, 62, 65, 69 }, true) == "Gm9"); // FUT03's
    CHECK (name ({ 48, 51, 55, 58, 62 }, true) == "Cm9");
    CHECK (name ({ 52, 55, 59, 66 }) == "Em(add9)"); // FUT04's
    CHECK (name ({ 52, 55, 59, 66, 64 }) == "Em(add9)"); // doubled keys don't matter

    CHECK (name ({ 60, 64, 67 }) == "C");
    CHECK (name ({ 57, 60, 64, 67 }) == "Am7");
    CHECK (name ({ 64, 67, 72 }) == "C/E");
    CHECK (name ({ 58, 61, 65, 68, 72 }, true) == "Bbm9");
    CHECK (name ({ 60, 64, 67, 71 }) == "Cmaj7");
    CHECK (name ({ 60, 64, 67, 70 }) == "C7");
    CHECK (name ({ 60, 62, 67 }) == "Csus2");
    CHECK (name ({ 60, 65, 67 }) == "Csus4");
    CHECK (name ({ 60, 67 }) == "C5");
    CHECK (name ({ 60, 61 }) == "C"); // no chord: the lowest note's name
    CHECK (name ({}) == "");
}

TEST_CASE ("note names are C4 = 60, in the key's spelling", "[looks]")
{
    CHECK (noteName (60, false) == "C4");
    CHECK (noteName (61, false) == "C#4");
    CHECK (noteName (61, true) == "Db4");
    CHECK (noteName (69, false) == "A4");
    CHECK (noteName (58, true) == "Bb3");
    CHECK (noteName (0, false) == "C-1");
    CHECK (pitchName (66, false) == "F#");

    CHECK (keyUsesFlats ("D minor"));
    CHECK (keyUsesFlats ("G minor"));
    CHECK (keyUsesFlats ("Bb major"));
    CHECK (keyUsesFlats ("F major"));
    CHECK_FALSE (keyUsesFlats ("F# minor"));
    CHECK_FALSE (keyUsesFlats ("E minor"));
    CHECK_FALSE (keyUsesFlats ("A minor"));
    CHECK_FALSE (keyUsesFlats (""));

    CHECK (shortTempo (134.0) == "134");
    CHECK (shortTempo (133.5) == "133.5");
    CHECK (shortTempo (127.96) == "128");
    CHECK (shortTempo (200.0) == "200");
}
