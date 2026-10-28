#include "PluginProcessor.h"
#include "ParameterIds.h"
#include "PluginEditor.h"

namespace wubansia
{
    namespace
    {
        constexpr float silenceDb = -60.0f; // the bottom of the gain range means silence
    }

    PluginProcessor::PluginProcessor()
        : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
          apvts (*this, nullptr, stateType, createParameterLayout())
    {
        masterGainDb = apvts.getRawParameterValue (ParameterIds::masterGain);
        jassert (masterGainDb != nullptr);
    }

    juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { ParameterIds::masterGain, 1 },
            "Gain",
            juce::NormalisableRange<float> (silenceDb, 6.0f),
            0.0f,
            juce::AudioParameterFloatAttributes().withLabel ("dB")));
        return layout;
    }

    void PluginProcessor::prepareToPlay (double sampleRate, int)
    {
        voices.prepare (sampleRate);
        masterGain.reset (sampleRate, 0.02); // 20 ms ramps, so gain changes never click
        masterGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (masterGainDb->load(), silenceDb));
    }

    void PluginProcessor::releaseResources()
    {
        voices.reset();
    }

    bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
    {
        const auto out = layouts.getMainOutputChannelSet();
        return layouts.getMainInputChannelSet().isDisabled()
               && (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo());
    }

    void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
    {
        juce::ScopedNoDenormals noDenormals;
        buffer.clear();

        // Sample-accurate: render up to each event, then apply it.
        const auto numSamples = buffer.getNumSamples();
        int rendered = 0;

        for (const auto metadata : midi)
        {
            const auto position = juce::jlimit (rendered, numSamples, metadata.samplePosition);
            voices.render (buffer, rendered, position - rendered);
            voices.handleMidi (metadata.data, metadata.numBytes);
            rendered = position;
        }

        voices.render (buffer, rendered, numSamples - rendered);

        masterGain.setTargetValue (juce::Decibels::decibelsToGain (masterGainDb->load (std::memory_order_relaxed), silenceDb));
        masterGain.applyGain (buffer, numSamples);
    }

    juce::AudioProcessorEditor* PluginProcessor::createEditor()
    {
        return new PluginEditor (*this);
    }

    void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
    {
        auto state = apvts.copyState();
        state.setProperty ("stateVersion", stateVersion, nullptr);

        if (const auto xml = state.createXml())
            copyXmlToBinary (*xml, destData);
    }

    void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
    {
        // Anything that isn't our state is ignored. Later versions migrate older
        // states here, keyed on the stateVersion attribute.
        if (const auto xml = getXmlFromBinary (data, sizeInBytes))
            if (xml->hasTagName (stateType))
                apvts.replaceState (juce::ValueTree::fromXml (*xml));
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new wubansia::PluginProcessor();
}
