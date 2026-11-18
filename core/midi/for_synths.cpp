#include "midi/for_synths.h"

namespace wubansia::midi
{
    std::vector<Note> synthNotes (std::vector<Note> notes, std::uint16_t, std::int64_t, bool)
    {
        return notes; // not yet: the tests come first
    }

    util::Result<std::vector<std::uint8_t>> forSynths (std::span<const std::uint8_t> smf, std::span<const SynthPart>)
    {
        return util::Result<std::vector<std::uint8_t>>::success (std::vector<std::uint8_t> (smf.begin(), smf.end()));
    }
}
