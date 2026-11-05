#pragma once

// The plugin's saved state, version 2 (ADR 0018, amending ADR 0002's version 1). It is JUCE's binary
// XML (AudioProcessor::copyXmlToBinary):
//
//     <WubansiaState stateVersion="2" kit="FUT01" kit_content="1" standalone_bpm="0" midi_in="multi"
//                    phrase_lanes="auto">
//       <PARAM id="master_gain" value="0.0"/> ... one per parameter, in the host's order
//     </WubansiaState>
//
// The five root attributes are the state-only settings: never automated, never parameters.
// - kit: the kit's id. Any string survives, so a project naming a kit this build lacks keeps it.
// - kit_content: the content version the project was made with; the plugin plays the version it
//   carries and says so when they differ.
// - standalone_bpm: the Standalone tempo; 0 follows the kit's.
// - midi_in: "multi" (MIDI in routes by channel, docs/midi-map.md) or "lane:<lane id>" (every channel
//   plays that lane, "lane:bass").
// - phrase_lanes: "auto" (recorded phrases while the tempo is near the kit's) or "off".
//
// Reading: a root other than WubansiaState, junk or a truncated block is ignored. stateVersion 1 (M0)
// migrates: every parameter it lacks takes its default and the kit is FUT01. Every state: a missing or
// unknown attribute value takes its default, a parameter whose value isn't a finite number takes its
// default, and whatever this version doesn't know (attributes, parameters, children) is kept, so a
// newer project saved here keeps its data. Writing: stateVersion is max (2, the version read), the
// attributes in the order above (then any kept ones), the parameters in the processor's order (then
// any kept children), so the bytes of getStateInformation () right after setStateInformation () are
// the bytes it was given.

#include "engine.h"
#include "kit/kit.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <optional>

namespace wubansia
{
    // The state-only settings, as a project saves them.
    struct StateOnly
    {
        juce::String kit { "FUT01" };
        int kitContent = 0; // the content version the project names; 0: the version the plugin carries
        double standaloneBpm = 0.0; // 0: the kit's
        engine::MidiInMode midiIn = engine::MidiInMode::multi;
        engine::Lane midiInLane = engine::Lane::kick; // for MidiInMode::lane
        engine::PhraseMode phrases = engine::PhraseMode::automatic;

        friend bool operator== (const StateOnly&, const StateOnly&) = default;
    };

    namespace State
    {
        inline constexpr const char* rootTag = "WubansiaState"; // PERMANENT (ADR 0002)
        inline constexpr int version = 2;
        inline constexpr const char* defaultKit = "FUT01"; // a new instance's, and every v1 state's

        // A state read from a host.
        struct Loaded
        {
            int version = 1; // its stateVersion (1 when it has none)
            StateOnly settings;
            // For AudioProcessorValueTreeState::replaceState: a WubansiaState tree with no properties,
            // a PARAM per parameter in the processor's order (the state's value, else the default),
            // then the state's other children as they were.
            juce::ValueTree parameters;
            juce::NamedValueSet otherAttributes; // root attributes this version doesn't know, in order
        };

        // The state in data, or nothing when it isn't one of ours.
        std::optional<Loaded> read (const void* data, int sizeInBytes, const juce::AudioProcessor& processor);

        // A state's bytes from the parameter tree (AudioProcessorValueTreeState::copyState ()), the
        // settings, the content version to name, the version and the kept attributes.
        void write (juce::MemoryBlock& dest, const juce::ValueTree& parameters, const StateOnly& settings, int kitContent,
            int stateVersion, const juce::NamedValueSet& otherAttributes, const juce::AudioProcessor& processor);

        // The attributes' spellings.
        juce::String midiInText (engine::MidiInMode mode, engine::Lane lane); // "multi", "lane:bass"
        bool readMidiIn (const juce::String& text, engine::MidiInMode& mode, engine::Lane& lane); // false: unknown
        juce::String phraseText (engine::PhraseMode mode); // "auto", "off"
        juce::String bpmText (double bpm); // "0", "128", "128.5": at most 3 decimals, no trailing zeros
    }
}
