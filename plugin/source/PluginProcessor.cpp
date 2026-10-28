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

    void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
    {
        buffer.clear();
    }

    juce::AudioProcessorEditor* PluginProcessor::createEditor()
    {
        return new PluginEditor (*this);
    }

    void PluginProcessor::getStateInformation (juce::MemoryBlock&) {}

    void PluginProcessor::setStateInformation (const void*, int) {}
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new wubansia::PluginProcessor();
}
