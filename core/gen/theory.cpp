#include "gen/theory.h"

#include "util/ints.h"

#include <array>

namespace wubansia::gen
{
    namespace
    {
        // Spelled as the legacy pack spells keys (the table midi::sharpsFor follows).
        constexpr std::array<std::string_view, 12> minorNames = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "G#", "A", "Bb", "B" };
        constexpr std::array<std::string_view, 12> majorNames = { "C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
        // The Camelot number of each minor key's tonic: A flat minor is 1A, E flat minor 2A ... (fifths).
        constexpr std::array<int, 12> camelotMinor = { 5, 12, 7, 2, 9, 4, 11, 6, 1, 8, 3, 10 };

        int absoluteStep (Mode mode, int degree) noexcept
        {
            const auto steps = modeSteps (mode);
            return 12 * util::floorDiv (degree, 7) + steps[static_cast<std::size_t> (util::floorMod (degree, 7))];
        }
    }

    std::string_view modeName (Mode mode) noexcept
    {
        switch (mode)
        {
            case Mode::aeolian:
                return "aeolian";
            case Mode::dorian:
                return "dorian";
            case Mode::phrygian:
                return "phrygian";
        }
        return "aeolian";
    }

    std::optional<Mode> modeFromName (std::string_view name) noexcept
    {
        if (name == "aeolian")
            return Mode::aeolian;
        if (name == "dorian")
            return Mode::dorian;
        if (name == "phrygian")
            return Mode::phrygian;
        return std::nullopt;
    }

    std::array<int, 7> modeSteps (Mode mode) noexcept
    {
        switch (mode)
        {
            case Mode::aeolian:
                return { 0, 2, 3, 5, 7, 8, 10 };
            case Mode::dorian:
                return { 0, 2, 3, 5, 7, 9, 10 };
            case Mode::phrygian:
                return { 0, 1, 3, 5, 7, 8, 10 };
        }
        return { 0, 2, 3, 5, 7, 8, 10 };
    }

    int degreePc (int tonicPc, Mode mode, int degree) noexcept { return pitchClass (tonicPc + absoluteStep (mode, degree)); }

    int chordInterval (Mode mode, int degree, int steps) noexcept { return absoluteStep (mode, degree + steps) - absoluteStep (mode, degree); }

    bool inMinorKey (int tonicPc, int pc) noexcept
    {
        const int rel = pitchClass (pc - tonicPc);
        for (const int s : modeSteps (Mode::aeolian))
            if (s == rel)
                return true;
        return false;
    }

    std::string keyName (int tonicPc, bool minor)
    {
        const auto pc = static_cast<std::size_t> (pitchClass (tonicPc));
        return std::string (minor ? minorNames[pc] : majorNames[pc]) + (minor ? " minor" : " major");
    }

    std::string camelot (int tonicPc, bool minor)
    {
        // A major key shares its number with its relative minor, 3 semitones below.
        const int pc = minor ? pitchClass (tonicPc) : pitchClass (tonicPc - 3);
        return std::to_string (camelotMinor[static_cast<std::size_t> (pc)]) + (minor ? "A" : "B");
    }
}
