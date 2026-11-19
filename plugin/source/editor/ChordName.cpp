#include "editor/ChordName.h"

namespace wubansia::editor
{
    juce::String chordName (std::span<const std::uint8_t>, bool) { return {}; }
    juce::String noteName (int, bool) { return {}; }
    juce::String pitchName (int, bool) { return {}; }
    bool keyUsesFlats (const juce::String&) { return false; }
    juce::String shortTempo (double) { return {}; }
}
