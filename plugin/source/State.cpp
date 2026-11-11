#include "State.h"

#include "transport/InternalTransport.h"

#include <algorithm>
#include <cmath>

namespace wubansia::State
{
    namespace
    {
        const juce::Identifier paramType ("PARAM");
        const juce::Identifier idProperty ("id");
        const juce::Identifier valueProperty ("value");

        // The root attributes this version reads and writes, in the order it writes them.
        constexpr std::array<const char*, 6> knownAttributes { "stateVersion", "kit", "kit_content", "standalone_bpm", "midi_in", "phrase_lanes" };

        bool isKnownAttribute (const juce::String& name)
        {
            for (const auto* known : knownAttributes)
                if (name == known)
                    return true;
            return false;
        }

        // The processor's parameters, in its order (the host's).
        std::vector<const juce::RangedAudioParameter*> parametersOf (const juce::AudioProcessor& processor)
        {
            std::vector<const juce::RangedAudioParameter*> out;
            for (const auto* parameter : processor.getParameters())
                if (const auto* ranged = dynamic_cast<const juce::RangedAudioParameter*> (parameter))
                    out.push_back (ranged);
            return out;
        }

        // A parameter's default as the APVTS keeps it (its unnormalised value).
        juce::var defaultOf (const juce::RangedAudioParameter& parameter)
        {
            return juce::var (parameter.convertFrom0to1 (parameter.getDefaultValue()));
        }

        juce::ValueTree paramTree (const juce::String& id, const juce::var& value)
        {
            juce::ValueTree tree (paramType);
            tree.setProperty (idProperty, id, nullptr);
            tree.setProperty (valueProperty, value, nullptr);
            return tree;
        }

        bool isFiniteNumber (const juce::var& value)
        {
            if (value.isVoid() || value.isUndefined())
                return false;
            return std::isfinite (static_cast<double> (value));
        }
    }

    juce::String midiInText (engine::MidiInMode mode, engine::Lane lane)
    {
        if (mode == engine::MidiInMode::lane)
            return "lane:" + juce::String (std::string (engine::laneId (lane)));
        return "multi";
    }

    bool readMidiIn (const juce::String& text, engine::MidiInMode& mode, engine::Lane& lane)
    {
        if (text == "multi")
        {
            mode = engine::MidiInMode::multi;
            return true;
        }
        if (text.startsWith ("lane:"))
        {
            const auto id = text.substring (5).toStdString();
            const auto index = engine::laneIndex (id);
            if (index >= 0)
            {
                mode = engine::MidiInMode::lane;
                lane = static_cast<engine::Lane> (index);
                return true;
            }
        }
        return false;
    }

    juce::String phraseText (engine::PhraseMode mode)
    {
        return mode == engine::PhraseMode::off ? "off" : "auto";
    }

    juce::String bpmText (double bpm)
    {
        if (! std::isfinite (bpm) || bpm <= 0.0)
            return "0";
        auto text = juce::String (bpm, 3); // "128.500"
        while (text.endsWithChar ('0'))
            text = text.dropLastCharacters (1);
        if (text.endsWithChar ('.'))
            text = text.dropLastCharacters (1);
        return text;
    }

    std::optional<Loaded> read (const void* data, int sizeInBytes, const juce::AudioProcessor& processor)
    {
        if (data == nullptr || sizeInBytes <= 0)
            return std::nullopt;
        const auto xml = juce::AudioProcessor::getXmlFromBinary (data, sizeInBytes);
        if (xml == nullptr || ! xml->hasTagName (rootTag))
            return std::nullopt;

        Loaded loaded;
        loaded.version = std::max (1, xml->getIntAttribute ("stateVersion", 1));

        // The state-only settings: each one's default where it is missing or unreadable. A version 1
        // state has none of them: kit FUT01.
        auto& s = loaded.settings;
        s.kit = xml->getStringAttribute ("kit").trim();
        if (s.kit.isEmpty() || s.kit.length() > 64)
            s.kit = defaultKit;
        s.kitContent = std::max (0, xml->getIntAttribute ("kit_content", 0));
        const auto bpm = xml->getStringAttribute ("standalone_bpm").trim();
        s.standaloneBpm = bpm.containsOnly ("0123456789.") && bpm.isNotEmpty() ? InternalTransport::clampBpm (bpm.getDoubleValue()) : 0.0;
        if (! readMidiIn (xml->getStringAttribute ("midi_in"), s.midiIn, s.midiInLane))
        {
            s.midiIn = engine::MidiInMode::multi;
            s.midiInLane = engine::Lane::kick;
        }
        s.phrases = xml->getStringAttribute ("phrase_lanes") == "off" ? engine::PhraseMode::off : engine::PhraseMode::automatic;

        for (int i = 0; i < xml->getNumAttributes(); ++i)
            if (const auto name = xml->getAttributeName (i); ! isKnownAttribute (name))
                loaded.otherAttributes.set (juce::Identifier (name), xml->getAttributeValue (i));

        // The parameters: every one, in the processor's order, the state's value where it has a finite
        // one, else the default; then the state's other children, as they were.
        const auto tree = juce::ValueTree::fromXml (*xml);
        loaded.parameters = juce::ValueTree (rootTag);
        const auto known = parametersOf (processor);
        for (const auto* parameter : known)
        {
            const auto child = tree.getChildWithProperty (idProperty, parameter->getParameterID());
            if (child.isValid() && child.hasType (paramType) && isFiniteNumber (child[valueProperty]))
                loaded.parameters.appendChild (child.createCopy(), nullptr);
            else
                loaded.parameters.appendChild (paramTree (parameter->getParameterID(), defaultOf (*parameter)), nullptr);
        }
        for (const auto& child : tree)
        {
            const auto id = child[idProperty].toString();
            const bool isKnown = child.hasType (paramType)
                                 && std::any_of (known.begin(), known.end(), [&id] (const juce::RangedAudioParameter* p) { return p->getParameterID() == id; });
            if (! isKnown)
                loaded.parameters.appendChild (child.createCopy(), nullptr);
        }
        return loaded;
    }

    void write (juce::MemoryBlock& dest, const juce::ValueTree& parameters, const StateOnly& settings, int kitContent, int stateVersion,
        const juce::NamedValueSet& otherAttributes, const juce::AudioProcessor& processor)
    {
        juce::XmlElement root (rootTag);
        root.setAttribute ("stateVersion", std::max (version, stateVersion));
        root.setAttribute ("kit", settings.kit);
        root.setAttribute ("kit_content", std::max (0, kitContent));
        root.setAttribute ("standalone_bpm", bpmText (settings.standaloneBpm));
        root.setAttribute ("midi_in", midiInText (settings.midiIn, settings.midiInLane));
        root.setAttribute ("phrase_lanes", phraseText (settings.phrases));
        for (const auto& attribute : otherAttributes)
            if (! isKnownAttribute (attribute.name.toString()))
                root.setAttribute (attribute.name, attribute.value.toString());

        // The parameters in the processor's order (whatever order the tree holds them in), each with its
        // default if the tree lacks it; then everything else the tree holds.
        const auto known = parametersOf (processor);
        for (const auto* parameter : known)
        {
            const auto child = parameters.getChildWithProperty (idProperty, parameter->getParameterID());
            const auto tree = child.isValid() && child.hasType (paramType) ? child : paramTree (parameter->getParameterID(), defaultOf (*parameter));
            if (auto xml = tree.createXml())
                root.addChildElement (xml.release());
        }
        for (const auto& child : parameters)
        {
            const auto id = child[idProperty].toString();
            const bool isKnown = child.hasType (paramType)
                                 && std::any_of (known.begin(), known.end(), [&id] (const juce::RangedAudioParameter* p) { return p->getParameterID() == id; });
            if (! isKnown)
                if (auto xml = child.createXml())
                    root.addChildElement (xml.release());
        }

        juce::AudioProcessor::copyXmlToBinary (root, dest);
    }
}
