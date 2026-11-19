#include "editor/ChordName.h"

#include <array>
#include <cmath>

namespace wubansia::editor
{
    namespace
    {
        constexpr std::array<const char*, 12> kSharps { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        constexpr std::array<const char*, 12> kFlats { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };

        struct Quality
        {
            std::uint16_t intervals; // bit per semitone above the root, the root's bit 0 included
            const char* suffix;
        };

        constexpr std::uint16_t bits (std::initializer_list<int> semitones)
        {
            std::uint16_t b = 0;
            for (const auto s : semitones)
                b = (std::uint16_t) (b | (1u << s));
            return b;
        }

        // In the order they're tried: the first exact match names the set.
        constexpr std::array<Quality, 14> kQualities { {
            { bits ({ 0, 3, 7, 10, 2 }), "m9" },
            { bits ({ 0, 4, 7, 11, 2 }), "maj9" },
            { bits ({ 0, 4, 7, 10, 2 }), "9" },
            { bits ({ 0, 3, 7, 2 }), "m(add9)" },
            { bits ({ 0, 4, 7, 2 }), "add9" },
            { bits ({ 0, 3, 7, 10 }), "m7" },
            { bits ({ 0, 4, 7, 11 }), "maj7" },
            { bits ({ 0, 4, 7, 10 }), "7" },
            { bits ({ 0, 3, 7 }), "m" },
            { bits ({ 0, 4, 7 }), "" },
            { bits ({ 0, 2, 7 }), "sus2" },
            { bits ({ 0, 5, 7 }), "sus4" },
            { bits ({ 0, 3, 7, 10, 2, 5 }), "m11" },
            { bits ({ 0, 7 }), "5" },
        } };

        // The pitch classes of set, as bits relative to root.
        std::uint16_t relative (std::uint16_t set, int root)
        {
            std::uint16_t r = 0;
            for (int pc = 0; pc < 12; ++pc)
                if ((set >> pc) & 1u)
                    r = (std::uint16_t) (r | (1u << ((pc - root + 12) % 12)));
            return r;
        }

        const char* suffixFor (std::uint16_t set, int root)
        {
            const auto r = relative (set, root);
            for (const auto& q : kQualities)
                if (q.intervals == r)
                    return q.suffix;
            return nullptr;
        }
    }

    juce::String pitchName (int key, bool flats)
    {
        const auto pc = (std::size_t) (((key % 12) + 12) % 12);
        return flats ? kFlats[pc] : kSharps[pc];
    }

    juce::String noteName (int key, bool flats) { return pitchName (key, flats) + juce::String (key / 12 - 1 - (key < 0 && key % 12 != 0 ? 1 : 0)); }

    juce::String chordName (std::span<const std::uint8_t> keys, bool flats)
    {
        if (keys.empty())
            return {};
        int lowest = 128;
        std::uint16_t set = 0;
        for (const auto k : keys)
        {
            lowest = std::min (lowest, (int) k);
            set = (std::uint16_t) (set | (1u << (k % 12)));
        }
        const auto bass = lowest % 12;
        if (const auto* suffix = suffixFor (set, bass))
            return pitchName (bass, flats) + suffix;
        // An inversion: the first other pitch, upwards from the bass, that names the set, over the bass.
        for (int step = 1; step < 12; ++step)
        {
            const auto root = (bass + step) % 12;
            if (((set >> root) & 1u) != 0)
                if (const auto* suffix = suffixFor (set, root))
                    return pitchName (root, flats) + suffix + "/" + pitchName (bass, flats);
        }
        return pitchName (bass, flats);
    }

    bool keyUsesFlats (const juce::String& key)
    {
        const auto tonic = key.upToFirstOccurrenceOf (" ", false, false).trim();
        if (tonic.isEmpty())
            return false;
        if (tonic.length() > 1 && tonic[1] == 'b')
            return true;
        const auto minor = key.containsIgnoreCase ("minor");
        // The natural tonics whose signatures carry flats: F major; D, G, C and F minor.
        return minor ? (tonic == "D" || tonic == "G" || tonic == "C" || tonic == "F") : tonic == "F";
    }

    juce::String shortTempo (double bpm)
    {
        const auto tenths = std::llround (bpm * 10.0);
        if (tenths % 10 == 0)
            return juce::String (tenths / 10);
        return juce::String ((double) tenths / 10.0, 1);
    }
}
