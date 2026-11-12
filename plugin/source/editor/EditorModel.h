#pragma once

// What the editor shows at one moment: gathered from the Controller (and the parameters the panels
// decorate) once per timer tick, then handed to every panel, so they all draw the same moment.

#include "Controller.h"

#include <juce_graphics/juce_graphics.h>

#include <cmath>
#include <vector>

namespace wubansia::editor
{
    // Phrase lanes play their recordings while the tempo is within this share of the kit's (the
    // manifests' default tolerance). The engine's own answer, Snapshot::phraseRecorded, wins while
    // it plays; this predicts what the next play does while stopped.
    inline constexpr double kPhraseTolerance = 0.02;

    inline constexpr std::array<const char*, engine::kLanes> kLaneNames {
        "Kick", "Snare", "Hats", "Perc", "Bass", "Lead", "Chords", "Vox", "Accents", "FX"
    };

    inline juce::String laneName (engine::Lane lane) { return kLaneNames[static_cast<std::size_t> (lane)]; }

    // Text with non-ASCII characters ("\xc2\xb7", "\xe2\x80\xa6") must reach juce::String as UTF-8.
    inline juce::String utf8 (const char* text) { return juce::String (juce::CharPointer_UTF8 (text)); }
    inline juce::String middleDot() { return utf8 (" \xc2\xb7 "); }

    struct EditorModel
    {
        std::vector<KitCard> kits;
        int currentIndex = -1; // into kits; -1 when the current kit isn't listed
        KitCard kit; // the current kit's card, or an empty one
        ContentStatus content;
        std::vector<SectionInfo> sections;
        Snapshot snapshot;
        TransportState transport;
        bool preview = false;
        bool standalone = false;
        engine::MidiInMode midiInMode = engine::MidiInMode::multi;
        engine::Lane midiInLane = engine::Lane::kick;
        engine::PhraseMode phraseMode = engine::PhraseMode::automatic;
        bool dragDrumsOnOneTrack = false; // the drag strip's 1 drum track switch (ADR 0025)
        juce::StringArray phraseFiles;

        // From the parameters.
        engine::PlayMode playMode = engine::PlayMode::loop;
        bool vox = true;

        juce::Colour accent;

        bool hasKit() const noexcept { return currentIndex >= 0; }

        bool hasLane (engine::Lane lane) const { return kit.laneLabels[static_cast<std::size_t> (lane)].isNotEmpty(); }

        juce::String laneLabel (engine::Lane lane) const
        {
            const auto& label = kit.laneLabels[static_cast<std::size_t> (lane)];
            return label.isNotEmpty() ? label : laneName (lane);
        }

        bool isPhraseLane (engine::Lane lane) const noexcept { return (kit.phraseLanes & engine::laneBit (lane)) != 0; }

        bool hasAb (int index) const
        {
            const auto i = static_cast<std::size_t> (index);
            return kit.abA[i].isNotEmpty() && kit.abB[i].isNotEmpty();
        }

        // The tempo playback runs at, or would start at.
        double tempo() const noexcept
        {
            if (snapshot.playing && snapshot.bpm > 0.0)
                return snapshot.bpm;
            if (transport.bpm > 0.0)
                return transport.bpm;
            return (double) kit.bpm;
        }

        // Whether a phrase lane plays its recorded line ("recorded") or the multisample.
        bool phrasePlaysRecorded (engine::Lane lane) const noexcept
        {
            if (! isPhraseLane (lane) || phraseMode == engine::PhraseMode::off || playMode == engine::PlayMode::off)
                return false;
            if (snapshot.playing)
                return (snapshot.phraseRecorded & engine::laneBit (lane)) != 0;
            const auto bpm = tempo();
            return kit.bpm <= 0 || bpm <= 0.0 || std::abs (bpm - (double) kit.bpm) <= (double) kit.bpm * kPhraseTolerance;
        }
    };

    // "F# minor" -> "F#m", "Bb major" -> "Bb".
    inline juce::String shortKey (const juce::String& key)
    {
        if (key.endsWithIgnoreCase (" minor"))
            return key.dropLastCharacters (6).trim() + "m";
        if (key.endsWithIgnoreCase (" major"))
            return key.dropLastCharacters (6).trim();
        return key;
    }

    // "FUT01_Phrase_ChromePluck_4bars.mid" -> "Chrome Pluck · 4 bars".
    inline juce::String phraseTitle (const juce::String& file, const juce::String& kitId)
    {
        auto name = file.upToLastOccurrenceOf (".", false, false);
        if (name.isEmpty())
            name = file;
        if (kitId.isNotEmpty() && name.startsWithIgnoreCase (kitId + "_"))
            name = name.substring (kitId.length() + 1);
        if (name.startsWithIgnoreCase ("Phrase_"))
            name = name.substring (7);

        juce::StringArray parts;
        for (const auto& token : juce::StringArray::fromTokens (name, "_", ""))
        {
            juce::String words;
            for (int i = 0; i < token.length(); ++i)
            {
                const auto c = token[i];
                const juce::juce_wchar previous = i > 0 ? token[i - 1] : 0;
                const juce::juce_wchar next = i + 1 < token.length() ? token[i + 1] : 0;
                const auto upperAfterLower = juce::CharacterFunctions::isUpperCase (c) && juce::CharacterFunctions::isLowerCase (previous);
                const auto upperBeforeLower = juce::CharacterFunctions::isUpperCase (c) && juce::CharacterFunctions::isUpperCase (previous)
                                              && juce::CharacterFunctions::isLowerCase (next);
                const auto letterAfterDigit = juce::CharacterFunctions::isLetter (c) && juce::CharacterFunctions::isDigit (previous);
                if (i > 0 && (upperAfterLower || upperBeforeLower || letterAfterDigit))
                    words << ' ';
                words << juce::String::charToString (c);
            }
            if (words.isNotEmpty())
                parts.add (words);
        }
        return parts.isEmpty() ? file : parts.joinIntoString (middleDot());
    }
}
